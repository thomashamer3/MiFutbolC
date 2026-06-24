#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "temporada.h"
#include "menu.h"
#include <string.h>

void setUp(void) { /* No setup needed */ }
void tearDown(void) { /* No cleanup needed */ }

static void test_get_nombre_tipo_fase_pretemporada(void)
{
    TEST_ASSERT_EQUAL_STRING("Pretemporada", get_nombre_tipo_fase(PRETEMPORADA));
}

static void test_get_nombre_tipo_fase_temporada_regular(void)
{
    TEST_ASSERT_EQUAL_STRING("Temporada Regular", get_nombre_tipo_fase(TEMPORADA_REGULAR));
}

static void test_get_nombre_tipo_fase_postemporada(void)
{
    TEST_ASSERT_EQUAL_STRING("Postemporada", get_nombre_tipo_fase(POSTTEMPORADA));
}

static void test_get_nombre_tipo_fase_desconocido(void)
{
    TEST_ASSERT_EQUAL_STRING("Desconocido", get_nombre_tipo_fase(TIPO_FASE_INVALIDA));
}

static void test_temporada_struct_sizes(void)
{
    Temporada t;
    memset(&t, 0, sizeof(t));
    TEST_ASSERT_EQUAL_INT(0, t.id);
    TEST_ASSERT_EQUAL_INT(0, t.anio);
}

static void test_temporada_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_temporada);
    TEST_ASSERT_NOT_NULL(listar_temporadas);
    TEST_ASSERT_NOT_NULL(modificar_temporada);
    TEST_ASSERT_NOT_NULL(eliminar_temporada);
    TEST_ASSERT_NOT_NULL(administrar_temporada);
    TEST_ASSERT_NOT_NULL(get_temporada_actual_id);
    TEST_ASSERT_NOT_NULL(calcular_fatiga_equipo);
    TEST_ASSERT_NOT_NULL(calcular_fatiga_jugador);
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

static void test_menu_temporadas_smoke(void)
{
    assert_menu_exec(&menu_temporadas);
}

static void test_menu_temporadas_has_options(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_temporadas();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 4);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_get_nombre_tipo_fase_pretemporada);
    RUN_TEST(test_get_nombre_tipo_fase_temporada_regular);
    RUN_TEST(test_get_nombre_tipo_fase_postemporada);
    RUN_TEST(test_get_nombre_tipo_fase_desconocido);
    RUN_TEST(test_temporada_struct_sizes);
    RUN_TEST(test_temporada_functions_exist);
    RUN_TEST(test_menu_temporadas_smoke);
    RUN_TEST(test_menu_temporadas_has_options);

    return UNITY_END();
}
