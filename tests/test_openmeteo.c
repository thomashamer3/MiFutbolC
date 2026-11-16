#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "openmeteo_client.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <Windows.h>
#endif

void setUp(void)
{
    openmeteo_set_silencioso(1);
}

void tearDown(void)
{
    openmeteo_set_silencioso(0);
}

/* Unity se compila sin soporte de double: se compara con tolerancia a mano. */
#define ASSERT_CERCA(esperado, real)                                                     \
    TEST_ASSERT_TRUE_MESSAGE(((real) - (esperado)) < 0.001 && ((esperado) - (real)) < 0.001, \
                             #real)

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

static const char *JSON_SIN_TEMPERATURAS =
    "{\"hourly\":{"
    "\"time\":[\"2026-11-15T00:00\",\"2026-11-15T01:00\"],"
    "\"temperature_2m\":[null,null],"
    "\"apparent_temperature\":[null,null],"
    "\"precipitation\":[null,null],"
    "\"weather_code\":[null,null],"
    "\"wind_speed_10m\":[null,null]},"
    "\"daily\":{\"sunrise\":[\"2026-11-15T05:40\"],\"sunset\":[\"2026-11-15T19:50\"]}}";

static void test_parse_con_datos(void)
{
    OpenMeteoResult r;
    TEST_ASSERT_EQUAL_INT(1, openmeteo_parse(JSON_CON_DATOS, &r));
    ASSERT_CERCA(20.0, r.temp_c);
    ASSERT_CERCA(10.0, r.apparent_temp_c);
    ASSERT_CERCA(2.0, r.precip_mm);
    ASSERT_CERCA(20.0, r.wind_kmh);
    TEST_ASSERT_EQUAL_INT(61, r.weather_code);
    TEST_ASSERT_EQUAL_INT(1, r.has_sun_data);
    TEST_ASSERT_EQUAL_INT(6, r.sunrise_hour);
    TEST_ASSERT_EQUAL_INT(15, r.sunrise_minute);
    TEST_ASSERT_EQUAL_INT(19, r.sunset_hour);
    TEST_ASSERT_EQUAL_INT(40, r.sunset_minute);
    TEST_ASSERT_NOT_NULL(r.clima_json);
    TEST_ASSERT_NOT_NULL(strstr(r.clima_json, "temperature_2m"));
    openmeteo_result_free(&r);
    TEST_ASSERT_NULL(r.clima_json);
}

static void test_parse_sin_temperaturas_devuelve_cero(void)
{
    OpenMeteoResult r;
    TEST_ASSERT_EQUAL_INT(0, openmeteo_parse(JSON_SIN_TEMPERATURAS, &r));
    TEST_ASSERT_NULL(r.clima_json);
    openmeteo_result_free(&r);
}

static void test_parse_json_invalido_devuelve_cero(void)
{
    OpenMeteoResult r;
    TEST_ASSERT_EQUAL_INT(0, openmeteo_parse("{\"hourly\": [1, 2", &r));
    TEST_ASSERT_NULL(r.clima_json);
    TEST_ASSERT_EQUAL_INT(0, openmeteo_parse("", &r));
    TEST_ASSERT_EQUAL_INT(0, openmeteo_parse(NULL, &r));
    TEST_ASSERT_EQUAL_INT(0, openmeteo_parse("{\"error\":true,\"reason\":\"x\"}", &r));
}

static void test_build_url(void)
{
    OpenMeteoParams p;
    char url[1536];

    memset(&p, 0, sizeof(p));
    p.latitud = -34.6;
    p.longitud = -58.4;
    p.anio = 2025;
    p.mes = 1;
    p.dia = 2;
    TEST_ASSERT_EQUAL_INT(1, openmeteo_build_url(&p, url, sizeof(url)));
    TEST_ASSERT_NOT_NULL(strstr(url, "start_date=2025-01-02&end_date=2025-01-02"));
    TEST_ASSERT_NOT_NULL(strstr(url, "archive-api.open-meteo.com"));

    /* Buffer chico: no trunca en silencio. */
    TEST_ASSERT_EQUAL_INT(0, openmeteo_build_url(&p, url, 16));

    p.latitud = 95.0;
    TEST_ASSERT_EQUAL_INT(0, openmeteo_build_url(&p, url, sizeof(url)));

    p.latitud = -34.6;
    p.anio = 2100;
    TEST_ASSERT_EQUAL_INT(1, openmeteo_fecha_es_futura(&p));
    TEST_ASSERT_EQUAL_INT(0, openmeteo_build_url(&p, url, sizeof(url)));
}

static void esperar_un_poco(void)
{
#ifdef _WIN32
    Sleep(20);
#else
    struct timespec ts = {0, 20000000L};
    nanosleep(&ts, NULL);
#endif
}

static void test_proceso_asincrono_codigo_salida(void)
{
#ifdef _WIN32
    const char *argv[] = {"cmd.exe", "/c", "exit 3", NULL};
#else
    const char *argv[] = {"sh", "-c", "exit 3", NULL};
#endif
    AppProceso p;
    int codigo = -1;
    int estado = 0;
    time_t inicio = time(NULL);

    TEST_ASSERT_EQUAL_INT(1, app_proceso_iniciar(argv, &p));
    while ((estado = app_proceso_estado(&p, &codigo)) == 0 && difftime(time(NULL), inicio) < 15)
    {
        esperar_un_poco();
    }
    TEST_ASSERT_EQUAL_INT(1, estado);
    TEST_ASSERT_EQUAL_INT(3, codigo);
    app_proceso_terminar(&p);
}

static void test_proceso_inexistente_falla(void)
{
    const char *argv[] = {"programa_que_no_existe_mifutbolc_xyz", NULL};
    AppProceso p;
    int codigo = 0;
    TEST_ASSERT_EQUAL_INT(0, app_proceso_iniciar(argv, &p));
    TEST_ASSERT_EQUAL_INT(-1, app_proceso_estado(&p, &codigo));
}

static void test_ruta_temporal_y_lectura(void)
{
    char ruta[1024];
    FILE *f = NULL;
    size_t tam = 0;

    TEST_ASSERT_EQUAL_INT(1, app_ruta_temporal(ruta, sizeof(ruta)));
    TEST_ASSERT_TRUE(fopen_s(&f, ruta, "wb") == 0 && f != NULL);
    fputs("{\"a\":1}", f);
    fclose(f);

    char *datos = app_leer_archivo_completo(ruta, &tam);
    TEST_ASSERT_NOT_NULL(datos);
    TEST_ASSERT_EQUAL_size_t(7, tam);
    TEST_ASSERT_EQUAL_STRING("{\"a\":1}", datos);
    free(datos);
    remove(ruta);
    TEST_ASSERT_NULL(app_leer_archivo_completo(ruta, &tam));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_con_datos);
    RUN_TEST(test_parse_sin_temperaturas_devuelve_cero);
    RUN_TEST(test_parse_json_invalido_devuelve_cero);
    RUN_TEST(test_build_url);
    RUN_TEST(test_proceso_asincrono_codigo_salida);
    RUN_TEST(test_proceso_inexistente_falla);
    RUN_TEST(test_ruta_temporal_y_lectura);
    return UNITY_END();
}
