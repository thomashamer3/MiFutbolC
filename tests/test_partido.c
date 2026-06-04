#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "partido.h"
#include "menu.h"

void setUp(void) { }
void tearDown(void) { }

static void test_partido_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_partido);
    TEST_ASSERT_NOT_NULL(listar_partidos);
    TEST_ASSERT_NOT_NULL(modificar_partido);
    TEST_ASSERT_NOT_NULL(eliminar_partido);
    TEST_ASSERT_NOT_NULL(buscar_partidos);
    TEST_ASSERT_NOT_NULL(menu_tacticas_partido);
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

static void test_menu_partidos_smoke(void)
{
    assert_menu_exec(&menu_partidos);
}

static void test_menu_partidos_has_options(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_partidos();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 4);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_menu_partidos_titulo_not_empty(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_partidos();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.titulo[0] != '\0');
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_partido_functions_exist);
    RUN_TEST(test_menu_partidos_smoke);
    RUN_TEST(test_menu_partidos_has_options);
    RUN_TEST(test_menu_partidos_titulo_not_empty);

    return UNITY_END();
}
