#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * Importacion de partidos desde CSV, TXT y JSON con archivos temporales.
 * Comportamiento que fija: cada archivo se importa en una sola transaccion,
 * las lineas invalidas (formato roto o camiseta inexistente) se omiten y las
 * validas se confirman; los duplicados se saltean; los IDs nuevos no reusan
 * IDs borrados. Usa un perfil propio (mifutbol_test_import.db en el
 * directorio de datos) y no toca la carpeta de importaciones del usuario.
 */

#include "unity/unity.h"
#include "db.h"
#include "import.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define TEST_SEP "\\"
#else
#define TEST_SEP "/"
#endif

#define PERFIL_TEST "test_import"

#define CSV_ENCABEZADO                                                                         \
    "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,"     \
    "Cansancio,Estado_Animo,Comentario_Personal\n"
#define CSV_FILA_1 "Club Norte,2025-03-10 20:00,2,1,Titular,VICTORIA,Despejado,Noche,8,4,7,Buen partido\n"
#define CSV_FILA_2 "Club Sur,2025-03-17 19:00,0,2,Titular,DERROTA,Lluvia,Tarde,5,6,4,Mucho barro\n"

static char g_db_path[1024];
static char g_archivo_path[1024];
static char g_log_path[1024];

static void borrar_base(const char *path)
{
    char aux[1100];
    remove(path);
    snprintf(aux, sizeof(aux), "%s-wal", path);
    remove(aux);
    snprintf(aux, sizeof(aux), "%s-shm", path);
    remove(aux);
}

static int consultar_int(const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    int valor = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return -1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        valor = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return valor;
}

static void exec_ok(const char *sql)
{
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(db, sql, NULL, NULL, NULL));
}

