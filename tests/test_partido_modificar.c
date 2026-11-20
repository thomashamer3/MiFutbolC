#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * "Modificar Partido": signo de la diferencia de gol, UPDATE con chequeo,
 * reajuste de la diferencia al cambiar el resultado, sincronizacion de la
 * transaccion de Finanzas, reencolado del clima real, borrado con gasto, EOF
 * en los pedidos de enteros y el flujo interactivo de modificar_partido con
 * stdin/stdout redirigidos a archivos. Usa un perfil propio
 * (mifutbol_test_partido_mod.db en el directorio de datos) y lo borra al terminar.
 */

#include "test_db_fixture.h"
#include "clima_diferido.h"
#include "financiamiento.h"
#include "partido.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define fd_dup _dup
#define fd_dup2 _dup2
#define fd_close _close
#define fd_fileno _fileno
#else
#include <unistd.h>
#define fd_dup dup
#define fd_dup2 dup2
#define fd_close close
#define fd_fileno fileno
#endif

#define PERFIL_TEST "test_partido_mod"

static char g_entrada_path[1024];
static char g_salida_path[1024];

/*
 * Corre @p accion con stdin leyendo @p guion y stdout a un archivo; despues
 * restaura los dos. Devuelve la salida (malloc) y en @p sobrante si quedo
 * entrada sin leer. No usa TEST_ASSERT mientras stdout esta redirigido.
 */
static char *correr_con_guion(void (*accion)(void), const char *guion, int *sobrante)
{
    FILE *f = NULL;
    TEST_ASSERT_EQUAL_INT(0, fopen_s(&f, g_entrada_path, "wb"));
    fputs(guion, f);
    fclose(f);

    fflush(stdout);
    int stdin_guardado = fd_dup(fd_fileno(stdin));
    int stdout_guardado = fd_dup(fd_fileno(stdout));
    TEST_ASSERT_TRUE(stdin_guardado >= 0 && stdout_guardado >= 0);
    int redirigido = freopen(g_entrada_path, "r", stdin) != NULL &&
                     freopen(g_salida_path, "w", stdout) != NULL;
    if (redirigido)
    {
        accion();
        *sobrante = fgetc(stdin) != EOF;
    }
    fflush(stdout);
    fd_dup2(stdout_guardado, fd_fileno(stdout));
    fd_dup2(stdin_guardado, fd_fileno(stdin));
    fd_close(stdout_guardado);
    fd_close(stdin_guardado);
    clearerr(stdin);
    clearerr(stdout);
    remove(g_entrada_path);

    TEST_ASSERT_TRUE(redirigido);
    return leer_y_borrar(g_salida_path, NULL);
}

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

/* Con borrar_gasto se van el partido y su transaccion; sin el, el gasto queda. */
static void test_eliminar_con_y_sin_gasto(void)
{
    insertar_partido(1, 1, 1, 0);
    insertar_partido(2, 1, 1, 0);
    insertar_transaccion(1, 500);
    insertar_transaccion(2, 700);

    TEST_ASSERT_EQUAL_INT(1, partido_eliminar_con_transaccion(1, 1));
    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 1;"));
    TEST_ASSERT_EQUAL_INT(0, transacciones_de(1));

    TEST_ASSERT_EQUAL_INT(1, partido_eliminar_con_transaccion(2, 0));
    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 2;"));
    TEST_ASSERT_EQUAL_INT(700, monto_de(2));
}

/* Si el borrado del partido falla, el gasto vuelve con el ROLLBACK. */
static void test_eliminar_fallido_conserva_gasto(void)
{
    insertar_partido(1, 1, 1, 0);
    insertar_transaccion(1, 500);
    exec_ok("CREATE TRIGGER trg_test_no_borrar BEFORE DELETE ON partido "
            "BEGIN SELECT RAISE(ABORT, 'rechazado'); END;");

    TEST_ASSERT_EQUAL_INT(0, partido_eliminar_con_transaccion(1, 1));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 1;"));
    TEST_ASSERT_EQUAL_INT(500, monto_de(1));

    /* Un partido inexistente tampoco se lleva su gasto huerfano. */
    insertar_transaccion(5, 900);
    TEST_ASSERT_EQUAL_INT(0, partido_eliminar_con_transaccion(5, 1));
    TEST_ASSERT_EQUAL_INT(900, monto_de(5));
}

