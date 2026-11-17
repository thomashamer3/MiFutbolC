#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * Clima diferido: tabla partido_clima_pendiente, trigger de borrado, tope de
 * intentos, lista de intentados de la sesion y aplicacion de una respuesta de
 * Open-Meteo ya descargada. No lanza curl: la respuesta se escribe en un
 * archivo y se aplica con el gancho de prueba. Usa un perfil propio
 * (mifutbol_test_clima.db en el directorio de datos) y lo borra al terminar.
 */

#include "test_db_fixture.h"
#include "clima_diferido.h"
#include "openmeteo_client.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PERFIL_TEST "test_clima"
#define MAX_INTENTOS 8
#define FECHA_VIEJA "2025-10-01 10:00"

static char g_json_path[1024];

/* Respuesta de ejemplo (la misma de test_openmeteo): 20 C de promedio, salida
 * del sol 06:15 y puesta 19:40. */
static const char *JSON_CON_DATOS =
    "{\"latitude\":-34.6,\"longitude\":-58.4,"
    "\"hourly\":{"
    "\"time\":[\"2025-10-01T00:00\",\"2025-10-01T01:00\",\"2025-10-01T02:00\"],"
    "\"temperature_2m\":[10.0,20.0,30.0],"
    "\"apparent_temperature\":[8.0,12.0,10.0],"
    "\"precipitation\":[0.5,1.0,0.5],"
    "\"weather_code\":[3,61,61],"
    "\"wind_speed_10m\":[10.0,20.0,30.0]},"
    "\"daily\":{\"time\":[\"2025-10-01\"],"
    "\"sunrise\":[\"2025-10-01T06:15\"],\"sunset\":[\"2025-10-01T19:40\"]}}";

static int intentos_de(long long partido_id)
{
    char sql[160];
    snprintf(sql, sizeof(sql),
             "SELECT COALESCE((SELECT intentos FROM partido_clima_pendiente "
             "WHERE partido_id = %lld), -1);",
             partido_id);
    return consultar_int(sql);
}

static int pendiente_existe(long long partido_id)
{
    char sql[128];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM partido_clima_pendiente WHERE partido_id = %lld;", partido_id);
    return consultar_int(sql);
}

/* Fecha local desplazada @p dias desde hoy, con el formato de partido. */
static void fecha_relativa(int dias, char *salida, size_t tam)
{
    time_t t = time(NULL) + (time_t)dias * 24 * 3600;
    struct tm tm_fecha = {0};
#ifdef _WIN32
    localtime_s(&tm_fecha, &t);
#else
    localtime_r(&t, &tm_fecha);
#endif
    strftime(salida, tam, "%Y-%m-%d 10:00", &tm_fecha);
}

/* Cancha 1 con coordenadas, cancha 2 sin coordenadas. */
static void crear_canchas(void)
{
    exec_ok("INSERT INTO camiseta(id, nombre) VALUES(1, 'Titular');");
    exec_ok("INSERT INTO cancha(id, nombre, latitud, longitud) VALUES(1, 'Con GPS', -34.6, -58.4);");
    exec_ok("INSERT INTO cancha(id, nombre) VALUES(2, 'Sin GPS');");
}

static void insertar_partido(long long id, int cancha_id, const char *fecha)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, "
             "mes_anio) VALUES(%lld, %d, '%s', 0, 0, 1, '');",
             id, cancha_id, fecha);
    exec_ok(sql);
}

static void escribir_json(const char *contenido)
{
    FILE *f = NULL;
    TEST_ASSERT_EQUAL_INT(0, fopen_s(&f, g_json_path, "wb"));
    TEST_ASSERT_NOT_NULL(f);
    fputs(contenido, f);
    fclose(f);
}

void setUp(void)
{
    borrar_base(g_db_path);
    remove(g_json_path);
    TEST_ASSERT_TRUE(db_init());
    TEST_ASSERT_TRUE(clima_diferido_init());
    openmeteo_set_silencioso(1);
}

void tearDown(void)
{
    /* Libera la lista de intentados y olvida la tabla: la proxima base es nueva. */
    clima_diferido_finalizar();
    openmeteo_set_silencioso(0);
    db_close();
    borrar_base(g_db_path);
    remove(g_json_path);
}

/* Encolar crea la fila con 0 intentos y volver a encolar la reinicia. */
static void test_encolar_crea_y_reinicia_intentos(void)
{
    crear_canchas();
    insertar_partido(1, 1, FECHA_VIEJA);

    TEST_ASSERT_EQUAL_INT(0, clima_diferido_encolar(0));
    TEST_ASSERT_EQUAL_INT(0, clima_diferido_encolar(-5));

    TEST_ASSERT_EQUAL_INT(1, clima_diferido_encolar(1));
    TEST_ASSERT_EQUAL_INT(0, intentos_de(1));

    exec_ok("UPDATE partido_clima_pendiente SET intentos = 5 WHERE partido_id = 1;");
    TEST_ASSERT_EQUAL_INT(1, clima_diferido_encolar(1));
    TEST_ASSERT_EQUAL_INT(0, intentos_de(1));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido_clima_pendiente;"));
}

