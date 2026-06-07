#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "equipo.h"
#include "menu.h"
#include <string.h>

void setUp(void) { /* No setup needed */ }
void tearDown(void) { /* No cleanup needed */ }

static void test_get_nombre_posicion_arquero(void)
{
    TEST_ASSERT_EQUAL_STRING("Arquero", get_nombre_posicion(ARQUERO));
}

static void test_get_nombre_posicion_defensor(void)
{
    TEST_ASSERT_EQUAL_STRING("Defensor", get_nombre_posicion(DEFENSOR));
}

static void test_get_nombre_posicion_mediocampista(void)
{
    TEST_ASSERT_EQUAL_STRING("Mediocampista", get_nombre_posicion(MEDIOCAMPISTA));
}

static void test_get_nombre_posicion_delantero(void)
{
    TEST_ASSERT_EQUAL_STRING("Delantero", get_nombre_posicion(DELANTERO));
}

static void test_get_nombre_posicion_desconocido(void)
{
    TEST_ASSERT_EQUAL_STRING("Desconocido", get_nombre_posicion(99));
}

static void test_get_nombre_tipo_futbol_5(void)
{
    TEST_ASSERT_EQUAL_STRING("Futbol 5", get_nombre_tipo_futbol(FUTBOL_5));
}

static void test_get_nombre_tipo_futbol_7(void)
{
    TEST_ASSERT_EQUAL_STRING("Futbol 7", get_nombre_tipo_futbol(FUTBOL_7));
}

static void test_get_nombre_tipo_futbol_8(void)
{
    TEST_ASSERT_EQUAL_STRING("Futbol 8", get_nombre_tipo_futbol(FUTBOL_8));
}

static void test_get_nombre_tipo_futbol_11(void)
{
    TEST_ASSERT_EQUAL_STRING("Futbol 11", get_nombre_tipo_futbol(FUTBOL_11));
}

static void test_get_nombre_tipo_futbol_desconocido(void)
{
    TEST_ASSERT_EQUAL_STRING("Desconocido", get_nombre_tipo_futbol(99));
}

static void test_equipo_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(crear_equipo);
    TEST_ASSERT_NOT_NULL(listar_equipos);
    TEST_ASSERT_NOT_NULL(modificar_equipo);
    TEST_ASSERT_NOT_NULL(eliminar_equipo);
    TEST_ASSERT_NOT_NULL(mostrar_equipo);
    TEST_ASSERT_NOT_NULL(imprimir_alineacion_equipo);
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

static void test_menu_equipos_smoke(void)
{
    assert_menu_exec(&menu_equipos);
}

static void test_menu_equipos_has_options(void)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    menu_equipos();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad >= 4);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_get_nombre_posicion_arquero);
    RUN_TEST(test_get_nombre_posicion_defensor);
    RUN_TEST(test_get_nombre_posicion_mediocampista);
    RUN_TEST(test_get_nombre_posicion_delantero);
    RUN_TEST(test_get_nombre_posicion_desconocido);
    RUN_TEST(test_get_nombre_tipo_futbol_5);
    RUN_TEST(test_get_nombre_tipo_futbol_7);
    RUN_TEST(test_get_nombre_tipo_futbol_8);
    RUN_TEST(test_get_nombre_tipo_futbol_11);
    RUN_TEST(test_get_nombre_tipo_futbol_desconocido);
    RUN_TEST(test_equipo_functions_exist);
    RUN_TEST(test_menu_equipos_smoke);
    RUN_TEST(test_menu_equipos_has_options);

    return UNITY_END();
}
