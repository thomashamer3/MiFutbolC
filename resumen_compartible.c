/**
 * @file resumen_compartible.c
 * @brief Generador de resumenes compartibles del perfil futbolistico
 *
 * Genera resumenes visuales del perfil del jugador en formato HTML y Markdown,
 * incluyendo estadisticas, rendimiento, rachas, logros y mejores temporadas.
 * Optimizados para compartir en redes sociales y mensajeria.
 */

#include "resumen_compartible.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "export.h"
#include "logros.h"
#include "pdfgen.h"
#include "estadisticas_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

/* ─── Helpers estaticos ───────────────────────────────────────────────────── */

/**
 * @brief Prepara una sentencia SQL usando la conexion global
 * @param sql Consulta SQL a preparar
 * @param stmt Puntero donde almacenar la sentencia preparada
 * @return 1 si la preparacion fue exitosa, 0 en caso contrario
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

/**
 * @brief Obtiene el nombre del jugador activo desde la base de datos
 *
 * Utiliza get_user_name() para obtener el nombre configurado.
 * Si no hay nombre asignado, retorna "Jugador".
 *
 * @param buffer Buffer de salida para el nombre
 * @param size Tamano del buffer
 */
static void obtener_nombre_jugador(char *buffer, size_t size)
{
    char *nombre = get_user_name();
    if (nombre && nombre[0] != '\0')
    {
        snprintf(buffer, size, "%s", nombre);
    }
    else
    {
        snprintf(buffer, size, "Jugador");
    }
    free(nombre);
}

/**
 * @brief Cuenta el total de partidos registrados
 * @return Numero total de partidos, 0 si no hay registros
 */
static int contar_total_partidos(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COUNT(*) FROM partido;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/**
 * @brief Obtiene la suma total de goles
 * @return Total de goles, 0 si no hay registros
 */
static int contar_total_goles(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COALESCE(SUM(goles), 0) FROM partido;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/**
 * @brief Obtiene la suma total de asistencias
 * @return Total de asistencias, 0 si no hay registros
 */
static int contar_total_asistencias(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COALESCE(SUM(asistencias), 0) FROM partido;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/**
 * @brief Muestra en consola el detalle de los partidos que forman la racha
 *
 * Lista los partidos mas recientes con su fecha, goles y asistencias
 * para que el usuario verifique visualmente la racha.
 */
static void mostrar_detalle_racha(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT fecha_hora, goles, asistencias FROM partido "
        "ORDER BY " SQL_PARTIDO_CRONO("", "DESC") " LIMIT 10;";

    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }

    ui_printf("\nUltimos 10 partidos:\n");
    ui_printf("  %-20s %s %s\n", "Fecha", "G", "A");
    ui_printf("  %-20s %s %s\n", "--------------------", "--", "--");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        int goles = sqlite3_column_int(stmt, 1);
        int asistencias = sqlite3_column_int(stmt, 2);
        ui_printf("  %-20s %d %d\n", fecha ? fecha : "?", goles, asistencias);
    }

    sqlite3_finalize(stmt);
    ui_printf("\n");
}

/**
 * @brief Obtiene el nombre de la camiseta mas usada
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_camiseta_favorita(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT c.nombre FROM partido p "
        "JOIN camiseta c ON p.camiseta_id = c.id "
        "GROUP BY p.camiseta_id ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", nombre ? nombre : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene la cancha mas jugada
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_cancha_favorita(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT ca.nombre FROM partido p "
        "JOIN cancha ca ON p.cancha_id = ca.id "
        "GROUP BY p.cancha_id ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", nombre ? nombre : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene la posicion donde se rindio mejor
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_mejor_posicion(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT posicion_jugada FROM partido "
        "WHERE posicion_jugada IS NOT NULL AND posicion_jugada != '' "
        "GROUP BY posicion_jugada "
        "ORDER BY AVG(rendimiento_general) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *pos = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", pos ? pos : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene el clima mas frecuente
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_clima_favorito(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT clima FROM partido "
        "WHERE clima > 0 "
        "GROUP BY clima ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int clima_val = sqlite3_column_int(stmt, 0);
            snprintf(buffer, size, "%s", clima_to_text(clima_val));
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene el torneo mas participado
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_torneo_favorito(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT t.nombre FROM partido_torneo pt "
        "JOIN torneo t ON pt.torneo_id = t.id "
        "GROUP BY pt.torneo_id ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", nombre ? nombre : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Ningun partido vinculado a torneos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Ningun partido vinculado a torneos");
    }
}

/**
 * @brief Cuenta el total de lesiones registradas
 * @return Numero total de lesiones, 0 si no hay registros
 */
static int contar_lesiones(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COUNT(*) FROM lesion;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/* ─── Funciones publicas ──────────────────────────────────────────────────── */

static void menu_resumen_anio(void);

void menu_resumen_compartible(void)
{
    app_log_event("RESUMEN_COMPARTIBLE", "Ingreso al modulo Resumen Compartible");

    MenuItem items[] =
    {
        {1, "Generar Resumen HTML", generar_resumen_html},
        {2, "Generar Resumen Markdown", generar_resumen_markdown},
        {3, "Estadisticas Destacadas", generar_resumen_estadisticas},
        {4, "Mejor Temporada", generar_resumen_mejor_temporada},
        {5, "Resumen Anual", menu_resumen_anio},
        {0, "Volver", NULL}
    };
    ejecutar_menu("RESUMEN COMPARTIBLE", items, 6);
}

/* ─── Resumen general: calculo ────────────────────────────────────────────── */

/*
 * El resumen general (opciones 1 HTML y 2 Markdown) se calcula una sola vez
 * en un ResumenGeneral, sin interaccion, y despues cada formato solo escribe.
 * Asi el calculo se puede probar con una base real sin pasar por la consola.
 */

/* Agregados comunes a los rankings: partidos, con resultado, victorias, goles. */
#define RESUMEN_SQL_AGREGADOS_TOP                                                                  \
    "COUNT(*), "                                                                                   \
    "SUM(CASE WHEN p.resultado IN (1, 2, 3) THEN 1 ELSE 0 END), "                                  \
    "SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END), "                                           \
    "IFNULL(SUM(p.goles), 0) "

/* Orden de los rankings: mas partidos, despues mas victorias, despues nombre. */
#define RESUMEN_SQL_ORDEN_TOP " ORDER BY COUNT(*) DESC, 4 DESC, 1 ASC LIMIT 3;"

/* Filtro opcional por anio sobre la fecha normalizada (acepta "aaaa-mm-dd" y
 * "dd/mm/aaaa"). El parametro ?1 vale 0 para no filtrar. */
#define RESUMEN_SQL_FILTRO_ANIO(col) \
    "(?1 = 0 OR CAST(substr(" SQL_FECHA_ORD(col) ", 1, 4) AS INTEGER) = ?1)"

/**
 * @brief Separa una fecha de partido en sus partes
 *
 * Acepta "aaaa-mm-dd hh:mm" y "dd/mm/aaaa hh:mm" (la hora es opcional).
 *
 * @return 1 si reconocio la fecha, 0 si no
 */
static int resumen_parsear_fecha(const char *txt, int *anio, int *mes, int *dia, int *hora)
{
    int a = 0;
    int m = 0;
    int d = 0;
    int h = -1;
    int mi = 0;
    int leidos;

    *anio = 0;
    *mes = 0;
    *dia = 0;
    *hora = -1;
    if (!txt)
    {
        return 0;
    }

    leidos = sscanf(txt, "%4d-%2d-%2d %2d:%2d", &a, &m, &d, &h, &mi);
    if (leidos < 3)
    {
        h = -1;
        leidos = sscanf(txt, "%2d/%2d/%4d %2d:%2d", &d, &m, &a, &h, &mi);
    }
    if (leidos < 3 || a < 1900 || m < 1 || m > 12 || d < 1 || d > 31)
    {
        return 0;
    }

    *anio = a;
    *mes = m;
    *dia = d;
    *hora = (leidos >= 4 && h >= 0 && h <= 23) ? h : -1;
    return 1;
}

/**
 * @brief Dia de la semana de una fecha del calendario gregoriano
 * @return 0 Domingo .. 6 Sabado
 */
static int resumen_dia_semana(int anio, int mes, int dia)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    if (mes < 3)
    {
        anio -= 1;
    }
    return (anio + anio / 4 - anio / 100 + anio / 400 + t[mes - 1] + dia) % 7;
}

/* Clasifica un partido con la misma regla que la racha historica del modulo:
 * 'P' con 2 goles o mas, 'N' sin goles ni asistencias, 'X' el resto. */
static char resumen_tipo_racha(int goles, int asistencias)
{
    if (goles >= 2)
    {
        return 'P';
    }
    if (goles == 0 && asistencias == 0)
    {
        return 'N';
    }
    return 'X';
}

static double resumen_pct(int parte, int total)
{
    return total > 0 ? (double)parte * 100.0 / (double)total : 0.0;
}

/* Lee la fila actual del SELECT de partidos (ver resumen_general_calcular). */
static void resumen_leer_partido(sqlite3_stmt *stmt, ResumenPartidoCorto *p, int *anio,
                                 int *mes, int *dia)
{
    const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
    const char *rival = (const char *)sqlite3_column_text(stmt, 9);
    int hora = -1;

    memset(p, 0, sizeof(*p));
    p->id = sqlite3_column_int(stmt, 0);
    p->goles = sqlite3_column_int(stmt, 2);
    p->asistencias = sqlite3_column_int(stmt, 3);
    p->resultado = sqlite3_column_int(stmt, 4);
    p->rendimiento = sqlite3_column_int(stmt, 5);
    snprintf(p->rival, sizeof(p->rival), "%s", rival ? rival : "");

    if (resumen_parsear_fecha(fecha, anio, mes, dia, &hora))
    {
        snprintf(p->fecha, sizeof(p->fecha), "%02d/%02d/%04d", *dia % 100, *mes % 100,
                 *anio % 10000);
    }
    else
    {
        snprintf(p->fecha, sizeof(p->fecha), "%.15s", fecha ? fecha : "");
    }
}

/* Suma el partido al anio que le corresponde. Los partidos llegan en orden
 * cronologico, asi que un anio nuevo siempre va al final; si no hay lugar se
 * descarta el anio mas viejo. */
static void resumen_sumar_anio(ResumenGeneral *r, int anio, const ResumenPartidoCorto *p)
{
    ResumenAnioItem *item = NULL;

    if (anio <= 0)
    {
        return;
    }
    for (int i = 0; i < r->num_anios; i++)
    {
        if (r->anios[i].anio == anio)
        {
            item = &r->anios[i];
            break;
        }
    }
    if (!item)
    {
        if (r->num_anios == RESUMEN_MAX_ANIOS)
        {
            memmove(&r->anios[0], &r->anios[1],
                    sizeof(r->anios[0]) * (size_t)(RESUMEN_MAX_ANIOS - 1));
            r->num_anios--;
        }
        item = &r->anios[r->num_anios++];
        memset(item, 0, sizeof(*item));
        item->anio = anio;
    }

    item->partidos++;
    item->goles += p->goles;
    item->asistencias += p->asistencias;
    if (p->resultado == 1)
    {
        item->victorias++;
    }
}

/* Agrega al final de una ventana deslizante de partidos (la mas vieja sale). */
static void resumen_ventana_partido(ResumenPartidoCorto *ventana, int *n, int max,
                                    const ResumenPartidoCorto *p)
{
    if (*n == max)
    {
        memmove(&ventana[0], &ventana[1], sizeof(ventana[0]) * (size_t)(max - 1));
        (*n)--;
    }
    ventana[(*n)++] = *p;
}

static void resumen_ventana_int(int *ventana, int *n, int max, int valor)
{
    if (*n == max)
    {
        memmove(&ventana[0], &ventana[1], sizeof(ventana[0]) * (size_t)(max - 1));
        (*n)--;
    }
    ventana[(*n)++] = valor;
}

/* Estado de las rachas mientras se recorren los partidos en orden. */
typedef struct
{
    int victorias;
    int invicto;
    int marcando;
} ResumenRachasEnCurso;

static void resumen_actualizar_rachas(ResumenGeneral *r, ResumenRachasEnCurso *en_curso,
                                      const ResumenPartidoCorto *p)
{
    char tipo = resumen_tipo_racha(p->goles, p->asistencias);

    en_curso->victorias = p->resultado == 1 ? en_curso->victorias + 1 : 0;
    en_curso->invicto = (p->resultado == 1 || p->resultado == 2) ? en_curso->invicto + 1 : 0;
    en_curso->marcando = p->goles > 0 ? en_curso->marcando + 1 : 0;

    if (en_curso->victorias > r->mejor_racha_victorias)
    {
        r->mejor_racha_victorias = en_curso->victorias;
    }
    if (en_curso->invicto > r->mejor_racha_invicto)
    {
        r->mejor_racha_invicto = en_curso->invicto;
    }
    if (en_curso->marcando > r->mejor_racha_marcando)
    {
        r->mejor_racha_marcando = en_curso->marcando;
    }
    r->racha_victorias_actual = en_curso->victorias;

    /* Racha actual: la corrida final de partidos del mismo tipo. */
    if (r->racha_longitud > 0 && tipo == r->racha_tipo)
    {
        r->racha_longitud++;
    }
    else
    {
        r->racha_tipo = tipo;
        r->racha_longitud = 1;
    }
}

/* Records por partido; con ">=" gana el mas reciente en caso de empate. */
static void resumen_actualizar_records(ResumenGeneral *r, const ResumenPartidoCorto *p)
{
    if (p->goles > 0 && p->goles >= r->max_goles.goles)
    {
        r->max_goles = *p;
    }
    if (p->asistencias > 0 && p->asistencias >= r->max_asistencias.asistencias)
    {
        r->max_asistencias = *p;
    }
    if (p->rendimiento > 0 && p->rendimiento >= r->mejor_rendimiento.rendimiento)
    {
        r->mejor_rendimiento = *p;
    }
}

/* Contadores de ataque del partido. */
static void resumen_sumar_ataque(ResumenGeneral *r, const ResumenPartidoCorto *p, int minutos)
{
    r->goles += p->goles;
    r->asistencias += p->asistencias;
    if (p->goles > 0)
    {
        r->partidos_con_gol++;
    }
    if (p->asistencias > 0)
    {
        r->partidos_con_asistencia++;
    }
    if (p->goles == 2)
    {
        r->dobletes++;
    }
    if (p->goles >= 3)
    {
        r->hat_tricks++;
    }
    if (minutos > 0)
    {
        r->partidos_con_minutos++;
        r->minutos_total += minutos;
    }
}

static void resumen_sumar_resultado(ResumenGeneral *r, int resultado)
{
    switch (resultado)
    {
    case 1:
        r->victorias++;
        break;
    case 2:
        r->empates++;
        break;
    case 3:
        r->derrotas++;
        break;
    default:
        return;
    }
    r->con_resultado++;
}

/* Acumulador de los promedios de rendimiento, animo y cansancio. */
typedef struct
{
    long rendimiento;
    long animo;
    long cansancio;
    long ga_con_minutos;
} ResumenSumas;

static void resumen_sumar_contexto(ResumenGeneral *r, ResumenSumas *s, sqlite3_stmt *stmt,
                                   const ResumenPartidoCorto *p, int anio, int mes, int dia)
{
    int cansancio = sqlite3_column_int(stmt, 6);
    int animo = sqlite3_column_int(stmt, 7);
    int minutos = sqlite3_column_int(stmt, 8);
    int franja = sqlite3_column_int(stmt, 10);

    if (p->rendimiento > 0)
    {
        r->con_rendimiento++;
        s->rendimiento += p->rendimiento;
    }
    if (animo > 0)
    {
        r->con_animo++;
        s->animo += animo;
    }
    if (cansancio > 0)
    {
        r->con_cansancio++;
        s->cansancio += cansancio;
    }
    if (minutos > 0)
    {
        s->ga_con_minutos += p->goles + p->asistencias;
    }
    resumen_sumar_ataque(r, p, minutos);

    if (franja >= 1 && franja <= RESUMEN_FRANJAS)
    {
        r->franjas[franja - 1].partidos++;
        r->franjas[franja - 1].victorias += p->resultado == 1;
    }
    if (anio > 0)
    {
        int ds = resumen_dia_semana(anio, mes, dia);
        r->dias_semana[ds].partidos++;
        r->dias_semana[ds].victorias += p->resultado == 1;
    }
}

/**
 * @brief Recorre todos los partidos en orden cronologico y acumula
 * @return 1 si la consulta pudo prepararse, 0 si no
 */
static int resumen_recorrer_partidos(ResumenGeneral *r, int anio)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha_hora, goles, asistencias, IFNULL(resultado, 0), "
        "IFNULL(rendimiento_general, 0), IFNULL(cansancio, 0), IFNULL(estado_animo, 0), "
        "IFNULL(minutos_jugados, 0), IFNULL(rival_nombre, ''), IFNULL(dia, 0) "
        "FROM partido WHERE " RESUMEN_SQL_FILTRO_ANIO("fecha_hora")
        " ORDER BY " SQL_PARTIDO_CRONO("", "ASC") ";";
    ResumenRachasEnCurso en_curso = {0, 0, 0};
    ResumenSumas sumas = {0, 0, 0, 0};

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, anio);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ResumenPartidoCorto p;
        int anio;
        int mes;
        int dia;

        resumen_leer_partido(stmt, &p, &anio, &mes, &dia);
        if (r->partidos == 0)
        {
            snprintf(r->primer_partido_fecha, sizeof(r->primer_partido_fecha), "%s", p.fecha);
        }
        r->partidos++;
        resumen_sumar_resultado(r, p.resultado);
        resumen_sumar_contexto(r, &sumas, stmt, &p, anio, mes, dia);
        resumen_actualizar_rachas(r, &en_curso, &p);
        resumen_actualizar_records(r, &p);
        resumen_sumar_anio(r, anio, &p);
        resumen_ventana_partido(r->ultimos, &r->ultimos_n, RESUMEN_ULTIMOS_N, &p);
        if (p.rendimiento > 0)
        {
            resumen_ventana_int(r->tendencia, &r->tendencia_n, RESUMEN_TENDENCIA_N,
                                p.rendimiento);
        }
        r->ultimo = p;
    }
    sqlite3_finalize(stmt);

    r->rendimiento_prom = r->con_rendimiento ? (double)sumas.rendimiento / r->con_rendimiento : 0;
    r->animo_prom = r->con_animo ? (double)sumas.animo / r->con_animo : 0;
    r->cansancio_prom = r->con_cansancio ? (double)sumas.cansancio / r->con_cansancio : 0;
    r->ga_por_90 = r->minutos_total > 0
                   ? (double)sumas.ga_con_minutos * 90.0 / (double)r->minutos_total
                   : 0.0;
    return 1;
}

/* Llena un ranking con las columnas: nombre + RESUMEN_SQL_AGREGADOS_TOP. */
static void resumen_cargar_top(const char *sql, ResumenTop *top, int anio)
{
    sqlite3_stmt *stmt;

    memset(top, 0, sizeof(*top));
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, anio);
    while (top->cantidad < RESUMEN_TOP_N && sqlite3_step(stmt) == SQLITE_ROW)
    {
        ResumenTopItem *it = &top->items[top->cantidad++];
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);

        snprintf(it->nombre, sizeof(it->nombre), "%s", nombre ? nombre : "Sin nombre");
        it->partidos = sqlite3_column_int(stmt, 1);
        it->con_resultado = sqlite3_column_int(stmt, 2);
        it->victorias = sqlite3_column_int(stmt, 3);
        it->goles = sqlite3_column_int(stmt, 4);
        it->victorias_pct = resumen_pct(it->victorias, it->con_resultado);
    }
    sqlite3_finalize(stmt);
}

static void resumen_cargar_rankings(ResumenGeneral *r, int anio)
{
    resumen_cargar_top("SELECT c.nombre, " RESUMEN_SQL_AGREGADOS_TOP
                       "FROM partido p JOIN camiseta c ON c.id = p.camiseta_id "
                       "WHERE " RESUMEN_SQL_FILTRO_ANIO("p.fecha_hora")
                       " GROUP BY p.camiseta_id" RESUMEN_SQL_ORDEN_TOP,
                       &r->camisetas, anio);
    resumen_cargar_top("SELECT ca.nombre, " RESUMEN_SQL_AGREGADOS_TOP
                       "FROM partido p JOIN cancha ca ON ca.id = p.cancha_id "
                       "WHERE " RESUMEN_SQL_FILTRO_ANIO("p.fecha_hora")
                       " GROUP BY p.cancha_id" RESUMEN_SQL_ORDEN_TOP,
                       &r->canchas, anio);
    resumen_cargar_top("SELECT TRIM(p.rival_nombre), " RESUMEN_SQL_AGREGADOS_TOP
                       "FROM partido p WHERE TRIM(IFNULL(p.rival_nombre, '')) <> '' "
                       "AND " RESUMEN_SQL_FILTRO_ANIO("p.fecha_hora")
                       " GROUP BY TRIM(p.rival_nombre)" RESUMEN_SQL_ORDEN_TOP,
                       &r->rivales, anio);
}

/* Clima real de Open-Meteo: reutiliza el calculo de estadisticas_meta. */
static void resumen_cargar_clima_real(ResumenGeneral *r, int anio)
{
    ClimaRealStats stats;

    if (!calcular_rendimiento_clima_real_anio(&stats, anio))
    {
        return;
    }
    r->clima_real_partidos = stats.partidos_con_datos;
    r->clima_real_cobertura_pct = stats.cobertura_pct;
    if (stats.mejor)
    {
        r->hay_clima_mejor = 1;
        snprintf(r->clima_mejor, sizeof(r->clima_mejor), "%s",
                 stats.mejor->etiqueta ? stats.mejor->etiqueta : "");
        r->clima_mejor_partidos = stats.mejor->partidos;
        r->clima_mejor_rendimiento = stats.mejor->rendimiento_promedio;
        r->clima_mejor_victorias_pct = stats.mejor->victorias_pct;
    }
}

int resumen_general_calcular(ResumenGeneral *r)
{
    return resumen_general_calcular_anio(r, 0);
}

int resumen_general_calcular_anio(ResumenGeneral *r, int anio)
{
    if (!r || anio < 0)
    {
        return 0;
    }
    memset(r, 0, sizeof(*r));
    r->racha_tipo = 'X';

    obtener_nombre_jugador(r->jugador, sizeof(r->jugador));
    get_datetime(r->fecha_gen, sizeof(r->fecha_gen));

    if (!resumen_recorrer_partidos(r, anio))
    {
        return 0;
    }

    if (r->con_resultado > 0)
    {
        r->efectividad_pct = (double)(r->victorias * 3 + r->empates) * 100.0 /
                             (double)(r->con_resultado * 3);
    }
    if (r->partidos > 0)
    {
        r->goles_pp = (double)r->goles / r->partidos;
        r->asistencias_pp = (double)r->asistencias / r->partidos;
        r->ga_pp = (double)(r->goles + r->asistencias) / r->partidos;
    }

    resumen_cargar_rankings(r, anio);
    resumen_cargar_clima_real(r, anio);
    if (anio > 0)
    {
        /* Datos de carrera: el Resumen Anual no los usa (tiene los suyos). */
        return 1;
    }
    obtener_mejor_posicion(r->posicion, sizeof(r->posicion));
    obtener_clima_favorito(r->clima_frecuente, sizeof(r->clima_frecuente));
    obtener_torneo_favorito(r->torneo, sizeof(r->torneo));
    r->lesiones = contar_lesiones();
    r->logros_total = logros_get_total();
    r->logros_completados = r->partidos > 0 ? logros_get_completados_primera_camiseta() : 0;
    return 1;
}

/* Textos en UTF-8: los dos formatos (HTML con charset UTF-8 y Markdown) los
 * escriben tal cual. */
static void resumen_describir_racha(char tipo, const char **texto, const char **desc)
{
    switch (tipo)
    {
    case 'P':
        *texto = "Positiva";
        *desc = "Rendimiento destacado en los últimos partidos";
        break;
    case 'N':
        *texto = "Negativa";
        *desc = "Momento difícil, pero siempre se puede dar la vuelta";
        break;
    default:
        *texto = "Neutral";
        *desc = "Resultados mixtos en los últimos partidos";
        break;
    }
}

static const char *resumen_texto_resultado(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "Victoria";
    case 2:
        return "Empate";
    case 3:
        return "Derrota";
    default:
        return "Sin resultado";
    }
}

static char resumen_letra_resultado(int resultado)
{
    switch (resultado)
    {
    case 1:
        return 'V';
    case 2:
        return 'E';
    case 3:
        return 'D';
    default:
        return '-';
    }
}

/* Nombres para HTML, con entidades para los acentos. */
static const char *const RESUMEN_DIAS_SEMANA[7] =
{
    "Domingo", "Lunes", "Martes", "Mi&eacute;rcoles", "Jueves", "Viernes", "S&aacute;bado"
};

/* Franjas de la columna partido.dia (1..6), mismo orden que dia_to_text(). */
static const char *const RESUMEN_FRANJAS_HTML[RESUMEN_FRANJAS] =
{
    "Madrugada", "Ma&ntilde;ana", "Mediod&iacute;a", "Tarde", "Atardecer", "Noche"
};

/* Numero con coma decimal en un anillo de buffers, para usar varios en el
 * mismo fprintf (como html_texto). */
static const char *rnum(double valor, int decimales)
{
    static char slots[12][32];
    static unsigned siguiente = 0;
    unsigned slot = siguiente % 12U;

    siguiente = (siguiente + 1U) % 12U;
    format_double_es(valor, slots[slot], sizeof(slots[slot]), decimales);
    return slots[slot];
}

/* obtener_torneo_favorito() devuelve un aviso cuando no hay torneos. */
static int resumen_hay_torneo(const ResumenGeneral *r)
{
    return r->torneo[0] && strncmp(r->torneo, "Ningun", 6) != 0 &&
           strcmp(r->torneo, "Sin datos") != 0;
}

static int resumen_max_conteo(const ResumenConteo *c, int n)
{
    int max = 0;
    for (int i = 0; i < n; i++)
    {
        if (c[i].partidos > max)
        {
            max = c[i].partidos;
        }
    }
    return max;
}

/* ─── Estilo comun de los resumenes HTML ──────────────────────────────────── */

/*
 * Mundo visual "planilla de partido": papel tiza y tinta verde cancha en modo
 * claro, cancha de noche con reflectores en modo oscuro. El hero siempre es
 * verde cancha con las lineas del campo dibujadas en SVG. Todo autocontenido:
 * fuentes del sistema (Bahnschrift en Windows para los numeros y titulos),
 * sin CDNs ni JavaScript.
 *
 * Tokens (variables CSS en :root, redefinidas en oscuro e impresion):
 *   --bg --surface          fondo de pagina y de panel
 *   --ink --ink-2 --ink-3   texto principal, secundario y terciario
 *   --line --line-2         bordes de panel y separadores internos
 *   --pitch --pitch-ink     fondo y texto del hero
 *   --accent                acento (lineas, sparkline, links)
 *   --win --draw --loss     victoria, empate, derrota
 *   --bar --bar-2           barras primaria (goles) y secundaria (asistencias)
 *   --display --text        familias tipograficas de titulos/numeros y texto
 *
 * Componentes: .pagina, .hero (.hero-campo, .bajada, .marcador, .forma,
 * .chip-v/e/d/x), .grilla (.panel, .panel-ancho), .cifras/.cifra, .filas,
 * .resultados (.donut, .leyenda, .punto), .spark, .anios, .tres/.ranking,
 * .barras, .medidor, .vacio, .pie.
 *
 * Se divide en tramos porque ISO C99 solo garantiza literales de 4095 bytes.
 */