/* El trigger limpia el pendiente al borrar el partido, por cualquier camino. */
static void test_trigger_borra_pendiente_con_el_partido(void)
{
    crear_canchas();
    insertar_partido(1, 1, FECHA_VIEJA);
    insertar_partido(2, 1, FECHA_VIEJA);
    TEST_ASSERT_TRUE(clima_diferido_encolar(1));
    TEST_ASSERT_TRUE(clima_diferido_encolar(2));

    exec_ok("DELETE FROM partido WHERE id = 1;");
    TEST_ASSERT_EQUAL_INT(0, pendiente_existe(1));
    TEST_ASSERT_EQUAL_INT(1, pendiente_existe(2));
}

/* Los fallos de partidos viejos suman intentos y con 8 dejan de buscarse;
 * los de partidos de los ultimos 14 dias no cuentan. */
static void test_tope_de_intentos_y_partidos_recientes(void)
{
    char reciente[32];
    fecha_relativa(-3, reciente, sizeof(reciente));
    crear_canchas();
    insertar_partido(1, 1, FECHA_VIEJA);
    insertar_partido(2, 1, reciente);
    TEST_ASSERT_TRUE(clima_diferido_encolar(1));
    TEST_ASSERT_TRUE(clima_diferido_encolar(2));

    int anio = 0;
    int mes = 0;
    int dia = 0;
    TEST_ASSERT_EQUAL_INT(3, sscanf(reciente, "%d-%d-%d", &anio, &mes, &dia));
    for (int i = 0; i < MAX_INTENTOS + 2; i++)
    {
        clima_diferido_test_registrar_fallo(2, anio, mes, dia);
    }
    TEST_ASSERT_EQUAL_INT(0, intentos_de(2));

    for (int i = 0; i < MAX_INTENTOS - 1; i++)
    {
        clima_diferido_test_registrar_fallo(1, 2025, 10, 1);
    }
    TEST_ASSERT_EQUAL_INT(MAX_INTENTOS - 1, intentos_de(1));
    TEST_ASSERT_EQUAL_INT(1, (int)clima_diferido_test_buscar_siguiente());

    clima_diferido_test_registrar_fallo(1, 2025, 10, 1);
    TEST_ASSERT_EQUAL_INT(MAX_INTENTOS, intentos_de(1));
    /* Con el tope alcanzado se saltea, pero la fila no se borra. */
    TEST_ASSERT_EQUAL_INT(2, (int)clima_diferido_test_buscar_siguiente());
    TEST_ASSERT_EQUAL_INT(1, pendiente_existe(1));

    /* Encolar de nuevo (p.ej. al editar el partido) le devuelve los intentos. */
    TEST_ASSERT_TRUE(clima_diferido_encolar(1));
    TEST_ASSERT_EQUAL_INT(1, (int)clima_diferido_test_buscar_siguiente());
}

/* Descartes: partido sin coordenadas o que ya tiene clima; las fechas futuras
 * quedan pendientes y se marcan como intentadas en la sesion. */
static void test_buscar_siguiente_descarta_y_saltea_futuros(void)
{
    char futura[32];
    fecha_relativa(30, futura, sizeof(futura));
    crear_canchas();
    insertar_partido(1, 2, FECHA_VIEJA); /* cancha sin coordenadas */
    insertar_partido(2, 1, FECHA_VIEJA);
    insertar_partido(3, 1, futura);
    insertar_partido(4, 1, FECHA_VIEJA);
    exec_ok("UPDATE partido SET temperatura_c = 18.5 WHERE id = 2;");
    for (long long id = 1; id <= 4; id++)
    {
        TEST_ASSERT_TRUE(clima_diferido_encolar(id));
    }

    TEST_ASSERT_EQUAL_INT(4, (int)clima_diferido_test_buscar_siguiente());
    TEST_ASSERT_EQUAL_INT(0, pendiente_existe(1));
    TEST_ASSERT_EQUAL_INT(0, pendiente_existe(2));
    TEST_ASSERT_EQUAL_INT(1, pendiente_existe(3));
    TEST_ASSERT_TRUE(clima_diferido_test_ya_intentado(3));
    TEST_ASSERT_EQUAL_INT(0, intentos_de(3));

    /* Ya intentado en la sesion: no se vuelve a ofrecer. */
    clima_diferido_test_marcar_intentado(4);
    TEST_ASSERT_EQUAL_INT(0, (int)clima_diferido_test_buscar_siguiente());
}

