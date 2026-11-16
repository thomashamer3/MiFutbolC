#include "clima_diferido.h"
#include "db.h"
#include "openmeteo_client.h"
#include "partido.h"
#include "settings.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Tope de intentos fallidos por partido (entre todas las sesiones). */
#define CLIMA_MAX_INTENTOS 8
/* Open-Meteo publica el archivo historico con algunos dias de demora: un fallo
 * para un partido de los ultimos dias no consume intentos. */
#define CLIMA_DIAS_RECIENTE 14
/* curl corta solo a los 20 s (--connect-timeout 5 + --max-time 15); esto es
 * un resguardo por si el proceso queda colgado. */
#define CLIMA_TIMEOUT_SEG 60
#define CLIMA_URL_TAM 1536
#define CLIMA_RUTA_TAM 1024

#ifdef _WIN32
#define CLIMA_CURL "curl.exe"
#else
#define CLIMA_CURL "curl"
#endif

/* Datos del curl en curso. Se guarda la fecha y la cancha con que se lanzo
 * para no aplicar el clima si el partido cambio mientras tanto. */
typedef struct
{
    int activo;
    int terminado;
    int codigo_salida;
    AppProceso proceso;
    long long partido_id;
    int cancha_id;
    char fecha_hora[64];
    int anio;
    int mes;
    int dia;
    int hora;
    int minuto;
    char ruta[CLIMA_RUTA_TAM];
    time_t inicio;
} ClimaTrabajo;

static ClimaTrabajo g_trabajo;
static int g_tabla_lista = 0;

/* Partidos ya intentados en esta sesion: cada uno se intenta como mucho una
 * vez por arranque para no martillar la red en cada vuelta de menu. */
static long long *g_intentados = NULL;
static size_t g_intentados_cant = 0;
static size_t g_intentados_cap = 0;

static void clima_log(long long partido_id, const char *texto, const char *detalle)
{
    char msg[256];
    snprintf(msg, sizeof(msg), "Partido %lld: %s%s%s", partido_id, texto, detalle ? " " : "",
             detalle ? detalle : "");
    app_log_event("CLIMA", msg);
}

static int clima_exec(const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Error SQL %d: %s", rc, err ? err : "");
        app_log_event("CLIMA", msg);
        sqlite3_free(err);
        return 0;
    }
    return 1;
}

static int asegurar_tabla(void)
{
    if (g_tabla_lista)
    {
        return 1;
    }
    if (!db)
    {
        return 0;
    }
    /* El trigger limpia el pendiente desde cualquier camino de borrado
     * (partido, cancha, camiseta, sincronizacion). */
    if (!clima_exec("CREATE TABLE IF NOT EXISTS partido_clima_pendiente ("
                    " partido_id INTEGER PRIMARY KEY,"
                    " intentos INTEGER NOT NULL DEFAULT 0,"
                    " creado_en TEXT);") ||
            !clima_exec("CREATE TRIGGER IF NOT EXISTS trg_partido_clima_pendiente_delete "
                        "AFTER DELETE ON partido BEGIN "
                        " DELETE FROM partido_clima_pendiente WHERE partido_id = old.id;"
                        " END;"))
    {
        return 0;
    }
    g_tabla_lista = 1;
    return 1;
}

static int ya_intentado(long long partido_id)
{
    for (size_t i = 0; i < g_intentados_cant; i++)
    {
        if (g_intentados[i] == partido_id)
        {
            return 1;
        }
    }
    return 0;
}

static void marcar_intentado(long long partido_id)
{
    if (ya_intentado(partido_id))
    {
        return;
    }
    if (g_intentados_cant == g_intentados_cap)
    {
        size_t nueva = g_intentados_cap ? g_intentados_cap * 2 : 16;
        long long *ampliado = (long long *)realloc(g_intentados, nueva * sizeof(long long));
        if (!ampliado)
        {
            return;
        }
        g_intentados = ampliado;
        g_intentados_cap = nueva;
    }
    g_intentados[g_intentados_cant++] = partido_id;
}

static void desmarcar_intentado(long long partido_id)
{
    size_t i = 0;
    while (i < g_intentados_cant && g_intentados[i] != partido_id)
    {
        i++;
    }
    if (i < g_intentados_cant)
    {
        /* El ultimo ocupa el lugar del quitado: el orden no importa. */
        g_intentados_cant--;
        g_intentados[i] = g_intentados[g_intentados_cant];
    }
}

