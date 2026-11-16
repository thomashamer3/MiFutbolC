#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * Resumen general compartible (opciones 1 HTML y 2 Markdown): calculo de
 * efectividad, rachas, records, ultimos N en orden por fecha (con fechas en
 * "dd/mm/aaaa" y "aaaa-mm-dd" mezcladas), rankings, evolucion por anio,
 * distribucion por dia/franja y escape de los textos del usuario en HTML y
 * Markdown. Usa un perfil propio (mifutbol_test_resumen_general.db en el
 * directorio de datos) y lo borra al terminar.
 */

#include "unity/unity.h"
#include "db.h"
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

#define PERFIL_TEST "test_resumen_general"
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
    int rendimiento;
    int camiseta;
    int cancha;
    const char *rival;
    int minutos;
    int dia;
    int cansancio;
} PartidoPrueba;

static void insertar_partido(const PartidoPrueba *p)
{
    char sql[640];
    snprintf(sql, sizeof(sql),
             "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, "
             "mes_anio, resultado, rendimiento_general, rival_nombre, minutos_jugados, dia, "
             "estado_animo, cansancio) "
             "VALUES(%d, %d, '%s', %d, %d, %d, '', %d, %d, '%s', %d, %d, 8, %d);",
             p->id, p->cancha, p->fecha, p->goles, p->asistencias, p->camiseta, p->resultado,
             p->rendimiento, p->rival, p->minutos, p->dia, p->cansancio);
    exec_ok(sql);
}

/*
 * Los ids NO siguen la fecha y los formatos estan mezclados. Orden real:
 *  id fecha              G A res rend cam can rival   min dia
 *   5 01/01/2025 10:00   1 0  V   6    1   1  Lobos     0  2
 *   1 2025-01-08 21:00   2 1  V   8    1   1  Lobos    60  6
 *   4 15/01/2025 20:00   3 0  V   9    1   2  Tigres   90  6
 *   2 2025-02-01 18:00   0 0  D   4    2   2  Tigres    0  4
 *   3 03/02/2025 19:00   0 2  E   7    1   1  Lobos     0  5
 *   6 2026-03-10 20:00   1 1  V   -    2   1  (vacio)   0  6
 *   7 2026-03-10 22:00   0 0  -   5    2   1  Lobos     0  6
 */
static void cargar_datos(void)
{
    static const PartidoPrueba partidos[] =
    {
        {1, "2025-01-08 21:00", 2, 1, 1, 8, 1, 1, "Lobos", 60, 6, 6},
        {2, "2025-02-01 18:00", 0, 0, 3, 4, 2, 2, "Tigres", 0, 4, 0},
        {3, "03/02/2025 19:00", 0, 2, 2, 7, 1, 1, "Lobos", 0, 5, 0},
        {4, "15/01/2025 20:00", 3, 0, 1, 9, 1, 2, "Tigres", 90, 6, 0},
        {5, "01/01/2025 10:00", 1, 0, 1, 6, 1, 1, "Lobos", 0, 2, 0},
        {6, "2026-03-10 20:00", 1, 1, 1, 0, 2, 1, "", 0, 6, 0},
        {7, "2026-03-10 22:00", 0, 0, 0, 5, 2, 1, "Lobos", 0, 6, 0},
    };

    exec_ok("INSERT INTO camiseta(id, nombre) VALUES(1, 'Roja <b>&\"x\"|');");
    exec_ok("INSERT INTO camiseta(id, nombre) VALUES(2, 'Azul');");
    exec_ok("INSERT INTO cancha(id, nombre) VALUES(1, 'Cancha A');");
    exec_ok("INSERT INTO cancha(id, nombre) VALUES(2, 'Cancha B');");
    for (size_t i = 0; i < sizeof(partidos) / sizeof(partidos[0]); i++)
    {
        insertar_partido(&partidos[i]);
    }
}