/* desmarcar_intentado (via encolar) quita con swap-remove sin perder al resto. */
static void test_desmarcar_intentado_swap_remove(void)
{
    crear_canchas();
    for (long long id = 1; id <= 4; id++)
    {
        insertar_partido(id, 1, FECHA_VIEJA);
        clima_diferido_test_marcar_intentado(id);
    }
    clima_diferido_test_marcar_intentado(2); /* repetido: no se duplica */
    TEST_ASSERT_EQUAL_INT(4, clima_diferido_test_intentados_cant());

    /* Quitar uno del medio: el ultimo ocupa su lugar. */
    TEST_ASSERT_TRUE(clima_diferido_encolar(2));
    TEST_ASSERT_EQUAL_INT(3, clima_diferido_test_intentados_cant());
    TEST_ASSERT_FALSE(clima_diferido_test_ya_intentado(2));
    TEST_ASSERT_TRUE(clima_diferido_test_ya_intentado(1));
    TEST_ASSERT_TRUE(clima_diferido_test_ya_intentado(3));
    TEST_ASSERT_TRUE(clima_diferido_test_ya_intentado(4));

    /* Quitar el ultimo y el primero. */
    TEST_ASSERT_TRUE(clima_diferido_encolar(3));
    TEST_ASSERT_TRUE(clima_diferido_encolar(1));
    TEST_ASSERT_EQUAL_INT(1, clima_diferido_test_intentados_cant());
    TEST_ASSERT_TRUE(clima_diferido_test_ya_intentado(4));

    /* Encolar uno que no estaba marcado no cambia la lista. */
    insertar_partido(5, 1, FECHA_VIEJA);
    TEST_ASSERT_TRUE(clima_diferido_encolar(5));
    TEST_ASSERT_EQUAL_INT(1, clima_diferido_test_intentados_cant());
}

/* Una respuesta valida se guarda en el partido y borra el pendiente. */
static void test_aplicar_respuesta_guarda_clima(void)
{
    crear_canchas();
    insertar_partido(1, 1, FECHA_VIEJA);
    TEST_ASSERT_TRUE(clima_diferido_encolar(1));
    escribir_json(JSON_CON_DATOS);

    clima_diferido_test_aplicar_archivo(1, 1, FECHA_VIEJA, g_json_path, 0);

    TEST_ASSERT_EQUAL_INT(0, pendiente_existe(1));
    TEST_ASSERT_EQUAL_INT(20, consultar_int("SELECT CAST(ROUND(temperatura_c) AS INT) "
                                            "FROM partido WHERE id = 1;"));
    TEST_ASSERT_EQUAL_INT(61, consultar_int("SELECT weather_code FROM partido WHERE id = 1;"));
    /* 10:00, despues de la salida del sol y antes del mediodia: franja 2. */
    TEST_ASSERT_EQUAL_INT(2, consultar_int("SELECT dia FROM partido WHERE id = 1;"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido "
                                           "WHERE id = 1 AND clima_json LIKE '%temperature_2m%';"));
}

/* Si el partido cambio durante la consulta no se aplica ni suma intento; un
 * fallo de curl o una respuesta sin datos suma uno. */
static void test_aplicar_respuesta_partido_cambiado_o_fallida(void)
{
    crear_canchas();
    insertar_partido(1, 1, FECHA_VIEJA);
    TEST_ASSERT_TRUE(clima_diferido_encolar(1));
    escribir_json(JSON_CON_DATOS);

    /* Lanzado con otra fecha: el partido ya no coincide. */
    clima_diferido_test_aplicar_archivo(1, 1, "2025-09-01 10:00", g_json_path, 0);
    TEST_ASSERT_EQUAL_INT(0, intentos_de(1));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT temperatura_c IS NULL FROM partido WHERE id = 1;"));

    /* curl termino con error: no se lee el archivo y suma un intento. */
    clima_diferido_test_aplicar_archivo(1, 1, FECHA_VIEJA, g_json_path, 7);
    TEST_ASSERT_EQUAL_INT(1, intentos_de(1));

    /* Respuesta sin JSON valido: otro intento. */
    escribir_json("{\"error\":true,\"reason\":\"sin datos\"}");
    clima_diferido_test_aplicar_archivo(1, 1, FECHA_VIEJA, g_json_path, 0);
    TEST_ASSERT_EQUAL_INT(2, intentos_de(1));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT temperatura_c IS NULL FROM partido WHERE id = 1;"));
}

int main(void)
{
    if (!fixture_iniciar(PERFIL_TEST, g_json_path, sizeof(g_json_path), "_respuesta.json"))
    {
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_encolar_crea_y_reinicia_intentos);
    RUN_TEST(test_trigger_borra_pendiente_con_el_partido);
    RUN_TEST(test_tope_de_intentos_y_partidos_recientes);
    RUN_TEST(test_buscar_siguiente_descarta_y_saltea_futuros);
    RUN_TEST(test_desmarcar_intentado_swap_remove);
    RUN_TEST(test_aplicar_respuesta_guarda_clima);
    RUN_TEST(test_aplicar_respuesta_partido_cambiado_o_fallida);

    return fixture_terminar();
}
