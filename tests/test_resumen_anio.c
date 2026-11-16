#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * Resumen Anual (opcion 5 del menu de resumenes): filtro por año con fechas
 * "dd/mm/aaaa" y "aaaa-mm-dd" mezcladas, mes a mes con rendimiento promedio,
 * mejor mes y mes goleador, tendencia del año, comparacion con el año
 * anterior (con y sin datos), lesiones, marcador del equipo, clima real
 * filtrado por año y los tres escritores (TXT, HTML y PDF). Usa un perfil
 * propio (mifutbol_test_resumen_anio.db en el directorio de datos) y lo borra
 * al terminar.
 */

#include "unity/unity.h"
#include "db.h"
#include "estadisticas_meta.h"
#include "resumen_compartible.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TEST_SEP "\\"
#else
#define TEST_SEP "/"
#endif

#define PERFIL_TEST "test_resumen_anio"
#define DELTA 0.001

static char g_db_path[1024];
static char g_log_path[1024];
static char g_out_path[1024];

/* Este Unity se compila sin precision double: comparacion propia. */
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

typedef struct
{
    int id;
    const char *fecha;
    int goles;
    int asistencias;
    int resultado;
    int rendimiento; /* 0 = sin cargar (NULL) */
    int camiseta;
} PartidoPrueba;

static void insertar_partido(const PartidoPrueba *p)
{
    char sql[512];
    char rend[16];

    if (p->rendimiento > 0)
    {
        snprintf(rend, sizeof(rend), "%d", p->rendimiento);
    }
    else
    {
        snprintf(rend, sizeof(rend), "NULL");
    }
    snprintf(sql, sizeof(sql),
             "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, "
             "mes_anio, resultado, rendimiento_general, rival_nombre, estado_animo, cansancio) "
             "VALUES(%d, 1, '%s', %d, %d, %d, '', %d, %s, 'Lobos', 8, 5);",
             p->id, p->fecha, p->goles, p->asistencias, p->camiseta, p->resultado, rend);
    exec_ok(sql);
}

/*
 *  id fecha              G A res rend cam
 *   1 2025-03-10 20:00   1 0  V   6    2    <- año anterior
 *   2 15/03/2025 20:00   0 1  D   8    2
 *   3 2026-01-05 20:00   2 0  V   9    1    (marcador 3-1, clima real)
 *   4 10/01/2026 18:00   0 1  E   7    1
 *   5 2026-01-20 21:00   1 1  V   8    1
 *   6 05/02/2026 20:00   4 2  V  10    1    (unico rendimiento de febrero)
 *   7 2026-02-20 20:00   0 0  D   -    1
 *   8 01/03/2026 20:00   0 2  D   5    1
 *   9 2026-03-08 20:00   0 0  D   6    2
 *  10 2027-01-01 10:00   5 0  V   7    1    <- año siguiente, no entra
 */
static void cargar_datos(void)
{
    static const PartidoPrueba partidos[] =
    {
        {1, "2025-03-10 20:00", 1, 0, 1, 6, 2},
        {2, "15/03/2025 20:00", 0, 1, 3, 8, 2},
        {3, "2026-01-05 20:00", 2, 0, 1, 9, 1},
        {4, "10/01/2026 18:00", 0, 1, 2, 7, 1},
        {5, "2026-01-20 21:00", 1, 1, 1, 8, 1},
        {6, "05/02/2026 20:00", 4, 2, 1, 10, 1},
        {7, "2026-02-20 20:00", 0, 0, 3, 0, 1},
        {8, "01/03/2026 20:00", 0, 2, 3, 5, 1},
        {9, "2026-03-08 20:00", 0, 0, 3, 6, 2},
        {10, "2027-01-01 10:00", 5, 0, 1, 7, 1},
    };

    exec_ok("INSERT INTO camiseta(id, nombre, activa) VALUES(1, 'Roja <b>&', 1);");
    exec_ok("INSERT INTO camiseta(id, nombre, activa) VALUES(2, 'Azul', 1);");
    exec_ok("INSERT INTO cancha(id, nombre, activa) VALUES(1, 'Cancha A', 1);");
    for (size_t i = 0; i < sizeof(partidos) / sizeof(partidos[0]); i++)
    {
        insertar_partido(&partidos[i]);
    }
    exec_ok("UPDATE partido SET goles_equipo = 3, goles_rival = 1, weather_code = 0 WHERE id = 3;");
    exec_ok("UPDATE partido SET weather_code = 61 WHERE id = 1;");
    /* El estado se guarda en mayusculas o no: las dos cuentan como activas. */
    exec_ok("INSERT INTO lesion(jugador, tipo, descripcion, fecha, camiseta_id, estado) "
            "VALUES('Yo', 'Esguince', 'x', '01/02/2026 21:40', 1, 'ACTIVA');");
    exec_ok("INSERT INTO lesion(jugador, tipo, descripcion, fecha, camiseta_id, estado) "
            "VALUES('Yo', 'Golpe', 'x', '2026-05-02', 1, 'RECUPERADO');");
    exec_ok("INSERT INTO lesion(jugador, tipo, descripcion, fecha, camiseta_id, estado) "
            "VALUES('Yo', 'Golpe', 'x', '2025-05-02', 1, 'Activa');");
}