static int g_valor_pedido;

static void pedir_en_rango_1_10(void)
{
    g_valor_pedido = partido_test_pedir_entero_en_rango(1, 10);
}

static void pedir_minimo_0(void)
{
    g_valor_pedido = partido_test_pedir_entero_minimo(0);
}

/* Con stdin en EOF devuelven el minimo en vez de quedar en un bucle; un -1
 * escrito a proposito sigue pidiendo de nuevo. */
static void test_pedir_enteros_eof(void)
{
    int sobrante = 0;
    free(correr_con_guion(pedir_en_rango_1_10, "", &sobrante));
    TEST_ASSERT_EQUAL_INT(1, g_valor_pedido);

    free(correr_con_guion(pedir_en_rango_1_10, "-1\n7\n", &sobrante));
    TEST_ASSERT_EQUAL_INT(7, g_valor_pedido);
    TEST_ASSERT_FALSE(sobrante);

    free(correr_con_guion(pedir_minimo_0, "-5\n", &sobrante));
    TEST_ASSERT_EQUAL_INT(0, g_valor_pedido);

    free(correr_con_guion(pedir_minimo_0, "-1\n3\n", &sobrante));
    TEST_ASSERT_EQUAL_INT(3, g_valor_pedido);
}

/* Con stdin en EOF el listado sale en vez de repetir el menu para siempre. */
static void test_listado_sale_con_eof(void)
{
    int sobrante = 1;
    insertar_partido(1, 1, 1, 0);
    free(correr_con_guion(listar_partidos, "", &sobrante));
    TEST_ASSERT_FALSE(sobrante);
}

/* Si el borrado falla, eliminar_partido muestra el motivo real y no el
 * "not an error" que deja el ROLLBACK. */
static void test_eliminar_muestra_motivo_real(void)
{
    int sobrante = 1;
    insertar_partido(1, 1, 1, 0);
    exec_ok("CREATE TRIGGER trg_test_no_borrar BEFORE DELETE ON partido "
            "BEGIN SELECT RAISE(ABORT, 'rechazado'); END;");

    char *salida = correr_con_guion(eliminar_partido, "0\n1\ns\n\n", &sobrante);

    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 1;"));
    TEST_ASSERT_FALSE(sobrante);
    TEST_ASSERT_NOT_NULL(strstr(salida, "No se pudo eliminar el partido: rechazado"));
    TEST_ASSERT_NULL(strstr(salida, "not an error"));
    free(salida);
}

/* Partido 1 en la cancha sin coordenadas: el menu no lanza consultas de clima. */
static void preparar_flujo(int goles, const char *goles_detalle)
{
    char sql[256];
    insertar_partido(1, 2, 1, 0);
    snprintf(sql, sizeof(sql), "UPDATE partido SET goles = %d, goles_detalle = '%s' WHERE id = 1;",
             goles, goles_detalle);
    exec_ok(sql);
    settings_get()->mode = MODE_SIMPLE;
}

/*
 * Guion de modificar_partido: listado (0), id (1), pausa de la ficha, opciones
 * del menu y "0" para volver. Cada pausa es una linea vacia: una pausa de mas
 * se comeria la opcion siguiente y una de menos dejaria "Entrada vacia".
 */
static void test_flujo_goles_y_cancelar(void)
{
    int sobrante = 1;
    preparar_flujo(1, "");
    char *salida = correr_con_guion(modificar_partido,
                                    "0\n1\n\n"
                                    "3\n5\n\n"   /* Goles = 5 */
                                    "3\n-1\n\n"  /* Goles cancelado */
                                    "0\n",
                                    &sobrante);

    TEST_ASSERT_EQUAL_INT(5, int_de_partido(1, "goles"));
    TEST_ASSERT_FALSE(sobrante);
    TEST_ASSERT_NOT_NULL(strstr(salida, "Partido 1 (valores actuales)"));
    TEST_ASSERT_NOT_NULL(strstr(salida, "Goles: 1 | Asistencias: 0"));
    TEST_ASSERT_NOT_NULL(strstr(salida, "Actual: 1"));
    TEST_ASSERT_NOT_NULL(strstr(salida, "Actual: 5"));
    TEST_ASSERT_NOT_NULL(strstr(salida, "Operacion cancelada"));
    TEST_ASSERT_NULL(strstr(salida, "Entrada vacia"));
    TEST_ASSERT_NULL(strstr(salida, "Entrada invalida"));
    free(salida);
}