static void borrar_pendiente(long long partido_id)
{
    sqlite3_stmt *s = NULL;
    if (!db_prepare_stmt(&s, "DELETE FROM partido_clima_pendiente WHERE partido_id = ?"))
    {
        return;
    }
    sqlite3_bind_int64(s, 1, partido_id);
    sqlite3_step(s);
    db_stmt_release(s);
}

static int es_fecha_reciente(int anio, int mes, int dia)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = anio - 1900;
    t.tm_mon = mes - 1;
    t.tm_mday = dia;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    time_t fecha = mktime(&t);
    if (fecha == (time_t)-1)
    {
        return 0;
    }
    return difftime(time(NULL), fecha) < (double)CLIMA_DIAS_RECIENTE * 24.0 * 3600.0;
}

/* Un fallo suma un intento, salvo para partidos recientes (datos aun no
 * publicados): esos se reintentan en el proximo arranque. */
static void registrar_fallo(long long partido_id, int anio, int mes, int dia, const char *motivo)
{
    clima_log(partido_id, "sin clima:", motivo);
    if (es_fecha_reciente(anio, mes, dia))
    {
        return;
    }
    sqlite3_stmt *s = NULL;
    if (!db_prepare_stmt(&s, "UPDATE partido_clima_pendiente SET intentos = intentos + 1 "
                         "WHERE partido_id = ?"))
    {
        return;
    }
    sqlite3_bind_int64(s, 1, partido_id);
    sqlite3_step(s);
    db_stmt_release(s);
}

/* Franja del dia segun el horario solar real (0 si no hay datos de sol). */
static int calcular_franja_horaria(int hora, int minuto, const OpenMeteoResult *res)
{
    if (!res->has_sun_data)
        return 0;
    int total_minutos = hora * 60 + minuto;
    int sunrise_min = res->sunrise_hour * 60 + res->sunrise_minute;
    int sunset_min = res->sunset_hour * 60 + res->sunset_minute;
    if (total_minutos < sunrise_min)
        return 1;
    if (total_minutos < 12 * 60)
        return 2;
    if (total_minutos < 15 * 60)
        return 3;
    if (total_minutos < sunset_min - 60)
        return 4;
    if (total_minutos < sunset_min + 60)
        return 5;
    return 6;
}

/* 1 si el partido sigue existiendo con la misma fecha y cancha del lanzamiento. */
static int partido_sigue_igual(const ClimaTrabajo *t)
{
    sqlite3_stmt *s = NULL;
    int igual = 0;
    if (!db_prepare_stmt(&s, "SELECT cancha_id, fecha_hora FROM partido WHERE id = ?"))
    {
        return 0;
    }
    sqlite3_bind_int64(s, 1, t->partido_id);
    if (sqlite3_step(s) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(s, 1);
        igual = sqlite3_column_int(s, 0) == t->cancha_id && fecha &&
                strcmp(fecha, t->fecha_hora) == 0;
    }
    db_stmt_release(s);
    return igual;
}

static int guardar_clima(const ClimaTrabajo *t, const OpenMeteoResult *res)
{
    sqlite3_stmt *u = NULL;
    int franja = calcular_franja_horaria(t->hora, t->minuto, res);

    /* Sin datos de sol se conserva la franja calculada al crear el partido. */
    if (!db_prepare_stmt(&u, "UPDATE partido SET temperatura_c=?, apparent_temp_c=?, "
                         "precip_mm=?, wind_kmh=?, weather_code=?, clima_json=?, "
                         "dia=COALESCE(NULLIF(?, 0), dia) WHERE id=?"))
    {
        return 0;
    }
    sqlite3_bind_double(u, 1, res->temp_c);
    sqlite3_bind_double(u, 2, res->apparent_temp_c);
    sqlite3_bind_double(u, 3, res->precip_mm);
    sqlite3_bind_double(u, 4, res->wind_kmh);
    sqlite3_bind_int(u, 5, res->weather_code);
    sqlite3_bind_text(u, 6, res->clima_json, -1, DB_TRANSIENT);
    sqlite3_bind_int(u, 7, franja);
    sqlite3_bind_int64(u, 8, t->partido_id);
    int ok = sqlite3_step(u) == SQLITE_DONE;
    db_stmt_release(u);
    return ok;
}

