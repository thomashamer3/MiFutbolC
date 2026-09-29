#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "botin.h"
#include "menu.h"

void setUp(void) { /* No setup needed */ }
void tearDown(void) { /* No cleanup needed */ }

static void test_botin_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_botin);
    TEST_ASSERT_NOT_NULL(listar_botines);
    TEST_ASSERT_NOT_NULL(editar_botin);
    TEST_ASSERT_NOT_NULL(eliminar_botin);
    TEST_ASSERT_NOT_NULL(sortear_botin);
    TEST_ASSERT_NOT_NULL(cargar_imagen_botin);
    TEST_ASSERT_NOT_NULL(ver_imagen_botin);
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

static void test_menu_botines_smoke(void)
{
    assert_menu_exec(&menu_botines);
}

static void test_menu_botines_has_sortear_option(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_botines();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 5);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_submenu_sorteo_botin_tiene_reiniciar(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    sortear_botin();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_EQUAL_STRING("SORTEO DE BOTINES", capture.titulo);
    TEST_ASSERT_EQUAL_INT(3, capture.cantidad);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
    TEST_ASSERT_EQUAL_INT(0, capture.last_item.opcion);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_botin_functions_exist);
    RUN_TEST(test_menu_botines_smoke);
    RUN_TEST(test_menu_botines_has_sortear_option);
    RUN_TEST(test_submenu_sorteo_botin_tiene_reiniciar);

    return UNITY_END();
}