static const char *const RESUMEN_CSS_TRAMOS[] =
{
    ":root {\n"
    "  color-scheme: light dark;\n"
    "  --bg: #f3f0e8; --surface: #fffdf8; --ink: #17201b; --ink-2: #48544d;\n"
    "  --ink-3: #66716a; --line: #dcd5c6; --line-2: #ebe5d9;\n"
    "  --pitch: #135c3a; --pitch-ink: #f3f7f2; --accent: #13683f;\n"
    "  --win: #1d7f47; --draw: #b07214; --loss: #b8342a;\n"
    "  --bar: #13683f; --bar-2: #c08a2c;\n"
    "  --display: \"Bahnschrift SemiCondensed\", \"Bahnschrift\", \"DIN Alternate\",\n"
    "             \"Roboto Condensed\", \"Arial Narrow\", sans-serif;\n"
    "  --text: \"Segoe UI\", system-ui, -apple-system, \"Helvetica Neue\", Roboto,\n"
    "          Arial, sans-serif;\n"
    "}\n"
    "@media (prefers-color-scheme: dark) {\n"
    "  :root {\n"
    "    --bg: #0c120f; --surface: #131b17; --ink: #e7eee9; --ink-2: #a8b5ad;\n"
    "    --ink-3: #8a978f; --line: #24302a; --line-2: #1d2722;\n"
    "    --pitch: #0f3f28; --pitch-ink: #eef5ef; --accent: #5fcf95;\n"
    "    --win: #52c489; --draw: #e2b04f; --loss: #f07466;\n"
    "    --bar: #5fcf95; --bar-2: #e2b04f;\n"
    "  }\n"
    "}\n"
    "* { box-sizing: border-box; margin: 0; padding: 0; }\n"
    "html { -webkit-text-size-adjust: 100%; text-size-adjust: 100%; }\n"
    "body { background: var(--bg); color: var(--ink); font: 16px/1.5 var(--text);\n"
    "       padding: 24px 16px 40px; }\n"
    "::selection { background: var(--accent); color: var(--surface); }\n"
    "a { color: var(--accent); text-underline-offset: 3px; }\n"
    "a:focus-visible { outline: 2px solid var(--accent); outline-offset: 3px;\n"
    "                  border-radius: 2px; }\n"
    ".pagina { max-width: 980px; margin: 0 auto; }\n"
    ".hero { position: relative; overflow: hidden; background: var(--pitch);\n"
    "        color: var(--pitch-ink); border-radius: 10px; padding: 28px 24px 22px;\n"
    "        -webkit-print-color-adjust: exact; print-color-adjust: exact; }\n"
    ".hero-campo { position: absolute; inset: 0; width: 100%; height: 100%;\n"
    "              opacity: .13; pointer-events: none; }\n"
    ".hero-campo * { fill: none; stroke: currentColor; stroke-width: 2; }\n"
    ".hero > * { position: relative; }\n"
    ".hero-campo { position: absolute; }\n"
    ".hero h1 { font-family: var(--display); font-weight: 700; text-transform: uppercase;\n"
    "           font-size: clamp(2.3rem, 8vw, 4.4rem); line-height: .95;\n"
    "           letter-spacing: -.01em; text-wrap: balance; overflow-wrap: anywhere; }\n"
    ".bajada { margin-top: 10px; color: rgba(243, 247, 242, .84); font-size: .95rem;\n"
    "          max-width: 60ch; }\n"
    ".marcador { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr));\n"
    "            margin-top: 26px; border-top: 1px solid rgba(243, 247, 242, .28); }\n"
    ".marcador div { padding: 14px 12px 2px 0; min-width: 0; }\n"
    ".marcador div + div { border-left: 1px solid rgba(243, 247, 242, .28);\n"
    "                      padding-left: 16px; }\n"
    ".marcador b { display: block; font-family: var(--display); font-weight: 600;\n"
    "              font-size: clamp(2rem, 6.5vw, 3.2rem); line-height: 1;\n"
    "              font-variant-numeric: tabular-nums; }\n"
    ".marcador span { display: block; margin-top: 6px; font-size: .78rem;\n"
    "                 letter-spacing: .06em; text-transform: uppercase;\n"
    "                 color: rgba(243, 247, 242, .8); }\n"
    ".forma { display: flex; flex-wrap: wrap; align-items: center; gap: 6px;\n"
    "         margin-top: 22px; }\n"
    ".forma-titulo { margin-right: 6px; font-size: .78rem; letter-spacing: .06em;\n"
    "                text-transform: uppercase; color: rgba(243, 247, 242, .8); }\n"
    ".chip { display: inline-grid; place-items: center; width: 28px; height: 28px;\n"
    "        border-radius: 4px; font: 700 .9rem/1 var(--display); }\n"
    ".chip-v { background: #8fe0b0; color: #0b2a1a; }\n"
    ".chip-e { background: #f0c96a; color: #3a2a05; }\n"
    ".chip-d { background: #f49a8f; color: #3d0e09; }\n"
    ".chip-x { background: rgba(243, 247, 242, .22); color: #f3f7f2; }\n",

    ".grilla { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr));\n"
    "          gap: 16px; margin-top: 16px; }\n"
    ".panel { background: var(--surface); border: 1px solid var(--line);\n"
    "         border-radius: 6px; padding: 20px; min-width: 0; break-inside: avoid; }\n"
    ".panel-ancho { grid-column: 1 / -1; }\n"
    ".panel h2 { font-family: var(--display); font-weight: 600; font-size: 1.4rem;\n"
    "            line-height: 1.1; margin-bottom: 16px; text-wrap: balance; }\n"
    ".panel h3 { font-size: .8rem; font-weight: 600; letter-spacing: .05em;\n"
    "            text-transform: uppercase; color: var(--ink-3); margin: 0 0 10px; }\n"
    ".nota { margin-top: 12px; color: var(--ink-3); font-size: .84rem; }\n"
    ".vacio { color: var(--ink-2); }\n"
    ".cifras { display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr));\n"
    "          gap: 16px 18px; }\n"
    ".cifra b { display: block; font-family: var(--display); font-weight: 600;\n"
    "           font-size: 2rem; line-height: 1; font-variant-numeric: tabular-nums; }\n"
    ".cifra span { display: block; margin-top: 5px; color: var(--ink-2); font-size: .84rem;\n"
    "              line-height: 1.3; }\n"
    ".filas { list-style: none; }\n"
    ".filas li { display: flex; justify-content: space-between; align-items: baseline;\n"
    "            gap: 14px; padding: 9px 0; border-top: 1px solid var(--line-2); }\n"
    ".filas li:first-child { border-top: 0; padding-top: 0; }\n"
    ".filas .k { color: var(--ink-2); }\n"
    ".filas .v { font-weight: 600; text-align: right; overflow-wrap: anywhere;\n"
    "            font-variant-numeric: tabular-nums; }\n"
    ".filas small { display: block; color: var(--ink-3); font-weight: 400;\n"
    "               font-size: .8rem; }\n"
    ".resultados { display: flex; flex-wrap: wrap; align-items: center; gap: 22px; }\n"
    ".donut { width: 136px; height: 136px; flex: none; }\n"
    ".donut circle { fill: none; stroke-width: 5.2; }\n"
    ".donut .pista { stroke: var(--line-2); }\n"
    ".donut .sv { stroke: var(--win); } .donut .se { stroke: var(--draw); }\n"
    ".donut .sd { stroke: var(--loss); }\n"
    ".donut text { fill: var(--ink); font-family: var(--display); text-anchor: middle; }\n"
    ".donut .grande { font-size: 8.5px; font-weight: 600; }\n"
    ".donut .chico { font-size: 3.4px; fill: var(--ink-2); letter-spacing: .04em; }\n"
    ".leyenda { list-style: none; flex: 1; min-width: 170px; }\n"
    ".leyenda li { display: flex; align-items: center; gap: 9px; padding: 6px 0;\n"
    "              border-top: 1px solid var(--line-2); }\n"
    ".leyenda li:first-child { border-top: 0; }\n"
    ".leyenda .n { margin-left: auto; font-weight: 600; font-variant-numeric: tabular-nums; }\n"
    ".leyenda .pc { width: 3.4rem; text-align: right; color: var(--ink-3);\n"
    "               font-variant-numeric: tabular-nums; }\n"
    ".punto { width: 10px; height: 10px; border-radius: 2px; flex: none; }\n"
    ".p-v { background: var(--win); } .p-e { background: var(--draw); }\n"
    ".p-d { background: var(--loss); }\n"
    ".spark { display: block; width: 100%; height: 150px; }\n"
    ".spark * { vector-effect: non-scaling-stroke; }\n"
    ".spark .area { fill: var(--accent); opacity: .13; }\n"
    ".spark .linea { fill: none; stroke: var(--accent); stroke-width: 2.5;\n"
    "                stroke-linejoin: round; stroke-linecap: round; }\n"
    ".spark .prom { stroke: var(--ink-3); stroke-width: 1.2; stroke-dasharray: 5 5; }\n"
    ".spark .guia { stroke: var(--line-2); stroke-width: 1; }\n"
    ".spark .ultimo { stroke: var(--accent); stroke-width: 11; stroke-linecap: round; }\n"
    ".spark-pie { display: flex; justify-content: space-between; gap: 10px;\n"
    "             margin-top: 6px; color: var(--ink-3); font-size: .8rem; }\n",

    ".anios { list-style: none; }\n"
    ".anios li { display: grid; grid-template-columns: 3.4rem minmax(0, 1fr) auto;\n"
    "            gap: 12px; align-items: center; padding: 7px 0; }\n"
    ".anios .anio { font-family: var(--display); font-weight: 600; font-size: 1.15rem;\n"
    "               font-variant-numeric: tabular-nums; }\n"
    ".anios svg { display: block; width: 100%; height: 22px; }\n"
    ".anios .bg { fill: var(--bar); } .anios .ba { fill: var(--bar-2); }\n"
    ".anios .det { color: var(--ink-2); font-size: .84rem; white-space: nowrap;\n"
    "              font-variant-numeric: tabular-nums; }\n"
    ".clave { display: flex; flex-wrap: wrap; gap: 6px 16px; margin-top: 10px;\n"
    "         color: var(--ink-2); font-size: .82rem; }\n"
    ".clave span { display: inline-flex; align-items: center; gap: 6px; }\n"
    ".c-g { background: var(--bar); } .c-a { background: var(--bar-2); }\n"
    ".tres { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));\n"
    "        gap: 26px; }\n"
    ".ranking { list-style: none; }\n"
    ".ranking li { padding: 9px 0; border-top: 1px solid var(--line-2); }\n"
    ".ranking li:first-child { border-top: 0; padding-top: 0; }\n"
    ".ranking .fila { display: flex; justify-content: space-between; align-items: baseline;\n"
    "                 gap: 10px; }\n"
    ".ranking .nombre { font-weight: 600; overflow-wrap: anywhere; }\n"
    ".ranking .dato { color: var(--ink-2); font-size: .84rem; white-space: nowrap;\n"
    "                 font-variant-numeric: tabular-nums; }\n"
    ".medidor { height: 6px; margin-top: 7px; background: var(--line-2);\n"
    "           border-radius: 3px; overflow: hidden; }\n"
    ".medidor i { display: block; height: 100%; background: var(--bar); border-radius: 3px; }\n"
    ".medidor.alto { height: 10px; border-radius: 5px; }\n"
    ".barras { list-style: none; }\n"
    ".barras li { display: grid; grid-template-columns: 6.6rem minmax(0, 1fr) 2.6rem;\n"
    "             gap: 10px; align-items: center; padding: 5px 0; font-size: .9rem; }\n"
    ".barras .k { color: var(--ink-2); }\n"
    ".barras .n { text-align: right; font-weight: 600; font-variant-numeric: tabular-nums; }\n"
    ".barras .medidor { margin-top: 0; height: 10px; border-radius: 5px; }\n"
    ".dos { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 26px; }\n"
    ".logros { margin-top: 18px; }\n"
    ".logros p { color: var(--ink-2); font-variant-numeric: tabular-nums; }\n"
    ".logros b { font-family: var(--display); font-size: 1.6rem; font-weight: 600;\n"
    "            color: var(--ink); }\n"
    ".pie { margin-top: 26px; text-align: center; color: var(--ink-3); font-size: .8rem; }\n"
    ".pie a { color: var(--ink-2); }\n"
    "@media (max-width: 760px) {\n"
    "  .grilla, .tres, .dos { grid-template-columns: minmax(0, 1fr); }\n"
    "  .tres, .dos { gap: 20px; }\n"
    "}\n"
    "@media (max-width: 560px) {\n"
    "  body { padding: 16px 16px 32px; }\n"
    "  .hero { padding: 22px 18px 18px; }\n"
    "  .marcador { grid-template-columns: repeat(2, minmax(0, 1fr)); }\n"
    "  .marcador div:nth-child(3) { border-left: 0; padding-left: 0; }\n"
    "  .panel { padding: 18px 16px; }\n"
    "  .forma { gap: 4px; }\n"
    "  .forma-titulo { flex-basis: 100%; margin: 0 0 2px; }\n"
    "  .chip { width: 25px; height: 25px; font-size: .82rem; }\n"
    "  .spark { height: 120px; }\n"
    "  .anios li { grid-template-columns: 3rem minmax(0, 1fr); row-gap: 2px; }\n"
    "  .anios .det { grid-column: 2; }\n"
    "}\n"
    "@media print {\n"
    "  :root { --bg: #fff; --surface: #fff; --ink: #111; --ink-2: #333; --ink-3: #555;\n"
    "          --line: #bbb; --line-2: #ddd; --accent: #13683f; --win: #1d7f47;\n"
    "          --draw: #b07214; --loss: #b8342a; --bar: #13683f; --bar-2: #c08a2c;\n"
    "          --pitch: #135c3a; }\n"
    "  body { padding: 0; font-size: 12px; }\n"
    "  .grilla { gap: 10px; }\n"
    "  .panel, .hero { break-inside: avoid; }\n"
    "  .compartir { display: none; }\n"
    "  @page { margin: 12mm; }\n"
    "}\n",

    /* Componentes del Resumen Anual (el resumen general no los usa). */
    ".antetitulo { margin-bottom: 6px; font-size: .8rem; font-weight: 600; letter-spacing: .08em;\n"
    "              text-transform: uppercase; color: rgba(243, 247, 242, .82); }\n"
    ".grilla-densa { grid-auto-flow: row dense; }\n"
    ".comparar { width: 100%; border-collapse: collapse; font-size: .92rem;\n"
    "            font-variant-numeric: tabular-nums; }\n"
    ".comparar th, .comparar td { padding: 8px 0 8px 10px; border-top: 1px solid var(--line-2);\n"
    "                             text-align: right; white-space: nowrap; }\n"
    ".comparar th:first-child, .comparar td:first-child { padding-left: 0; text-align: left;\n"
    "                             white-space: normal; color: var(--ink-2); font-weight: 400; }\n"
    ".comparar thead th { border-top: 0; padding-top: 0; font-size: .74rem; font-weight: 600;\n"
    "                     letter-spacing: .05em; text-transform: uppercase; color: var(--ink-3); }\n"
    ".comparar .actual { font-weight: 600; }\n"
    ".comparar .previo { color: var(--ink-3); }\n"
    ".delta { font-weight: 600; }\n"
    ".sube { color: var(--win); } .baja { color: var(--loss); } .igual { color: var(--ink-3); }\n"
    ".anios li.sin { opacity: .45; }\n"
    ".anios li.destacado .anio { color: var(--accent); }\n"
    ".marca { display: inline-block; margin-left: 6px; padding: 1px 6px; border-radius: 3px;\n"
    "         background: var(--accent); color: var(--surface); font-size: .68rem;\n"
    "         font-weight: 600; letter-spacing: .04em; text-transform: uppercase; }\n"
    ".columnas { display: block; width: 100%; height: 130px; }\n"
    ".columnas .c { fill: var(--bar); opacity: .45; }\n"
    ".columnas .c.top { opacity: 1; }\n"
    ".columnas .guia { stroke: var(--line-2); stroke-width: 1; vector-effect: non-scaling-stroke; }\n"
    ".ejes { display: grid; grid-template-columns: repeat(12, minmax(0, 1fr)); margin-top: 6px;\n"
    "        text-align: center; color: var(--ink-3); font-size: .72rem; }\n"
    ".ejes .top { color: var(--accent); font-weight: 700; }\n"
    ".ficha { list-style: none; }\n"
    ".ficha li { display: grid; grid-template-columns: 9.5rem minmax(0, 1fr); gap: 12px;\n"
    "            padding: 8px 0; border-top: 1px solid var(--line-2); }\n"
    ".ficha li:first-child { border-top: 0; padding-top: 0; }\n"
    ".ficha .k { color: var(--ink-2); }\n"
    ".ficha .v { overflow-wrap: anywhere; }\n"
    ".cifras + .ficha { margin-top: 18px; }\n"
    "@media (max-width: 560px) {\n"
    "  .ficha li { grid-template-columns: minmax(0, 1fr); gap: 2px; }\n"
    "  .comparar { font-size: .86rem; }\n"
    "  .comparar th, .comparar td { padding-left: 6px; }\n"
    "  .columnas { height: 110px; }\n"
    "}\n"
};

void resumen_escribir_css_comun(FILE *f)
{
    for (size_t i = 0; i < sizeof(RESUMEN_CSS_TRAMOS) / sizeof(RESUMEN_CSS_TRAMOS[0]); i++)
    {
        fputs(RESUMEN_CSS_TRAMOS[i], f);
    }
}

/* ─── Resumen general: HTML ───────────────────────────────────────────────── */

static const char *resumen_clase_chip(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "chip-v";
    case 2:
        return "chip-e";
    case 3:
        return "chip-d";
    default:
        return "chip-x";
    }
}

/* Lineas del campo de fondo del hero (comun a los dos resumenes). */
static void resumen_html_campo(FILE *f)
{
    fputs("<svg class=\"hero-campo\" viewBox=\"0 0 1000 320\" "
          "preserveAspectRatio=\"xMidYMid slice\" aria-hidden=\"true\">"
          "<rect x=\"20\" y=\"20\" width=\"960\" height=\"280\"/>"
          "<line x1=\"500\" y1=\"20\" x2=\"500\" y2=\"300\"/>"
          "<circle cx=\"500\" cy=\"160\" r=\"74\"/>"
          "<rect x=\"20\" y=\"80\" width=\"130\" height=\"160\"/>"
          "<rect x=\"850\" y=\"80\" width=\"130\" height=\"160\"/>"
          "<rect x=\"20\" y=\"125\" width=\"44\" height=\"70\"/>"
          "<rect x=\"936\" y=\"125\" width=\"44\" height=\"70\"/>"
          "</svg>\n",
          f);
}

/* Marcador de 4 numeros y chips de forma del hero (comun a los dos resumenes). */
static void resumen_html_marcador_y_forma(FILE *f, const ResumenGeneral *r)
{
    fprintf(f,
            "<div class=\"marcador\">\n"
            "  <div><b>%d</b><span>Partidos</span></div>\n"
            "  <div><b>%d</b><span>Goles</span></div>\n"
            "  <div><b>%d</b><span>Asistencias</span></div>\n",
            r->partidos, r->goles, r->asistencias);
    if (r->con_resultado > 0)
    {
        fprintf(f, "  <div><b>%s%%</b><span>Efectividad</span></div>\n",
                rnum(r->efectividad_pct, 0));
    }
    else
    {
        fputs("  <div><b>&ndash;</b><span>Efectividad</span></div>\n", f);
    }
    fputs("</div>\n", f);

    if (r->ultimos_n > 0)
    {
        fprintf(f, "<div class=\"forma\"><span class=\"forma-titulo\">&Uacute;ltimos %d</span>",
                r->ultimos_n);
        for (int i = 0; i < r->ultimos_n; i++)
        {
            const ResumenPartidoCorto *p = &r->ultimos[i];
            fprintf(f,
                    "<span class=\"chip %s\" title=\"%s%s%s: %s, %d G %d A\" "
                    "aria-label=\"%s del %s\">%c</span>",
                    resumen_clase_chip(p->resultado), html_texto(p->fecha),
                    p->rival[0] ? " vs " : "", html_texto(p->rival),
                    resumen_texto_resultado(p->resultado), p->goles, p->asistencias,
                    resumen_texto_resultado(p->resultado), html_texto(p->fecha),
                    resumen_letra_resultado(p->resultado));
        }
        fputs("</div>\n", f);
    }
}

static void resumen_html_hero(FILE *f, const ResumenGeneral *r)
{
    fputs("<header class=\"hero\">\n", f);
    resumen_html_campo(f);
    fprintf(f, "<h1>%s</h1>\n", html_texto(r->jugador));

    if (r->partidos > 0)
    {
        fprintf(f,
                "<p class=\"bajada\">Perfil de carrera en MiFutbolC &middot; %d partido%s "
                "desde el %s</p>\n",
                r->partidos, r->partidos == 1 ? "" : "s", html_texto(r->primer_partido_fecha));
    }
    else
    {
        fputs("<p class=\"bajada\">Perfil de carrera en MiFutbolC</p>\n", f);
    }

    resumen_html_marcador_y_forma(f, r);
    fputs("</header>\n", f);
}

/* Segmento de la dona: circunferencia 100 (r = 15.9155), arranca a las 12. */
static void resumen_svg_segmento(FILE *f, const char *clase, double pct, double desde)
{
    if (pct <= 0.0)
    {
        return;
    }
    fprintf(f,
            "<circle class=\"%s\" cx=\"21\" cy=\"21\" r=\"15.9155\" "
            "stroke-dasharray=\"%.2f %.2f\" stroke-dashoffset=\"%.2f\"/>",
            clase, pct, 100.0 - pct, 25.0 - desde);
}

static void resumen_html_resultados(FILE *f, const ResumenGeneral *r)
{
    double pv = resumen_pct(r->victorias, r->con_resultado);
    double pe = resumen_pct(r->empates, r->con_resultado);
    double pd = resumen_pct(r->derrotas, r->con_resultado);

    if (r->con_resultado == 0)
    {
        return;
    }

    fputs("<section class=\"panel\">\n<h2>Resultados</h2>\n<div class=\"resultados\">\n"
          "<svg class=\"donut\" viewBox=\"0 0 42 42\" role=\"img\" "
          "aria-label=\"Distribuci&oacute;n de victorias, empates y derrotas\">"
          "<circle class=\"pista\" cx=\"21\" cy=\"21\" r=\"15.9155\"/>",
          f);
    resumen_svg_segmento(f, "sv", pv, 0.0);
    resumen_svg_segmento(f, "se", pe, pv);
    resumen_svg_segmento(f, "sd", pd, pv + pe);
    fprintf(f,
            "<text class=\"grande\" x=\"21\" y=\"23.2\">%s%%</text>"
            "<text class=\"chico\" x=\"21\" y=\"28.2\">GANADOS</text></svg>\n"
            "<ul class=\"leyenda\">\n"
            "<li><span class=\"punto p-v\"></span>Victorias<span class=\"n\">%d</span>"
            "<span class=\"pc\">%s%%</span></li>\n"
            "<li><span class=\"punto p-e\"></span>Empates<span class=\"n\">%d</span>"
            "<span class=\"pc\">%s%%</span></li>\n"
            "<li><span class=\"punto p-d\"></span>Derrotas<span class=\"n\">%d</span>"
            "<span class=\"pc\">%s%%</span></li>\n"
            "</ul>\n</div>\n"
            "<p class=\"nota\">Efectividad %s%% de los puntos en juego (3 por victoria, 1 por "
            "empate) sobre %d partido%s con resultado.</p>\n</section>\n",
            rnum(pv, 0), r->victorias, rnum(pv, 1), r->empates, rnum(pe, 1), r->derrotas,
            rnum(pd, 1), rnum(r->efectividad_pct, 1), r->con_resultado,
            r->con_resultado == 1 ? "" : "s");
}

static void resumen_html_cifra(FILE *f, const char *valor, const char *etiqueta)
{
    fprintf(f, "<div class=\"cifra\"><b>%s</b><span>%s</span></div>\n", valor, etiqueta);
}

static void resumen_html_ataque(FILE *f, const ResumenGeneral *r)
{
    char buf[32];

    if (r->partidos == 0)
    {
        return;
    }
    fputs("<section class=\"panel\">\n<h2>Ataque</h2>\n<div class=\"cifras\">\n", f);
    resumen_html_cifra(f, rnum(r->goles + r->asistencias, 0), "Goles + asistencias");
    resumen_html_cifra(f, rnum(r->ga_pp, 2), "G+A por partido");
    resumen_html_cifra(f, rnum(r->goles_pp, 2), "Goles por partido");
    resumen_html_cifra(f, rnum(r->asistencias_pp, 2), "Asistencias por partido");
    snprintf(buf, sizeof(buf), "%d", r->partidos_con_gol);
    resumen_html_cifra(f, buf, "Partidos con gol");
    snprintf(buf, sizeof(buf), "%d", r->dobletes);
    resumen_html_cifra(f, buf, "Dobletes");
    snprintf(buf, sizeof(buf), "%d", r->hat_tricks);
    resumen_html_cifra(f, buf, "Hat-tricks o m&aacute;s");
    if (r->partidos_con_minutos > 0)
    {
        resumen_html_cifra(f, rnum(r->ga_por_90, 2), "G+A cada 90 minutos");
    }
    fputs("</div>\n", f);
    if (r->partidos_con_minutos > 0)
    {
        fprintf(f,
                "<p class=\"nota\">G+A cada 90' sobre %d partido%s con minutos cargados "
                "(%d minutos).</p>\n",
                r->partidos_con_minutos, r->partidos_con_minutos == 1 ? "" : "s",
                r->minutos_total);
    }
    fprintf(f, "<p class=\"nota\">Marcaste en el %s%% de los partidos.</p>\n</section>\n",
            rnum(resumen_pct(r->partidos_con_gol, r->partidos), 0));
}

/* SVG de la sparkline del rendimiento (1..10); devuelve el promedio de @p v.
 * Requiere n >= 2. La usan la tendencia del resumen general y la del anual. */
static double resumen_html_spark_svg(FILE *f, const int *v, int n)
{
    const double ancho = 600.0;
    const double alto = 140.0;
    const double margen = 12.0;
    double paso = (ancho - 2.0 * margen) / (double)(n - 1);
    double suma = 0.0;
    double y_prom;
    double ux = 0.0;
    double uy = 0.0;

    fprintf(f,
            "<svg class=\"spark\" viewBox=\"0 0 600 140\" preserveAspectRatio=\"none\" "
            "role=\"img\" "
            "aria-label=\"Rendimiento de %d partidos, escala 1 a 10\">"
            "<line class=\"guia\" x1=\"0\" y1=\"%.1f\" x2=\"600\" y2=\"%.1f\"/>"
            "<line class=\"guia\" x1=\"0\" y1=\"%.1f\" x2=\"600\" y2=\"%.1f\"/>",
            n, margen, margen, alto - margen, alto - margen);

    /* Area bajo la curva y linea con los mismos puntos. */
    fprintf(f, "<path class=\"area\" d=\"M%.1f %.1f", margen, alto - margen);
    for (int i = 0; i < n; i++)
    {
        double x = margen + paso * i;
        double y = margen + (10.0 - v[i]) / 9.0 * (alto - 2.0 * margen);
        fprintf(f, " L%.1f %.1f", x, y);
        suma += v[i];
    }
    fprintf(f, " L%.1f %.1f Z\"/>", margen + paso * (n - 1), alto - margen);

    y_prom = margen + (10.0 - suma / n) / 9.0 * (alto - 2.0 * margen);
    fprintf(f, "<line class=\"prom\" x1=\"0\" y1=\"%.1f\" x2=\"600\" y2=\"%.1f\"/>", y_prom,
            y_prom);

    fputs("<polyline class=\"linea\" points=\"", f);
    for (int i = 0; i < n; i++)
    {
        ux = margen + paso * i;
        uy = margen + (10.0 - v[i]) / 9.0 * (alto - 2.0 * margen);
        fprintf(f, "%s%.1f,%.1f", i ? " " : "", ux, uy);
    }
    /* El SVG se estira (preserveAspectRatio none): el punto final es una linea
     * de largo 0 con extremo redondo, que no se deforma como un circulo. */
    fprintf(f,
            "\"/><line class=\"ultimo\" x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\"/>"
            "</svg>\n",
            ux, uy, ux, uy);
    return suma / n;
}

/* Sparkline del rendimiento (1..10) de los ultimos partidos con dato. */
static void resumen_html_tendencia(FILE *f, const ResumenGeneral *r)
{
    double prom;

    if (r->tendencia_n < 2)
    {
        return;
    }
    fputs("<section class=\"panel panel-ancho\">\n<h2>Tendencia de rendimiento</h2>\n", f);
    prom = resumen_html_spark_svg(f, r->tendencia, r->tendencia_n);
    fprintf(f,
            "<div class=\"spark-pie\"><span>Hace %d partidos</span>"
            "<span>Promedio %s &middot; &uacute;ltimo %d/10</span><span>&Uacute;ltimo</span></div>\n"
            "<p class=\"nota\">Rendimiento general (1 a 10) de los &uacute;ltimos %d partidos con "
            "dato; la linea punteada es su promedio.</p>\n</section>\n",
            r->tendencia_n, rnum(prom, 1), r->tendencia[r->tendencia_n - 1], r->tendencia_n);
}

