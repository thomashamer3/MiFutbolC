#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "torneo.h"
#include "menu.h"
#include <string.h>

void setUp(void) { /* No setup needed */ }
void tearDown(void) { /* No cleanup needed */ }

static void test_get_nombre_tipo_torneo_ida_vuelta(void)
{
    TEST_ASSERT_EQUAL_STRING("Ida y Vuelta", get_nombre_tipo_torneo(IDA_Y_VUELTA));
}

static void test_get_nombre_tipo_torneo_solo_ida(void)
{
    TEST_ASSERT_EQUAL_STRING("Solo Ida", get_nombre_tipo_torneo(SOLO_IDA));
}

static void test_get_nombre_tipo_torneo_eliminacion_directa(void)
{
    TEST_ASSERT_EQUAL_STRING("Eliminacion Directa", get_nombre_tipo_torneo(ELIMINACION_DIRECTA));
}

static void test_get_nombre_tipo_torneo_grupos_eliminacion(void)
{
    TEST_ASSERT_EQUAL_STRING("Grupos y Eliminacion", get_nombre_tipo_torneo(GRUPOS_Y_ELIMINACION));
}

static void test_get_nombre_tipo_torneo_desconocido(void)
{
    TEST_ASSERT_EQUAL_STRING("Desconocido", get_nombre_tipo_torneo(TIPO_TORNEO_INVALIDO));
}

static void test_get_nombre_formato_torneo_round_robin(void)
{
    TEST_ASSERT_EQUAL_STRING("Round-robin (sistema liga)", get_nombre_formato_torneo(ROUND_ROBIN));
}

static void test_get_nombre_formato_torneo_mini_grupo_final(void)
{
    TEST_ASSERT_EQUAL_STRING("Mini grupo con final", get_nombre_formato_torneo(MINI_GRUPO_CON_FINAL));
}

static void test_get_nombre_formato_torneo_eliminacion_fases(void)
{
    TEST_ASSERT_EQUAL_STRING("Eliminacion directa por fases", get_nombre_formato_torneo(ELIMINACION_FASES));
}

static void test_get_nombre_formato_torneo_desconocido(void)
{
    TEST_ASSERT_EQUAL_STRING("Desconocido", get_nombre_formato_torneo(FORMATO_TORNEO_INVALIDO));
}

static void test_torneo_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_torneo);
    TEST_ASSERT_NOT_NULL(listar_torneos);
    TEST_ASSERT_NOT_NULL(modificar_torneo);
    TEST_ASSERT_NOT_NULL(eliminar_torneo);
    TEST_ASSERT_NOT_NULL(mostrar_torneo);
    TEST_ASSERT_NOT_NULL(administrar_torneo);
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

static void test_menu_torneos_smoke(void)
{
    assert_menu_exec(&menu_torneos);
}

static void test_menu_torneos_has_options(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_torneos();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 4);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_get_nombre_tipo_torneo_ida_vuelta);
    RUN_TEST(test_get_nombre_tipo_torneo_solo_ida);
    RUN_TEST(test_get_nombre_tipo_torneo_eliminacion_directa);
    RUN_TEST(test_get_nombre_tipo_torneo_grupos_eliminacion);
    RUN_TEST(test_get_nombre_tipo_torneo_desconocido);
    RUN_TEST(test_get_nombre_formato_torneo_round_robin);
    RUN_TEST(test_get_nombre_formato_torneo_mini_grupo_final);
    RUN_TEST(test_get_nombre_formato_torneo_eliminacion_fases);
    RUN_TEST(test_get_nombre_formato_torneo_desconocido);
    RUN_TEST(test_torneo_functions_exist);
    RUN_TEST(test_menu_torneos_smoke);
    RUN_TEST(test_menu_torneos_has_options);

    return UNITY_END();
}
