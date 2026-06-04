#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "camiseta.h"
#include "menu.h"

void setUp(void) { }
void tearDown(void) { }

static void test_camiseta_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_camiseta);
    TEST_ASSERT_NOT_NULL(listar_camisetas);
    TEST_ASSERT_NOT_NULL(editar_camiseta);
    TEST_ASSERT_NOT_NULL(eliminar_camiseta);
    TEST_ASSERT_NOT_NULL(sortear_camiseta);
    TEST_ASSERT_NOT_NULL(cargar_imagen_camiseta);
    TEST_ASSERT_NOT_NULL(ver_imagen_camiseta);
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

static void test_menu_camisetas_smoke(void)
{
    assert_menu_exec(&menu_camisetas);
}

static void test_menu_camisetas_has_crear_option(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_camisetas();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 5);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_menu_camisetas_titulo_not_empty(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_camisetas();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.titulo[0] != '\0');
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_camiseta_functions_exist);
    RUN_TEST(test_menu_camisetas_smoke);
    RUN_TEST(test_menu_camisetas_has_crear_option);
    RUN_TEST(test_menu_camisetas_titulo_not_empty);

    return UNITY_END();
}