static void resumen_html_fila_partido(FILE *f, const char *k, const char *valor,
                                      const ResumenPartidoCorto *p)
{
    fprintf(f, "<li><span class=\"k\">%s</span><span class=\"v\">%s<small>%s%s%s</small>"
            "</span></li>\n",
            k, valor, html_texto(p->fecha), p->rival[0] ? " vs " : "", html_texto(p->rival));
}

static void resumen_html_racha_fila(FILE *f, const char *k, int n)
{
    fprintf(f, "<li><span class=\"k\">%s</span><span class=\"v\">%d partido%s</span></li>\n", k,
            n, n == 1 ? "" : "s");
}

static void resumen_html_records(FILE *f, const ResumenGeneral *r)
{
    char buf[48];

    if (r->partidos == 0)
    {
        return;
    }
    fputs("<section class=\"panel\">\n<h2>R&eacute;cords</h2>\n<ul class=\"filas\">\n", f);
    if (r->con_resultado > 0)
    {
        resumen_html_racha_fila(f, "Mejor racha de victorias", r->mejor_racha_victorias);
        resumen_html_racha_fila(f, "Mejor racha invicto", r->mejor_racha_invicto);
    }
    resumen_html_racha_fila(f, "Racha m&aacute;s larga marcando", r->mejor_racha_marcando);
    if (r->max_goles.goles > 0)
    {
        snprintf(buf, sizeof(buf), "%d gol%s", r->max_goles.goles,
                 r->max_goles.goles == 1 ? "" : "es");
        resumen_html_fila_partido(f, "M&aacute;s goles en un partido", buf, &r->max_goles);
    }
    if (r->max_asistencias.asistencias > 0)
    {
        snprintf(buf, sizeof(buf), "%d asistencia%s", r->max_asistencias.asistencias,
                 r->max_asistencias.asistencias == 1 ? "" : "s");
        resumen_html_fila_partido(f, "M&aacute;s asistencias", buf, &r->max_asistencias);
    }
    if (r->mejor_rendimiento.rendimiento > 0)
    {
        snprintf(buf, sizeof(buf), "%d/10", r->mejor_rendimiento.rendimiento);
        resumen_html_fila_partido(f, "Mejor rendimiento", buf, &r->mejor_rendimiento);
    }
    fputs("</ul>\n</section>\n", f);
}

static void resumen_html_actualidad(FILE *f, const ResumenGeneral *r)
{
    const char *texto;
    const char *desc;
    char buf[64];

    if (r->partidos == 0)
    {
        return;
    }
    resumen_describir_racha(r->racha_tipo, &texto, &desc);
    fputs("<section class=\"panel\">\n<h2>Momento actual</h2>\n<ul class=\"filas\">\n", f);
    fprintf(f,
            "<li><span class=\"k\">Racha</span><span class=\"v\">%s: %d partido%s"
            "<small>%s</small></span></li>\n",
            texto, r->racha_longitud, r->racha_longitud == 1 ? "" : "s", desc);
    if (r->con_resultado > 0)
    {
        resumen_html_racha_fila(f, "Victorias seguidas ahora", r->racha_victorias_actual);
    }
    snprintf(buf, sizeof(buf), "%s &middot; %d G, %d A",
             resumen_texto_resultado(r->ultimo.resultado), r->ultimo.goles,
             r->ultimo.asistencias);
    resumen_html_fila_partido(f, "&Uacute;ltimo partido", buf, &r->ultimo);
    fputs("</ul>\n</section>\n", f);
}

static void resumen_html_anios(FILE *f, const ResumenGeneral *r)
{
    int max = 1;

    if (r->num_anios == 0)
    {
        return;
    }
    for (int i = 0; i < r->num_anios; i++)
    {
        int total = r->anios[i].goles + r->anios[i].asistencias;
        if (total > max)
        {
            max = total;
        }
    }

    fputs("<section class=\"panel\">\n<h2>A&ntilde;o por a&ntilde;o</h2>\n<ul class=\"anios\">\n", f);
    for (int i = r->num_anios - 1; i >= 0; i--)
    {
        const ResumenAnioItem *a = &r->anios[i];
        double wg = (double)a->goles * 100.0 / max;
        double wa = (double)a->asistencias * 100.0 / max;

        fprintf(f,
                "<li><span class=\"anio\">%d</span>"
                "<svg viewBox=\"0 0 100 22\" preserveAspectRatio=\"none\" role=\"img\" "
                "aria-label=\"%d goles y %d asistencias\">"
                "<rect class=\"bg\" x=\"0\" y=\"2\" width=\"%.2f\" height=\"18\" rx=\"1\"/>"
                "<rect class=\"ba\" x=\"%.2f\" y=\"2\" width=\"%.2f\" height=\"18\" rx=\"1\"/>"
                "</svg>"
                "<span class=\"det\">%d PJ &middot; %d G &middot; %d A</span></li>\n",
                a->anio, a->goles, a->asistencias, wg, wg, wa, a->partidos, a->goles,
                a->asistencias);
    }
    fputs("</ul>\n<div class=\"clave\"><span><i class=\"punto c-g\"></i>Goles</span>"
          "<span><i class=\"punto c-a\"></i>Asistencias</span></div>\n</section>\n",
          f);
}

static void resumen_html_ranking(FILE *f, const char *titulo, const ResumenTop *top)
{
    if (top->cantidad == 0)
    {
        return;
    }
    fprintf(f, "<div>\n<h3>%s</h3>\n<ul class=\"ranking\">\n", titulo);
    for (int i = 0; i < top->cantidad; i++)
    {
        const ResumenTopItem *it = &top->items[i];
        fprintf(f,
                "<li><div class=\"fila\"><span class=\"nombre\">%s</span>"
                "<span class=\"dato\">%d PJ &middot; %s%% V</span></div>"
                "<div class=\"medidor\"><i style=\"width:%.1f%%\"></i></div></li>\n",
                html_texto(it->nombre), it->partidos, rnum(it->victorias_pct, 0),
                it->victorias_pct);
    }
    fputs("</ul>\n</div>\n", f);
}

static void resumen_html_rankings(FILE *f, const ResumenGeneral *r)
{
    if (r->camisetas.cantidad + r->canchas.cantidad + r->rivales.cantidad == 0)
    {
        return;
    }
    fputs("<section class=\"panel panel-ancho\">\n<h2>D&oacute;nde y con qu&eacute;</h2>\n"
          "<div class=\"tres\">\n",
          f);
    resumen_html_ranking(f, "Camisetas", &r->camisetas);
    resumen_html_ranking(f, "Canchas", &r->canchas);
    resumen_html_ranking(f, "Rivales", &r->rivales);
    fputs("</div>\n<p class=\"nota\">Partidos jugados y porcentaje de victorias sobre los "
          "partidos con resultado. La barra es el % de victorias.</p>\n</section>\n",
          f);
}

static void resumen_html_barra(FILE *f, const char *k, double pct, const char *n)
{
    fprintf(f,
            "<li><span class=\"k\">%s</span><span class=\"medidor\"><i style=\"width:%.1f%%\">"
            "</i></span><span class=\"n\">%s</span></li>\n",
            k, pct, n);
}

static void resumen_html_conteos(FILE *f, const char *titulo, const ResumenConteo *c, int n,
                                 const char *(*nombre)(int))
{
    int max = resumen_max_conteo(c, n);
    char buf[16];

    if (max == 0)
    {
        return;
    }
    fprintf(f, "<div>\n<h3>%s</h3>\n<ul class=\"barras\">\n", titulo);
    for (int i = 0; i < n; i++)
    {
        if (c[i].partidos == 0)
        {
            continue;
        }
        snprintf(buf, sizeof(buf), "%d", c[i].partidos);
        resumen_html_barra(f, nombre(i), (double)c[i].partidos * 100.0 / max, buf);
    }
    fputs("</ul>\n</div>\n", f);
}

static const char *resumen_nombre_dia(int i)
{
    return RESUMEN_DIAS_SEMANA[i];
}

static const char *resumen_nombre_franja(int i)
{
    return RESUMEN_FRANJAS_HTML[i];
}

static void resumen_html_cuando(FILE *f, const ResumenGeneral *r)
{
    if (resumen_max_conteo(r->dias_semana, 7) == 0 &&
            resumen_max_conteo(r->franjas, RESUMEN_FRANJAS) == 0)
    {
        return;
    }
    fputs("<section class=\"panel panel-ancho\">\n<h2>Cu&aacute;ndo jug&aacute;s</h2>\n<div class=\"dos\">\n", f);
    resumen_html_conteos(f, "D&iacute;a de la semana", r->dias_semana, 7, resumen_nombre_dia);
    resumen_html_conteos(f, "Franja horaria", r->franjas, RESUMEN_FRANJAS,
                         resumen_nombre_franja);
    fputs("</div>\n<p class=\"nota\">Cantidad de partidos.</p>\n</section>\n", f);
}

static void resumen_html_como_llegas(FILE *f, const ResumenGeneral *r)
{
    if (r->con_rendimiento + r->con_animo + r->con_cansancio == 0)
    {
        return;
    }
    fputs("<section class=\"panel\">\n<h2>C&oacute;mo lleg&aacute;s</h2>\n<ul class=\"barras\">\n", f);
    if (r->con_rendimiento > 0)
    {
        resumen_html_barra(f, "Rendimiento", r->rendimiento_prom * 10.0,
                           rnum(r->rendimiento_prom, 1));
    }
    if (r->con_animo > 0)
    {
        resumen_html_barra(f, "&Aacute;nimo", r->animo_prom * 10.0, rnum(r->animo_prom, 1));
    }
    if (r->con_cansancio > 0)
    {
        resumen_html_barra(f, "Cansancio", r->cansancio_prom * 10.0,
                           rnum(r->cansancio_prom, 1));
    }
    fputs("</ul>\n<p class=\"nota\">Promedios de 1 a 10 sobre los partidos con el dato "
          "cargado.</p>\n</section>\n",
          f);
}

static void resumen_html_clima(FILE *f, const ResumenGeneral *r)
{
    int hay_manual = strcmp(r->clima_frecuente, "Sin datos") != 0 && r->clima_frecuente[0];

    if (!hay_manual && r->clima_real_partidos == 0)
    {
        return;
    }
    fputs("<section class=\"panel\">\n<h2>Clima</h2>\n<ul class=\"filas\">\n", f);
    if (hay_manual)
    {
        fprintf(f, "<li><span class=\"k\">Clima m&aacute;s frecuente</span><span class=\"v\">%s"
                "</span></li>\n",
                html_texto(r->clima_frecuente));
    }
    if (r->hay_clima_mejor)
    {
        fprintf(f,
                "<li><span class=\"k\">Tu mejor condici&oacute;n</span><span class=\"v\">%s"
                "<small>Rendimiento %s &middot; %s%% V &middot; %d partidos</small>"
                "</span></li>\n",
                html_texto(r->clima_mejor), rnum(r->clima_mejor_rendimiento, 1),
                rnum(r->clima_mejor_victorias_pct, 0), r->clima_mejor_partidos);
    }
    if (r->clima_real_partidos > 0)
    {
        fprintf(f,
                "<li><span class=\"k\">Con clima real</span><span class=\"v\">%d partidos"
                "<small>%s%% del total, datos de Open-Meteo</small></span></li>\n",
                r->clima_real_partidos, rnum(r->clima_real_cobertura_pct, 0));
    }
    fputs("</ul>\n</section>\n", f);
}

static void resumen_html_perfil(FILE *f, const ResumenGeneral *r)
{
    double pct = resumen_pct(r->logros_completados, r->logros_total);

    fputs("<section class=\"panel\">\n<h2>Perfil</h2>\n<ul class=\"filas\">\n", f);
    if (strcmp(r->posicion, "Sin datos") != 0 && r->posicion[0])
    {
        fprintf(f, "<li><span class=\"k\">Mejor posici&oacute;n</span><span class=\"v\">%s</span>"
                "</li>\n",
                html_texto(r->posicion));
    }
    if (resumen_hay_torneo(r))
    {
        fprintf(f, "<li><span class=\"k\">Torneo m&aacute;s jugado</span>"
                "<span class=\"v\">%s</span></li>\n",
                html_texto(r->torneo));
    }
    if (r->lesiones == 0)
    {
        fputs("<li><span class=\"k\">Lesiones</span><span class=\"v\">Ninguna registrada"
              "</span></li>\n",
              f);
    }
    else
    {
        fprintf(f, "<li><span class=\"k\">Lesiones</span><span class=\"v\">%d</span></li>\n",
                r->lesiones);
    }
    fputs("</ul>\n", f);
    if (r->logros_total > 0)
    {
        fprintf(f,
                "<div class=\"logros\"><h3>Logros de la camiseta principal</h3>"
                "<p><b>%d</b> de %d &middot; %s%%</p>\n"
                "<div class=\"medidor alto\"><i style=\"width:%.1f%%\"></i></div></div>\n",
                r->logros_completados, r->logros_total, rnum(pct, 0), pct);
    }
    fputs("</section>\n", f);
}

void resumen_general_escribir_html(FILE *f, const ResumenGeneral *r)
{
    if (!f || !r)
    {
        return;
    }
    fprintf(f,
            "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "<title>%s &middot; Perfil MiFutbolC</title>\n"
            "<meta property=\"og:title\" content=\"Perfil de %s - MiFutbolC\">\n"
            "<meta property=\"og:description\" content=\"%d partidos, %d goles, %d "
            "asistencias\">\n"
            "<meta property=\"og:type\" content=\"profile\">\n<style>\n",
            html_texto(r->jugador), html_texto(r->jugador), r->partidos, r->goles,
            r->asistencias);
    resumen_escribir_css_comun(f);
    fputs("</style>\n</head>\n<body>\n<main class=\"pagina\">\n", f);

    resumen_html_hero(f, r);
    fputs("<div class=\"grilla\">\n", f);
    if (r->partidos == 0)
    {
        fputs("<section class=\"panel panel-ancho\">\n<h2>Todav&iacute;a no hay partidos</h2>\n"
              "<p class=\"vacio\">Carg&aacute; tu primer partido en MiFutbolC y volv&eacute; a generar el "
              "resumen: ac&aacute; van a aparecer tus resultados, r&eacute;cords y "
              "tendencias.</p>\n"
              "</section>\n",
              f);
    }
    resumen_html_resultados(f, r);
    resumen_html_ataque(f, r);
    resumen_html_tendencia(f, r);
    resumen_html_records(f, r);
    resumen_html_actualidad(f, r);
    resumen_html_rankings(f, r);
    resumen_html_anios(f, r);
    resumen_html_como_llegas(f, r);
    resumen_html_cuando(f, r);
    resumen_html_clima(f, r);
    resumen_html_perfil(f, r);
    fputs("</div>\n", f);

    fprintf(f,
            "<footer class=\"pie\">Generado por MiFutbolC v4.3 el %s"
            "<span class=\"compartir\"> &middot; <a href=\"mailto:?subject=Mi%%20perfil%%20futbolistico&amp;body=Mira%%20mi%%20"
            "perfil%%20en%%20MiFutbolC:%%20%d%%20partidos,%%20%d%%20goles,%%20%d%%20"
            "asistencias\">Compartir por email</a></span></footer>\n"
            "</main>\n</body>\n</html>\n",
            html_texto(r->fecha_gen), r->partidos, r->goles, r->asistencias);
}

/* ─── Resumen general: Markdown ───────────────────────────────────────────── */

/* Celda de tabla Markdown: escapa '|' y cambia saltos de linea por espacios.
 * Usa un anillo de buffers para varias celdas en el mismo fprintf. */
static const char *md_celda(const char *texto)
{
    static char slots[8][256];
    static unsigned siguiente = 0;
    unsigned slot = siguiente % 8U;
    size_t usado = 0;
    char *dst = slots[slot];

    siguiente = (siguiente + 1U) % 8U;
    for (const char *p = texto ? texto : ""; *p && usado + 2 < sizeof(slots[slot]); p++)
    {
        if (*p == '|')
        {
            dst[usado++] = '\\';
            dst[usado++] = '|';
        }
        else
        {
            dst[usado++] = (*p == '\n' || *p == '\r') ? ' ' : *p;
        }
    }
    dst[usado] = '\0';
    return dst;
}

static void md_fila_partido(FILE *f, const char *k, const char *valor,
                            const ResumenPartidoCorto *p)
{
    fprintf(f, "| %s | %s | %s%s%s |\n", k, valor, md_celda(p->fecha),
            p->rival[0] ? " vs " : "", md_celda(p->rival));
}

static void md_ranking(FILE *f, const char *titulo, const ResumenTop *top)
{
    if (top->cantidad == 0)
    {
        return;
    }
    fprintf(f, "### %s\n\n| # | Nombre | PJ | %% V | Goles |\n|---|---|---:|---:|---:|\n",
            titulo);
    for (int i = 0; i < top->cantidad; i++)
    {
        const ResumenTopItem *it = &top->items[i];
        fprintf(f, "| %d | %s | %d | %s%% | %d |\n", i + 1, md_celda(it->nombre), it->partidos,
                rnum(it->victorias_pct, 0), it->goles);
    }
    fputs("\n", f);
}

static void md_conteos(FILE *f, const char *titulo, const ResumenConteo *c, int n,
                       const char *const *nombres)
{
    if (resumen_max_conteo(c, n) == 0)
    {
        return;
    }
    fprintf(f, "### %s\n\n| %s | PJ | %% V |\n|---|---:|---:|\n", titulo, titulo);
    for (int i = 0; i < n; i++)
    {
        if (c[i].partidos > 0)
        {
            fprintf(f, "| %s | %d | %s%% |\n", nombres[i], c[i].partidos,
                    rnum(resumen_pct(c[i].victorias, c[i].partidos), 0));
        }
    }
    fputs("\n", f);
}

static const char *const MD_DIAS_SEMANA[7] =
{
    "Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"
};

static const char *const MD_FRANJAS[RESUMEN_FRANJAS] =
{
    "Madrugada", "Mañana", "Mediodía", "Tarde", "Atardecer", "Noche"
};

static void md_resultados_y_ataque(FILE *f, const ResumenGeneral *r)
{
    fprintf(f,
            "## Números de carrera\n\n"
            "| Métrica | Valor |\n|---|---:|\n"
            "| Partidos | **%d** |\n| Goles | **%d** |\n| Asistencias | **%d** |\n"
            "| Goles + asistencias | %d |\n| G+A por partido | %s |\n"
            "| Goles por partido | %s |\n| Asistencias por partido | %s |\n"
            "| Partidos con gol | %d (%s%%) |\n| Dobletes | %d |\n| Hat-tricks o más | %d |\n",
            r->partidos, r->goles, r->asistencias, r->goles + r->asistencias, rnum(r->ga_pp, 2),
            rnum(r->goles_pp, 2), rnum(r->asistencias_pp, 2), r->partidos_con_gol,
            rnum(resumen_pct(r->partidos_con_gol, r->partidos), 0), r->dobletes, r->hat_tricks);
    if (r->partidos_con_minutos > 0)
    {
        fprintf(f, "| G+A cada 90' | %s (%d partidos, %d min) |\n", rnum(r->ga_por_90, 2),
                r->partidos_con_minutos, r->minutos_total);
    }
    fputs("\n", f);

    if (r->con_resultado > 0)
    {
        fprintf(f,
                "## Resultados\n\n| Resultado | Cantidad | %% |\n|---|---:|---:|\n"
                "| Victorias | **%d** | %s%% |\n| Empates | **%d** | %s%% |\n"
                "| Derrotas | **%d** | %s%% |\n\n"
                "**Efectividad: %s%%** de los puntos en juego (3 por victoria, 1 por empate), "
                "sobre %d partidos con resultado.\n\n",
                r->victorias, rnum(resumen_pct(r->victorias, r->con_resultado), 1), r->empates,
                rnum(resumen_pct(r->empates, r->con_resultado), 1), r->derrotas,
                rnum(resumen_pct(r->derrotas, r->con_resultado), 1),
                rnum(r->efectividad_pct, 1), r->con_resultado);
    }
}

static void md_forma(FILE *f, const ResumenGeneral *r)
{
    if (r->ultimos_n > 0)
    {
        fprintf(f, "## Forma reciente\n\nÚltimos %d, del más viejo al más nuevo: **",
                r->ultimos_n);
        for (int i = 0; i < r->ultimos_n; i++)
        {
            fprintf(f, "%s%c", i ? " " : "", resumen_letra_resultado(r->ultimos[i].resultado));
        }
        fputs("**\n\n| Fecha | Rival | Resultado | G | A | Rend. |\n"
              "|---|---|---|---:|---:|---:|\n",
              f);
        for (int i = r->ultimos_n - 1; i >= 0; i--)
        {
            const ResumenPartidoCorto *p = &r->ultimos[i];
            char rend[16] = "-";
            if (p->rendimiento > 0)
            {
                snprintf(rend, sizeof(rend), "%d", p->rendimiento);
            }
            fprintf(f, "| %s | %s | %s | %d | %d | %s |\n", md_celda(p->fecha),
                    p->rival[0] ? md_celda(p->rival) : "-",
                    resumen_texto_resultado(p->resultado), p->goles, p->asistencias, rend);
        }
        fputs("\n", f);
    }
    if (r->tendencia_n >= 2)
    {
        double suma = 0.0;
        fputs("Tendencia de rendimiento (1 a 10), del más viejo al más nuevo: `", f);
        for (int i = 0; i < r->tendencia_n; i++)
        {
            fprintf(f, "%s%d", i ? " " : "", r->tendencia[i]);
            suma += r->tendencia[i];
        }
        fprintf(f, "` (promedio %s)\n\n", rnum(suma / r->tendencia_n, 1));
    }
}

static void md_records(FILE *f, const ResumenGeneral *r)
{
    char buf[48];
    const char *texto;
    const char *desc;

    fputs("## Récords y momento actual\n\n| Récord | Valor | Partido |\n|---|---|---|\n", f);
    if (r->con_resultado > 0)
    {
        fprintf(f, "| Mejor racha de victorias | %d partidos | |\n", r->mejor_racha_victorias);
        fprintf(f, "| Mejor racha invicto | %d partidos | |\n", r->mejor_racha_invicto);
    }
    fprintf(f, "| Racha más larga marcando | %d partidos | |\n", r->mejor_racha_marcando);
    if (r->max_goles.goles > 0)
    {
        snprintf(buf, sizeof(buf), "%d goles", r->max_goles.goles);
        md_fila_partido(f, "Más goles en un partido", buf, &r->max_goles);
    }
    if (r->max_asistencias.asistencias > 0)
    {
        snprintf(buf, sizeof(buf), "%d asistencias", r->max_asistencias.asistencias);
        md_fila_partido(f, "Más asistencias", buf, &r->max_asistencias);
    }
    if (r->mejor_rendimiento.rendimiento > 0)
    {
        snprintf(buf, sizeof(buf), "%d/10", r->mejor_rendimiento.rendimiento);
        md_fila_partido(f, "Mejor rendimiento", buf, &r->mejor_rendimiento);
    }
    resumen_describir_racha(r->racha_tipo, &texto, &desc);
    snprintf(buf, sizeof(buf), "%s, %d partido%s", texto, r->racha_longitud,
             r->racha_longitud == 1 ? "" : "s");
    fprintf(f, "| Racha actual | %s | %s |\n", buf, desc);
    snprintf(buf, sizeof(buf), "%s, %d G, %d A", resumen_texto_resultado(r->ultimo.resultado),
             r->ultimo.goles, r->ultimo.asistencias);
    md_fila_partido(f, "Último partido", buf, &r->ultimo);
    fputs("\n", f);
}

static void md_anios(FILE *f, const ResumenGeneral *r)
{
    if (r->num_anios == 0)
    {
        return;
    }
    fputs("## Año por año\n\n| Año | PJ | Goles | Asist. | Victorias |\n"
          "|---|---:|---:|---:|---:|\n",
          f);
    for (int i = r->num_anios - 1; i >= 0; i--)
    {
        const ResumenAnioItem *a = &r->anios[i];
        fprintf(f, "| %d | %d | %d | %d | %d |\n", a->anio, a->partidos, a->goles,
                a->asistencias, a->victorias);
    }
    fputs("\n", f);
}

static void md_contexto(FILE *f, const ResumenGeneral *r)
{
    int hay_manual = strcmp(r->clima_frecuente, "Sin datos") != 0 && r->clima_frecuente[0];

    if (r->con_rendimiento + r->con_animo + r->con_cansancio > 0)
    {
        fputs("## Cómo llegás\n\n| Promedio (1 a 10) | Valor | Partidos con dato |\n"
              "|---|---:|---:|\n",
              f);
        if (r->con_rendimiento > 0)
        {
            fprintf(f, "| Rendimiento | %s | %d |\n", rnum(r->rendimiento_prom, 1),
                    r->con_rendimiento);
        }
        if (r->con_animo > 0)
        {
            fprintf(f, "| Ánimo | %s | %d |\n", rnum(r->animo_prom, 1), r->con_animo);
        }
        if (r->con_cansancio > 0)
        {
            fprintf(f, "| Cansancio | %s | %d |\n", rnum(r->cansancio_prom, 1),
                    r->con_cansancio);
        }
        fputs("\n", f);
    }

    if (resumen_max_conteo(r->dias_semana, 7) > 0 ||
            resumen_max_conteo(r->franjas, RESUMEN_FRANJAS) > 0)
    {
        fputs("## Cuándo jugás\n\n", f);
        md_conteos(f, "Día", r->dias_semana, 7, MD_DIAS_SEMANA);
        md_conteos(f, "Franja", r->franjas, RESUMEN_FRANJAS, MD_FRANJAS);
    }

    if (hay_manual || r->clima_real_partidos > 0)
    {
        fputs("## Clima\n\n| Detalle | Valor |\n|---|---|\n", f);
        if (hay_manual)
        {
            fprintf(f, "| Clima más frecuente | %s |\n", md_celda(r->clima_frecuente));
        }
        if (r->hay_clima_mejor)
        {
            fprintf(f, "| Tu mejor condición | %s (rendimiento %s, %s%% V, %d partidos) |\n",
                    md_celda(r->clima_mejor), rnum(r->clima_mejor_rendimiento, 1),
                    rnum(r->clima_mejor_victorias_pct, 0), r->clima_mejor_partidos);
        }
        if (r->clima_real_partidos > 0)
        {
            fprintf(f, "| Con clima real (Open-Meteo) | %d partidos (%s%%) |\n",
                    r->clima_real_partidos, rnum(r->clima_real_cobertura_pct, 0));
        }
        fputs("\n", f);
    }
}

void resumen_general_escribir_markdown(FILE *f, const ResumenGeneral *r)
{
    if (!f || !r)
    {
        return;
    }
    fprintf(f, "# Perfil futbolístico de %s\n\n> Generado por **MiFutbolC** v4.3 el %s\n\n",
            md_celda(r->jugador), md_celda(r->fecha_gen));

    if (r->partidos == 0)
    {
        fputs("Todavía no hay partidos cargados. Cargá tu primer partido en MiFutbolC y volvé "
              "a generar el resumen.\n",
              f);
        return;
    }
    fprintf(f, "%d partidos desde el %s.\n\n", r->partidos, md_celda(r->primer_partido_fecha));

    md_resultados_y_ataque(f, r);
    md_forma(f, r);
    md_records(f, r);
    md_anios(f, r);

    if (r->camisetas.cantidad + r->canchas.cantidad + r->rivales.cantidad > 0)
    {
        fputs("## Dónde y con qué\n\n", f);
        md_ranking(f, "Camisetas", &r->camisetas);
        md_ranking(f, "Canchas", &r->canchas);
        md_ranking(f, "Rivales", &r->rivales);
    }

    md_contexto(f, r);

    fputs("## Perfil\n\n| Detalle | Valor |\n|---|---|\n", f);
    if (strcmp(r->posicion, "Sin datos") != 0 && r->posicion[0])
    {
        fprintf(f, "| Mejor posición | %s |\n", md_celda(r->posicion));
    }
    if (resumen_hay_torneo(r))
    {
        fprintf(f, "| Torneo más jugado | %s |\n", md_celda(r->torneo));
    }
    if (r->lesiones == 0)
    {
        fputs("| Lesiones | Ninguna registrada |\n", f);
    }
    else
    {
        fprintf(f, "| Lesiones | %d |\n", r->lesiones);
    }
    fprintf(f, "| Logros (camiseta principal) | %d de %d (%s%%) |\n\n", r->logros_completados,
            r->logros_total, rnum(resumen_pct(r->logros_completados, r->logros_total), 0));
    fprintf(f, "---\n\n*Generado el %s*\n", md_celda(r->fecha_gen));
}

