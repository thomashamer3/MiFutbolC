#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * Rendimiento por condiciones reales de clima (Open-Meteo): agrupacion por
 * lluvia, viento y cielo, promedios sin NULL, % de victorias, umbral de
 * partidos para "mejor condicion" y base sin datos. Usa un perfil propio
 * (mifutbol_test_estadisticas_clima.db en el directorio de datos) y lo borra
 * al terminar.
 */

#include "unity/unity.h"
#include "db.h"
#include "estadisticas.h"
#include "estadisticas_meta.h"
#include "menu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define TEST_SEP "\\"
#else
#define TEST_SEP "/"
#endif

#define PERFIL_TEST "test_estadisticas_clima"
#define DELTA 0.001

/* Grupos por indice, en el orden logico de la pantalla. */
#define LLUVIA_SIN 0
#define LLUVIA_LEVE 1
#define LLUVIA_FUERTE 2
#define VIENTO_CALMO 0
#define VIENTO_MODERADO 1
#define VIENTO_FUERTE 2
#define CIELO_DESPEJADO 0
#define CIELO_NUBLADO 1
#define CIELO_NIEBLA 2
#define CIELO_LLUVIA 3
#define CIELO_NIEVE 4
#define CIELO_TORMENTA 5

static char g_db_path[1024];
static char g_log_path[1024];

/* Este Unity se compila sin precision double (TEST_ASSERT_DOUBLE_WITHIN
 * falla con "Double Precision Disabled"): comparacion propia con tolerancia. */
#define ASSERT_CERCA(esperado, real) assert_cerca((esperado), (real), __LINE__)

static void assert_cerca(double esperado, double real, int linea)
{
    if (fabs(esperado - real) > DELTA)
    {
        static char mensaje[128];
        snprintf(mensaje, sizeof(mensaje), "Esperado %.4f, obtenido %.4f", esperado, real);
        UNITY_TEST_FAIL(linea, mensaje);
    }
}

static void borrar_base(const char *path)
{
    char aux[1100];
    remove(path);
    snprintf(aux, sizeof(aux), "%s-wal", path);
    remove(aux);
    snprintf(aux, sizeof(aux), "%s-shm", path);
    remove(aux);
}

static void exec_ok(const char *sql)
{
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(db, sql, NULL, NULL, NULL));
}

/* Los valores van como texto SQL para poder pasar "NULL". */
static void insertar_partido(int id, const char *precip, const char *viento, const char *codigo,
                             const char *rendimiento, int goles, int asistencias, int resultado)
{
    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, "
             "mes_anio, resultado, rendimiento_general, precip_mm, wind_kmh, weather_code) "
             "VALUES(%d, 1, '2026-01-05 10:00', %d, %d, 1, '', %d, %s, %s, %s, %s);",
             id, goles, asistencias, resultado, rendimiento, precip, viento, codigo);
    exec_ok(sql);
}

void setUp(void)
{
    borrar_base(g_db_path);
    TEST_ASSERT_TRUE(db_init());
    TEST_ASSERT_NOT_NULL(db);
}

void tearDown(void)
{
    db_close();
    borrar_base(g_db_path);
}

/*
 * Datos: 8 partidos, 7 con clima de Open-Meteo.
 *  id precip viento codigo rend goles asist resultado
 *   1   0.0     5     0      8    2     1     1 (victoria)
 *   2   0.05    8     1      6    0     1     3 (derrota)
 *   3   0.0    12     2      7    1     0     1
 *   4   3.0    20    61      5    1     0     2 (empate)
 *   5  12.0    30    95      9    3     2     1
 *   6   0.0     3     0   NULL    1     1     1
 *   7  NULL  NULL  NULL     10    5     5     1 (sin clima: no cuenta)
 *   8   5.0    25    45      4    0     0     0 (sin resultado)
 */
static void cargar_datos_completos(void)
{
    insertar_partido(1, "0.0", "5.0", "0", "8", 2, 1, 1);
    insertar_partido(2, "0.05", "8.0", "1", "6", 0, 1, 3);
    insertar_partido(3, "0.0", "12.0", "2", "7", 1, 0, 1);
    insertar_partido(4, "3.0", "20.0", "61", "5", 1, 0, 2);
    insertar_partido(5, "12.0", "30.0", "95", "9", 3, 2, 1);
    insertar_partido(6, "0.0", "3.0", "0", "NULL", 1, 1, 1);
    insertar_partido(7, "NULL", "NULL", "NULL", "10", 5, 5, 1);
    insertar_partido(8, "5.0", "25.0", "45", "4", 0, 0, 0);
}

static void test_cobertura_cuenta_solo_partidos_con_weather_code(void)
{
    cargar_datos_completos();
    ClimaRealStats s;
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));
    TEST_ASSERT_EQUAL_INT(8, s.total_partidos);
    TEST_ASSERT_EQUAL_INT(7, s.partidos_con_datos);
    ASSERT_CERCA(87.5, s.cobertura_pct);
}

