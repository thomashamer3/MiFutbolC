#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "lesion.h"
#include "menu.h"

void setUp(void) { /* No setup needed */ }
void tearDown(void) { /* No cleanup needed */ }

static void test_lesion_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_lesion);
    TEST_ASSERT_NOT_NULL(listar_lesiones);
    TEST_ASSERT_NOT_NULL(modificar_lesion);
    TEST_ASSERT_NOT_NULL(eliminar_lesion);
    TEST_ASSERT_NOT_NULL(mostrar_estadisticas_lesiones);
    TEST_ASSERT_NOT_NULL(mostrar_diferencias_lesiones);
    TEST_ASSERT_NOT_NULL(actualizar_estados_lesiones);
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

static void test_menu_lesiones_smoke(void)
{
    assert_menu_exec(&menu_lesiones);
}

static void test_menu_lesiones_has_options(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_lesiones();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 4);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_menu_lesiones_titulo_not_empty(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_lesiones();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.titulo[0] != '\0');
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_lesion_functions_exist);
    RUN_TEST(test_menu_lesiones_smoke);
    RUN_TEST(test_menu_lesiones_has_options);
    RUN_TEST(test_menu_lesiones_titulo_not_empty);

    return UNITY_END();
}