/* Con detalle cargado: 0 goles lo limpia y otra cantidad ofrece recargarlo,
 * con una sola pausa en cada camino. */
static void test_flujo_goles_con_detalle(void)
{
    int sobrante = 1;
    preparar_flujo(2, "Derecha,Cabeza");
    char *salida = correr_con_guion(modificar_partido,
                                    "0\n1\n\n"
                                    "3\n3\nS\n1\n2\n3\n\n" /* 3 goles y detalle nuevo */
                                    "0\n",
                                    &sobrante);
    TEST_ASSERT_EQUAL_INT(3, int_de_partido(1, "goles"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT goles_detalle = 'Derecha,Izquierda,Cabeza' "
                                           "FROM partido WHERE id = 1;"));
    TEST_ASSERT_FALSE(sobrante);
    TEST_ASSERT_NULL(strstr(salida, "Entrada vacia"));
    free(salida);

    salida = correr_con_guion(modificar_partido, "0\n1\n\n3\n0\n\n0\n", &sobrante);
    TEST_ASSERT_EQUAL_INT(0, int_de_partido(1, "goles"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT goles_detalle = '' FROM partido WHERE id = 1;"));
    TEST_ASSERT_FALSE(sobrante);
    TEST_ASSERT_NOT_NULL(strstr(salida, "Se limpio el detalle de goles"));
    TEST_ASSERT_NULL(strstr(salida, "Entrada vacia"));
    free(salida);
}

/* Fecha (franja recalculada) y precio (transaccion creada) por el menu. */
static void test_flujo_fecha_y_precio(void)
{
    int sobrante = 1;
    preparar_flujo(1, "");
    char *salida = correr_con_guion(modificar_partido,
                                    "0\n1\n\n"
                                    "2\n25/05/2026 22:20\n\n" /* Fecha y hora */
                                    "11\n800\n\n"            /* Precio */
                                    "0\n",
                                    &sobrante);

    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT fecha_hora = '2026-05-25 22:20' AND "
                                           "mes_anio = '2026-05' FROM partido WHERE id = 1;"));
    TEST_ASSERT_EQUAL_INT(6, int_de_partido(1, "dia"));
    TEST_ASSERT_EQUAL_INT(800, int_de_partido(1, "precio"));
    TEST_ASSERT_EQUAL_INT(800, monto_de(1));
    TEST_ASSERT_FALSE(sobrante);
    TEST_ASSERT_NOT_NULL(strstr(salida, "Franja horaria calculada automaticamente: Noche"));
    TEST_ASSERT_NOT_NULL(strstr(salida, "Actual: 0"));
    TEST_ASSERT_NULL(strstr(salida, "Entrada vacia"));
    free(salida);
}

int main(void)
{
    if (!fixture_iniciar(PERFIL_TEST, g_entrada_path, sizeof(g_entrada_path), "_entrada.txt"))
    {
        return 1;
    }
    snprintf(g_salida_path, sizeof(g_salida_path), "%s" TEST_SEP "%s_salida.txt", get_data_dir(),
             PERFIL_TEST);

    UNITY_BEGIN();

    RUN_TEST(test_diferencia_con_signo);
    RUN_TEST(test_update_con_chequeo);
    RUN_TEST(test_cambiar_resultado_reajusta_signo);
    RUN_TEST(test_sincronizar_transaccion);
    RUN_TEST(test_sincronizar_transaccion_texto_de_respaldo);
    RUN_TEST(test_crear_transaccion_no_duplica);
    RUN_TEST(test_reencolar_clima);
    RUN_TEST(test_buscar_transaccion_formato_finanzas);
    RUN_TEST(test_eliminar_con_y_sin_gasto);
    RUN_TEST(test_eliminar_fallido_conserva_gasto);
    RUN_TEST(test_pedir_enteros_eof);
    RUN_TEST(test_listado_sale_con_eof);
    RUN_TEST(test_eliminar_muestra_motivo_real);
    RUN_TEST(test_flujo_goles_y_cancelar);
    RUN_TEST(test_flujo_goles_con_detalle);
    RUN_TEST(test_flujo_fecha_y_precio);

    return fixture_terminar();
}
