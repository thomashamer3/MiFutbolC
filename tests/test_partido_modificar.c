#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * "Modificar Partido": signo de la diferencia de gol, UPDATE con chequeo,
 * reajuste de la diferencia al cambiar el resultado, sincronizacion de la
 * transaccion de Finanzas y reencolado del clima real. Usa un perfil propio
 * (mifutbol_test_partido_mod.db en el directorio de datos) y lo borra al terminar.
 */

#include "test_db_fixture.h"
#include "clima_diferido.h"
#include "financiamiento.h"
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

/* Transaccion de cancha con el texto que arma crear_transaccion_partido.
 * tipo 1 = GASTO y categoria 5 = CANCHAS (test_sincronizar_transaccion lo verifica). */
static void insertar_transaccion(long long partido_id, int monto)
{
    char sql[320];
    snprintf(sql, sizeof(sql),
             "INSERT INTO financiamiento(fecha, tipo, categoria, descripcion, monto, "
             "item_especifico) VALUES('2025-10-01', 1, 5, 'Pago por alquiler de cancha', %d, "
             "'(%lld |Cancha:Con GPS |Fecha:01/10/2025 10:00 | G:1 A:0 |Camiseta:Titular | "
             "VICTORIA)');",
             monto, partido_id);
    exec_ok(sql);
}

/* Monto de la transaccion del partido (formato normal), -1 si no hay. */
static int monto_de(long long partido_id)
{
    char sql[200];
    snprintf(sql, sizeof(sql),
             "SELECT COALESCE((SELECT monto FROM financiamiento WHERE tipo = 1 AND categoria = 5 "
             "AND item_especifico LIKE '(%lld |%%'), -1);",
             partido_id);
    return consultar_int(sql);
}

static int transacciones_de(long long partido_id)
{
    char sql[200];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM financiamiento WHERE item_especifico LIKE '(%lld |%%';",
             partido_id);
    return consultar_int(sql);
}

void tearDown(void)
{
    /* Olvida la tabla de pendientes: la proxima base es nueva. */
    clima_diferido_finalizar();
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

/* Crear 0->500, actualizar 500->800 y borrar 800->0 sin tocar al partido 10. */
static void test_sincronizar_transaccion(void)
{
    TEST_ASSERT_EQUAL_INT(1, GASTO);
    TEST_ASSERT_EQUAL_INT(5, CANCHAS);
    insertar_partido(1, 1, 1, 0);
    insertar_partido(10, 1, 1, 0);
    insertar_transaccion(10, 300);

    TEST_ASSERT_EQUAL_INT(1, partido_sincronizar_transaccion(1, 0));
    TEST_ASSERT_EQUAL_INT(0, transacciones_de(1));

    TEST_ASSERT_EQUAL_INT(1, partido_sincronizar_transaccion(1, 500));
    TEST_ASSERT_EQUAL_INT(1, transacciones_de(1));
    TEST_ASSERT_EQUAL_INT(500, monto_de(1));

    TEST_ASSERT_EQUAL_INT(1, partido_sincronizar_transaccion(1, 800));
    TEST_ASSERT_EQUAL_INT(1, transacciones_de(1));
    TEST_ASSERT_EQUAL_INT(800, monto_de(1));

    TEST_ASSERT_EQUAL_INT(1, partido_sincronizar_transaccion(1, 0));
    TEST_ASSERT_EQUAL_INT(0, transacciones_de(1));

    TEST_ASSERT_EQUAL_INT(300, monto_de(10));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM financiamiento;"));
}

/* Los textos de respaldo "Partido ID: N" tambien se reconocen, con delimitador. */
static void test_sincronizar_transaccion_texto_de_respaldo(void)
{
    insertar_partido(3, 1, 1, 0);
    exec_ok("INSERT INTO financiamiento(fecha, tipo, categoria, descripcion, monto, "
            "item_especifico) VALUES('2025-10-01', 1, 5, 'x', 100, 'Partido ID: 3');");
    exec_ok("INSERT INTO financiamiento(fecha, tipo, categoria, descripcion, monto, "
            "item_especifico) VALUES('2025-10-01', 1, 5, 'x', 200, 'Partido ID: 30');");

    TEST_ASSERT_EQUAL_INT(1, partido_sincronizar_transaccion(3, 700));
    TEST_ASSERT_EQUAL_INT(700, consultar_int("SELECT monto FROM financiamiento "
                          "WHERE item_especifico = 'Partido ID: 3';"));
    TEST_ASSERT_EQUAL_INT(200, consultar_int("SELECT monto FROM financiamiento "
                          "WHERE item_especifico = 'Partido ID: 30';"));
}

/* crear_transaccion_partido reconoce la transaccion existente y no la duplica. */
static void test_crear_transaccion_no_duplica(void)
{
    insertar_partido(1, 1, 1, 0);
    insertar_partido(10, 1, 1, 0);
    insertar_transaccion(10, 300);

    TEST_ASSERT_EQUAL_INT(0, partido_test_crear_transaccion(10, 300));
    TEST_ASSERT_EQUAL_INT(1, transacciones_de(10));

    TEST_ASSERT_EQUAL_INT(1, partido_test_crear_transaccion(1, 400));
    TEST_ASSERT_EQUAL_INT(0, partido_test_crear_transaccion(1, 400));
    TEST_ASSERT_EQUAL_INT(1, transacciones_de(1));
    TEST_ASSERT_EQUAL_INT(2, consultar_int("SELECT COUNT(*) FROM financiamiento;"));
}

static void poner_clima_real(long long id)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE partido SET temperatura_c = 20.5, apparent_temp_c = 19, precip_mm = 1, "
             "wind_kmh = 10, weather_code = 61, clima_json = '{}' WHERE id = %lld;",
             id);
    exec_ok(sql);
}