/* Lee el archivo del curl terminado y, si trae datos, actualiza el partido. */
static void aplicar_resultado(const ClimaTrabajo *t)
{
    OpenMeteoResult res;
    char *json = NULL;
    int ok = 0;

    memset(&res, 0, sizeof(res));
    if (t->codigo_salida == 0)
    {
        json = app_leer_archivo_completo(t->ruta, NULL);
    }
    if (json)
    {
        openmeteo_set_silencioso(1);
        ok = openmeteo_parse(json, &res);
        openmeteo_set_silencioso(0);
        free(json);
    }
    if (!ok)
    {
        char motivo[64];
        snprintf(motivo, sizeof(motivo), "curl %d o respuesta sin datos", t->codigo_salida);
        registrar_fallo(t->partido_id, t->anio, t->mes, t->dia, motivo);
        openmeteo_result_free(&res);
        return;
    }
    if (!partido_sigue_igual(t))
    {
        /* Cambio o se borro mientras corria curl: queda pendiente sin sumar intento. */
        clima_log(t->partido_id, "cambio durante la consulta, se reintentara", NULL);
        openmeteo_result_free(&res);
        return;
    }
    if (guardar_clima(t, &res))
    {
        borrar_pendiente(t->partido_id);
        char detalle[64];
        snprintf(detalle, sizeof(detalle), "%.1f C", res.temp_c);
        clima_log(t->partido_id, "clima registrado:", detalle);
    }
    else
    {
        clima_log(t->partido_id, "no se pudo guardar el clima:", sqlite3_errmsg(db));
    }
    openmeteo_result_free(&res);
}

static void limpiar_trabajo(void)
{
    if (g_trabajo.ruta[0] != '\0')
    {
        remove(g_trabajo.ruta);
    }
    memset(&g_trabajo, 0, sizeof(g_trabajo));
}

/* Descarta los pendientes que ya no tienen sentido consultar. */
static void descartar_pendientes(const long long *ids, size_t cant)
{
    for (size_t i = 0; i < cant; i++)
    {
        borrar_pendiente(ids[i]);
        clima_log(ids[i], "pendiente descartado", NULL);
    }
}

/* 1 si el pendiente de la fila actual no se puede consultar: el partido no
 * existe, ya tiene clima, su cancha no tiene coordenadas o la fecha no se
 * entiende. Si se puede, deja coordenadas y fecha en @p lat, @p lon y @p t. */
static int fila_descartable(sqlite3_stmt *s, const char *fecha, ClimaTrabajo *t, double *lat,
                            double *lon)
{
    if (sqlite3_column_type(s, 1) == SQLITE_NULL || sqlite3_column_int(s, 4) != 0 ||
            sqlite3_column_type(s, 5) == SQLITE_NULL || sqlite3_column_type(s, 6) == SQLITE_NULL)
    {
        return 1;
    }
    *lat = sqlite3_column_double(s, 5);
    *lon = sqlite3_column_double(s, 6);
    return (*lat == 0.0 && *lon == 0.0) || !fecha ||
           !partido_parsear_fecha_hora(fecha, &t->anio, &t->mes, &t->dia, &t->hora, &t->minuto);
}

/* 1 si la fecha del trabajo es futura (Open-Meteo todavia no tiene el dato). */
static int trabajo_fecha_futura(const ClimaTrabajo *t)
{
    OpenMeteoParams omp;
    memset(&omp, 0, sizeof(omp));
    omp.anio = t->anio;
    omp.mes = t->mes;
    omp.dia = t->dia;
    return openmeteo_fecha_es_futura(&omp);
}

