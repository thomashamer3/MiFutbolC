#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * Integridad de la base: backup con WAL, restauracion sobre la conexion
 * abierta y avance de user_version. Usa un perfil propio
 * (mifutbol_test_integridad.db en el directorio de datos), asi que no toca
 * los perfiles reales.
 */

#include "unity/unity.h"
#include "db.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <share.h>
#define TEST_SEP "\\"
#else
#define TEST_SEP "/"
#endif

#define PERFIL_TEST "test_integridad"
#define VERSION_ESPERADA 17
#define FILAS_BACKUP 50
#define FILAS_EXTRA 25

static char g_db_path[1024];
static char g_backup_path[1024];
static char g_inicio_path[1100]; /* backup de inicio de db_init en Backups */
static char g_invalido_path[1024];
static char g_wal_path[1100];
static char g_log_path[1024];

static long tamano_archivo(const char *path)
{
    FILE *f = NULL;
#ifdef _WIN32
    /* fopen_s abre sin compartir y SQLite tiene abierto el -wal: se pide
     * acceso compartido como hacia fopen. */
    f = _fsopen(path, "rb", _SH_DENYNO);
#else
    if (fopen_s(&f, path, "rb") != 0)
    {
        f = NULL;
    }
#endif
    if (!f)
    {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fclose(f);
    return tam;
}

static void borrar_base(const char *path)
{
    char aux[1100];
    remove(path);
    snprintf(aux, sizeof(aux), "%s-wal", path);
    remove(aux);
    snprintf(aux, sizeof(aux), "%s-shm", path);
    remove(aux);
}

static int consultar_int(sqlite3 *conn, const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    int valor = -1;
    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) != SQLITE_OK)
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

static int integridad_ok(sqlite3 *conn)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn, "PRAGMA integrity_check;", -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *r = (const char *)sqlite3_column_text(stmt, 0);
        ok = r && strcmp(r, "ok") == 0;
    }
    sqlite3_finalize(stmt);
    return ok;
}

static void insertar_filas(int desde, int cantidad)
{
    char sql[128];
    for (int i = desde; i < desde + cantidad; i++)
    {
        snprintf(sql, sizeof(sql), "INSERT INTO integ_t(valor) VALUES(%d);", i);
        TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(db, sql, NULL, NULL, NULL));
    }
}

static void reabrir(void)
{
    db_close();
    TEST_ASSERT_TRUE(db_init());
    TEST_ASSERT_NOT_NULL(db);
}

void setUp(void)
{
    borrar_base(g_db_path);
    borrar_base(g_backup_path);
    remove(g_invalido_path);
    TEST_ASSERT_TRUE(db_init());
    TEST_ASSERT_NOT_NULL(db);
}

void tearDown(void)
{
    db_close();
    borrar_base(g_db_path);
    borrar_base(g_backup_path);
    borrar_base(g_inicio_path);
    remove(g_invalido_path);
}

/* (a) Un backup hecho con filas que todavia estan en el WAL las contiene. */
static void test_backup_incluye_filas_del_wal(void)
{
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_exec(db, "CREATE TABLE integ_t(id INTEGER PRIMARY KEY, valor INT);",
                                       NULL, NULL, NULL));
    insertar_filas(0, FILAS_BACKUP);

    /* El escenario del bug: las filas nuevas siguen en el -wal. */
    TEST_ASSERT_TRUE(tamano_archivo(g_wal_path) > 0);

    TEST_ASSERT_EQUAL_INT(SQLITE_OK, db_backup_a_archivo(g_backup_path));

    sqlite3 *copia = NULL;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_open_v2(g_backup_path, &copia, SQLITE_OPEN_READONLY, NULL));
    TEST_ASSERT_EQUAL_INT(FILAS_BACKUP, consultar_int(copia, "SELECT COUNT(*) FROM integ_t;"));
    TEST_ASSERT_TRUE(integridad_ok(copia));
    sqlite3_close(copia);
}

/* (a2) db_init copia la base a Backups al arrancar, pero solo si tiene datos:
 * una base recien creada (app reinstalada) no debe pisar el backup bueno. */
static void test_backup_de_inicio_solo_con_datos(void)
{
    const char *inicio_path = g_inicio_path;
    borrar_base(inicio_path);

    reabrir(); /* base vacia */
    TEST_ASSERT_EQUAL_INT(-1, tamano_archivo(inicio_path));

    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(db, "INSERT INTO camiseta(nombre) VALUES('X');",
                          NULL, NULL, NULL));
    reabrir(); /* ahora hay datos: se escribe el backup */

    sqlite3 *copia = NULL;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_open_v2(inicio_path, &copia, SQLITE_OPEN_READONLY, NULL));
    TEST_ASSERT_EQUAL_INT(1, consultar_int(copia, "SELECT COUNT(*) FROM camiseta;"));
    sqlite3_close(copia);
}

/* (b) Restaurar sobre la conexion abierta deja los datos del backup y la base
 * sana, tambien despues de cerrar (cuando SQLite vuelca el WAL) y reabrir. */