static void test_agrupa_por_lluvia_con_umbrales(void)
{
    cargar_datos_completos();
    ClimaRealStats s;
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));

    /* Sin lluvia: 1, 2 (0.05 < 0.1), 3 y 6. El rendimiento NULL del 6 no
     * entra en el promedio, pero sus goles y asistencias si. */
    const ClimaRealGrupo *sin = &s.lluvia[LLUVIA_SIN];
    TEST_ASSERT_EQUAL_STRING("Sin lluvia", sin->etiqueta);
    TEST_ASSERT_EQUAL_INT(4, sin->partidos);
    TEST_ASSERT_EQUAL_INT(3, sin->con_rendimiento);
    ASSERT_CERCA(7.0, sin->rendimiento_promedio);
    TEST_ASSERT_EQUAL_INT(4, sin->con_goles);
    ASSERT_CERCA(1.0, sin->goles_promedio);
    ASSERT_CERCA(0.75, sin->asistencias_promedio);
    TEST_ASSERT_EQUAL_INT(4, sin->con_resultado);
    ASSERT_CERCA(75.0, sin->victorias_pct);

    /* Leve: 4 (3 mm) y 8 (5 mm, limite incluido). El 8 no tiene resultado:
     * el % de victorias se calcula sobre 1 partido (empate). */
    const ClimaRealGrupo *leve = &s.lluvia[LLUVIA_LEVE];
    TEST_ASSERT_EQUAL_INT(2, leve->partidos);
    ASSERT_CERCA(4.5, leve->rendimiento_promedio);
    ASSERT_CERCA(0.5, leve->goles_promedio);
    TEST_ASSERT_EQUAL_INT(1, leve->con_resultado);
    ASSERT_CERCA(0.0, leve->victorias_pct);

    const ClimaRealGrupo *fuerte = &s.lluvia[LLUVIA_FUERTE];
    TEST_ASSERT_EQUAL_INT(1, fuerte->partidos);
    ASSERT_CERCA(9.0, fuerte->rendimiento_promedio);
    ASSERT_CERCA(100.0, fuerte->victorias_pct);

    /* El partido 7 (sin clima) no aparece en ningun grupo. */
    TEST_ASSERT_EQUAL_INT(7, sin->partidos + leve->partidos + fuerte->partidos);
}

static void test_agrupa_por_viento(void)
{
    cargar_datos_completos();
    ClimaRealStats s;
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));

    const ClimaRealGrupo *calmo = &s.viento[VIENTO_CALMO];
    TEST_ASSERT_EQUAL_STRING("Viento calmo", calmo->etiqueta);
    TEST_ASSERT_EQUAL_INT(3, calmo->partidos);
    TEST_ASSERT_EQUAL_INT(2, calmo->con_rendimiento);
    ASSERT_CERCA(7.0, calmo->rendimiento_promedio);
    ASSERT_CERCA(200.0 / 3.0, calmo->victorias_pct);

    /* Moderado: 3 (12), 4 (20) y 8 (25, limite incluido). */
    const ClimaRealGrupo *moderado = &s.viento[VIENTO_MODERADO];
    TEST_ASSERT_EQUAL_INT(3, moderado->partidos);
    ASSERT_CERCA(16.0 / 3.0, moderado->rendimiento_promedio);
    TEST_ASSERT_EQUAL_INT(2, moderado->con_resultado);
    ASSERT_CERCA(50.0, moderado->victorias_pct);

    TEST_ASSERT_EQUAL_INT(1, s.viento[VIENTO_FUERTE].partidos);
}

static void test_agrupa_por_cielo_wmo(void)
{
    cargar_datos_completos();
    ClimaRealStats s;
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));

    TEST_ASSERT_EQUAL_STRING("Despejado", s.cielo[CIELO_DESPEJADO].etiqueta);
    TEST_ASSERT_EQUAL_INT(3, s.cielo[CIELO_DESPEJADO].partidos);
    TEST_ASSERT_EQUAL_INT(1, s.cielo[CIELO_NUBLADO].partidos);
    TEST_ASSERT_EQUAL_INT(1, s.cielo[CIELO_NIEBLA].partidos);
    TEST_ASSERT_EQUAL_INT(1, s.cielo[CIELO_LLUVIA].partidos);
    TEST_ASSERT_EQUAL_INT(0, s.cielo[CIELO_NIEVE].partidos);
    TEST_ASSERT_EQUAL_INT(1, s.cielo[CIELO_TORMENTA].partidos);
    ASSERT_CERCA(4.0, s.cielo[CIELO_NIEBLA].rendimiento_promedio);
    /* El partido de niebla no tiene resultado. */
    TEST_ASSERT_EQUAL_INT(0, s.cielo[CIELO_NIEBLA].con_resultado);
}

/* Sin lluvia (7.0, 4 PJ), viento calmo (7.0, 3 PJ) y despejado (7.0, 3 PJ)
 * empatan en rendimiento: gana el de mas partidos. La lluvia fuerte (9.0)
 * tiene 1 solo partido y no compite. */
