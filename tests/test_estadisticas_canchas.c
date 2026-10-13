#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "estadisticas_canchas.h"
#include "menu.h"
#include "utils.h"

void setUp(void) { /* No setup needed */ }
void tearDown(void) { /* No cleanup needed */ }

static void test_estadisticas_canchas_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(menu_estadisticas_canchas);
    TEST_ASSERT_NOT_NULL(mostrar_resumen_canchas);
    TEST_ASSERT_NOT_NULL(mostrar_ranking_canchas_mas_usadas);
    TEST_ASSERT_NOT_NULL(mostrar_canchas_menos_usadas);
    TEST_ASSERT_NOT_NULL(mostrar_rendimiento_por_cancha);
    TEST_ASSERT_NOT_NULL(mostrar_goles_asistencias_por_cancha);
    TEST_ASSERT_NOT_NULL(mostrar_rendimiento_por_tipo_cancha);
    TEST_ASSERT_NOT_NULL(mostrar_rendimiento_por_superficie);
    TEST_ASSERT_NOT_NULL(mostrar_canchas_techadas_vs_descubiertas);
    TEST_ASSERT_NOT_NULL(mostrar_partidos_por_localidad);
    TEST_ASSERT_NOT_NULL(mostrar_partidos_por_franja_horaria);
}

typedef void (*MenuFunc)(void);

static void assert_menu_exec(MenuFunc func)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    func();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad > 0);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_menu_estadisticas_canchas_smoke(void)
{
    assert_menu_exec(&menu_estadisticas_canchas);
}

static void test_menu_estadisticas_canchas_has_options(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_estadisticas_canchas();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 6);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_listado_paginado_config_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(0, listado_paginado_config(NULL));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_estadisticas_canchas_functions_exist);
    RUN_TEST(test_menu_estadisticas_canchas_smoke);
    RUN_TEST(test_menu_estadisticas_canchas_has_options);
    RUN_TEST(test_listado_paginado_config_rejects_null);

    return UNITY_END();
}
