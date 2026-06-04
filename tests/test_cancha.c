#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "cancha.h"
#include "menu.h"

void setUp(void) { }
void tearDown(void) { }

static void test_cancha_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_cancha);
    TEST_ASSERT_NOT_NULL(listar_canchas);
    TEST_ASSERT_NOT_NULL(modificar_cancha);
    TEST_ASSERT_NOT_NULL(eliminar_cancha);
    TEST_ASSERT_NOT_NULL(cargar_imagen_cancha);
    TEST_ASSERT_NOT_NULL(ver_imagen_cancha);
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

static void test_menu_canchas_smoke(void)
{
    assert_menu_exec(&menu_canchas);
}

static void test_menu_canchas_has_options(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_canchas();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 4);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_menu_canchas_titulo_not_empty(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_canchas();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.titulo[0] != '\0');
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_cancha_functions_exist);
    RUN_TEST(test_menu_canchas_smoke);
    RUN_TEST(test_menu_canchas_has_options);
    RUN_TEST(test_menu_canchas_titulo_not_empty);

    return UNITY_END();
}