static void test_mejor_condicion_con_umbral_y_desempate(void)
{
    cargar_datos_completos();
    ClimaRealStats s;
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));
    TEST_ASSERT_NOT_NULL(s.mejor);
    TEST_ASSERT_EQUAL_PTR(&s.lluvia[LLUVIA_SIN], s.mejor);
    ASSERT_CERCA(7.0, s.mejor->rendimiento_promedio);
    TEST_ASSERT_EQUAL_INT(4, s.mejor->partidos);
}

/* Con 2 partidos por grupo nadie llega al minimo; un codigo WMO fuera de las
 * listas (4) cuenta para lluvia y viento pero no para el cielo. */
static void test_sin_grupo_con_tres_partidos_no_hay_mejor(void)
{
    insertar_partido(1, "0.0", "5.0", "4", "10", 1, 0, 1);
    insertar_partido(2, "0.0", "5.0", "4", "9", 1, 0, 1);
    ClimaRealStats s;
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));
    TEST_ASSERT_EQUAL_INT(2, s.lluvia[LLUVIA_SIN].partidos);
    TEST_ASSERT_EQUAL_INT(2, s.viento[VIENTO_CALMO].partidos);
    for (int i = 0; i < CLIMA_REAL_GRUPOS_CIELO; i++)
    {
        TEST_ASSERT_EQUAL_INT(0, s.cielo[i].partidos);
    }
    TEST_ASSERT_NULL(s.mejor);

    /* Un grupo con 3 partidos pero todos sin rendimiento tampoco compite. */
    insertar_partido(3, "0.0", "5.0", "4", "NULL", 1, 0, 1);
    exec_ok("UPDATE partido SET rendimiento_general = NULL;");
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));
    TEST_ASSERT_EQUAL_INT(3, s.lluvia[LLUVIA_SIN].partidos);
    TEST_ASSERT_EQUAL_INT(0, s.lluvia[LLUVIA_SIN].con_rendimiento);
    TEST_ASSERT_NULL(s.mejor);
}

static void test_sin_datos(void)
{
    ClimaRealStats s;
    TEST_ASSERT_EQUAL_INT(0, calcular_rendimiento_clima_real(NULL));

    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));
    TEST_ASSERT_EQUAL_INT(0, s.total_partidos);
    TEST_ASSERT_EQUAL_INT(0, s.partidos_con_datos);
    ASSERT_CERCA(0.0, s.cobertura_pct);
    TEST_ASSERT_NULL(s.mejor);

    /* Partidos cargados, pero ninguno con clima de Open-Meteo. */
    insertar_partido(1, "NULL", "NULL", "NULL", "8", 1, 0, 1);
    insertar_partido(2, "NULL", "NULL", "NULL", "7", 1, 0, 1);
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));
    TEST_ASSERT_EQUAL_INT(2, s.total_partidos);
    TEST_ASSERT_EQUAL_INT(0, s.partidos_con_datos);
    for (int i = 0; i < CLIMA_REAL_GRUPOS_LLUVIA; i++)
    {
        TEST_ASSERT_EQUAL_INT(0, s.lluvia[i].partidos);
    }
    TEST_ASSERT_NULL(s.mejor);
}

static void test_menu_rendimiento_tiene_opcion_clima_real(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_estadisticas_rendimiento();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    /* 21 opciones mas "Volver", que sigue siendo la ultima. */
    TEST_ASSERT_EQUAL_INT(22, capture.cantidad);
    TEST_ASSERT_EQUAL_INT(0, capture.last_item.opcion);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
    TEST_ASSERT_NOT_NULL(mostrar_rendimiento_por_clima_real);
}

int main(void)
{
    if (!db_set_active_user(PERFIL_TEST))
    {
        fprintf(stderr, "No se pudo seleccionar el perfil de prueba\n");
        return 1;
    }
    if (!db_init())
    {
        fprintf(stderr, "db_init fallo\n");
        return 1;
    }
    const char *data_dir = get_data_dir();
    if (!data_dir)
    {
        fprintf(stderr, "Sin directorio de datos\n");
        return 1;
    }
    snprintf(g_db_path, sizeof(g_db_path), "%s" TEST_SEP "mifutbol_%s.db", data_dir, PERFIL_TEST);
    snprintf(g_log_path, sizeof(g_log_path), "%s" TEST_SEP "mifutbol_%s.log", data_dir,
             PERFIL_TEST);
    db_close();

    UNITY_BEGIN();

    RUN_TEST(test_cobertura_cuenta_solo_partidos_con_weather_code);
    RUN_TEST(test_agrupa_por_lluvia_con_umbrales);
    RUN_TEST(test_agrupa_por_viento);
    RUN_TEST(test_agrupa_por_cielo_wmo);
    RUN_TEST(test_mejor_condicion_con_umbral_y_desempate);
    RUN_TEST(test_sin_grupo_con_tres_partidos_no_hay_mejor);
    RUN_TEST(test_sin_datos);
    RUN_TEST(test_menu_rendimiento_tiene_opcion_clima_real);

    int fallos = UNITY_END();
    remove(g_log_path);
    return fallos;
}