/* Escribe el resumen con el escritor pedido y devuelve el texto (malloc). */
static char *escribir_a_texto(const ResumenGeneral *r, int es_markdown)
{
    FILE *f = fopen(g_out_path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    if (es_markdown)
    {
        resumen_general_escribir_markdown(f, r);
    }
    else
    {
        resumen_general_escribir_html(f, r);
    }
    fclose(f);

    f = fopen(g_out_path, "rb");
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

static void test_totales_y_efectividad(void)
{
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    TEST_ASSERT_EQUAL_INT(7, r.partidos);
    TEST_ASSERT_EQUAL_INT(7, r.goles);
    TEST_ASSERT_EQUAL_INT(4, r.asistencias);
    TEST_ASSERT_EQUAL_INT(4, r.victorias);
    TEST_ASSERT_EQUAL_INT(1, r.empates);
    TEST_ASSERT_EQUAL_INT(1, r.derrotas);
    TEST_ASSERT_EQUAL_INT(6, r.con_resultado);
    /* (3 * 4 + 1) / (3 * 6): el partido sin resultado no cuenta. */
    ASSERT_CERCA(13.0 * 100.0 / 18.0, r.efectividad_pct);
    ASSERT_CERCA(11.0 / 7.0, r.ga_pp);
    TEST_ASSERT_EQUAL_STRING("01/01/2025", r.primer_partido_fecha);
}

static void test_ataque_y_minutos(void)
{
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    TEST_ASSERT_EQUAL_INT(4, r.partidos_con_gol);
    TEST_ASSERT_EQUAL_INT(1, r.dobletes);
    TEST_ASSERT_EQUAL_INT(1, r.hat_tricks);
    TEST_ASSERT_EQUAL_INT(2, r.partidos_con_minutos);
    TEST_ASSERT_EQUAL_INT(150, r.minutos_total);
    /* G+A solo de los partidos con minutos: (2+1) + (3+0) = 6 en 150'. */
    ASSERT_CERCA(3.6, r.ga_por_90);
    TEST_ASSERT_EQUAL_INT(6, r.con_rendimiento);
    ASSERT_CERCA(6.5, r.rendimiento_prom);
    TEST_ASSERT_EQUAL_INT(1, r.con_cansancio);
    ASSERT_CERCA(6.0, r.cansancio_prom);
}

static void test_rachas_y_records(void)
{
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    TEST_ASSERT_EQUAL_INT(3, r.mejor_racha_victorias);
    TEST_ASSERT_EQUAL_INT(3, r.mejor_racha_invicto);
    TEST_ASSERT_EQUAL_INT(3, r.mejor_racha_marcando);
    TEST_ASSERT_EQUAL_INT(0, r.racha_victorias_actual);
    /* Ultimo partido (id 7) sin goles ni asistencias: racha negativa de 1. */
    TEST_ASSERT_EQUAL_INT('N', r.racha_tipo);
    TEST_ASSERT_EQUAL_INT(1, r.racha_longitud);

    TEST_ASSERT_EQUAL_INT(4, r.max_goles.id);
    TEST_ASSERT_EQUAL_STRING("15/01/2025", r.max_goles.fecha);
    TEST_ASSERT_EQUAL_STRING("Tigres", r.max_goles.rival);
    TEST_ASSERT_EQUAL_INT(3, r.max_asistencias.id);
    TEST_ASSERT_EQUAL_INT(4, r.mejor_rendimiento.id);
    TEST_ASSERT_EQUAL_INT(7, r.ultimo.id);
}

static void test_ultimos_en_orden_por_fecha(void)
{
    static const int esperado[] = {5, 1, 4, 2, 3, 6, 7};
    static const int tendencia[] = {6, 8, 9, 4, 7, 5};
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    TEST_ASSERT_EQUAL_INT(7, r.ultimos_n);
    for (int i = 0; i < 7; i++)
    {
        TEST_ASSERT_EQUAL_INT(esperado[i], r.ultimos[i].id);
    }
    /* La tendencia omite el partido sin rendimiento (id 6). */
    TEST_ASSERT_EQUAL_INT(6, r.tendencia_n);
    for (int i = 0; i < 6; i++)
    {
        TEST_ASSERT_EQUAL_INT(tendencia[i], r.tendencia[i]);
    }
}

static void test_rankings_top(void)
{
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    TEST_ASSERT_EQUAL_INT(2, r.camisetas.cantidad);
    TEST_ASSERT_EQUAL_STRING("Roja <b>&\"x\"|", r.camisetas.items[0].nombre);
    TEST_ASSERT_EQUAL_INT(4, r.camisetas.items[0].partidos);
    ASSERT_CERCA(75.0, r.camisetas.items[0].victorias_pct);
    TEST_ASSERT_EQUAL_INT(3, r.camisetas.items[1].partidos);
    ASSERT_CERCA(50.0, r.camisetas.items[1].victorias_pct);

    TEST_ASSERT_EQUAL_INT(2, r.canchas.cantidad);
    TEST_ASSERT_EQUAL_STRING("Cancha A", r.canchas.items[0].nombre);
    TEST_ASSERT_EQUAL_INT(5, r.canchas.items[0].partidos);

    /* El rival vacio no entra al ranking. */
    TEST_ASSERT_EQUAL_INT(2, r.rivales.cantidad);
    TEST_ASSERT_EQUAL_STRING("Lobos", r.rivales.items[0].nombre);
    TEST_ASSERT_EQUAL_INT(4, r.rivales.items[0].partidos);
    TEST_ASSERT_EQUAL_INT(3, r.rivales.items[0].con_resultado);
    ASSERT_CERCA(200.0 / 3.0, r.rivales.items[0].victorias_pct);
}

static void test_anios_dias_y_franjas(void)
{
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    TEST_ASSERT_EQUAL_INT(2, r.num_anios);
    TEST_ASSERT_EQUAL_INT(2025, r.anios[0].anio);
    TEST_ASSERT_EQUAL_INT(5, r.anios[0].partidos);
    TEST_ASSERT_EQUAL_INT(6, r.anios[0].goles);
    TEST_ASSERT_EQUAL_INT(3, r.anios[0].asistencias);
    TEST_ASSERT_EQUAL_INT(3, r.anios[0].victorias);
    TEST_ASSERT_EQUAL_INT(2026, r.anios[1].anio);
    TEST_ASSERT_EQUAL_INT(2, r.anios[1].partidos);

    /* 01, 08 y 15/01/2025 fueron miercoles; 01/02/2025 sabado; 03/02/2025
     * lunes; 10/03/2026 martes. */
    TEST_ASSERT_EQUAL_INT(3, r.dias_semana[3].partidos);
    TEST_ASSERT_EQUAL_INT(3, r.dias_semana[3].victorias);
    TEST_ASSERT_EQUAL_INT(1, r.dias_semana[6].partidos);
    TEST_ASSERT_EQUAL_INT(1, r.dias_semana[1].partidos);
    TEST_ASSERT_EQUAL_INT(2, r.dias_semana[2].partidos);
    TEST_ASSERT_EQUAL_INT(0, r.dias_semana[0].partidos);

    /* Franja = columna dia (1..6) - 1. */
    TEST_ASSERT_EQUAL_INT(4, r.franjas[5].partidos);
    TEST_ASSERT_EQUAL_INT(1, r.franjas[1].partidos);
    TEST_ASSERT_EQUAL_INT(0, r.franjas[0].partidos);
}

static void test_html_escapa_textos_del_usuario(void)
{
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    char *html = escribir_a_texto(&r, 0);
    TEST_ASSERT_NOT_NULL(strstr(html, "Roja &lt;b&gt;&amp;&quot;x&quot;|"));
    TEST_ASSERT_NULL(strstr(html, "Roja <b>"));
    /* Autocontenido: sin recursos externos ni scripts. */
    TEST_ASSERT_NULL(strstr(html, "http://"));
    TEST_ASSERT_NULL(strstr(html, "https://"));
    TEST_ASSERT_NULL(strstr(html, "<script"));
    TEST_ASSERT_NOT_NULL(strstr(html, "prefers-color-scheme: dark"));
    TEST_ASSERT_NOT_NULL(strstr(html, "@media print"));
    TEST_ASSERT_NOT_NULL(strstr(html, "class=\"donut\""));
    TEST_ASSERT_NOT_NULL(strstr(html, "class=\"spark\""));
    TEST_ASSERT_NOT_NULL(strstr(html, "</html>"));
    free(html);
}

static void test_markdown_escapa_barras_y_no_usa_html(void)
{
    cargar_datos();
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));

    char *md = escribir_a_texto(&r, 1);
    TEST_ASSERT_NOT_NULL(strstr(md, "Roja <b>&\"x\"\\|"));
    TEST_ASSERT_NOT_NULL(strstr(md, "## Resultados"));
    TEST_ASSERT_NOT_NULL(strstr(md, "| Victorias | **4** |"));
    TEST_ASSERT_NULL(strstr(md, "<div"));
    TEST_ASSERT_NULL(strstr(md, "&#"));
    free(md);
}