static int pendiente_existe(long long partido_id)
{
    char sql[128];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM partido_clima_pendiente WHERE partido_id = %lld;", partido_id);
    return consultar_int(sql);
}

/* Con coordenadas limpia el clima real y deja el pendiente; sin ellas no toca nada. */
static void test_reencolar_clima(void)
{
    insertar_partido(1, 1, 1, 0);
    insertar_partido(2, 2, 1, 0);
    poner_clima_real(1);
    poner_clima_real(2);
    TEST_ASSERT_TRUE(clima_diferido_init());

    TEST_ASSERT_EQUAL_INT(1, partido_reencolar_clima(1));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT temperatura_c IS NULL AND "
                          "apparent_temp_c IS NULL AND precip_mm IS NULL AND "
                          "wind_kmh IS NULL AND weather_code IS NULL AND clima_json IS NULL "
                          "FROM partido WHERE id = 1;"));
    TEST_ASSERT_EQUAL_INT(1, pendiente_existe(1));

    TEST_ASSERT_EQUAL_INT(0, partido_reencolar_clima(2));
    TEST_ASSERT_EQUAL_INT(20, consultar_int("SELECT temperatura_c FROM partido WHERE id = 2;"));
    TEST_ASSERT_EQUAL_INT(61, consultar_int("SELECT weather_code FROM partido WHERE id = 2;"));
    TEST_ASSERT_EQUAL_INT(0, pendiente_existe(2));

    TEST_ASSERT_EQUAL_INT(0, partido_reencolar_clima(99));
}

/* El texto que arma Finanzas a mano (construir_item_partido, con Clima y Dia)
 * tambien se encuentra; otro tipo/categoria o el partido 40 no cuentan. */
static void test_buscar_transaccion_formato_finanzas(void)
{
    TEST_ASSERT_EQUAL_INT(0, (int)partido_buscar_transaccion(4));
    exec_ok("INSERT INTO financiamiento(id, fecha, tipo, categoria, descripcion, monto, "
            "item_especifico) VALUES(7, '2025-10-01', 0, 5, 'x', 100, '(4 |Cancha:A)');");
    exec_ok("INSERT INTO financiamiento(id, fecha, tipo, categoria, descripcion, monto, "
            "item_especifico) VALUES(8, '2025-10-01', 1, 5, 'x', 100, '(40 |Cancha:A)');");
    TEST_ASSERT_EQUAL_INT(0, (int)partido_buscar_transaccion(4));

    exec_ok("INSERT INTO financiamiento(id, fecha, tipo, categoria, descripcion, monto, "
            "item_especifico) VALUES(9, '2025-10-01', 1, 5, 'x', 100, "
            "'(4 |Cancha:Con GPS |Fecha:01/10/2025 10:00 | G:1 A:0 |Camiseta:Titular | "
            "VICTORIA |Clima:Soleado |Dia:Tarde)');");
    TEST_ASSERT_EQUAL_INT(9, (int)partido_buscar_transaccion(4));
    TEST_ASSERT_EQUAL_INT(8, (int)partido_buscar_transaccion(40));
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
    RUN_TEST(test_sincronizar_transaccion);
    RUN_TEST(test_sincronizar_transaccion_texto_de_respaldo);
    RUN_TEST(test_crear_transaccion_no_duplica);
    RUN_TEST(test_reencolar_clima);
    RUN_TEST(test_buscar_transaccion_formato_finanzas);

    return fixture_terminar();
}
