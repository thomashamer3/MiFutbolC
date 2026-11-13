#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "resumen_compartible.h"
#include "menu.h"

void setUp(void)
{
    /* Sin preparacion previa */
}

void tearDown(void)
{
    /* Sin limpieza posterior */
}

/**
 * Comprueba que el modulo exporta todas las salidas del resumen.
 */
static void test_resumen_compartible_functions_exist(void)
{
    TEST_ASSERT_NOT_NULL(generar_resumen_html);
    TEST_ASSERT_NOT_NULL(generar_resumen_markdown);
    TEST_ASSERT_NOT_NULL(generar_resumen_estadisticas);
    TEST_ASSERT_NOT_NULL(generar_resumen_mejor_temporada);
    TEST_ASSERT_NOT_NULL(generar_resumen_anio_txt);
    TEST_ASSERT_NOT_NULL(generar_resumen_anio_html);
    TEST_ASSERT_NOT_NULL(generar_resumen_anio_pdf);
    TEST_ASSERT_NOT_NULL(generar_resumen_anio_todos);
}

/**
 * El menu debe ofrecer cinco acciones y terminar en "Volver".
 */
static void test_menu_resumen_compartible_tiene_resumen_anio(void)
{
    MenuTestCapture capture = {0};

    menu_test_set_capture(&capture);

    menu_resumen_compartible();

    menu_test_set_capture(NULL);

    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_EQUAL_STRING("RESUMEN COMPARTIBLE", capture.titulo);
    TEST_ASSERT_EQUAL_INT(6, capture.cantidad);
    TEST_ASSERT_EQUAL_INT(0, capture.last_item.opcion);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

/**
 * La opcion 22 del menu principal debe seguir apuntando al modulo.
 */
static void test_menu_principal_tiene_resumen_compartible(void)
{
    const MenuItem *items = menu_get_items();
    const MenuItem *item = NULL;

    TEST_ASSERT_NOT_NULL(items);

    item = menu_buscar_item(items, menu_get_item_count(), 22);

    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL_STRING("Resumen Compartible", item->texto);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_resumen_compartible_functions_exist);
    RUN_TEST(test_menu_resumen_compartible_tiene_resumen_anio);
    RUN_TEST(test_menu_principal_tiene_resumen_compartible);

    return UNITY_END();
}