static void test_base_sin_partidos(void)
{
    ResumenGeneral r;
    TEST_ASSERT_EQUAL_INT(1, resumen_general_calcular(&r));
    TEST_ASSERT_EQUAL_INT(0, r.partidos);
    TEST_ASSERT_EQUAL_INT(0, r.ultimos_n);
    TEST_ASSERT_EQUAL_INT(0, r.num_anios);
    TEST_ASSERT_EQUAL_INT(0, r.camisetas.cantidad);
    ASSERT_CERCA(0.0, r.efectividad_pct);

    char *html = escribir_a_texto(&r, 0);
    TEST_ASSERT_NOT_NULL(strstr(html, "no hay partidos"));
    TEST_ASSERT_NULL(strstr(html, "class=\"spark\""));
    TEST_ASSERT_NULL(strstr(html, "class=\"donut\""));
    free(html);
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
    snprintf(g_out_path, sizeof(g_out_path), "%s" TEST_SEP "%s_salida.txt", data_dir,
             PERFIL_TEST);
    db_close();

    UNITY_BEGIN();

    RUN_TEST(test_totales_y_efectividad);
    RUN_TEST(test_ataque_y_minutos);
    RUN_TEST(test_rachas_y_records);
    RUN_TEST(test_ultimos_en_orden_por_fecha);
    RUN_TEST(test_rankings_top);
    RUN_TEST(test_anios_dias_y_franjas);
    RUN_TEST(test_html_escapa_textos_del_usuario);
    RUN_TEST(test_markdown_escapa_barras_y_no_usa_html);
    RUN_TEST(test_base_sin_partidos);

    int fallos = UNITY_END();
    remove(g_log_path);
    return fallos;
}