static void generar_resumen_formato(int es_markdown)
{
    clear_screen();
    print_header(es_markdown ? "RESUMEN MARKDOWN COMPARTIBLE" : "RESUMEN HTML COMPARTIBLE");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    ResumenGeneral r;
    if (!resumen_general_calcular(&r))
    {
        printf("No se pudieron leer los partidos para el resumen.\n");
        pause_console();
        return;
    }

    const char *nombre_archivo = es_markdown ? "resumen_compartible.md" : "resumen_compartible.html";
    char *filepath = get_export_path(nombre_archivo);
    FILE *f = NULL;
    fopen_s(&f, filepath, "w");
    if (!f)
    {
        printf("Error al crear el archivo %s: %s\n", es_markdown ? "Markdown" : "HTML", filepath);
        pause_console();
        return;
    }

    if (es_markdown)
    {
        resumen_general_escribir_markdown(f, &r);
        ui_printf("\nResumen Markdown generado correctamente.\n");
    }
    else
    {
        resumen_general_escribir_html(f, &r);
        ui_printf("\nResumen HTML generado correctamente.\n");
    }

    fclose(f);
    ui_printf("Archivo: %s\n\n", filepath);
    mostrar_detalle_racha();
    app_log_event("RESUMEN_COMPARTIBLE",
                  es_markdown ? "Resumen Markdown generado" : "Resumen HTML generado");
    pause_console();
}

void generar_resumen_html(void)
{
    generar_resumen_formato(0);
}

void generar_resumen_markdown(void)
{
    generar_resumen_formato(1);
}

void generar_resumen_estadisticas(void)
{
    clear_screen();
    print_header("ESTADISTICAS DESTACADAS");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;

    /* Mejor partido */
    ui_printf("\n=== MEJOR PARTIDO ===\n");
    if (preparar_stmt(
                "SELECT p.fecha_hora, c.nombre, p.goles, p.asistencias, p.rendimiento_general "
                "FROM partido p "
                "JOIN camiseta c ON p.camiseta_id = c.id "
                "ORDER BY p.rendimiento_general DESC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ui_printf("Fecha: %s\n", sqlite3_column_text(stmt, 0));
            ui_printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
            ui_printf("Goles: %d | Asistencias: %d\n",
                      sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3));
            ui_printf("Rendimiento: %d/10\n", sqlite3_column_int(stmt, 4));
        }
        else
        {
            mostrar_no_hay_registros("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }

    /* Peor partido */
    ui_printf("\n=== PEOR PARTIDO ===\n");
    if (preparar_stmt(
                "SELECT p.fecha_hora, c.nombre, p.goles, p.asistencias, p.rendimiento_general "
                "FROM partido p "
                "JOIN camiseta c ON p.camiseta_id = c.id "
                "ORDER BY p.rendimiento_general ASC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ui_printf("Fecha: %s\n", sqlite3_column_text(stmt, 0));
            ui_printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
            ui_printf("Goles: %d | Asistencias: %d\n",
                      sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3));
            ui_printf("Rendimiento: %d/10\n", sqlite3_column_int(stmt, 4));
        }
        else
        {
            mostrar_no_hay_registros("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }

    /* Promedios generales */
    ui_printf("\n=== PROMEDIOS GENERALES ===\n");
    if (preparar_stmt(
                "SELECT "
                "ROUND(AVG(goles), 2), "
                "ROUND(AVG(asistencias), 2), "
                "ROUND(AVG(rendimiento_general), 2), "
                "ROUND(AVG(cansancio), 2), "
                "ROUND(AVG(estado_animo), 2), "
                "COUNT(*) "
                "FROM partido;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ui_printf("Goles promedio: %.2f\n", sqlite3_column_double(stmt, 0));
            ui_printf("Asistencias promedio: %.2f\n", sqlite3_column_double(stmt, 1));
            ui_printf("Rendimiento promedio: %.2f/10\n", sqlite3_column_double(stmt, 2));
            ui_printf("Cansancio promedio: %.2f/10\n", sqlite3_column_double(stmt, 3));
            ui_printf("Estado de animo promedio: %.2f/10\n", sqlite3_column_double(stmt, 4));
            ui_printf("Total de partidos: %d\n", sqlite3_column_int(stmt, 5));
        }
        sqlite3_finalize(stmt);
    }

    /* Goles y asistencias totales */
    ui_printf("\n=== TOTALES ===\n");
    ui_printf("Goles totales: %d\n", contar_total_goles());
    ui_printf("Asistencias totales: %d\n", contar_total_asistencias());
    ui_printf("Partidos totales: %d\n", contar_total_partidos());

    /* Camiseta favorita */
    char camiseta_fav[128];
    obtener_camiseta_favorita(camiseta_fav, sizeof(camiseta_fav));
    ui_printf("Camiseta mas usada: %s\n", camiseta_fav);

    /* Cancha favorita */
    char cancha_fav[128];
    obtener_cancha_favorita(cancha_fav, sizeof(cancha_fav));
    ui_printf("Cancha mas jugada: %s\n", cancha_fav);

    ui_printf("\n");
    app_log_event("RESUMEN_COMPARTIBLE", "Estadisticas destacadas mostradas");
    pause_console();
}

static void mostrar_mejor_temporada_fallback(void)
{
    sqlite3_stmt *stmt;

    ui_printf("\n=== MEJOR TEMPORADA POR RENDIMIENTO ===\n\n");

    if (!preparar_stmt(
                "SELECT substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) as anio, "
                "ROUND(AVG(p.rendimiento_general), 2) as rendimiento, "
                "COUNT(*) as partidos, "
                "SUM(p.goles) as goles, "
                "SUM(p.asistencias) as asistencias "
                "FROM partido p "
                "WHERE p.fecha_hora IS NOT NULL "
                "GROUP BY anio "
                "ORDER BY rendimiento DESC LIMIT 1;",
                &stmt))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        double rendimiento = sqlite3_column_double(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int goles = sqlite3_column_int(stmt, 3);
        int asistencias = sqlite3_column_int(stmt, 4);

        ui_printf("Anio: %s\n", anio ? anio : "Desconocido");
        ui_printf("Rendimiento promedio: %.2f/10\n", rendimiento);
        ui_printf("Partidos jugados: %d\n", partidos);
        ui_printf("Goles totales: %d\n", goles);
        ui_printf("Asistencias totales: %d\n", asistencias);
    }
    else
    {
        mostrar_no_hay_registros("temporadas con datos");
    }
    sqlite3_finalize(stmt);
}

static void mostrar_mejor_temporada_victorias(void)
{
    sqlite3_stmt *stmt;

    ui_printf("\n=== MEJOR TEMPORADA POR VICTORIAS ===\n\n");

    if (!preparar_stmt(
                "SELECT t.nombre, "
                "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                "COUNT(*) as total "
                "FROM partido p "
                "JOIN temporada t ON substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) = t.nombre "
                "WHERE p.fecha_hora IS NOT NULL "
                "GROUP BY t.nombre "
                "ORDER BY victorias DESC LIMIT 1;",
                &stmt))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre_temp = (const char *)sqlite3_column_text(stmt, 0);
        int victorias = sqlite3_column_int(stmt, 1);
        int total = sqlite3_column_int(stmt, 2);

        ui_printf("Temporada: %s\n", nombre_temp ? nombre_temp : "Sin nombre");
        ui_printf("Victorias: %d de %d partidos\n", victorias, total);
        double porcentaje = total > 0 ? ((double)victorias / total) * 100.0 : 0.0;
        ui_printf("Porcentaje de victorias: %.1f%%\n", porcentaje);
    }
    else
    {
        mostrar_no_hay_registros("temporadas con victorias");
    }
    sqlite3_finalize(stmt);
}

void generar_resumen_mejor_temporada(void)
{
    clear_screen();
    print_header("MEJOR TEMPORADA");

    sqlite3_stmt *stmt;

    if (preparar_stmt(
                "SELECT t.nombre, tr.total_partidos, tr.total_goles, "
                "tr.promedio_goles_partido, "
                "e.nombre as campeon, "
                "tr.total_lesiones "
                "FROM temporada_resumen tr "
                "JOIN temporada t ON tr.temporada_id = t.id "
                "LEFT JOIN equipo e ON tr.equipo_campeon_id = e.id "
                "ORDER BY tr.total_goles DESC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre_temp = (const char *)sqlite3_column_text(stmt, 0);
            int partidos = sqlite3_column_int(stmt, 1);
            int goles = sqlite3_column_int(stmt, 2);
            double promedio = sqlite3_column_double(stmt, 3);
            const char *campeon = (const char *)sqlite3_column_text(stmt, 4);
            int lesiones = sqlite3_column_int(stmt, 5);

            ui_printf("\n=== MEJOR TEMPORADA POR GOLES ===\n\n");
            ui_printf("Temporada: %s\n", nombre_temp ? nombre_temp : "Sin nombre");
            ui_printf("Partidos jugados: %d\n", partidos);
            ui_printf("Total de goles: %d\n", goles);
            ui_printf("Promedio goles/partido: %.2f\n", promedio);
            ui_printf("Equipo campeon: %s\n", campeon ? campeon : "No determinado");
            ui_printf("Lesiones: %d\n", lesiones);
            sqlite3_finalize(stmt);
        }
        else
        {
            sqlite3_finalize(stmt);
            mostrar_mejor_temporada_fallback();
        }
    }

    mostrar_mejor_temporada_victorias();

    ui_printf("\n");
    app_log_event("RESUMEN_COMPARTIBLE", "Mejor temporada mostrada");
    pause_console();
}

/* ─── Resumen del año ─────────────────────────────────────────────────────── */

/*
 * Reune las estadisticas del año elegido y las exporta a TXT, HTML y PDF.
 * Reutiliza el calculo del resumen general filtrado por año
 * (resumen_general_calcular_anio) y suma lo propio del año: mes a mes,
 * tendencia, comparacion con el año anterior, marcador del equipo,
 * equipamiento, lesiones y el mejor partido.
 */

#define RESUMEN_ANIO_MAX_GRUPOS 64

/**
 * @brief Lee un numero de digitos consecutivos dentro de una fecha.
 * @return Valor leido, o -1 si algun caracter no es un digito.
 */
static int resumen_anio_digitos(const char *texto, size_t inicio, int cantidad)
{
    int valor = 0;

    for (int i = 0; i < cantidad; i++)
    {
        char c = texto[inicio + (size_t)i];
        if (c < '0' || c > '9')
        {
            return -1;
        }
        valor = valor * 10 + (c - '0');
    }

    return valor;
}

/**
 * @brief Extrae el año de una fecha ISO (AAAA-MM-DD) o local (DD/MM/AAAA).
 * @return Año leido, o -1 si la fecha no tiene un formato reconocible.
 */
static int resumen_anio_desde_fecha(const char *fecha)
{
    size_t largo;

    if (!fecha)
    {
        return -1;
    }

    largo = strlen(fecha);
    if (largo >= 10 && fecha[2] == '/' && fecha[5] == '/')
    {
        return resumen_anio_digitos(fecha, 6, 4);
    }
    if (largo >= 4)
    {
        return resumen_anio_digitos(fecha, 0, 4);
    }

    return -1;
}

static int resumen_anio_anio_actual(void)
{
    time_t ahora = time(NULL);
    struct tm tm_ahora;

#ifdef _WIN32
    localtime_s(&tm_ahora, &ahora);
#else
    localtime_r(&ahora, &tm_ahora);
#endif

    return tm_ahora.tm_year + 1900;
}

static const char *resumen_anio_mes_nombre(int indice)
{
    static const char *const nombres[RESUMEN_ANIO_MESES] =
    {
        "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
        "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
    };

    if (indice < 0 || indice >= RESUMEN_ANIO_MESES)
    {
        return "Sin mes";
    }

    return nombres[indice];
}

/**
 * @brief Ejecuta un COUNT(*) y devuelve el valor, 0 si falla.
 */
static int resumen_anio_contar(const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    int total = 0;

    if (!sql || !preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return total;
}

/**
 * @brief Busca un nombre por id con la consulta indicada.
 */
static void resumen_anio_nombre_por_id(const char *sql, int id, char *buffer, size_t size)
{
    sqlite3_stmt *stmt = NULL;
    size_t largo;

    snprintf(buffer, size, "Sin datos");
    if (!sql || id < 0 || !preparar_stmt(sql, &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        if (nombre && nombre[0] != '\0')
        {
            snprintf(buffer, size, "%s", nombre);
            /* Recorta espacios finales para que el reporte no muestre dobles separaciones. */
            largo = strlen(buffer);
            while (largo > 0 && (buffer[largo - 1] == ' ' || buffer[largo - 1] == '\t'))
            {
                buffer[--largo] = '\0';
            }
        }
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Devuelve el indice del grupo con ese id, agregandolo si es nuevo.
 * @return Indice dentro de los arreglos, o -1 si ya no cabe otro grupo.
 */
static int resumen_anio_indice_grupo(int *ids, int *cuentas, int *total, int id)
{

    for (int i = 0; i < *total; i++)
    {
        if (ids[i] == id)
        {
            return i;
        }
    }

    if (*total >= RESUMEN_ANIO_MAX_GRUPOS)
    {
        return -1;
    }

    ids[*total] = id;
    cuentas[*total] = 0;
    (*total)++;
    return *total - 1;
}

static void resumen_anio_top_grupo(const int *ids, const int *cuentas, int total,
                                   int *id_top, int *partidos_top)
{
    *id_top = -1;
    *partidos_top = 0;
    for (int i = 0; i < total; i++)
    {
        if (cuentas[i] > *partidos_top)
        {
            *id_top = ids[i];
            *partidos_top = cuentas[i];
        }
    }
}

/**
 * @brief Copia una columna de texto de SQLite a un buffer, tolerando NULL.
 */
static void resumen_anio_copiar_texto(char *destino, size_t tam, sqlite3_stmt *stmt, int columna)
{
    const char *valor = (const char *)sqlite3_column_text(stmt, columna);
    size_t largo;

    if (!destino || tam == 0)
    {
        return;
    }

    snprintf(destino, tam, "%s", valor ? valor : "");

    /* La base puede guardar nombres con espacios finales: se recortan aqui. */
    largo = strlen(destino);
    while (largo > 0 && (destino[largo - 1] == ' ' || destino[largo - 1] == '\t'))
    {
        destino[--largo] = '\0';
    }
}

/**
 * @brief Agrega un texto a un buffer de descripcion sin desbordarlo.
 */
static void resumen_anio_append(char *buffer, size_t tam, int *usados, const char *texto)
{
    size_t restante;
    int escrito;

    if (!buffer || tam == 0 || !usados || !texto || texto[0] == '\0' ||
            *usados < 0 || (size_t)*usados >= tam)
    {
        return;
    }

    if (*usados > 0)
    {
        restante = tam - (size_t)*usados;
        escrito = snprintf(buffer + *usados, restante, " | ");
        if (escrito < 0 || (size_t)escrito >= restante)
        {
            return;
        }
        *usados += escrito;
    }

    restante = tam - (size_t)*usados;
    escrito = snprintf(buffer + *usados, restante, "%s", texto);
    if (escrito > 0 && (size_t)escrito < restante)
    {
        *usados += escrito;
    }
}

static const char *resumen_anio_tipo_partido_texto(int tipo_partido)
{
    switch (tipo_partido)
    {
    case 2:
        return "Torneo";
    case 3:
        return "Entrenamiento";
    case 1:
    default:
        return "Amistoso";
    }
}

static const char *resumen_anio_tarjeta_texto(int tarjeta)
{
    switch (tarjeta)
    {
    case 2:
        return "Amarilla";
    case 3:
        return "Roja";
    default:
        return "No";
    }
}

static const char *resumen_anio_estado_cancha_texto(int estado_cancha)
{
    switch (estado_cancha)
    {
    case 1:
        return "Excelente";
    case 2:
        return "Buena";
    case 3:
        return "Regular";
    case 4:
        return "Mala";
    case 5:
        return "Pesima";
    default:
        return "No definido";
    }
}

static const char *resumen_anio_dolor_texto(int dolor_fisico)
{
    switch (dolor_fisico)
    {
    case 1:
        return "Leve";
    case 2:
        return "Moderada";
    case 3:
        return "Fuerte";
    default:
        return "Ninguna";
    }
}

static const char *resumen_anio_arbitraje_texto(int arbitraje_score)
{
    switch (arbitraje_score)
    {
    case 1:
        return "Muy malo";
    case 2:
        return "Regular";
    case 3:
        return "Normal";
    case 4:
        return "Bueno";
    case 5:
        return "Excelente";
    default:
        return "";
    }
}

/**
 * @brief Carga todas las columnas de un partido para el detalle del mejor del año.
 *
 * @param id Identificador del partido.
 * @param p Estructura de salida; se limpia antes de cargar.
 */
static void resumen_anio_cargar_partido(int id, ResumenAnioPartido *p)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT p.fecha_hora, IFNULL(p.rival_nombre, ''), IFNULL(p.tipo_rival, ''), "
        "p.resultado, p.goles, p.asistencias, IFNULL(p.goles_detalle, ''), "
        "IFNULL(p.asistencias_detalle, ''), p.rendimiento_general, p.rating_tecnico, "
        "p.rating_fisico, p.rating_mental, p.minutos_jugados, p.intensidad, "
        "p.esfuerzo_percibido, p.cansancio, p.estado_animo, p.dolor_fisico, "
        "IFNULL(p.posicion_jugada, ''), IFNULL(p.formato_partido, ''), p.tipo_partido, "
        "p.tarjeta, IFNULL(can.nombre, ''), IFNULL(cam.nombre, ''), p.clima, "
        "p.temperatura_c, p.precip_mm, p.wind_kmh, p.apparent_temp_c, "
        "IFNULL(p.condicion_cancha, ''), p.estado_cancha, IFNULL(p.arbitraje, ''), "
        "p.arbitraje_score, IFNULL(p.comentario_personal, ''), IFNULL(p.lo_mejor, ''), "
        "IFNULL(p.que_mejorar, ''), IFNULL(p.tags, ''), IFNULL(p.eventos_clave, ''), "
        "p.goles_equipo, p.goles_rival "
        "FROM partido p "
        "LEFT JOIN cancha can ON p.cancha_id = can.id "
        "LEFT JOIN camiseta cam ON p.camiseta_id = cam.id "
        "WHERE p.id = ?;";

    if (!p)
    {
        return;
    }

    memset(p, 0, sizeof(*p));
    p->id = id;
    p->goles_equipo = -1;
    p->goles_rival = -1;

    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        resumen_anio_copiar_texto(p->fecha_hora, sizeof(p->fecha_hora), stmt, 0);
        resumen_anio_copiar_texto(p->rival, sizeof(p->rival), stmt, 1);
        resumen_anio_copiar_texto(p->tipo_rival, sizeof(p->tipo_rival), stmt, 2);
        p->resultado = sqlite3_column_int(stmt, 3);
        p->goles = sqlite3_column_int(stmt, 4);
        p->asistencias = sqlite3_column_int(stmt, 5);
        resumen_anio_copiar_texto(p->textos.goles_detalle, sizeof(p->textos.goles_detalle), stmt, 6);
        resumen_anio_copiar_texto(p->textos.asistencias_detalle, sizeof(p->textos.asistencias_detalle), stmt, 7);
        p->rendimiento_general = sqlite3_column_int(stmt, 8);
        p->fisico.rating_tecnico = sqlite3_column_int(stmt, 9);
        p->fisico.rating_fisico = sqlite3_column_int(stmt, 10);
        p->fisico.rating_mental = sqlite3_column_int(stmt, 11);
        p->fisico.minutos_jugados = sqlite3_column_int(stmt, 12);
        p->fisico.intensidad = sqlite3_column_int(stmt, 13);
        p->fisico.esfuerzo_percibido = sqlite3_column_int(stmt, 14);
        p->fisico.cansancio = sqlite3_column_int(stmt, 15);
        p->fisico.estado_animo = sqlite3_column_int(stmt, 16);
        p->fisico.dolor_fisico = sqlite3_column_int(stmt, 17);
        resumen_anio_copiar_texto(p->textos.posicion_jugada, sizeof(p->textos.posicion_jugada), stmt, 18);
        resumen_anio_copiar_texto(p->textos.formato_partido, sizeof(p->textos.formato_partido), stmt, 19);
        p->tipo_partido = sqlite3_column_int(stmt, 20);
        p->tarjeta = sqlite3_column_int(stmt, 21);
        resumen_anio_copiar_texto(p->cancha, sizeof(p->cancha), stmt, 22);
        resumen_anio_copiar_texto(p->camiseta, sizeof(p->camiseta), stmt, 23);
        p->meteo.codigo = sqlite3_column_int(stmt, 24);
        p->meteo.tiene_temperatura = sqlite3_column_type(stmt, 25) != SQLITE_NULL;
        p->meteo.temperatura_c = sqlite3_column_double(stmt, 25);
        p->meteo.tiene_precipitacion = sqlite3_column_type(stmt, 26) != SQLITE_NULL;
        p->meteo.precip_mm = sqlite3_column_double(stmt, 26);
        p->meteo.tiene_viento = sqlite3_column_type(stmt, 27) != SQLITE_NULL;
        p->meteo.wind_kmh = sqlite3_column_double(stmt, 27);
        p->meteo.tiene_sensacion = sqlite3_column_type(stmt, 28) != SQLITE_NULL;
        p->meteo.apparent_temp_c = sqlite3_column_double(stmt, 28);
        resumen_anio_copiar_texto(p->meteo.condicion_cancha, sizeof(p->meteo.condicion_cancha), stmt, 29);
        p->meteo.estado_cancha = sqlite3_column_int(stmt, 30);
        resumen_anio_copiar_texto(p->textos.arbitraje, sizeof(p->textos.arbitraje), stmt, 31);
        p->arbitraje_score = sqlite3_column_int(stmt, 32);
        resumen_anio_copiar_texto(p->textos.comentario, sizeof(p->textos.comentario), stmt, 33);
        resumen_anio_copiar_texto(p->textos.lo_mejor, sizeof(p->textos.lo_mejor), stmt, 34);
        resumen_anio_copiar_texto(p->textos.que_mejorar, sizeof(p->textos.que_mejorar), stmt, 35);
        resumen_anio_copiar_texto(p->textos.tags, sizeof(p->textos.tags), stmt, 36);
        resumen_anio_copiar_texto(p->textos.eventos_clave, sizeof(p->textos.eventos_clave), stmt, 37);
        p->goles_equipo = sqlite3_column_int(stmt, 38);
        p->goles_rival = sqlite3_column_int(stmt, 39);

        if (p->goles_equipo >= 0 && p->goles_rival >= 0)
        {
            p->diferencia_gol = p->goles_equipo - p->goles_rival;
        }
    }

    sqlite3_finalize(stmt);
}

static void resumen_anio_texto_rendimiento(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->fisico.rating_tecnico > 0 || p->fisico.rating_fisico > 0 || p->fisico.rating_mental > 0)
    {
        snprintf(temp, sizeof(temp), "%d/10 (tecnico %d, fisico %d, mental %d)",
                 p->rendimiento_general, p->fisico.rating_tecnico, p->fisico.rating_fisico, p->fisico.rating_mental);
    }
    else
    {
        snprintf(temp, sizeof(temp), "%d/10", p->rendimiento_general);
    }
    resumen_anio_append(buffer, tam, &usados, temp);
}

static void resumen_anio_texto_marcador(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;
    int resultado;

    buffer[0] = '\0';
    if (p->goles_equipo < 0 || p->goles_rival < 0)
    {
        return;
    }

    /* El resultado se deduce del marcador: la columna resultado puede no coincidir. */
    if (p->goles_equipo > p->goles_rival)
    {
        resultado = 1;
    }
    else if (p->goles_equipo == p->goles_rival)
    {
        resultado = 2;
    }
    else
    {
        resultado = 3;
    }

    snprintf(temp, sizeof(temp), "%d - %d (%s)", p->goles_equipo, p->goles_rival,
             resultado_to_text(resultado));
    resumen_anio_append(buffer, tam, &usados, temp);
    snprintf(temp, sizeof(temp), "Diferencia: %+d", p->diferencia_gol);
    resumen_anio_append(buffer, tam, &usados, temp);
}

static void resumen_anio_texto_clima(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->meteo.codigo >= 1 && p->meteo.codigo <= 12)
    {
        resumen_anio_append(buffer, tam, &usados, clima_to_text(p->meteo.codigo));
    }

    if (p->meteo.tiene_temperatura)
    {
        snprintf(temp, sizeof(temp), "%.1f C", p->meteo.temperatura_c);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->meteo.tiene_sensacion)
    {
        snprintf(temp, sizeof(temp), "sensacion %.1f C", p->meteo.apparent_temp_c);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->meteo.tiene_viento)
    {
        snprintf(temp, sizeof(temp), "viento %.1f km/h", p->meteo.wind_kmh);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->meteo.tiene_precipitacion)
    {
        snprintf(temp, sizeof(temp), "lluvia %.1f mm", p->meteo.precip_mm);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

static void resumen_anio_texto_fisico(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->fisico.minutos_jugados > 0)
    {
        snprintf(temp, sizeof(temp), "Duracion %d min", p->fisico.minutos_jugados);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->fisico.intensidad > 0)
    {
        snprintf(temp, sizeof(temp), "Intensidad %d", p->fisico.intensidad);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->fisico.esfuerzo_percibido > 0)
    {
        snprintf(temp, sizeof(temp), "Esfuerzo %d", p->fisico.esfuerzo_percibido);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->fisico.cansancio > 0)
    {
        snprintf(temp, sizeof(temp), "Cansancio %d", p->fisico.cansancio);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->fisico.estado_animo > 0)
    {
        snprintf(temp, sizeof(temp), "Animo %d", p->fisico.estado_animo);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->fisico.dolor_fisico > 0)
    {
        snprintf(temp, sizeof(temp), "Dolor %s", resumen_anio_dolor_texto(p->fisico.dolor_fisico));
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

static void resumen_anio_texto_contexto(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[128];
    int usados = 0;

    buffer[0] = '\0';
    snprintf(temp, sizeof(temp), "%s", resumen_anio_tipo_partido_texto(p->tipo_partido));
    resumen_anio_append(buffer, tam, &usados, temp);

    if (p->textos.formato_partido[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "Formato %s", p->textos.formato_partido);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->tarjeta > 1)
    {
        snprintf(temp, sizeof(temp), "Tarjeta %s", resumen_anio_tarjeta_texto(p->tarjeta));
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->textos.posicion_jugada[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "Posicion %s", p->textos.posicion_jugada);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

static void resumen_anio_texto_arbitraje(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->textos.arbitraje[0] != '\0' && p->arbitraje_score > 0)
    {
        snprintf(temp, sizeof(temp), "%s (%d/5)", p->textos.arbitraje, p->arbitraje_score);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    else if (p->textos.arbitraje[0] != '\0')
    {
        resumen_anio_append(buffer, tam, &usados, p->textos.arbitraje);
    }
    else if (p->arbitraje_score > 0)
    {
        snprintf(temp, sizeof(temp), "%s (%d/5)", resumen_anio_arbitraje_texto(p->arbitraje_score),
                 p->arbitraje_score);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

/* ── Calculo del resumen anual ───────────────────────────────────────────── */

/**
 * @brief Conteo de apariciones de un grupo (cancha o camiseta) en el año.
 */
typedef struct
{
    int ids[RESUMEN_ANIO_MAX_GRUPOS];
    int cuentas[RESUMEN_ANIO_MAX_GRUPOS];
    int total;
} ResumenAnioGrupoConteo;

/**
 * @brief Estado de trabajo mientras se recorren los partidos del año.
 */
typedef struct
{
    ResumenAnioGrupoConteo canchas;
    ResumenAnioGrupoConteo camisetas;
    long rendimiento_mes[RESUMEN_ANIO_MESES];
} ResumenAnioRecorrido;

static void resumen_anio_contar_resultado(int resultado, ResumenAnioMes *fila)
{
    if (resultado == 1)
    {
        fila->victorias++;
    }
    else if (resultado == 2)
    {
        fila->empates++;
    }
    else if (resultado == 3)
    {
        fila->derrotas++;
    }
}

static void resumen_anio_acumular_grupo(ResumenAnioGrupoConteo *grupo, int id)
{
    int indice;

    if (id <= 0)
    {
        return;
    }

    indice = resumen_anio_indice_grupo(grupo->ids, grupo->cuentas, &grupo->total, id);
    if (indice >= 0)
    {
        grupo->cuentas[indice]++;
    }
}

static void resumen_anio_resolver_grupo(ResumenAnioGrupo *destino,
                                        const ResumenAnioGrupoConteo *grupo,
                                        const char *sql)
{
    int id_top = -1;

    destino->usadas = grupo->total;
    resumen_anio_top_grupo(grupo->ids, grupo->cuentas, grupo->total, &id_top,
                           &destino->top_partidos);
    if (id_top >= 0)
    {
        resumen_anio_nombre_por_id(sql, id_top, destino->top, sizeof(destino->top));
    }
}

/* Suma un partido del año: mes, marcador del equipo, tendencia y grupos. */
static void resumen_anio_procesar_fila(ResumenAnio *r, sqlite3_stmt *stmt,
                                       ResumenAnioRecorrido *rec)
{
    const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
    int goles = sqlite3_column_int(stmt, 1);
    int asistencias = sqlite3_column_int(stmt, 2);
    int resultado = sqlite3_column_int(stmt, 3);
    int rendimiento = sqlite3_column_int(stmt, 4);
    int cancha_id = sqlite3_column_int(stmt, 5);
    int camiseta_id = sqlite3_column_int(stmt, 6);
    int goles_equipo = sqlite3_column_int(stmt, 7);
    int goles_rival = sqlite3_column_int(stmt, 8);
    int anio;
    int mes;
    int dia;
    int hora;

    if (goles_equipo >= 0 && goles_rival >= 0)
    {
        r->goles_favor += goles_equipo;
        r->goles_contra += goles_rival;
        r->partidos_con_marcador++;
    }

    if (resumen_parsear_fecha(fecha, &anio, &mes, &dia, &hora))
    {
        ResumenAnioMes *fila = &r->meses[mes - 1];

        fila->partidos++;
        fila->goles += goles;
        fila->asistencias += asistencias;
        resumen_anio_contar_resultado(resultado, fila);
        if (rendimiento > 0)
        {
            fila->con_rendimiento++;
            rec->rendimiento_mes[mes - 1] += rendimiento;
        }
    }

    if (rendimiento > 0)
    {
        resumen_ventana_int(r->tendencia, &r->tendencia_n, RESUMEN_ANIO_TENDENCIA_MAX,
                            rendimiento);
    }

    resumen_anio_acumular_grupo(&rec->canchas, cancha_id);
    resumen_anio_acumular_grupo(&rec->camisetas, camiseta_id);
}

/* Recorre los partidos del año en orden cronologico, con el mismo filtro por
 * año que el resumen general. */
static int resumen_anio_leer_partidos(ResumenAnio *r)
{
    sqlite3_stmt *stmt = NULL;
    ResumenAnioRecorrido rec;
    const char *sql =
        "SELECT fecha_hora, goles, asistencias, IFNULL(resultado, 0), "
        "IFNULL(rendimiento_general, 0), IFNULL(cancha_id, 0), IFNULL(camiseta_id, 0), "
        "IFNULL(goles_equipo, -1), IFNULL(goles_rival, -1) "
        "FROM partido WHERE " RESUMEN_SQL_FILTRO_ANIO("fecha_hora")
        " ORDER BY " SQL_PARTIDO_CRONO("", "ASC") ";";

    memset(&rec, 0, sizeof(rec));
    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, r->anio);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        resumen_anio_procesar_fila(r, stmt, &rec);
    }
    sqlite3_finalize(stmt);

    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        ResumenAnioMes *m = &r->meses[i];
        m->rendimiento_prom = m->con_rendimiento > 0
                              ? (double)rec.rendimiento_mes[i] / m->con_rendimiento
                              : 0.0;
    }

    resumen_anio_resolver_grupo(&r->cancha, &rec.canchas,
                                "SELECT nombre FROM cancha WHERE id = ?;");
    resumen_anio_resolver_grupo(&r->camiseta, &rec.camisetas,
                                "SELECT nombre FROM camiseta WHERE id = ?;");
    return 1;
}

/* El estado de la lesion se guarda en mayusculas ("ACTIVA") o no. */
static int resumen_anio_es_activa(const char *estado)
{
    const char *esperado = "activa";

    if (!estado)
    {
        return 0;
    }
    for (int i = 0; esperado[i] != '\0'; i++)
    {
        if (tolower((unsigned char)estado[i]) != esperado[i])
        {
            return 0;
        }
    }
    return estado[6] == '\0';
}

/**
 * @brief Cuenta las lesiones registradas en el año y cuantas siguen activas.
 */
static void resumen_anio_leer_lesiones(ResumenAnio *r)
{
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt("SELECT fecha, estado FROM lesion;", &stmt))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        const char *estado = (const char *)sqlite3_column_text(stmt, 1);

        if (resumen_anio_desde_fecha(fecha) != r->anio)
        {
            continue;
        }

        r->lesiones++;
        if (resumen_anio_es_activa(estado))
        {
            r->lesiones_activas++;
        }
    }
    sqlite3_finalize(stmt);
}

/* Mejor mes: mayor rendimiento promedio entre los meses con al menos 2
 * partidos con rendimiento (o 1 si ninguno llega); empata el de mas partidos
 * y despues el primero. */
static int resumen_anio_elegir_mejor_mes(const ResumenAnio *r)
{
    int minimo = 1;
    int mejor = -1;

    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        if (r->meses[i].con_rendimiento >= 2)
        {
            minimo = 2;
        }
    }
    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        const ResumenAnioMes *m = &r->meses[i];

        if (m->con_rendimiento < minimo)
        {
            continue;
        }
        if (mejor < 0 || m->rendimiento_prom > r->meses[mejor].rendimiento_prom ||
                (m->rendimiento_prom == r->meses[mejor].rendimiento_prom &&
                 m->partidos > r->meses[mejor].partidos))
        {
            mejor = i;
        }
    }
    return mejor;
}

/* Mes con mas goles + asistencias (empata el primero); -1 si no hubo. */
static int resumen_anio_elegir_mes_goleador(const ResumenAnio *r)
{
    int mejor = -1;
    int max = 0;

    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        int ga = r->meses[i].goles + r->meses[i].asistencias;
        if (ga > max)
        {
            max = ga;
            mejor = i;
        }
    }
    return mejor;
}

static void resumen_anio_totales(ResumenAnioTotales *t, const ResumenGeneral *g, int anio)
{
    t->anio = anio;
    t->partidos = g->partidos;
    t->goles = g->goles;
    t->asistencias = g->asistencias;
    t->ga_pp = g->ga_pp;
    t->con_resultado = g->con_resultado;
    t->efectividad_pct = g->efectividad_pct;
    t->con_rendimiento = g->con_rendimiento;
    t->rendimiento_prom = g->rendimiento_prom;
}

/* Totales del año anterior para la comparacion (solo si tiene partidos). */
static void resumen_anio_cargar_anterior(ResumenAnio *r)
{
    ResumenGeneral *previo;

    if (r->anio <= 1)
    {
        return;
    }
    previo = malloc(sizeof(*previo));
    if (!previo)
    {
        return;
    }
    if (resumen_general_calcular_anio(previo, r->anio - 1) && previo->partidos > 0)
    {
        r->hay_anterior = 1;
        resumen_anio_totales(&r->anterior, previo, r->anio - 1);
    }
    free(previo);
}

int resumen_anio_calcular(ResumenAnio *r, int anio)
{
    int destacado;

    if (!r || anio <= 0)
    {
        return 0;
    }
    memset(r, 0, sizeof(*r));
    r->anio = anio;
    r->mejor_mes = -1;
    r->mes_goleador = -1;
    snprintf(r->cancha.top, sizeof(r->cancha.top), "Sin datos");
    snprintf(r->camiseta.top, sizeof(r->camiseta.top), "Sin datos");

    if (!resumen_general_calcular_anio(&r->g, anio) || !resumen_anio_leer_partidos(r))
    {
        return 0;
    }
    snprintf(r->jugador, sizeof(r->jugador), "%s", r->g.jugador);
    snprintf(r->fecha_gen, sizeof(r->fecha_gen), "%s", r->g.fecha_gen);

    resumen_anio_leer_lesiones(r);
    r->camiseta.activas = resumen_anio_contar("SELECT COUNT(*) FROM camiseta WHERE activa = 1;");
    r->cancha.activas = resumen_anio_contar("SELECT COUNT(*) FROM cancha WHERE activa = 1;");

    r->mejor_mes = resumen_anio_elegir_mejor_mes(r);
    r->mes_goleador = resumen_anio_elegir_mes_goleador(r);

    /* Partido destacado: el de mejor rendimiento; sin rendimientos, el ultimo. */
    destacado = r->g.mejor_rendimiento.id > 0 ? r->g.mejor_rendimiento.id : r->g.ultimo.id;
    if (r->g.partidos > 0 && destacado > 0)
    {
        r->mejor_id = destacado;
        resumen_anio_cargar_partido(destacado, &r->mejor);
    }

    resumen_anio_cargar_anterior(r);
    return 1;
}

/* ── Textos comunes de los tres formatos ─────────────────────────────────── */

static const char *resumen_anio_mes_corto(int indice)
{
    static const char *const nombres[RESUMEN_ANIO_MESES] =
    {
        "Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"
    };
    return (indice >= 0 && indice < RESUMEN_ANIO_MESES) ? nombres[indice] : "-";
}

/* Cancha del mejor partido con su estado y condicion. */
static void resumen_anio_texto_cancha(const ResumenAnioPartido *p, char *texto, size_t tam)
{
    char temp[160];
    int usados = 0;

    texto[0] = '\0';
    resumen_anio_append(texto, tam, &usados, p->cancha);
    if (p->meteo.estado_cancha > 0)
    {
        snprintf(temp, sizeof(temp), "estado %s",
                 resumen_anio_estado_cancha_texto(p->meteo.estado_cancha));
        resumen_anio_append(texto, tam, &usados, temp);
    }
    if (p->meteo.condicion_cancha[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "condicion %s", p->meteo.condicion_cancha);
        resumen_anio_append(texto, tam, &usados, temp);
    }
}

/* Goles o asistencias del mejor partido con su detalle; "" si no hubo. */
static void resumen_anio_texto_aporte(int cantidad, const char *detalle, char *texto, size_t tam)
{
    if (detalle[0] != '\0')
    {
        snprintf(texto, tam, "%d [%s]", cantidad, detalle);
    }
    else if (cantidad > 0)
    {
        snprintf(texto, tam, "%d", cantidad);
    }
    else
    {
        texto[0] = '\0';
    }
}

/* Recorre los datos del mejor partido como pares etiqueta/valor (UTF-8, sin
 * escapar). Los valores vacios se saltean. */
typedef void (*ResumenAnioDetalleFn)(void *destino, const char *etiqueta, const char *valor);

static void resumen_anio_recorrer_detalle(const ResumenAnioPartido *p, ResumenAnioDetalleFn fn,
        void *destino)
{
    char texto[1100];

    resumen_anio_texto_rendimiento(p, texto, sizeof(texto));
    fn(destino, "Rendimiento", texto);
    fn(destino, "Fecha", p->fecha_hora[0] != '\0' ? p->fecha_hora : "Sin registrar");
    fn(destino, "Rival", p->rival[0] != '\0' ? p->rival : "Sin registrar");
    fn(destino, "Tipo de rival", p->tipo_rival);
    resumen_anio_texto_marcador(p, texto, sizeof(texto));
    fn(destino, "Marcador", texto);
    resumen_anio_texto_aporte(p->goles, p->textos.goles_detalle, texto, sizeof(texto));
    fn(destino, "Goles", texto);
    resumen_anio_texto_aporte(p->asistencias, p->textos.asistencias_detalle, texto,
                              sizeof(texto));
    fn(destino, "Asistencias", texto);
    resumen_anio_texto_cancha(p, texto, sizeof(texto));
    fn(destino, "Cancha", texto);
    fn(destino, "Camiseta", p->camiseta);
    resumen_anio_texto_clima(p, texto, sizeof(texto));
    fn(destino, "Clima", texto);
    resumen_anio_texto_fisico(p, texto, sizeof(texto));
    fn(destino, "Físico", texto);
    resumen_anio_texto_contexto(p, texto, sizeof(texto));
    fn(destino, "Contexto", texto);
    resumen_anio_texto_arbitraje(p, texto, sizeof(texto));
    fn(destino, "Arbitraje", texto);
    fn(destino, "Lo mejor", p->textos.lo_mejor);
    fn(destino, "A mejorar", p->textos.que_mejorar);
    fn(destino, "Comentario", p->textos.comentario);
    fn(destino, "Tags", p->textos.tags);
    fn(destino, "Eventos clave", p->textos.eventos_clave);
}

/* Una fila de la comparacion con el año anterior. */
typedef struct
{
    const char *dato;
    double actual;
    double previo;
    int decimales;
    int es_pct; /**< Efectividad: el cambio se muestra en puntos */
    int valido; /**< 0 si alguno de los dos años no tiene el dato */
} ResumenAnioComparacion;

#define RESUMEN_ANIO_COMPARACIONES 6

static int resumen_anio_comparaciones(const ResumenAnio *r,
                                      ResumenAnioComparacion c[RESUMEN_ANIO_COMPARACIONES])
{
    const ResumenGeneral *g = &r->g;
    const ResumenAnioTotales *a = &r->anterior;
    const ResumenAnioComparacion filas[RESUMEN_ANIO_COMPARACIONES] =
    {
        {"Partidos", g->partidos, a->partidos, 0, 0, 1},
        {"Goles", g->goles, a->goles, 0, 0, 1},
        {"Asistencias", g->asistencias, a->asistencias, 0, 0, 1},
        {"G+A por partido", g->ga_pp, a->ga_pp, 2, 0, 1},
        {
            "Efectividad", g->efectividad_pct, a->efectividad_pct, 0, 1,
            g->con_resultado > 0 && a->con_resultado > 0
        },
        {
            "Rendimiento", g->rendimiento_prom, a->rendimiento_prom, 1, 0,
            g->con_rendimiento > 0 && a->con_rendimiento > 0
        },
    };

    memcpy(c, filas, sizeof(filas));
    return RESUMEN_ANIO_COMPARACIONES;
}

/* Signo del cambio redondeado a los decimales mostrados: 1, -1 o 0. */
static int resumen_anio_signo_delta(const ResumenAnioComparacion *c)
{
    double escala = c->decimales == 2 ? 100.0 : (c->decimales == 1 ? 10.0 : 1.0);
    long a = (long)(c->actual * escala + (c->actual >= 0 ? 0.5 : -0.5));
    long p = (long)(c->previo * escala + (c->previo >= 0 ? 0.5 : -0.5));

    return a > p ? 1 : (a < p ? -1 : 0);
}

static double resumen_anio_abs(double v)
{
    return v < 0 ? -v : v;
}

/* ── Formato TXT ──────────────────────────────────────────────────────────── */

/* Ancho de linea del TXT (entra en una consola o en un mensaje). */
#define RESUMEN_TXT_ANCHO 66
/* Ancho de las etiquetas en las filas clave/valor. */
#define RESUMEN_TXT_ETIQUETA 26

/* Caracteres visibles de un texto UTF-8 (para alinear con acentos). */
static int resumen_txt_largo(const char *texto)
{
    int n = 0;

    for (const unsigned char *p = (const unsigned char *)texto; *p; p++)
    {
        if ((*p & 0xC0) != 0x80)
        {
            n++;
        }
    }
    return n;
}

/* Escribe el texto y completa con espacios hasta @p ancho caracteres. */
static void resumen_txt_celda(FILE *f, const char *texto, int ancho)
{
    fputs(texto, f);
    for (int i = resumen_txt_largo(texto); i < ancho; i++)
    {
        fputc(' ', f);
    }
}

/* Igual, alineado a la derecha. */
static void resumen_txt_celda_der(FILE *f, const char *texto, int ancho)
{
    for (int i = resumen_txt_largo(texto); i < ancho; i++)
    {
        fputc(' ', f);
    }
    fputs(texto, f);
}

static void resumen_txt_repetir(FILE *f, char c, int n)
{
    for (int i = 0; i < n; i++)
    {
        fputc(c, f);
    }
}

static void resumen_txt_seccion(FILE *f, const char *titulo)
{
    fputs("\n== ", f);
    fputs(titulo, f);
    fputc(' ', f);
    resumen_txt_repetir(f, '=', RESUMEN_TXT_ANCHO - 4 - resumen_txt_largo(titulo));
    fputc('\n', f);
}

static void resumen_txt_fila(FILE *f, const char *k, const char *v)
{
    fputs("  ", f);
    resumen_txt_celda(f, k, RESUMEN_TXT_ETIQUETA);
    fputs(v, f);
    fputc('\n', f);
}

static void resumen_txt_fila_int(FILE *f, const char *k, int v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", v);
    resumen_txt_fila(f, k, buf);
}

/* Barra ASCII: '#' proporcional a parte/total sobre @p ancho, resto '.'. */
static void resumen_txt_barra(FILE *f, double parte, double total, int ancho)
{
    int llenos = total > 0 ? (int)(parte / total * ancho + 0.5) : 0;

    if (llenos > ancho)
    {
        llenos = ancho;
    }
    resumen_txt_repetir(f, '#', llenos);
    resumen_txt_repetir(f, '.', ancho - llenos);
}

static void resumen_anio_txt_cabecera(FILE *f, const ResumenAnio *r)
{
    char titulo[64];

    resumen_txt_repetir(f, '=', RESUMEN_TXT_ANCHO);
    snprintf(titulo, sizeof(titulo), "\n  RESUMEN DEL AÑO %d\n", r->anio);
    fputs(titulo, f);
    fprintf(f, "  Jugador : %s\n", r->jugador);
    fprintf(f, "  Generado: %s\n", r->fecha_gen);
    if (r->g.partidos > 0)
    {
        fprintf(f, "  Período : %d partido%s, del %s al %s\n", r->g.partidos,
                r->g.partidos == 1 ? "" : "s", r->g.primer_partido_fecha, r->g.ultimo.fecha);
    }
    resumen_txt_repetir(f, '=', RESUMEN_TXT_ANCHO);
    fputc('\n', f);

    if (r->g.partidos == 0)
    {
        fprintf(f, "\n  Sin partidos registrados en %d. Se muestran el equipamiento y las\n"
                "  lesiones del año.\n",
                r->anio);
    }
}

static void resumen_anio_txt_resultados(FILE *f, const ResumenGeneral *g)
{
    char buf[96];
    const int ancho = 40;
    int nv;
    int ne;

    resumen_txt_seccion(f, "PARTIDOS Y RESULTADOS");
    resumen_txt_fila_int(f, "Partidos jugados", g->partidos);
    resumen_txt_fila_int(f, "Goles", g->goles);
    resumen_txt_fila_int(f, "Asistencias", g->asistencias);
    if (g->con_resultado == 0)
    {
        resumen_txt_fila(f, "Resultados", "sin cargar");
        return;
    }
    snprintf(buf, sizeof(buf), "%d V  %d E  %d D", g->victorias, g->empates, g->derrotas);
    resumen_txt_fila(f, "Resultados", buf);
    snprintf(buf, sizeof(buf), "%s%% (sobre %d con resultado)", rnum(g->efectividad_pct, 1),
             g->con_resultado);
    resumen_txt_fila(f, "Efectividad", buf);

    /* Barra apilada: V, E y D sobre el ancho total. */
    nv = (int)((double)g->victorias * ancho / g->con_resultado + 0.5);
    ne = (int)((double)g->empates * ancho / g->con_resultado + 0.5);
    if (nv + ne > ancho)
    {
        ne = ancho - nv;
    }
    fputs("  ", f);
    resumen_txt_celda(f, "", RESUMEN_TXT_ETIQUETA);
    fputc('[', f);
    resumen_txt_repetir(f, 'V', nv);
    resumen_txt_repetir(f, 'E', ne);
    resumen_txt_repetir(f, 'D', g->derrotas > 0 ? ancho - nv - ne : 0);
    resumen_txt_repetir(f, ' ', g->derrotas > 0 ? 0 : ancho - nv - ne);
    fputs("]\n", f);
}

static void resumen_anio_txt_ataque(FILE *f, const ResumenGeneral *g)
{
    char buf[96];

    if (g->partidos == 0)
    {
        return;
    }
    resumen_txt_seccion(f, "ATAQUE");
    resumen_txt_fila(f, "G+A por partido", rnum(g->ga_pp, 2));
    resumen_txt_fila(f, "Goles por partido", rnum(g->goles_pp, 2));
    resumen_txt_fila(f, "Asistencias por partido", rnum(g->asistencias_pp, 2));
    snprintf(buf, sizeof(buf), "%d (%s%%)", g->partidos_con_gol,
             rnum(resumen_pct(g->partidos_con_gol, g->partidos), 0));
    resumen_txt_fila(f, "Partidos con gol", buf);
    resumen_txt_fila_int(f, "Partidos con asistencia", g->partidos_con_asistencia);
    resumen_txt_fila_int(f, "Dobletes", g->dobletes);
    resumen_txt_fila_int(f, "Hat-tricks o más", g->hat_tricks);
    if (g->partidos_con_minutos > 0)
    {
        snprintf(buf, sizeof(buf), "%s (en %d partidos con minutos)", rnum(g->ga_por_90, 2),
                 g->partidos_con_minutos);
        resumen_txt_fila(f, "G+A cada 90 minutos", buf);
    }
}

static void resumen_anio_txt_comparacion(FILE *f, const ResumenAnio *r)
{
    ResumenAnioComparacion c[RESUMEN_ANIO_COMPARACIONES];
    char titulo[64];
    char buf[48];
    int n;

    if (!r->hay_anterior)
    {
        return;
    }
    snprintf(titulo, sizeof(titulo), "COMPARACIÓN CON %d", r->anterior.anio);
    resumen_txt_seccion(f, titulo);
    fputs("  ", f);
    resumen_txt_celda(f, "", RESUMEN_TXT_ETIQUETA - 4);
    snprintf(buf, sizeof(buf), "%d", r->anio);
    resumen_txt_celda_der(f, buf, 10);
    snprintf(buf, sizeof(buf), "%d", r->anterior.anio);
    resumen_txt_celda_der(f, buf, 10);
    resumen_txt_celda_der(f, "Cambio", 14);
    fputc('\n', f);

    n = resumen_anio_comparaciones(r, c);
    for (int i = 0; i < n; i++)
    {
        const char *flecha;
        int signo;

        if (!c[i].valido)
        {
            continue;
        }
        signo = resumen_anio_signo_delta(&c[i]);
        flecha = signo > 0 ? "↑" : (signo < 0 ? "↓" : "=");
        fputs("  ", f);
        resumen_txt_celda(f, c[i].dato, RESUMEN_TXT_ETIQUETA - 4);
        snprintf(buf, sizeof(buf), "%s%s", rnum(c[i].actual, c[i].decimales),
                 c[i].es_pct ? "%" : "");
        resumen_txt_celda_der(f, buf, 10);
        snprintf(buf, sizeof(buf), "%s%s", rnum(c[i].previo, c[i].decimales),
                 c[i].es_pct ? "%" : "");
        resumen_txt_celda_der(f, buf, 10);
        if (signo == 0)
        {
            snprintf(buf, sizeof(buf), "= igual");
        }
        else
        {
            snprintf(buf, sizeof(buf), "%s %s%s", flecha,
                     rnum(resumen_anio_abs(c[i].actual - c[i].previo), c[i].decimales),
                     c[i].es_pct ? " pts" : "");
        }
        resumen_txt_celda_der(f, buf, 14);
        fputc('\n', f);
    }
}

/* Forma de los ultimos 10 y grafico de barras verticales del rendimiento. */
static void resumen_anio_txt_forma(FILE *f, const ResumenAnio *r)
{
    char etiqueta[24];
    double suma = 0.0;

    if (r->g.ultimos_n == 0 && r->tendencia_n < 2)
    {
        return;
    }
    resumen_txt_seccion(f, "FORMA Y TENDENCIA");
    if (r->g.ultimos_n > 0)
    {
        snprintf(etiqueta, sizeof(etiqueta), "Últimos %d", r->g.ultimos_n);
        fputs("  ", f);
        resumen_txt_celda(f, etiqueta, RESUMEN_TXT_ETIQUETA);
        for (int i = 0; i < r->g.ultimos_n; i++)
        {
            fprintf(f, "%c ", resumen_letra_resultado(r->g.ultimos[i].resultado));
        }
        fputs(" (viejo -> nuevo)\n", f);
    }
    if (r->tendencia_n < 2)
    {
        return;
    }

    /* Una columna por partido; cada fila es un escalon de 2 puntos. */
    fprintf(f, "\n  Rendimiento de los %d partidos del año con dato (1 a 10):\n",
            r->tendencia_n);
    for (int nivel = 10; nivel >= 2; nivel -= 2)
    {
        fprintf(f, "  %2d | ", nivel);
        for (int i = 0; i < r->tendencia_n; i++)
        {
            fputc(r->tendencia[i] >= nivel - 1 ? '#' : ' ', f);
        }
        fputc('\n', f);
    }
    fputs("     +-", f);
    resumen_txt_repetir(f, '-', r->tendencia_n);
    fputc('\n', f);
    for (int i = 0; i < r->tendencia_n; i++)
    {
        suma += r->tendencia[i];
    }
    fprintf(f, "  Promedio %s, último %d/10.\n", rnum(suma / r->tendencia_n, 1),
            r->tendencia[r->tendencia_n - 1]);
}

static void resumen_anio_txt_partido(FILE *f, const char *k, const char *valor,
                                     const ResumenPartidoCorto *p)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (%s%s%s)", valor, p->fecha, p->rival[0] ? " vs " : "",
             p->rival);
    resumen_txt_fila(f, k, buf);
}