static void test_restaurar_deja_datos_del_backup(void)
{
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_exec(db, "CREATE TABLE integ_t(id INTEGER PRIMARY KEY, valor INT);",
                                       NULL, NULL, NULL));
    insertar_filas(0, FILAS_BACKUP);
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, db_backup_a_archivo(g_backup_path));

    insertar_filas(FILAS_BACKUP, FILAS_EXTRA);
    TEST_ASSERT_EQUAL_INT(FILAS_BACKUP + FILAS_EXTRA,
                          consultar_int(db, "SELECT COUNT(*) FROM integ_t;"));

    char detalle[256];
    TEST_ASSERT_EQUAL_INT(DB_RESTAURAR_OK,
                          db_restaurar_desde_archivo(g_backup_path, detalle, sizeof(detalle)));
    TEST_ASSERT_EQUAL_INT(FILAS_BACKUP, consultar_int(db, "SELECT COUNT(*) FROM integ_t;"));
    TEST_ASSERT_TRUE(integridad_ok(db));

    reabrir();
    TEST_ASSERT_EQUAL_INT(FILAS_BACKUP, consultar_int(db, "SELECT COUNT(*) FROM integ_t;"));
    TEST_ASSERT_TRUE(integridad_ok(db));
}

/* (b) Un origen que no es una base valida se rechaza sin tocar la actual. */
static void test_restaurar_rechaza_origen_invalido(void)
{
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_exec(db, "CREATE TABLE integ_t(id INTEGER PRIMARY KEY, valor INT);",
                                       NULL, NULL, NULL));
    insertar_filas(0, FILAS_EXTRA);

    FILE *f = NULL;
    TEST_ASSERT_EQUAL_INT(0, fopen_s(&f, g_invalido_path, "wb"));
    TEST_ASSERT_NOT_NULL(f);
    fputs("esto no es una base SQLite, solo texto de relleno para el encabezado...\n", f);
    fclose(f);

    char detalle[256];
    TEST_ASSERT_EQUAL_INT(DB_RESTAURAR_ORIGEN_INVALIDO,
                          db_restaurar_desde_archivo(g_invalido_path, detalle, sizeof(detalle)));
    TEST_ASSERT_EQUAL_INT(DB_RESTAURAR_ORIGEN_NO_ABRE,
                          db_restaurar_desde_archivo(g_backup_path, detalle, sizeof(detalle)));
    TEST_ASSERT_EQUAL_INT(FILAS_EXTRA, consultar_int(db, "SELECT COUNT(*) FROM integ_t;"));
    TEST_ASSERT_TRUE(integridad_ok(db));
}

/* (c) Tras iniciar, user_version es DB_VERSION_CURRENT, tambien partiendo de
 * una base vieja; y los backfills "once" no se repiten en el segundo arranque. */
static void test_user_version_avanza_y_backfill_no_se_repite(void)
{
    TEST_ASSERT_EQUAL_INT(VERSION_ESPERADA, consultar_int(db, "PRAGMA user_version;"));

    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_exec(db, "PRAGMA user_version = 15;", NULL, NULL, NULL));
    reabrir();
    TEST_ASSERT_EQUAL_INT(VERSION_ESPERADA, consultar_int(db, "PRAGMA user_version;"));

    /* Un partido con mes_anio vacio: si el backfill de mes_anio corriera otra
     * vez, lo completaria. */
    TEST_ASSERT_EQUAL_INT(
        SQLITE_OK,
        sqlite3_exec(db,
                     "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, "
                     "camiseta_id, mes_anio) VALUES(1, 1, '2026-01-05 10:00', 0, 0, 1, '');",
                     NULL, NULL, NULL));
    reabrir();
    TEST_ASSERT_EQUAL_INT(VERSION_ESPERADA, consultar_int(db, "PRAGMA user_version;"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int(db, "SELECT COUNT(*) FROM partido "
                                           "WHERE id = 1 AND mes_anio = '';"));
}

/* Restaurar un backup de esquema viejo lo migra y deja la version actual. */
static void test_restaurar_backup_viejo_migra_version(void)
{
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_exec(db, "PRAGMA user_version = 15;", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, db_backup_a_archivo(g_backup_path));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_exec(db, "PRAGMA user_version = 17;", NULL, NULL, NULL));

    char detalle[256];
    TEST_ASSERT_EQUAL_INT(DB_RESTAURAR_OK,
                          db_restaurar_desde_archivo(g_backup_path, detalle, sizeof(detalle)));
    TEST_ASSERT_EQUAL_INT(VERSION_ESPERADA, consultar_int(db, "PRAGMA user_version;"));
    TEST_ASSERT_TRUE(integridad_ok(db));
}

int main(void)
{
    if (!db_set_active_user(PERFIL_TEST))
    {
        fprintf(stderr, "No se pudo seleccionar el perfil de prueba\n");
        return 1;
    }
    /* Resolver rutas: db_init crea el directorio de datos. */
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
    snprintf(g_backup_path, sizeof(g_backup_path), "%s" TEST_SEP "%s_backup.db", data_dir,
             PERFIL_TEST);
    snprintf(g_invalido_path, sizeof(g_invalido_path), "%s" TEST_SEP "%s_invalido.db", data_dir,
             PERFIL_TEST);
    snprintf(g_wal_path, sizeof(g_wal_path), "%s-wal", g_db_path);
    snprintf(g_inicio_path, sizeof(g_inicio_path), "%s" TEST_SEP "mifutbol_%s.db",
             get_backups_dir(), PERFIL_TEST);
    snprintf(g_log_path, sizeof(g_log_path), "%s" TEST_SEP "mifutbol_%s.log", data_dir,
             PERFIL_TEST);
    db_close();

    UNITY_BEGIN();

    RUN_TEST(test_backup_incluye_filas_del_wal);
    RUN_TEST(test_backup_de_inicio_solo_con_datos);
    RUN_TEST(test_restaurar_deja_datos_del_backup);
    RUN_TEST(test_restaurar_rechaza_origen_invalido);
    RUN_TEST(test_user_version_avanza_y_backfill_no_se_repite);
    RUN_TEST(test_restaurar_backup_viejo_migra_version);

    int fallos = UNITY_END();
    remove(g_log_path);
    return fallos;
}