/* Escribe el archivo de prueba en binario: el contenido llega tal cual. */
static void escribir_archivo(const char *contenido)
{
    FILE *f = fopen(g_archivo_path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    fputs(contenido, f);
    fclose(f);
}

static int contar_partidos(void)
{
    return consultar_int("SELECT COUNT(*) FROM partido;");
}

void setUp(void)
{
    borrar_base(g_db_path);
    remove(g_archivo_path);
    TEST_ASSERT_TRUE(db_init());
    exec_ok("INSERT INTO camiseta(id, nombre) VALUES(1, 'Titular');");
}

void tearDown(void)
{
    db_close();
    borrar_base(g_db_path);
    remove(g_archivo_path);
}

/* CSV valido: crea los partidos, las canchas que faltan y convierte los campos. */
static void test_csv_importa_filas_y_crea_canchas(void)
{
    escribir_archivo(CSV_ENCABEZADO CSV_FILA_1 CSV_FILA_2);

    TEST_ASSERT_EQUAL_INT(2, import_test_partidos_csv_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(2, contar_partidos());
    TEST_ASSERT_EQUAL_INT(2, consultar_int("SELECT COUNT(*) FROM cancha "
                                           "WHERE nombre IN ('Club Norte', 'Club Sur');"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int(
                              "SELECT COUNT(*) FROM partido p JOIN cancha c ON c.id = p.cancha_id "
                              "WHERE c.nombre = 'Club Norte' AND p.fecha_hora = '2025-03-10 20:00' "
                              "AND p.goles = 2 AND p.asistencias = 1 AND p.camiseta_id = 1 "
                              "AND p.resultado = 1 AND p.clima = 1 AND p.dia = 3 "
                              "AND p.rendimiento_general = 8 AND p.cansancio = 4 "
                              "AND p.estado_animo = 7 AND p.mes_anio = '2025-03' "
                              "AND p.comentario_personal = 'Buen partido';"));
    /* La transaccion quedo cerrada. */
    TEST_ASSERT_TRUE(sqlite3_get_autocommit(db));
}

/* Una linea rota o con camiseta inexistente se omite; las demas se confirman. */
static void test_csv_linea_invalida_se_omite_y_el_resto_se_confirma(void)
{
    escribir_archivo(CSV_ENCABEZADO CSV_FILA_1
                     "esto,no,es,un,partido\n"
                     "Club Este,2025-04-01 21:00,1,1,Suplente,EMPATE,Nublado,Noche,6,5,6,Sin camiseta\n"
                     "\n" CSV_FILA_2);

    TEST_ASSERT_EQUAL_INT(2, import_test_partidos_csv_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(2, contar_partidos());
    /* Comportamiento actual: la cancha de la fila sin camiseta se crea antes de
     * validar la camiseta, y queda aunque el partido se omita. */
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM cancha WHERE nombre = 'Club Este';"));
    TEST_ASSERT_EQUAL_INT(0,consultar_int("SELECT COUNT(*) FROM partido p JOIN cancha c "
                                           "ON c.id = p.cancha_id WHERE c.nombre = 'Club Este';"));
    TEST_ASSERT_TRUE(sqlite3_get_autocommit(db));
}

/* Importar dos veces el mismo archivo no duplica partidos. */
static void test_csv_reimportar_no_duplica(void)
{
    escribir_archivo(CSV_ENCABEZADO CSV_FILA_1 CSV_FILA_2);
    TEST_ASSERT_EQUAL_INT(2, import_test_partidos_csv_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(0, import_test_partidos_csv_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(2, contar_partidos());
}

/* Entradas vacias: sin archivo o vacio devuelve -1; solo encabezado, 0. */
static void test_csv_entradas_vacias(void)
{
    TEST_ASSERT_EQUAL_INT(-1, import_test_partidos_csv_desde_ruta(g_archivo_path));

    escribir_archivo("");
    TEST_ASSERT_EQUAL_INT(-1, import_test_partidos_csv_desde_ruta(g_archivo_path));

    escribir_archivo(CSV_ENCABEZADO);
    TEST_ASSERT_EQUAL_INT(0, import_test_partidos_csv_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(0, contar_partidos());
    TEST_ASSERT_TRUE(sqlite3_get_autocommit(db));
}

/* Los IDs nuevos siguen a partido_id_secuencia: no reusan IDs borrados,
 * aunque el borrado fuera el ultimo. */
static void test_ids_nuevos_no_reusan_borrados(void)
{
    exec_ok("INSERT INTO cancha(id, nombre) VALUES(1, 'Club Norte');");
    for (int id = 1; id <= 3; id++)
    {
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, "
                 "camiseta_id, mes_anio) VALUES(%d, 1, '2024-01-0%d 10:00', 0, 0, 1, '2024-01');",
                 id, id);
        exec_ok(sql);
    }
    exec_ok("DELETE FROM partido WHERE id = 3;");
    TEST_ASSERT_EQUAL_INT(3, consultar_int("SELECT ultimo FROM partido_id_secuencia WHERE id = 1;"));

    escribir_archivo(CSV_ENCABEZADO CSV_FILA_1 CSV_FILA_2);
    TEST_ASSERT_EQUAL_INT(2, import_test_partidos_csv_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 3;"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 4 "
                                           "AND fecha_hora = '2025-03-10 20:00';"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 5 "
                                           "AND fecha_hora = '2025-03-17 19:00';"));
}

/* TXT: formato con separadores y campos meta; una linea sin meta completa se omite. */
static void test_txt_importa_y_omite_lineas_invalidas(void)
{
    escribir_archivo("PARTIDOS\n"
                     "Club Norte | 2025-03-10 20:00 | G:2 A:1 | Titular | "
                     "Res:VICTORIA Cli:Despejado Dia:Noche RG:8 Can:4 EA:7 | Buen partido\n"
                     "Club Sur | 2025-03-17 19:00 | G:0 A:2 | Titular | "
                     "Res:DERROTA Cli:Lluvia | Faltan campos meta\n"
                     "linea sin separadores\n");

    TEST_ASSERT_EQUAL_INT(1, import_test_partidos_txt_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(1, contar_partidos());
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE goles = 2 "
                                           "AND resultado = 1 AND clima = 1 AND dia = 3 "
                                           "AND comentario_personal = 'Buen partido';"));
}

/* JSON: items invalidos se omiten; el array completo va en una transaccion. */
static void test_json_importa_items_validos(void)
{
    escribir_archivo("[\r\n"
                     "  {\"cancha\":\"Club Norte\",\"fecha\":\"2025-03-10 20:00\",\"goles\":2,"
                     "\"asistencias\":1,\"camiseta\":\"Titular\",\"resultado\":1,\"clima\":1,"
                     "\"dia\":3,\"comentario_personal\":\"Buen partido\"},\r\n"
                     "  {\"cancha\":\"Club Sur\",\"fecha\":\"2025-03-17 19:00\"},\r\n"
                     "  {\"cancha\":\"Club Sur\",\"fecha\":\"2025-03-17 19:00\",\"goles\":0,"
                     "\"asistencias\":2,\"camiseta\":\"Inexistente\"},\r\n"
                     "  42\r\n"
                     "]\r\n");

    TEST_ASSERT_EQUAL_INT(1, importar_partidos_desde_ruta(g_archivo_path));
    TEST_ASSERT_EQUAL_INT(1, contar_partidos());
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE goles = 2 "
                                           "AND mes_anio = '2025-03' "
                                           "AND comentario_personal = 'Buen partido';"));
    TEST_ASSERT_TRUE(sqlite3_get_autocommit(db));
}

/* JSON malformado, vacio, que no es array o ruta invalida: -1 sin tocar la base. */
static void test_json_entradas_malformadas(void)
{
    TEST_ASSERT_EQUAL_INT(-1, importar_partidos_desde_ruta(NULL));
    TEST_ASSERT_EQUAL_INT(-1, importar_partidos_desde_ruta(""));
    TEST_ASSERT_EQUAL_INT(-1, importar_partidos_desde_ruta(g_archivo_path));

    escribir_archivo("");
    TEST_ASSERT_EQUAL_INT(-1, importar_partidos_desde_ruta(g_archivo_path));

    escribir_archivo("[{\"cancha\":\"Club Norte\",");
    TEST_ASSERT_EQUAL_INT(-1, importar_partidos_desde_ruta(g_archivo_path));

    escribir_archivo("{\"cancha\":\"Club Norte\"}");
    TEST_ASSERT_EQUAL_INT(-1, importar_partidos_desde_ruta(g_archivo_path));

    escribir_archivo("[]");
    TEST_ASSERT_EQUAL_INT(0, importar_partidos_desde_ruta(g_archivo_path));

    TEST_ASSERT_EQUAL_INT(0, contar_partidos());
    TEST_ASSERT_TRUE(sqlite3_get_autocommit(db));
}

int main(void)
{
    if (!db_set_active_user(PERFIL_TEST))
    {
        fprintf(stderr, "No se pudo seleccionar el perfil de prueba\n");
        return 1;
    }
    if (!db_init())
    {
        fprintf(stderr, "db_init fallo\n");
        return 1;
    }
    const char *data_dir = get_data_dir();
    if (!data_dir)
    {
        fprintf(stderr, "Sin directorio de datos\n");
        return 1;
    }
    snprintf(g_db_path, sizeof(g_db_path), "%s" TEST_SEP "mifutbol_%s.db", data_dir, PERFIL_TEST);
    snprintf(g_archivo_path, sizeof(g_archivo_path), "%s" TEST_SEP "%s_entrada.txt", data_dir,
             PERFIL_TEST);
    snprintf(g_log_path, sizeof(g_log_path), "%s" TEST_SEP "mifutbol_%s.log", data_dir,
             PERFIL_TEST);
    db_close();

    UNITY_BEGIN();

    RUN_TEST(test_csv_importa_filas_y_crea_canchas);
    RUN_TEST(test_csv_linea_invalida_se_omite_y_el_resto_se_confirma);
    RUN_TEST(test_csv_reimportar_no_duplica);
    RUN_TEST(test_csv_entradas_vacias);
    RUN_TEST(test_ids_nuevos_no_reusan_borrados);
    RUN_TEST(test_txt_importa_y_omite_lineas_invalidas);
    RUN_TEST(test_json_importa_items_validos);
    RUN_TEST(test_json_entradas_malformadas);

    int fallos = UNITY_END();
    remove(g_log_path);
    return fallos;
}