static void resumen_anio_txt_records(FILE *f, const ResumenGeneral *g)
{
    char buf[48];

    if (g->partidos == 0)
    {
        return;
    }
    resumen_txt_seccion(f, "RÉCORDS DEL AÑO");
    if (g->con_resultado > 0)
    {
        snprintf(buf, sizeof(buf), "%d partido%s", g->mejor_racha_victorias,
                 g->mejor_racha_victorias == 1 ? "" : "s");
        resumen_txt_fila(f, "Mejor racha de victorias", buf);
        snprintf(buf, sizeof(buf), "%d partido%s", g->mejor_racha_invicto,
                 g->mejor_racha_invicto == 1 ? "" : "s");
        resumen_txt_fila(f, "Mejor racha invicto", buf);
    }
    snprintf(buf, sizeof(buf), "%d partido%s", g->mejor_racha_marcando,
             g->mejor_racha_marcando == 1 ? "" : "s");
    resumen_txt_fila(f, "Racha más larga marcando", buf);
    if (g->max_goles.goles > 0)
    {
        snprintf(buf, sizeof(buf), "%d gol%s", g->max_goles.goles,
                 g->max_goles.goles == 1 ? "" : "es");
        resumen_anio_txt_partido(f, "Más goles en un partido", buf, &g->max_goles);
    }
    if (g->max_asistencias.asistencias > 0)
    {
        snprintf(buf, sizeof(buf), "%d asist.", g->max_asistencias.asistencias);
        resumen_anio_txt_partido(f, "Más asistencias", buf, &g->max_asistencias);
    }
    if (g->mejor_rendimiento.rendimiento > 0)
    {
        snprintf(buf, sizeof(buf), "%d/10", g->mejor_rendimiento.rendimiento);
        resumen_anio_txt_partido(f, "Mejor rendimiento", buf, &g->mejor_rendimiento);
    }
}