static ResumenAnio *calcular(int anio)
{
    ResumenAnio *r = malloc(sizeof(*r));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT(1, resumen_anio_calcular(r, anio));
    return r;
}

/* Lee el archivo de salida entero (malloc) y lo borra. */
static char *leer_salida(long *largo_out)
{
    FILE *f = fopen(g_out_path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long largo = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *texto = malloc((size_t)largo + 1);
    TEST_ASSERT_NOT_NULL(texto);
    size_t leidos = fread(texto, 1, (size_t)largo, f);
    texto[leidos] = '\0';
    fclose(f);
    remove(g_out_path);
    if (largo_out)
    {
        *largo_out = (long)leidos;
    }
    return texto;
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

static void test_filtro_por_anio(void)
{
    cargar_datos();
    ResumenAnio *r = calcular(2026);

    /* Solo los 7 partidos de 2026, con los dos formatos de fecha. */
    TEST_ASSERT_EQUAL_INT(7, r->g.partidos);
    TEST_ASSERT_EQUAL_INT(7, r->g.goles);
    TEST_ASSERT_EQUAL_INT(6, r->g.asistencias);
    TEST_ASSERT_EQUAL_INT(3, r->g.victorias);
    TEST_ASSERT_EQUAL_INT(1, r->g.empates);
    TEST_ASSERT_EQUAL_INT(3, r->g.derrotas);
    ASSERT_CERCA(10.0 * 100.0 / 21.0, r->g.efectividad_pct);
    ASSERT_CERCA(7.5, r->g.rendimiento_prom);
    TEST_ASSERT_EQUAL_STRING("05/01/2026", r->g.primer_partido_fecha);
    TEST_ASSERT_EQUAL_INT(9, r->g.ultimo.id);
    TEST_ASSERT_EQUAL_INT(1, r->g.dobletes);
    TEST_ASSERT_EQUAL_INT(1, r->g.hat_tricks);

    /* Records y rachas dentro del año. */
    TEST_ASSERT_EQUAL_INT(6, r->g.max_goles.id);
    TEST_ASSERT_EQUAL_INT(6, r->g.mejor_rendimiento.id);
    TEST_ASSERT_EQUAL_INT(2, r->g.mejor_racha_victorias);
    TEST_ASSERT_EQUAL_INT(4, r->g.mejor_racha_invicto);

    /* Rankings filtrados: la camiseta 2 tiene 1 partido en 2026 (2 en 2025). */
    TEST_ASSERT_EQUAL_INT(2, r->g.camisetas.cantidad);
    TEST_ASSERT_EQUAL_INT(6, r->g.camisetas.items[0].partidos);
    TEST_ASSERT_EQUAL_INT(1, r->g.camisetas.items[1].partidos);
    TEST_ASSERT_EQUAL_INT(2, r->camiseta.usadas);
    TEST_ASSERT_EQUAL_INT(2, r->camiseta.activas);

    /* Marcador del equipo solo del partido que lo tiene cargado. */
    TEST_ASSERT_EQUAL_INT(1, r->partidos_con_marcador);
    TEST_ASSERT_EQUAL_INT(3, r->goles_favor);
    TEST_ASSERT_EQUAL_INT(1, r->goles_contra);

    /* Lesiones del año, con estado en mayusculas. */
    TEST_ASSERT_EQUAL_INT(2, r->lesiones);
    TEST_ASSERT_EQUAL_INT(1, r->lesiones_activas);

    /* Partido destacado: el de mejor rendimiento. */
    TEST_ASSERT_EQUAL_INT(6, r->mejor_id);
    TEST_ASSERT_EQUAL_INT(4, r->mejor.goles);
    free(r);
}

static void test_mes_a_mes_y_mejor_mes(void)
{
    static const int tendencia[] = {9, 7, 8, 10, 5, 6};
    cargar_datos();
    ResumenAnio *r = calcular(2026);

    TEST_ASSERT_EQUAL_INT(3, r->meses[0].partidos);
    TEST_ASSERT_EQUAL_INT(3, r->meses[0].goles);
    TEST_ASSERT_EQUAL_INT(2, r->meses[0].asistencias);
    TEST_ASSERT_EQUAL_INT(2, r->meses[0].victorias);
    TEST_ASSERT_EQUAL_INT(1, r->meses[0].empates);
    TEST_ASSERT_EQUAL_INT(3, r->meses[0].con_rendimiento);
    ASSERT_CERCA(8.0, r->meses[0].rendimiento_prom);
    /* Febrero: el partido sin rendimiento no entra al promedio. */
    TEST_ASSERT_EQUAL_INT(2, r->meses[1].partidos);
    TEST_ASSERT_EQUAL_INT(1, r->meses[1].con_rendimiento);
    ASSERT_CERCA(10.0, r->meses[1].rendimiento_prom);
    ASSERT_CERCA(5.5, r->meses[2].rendimiento_prom);
    TEST_ASSERT_EQUAL_INT(0, r->meses[3].partidos);

    /* Febrero tiene el mejor promedio pero con un solo partido con dato:
     * con meses de 2 o mas, gana enero. El mes goleador es febrero (6 G+A). */
    TEST_ASSERT_EQUAL_INT(0, r->mejor_mes);
    TEST_ASSERT_EQUAL_INT(1, r->mes_goleador);

    TEST_ASSERT_EQUAL_INT(6, r->tendencia_n);
    for (int i = 0; i < 6; i++)
    {
        TEST_ASSERT_EQUAL_INT(tendencia[i], r->tendencia[i]);
    }
    free(r);
}

static void test_comparacion_con_anio_anterior(void)
{
    cargar_datos();
    ResumenAnio *r = calcular(2026);

    TEST_ASSERT_EQUAL_INT(1, r->hay_anterior);
    TEST_ASSERT_EQUAL_INT(2025, r->anterior.anio);
    TEST_ASSERT_EQUAL_INT(2, r->anterior.partidos);
    TEST_ASSERT_EQUAL_INT(1, r->anterior.goles);
    TEST_ASSERT_EQUAL_INT(1, r->anterior.asistencias);
    ASSERT_CERCA(1.0, r->anterior.ga_pp);
    ASSERT_CERCA(50.0, r->anterior.efectividad_pct);
    ASSERT_CERCA(7.0, r->anterior.rendimiento_prom);
    /* Deltas que muestran los escritores. */
    ASSERT_CERCA(5.0, r->g.partidos - r->anterior.partidos);
    ASSERT_CERCA(10.0 * 100.0 / 21.0 - 50.0, r->g.efectividad_pct - r->anterior.efectividad_pct);
    ASSERT_CERCA(0.5, r->g.rendimiento_prom - r->anterior.rendimiento_prom);
    free(r);

    /* 2027 compara contra 2026 completo. */
    r = calcular(2027);
    TEST_ASSERT_EQUAL_INT(1, r->g.partidos);
    TEST_ASSERT_EQUAL_INT(1, r->hay_anterior);
    TEST_ASSERT_EQUAL_INT(7, r->anterior.partidos);
    free(r);
}

static void test_sin_datos_del_anio_anterior(void)
{
    cargar_datos();
    ResumenAnio *r = calcular(2025);

    TEST_ASSERT_EQUAL_INT(2, r->g.partidos);
    TEST_ASSERT_EQUAL_INT(0, r->hay_anterior);
    TEST_ASSERT_EQUAL_INT(2, r->mejor_mes);
    TEST_ASSERT_EQUAL_INT(1, r->lesiones);
    TEST_ASSERT_EQUAL_INT(1, r->lesiones_activas);

    FILE *f = fopen(g_out_path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    resumen_anio_escribir_html(f, r);
    fclose(f);
    char *html = leer_salida(NULL);
    TEST_ASSERT_NULL(strstr(html, "class=\"comparar\""));
    free(html);
    free(r);
}

static void test_anio_sin_partidos(void)
{
    cargar_datos();
    ResumenAnio *r = calcular(2030);

    TEST_ASSERT_EQUAL_INT(0, r->g.partidos);
    TEST_ASSERT_EQUAL_INT(0, r->hay_anterior);
    TEST_ASSERT_EQUAL_INT(-1, r->mejor_mes);
    TEST_ASSERT_EQUAL_INT(-1, r->mes_goleador);
    TEST_ASSERT_EQUAL_INT(0, r->mejor_id);
    TEST_ASSERT_EQUAL_INT(0, r->tendencia_n);
    TEST_ASSERT_EQUAL_INT(0, r->g.camisetas.cantidad);

    FILE *f = fopen(g_out_path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    resumen_anio_escribir_html(f, r);
    fclose(f);
    char *html = leer_salida(NULL);
    TEST_ASSERT_NOT_NULL(strstr(html, "Sin partidos en 2030"));
    TEST_ASSERT_NULL(strstr(html, "class=\"donut\""));
    TEST_ASSERT_NULL(strstr(html, "class=\"spark\""));
    TEST_ASSERT_NOT_NULL(strstr(html, "Equipamiento y salud"));
    free(html);
    free(r);

    TEST_ASSERT_EQUAL_INT(0, resumen_anio_calcular(NULL, 2026));
    ResumenAnio *invalido = malloc(sizeof(*invalido));
    TEST_ASSERT_NOT_NULL(invalido);
    TEST_ASSERT_EQUAL_INT(0, resumen_anio_calcular(invalido, 0));
    free(invalido);
}

static void test_clima_real_por_anio(void)
{
    ClimaRealStats s;
    cargar_datos();

    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real_anio(&s, 2026));
    TEST_ASSERT_EQUAL_INT(7, s.total_partidos);
    TEST_ASSERT_EQUAL_INT(1, s.partidos_con_datos);
    TEST_ASSERT_EQUAL_INT(1, s.cielo[0].partidos);
    TEST_ASSERT_EQUAL_INT(0, s.cielo[3].partidos);

    /* anio 0 equivale a la funcion de siempre (todos los partidos). */
    TEST_ASSERT_EQUAL_INT(1, calcular_rendimiento_clima_real(&s));
    TEST_ASSERT_EQUAL_INT(10, s.total_partidos);
    TEST_ASSERT_EQUAL_INT(2, s.partidos_con_datos);
    TEST_ASSERT_EQUAL_INT(1, s.cielo[3].partidos);
    TEST_ASSERT_EQUAL_INT(0, calcular_rendimiento_clima_real_anio(&s, -1));
}

static void test_escritores(void)
{
    long largo = 0;
    cargar_datos();
    ResumenAnio *r = calcular(2026);

    FILE *f = fopen(g_out_path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    resumen_anio_escribir_html(f, r);
    fclose(f);
    char *html = leer_salida(NULL);
    TEST_ASSERT_NOT_NULL(strstr(html, "Roja &lt;b&gt;&amp;"));
    TEST_ASSERT_NULL(strstr(html, "Roja <b>"));
    TEST_ASSERT_NULL(strstr(html, "http://"));
    TEST_ASSERT_NULL(strstr(html, "https://"));
    TEST_ASSERT_NULL(strstr(html, "<script"));
    TEST_ASSERT_NOT_NULL(strstr(html, "prefers-color-scheme: dark"));
    TEST_ASSERT_NOT_NULL(strstr(html, "class=\"comparar\""));
    TEST_ASSERT_NOT_NULL(strstr(html, "class=\"columnas\""));
    TEST_ASSERT_NOT_NULL(strstr(html, "class=\"spark\""));
    TEST_ASSERT_NOT_NULL(strstr(html, "Mejor mes"));
    TEST_ASSERT_NOT_NULL(strstr(html, "</html>"));
    free(html);

    f = fopen(g_out_path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    resumen_anio_escribir_txt(f, r);
    fclose(f);
    char *txt = leer_salida(NULL);
    TEST_ASSERT_NOT_NULL(strstr(txt, "RESUMEN DEL AÑO 2026"));
    TEST_ASSERT_NOT_NULL(strstr(txt, "COMPARACIÓN CON 2025"));
    TEST_ASSERT_NOT_NULL(strstr(txt, "<- mejor mes"));
    TEST_ASSERT_NOT_NULL(strstr(txt, "↑"));
    free(txt);

    TEST_ASSERT_EQUAL_INT(1, resumen_anio_escribir_pdf(r, g_out_path));
    char *pdf = leer_salida(&largo);
    TEST_ASSERT_TRUE(largo > 1000);
    TEST_ASSERT_EQUAL_INT(0, strncmp(pdf, "%PDF", 4));
    TEST_ASSERT_NOT_NULL(strstr(pdf, "%%EOF"));
    free(pdf);
    free(r);
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
    snprintf(g_out_path, sizeof(g_out_path), "%s" TEST_SEP "%s_salida.tmp", data_dir,
             PERFIL_TEST);
    db_close();

    UNITY_BEGIN();

    RUN_TEST(test_filtro_por_anio);
    RUN_TEST(test_mes_a_mes_y_mejor_mes);
    RUN_TEST(test_comparacion_con_anio_anterior);
    RUN_TEST(test_sin_datos_del_anio_anterior);
    RUN_TEST(test_anio_sin_partidos);
    RUN_TEST(test_clima_real_por_anio);
    RUN_TEST(test_escritores);

    int fallos = UNITY_END();
    remove(g_log_path);
    return fallos;
}
