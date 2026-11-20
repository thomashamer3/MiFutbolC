#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * "Modificar Partido": signo de la diferencia de gol, UPDATE con chequeo y
 * reajuste de la diferencia al cambiar el resultado. Usa un perfil propio
 * (mifutbol_test_partido_mod.db en el directorio de datos) y lo borra al terminar.
 */

#include "test_db_fixture.h"
#include "partido.h"
#include <stdio.h>

#define PERFIL_TEST "test_partido_mod"

static void crear_cancha_y_camiseta(void)
{
    exec_ok("INSERT INTO camiseta(id, nombre) VALUES(1, 'Titular');");
    exec_ok("INSERT INTO cancha(id, nombre, latitud, longitud) VALUES(1, 'Con GPS', -34.6, -58.4);");
    exec_ok("INSERT INTO cancha(id, nombre) VALUES(2, 'Sin GPS');");
}

static void insertar_partido(long long id, int cancha_id, int resultado, int diferencia)
{
    char sql[320];
    snprintf(sql, sizeof(sql),
             "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, "
             "mes_anio, resultado, diferencia_gol, precio) "
             "VALUES(%lld, %d, '2025-10-01 10:00', 1, 0, 1, '2025-10', %d, %d, 0);",
             id, cancha_id, resultado, diferencia);
    exec_ok(sql);
}

static int int_de_partido(long long id, const char *campo)
{
    char sql[160];
    snprintf(sql, sizeof(sql), "SELECT %s FROM partido WHERE id = %lld;", campo, id);
    return consultar_int(sql);
}

void setUp(void)
{
    abrir_base_limpia();
    crear_cancha_y_camiseta();
}

void tearDown(void)
{
    cerrar_y_borrar_base();
}

static void test_diferencia_con_signo(void)
{
    TEST_ASSERT_EQUAL_INT(3, partido_diferencia_con_signo(1, 3));
    TEST_ASSERT_EQUAL_INT(0, partido_diferencia_con_signo(2, 3));
    TEST_ASSERT_EQUAL_INT(-3, partido_diferencia_con_signo(3, 3));
    /* Una entrada negativa toma el signo del resultado, no el suyo. */
    TEST_ASSERT_EQUAL_INT(2, partido_diferencia_con_signo(1, -2));
    TEST_ASSERT_EQUAL_INT(-2, partido_diferencia_con_signo(3, -2));
    TEST_ASSERT_EQUAL_INT(0, partido_diferencia_con_signo(2, -2));
    TEST_ASSERT_EQUAL_INT(0, partido_diferencia_con_signo(0, 5));
}

static void test_update_con_chequeo(void)
{
    insertar_partido(1, 1, 1, 0);
    insertar_partido(2, 1, 1, 0);

    TEST_ASSERT_EQUAL_INT(1, partido_test_actualizar_int(1, "goles", 7));
    TEST_ASSERT_EQUAL_INT(7, int_de_partido(1, "goles"));
    TEST_ASSERT_EQUAL_INT(1, int_de_partido(2, "goles"));

    /* Columna inexistente, partido inexistente y step que falla: 0 sin cambios. */
    TEST_ASSERT_EQUAL_INT(0, partido_test_actualizar_int(1, "columna_que_no_existe", 1));
    TEST_ASSERT_EQUAL_INT(0, partido_test_actualizar_int(99, "goles", 3));
    exec_ok("CREATE TRIGGER trg_test_rechazo BEFORE UPDATE ON partido "
            "BEGIN SELECT RAISE(ABORT, 'rechazado'); END;");
    TEST_ASSERT_EQUAL_INT(0, partido_test_actualizar_int(1, "goles", 9));
    TEST_ASSERT_EQUAL_INT(7, int_de_partido(1, "goles"));
}

static void test_cambiar_resultado_reajusta_signo(void)
{
    insertar_partido(1, 1, 1, 3);

    TEST_ASSERT_EQUAL_INT(1, partido_test_guardar_resultado(1, 3));
    TEST_ASSERT_EQUAL_INT(3, int_de_partido(1, "resultado"));
    TEST_ASSERT_EQUAL_INT(-3, int_de_partido(1, "diferencia_gol"));

    TEST_ASSERT_EQUAL_INT(1, partido_test_guardar_resultado(1, 1));
    TEST_ASSERT_EQUAL_INT(3, int_de_partido(1, "diferencia_gol"));

    TEST_ASSERT_EQUAL_INT(1, partido_test_guardar_resultado(1, 2));
    TEST_ASSERT_EQUAL_INT(2, int_de_partido(1, "resultado"));
    TEST_ASSERT_EQUAL_INT(0, int_de_partido(1, "diferencia_gol"));

    TEST_ASSERT_EQUAL_INT(0, partido_test_guardar_resultado(99, 1));
}

int main(void)
{
    if (!fixture_iniciar(PERFIL_TEST, NULL, 0, NULL))
    {
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_diferencia_con_signo);
    RUN_TEST(test_update_con_chequeo);
    RUN_TEST(test_cambiar_resultado_reajusta_signo);

    return fixture_terminar();
}