static void resumen_anio_txt_meses(FILE *f, const ResumenAnio *r)
{
    int max = 1;
    char buf[160];

    if (r->g.partidos == 0)
    {
        return;
    }
    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        int ga = r->meses[i].goles + r->meses[i].asistencias;
        max = ga > max ? ga : max;
    }

    resumen_txt_seccion(f, "MES A MES");
    fprintf(f, "  %-4s %3s %3s %3s %8s %5s  %s\n", "Mes", "PJ", "G", "A", "V-E-D", "Rend",
            "G+A");
    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        const ResumenAnioMes *m = &r->meses[i];
        char ved[24];

        if (m->partidos == 0)
        {
            fprintf(f, "  %-4s %3s\n", resumen_anio_mes_corto(i), "-");
            continue;
        }
        snprintf(ved, sizeof(ved), "%d-%d-%d", m->victorias, m->empates, m->derrotas);
        fprintf(f, "  %-4s %3d %3d %3d %8s %5s  ", resumen_anio_mes_corto(i), m->partidos,
                m->goles, m->asistencias, ved,
                m->con_rendimiento > 0 ? rnum(m->rendimiento_prom, 1) : "-");
        resumen_txt_barra(f, m->goles + m->asistencias, max, 20);
        fputs(i == r->mejor_mes ? "  <- mejor mes\n" : "\n", f);
    }
    if (r->mejor_mes >= 0)
    {
        const ResumenAnioMes *m = &r->meses[r->mejor_mes];
        snprintf(buf, sizeof(buf), "%s (rendimiento %s en %d partido%s)",
                 resumen_anio_mes_nombre(r->mejor_mes), rnum(m->rendimiento_prom, 1),
                 m->con_rendimiento, m->con_rendimiento == 1 ? "" : "s");
        fputc('\n', f);
        resumen_txt_fila(f, "Mejor mes", buf);
    }
    if (r->mes_goleador >= 0)
    {
        const ResumenAnioMes *m = &r->meses[r->mes_goleador];
        snprintf(buf, sizeof(buf), "%s (%d G + %d A)", resumen_anio_mes_nombre(r->mes_goleador),
                 m->goles, m->asistencias);
        resumen_txt_fila(f, "Mes más goleador", buf);
    }
}

static void resumen_anio_txt_promedio(FILE *f, const char *k, int con, double prom)
{
    if (con == 0)
    {
        return;
    }
    fputs("  ", f);
    resumen_txt_celda(f, k, 14);
    resumen_txt_celda_der(f, rnum(prom, 1), 5);
    fputs("  ", f);
    resumen_txt_barra(f, prom, 10.0, 30);
    fprintf(f, "  (%d partidos)\n", con);
}

static void resumen_anio_txt_como_llegas(FILE *f, const ResumenGeneral *g)
{
    if (g->con_rendimiento + g->con_animo + g->con_cansancio == 0)
    {
        return;
    }
    resumen_txt_seccion(f, "CÓMO LLEGÁS (PROMEDIOS DE 1 A 10)");
    resumen_anio_txt_promedio(f, "Rendimiento", g->con_rendimiento, g->rendimiento_prom);
    resumen_anio_txt_promedio(f, "Ánimo", g->con_animo, g->animo_prom);
    resumen_anio_txt_promedio(f, "Cansancio", g->con_cansancio, g->cansancio_prom);
}

static void resumen_anio_txt_ranking(FILE *f, const char *titulo, const ResumenTop *top)
{
    char buf[48];

    if (top->cantidad == 0)
    {
        return;
    }
    fprintf(f, "  %s\n", titulo);
    for (int i = 0; i < top->cantidad; i++)
    {
        const ResumenTopItem *it = &top->items[i];
        char nombre[40];

        snprintf(nombre, sizeof(nombre), "%.30s", it->nombre);
        fprintf(f, "    %d. ", i + 1);
        resumen_txt_celda(f, nombre, 26);
        snprintf(buf, sizeof(buf), "%3d PJ %4s%% V  ", it->partidos, rnum(it->victorias_pct, 0));
        fputs(buf, f);
        resumen_txt_barra(f, it->victorias_pct, 100.0, 16);
        fputc('\n', f);
    }
}

static void resumen_anio_txt_rankings(FILE *f, const ResumenGeneral *g)
{
    if (g->camisetas.cantidad + g->canchas.cantidad + g->rivales.cantidad == 0)
    {
        return;
    }
    resumen_txt_seccion(f, "DÓNDE Y CON QUÉ (TOP 3)");
    resumen_anio_txt_ranking(f, "Camisetas", &g->camisetas);
    resumen_anio_txt_ranking(f, "Canchas", &g->canchas);
    resumen_anio_txt_ranking(f, "Rivales", &g->rivales);
}

static void resumen_anio_txt_conteos(FILE *f, const char *titulo, const ResumenConteo *c, int n,
                                     const char *const *nombres)
{
    int max = resumen_max_conteo(c, n);

    if (max == 0)
    {
        return;
    }
    fprintf(f, "  %s\n", titulo);
    for (int i = 0; i < n; i++)
    {
        if (c[i].partidos == 0)
        {
            continue;
        }
        fputs("    ", f);
        resumen_txt_celda(f, nombres[i], 12);
        fprintf(f, "%3d  ", c[i].partidos);
        resumen_txt_barra(f, c[i].partidos, max, 24);
        fputc('\n', f);
    }
}

static void resumen_anio_txt_cuando(FILE *f, const ResumenGeneral *g)
{
    if (resumen_max_conteo(g->dias_semana, 7) == 0 &&
            resumen_max_conteo(g->franjas, RESUMEN_FRANJAS) == 0)
    {
        return;
    }
    resumen_txt_seccion(f, "CUÁNDO JUGÁS (PARTIDOS)");
    resumen_anio_txt_conteos(f, "Día de la semana", g->dias_semana, 7, MD_DIAS_SEMANA);
    resumen_anio_txt_conteos(f, "Franja horaria", g->franjas, RESUMEN_FRANJAS, MD_FRANJAS);
}

static void resumen_anio_txt_clima(FILE *f, const ResumenGeneral *g)
{
    char buf[160];

    if (g->clima_real_partidos == 0)
    {
        return;
    }
    resumen_txt_seccion(f, "CLIMA REAL (OPEN-METEO)");
    snprintf(buf, sizeof(buf), "%d (%s%% del año)", g->clima_real_partidos,
             rnum(g->clima_real_cobertura_pct, 0));
    resumen_txt_fila(f, "Partidos con clima real", buf);
    if (g->hay_clima_mejor)
    {
        snprintf(buf, sizeof(buf), "%s (rend. %s, %s%% V, %d PJ)", g->clima_mejor,
                 rnum(g->clima_mejor_rendimiento, 1), rnum(g->clima_mejor_victorias_pct, 0),
                 g->clima_mejor_partidos);
        resumen_txt_fila(f, "Tu mejor condición", buf);
    }
}

static void resumen_anio_txt_equipo(FILE *f, const ResumenAnio *r)
{
    char buf[200];

    if (r->partidos_con_marcador > 0)
    {
        resumen_txt_seccion(f, "MARCADOR DEL EQUIPO");
        resumen_txt_fila_int(f, "Goles a favor", r->goles_favor);
        resumen_txt_fila_int(f, "Goles en contra", r->goles_contra);
        snprintf(buf, sizeof(buf), "%+d", r->goles_favor - r->goles_contra);
        resumen_txt_fila(f, "Diferencia", buf);
        snprintf(buf, sizeof(buf), "%d partidos", r->partidos_con_marcador);
        resumen_txt_fila(f, "Con marcador cargado", buf);
    }

    resumen_txt_seccion(f, "EQUIPAMIENTO Y SALUD");
    snprintf(buf, sizeof(buf), "%d (de %d activas)", r->camiseta.usadas, r->camiseta.activas);
    resumen_txt_fila(f, "Camisetas usadas", buf);
    if (r->camiseta.top_partidos > 0)
    {
        snprintf(buf, sizeof(buf), "%s (%d partidos)", r->camiseta.top, r->camiseta.top_partidos);
        resumen_txt_fila(f, "Camiseta más usada", buf);
    }
    snprintf(buf, sizeof(buf), "%d (de %d activas)", r->cancha.usadas, r->cancha.activas);
    resumen_txt_fila(f, "Canchas usadas", buf);
    if (r->cancha.top_partidos > 0)
    {
        snprintf(buf, sizeof(buf), "%s (%d partidos)", r->cancha.top, r->cancha.top_partidos);
        resumen_txt_fila(f, "Cancha más usada", buf);
    }
    if (r->lesiones == 0)
    {
        resumen_txt_fila(f, "Lesiones del año", "ninguna");
    }
    else
    {
        snprintf(buf, sizeof(buf), "%d (%d activa%s)", r->lesiones, r->lesiones_activas,
                 r->lesiones_activas == 1 ? "" : "s");
        resumen_txt_fila(f, "Lesiones del año", buf);
    }
}

static void resumen_anio_txt_detalle(void *destino, const char *etiqueta, const char *valor)
{
    if (valor && valor[0] != '\0')
    {
        resumen_txt_fila((FILE *)destino, etiqueta, valor);
    }
}

void resumen_anio_escribir_txt(FILE *f, const ResumenAnio *r)
{
    if (!f || !r)
    {
        return;
    }
    resumen_anio_txt_cabecera(f, r);
    if (r->g.partidos > 0)
    {
        resumen_anio_txt_resultados(f, &r->g);
        resumen_anio_txt_ataque(f, &r->g);
        resumen_anio_txt_comparacion(f, r);
        resumen_anio_txt_forma(f, r);
        resumen_anio_txt_records(f, &r->g);
        resumen_anio_txt_meses(f, r);
        resumen_anio_txt_como_llegas(f, &r->g);
        resumen_anio_txt_rankings(f, &r->g);
        resumen_anio_txt_cuando(f, &r->g);
        resumen_anio_txt_clima(f, &r->g);
    }
    resumen_anio_txt_equipo(f, r);
    if (r->mejor_id > 0)
    {
        resumen_txt_seccion(f, "MEJOR PARTIDO DEL AÑO");
        resumen_anio_recorrer_detalle(&r->mejor, resumen_anio_txt_detalle, f);
    }

    fputc('\n', f);
    resumen_txt_repetir(f, '=', RESUMEN_TXT_ANCHO);
    fputs("\n  Generado por MiFutbolC\n", f);
    resumen_txt_repetir(f, '=', RESUMEN_TXT_ANCHO);
    fputc('\n', f);
}

/* ── Formato HTML ─────────────────────────────────────────────────────────── */

/*
 * Mismo mundo visual que el resumen general (resumen_escribir_css_comun):
 * hero verde cancha con el año, marcador y forma; paneles con la dona de
 * resultados, ataque, records, rankings, promedios, dia/franja y clima (los
 * mismos componentes, alimentados con el calculo filtrado por año) mas los
 * propios del año: comparacion con el anterior, tendencia del año, mes a mes
 * y el mejor partido.
 */

static void resumen_anio_html_hero(FILE *f, const ResumenAnio *r)
{
    fputs("<header class=\"hero\">\n", f);
    resumen_html_campo(f);
    fprintf(f, "<p class=\"antetitulo\">Resumen del a&ntilde;o &middot; %s</p>\n<h1>%d</h1>\n",
            html_texto(r->jugador), r->anio);
    if (r->g.partidos > 0)
    {
        fprintf(f, "<p class=\"bajada\">%d partido%s entre el %s y el %s</p>\n", r->g.partidos,
                r->g.partidos == 1 ? "" : "s", html_texto(r->g.primer_partido_fecha),
                html_texto(r->g.ultimo.fecha));
    }
    else
    {
        fprintf(f, "<p class=\"bajada\">Sin partidos registrados en %d</p>\n", r->anio);
    }
    resumen_html_marcador_y_forma(f, &r->g);
    fputs("</header>\n", f);
}

static void resumen_anio_html_comparacion(FILE *f, const ResumenAnio *r)
{
    ResumenAnioComparacion c[RESUMEN_ANIO_COMPARACIONES];
    int n;

    if (!r->hay_anterior)
    {
        return;
    }
    fprintf(f,
            "<section class=\"panel\">\n<h2>Contra %d</h2>\n"
            "<table class=\"comparar\">\n<thead><tr><th scope=\"col\">Dato</th>"
            "<th scope=\"col\">%d</th><th scope=\"col\">%d</th><th scope=\"col\">Cambio</th>"
            "</tr></thead>\n<tbody>\n",
            r->anterior.anio, r->anio, r->anterior.anio);
    n = resumen_anio_comparaciones(r, c);
    for (int i = 0; i < n; i++)
    {
        int signo;
        const char *sufijo;

        if (!c[i].valido)
        {
            continue;
        }
        signo = resumen_anio_signo_delta(&c[i]);
        sufijo = c[i].es_pct ? "%" : "";
        fprintf(f,
                "<tr><td>%s</td><td class=\"actual\">%s%s</td><td class=\"previo\">%s%s</td>",
                c[i].dato, rnum(c[i].actual, c[i].decimales), sufijo,
                rnum(c[i].previo, c[i].decimales), sufijo);
        if (signo == 0)
        {
            fputs("<td class=\"delta igual\">=</td></tr>\n", f);
        }
        else
        {
            fprintf(f, "<td class=\"delta %s\"><span aria-hidden=\"true\">%s</span> %s%s"
                    "<span class=\"oculto\"> %s</span></td></tr>\n",
                    signo > 0 ? "sube" : "baja", signo > 0 ? "&uarr;" : "&darr;",
                    rnum(resumen_anio_abs(c[i].actual - c[i].previo), c[i].decimales),
                    c[i].es_pct ? " pts" : "", signo > 0 ? "m&aacute;s" : "menos");
        }
    }
    fprintf(f, "</tbody>\n</table>\n<p class=\"nota\">%d tuvo %d partido%s. Efectividad en puntos "
            "porcentuales; rendimiento promedio de 1 a 10.</p>\n</section>\n",
            r->anterior.anio, r->anterior.partidos, r->anterior.partidos == 1 ? "" : "s");
}

static void resumen_anio_html_tendencia(FILE *f, const ResumenAnio *r)
{
    double prom;

    if (r->tendencia_n < 2)
    {
        return;
    }
    fprintf(f, "<section class=\"panel panel-ancho\">\n<h2>Tendencia de %d</h2>\n", r->anio);
    prom = resumen_html_spark_svg(f, r->tendencia, r->tendencia_n);
    fprintf(f,
            "<div class=\"spark-pie\"><span>Primer partido</span>"
            "<span>Promedio %s &middot; &uacute;ltimo %d/10</span><span>&Uacute;ltimo</span></div>\n"
            "<p class=\"nota\">Rendimiento general (1 a 10) de %s%d partidos del a&ntilde;o con "
            "dato, en orden; la l&iacute;nea punteada es su promedio.</p>\n</section>\n",
            rnum(prom, 1), r->tendencia[r->tendencia_n - 1],
            r->tendencia_n == RESUMEN_ANIO_TENDENCIA_MAX ? "los &uacute;ltimos " : "los ",
            r->tendencia_n);
}

/* Lista de meses con barra apilada de goles y asistencias (componente .anios). */
static void resumen_anio_html_meses_lista(FILE *f, const ResumenAnio *r)
{
    int max = 1;

    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        int ga = r->meses[i].goles + r->meses[i].asistencias;
        max = ga > max ? ga : max;
    }
    fputs("<div>\n<h3>Goles y asistencias</h3>\n<ul class=\"anios\">\n", f);
    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        const ResumenAnioMes *m = &r->meses[i];
        double wg = (double)m->goles * 100.0 / max;
        double wa = (double)m->asistencias * 100.0 / max;

        fprintf(f, "<li%s><span class=\"anio\">%s</span>",
                m->partidos == 0 ? " class=\"sin\""
                : (i == r->mejor_mes ? " class=\"destacado\"" : ""),
                resumen_anio_mes_corto(i));
        fprintf(f,
                "<svg viewBox=\"0 0 100 22\" preserveAspectRatio=\"none\" role=\"img\" "
                "aria-label=\"%s: %d goles y %d asistencias\">"
                "<rect class=\"bg\" x=\"0\" y=\"2\" width=\"%.2f\" height=\"18\" rx=\"1\"/>"
                "<rect class=\"ba\" x=\"%.2f\" y=\"2\" width=\"%.2f\" height=\"18\" rx=\"1\"/>"
                "</svg>",
                resumen_anio_mes_nombre(i), m->goles, m->asistencias, wg, wg, wa);
        if (m->partidos == 0)
        {
            fputs("<span class=\"det\">Sin partidos</span></li>\n", f);
            continue;
        }
        fprintf(f, "<span class=\"det\">%d PJ &middot; %d G &middot; %d A", m->partidos,
                m->goles, m->asistencias);
        if (m->con_rendimiento > 0)
        {
            fprintf(f, " &middot; R %s", rnum(m->rendimiento_prom, 1));
        }
        fputs(i == r->mejor_mes ? "<span class=\"marca\">Mejor mes</span></span></li>\n"
              : "</span></li>\n",
              f);
    }
    fputs("</ul>\n<div class=\"clave\"><span><i class=\"punto c-g\"></i>Goles</span>"
          "<span><i class=\"punto c-a\"></i>Asistencias</span><span>R: rendimiento promedio</span>"
          "</div>\n</div>\n",
          f);
}

/* Columnas del rendimiento promedio por mes (SVG, escala 0 a 10). */
static void resumen_anio_html_meses_columnas(FILE *f, const ResumenAnio *r)
{
    const double paso = 20.0;
    const double ancho_col = 13.0;
    char buf[160];

    fputs("<div>\n<h3>Rendimiento promedio por mes</h3>\n", f);
    if (r->mejor_mes < 0)
    {
        fputs("<p class=\"vacio\">Sin rendimientos cargados en el a&ntilde;o.</p>\n", f);
    }
    else
    {
        fputs("<svg class=\"columnas\" viewBox=\"0 0 240 100\" preserveAspectRatio=\"none\" "
              "role=\"img\" aria-label=\"Rendimiento promedio de cada mes, escala 0 a 10\">"
              "<line class=\"guia\" x1=\"0\" y1=\"50\" x2=\"240\" y2=\"50\"/>"
              "<line class=\"guia\" x1=\"0\" y1=\"99.5\" x2=\"240\" y2=\"99.5\"/>",
              f);
        for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
        {
            const ResumenAnioMes *m = &r->meses[i];
            double alto;

            if (m->con_rendimiento == 0)
            {
                continue;
            }
            alto = m->rendimiento_prom * 10.0;
            fprintf(f, "<rect class=\"c%s\" x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\">"
                    "<title>%s: %s</title></rect>",
                    i == r->mejor_mes ? " top" : "", i * paso + (paso - ancho_col) / 2.0,
                    100.0 - alto, ancho_col, alto, resumen_anio_mes_nombre(i),
                    rnum(m->rendimiento_prom, 1));
        }
        fputs("</svg>\n<div class=\"ejes\" aria-hidden=\"true\">", f);
        for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
        {
            fprintf(f, "<span%s>%.1s</span>", i == r->mejor_mes ? " class=\"top\"" : "",
                    resumen_anio_mes_corto(i));
        }
        fputs("</div>\n", f);
    }

    fputs("<ul class=\"filas\" style=\"margin-top:16px\">\n", f);
    if (r->mejor_mes >= 0)
    {
        const ResumenAnioMes *m = &r->meses[r->mejor_mes];
        snprintf(buf, sizeof(buf), "%s<small>Rendimiento %s en %d partido%s</small>",
                 resumen_anio_mes_nombre(r->mejor_mes), rnum(m->rendimiento_prom, 1),
                 m->con_rendimiento, m->con_rendimiento == 1 ? "" : "s");
        fprintf(f, "<li><span class=\"k\">Mejor mes</span><span class=\"v\">%s</span></li>\n", buf);
    }
    if (r->mes_goleador >= 0)
    {
        const ResumenAnioMes *m = &r->meses[r->mes_goleador];
        fprintf(f, "<li><span class=\"k\">Mes m&aacute;s goleador</span><span class=\"v\">%s"
                "<small>%d gol%s y %d asistencia%s</small></span></li>\n",
                resumen_anio_mes_nombre(r->mes_goleador), m->goles, m->goles == 1 ? "" : "es",
                m->asistencias, m->asistencias == 1 ? "" : "s");
    }
    fputs("</ul>\n</div>\n", f);
}

static void resumen_anio_html_meses(FILE *f, const ResumenAnio *r)
{
    if (r->g.partidos == 0)
    {
        return;
    }
    fputs("<section class=\"panel panel-ancho\">\n<h2>Mes a mes</h2>\n<div class=\"dos\">\n", f);
    resumen_anio_html_meses_lista(f, r);
    resumen_anio_html_meses_columnas(f, r);
    fputs("</div>\n</section>\n", f);
}

static void resumen_anio_html_equipo(FILE *f, const ResumenAnio *r)
{
    char buf[32];

    if (r->partidos_con_marcador == 0)
    {
        return;
    }
    fputs("<section class=\"panel\">\n<h2>Marcador del equipo</h2>\n<div class=\"cifras\">\n", f);
    snprintf(buf, sizeof(buf), "%d", r->goles_favor);
    resumen_html_cifra(f, buf, "Goles a favor");
    snprintf(buf, sizeof(buf), "%d", r->goles_contra);
    resumen_html_cifra(f, buf, "Goles en contra");
    snprintf(buf, sizeof(buf), "%+d", r->goles_favor - r->goles_contra);
    resumen_html_cifra(f, buf, "Diferencia");
    fprintf(f, "</div>\n<p class=\"nota\">Sobre %d partido%s con el marcador cargado.</p>\n"
            "</section>\n",
            r->partidos_con_marcador, r->partidos_con_marcador == 1 ? "" : "s");
}

static void resumen_anio_html_grupo(FILE *f, const char *k, const char *k_top,
                                    const ResumenAnioGrupo *g)
{
    fprintf(f, "<li><span class=\"k\">%s</span><span class=\"v\">%d<small>de %d activa%s</small>"
            "</span></li>\n",
            k, g->usadas, g->activas, g->activas == 1 ? "" : "s");
    if (g->top_partidos > 0)
    {
        fprintf(f, "<li><span class=\"k\">%s</span><span class=\"v\">%s<small>%d partido%s"
                "</small></span></li>\n",
                k_top, html_texto(g->top), g->top_partidos, g->top_partidos == 1 ? "" : "s");
    }
}

/* Paneles de media columna de la grilla (la grilla es densa: un panel de
 * media columna posterior rellena un hueco anterior). Si quedan impares el
 * equipamiento ocupa todo el ancho para no dejar un hueco. */
static int resumen_anio_html_mitades(const ResumenAnio *r)
{
    const ResumenGeneral *g = &r->g;

    return (g->con_resultado > 0) + 2 * (g->partidos > 0) + r->hay_anterior +
           (g->con_rendimiento + g->con_animo + g->con_cansancio > 0) +
           (g->clima_real_partidos > 0) + (r->partidos_con_marcador > 0) + 1;
}

static void resumen_anio_html_equipamiento(FILE *f, const ResumenAnio *r, int ancho)
{
    fputs(ancho ? "<section class=\"panel panel-ancho\">\n<h2>Equipamiento y salud</h2>\n"
          "<div class=\"dos\">\n<ul class=\"filas\">\n"
          : "<section class=\"panel\">\n<h2>Equipamiento y salud</h2>\n<ul class=\"filas\">\n",
          f);
    resumen_anio_html_grupo(f, "Camisetas usadas", "Camiseta m&aacute;s usada", &r->camiseta);
    if (ancho)
    {
        fputs("</ul>\n<ul class=\"filas\">\n", f);
    }
    resumen_anio_html_grupo(f, "Canchas usadas", "Cancha m&aacute;s usada", &r->cancha);
    if (r->lesiones == 0)
    {
        fputs("<li><span class=\"k\">Lesiones del a&ntilde;o</span><span class=\"v\">Ninguna"
              "</span></li>\n",
              f);
    }
    else
    {
        fprintf(f, "<li><span class=\"k\">Lesiones del a&ntilde;o</span><span class=\"v\">%d"
                "<small>%d activa%s</small></span></li>\n",
                r->lesiones, r->lesiones_activas, r->lesiones_activas == 1 ? "" : "s");
    }
    fputs(ancho ? "</ul>\n</div>\n</section>\n" : "</ul>\n</section>\n", f);
}

/* Fila de la ficha del mejor partido; escapa sin el limite de html_texto. */
static void resumen_anio_html_detalle(void *destino, const char *etiqueta, const char *valor)
{
    char k[128];
    char v[4096];

    if (!valor || valor[0] == '\0')
    {
        return;
    }
    html_escape_a(k, sizeof(k), etiqueta);
    html_escape_a(v, sizeof(v), valor);
    fprintf((FILE *)destino, "<li><span class=\"k\">%s</span><span class=\"v\">%s</span></li>\n",
            k, v);
}

static void resumen_anio_html_mejor(FILE *f, const ResumenAnio *r)
{
    const ResumenAnioPartido *p = &r->mejor;
    char buf[32];

    if (r->mejor_id <= 0)
    {
        return;
    }
    fprintf(f, "<section class=\"panel panel-ancho\">\n<h2>Partido del a&ntilde;o%s%s</h2>\n"
            "<div class=\"cifras\">\n",
            p->rival[0] ? ": vs " : "", html_texto(p->rival));
    if (p->rendimiento_general > 0)
    {
        snprintf(buf, sizeof(buf), "%d/10", p->rendimiento_general);
        resumen_html_cifra(f, buf, "Rendimiento");
    }
    snprintf(buf, sizeof(buf), "%d", p->goles);
    resumen_html_cifra(f, buf, "Goles");
    snprintf(buf, sizeof(buf), "%d", p->asistencias);
    resumen_html_cifra(f, buf, "Asistencias");
    if (p->goles_equipo >= 0 && p->goles_rival >= 0)
    {
        snprintf(buf, sizeof(buf), "%d-%d", p->goles_equipo, p->goles_rival);
        resumen_html_cifra(f, buf, "Marcador");
    }
    fputs("</div>\n<ul class=\"ficha\">\n", f);
    resumen_anio_recorrer_detalle(p, resumen_anio_html_detalle, f);
    fputs(r->g.con_rendimiento > 0
          ? "</ul>\n<p class=\"nota\">El de mejor rendimiento del a&ntilde;o (con empate, el "
          "m&aacute;s reciente).</p>\n</section>\n"
          : "</ul>\n<p class=\"nota\">No hay rendimientos cargados: es el &uacute;ltimo partido "
          "del a&ntilde;o.</p>\n</section>\n",
          f);
}

void resumen_anio_escribir_html(FILE *f, const ResumenAnio *r)
{
    if (!f || !r)
    {
        return;
    }
    fprintf(f,
            "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "<title>%d &middot; Resumen del a&ntilde;o de %s</title>\n"
            "<meta property=\"og:title\" content=\"Resumen %d de %s - MiFutbolC\">\n"
            "<meta property=\"og:description\" content=\"%d partidos, %d goles, %d "
            "asistencias\">\n<style>\n",
            r->anio, html_texto(r->jugador), r->anio, html_texto(r->jugador), r->g.partidos,
            r->g.goles, r->g.asistencias);
    resumen_escribir_css_comun(f);
    fputs(".oculto { position: absolute; width: 1px; height: 1px; overflow: hidden;\n"
          "          clip: rect(0 0 0 0); white-space: nowrap; }\n"
          "</style>\n</head>\n<body>\n<main class=\"pagina\">\n",
          f);

    resumen_anio_html_hero(f, r);
    fputs("<div class=\"grilla grilla-densa\">\n", f);
    if (r->g.partidos == 0)
    {
        fprintf(f, "<section class=\"panel panel-ancho\">\n<h2>Sin partidos en %d</h2>\n"
                "<p class=\"vacio\">No hay partidos cargados con fecha de este a&ntilde;o. "
                "Abajo quedan el equipamiento y las lesiones del a&ntilde;o.</p>\n</section>\n",
                r->anio);
    }
    resumen_html_resultados(f, &r->g);
    resumen_html_ataque(f, &r->g);
    resumen_anio_html_comparacion(f, r);
    resumen_html_records(f, &r->g);
    resumen_anio_html_tendencia(f, r);
    resumen_anio_html_meses(f, r);
    resumen_html_rankings(f, &r->g);
    resumen_html_como_llegas(f, &r->g);
    resumen_html_clima(f, &r->g);
    resumen_anio_html_equipo(f, r);
    resumen_anio_html_equipamiento(f, r, resumen_anio_html_mitades(r) % 2 == 1);
    resumen_html_cuando(f, &r->g);
    resumen_anio_html_mejor(f, r);
    fputs("</div>\n", f);

    fprintf(f, "<footer class=\"pie\">Generado por MiFutbolC el %s</footer>\n"
            "</main>\n</body>\n</html>\n",
            html_texto(r->fecha_gen));
}