/* Busca el siguiente pendiente lanzable y deja sus datos en @p t. */
static int buscar_siguiente(ClimaTrabajo *t, double *lat, double *lon)
{
    sqlite3_stmt *s = NULL;
    long long descartes[32] = {0};
    size_t cant_descartes = 0;
    int encontrado = 0;

    if (!db_prepare_stmt(&s, "SELECT pc.partido_id, p.id, p.cancha_id, p.fecha_hora, "
                         "p.temperatura_c IS NOT NULL, c.latitud, c.longitud "
                         "FROM partido_clima_pendiente pc "
                         "LEFT JOIN partido p ON p.id = pc.partido_id "
                         "LEFT JOIN cancha c ON c.id = p.cancha_id "
                         "WHERE pc.intentos < ? ORDER BY pc.partido_id"))
    {
        return 0;
    }
    sqlite3_bind_int(s, 1, CLIMA_MAX_INTENTOS);
    while (!encontrado && sqlite3_step(s) == SQLITE_ROW)
    {
        long long pid = sqlite3_column_int64(s, 0);
        const char *fecha = (const char *)sqlite3_column_text(s, 3);

        if (ya_intentado(pid))
        {
            continue;
        }
        if (fila_descartable(s, fecha, t, lat, lon))
        {
            if (cant_descartes < sizeof(descartes) / sizeof(descartes[0]))
            {
                descartes[cant_descartes++] = pid;
            }
            continue;
        }

        if (trabajo_fecha_futura(t))
        {
            /* Queda pendiente sin sumar intento; se revisa en otro arranque. */
            marcar_intentado(pid);
            continue;
        }
        t->partido_id = pid;
        t->cancha_id = sqlite3_column_int(s, 2);
        strncpy_s(t->fecha_hora, sizeof(t->fecha_hora), fecha, sizeof(t->fecha_hora) - 1);
        encontrado = 1;
    }
    db_stmt_release(s);
    descartar_pendientes(descartes, cant_descartes);
    return encontrado;
}

static void lanzar_siguiente(void)
{
    ClimaTrabajo t;
    double lat = 0.0;
    double lon = 0.0;
    char url[CLIMA_URL_TAM];

    memset(&t, 0, sizeof(t));
    if (!buscar_siguiente(&t, &lat, &lon))
    {
        return;
    }
    marcar_intentado(t.partido_id);

    OpenMeteoParams omp;
    omp.latitud = lat;
    omp.longitud = lon;
    omp.anio = t.anio;
    omp.mes = t.mes;
    omp.dia = t.dia;
    omp.hora = t.hora;
    omp.minuto = t.minuto;
    if (!openmeteo_build_url(&omp, url, sizeof(url)))
    {
        /* Coordenadas o fecha fuera de rango: nunca va a funcionar. */
        borrar_pendiente(t.partido_id);
        clima_log(t.partido_id, "parametros invalidos, pendiente descartado", NULL);
        return;
    }
    if (!app_ruta_temporal(t.ruta, sizeof(t.ruta)))
    {
        registrar_fallo(t.partido_id, t.anio, t.mes, t.dia, "sin archivo temporal");
        return;
    }

    const char *argv[] = {CLIMA_CURL, "-s", "--compressed", "--connect-timeout", "5",
                          "--max-time", "15", "-o", t.ruta, url, NULL
                         };
    if (!app_proceso_iniciar(argv, &t.proceso))
    {
        remove(t.ruta);
        registrar_fallo(t.partido_id, t.anio, t.mes, t.dia, "no se pudo lanzar curl");
        return;
    }
    t.activo = 1;
    t.inicio = time(NULL);
    g_trabajo = t;
    clima_log(t.partido_id, "consulta de clima lanzada", NULL);
}

int clima_diferido_init(void)
{
    static int registrado = 0;
    if (!registrado)
    {
        /* Salidas con exit() (p.ej. borrar cuenta) tampoco dejan curl ni
         * archivos temporales colgados. finalizar no usa la base. */
        atexit(clima_diferido_finalizar);
        registrado = 1;
    }
    return asegurar_tabla();
}

int clima_diferido_encolar(long long partido_id)
{
    sqlite3_stmt *s = NULL;
    int ok;

    if (partido_id <= 0 || !asegurar_tabla())
    {
        return 0;
    }
    if (!db_prepare_stmt(&s, "INSERT INTO partido_clima_pendiente (partido_id, intentos, creado_en) "
                         "VALUES (?, 0, datetime('now', 'localtime')) "
                         "ON CONFLICT(partido_id) DO UPDATE SET intentos = 0"))
    {
        return 0;
    }
    sqlite3_bind_int64(s, 1, partido_id);
    ok = sqlite3_step(s) == SQLITE_DONE;
    db_stmt_release(s);
    if (ok)
    {
        desmarcar_intentado(partido_id);
    }
    return ok;
}