/* ── Formato PDF ──────────────────────────────────────────────────────────── */

/*
 * A4 con el mismo mundo visual: banda verde cancha arriba con el año, el
 * marcador y la forma; secciones con titulo verde y filete, cifras grandes,
 * barras de colores (V/E/D, goles/asistencias) y la tendencia dibujada con
 * lineas. Cada bloque reserva su alto antes de dibujarse y, si no entra,
 * pasa a una pagina nueva con su pie numerado.
 *
 * pdfgen convierte UTF-8 a WinAnsi (acentos, enie y punto medio entran).
 * Si un texto trae algo que WinAnsi no cubre se reintenta en ASCII basico.
 */

#define RESUMEN_PDF_MARGEN 40.0f
#define RESUMEN_PDF_PIE 46.0f

#define PDF_C_VERDE PDF_RGB(19, 92, 58)
#define PDF_C_VERDE_LINEA PDF_RGB(52, 122, 88)
#define PDF_C_VERDE_SUAVE PDF_RGB(226, 239, 230)
#define PDF_C_TIZA PDF_RGB(243, 247, 242)
#define PDF_C_TIZA_2 PDF_RGB(190, 215, 200)
#define PDF_C_TINTA PDF_RGB(23, 32, 27)
#define PDF_C_TINTA_2 PDF_RGB(72, 84, 77)
#define PDF_C_TINTA_3 PDF_RGB(102, 113, 106)
#define PDF_C_LINEA PDF_RGB(220, 213, 198)
#define PDF_C_LINEA_2 PDF_RGB(235, 229, 217)
#define PDF_C_ACENTO PDF_RGB(19, 104, 63)
#define PDF_C_WIN PDF_RGB(29, 127, 71)
#define PDF_C_DRAW PDF_RGB(176, 114, 20)
#define PDF_C_LOSS PDF_RGB(184, 52, 42)
#define PDF_C_BAR_2 PDF_RGB(192, 138, 44)

typedef struct
{
    struct pdf_doc *pdf;
    struct pdf_object *page;
    float y;
    float margin;
    float ancho; /**< Ancho util entre margenes */
    int warned;
    int pagina;
    int anio;
} ResumenAnioPdfCtx;

static void resumen_pdf_texto(ResumenAnioPdfCtx *ctx, float x, float y, const char *texto,
                              float tam, const char *fuente, uint32_t color)
{
    char ascii[512];

    if (!ctx->page || !texto || texto[0] == '\0')
    {
        return;
    }
    pdf_set_font(ctx->pdf, fuente);
    if (pdf_add_text(ctx->pdf, ctx->page, texto, tam, x, y, color) >= 0)
    {
        return;
    }
    sanitizar_ascii_basico(texto, ascii, sizeof(ascii));
    if (pdf_add_text(ctx->pdf, ctx->page, ascii, tam, x, y, color) < 0 && !ctx->warned)
    {
        int errval = 0;
        const char *err = pdf_get_err(ctx->pdf, &errval);
        printf("Advertencia PDF: no se pudo escribir texto (%d): %s\n", errval,
               err ? err : "error desconocido");
        ctx->warned = 1;
    }
}

static float resumen_pdf_ancho_texto(ResumenAnioPdfCtx *ctx, const char *texto, float tam,
                                     const char *fuente)
{
    float ancho = 0.0f;

    if (pdf_get_font_text_width(ctx->pdf, fuente, texto, tam, &ancho) < 0)
    {
        /* Aproximacion si el texto no se puede medir (caracter fuera de WinAnsi). */
        return (float)resumen_txt_largo(texto) * tam * 0.5f;
    }
    return ancho;
}

static void resumen_pdf_texto_der(ResumenAnioPdfCtx *ctx, float x_der, float y,
                                  const char *texto, float tam, const char *fuente,
                                  uint32_t color)
{
    resumen_pdf_texto(ctx, x_der - resumen_pdf_ancho_texto(ctx, texto, tam, fuente), y, texto,
                      tam, fuente, color);
}

static void resumen_pdf_texto_centro(ResumenAnioPdfCtx *ctx, float x_centro, float y,
                                     const char *texto, float tam, const char *fuente,
                                     uint32_t color)
{
    resumen_pdf_texto(ctx, x_centro - resumen_pdf_ancho_texto(ctx, texto, tam, fuente) / 2.0f, y,
                      texto, tam, fuente, color);
}

/* Copia el texto recortado con "..." para que entre en @p ancho puntos. */
static void resumen_pdf_recortar(ResumenAnioPdfCtx *ctx, const char *texto, float tam,
                                 const char *fuente, float ancho, char *out, size_t tam_out)
{
    size_t largo;

    snprintf(out, tam_out, "%s", texto);
    if (resumen_pdf_ancho_texto(ctx, out, tam, fuente) <= ancho)
    {
        return;
    }
    largo = strlen(out);
    while (largo > 1)
    {
        largo--;
        /* No cortar en medio de un caracter UTF-8. */
        while (largo > 0 && ((unsigned char)out[largo] & 0xC0) == 0x80)
        {
            largo--;
        }
        if (largo + 4 <= tam_out)
        {
            memcpy(out + largo, "...", 4);
            if (resumen_pdf_ancho_texto(ctx, out, tam, fuente) <= ancho)
            {
                return;
            }
        }
        out[largo] = '\0';
    }
}

static void resumen_pdf_rect(ResumenAnioPdfCtx *ctx, float x, float y, float w, float h,
                             uint32_t color)
{
    if (w > 0.0f && h > 0.0f)
    {
        pdf_add_filled_rectangle(ctx->pdf, ctx->page, x, y, w, h, 0.0f, color, color);
    }
}

static void resumen_pdf_linea(ResumenAnioPdfCtx *ctx, float x1, float y1, float x2, float y2,
                              float grosor, uint32_t color)
{
    pdf_add_line(ctx->pdf, ctx->page, x1, y1, x2, y2, grosor, color);
}

/* Pie de la pagina actual: titulo y numero. */
static void resumen_pdf_pie(ResumenAnioPdfCtx *ctx)
{
    char texto[96];

    resumen_pdf_linea(ctx, ctx->margin, 34.0f, ctx->margin + ctx->ancho, 34.0f, 0.5f,
                      PDF_C_LINEA);
    snprintf(texto, sizeof(texto), "MiFutbolC · Resumen del año %d", ctx->anio);
    resumen_pdf_texto(ctx, ctx->margin, 22.0f, texto, 8.0f, "Helvetica", PDF_C_TINTA_3);
    snprintf(texto, sizeof(texto), "Página %d", ctx->pagina);
    resumen_pdf_texto_der(ctx, ctx->margin + ctx->ancho, 22.0f, texto, 8.0f, "Helvetica",
                          PDF_C_TINTA_3);
}

static int resumen_pdf_nueva_pagina(ResumenAnioPdfCtx *ctx)
{
    struct pdf_object *page = pdf_append_page(ctx->pdf);

    if (!page)
    {
        ctx->page = NULL;
        return 0;
    }
    pdf_page_set_size(ctx->pdf, page, PDF_A4_WIDTH, PDF_A4_HEIGHT);
    ctx->page = page;
    ctx->pagina++;
    ctx->y = pdf_page_height(page) - ctx->margin;
    resumen_pdf_pie(ctx);
    return 1;
}

/* Pasa de pagina si no entran @p alto puntos sobre el pie. */
static void resumen_pdf_reservar(ResumenAnioPdfCtx *ctx, float alto)
{
    if (ctx->y - alto < RESUMEN_PDF_PIE)
    {
        resumen_pdf_nueva_pagina(ctx);
    }
}

/* Titulo de seccion; reserva tambien el primer tramo del contenido para que
 * el titulo no quede solo al pie de una pagina. */
static void resumen_pdf_titulo(ResumenAnioPdfCtx *ctx, const char *titulo, float alto_contenido)
{
    resumen_pdf_reservar(ctx, 30.0f + alto_contenido);
    ctx->y -= 12.0f;
    resumen_pdf_texto(ctx, ctx->margin, ctx->y, titulo, 12.5f, "Helvetica-Bold", PDF_C_ACENTO);
    ctx->y -= 6.0f;
    resumen_pdf_linea(ctx, ctx->margin, ctx->y, ctx->margin + ctx->ancho, ctx->y, 0.8f,
                      PDF_C_ACENTO);
    ctx->y -= 16.0f;
}

/* Fila clave/valor en una columna (la linea base es ctx->y). */
static void resumen_pdf_fila(ResumenAnioPdfCtx *ctx, float x, float ancho, float y, const char *k,
                             const char *v)
{
    char recorte[192];
    float ancho_k = resumen_pdf_ancho_texto(ctx, k, 9.0f, "Helvetica");

    resumen_pdf_texto(ctx, x, y, k, 9.0f, "Helvetica", PDF_C_TINTA_2);
    resumen_pdf_recortar(ctx, v, 9.0f, "Helvetica-Bold", ancho - ancho_k - 10.0f, recorte,
                         sizeof(recorte));
    resumen_pdf_texto_der(ctx, x + ancho, y, recorte, 9.0f, "Helvetica-Bold", PDF_C_TINTA);
    resumen_pdf_linea(ctx, x, y - 5.0f, x + ancho, y - 5.0f, 0.4f, PDF_C_LINEA_2);
}

/* Pares clave/valor en dos columnas. */
static void resumen_pdf_filas(ResumenAnioPdfCtx *ctx, const char *const *k, char v[][192], int n)
{
    const float alto = 17.0f;
    const float sep = 24.0f;
    float col = (ctx->ancho - sep) / 2.0f;

    for (int i = 0; i < n; i += 2)
    {
        resumen_pdf_reservar(ctx, alto);
        resumen_pdf_fila(ctx, ctx->margin, col, ctx->y, k[i], v[i]);
        if (i + 1 < n)
        {
            resumen_pdf_fila(ctx, ctx->margin + col + sep, col, ctx->y, k[i + 1], v[i + 1]);
        }
        ctx->y -= alto;
    }
    ctx->y -= 4.0f;
}

/* Fila de cifras grandes (hasta 4 por renglon). */
static void resumen_pdf_cifras(ResumenAnioPdfCtx *ctx, char v[][32], const char *const *k, int n)
{
    float col = ctx->ancho / 4.0f;

    for (int i = 0; i < n; i += 4)
    {
        resumen_pdf_reservar(ctx, 40.0f);
        for (int j = i; j < n && j < i + 4; j++)
        {
            float x = ctx->margin + col * (float)(j - i);
            resumen_pdf_texto(ctx, x, ctx->y - 14.0f, v[j], 20.0f, "Helvetica-Bold", PDF_C_TINTA);
            resumen_pdf_texto(ctx, x, ctx->y - 27.0f, k[j], 8.0f, "Helvetica", PDF_C_TINTA_2);
        }
        ctx->y -= 40.0f;
    }
}

/* Medidor horizontal: fondo y parte llena. */
static void resumen_pdf_medidor(ResumenAnioPdfCtx *ctx, float x, float y, float ancho, float alto,
                                double frac, uint32_t color)
{
    if (frac < 0.0)
    {
        frac = 0.0;
    }
    if (frac > 1.0)
    {
        frac = 1.0;
    }
    resumen_pdf_rect(ctx, x, y, ancho, alto, PDF_C_LINEA_2);
    resumen_pdf_rect(ctx, x, y, ancho * (float)frac, alto, color);
}

static uint32_t resumen_pdf_color_chip(int resultado)
{
    switch (resultado)
    {
    case 1:
        return PDF_RGB(143, 224, 176);
    case 2:
        return PDF_RGB(240, 201, 106);
    case 3:
        return PDF_RGB(244, 154, 143);
    default:
        return PDF_C_VERDE_LINEA;
    }
}

static void resumen_pdf_hero(ResumenAnioPdfCtx *ctx, const ResumenAnio *r)
{
    const ResumenGeneral *g = &r->g;
    float alto_pag = pdf_page_height(ctx->page);
    float ancho_pag = pdf_page_width(ctx->page);
    float alto = 182.0f;
    float base = alto_pag - alto;
    float col = ctx->ancho / 4.0f;
    char texto[192];
    char valores[4][32];
    const char *etiquetas[4] = {"PARTIDOS", "GOLES", "ASISTENCIAS", "EFECTIVIDAD"};

    resumen_pdf_rect(ctx, 0.0f, base, ancho_pag, alto, PDF_C_VERDE);
    /* Lineas del campo, como en el hero del HTML. */
    pdf_add_rectangle(ctx->pdf, ctx->page, 14.0f, base + 12.0f, ancho_pag - 28.0f, alto - 24.0f,
                      1.0f, PDF_C_VERDE_LINEA);
    resumen_pdf_linea(ctx, ancho_pag / 2.0f, base + 12.0f, ancho_pag / 2.0f, alto_pag - 12.0f,
                      1.0f, PDF_C_VERDE_LINEA);
    pdf_add_circle(ctx->pdf, ctx->page, ancho_pag / 2.0f, base + alto / 2.0f, 40.0f, 1.0f,
                   PDF_C_VERDE_LINEA, PDF_C_VERDE);

    snprintf(texto, sizeof(texto), "RESUMEN DEL AÑO · %s", r->jugador);
    resumen_pdf_texto(ctx, ctx->margin, alto_pag - 38.0f, texto, 9.5f, "Helvetica-Bold",
                      PDF_C_TIZA_2);
    snprintf(texto, sizeof(texto), "%d", r->anio);
    resumen_pdf_texto(ctx, ctx->margin, alto_pag - 84.0f, texto, 46.0f, "Helvetica-Bold",
                      PDF_C_TIZA);
    if (g->partidos > 0)
    {
        snprintf(texto, sizeof(texto), "%d partido%s entre el %s y el %s · generado el %s",
                 g->partidos, g->partidos == 1 ? "" : "s", g->primer_partido_fecha,
                 g->ultimo.fecha, r->fecha_gen);
    }
    else
    {
        snprintf(texto, sizeof(texto), "Sin partidos registrados en %d · generado el %s",
                 r->anio, r->fecha_gen);
    }
    resumen_pdf_texto(ctx, ctx->margin, alto_pag - 102.0f, texto, 9.0f, "Helvetica",
                      PDF_C_TIZA_2);

    /* Forma: chips de los ultimos partidos, a la derecha del año. */
    if (g->ultimos_n > 0)
    {
        float x = ctx->margin + ctx->ancho - (float)g->ultimos_n * 19.0f + 3.0f;
        resumen_pdf_texto_der(ctx, x - 6.0f, alto_pag - 77.0f, "ÚLTIMOS", 7.5f,
                              "Helvetica-Bold", PDF_C_TIZA_2);
        for (int i = 0; i < g->ultimos_n; i++)
        {
            char letra[2] = {resumen_letra_resultado(g->ultimos[i].resultado), '\0'};
            float cx = x + (float)i * 19.0f;
            resumen_pdf_rect(ctx, cx, alto_pag - 84.0f, 16.0f, 16.0f,
                             resumen_pdf_color_chip(g->ultimos[i].resultado));
            resumen_pdf_texto_centro(ctx, cx + 8.0f, alto_pag - 79.5f, letra, 9.0f,
                                     "Helvetica-Bold",
                                     g->ultimos[i].resultado ? PDF_C_TINTA : PDF_C_TIZA);
        }
    }

    /* Marcador de cuatro numeros con separadores. */
    snprintf(valores[0], sizeof(valores[0]), "%d", g->partidos);
    snprintf(valores[1], sizeof(valores[1]), "%d", g->goles);
    snprintf(valores[2], sizeof(valores[2]), "%d", g->asistencias);
    if (g->con_resultado > 0)
    {
        snprintf(valores[3], sizeof(valores[3]), "%s%%", rnum(g->efectividad_pct, 0));
    }
    else
    {
        snprintf(valores[3], sizeof(valores[3]), "-");
    }
    resumen_pdf_linea(ctx, ctx->margin, base + 62.0f, ctx->margin + ctx->ancho, base + 62.0f,
                      0.6f, PDF_C_VERDE_LINEA);
    for (int i = 0; i < 4; i++)
    {
        float x = ctx->margin + col * (float)i + (i > 0 ? 12.0f : 0.0f);
        if (i > 0)
        {
            resumen_pdf_linea(ctx, x - 12.0f, base + 22.0f, x - 12.0f, base + 58.0f, 0.6f,
                              PDF_C_VERDE_LINEA);
        }
        resumen_pdf_texto(ctx, x, base + 34.0f, valores[i], 24.0f, "Helvetica-Bold", PDF_C_TIZA);
        resumen_pdf_texto(ctx, x, base + 22.0f, etiquetas[i], 7.5f, "Helvetica-Bold",
                          PDF_C_TIZA_2);
    }

    ctx->y = base - 14.0f;
}

static void resumen_pdf_resultados(ResumenAnioPdfCtx *ctx, const ResumenGeneral *g)
{
    char texto[192];
    float x = ctx->margin;
    float w = ctx->ancho;
    double pv = resumen_pct(g->victorias, g->con_resultado);
    double pe = resumen_pct(g->empates, g->con_resultado);
    double pd = resumen_pct(g->derrotas, g->con_resultado);

    if (g->con_resultado == 0)
    {
        return;
    }
    resumen_pdf_titulo(ctx, "Resultados", 58.0f);
    resumen_pdf_rect(ctx, x, ctx->y - 12.0f, w * (float)(pv / 100.0), 14.0f, PDF_C_WIN);
    resumen_pdf_rect(ctx, x + w * (float)(pv / 100.0), ctx->y - 12.0f, w * (float)(pe / 100.0),
                     14.0f, PDF_C_DRAW);
    resumen_pdf_rect(ctx, x + w * (float)((pv + pe) / 100.0), ctx->y - 12.0f,
                     w * (float)(pd / 100.0), 14.0f, PDF_C_LOSS);
    ctx->y -= 28.0f;

    snprintf(texto, sizeof(texto), "Victorias %d (%s%%)", g->victorias, rnum(pv, 0));
    resumen_pdf_rect(ctx, x, ctx->y - 1.0f, 8.0f, 8.0f, PDF_C_WIN);
    resumen_pdf_texto(ctx, x + 12.0f, ctx->y, texto, 9.0f, "Helvetica", PDF_C_TINTA);
    snprintf(texto, sizeof(texto), "Empates %d (%s%%)", g->empates, rnum(pe, 0));
    resumen_pdf_rect(ctx, x + 130.0f, ctx->y - 1.0f, 8.0f, 8.0f, PDF_C_DRAW);
    resumen_pdf_texto(ctx, x + 142.0f, ctx->y, texto, 9.0f, "Helvetica", PDF_C_TINTA);
    snprintf(texto, sizeof(texto), "Derrotas %d (%s%%)", g->derrotas, rnum(pd, 0));
    resumen_pdf_rect(ctx, x + 260.0f, ctx->y - 1.0f, 8.0f, 8.0f, PDF_C_LOSS);
    resumen_pdf_texto(ctx, x + 272.0f, ctx->y, texto, 9.0f, "Helvetica", PDF_C_TINTA);
    ctx->y -= 15.0f;
    snprintf(texto, sizeof(texto),
             "Efectividad %s%% de los puntos en juego (3 por victoria, 1 por empate) sobre %d "
             "partido%s con resultado.",
             rnum(g->efectividad_pct, 1), g->con_resultado, g->con_resultado == 1 ? "" : "s");
    resumen_pdf_texto(ctx, x, ctx->y, texto, 8.5f, "Helvetica", PDF_C_TINTA_2);
    ctx->y -= 18.0f;
}

static void resumen_pdf_ataque(ResumenAnioPdfCtx *ctx, const ResumenGeneral *g)
{
    char v[8][32];
    const char *k[8] = {"Goles + asistencias", "G+A por partido", "Goles por partido",
                        "Asistencias por partido", "Partidos con gol", "Dobletes",
                        "Hat-tricks o más", "G+A cada 90 min"
                       };
    int n = 7;

    if (g->partidos == 0)
    {
        return;
    }
    snprintf(v[0], sizeof(v[0]), "%d", g->goles + g->asistencias);
    snprintf(v[1], sizeof(v[1]), "%s", rnum(g->ga_pp, 2));
    snprintf(v[2], sizeof(v[2]), "%s", rnum(g->goles_pp, 2));
    snprintf(v[3], sizeof(v[3]), "%s", rnum(g->asistencias_pp, 2));
    snprintf(v[4], sizeof(v[4]), "%d", g->partidos_con_gol);
    snprintf(v[5], sizeof(v[5]), "%d", g->dobletes);
    snprintf(v[6], sizeof(v[6]), "%d", g->hat_tricks);
    if (g->partidos_con_minutos > 0)
    {
        snprintf(v[7], sizeof(v[7]), "%s", rnum(g->ga_por_90, 2));
        n = 8;
    }
    resumen_pdf_titulo(ctx, "Ataque", 40.0f);
    resumen_pdf_cifras(ctx, v, k, n);
}

/* Triangulo de subida (verde) o bajada (rojo) con la base en @p y. */
static void resumen_pdf_flecha(ResumenAnioPdfCtx *ctx, float x, float y, int sube)
{
    float px[3];
    float py[3];

    px[0] = x;
    px[1] = x + 7.0f;
    px[2] = x + 3.5f;
    py[0] = sube ? y : y + 6.0f;
    py[1] = py[0];
    py[2] = sube ? y + 6.0f : y;
    pdf_add_filled_polygon(ctx->pdf, ctx->page, px, py, 3, 0.0f, sube ? PDF_C_WIN : PDF_C_LOSS);
}

static void resumen_pdf_comparacion(ResumenAnioPdfCtx *ctx, const ResumenAnio *r)
{
    ResumenAnioComparacion c[RESUMEN_ANIO_COMPARACIONES];
    char texto[64];
    float x = ctx->margin;
    float c1 = x + 250.0f;
    float c2 = x + 340.0f;
    float c3 = x + ctx->ancho;
    int n;

    if (!r->hay_anterior)
    {
        return;
    }
    snprintf(texto, sizeof(texto), "Comparación con %d", r->anterior.anio);
    resumen_pdf_titulo(ctx, texto, 40.0f);
    snprintf(texto, sizeof(texto), "%d", r->anio);
    resumen_pdf_texto_der(ctx, c1, ctx->y, texto, 8.0f, "Helvetica-Bold", PDF_C_TINTA_3);
    snprintf(texto, sizeof(texto), "%d", r->anterior.anio);
    resumen_pdf_texto_der(ctx, c2, ctx->y, texto, 8.0f, "Helvetica-Bold", PDF_C_TINTA_3);
    resumen_pdf_texto_der(ctx, c3, ctx->y, "CAMBIO", 8.0f, "Helvetica-Bold", PDF_C_TINTA_3);
    ctx->y -= 16.0f;

    n = resumen_anio_comparaciones(r, c);
    for (int i = 0; i < n; i++)
    {
        const char *sufijo = c[i].es_pct ? "%" : "";
        int signo;

        if (!c[i].valido)
        {
            continue;
        }
        resumen_pdf_reservar(ctx, 16.0f);
        signo = resumen_anio_signo_delta(&c[i]);
        resumen_pdf_texto(ctx, x, ctx->y, c[i].dato, 9.5f, "Helvetica", PDF_C_TINTA_2);
        snprintf(texto, sizeof(texto), "%s%s", rnum(c[i].actual, c[i].decimales), sufijo);
        resumen_pdf_texto_der(ctx, c1, ctx->y, texto, 9.5f, "Helvetica-Bold", PDF_C_TINTA);
        snprintf(texto, sizeof(texto), "%s%s", rnum(c[i].previo, c[i].decimales), sufijo);
        resumen_pdf_texto_der(ctx, c2, ctx->y, texto, 9.5f, "Helvetica", PDF_C_TINTA_3);
        if (signo == 0)
        {
            resumen_pdf_texto_der(ctx, c3, ctx->y, "igual", 9.5f, "Helvetica", PDF_C_TINTA_3);
        }
        else
        {
            float w;
            snprintf(texto, sizeof(texto), "%s%s",
                     rnum(resumen_anio_abs(c[i].actual - c[i].previo), c[i].decimales),
                     c[i].es_pct ? " pts" : "");
            w = resumen_pdf_ancho_texto(ctx, texto, 9.5f, "Helvetica-Bold");
            resumen_pdf_texto_der(ctx, c3, ctx->y, texto, 9.5f, "Helvetica-Bold",
                                  signo > 0 ? PDF_C_WIN : PDF_C_LOSS);
            resumen_pdf_flecha(ctx, c3 - w - 11.0f, ctx->y, signo > 0);
        }
        resumen_pdf_linea(ctx, x, ctx->y - 5.0f, c3, ctx->y - 5.0f, 0.4f, PDF_C_LINEA_2);
        ctx->y -= 16.0f;
    }
    ctx->y -= 4.0f;
}

static void resumen_pdf_tendencia(ResumenAnioPdfCtx *ctx, const ResumenAnio *r)
{
    float px[RESUMEN_ANIO_TENDENCIA_MAX + 2];
    float py[RESUMEN_ANIO_TENDENCIA_MAX + 2];
    const float alto = 84.0f;
    float x0 = ctx->margin;
    float base;
    float paso;
    double suma = 0.0;
    float y_prom;
    char texto[160];
    int n = r->tendencia_n;

    if (n < 2)
    {
        return;
    }
    snprintf(texto, sizeof(texto), "Tendencia de %d", r->anio);
    resumen_pdf_titulo(ctx, texto, alto + 18.0f);
    base = ctx->y - alto;
    paso = (ctx->ancho - 12.0f) / (float)(n - 1);

    resumen_pdf_linea(ctx, x0, base, x0 + ctx->ancho, base, 0.5f, PDF_C_LINEA);
    resumen_pdf_linea(ctx, x0, base + alto, x0 + ctx->ancho, base + alto, 0.5f, PDF_C_LINEA_2);
    for (int i = 0; i < n; i++)
    {
        px[i] = x0 + 6.0f + paso * (float)i;
        py[i] = base + 6.0f + (float)(r->tendencia[i] - 1) / 9.0f * (alto - 12.0f);
        suma += r->tendencia[i];
    }
    /* Area bajo la curva: los puntos mas las dos esquinas de la base. */
    px[n] = px[n - 1];
    py[n] = base;
    px[n + 1] = px[0];
    py[n + 1] = base;
    pdf_add_filled_polygon(ctx->pdf, ctx->page, px, py, n + 2, 0.0f, PDF_C_VERDE_SUAVE);

    y_prom = base + 6.0f + (float)(suma / n - 1.0) / 9.0f * (alto - 12.0f);
    for (float x = x0; x < x0 + ctx->ancho; x += 8.0f)
    {
        resumen_pdf_linea(ctx, x, y_prom, x + 4.0f, y_prom, 0.6f, PDF_C_TINTA_3);
    }
    for (int i = 1; i < n; i++)
    {
        resumen_pdf_linea(ctx, px[i - 1], py[i - 1], px[i], py[i], 1.6f, PDF_C_ACENTO);
    }
    pdf_add_circle(ctx->pdf, ctx->page, px[n - 1], py[n - 1], 3.0f, 0.0f, PDF_C_ACENTO,
                   PDF_C_ACENTO);
    resumen_pdf_texto(ctx, x0 + ctx->ancho + 4.0f, base + alto - 3.0f, "10", 7.0f, "Helvetica",
                      PDF_C_TINTA_3);

    ctx->y = base - 12.0f;
    snprintf(texto, sizeof(texto),
             "Rendimiento (1 a 10) de %d partidos con dato · promedio %s (línea punteada) · "
             "último %d/10",
             n, rnum(suma / n, 1), r->tendencia[n - 1]);
    resumen_pdf_texto(ctx, x0, ctx->y, texto, 8.0f, "Helvetica", PDF_C_TINTA_3);
    ctx->y -= 16.0f;
}

static void resumen_pdf_partido_corto(char *v, size_t tam, const char *valor,
                                      const ResumenPartidoCorto *p)
{
    snprintf(v, tam, "%s · %s%s%s", valor, p->fecha, p->rival[0] ? " vs " : "", p->rival);
}