int clima_diferido_cancha_tiene_coordenadas(int cancha_id)
{
    sqlite3_stmt *s = NULL;
    int tiene = 0;

    if (!db || !db_prepare_stmt(&s, "SELECT latitud, longitud FROM cancha WHERE id = ?"))
    {
        return 0;
    }
    sqlite3_bind_int(s, 1, cancha_id);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL &&
            sqlite3_column_type(s, 1) != SQLITE_NULL)
    {
        tiene = !(sqlite3_column_double(s, 0) == 0.0 && sqlite3_column_double(s, 1) == 0.0);
    }
    db_stmt_release(s);
    return tiene;
}

/* Consulta si el curl en curso termino. Devuelve 1 si sigue corriendo (o se
 * acaba de cortar por tiempo) y no hay nada mas que hacer en este sondeo. */
static int sondear_proceso_en_curso(void)
{
    int codigo = -1;
    int estado = app_proceso_estado(&g_trabajo.proceso, &codigo);
    if (estado != 0)
    {
        g_trabajo.terminado = 1;
        g_trabajo.codigo_salida = estado == 1 ? codigo : -1;
        return 0;
    }
    if (difftime(time(NULL), g_trabajo.inicio) > CLIMA_TIMEOUT_SEG)
    {
        app_proceso_terminar(&g_trabajo.proceso);
        registrar_fallo(g_trabajo.partido_id, g_trabajo.anio, g_trabajo.mes, g_trabajo.dia,
                        "curl excedio el tiempo");
        limpiar_trabajo();
    }
    return 1;
}

void clima_diferido_procesar(void)
{
    if (!db || settings_get()->mode == MODE_SIMPLE || !asegurar_tabla())
    {
        return;
    }

    if (g_trabajo.activo)
    {
        if (!g_trabajo.terminado && sondear_proceso_en_curso())
        {
            return;
        }
        /* Dentro de una transaccion ajena el UPDATE quedaria atado a su
         * COMMIT/ROLLBACK: se posterga al siguiente sondeo. */
        if (!sqlite3_get_autocommit(db))
        {
            return;
        }
        aplicar_resultado(&g_trabajo);
        limpiar_trabajo();
    }

    if (!sqlite3_get_autocommit(db))
    {
        return;
    }
    lanzar_siguiente();
}

void clima_diferido_finalizar(void)
{
    if (g_trabajo.activo && !g_trabajo.terminado)
    {
        app_proceso_terminar(&g_trabajo.proceso);
        clima_log(g_trabajo.partido_id, "consulta cortada al salir, sigue pendiente", NULL);
    }
    limpiar_trabajo();
    free(g_intentados);
    g_intentados = NULL;
    g_intentados_cant = 0;
    g_intentados_cap = 0;
    g_tabla_lista = 0;
}

#ifdef UNIT_TEST
int clima_diferido_test_ya_intentado(long long partido_id)
{
    return ya_intentado(partido_id);
}

void clima_diferido_test_marcar_intentado(long long partido_id)
{
    marcar_intentado(partido_id);
}

int clima_diferido_test_intentados_cant(void)
{
    return (int)g_intentados_cant;
}

void clima_diferido_test_registrar_fallo(long long partido_id, int anio, int mes, int dia)
{
    registrar_fallo(partido_id, anio, mes, dia, "prueba");
}

long long clima_diferido_test_buscar_siguiente(void)
{
    ClimaTrabajo t;
    double lat = 0.0;
    double lon = 0.0;

    memset(&t, 0, sizeof(t));
    if (!asegurar_tabla() || !buscar_siguiente(&t, &lat, &lon))
    {
        return 0;
    }
    return t.partido_id;
}

void clima_diferido_test_aplicar_archivo(long long partido_id, int cancha_id,
        const char *fecha_hora, const char *ruta, int codigo_salida)
{
    ClimaTrabajo t;

    memset(&t, 0, sizeof(t));
    t.partido_id = partido_id;
    t.cancha_id = cancha_id;
    t.codigo_salida = codigo_salida;
    strncpy_s(t.fecha_hora, sizeof(t.fecha_hora), fecha_hora, sizeof(t.fecha_hora) - 1);
    strncpy_s(t.ruta, sizeof(t.ruta), ruta, sizeof(t.ruta) - 1);
    if (!partido_parsear_fecha_hora(fecha_hora, &t.anio, &t.mes, &t.dia, &t.hora, &t.minuto))
    {
        return;
    }
    aplicar_resultado(&t);
}
#endif