static void resumen_pdf_records(ResumenAnioPdfCtx *ctx, const ResumenGeneral *g)
{
    const char *k[6];
    char v[6][192];
    char buf[48];
    int n = 0;

    if (g->partidos == 0)
    {
        return;
    }
    if (g->con_resultado > 0)
    {
        k[n] = "Mejor racha de victorias";
        snprintf(v[n++], sizeof(v[0]), "%d partido%s", g->mejor_racha_victorias,
                 g->mejor_racha_victorias == 1 ? "" : "s");
        k[n] = "Mejor racha invicto";
        snprintf(v[n++], sizeof(v[0]), "%d partido%s", g->mejor_racha_invicto,
                 g->mejor_racha_invicto == 1 ? "" : "s");
    }
    k[n] = "Racha más larga marcando";
    snprintf(v[n++], sizeof(v[0]), "%d partido%s", g->mejor_racha_marcando,
             g->mejor_racha_marcando == 1 ? "" : "s");
    if (g->max_goles.goles > 0)
    {
        snprintf(buf, sizeof(buf), "%d gol%s", g->max_goles.goles,
                 g->max_goles.goles == 1 ? "" : "es");
        k[n] = "Más goles";
        resumen_pdf_partido_corto(v[n++], sizeof(v[0]), buf, &g->max_goles);
    }
    if (g->max_asistencias.asistencias > 0)
    {
        snprintf(buf, sizeof(buf), "%d asist.", g->max_asistencias.asistencias);
        k[n] = "Más asistencias";
        resumen_pdf_partido_corto(v[n++], sizeof(v[0]), buf, &g->max_asistencias);
    }
    if (g->mejor_rendimiento.rendimiento > 0)
    {
        snprintf(buf, sizeof(buf), "%d/10", g->mejor_rendimiento.rendimiento);
        k[n] = "Mejor rendimiento";
        resumen_pdf_partido_corto(v[n++], sizeof(v[0]), buf, &g->mejor_rendimiento);
    }
    resumen_pdf_titulo(ctx, "Récords del año", 34.0f);
    resumen_pdf_filas(ctx, k, v, n);
}

static void resumen_pdf_meses(ResumenAnioPdfCtx *ctx, const ResumenAnio *r)
{
    const float alto_fila = 15.0f;
    float x = ctx->margin;
    float x_barra = x + 262.0f;
    float ancho_barra = ctx->ancho - 262.0f;
    int max = 1;
    char texto[160];

    if (r->g.partidos == 0)
    {
        return;
    }
    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        int ga = r->meses[i].goles + r->meses[i].asistencias;
        max = ga > max ? ga : max;
    }

    /* La tabla de 12 meses va entera en una pagina. */
    resumen_pdf_titulo(ctx, "Mes a mes", alto_fila * 13.0f + 40.0f);
    resumen_pdf_texto(ctx, x, ctx->y, "MES", 7.5f, "Helvetica-Bold", PDF_C_TINTA_3);
    resumen_pdf_texto_der(ctx, x + 100.0f, ctx->y, "PJ", 7.5f, "Helvetica-Bold", PDF_C_TINTA_3);
    resumen_pdf_texto_der(ctx, x + 130.0f, ctx->y, "G", 7.5f, "Helvetica-Bold", PDF_C_TINTA_3);
    resumen_pdf_texto_der(ctx, x + 160.0f, ctx->y, "A", 7.5f, "Helvetica-Bold", PDF_C_TINTA_3);
    resumen_pdf_texto_der(ctx, x + 210.0f, ctx->y, "V-E-D", 7.5f, "Helvetica-Bold",
                          PDF_C_TINTA_3);
    resumen_pdf_texto_der(ctx, x + 248.0f, ctx->y, "REND.", 7.5f, "Helvetica-Bold",
                          PDF_C_TINTA_3);
    resumen_pdf_texto(ctx, x_barra, ctx->y, "GOLES Y ASISTENCIAS", 7.5f, "Helvetica-Bold",
                      PDF_C_TINTA_3);
    ctx->y -= alto_fila;

    for (int i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        const ResumenAnioMes *m = &r->meses[i];
        uint32_t tinta = m->partidos > 0 ? PDF_C_TINTA : PDF_C_LINEA;
        float wg = ancho_barra * (float)m->goles / (float)max;
        float wa = ancho_barra * (float)m->asistencias / (float)max;

        if (i == r->mejor_mes)
        {
            resumen_pdf_rect(ctx, x - 4.0f, ctx->y - 4.5f, ctx->ancho + 8.0f, alto_fila,
                             PDF_C_VERDE_SUAVE);
        }
        resumen_pdf_texto(ctx, x, ctx->y, resumen_anio_mes_nombre(i), 9.0f,
                          i == r->mejor_mes ? "Helvetica-Bold" : "Helvetica", tinta);
        if (m->partidos > 0)
        {
            snprintf(texto, sizeof(texto), "%d", m->partidos);
            resumen_pdf_texto_der(ctx, x + 100.0f, ctx->y, texto, 9.0f, "Helvetica", tinta);
            snprintf(texto, sizeof(texto), "%d", m->goles);
            resumen_pdf_texto_der(ctx, x + 130.0f, ctx->y, texto, 9.0f, "Helvetica", tinta);
            snprintf(texto, sizeof(texto), "%d", m->asistencias);
            resumen_pdf_texto_der(ctx, x + 160.0f, ctx->y, texto, 9.0f, "Helvetica", tinta);
            snprintf(texto, sizeof(texto), "%d-%d-%d", m->victorias, m->empates, m->derrotas);
            resumen_pdf_texto_der(ctx, x + 210.0f, ctx->y, texto, 9.0f, "Helvetica", tinta);
            resumen_pdf_texto_der(ctx, x + 248.0f, ctx->y,
                                  m->con_rendimiento > 0 ? rnum(m->rendimiento_prom, 1) : "-",
                                  9.0f, "Helvetica-Bold", tinta);
            resumen_pdf_rect(ctx, x_barra, ctx->y - 1.0f, wg, 8.0f, PDF_C_ACENTO);
            resumen_pdf_rect(ctx, x_barra + wg, ctx->y - 1.0f, wa, 8.0f, PDF_C_BAR_2);
        }
        else
        {
            resumen_pdf_texto_der(ctx, x + 100.0f, ctx->y, "-", 9.0f, "Helvetica", tinta);
        }
        ctx->y -= alto_fila;
    }

    ctx->y -= 4.0f;
    texto[0] = '\0';
    if (r->mejor_mes >= 0)
    {
        const ResumenAnioMes *m = &r->meses[r->mejor_mes];
        snprintf(texto, sizeof(texto), "Mejor mes: %s (rendimiento %s en %d partido%s)",
                 resumen_anio_mes_nombre(r->mejor_mes), rnum(m->rendimiento_prom, 1),
                 m->con_rendimiento, m->con_rendimiento == 1 ? "" : "s");
        resumen_pdf_texto(ctx, x, ctx->y, texto, 9.0f, "Helvetica-Bold", PDF_C_TINTA);
    }
    if (r->mes_goleador >= 0)
    {
        const ResumenAnioMes *m = &r->meses[r->mes_goleador];
        snprintf(texto, sizeof(texto), "Mes más goleador: %s (%d G + %d A)",
                 resumen_anio_mes_nombre(r->mes_goleador), m->goles, m->asistencias);
        resumen_pdf_texto_der(ctx, x + ctx->ancho, ctx->y, texto, 9.0f, "Helvetica",
                              PDF_C_TINTA_2);
    }
    resumen_pdf_rect(ctx, x_barra, ctx->y - 16.0f, 8.0f, 6.0f, PDF_C_ACENTO);
    resumen_pdf_texto(ctx, x_barra + 11.0f, ctx->y - 16.0f, "Goles", 7.5f, "Helvetica",
                      PDF_C_TINTA_3);
    resumen_pdf_rect(ctx, x_barra + 46.0f, ctx->y - 16.0f, 8.0f, 6.0f, PDF_C_BAR_2);
    resumen_pdf_texto(ctx, x_barra + 57.0f, ctx->y - 16.0f, "Asistencias", 7.5f, "Helvetica",
                      PDF_C_TINTA_3);
    ctx->y -= 30.0f;
}

static void resumen_pdf_ranking(ResumenAnioPdfCtx *ctx, float x, float ancho, float y,
                                const char *titulo, const ResumenTop *top)
{
    char nombre[160];
    char dato[48];

    resumen_pdf_texto(ctx, x, y, titulo, 7.5f, "Helvetica-Bold", PDF_C_TINTA_3);
    y -= 15.0f;
    if (top->cantidad == 0)
    {
        resumen_pdf_texto(ctx, x, y, "Sin datos", 9.0f, "Helvetica", PDF_C_TINTA_3);
        return;
    }
    for (int i = 0; i < top->cantidad; i++)
    {
        const ResumenTopItem *it = &top->items[i];
        float w_dato;

        snprintf(dato, sizeof(dato), "%d PJ · %s%% V", it->partidos, rnum(it->victorias_pct, 0));
        w_dato = resumen_pdf_ancho_texto(ctx, dato, 8.0f, "Helvetica");
        resumen_pdf_recortar(ctx, it->nombre, 9.0f, "Helvetica-Bold", ancho - w_dato - 8.0f,
                             nombre, sizeof(nombre));
        resumen_pdf_texto(ctx, x, y, nombre, 9.0f, "Helvetica-Bold", PDF_C_TINTA);
        resumen_pdf_texto_der(ctx, x + ancho, y, dato, 8.0f, "Helvetica", PDF_C_TINTA_2);
        resumen_pdf_medidor(ctx, x, y - 7.0f, ancho, 3.5f, it->victorias_pct / 100.0,
                            PDF_C_ACENTO);
        y -= 22.0f;
    }
}

static void resumen_pdf_rankings(ResumenAnioPdfCtx *ctx, const ResumenGeneral *g)
{
    const float sep = 18.0f;
    float col = (ctx->ancho - 2.0f * sep) / 3.0f;
    float alto = 15.0f + 22.0f * RESUMEN_TOP_N + 6.0f;

    if (g->camisetas.cantidad + g->canchas.cantidad + g->rivales.cantidad == 0)
    {
        return;
    }
    resumen_pdf_titulo(ctx, "Dónde y con qué (top 3, % de victorias)", alto);
    resumen_pdf_ranking(ctx, ctx->margin, col, ctx->y, "CAMISETAS", &g->camisetas);
    resumen_pdf_ranking(ctx, ctx->margin + col + sep, col, ctx->y, "CANCHAS", &g->canchas);
    resumen_pdf_ranking(ctx, ctx->margin + 2.0f * (col + sep), col, ctx->y, "RIVALES",
                        &g->rivales);
    ctx->y -= alto;
}

/* Barra etiquetada: nombre, medidor y valor (para promedios y conteos). */
static void resumen_pdf_barra(ResumenAnioPdfCtx *ctx, float x, float ancho, float y,
                              const char *k, double frac, const char *n)
{
    resumen_pdf_texto(ctx, x, y, k, 9.0f, "Helvetica", PDF_C_TINTA_2);
    resumen_pdf_medidor(ctx, x + 74.0f, y - 0.5f, ancho - 74.0f - 30.0f, 7.0f, frac,
                        PDF_C_ACENTO);
    resumen_pdf_texto_der(ctx, x + ancho, y, n, 9.0f, "Helvetica-Bold", PDF_C_TINTA);
}

static void resumen_pdf_como_llegas(ResumenAnioPdfCtx *ctx, const ResumenGeneral *g)
{
    float ancho = ctx->ancho * 0.6f;

    if (g->con_rendimiento + g->con_animo + g->con_cansancio == 0)
    {
        return;
    }
    resumen_pdf_titulo(ctx, "Cómo llegás (promedios de 1 a 10)", 50.0f);
    if (g->con_rendimiento > 0)
    {
        resumen_pdf_barra(ctx, ctx->margin, ancho, ctx->y, "Rendimiento",
                          g->rendimiento_prom / 10.0, rnum(g->rendimiento_prom, 1));
        ctx->y -= 16.0f;
    }
    if (g->con_animo > 0)
    {
        resumen_pdf_barra(ctx, ctx->margin, ancho, ctx->y, "Ánimo", g->animo_prom / 10.0,
                          rnum(g->animo_prom, 1));
        ctx->y -= 16.0f;
    }
    if (g->con_cansancio > 0)
    {
        resumen_pdf_barra(ctx, ctx->margin, ancho, ctx->y, "Cansancio", g->cansancio_prom / 10.0,
                          rnum(g->cansancio_prom, 1));
        ctx->y -= 16.0f;
    }
    ctx->y -= 4.0f;
}

static float resumen_pdf_conteos(ResumenAnioPdfCtx *ctx, float x, float ancho, float y,
                                 const char *titulo, const ResumenConteo *c, int n,
                                 const char *const *nombres)
{
    int max = resumen_max_conteo(c, n);
    char buf[16];

    resumen_pdf_texto(ctx, x, y, titulo, 7.5f, "Helvetica-Bold", PDF_C_TINTA_3);
    y -= 15.0f;
    for (int i = 0; i < n && max > 0; i++)
    {
        if (c[i].partidos == 0)
        {
            continue;
        }
        snprintf(buf, sizeof(buf), "%d", c[i].partidos);
        resumen_pdf_barra(ctx, x, ancho, y, nombres[i], (double)c[i].partidos / max, buf);
        y -= 14.0f;
    }
    return y;
}

static void resumen_pdf_cuando(ResumenAnioPdfCtx *ctx, const ResumenGeneral *g)
{
    const float sep = 24.0f;
    float col = (ctx->ancho - sep) / 2.0f;
    int filas = 0;
    int filas_franja = 0;
    float y1;
    float y2;

    for (int i = 0; i < 7; i++)
    {
        filas += g->dias_semana[i].partidos > 0;
    }
    for (int i = 0; i < RESUMEN_FRANJAS; i++)
    {
        filas_franja += g->franjas[i].partidos > 0;
    }
    if (filas + filas_franja == 0)
    {
        return;
    }
    if (filas_franja > filas)
    {
        filas = filas_franja;
    }
    resumen_pdf_titulo(ctx, "Cuándo jugás (partidos)", 15.0f + 14.0f * (float)filas);
    y1 = resumen_pdf_conteos(ctx, ctx->margin, col, ctx->y, "DÍA DE LA SEMANA", g->dias_semana, 7,
                             MD_DIAS_SEMANA);
    y2 = resumen_pdf_conteos(ctx, ctx->margin + col + sep, col, ctx->y, "FRANJA HORARIA",
                             g->franjas, RESUMEN_FRANJAS, MD_FRANJAS);
    ctx->y = (y1 < y2 ? y1 : y2) - 4.0f;
}

static void resumen_pdf_contexto(ResumenAnioPdfCtx *ctx, const ResumenAnio *r)
{
    const ResumenGeneral *g = &r->g;
    const char *k[10];
    char v[10][192];
    int n = 0;

    if (g->clima_real_partidos > 0)
    {
        k[n] = "Partidos con clima real";
        snprintf(v[n++], sizeof(v[0]), "%d (%s%% del año)", g->clima_real_partidos,
                 rnum(g->clima_real_cobertura_pct, 0));
        if (g->hay_clima_mejor)
        {
            k[n] = "Tu mejor condición";
            snprintf(v[n++], sizeof(v[0]), "%s · rend. %s · %s%% V", g->clima_mejor,
                     rnum(g->clima_mejor_rendimiento, 1), rnum(g->clima_mejor_victorias_pct, 0));
        }
    }
    if (r->partidos_con_marcador > 0)
    {
        k[n] = "Goles del equipo";
        snprintf(v[n++], sizeof(v[0]), "%d a favor · %d en contra", r->goles_favor,
                 r->goles_contra);
        k[n] = "Diferencia de gol";
        snprintf(v[n++], sizeof(v[0]), "%+d en %d partidos", r->goles_favor - r->goles_contra,
                 r->partidos_con_marcador);
    }
    k[n] = "Camisetas usadas";
    snprintf(v[n++], sizeof(v[0]), "%d de %d activas", r->camiseta.usadas, r->camiseta.activas);
    k[n] = "Camiseta más usada";
    snprintf(v[n++], sizeof(v[0]), r->camiseta.top_partidos > 0 ? "%s (%d PJ)" : "%s",
             r->camiseta.top, r->camiseta.top_partidos);
    k[n] = "Canchas usadas";
    snprintf(v[n++], sizeof(v[0]), "%d de %d activas", r->cancha.usadas, r->cancha.activas);
    k[n] = "Cancha más usada";
    snprintf(v[n++], sizeof(v[0]), r->cancha.top_partidos > 0 ? "%s (%d PJ)" : "%s",
             r->cancha.top, r->cancha.top_partidos);
    k[n] = "Lesiones del año";
    if (r->lesiones == 0)
    {
        snprintf(v[n++], sizeof(v[0]), "Ninguna");
    }
    else
    {
        snprintf(v[n++], sizeof(v[0]), "%d (%d activa%s)", r->lesiones, r->lesiones_activas,
                 r->lesiones_activas == 1 ? "" : "s");
    }
    resumen_pdf_titulo(ctx, "Clima, equipo y salud", 34.0f);
    resumen_pdf_filas(ctx, k, v, n);
}

/* Fila de la ficha del mejor partido: etiqueta y valor que puede ocupar
 * varias lineas; si no entra en la pagina pasa a la siguiente. */
static void resumen_pdf_detalle(void *destino, const char *etiqueta, const char *valor)
{
    ResumenAnioPdfCtx *ctx = (ResumenAnioPdfCtx *)destino;
    const float sangria = 110.0f;
    float ancho = ctx->ancho - sangria;
    float alto = 0.0f;
    char ascii[1100];
    const char *texto = valor;

    if (!valor || valor[0] == '\0' || !ctx->page)
    {
        return;
    }
    pdf_set_font(ctx->pdf, "Helvetica");
    if (pdf_add_text_wrap(ctx->pdf, ctx->page, texto, 9.0f, 0.0f, 0.0f, 0.0f, PDF_C_TINTA, ancho,
                          PDF_ALIGN_NO_WRITE, &alto) < 0)
    {
        sanitizar_ascii_basico(valor, ascii, sizeof(ascii));
        texto = ascii;
        alto = 0.0f;
        pdf_add_text_wrap(ctx->pdf, ctx->page, texto, 9.0f, 0.0f, 0.0f, 0.0f, PDF_C_TINTA, ancho,
                          PDF_ALIGN_NO_WRITE, &alto);
    }
    if (alto < 11.0f)
    {
        alto = 11.0f;
    }
    resumen_pdf_reservar(ctx, alto + 6.0f);
    resumen_pdf_texto(ctx, ctx->margin, ctx->y, etiqueta, 9.0f, "Helvetica", PDF_C_TINTA_2);
    pdf_set_font(ctx->pdf, "Helvetica");
    if (pdf_add_text_wrap(ctx->pdf, ctx->page, texto, 9.0f, ctx->margin + sangria, ctx->y, 0.0f,
                          PDF_C_TINTA, ancho, PDF_ALIGN_LEFT, NULL) < 0 && !ctx->warned)
    {
        printf("Advertencia PDF: no se pudo escribir el detalle '%s'.\n", etiqueta);
        ctx->warned = 1;
    }
    ctx->y -= alto + 6.0f;
}

static void resumen_pdf_mejor(ResumenAnioPdfCtx *ctx, const ResumenAnio *r)
{
    const ResumenAnioPartido *p = &r->mejor;
    char v[4][32];
    const char *k[4];
    char titulo[192];
    int n = 0;

    if (r->mejor_id <= 0)
    {
        return;
    }
    if (p->rendimiento_general > 0)
    {
        k[n] = "Rendimiento";
        snprintf(v[n++], sizeof(v[0]), "%d/10", p->rendimiento_general);
    }
    k[n] = "Goles";
    snprintf(v[n++], sizeof(v[0]), "%d", p->goles);
    k[n] = "Asistencias";
    snprintf(v[n++], sizeof(v[0]), "%d", p->asistencias);
    if (p->goles_equipo >= 0 && p->goles_rival >= 0)
    {
        k[n] = "Marcador";
        snprintf(v[n++], sizeof(v[0]), "%d-%d", p->goles_equipo, p->goles_rival);
    }
    snprintf(titulo, sizeof(titulo), "Partido del año%s%.120s", p->rival[0] ? ": vs " : "",
             p->rival);
    resumen_pdf_titulo(ctx, titulo, 60.0f);
    resumen_pdf_cifras(ctx, v, k, n);
    ctx->y -= 4.0f;
    resumen_anio_recorrer_detalle(p, resumen_pdf_detalle, ctx);
}

int resumen_anio_escribir_pdf(const ResumenAnio *r, const char *ruta)
{
    struct pdf_info info;
    ResumenAnioPdfCtx ctx;

    if (!r || !ruta)
    {
        return 0;
    }
    memset(&info, 0, sizeof(info));
    snprintf(info.creator, sizeof(info.creator), "MiFutbolC");
    snprintf(info.producer, sizeof(info.producer), "MiFutbolC");
    snprintf(info.title, sizeof(info.title), "Resumen del año %d", r->anio);
    snprintf(info.author, sizeof(info.author), "MiFutbolC");
    snprintf(info.subject, sizeof(info.subject), "Resumen anual del perfil futbolistico");
    get_datetime(info.date, (int)sizeof(info.date));

    memset(&ctx, 0, sizeof(ctx));
    ctx.pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    if (!ctx.pdf)
    {
        printf("Error al crear el documento PDF.\n");
        return 0;
    }
    ctx.margin = RESUMEN_PDF_MARGEN;
    ctx.ancho = PDF_A4_WIDTH - 2.0f * RESUMEN_PDF_MARGEN;
    ctx.anio = r->anio;
    if (!resumen_pdf_nueva_pagina(&ctx))
    {
        pdf_destroy(ctx.pdf);
        printf("Error al crear la pagina del PDF.\n");
        return 0;
    }

    resumen_pdf_hero(&ctx, r);
    if (r->g.partidos == 0)
    {
        resumen_pdf_titulo(&ctx, "Sin partidos en el año", 16.0f);
        resumen_pdf_texto(&ctx, ctx.margin, ctx.y,
                          "No hay partidos con fecha de este año. Quedan el equipamiento y las "
                          "lesiones.",
                          9.5f, "Helvetica", PDF_C_TINTA_2);
        ctx.y -= 18.0f;
    }
    resumen_pdf_resultados(&ctx, &r->g);
    resumen_pdf_ataque(&ctx, &r->g);
    resumen_pdf_comparacion(&ctx, r);
    resumen_pdf_tendencia(&ctx, r);
    resumen_pdf_records(&ctx, &r->g);
    resumen_pdf_meses(&ctx, r);
    resumen_pdf_rankings(&ctx, &r->g);
    resumen_pdf_como_llegas(&ctx, &r->g);
    resumen_pdf_cuando(&ctx, &r->g);
    resumen_pdf_contexto(&ctx, r);
    resumen_pdf_mejor(&ctx, r);

    if (pdf_save(ctx.pdf, ruta) < 0)
    {
        int errval = 0;
        const char *err = pdf_get_err(ctx.pdf, &errval);

        printf("Error al guardar el PDF (%d): %s\n", errval, err ? err : "error desconocido");
        pdf_destroy(ctx.pdf);
        return 0;
    }

    pdf_destroy(ctx.pdf);
    return 1;
}

/* ── Generacion y menu ────────────────────────────────────────────────────── */

static int resumen_anio_generar_txt(const ResumenAnio *r)
{
    char nombre[64];
    char ruta[1024];
    FILE *f = NULL;

    snprintf(nombre, sizeof(nombre), "resumen_anio_%d.txt", r->anio);
    snprintf(ruta, sizeof(ruta), "%s", get_export_path(nombre));
    fopen_s(&f, ruta, "w");
    if (!f)
    {
        printf("Error al crear el archivo TXT: %s\n", ruta);
        return 0;
    }

    resumen_anio_escribir_txt(f, r);
    fclose(f);
    ui_printf("Resumen TXT generado: %s\n", ruta);
    return 1;
}

static int resumen_anio_generar_html(const ResumenAnio *r)
{
    char nombre[64];
    char ruta[1024];
    FILE *f = NULL;

    snprintf(nombre, sizeof(nombre), "resumen_anio_%d.html", r->anio);
    snprintf(ruta, sizeof(ruta), "%s", get_export_path(nombre));
    fopen_s(&f, ruta, "w");
    if (!f)
    {
        printf("Error al crear el archivo HTML: %s\n", ruta);
        return 0;
    }

    resumen_anio_escribir_html(f, r);
    fclose(f);
    ui_printf("Resumen HTML generado: %s\n", ruta);
    return 1;
}

static int resumen_anio_generar_pdf(const ResumenAnio *r)
{
    char nombre[64];
    char ruta[1024];

    snprintf(nombre, sizeof(nombre), "resumen_anio_%d.pdf", r->anio);
    snprintf(ruta, sizeof(ruta), "%s", get_export_path(nombre));
    if (!resumen_anio_escribir_pdf(r, ruta))
    {
        return 0;
    }

    ui_printf("Resumen PDF generado: %s\n", ruta);
    return 1;
}

static void resumen_anio_mostrar_resumen(const ResumenAnio *r)
{
    ui_printf("\nPeriodo %d\n", r->anio);
    ui_printf("Partidos: %d | Goles: %d | Asistencias: %d\n",
              r->g.partidos, r->g.goles, r->g.asistencias);
    ui_printf("Victorias: %d | Empates: %d | Derrotas: %d\n",
              r->g.victorias, r->g.empates, r->g.derrotas);
    ui_printf("Camisetas usadas: %d | Lesiones: %d | Canchas usadas: %d\n",
              r->camiseta.usadas, r->lesiones, r->cancha.usadas);
}

/* Pide el año del resumen; Enter deja el año en curso. */
static int resumen_anio_pedir_anio(void)
{
    char defecto[16];
    char texto[16];
    int actual = resumen_anio_anio_actual();
    int anio;

    snprintf(defecto, sizeof(defecto), "%d", actual);
    input_string_default("Año del resumen", defecto, texto, (int)sizeof(texto));
    anio = atoi(texto);
    if (anio < 1900 || anio > 9999)
    {
        printf("Año no valido, se usa %d.\n", actual);
        anio = actual;
    }
    return anio;
}

/**
 * @brief Genera el resumen anual en el formato indicado.
 * @param modo 0=TXT, 1=HTML, 2=PDF, 3=los tres formatos.
 */
static void generar_resumen_anio_formato(int modo)
{
    ResumenAnio *r;
    int generados = 0;
    int anio;

    clear_screen();
    print_header("RESUMEN ANUAL");
    anio = resumen_anio_pedir_anio();

    /* El struct pesa varias decenas de KB: va al heap. */
    r = malloc(sizeof(*r));
    if (!r || !resumen_anio_calcular(r, anio))
    {
        printf("\nNo se pudo calcular el resumen del año.\n");
        free(r);
        pause_console();
        return;
    }

    if (modo == 0 || modo == 3)
    {
        generados += resumen_anio_generar_txt(r);
    }
    if (modo == 1 || modo == 3)
    {
        generados += resumen_anio_generar_html(r);
    }
    if (modo == 2 || modo == 3)
    {
        generados += resumen_anio_generar_pdf(r);
    }

    if (generados == 0)
    {
        printf("\nNo se pudo generar ningun archivo del resumen.\n");
    }
    else
    {
        printf("\nArchivos guardados en la carpeta de exportacion.\n");
        resumen_anio_mostrar_resumen(r);
    }
    free(r);

    app_log_event("RESUMEN_COMPARTIBLE", "Resumen anual generado");
    pause_console();
}

void generar_resumen_anio_txt(void)
{
    generar_resumen_anio_formato(0);
}

void generar_resumen_anio_html(void)
{
    generar_resumen_anio_formato(1);
}

void generar_resumen_anio_pdf(void)
{
    generar_resumen_anio_formato(2);
}

void generar_resumen_anio_todos(void)
{
    generar_resumen_anio_formato(3);
}

static void menu_resumen_anio(void)
{
    MenuItem items[] =
    {
        {1, "Resumen Anual en TXT", generar_resumen_anio_txt},
        {2, "Resumen Anual en HTML", generar_resumen_anio_html},
        {3, "Resumen Anual en PDF", generar_resumen_anio_pdf},
        {4, "Generar los tres formatos", generar_resumen_anio_todos},
        {0, "Volver", NULL}
    };
    ejecutar_menu("RESUMEN ANUAL", items, 5);
}
