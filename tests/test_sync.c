#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

/*
 * Parte local de la sincronizacion, sin red: backfill de uuid, triggers que
 * encolan cambios en sync_dirty, compactacion de la cola, busqueda por uuid y
 * borrado local (con dependencias y duplicados con el mismo uuid). Usa un
 * perfil propio (mifutbol_test_sync.db en el directorio de datos) y lo borra
 * al terminar. No configura servidores ni hace llamadas HTTP.
 */

#include "test_db_fixture.h"
#include "sync.h"
#include <stdio.h>
#include <string.h>

#define PERFIL_TEST "test_sync"
#define UUID_DUPLICADO "11111111-1111-4111-8111-111111111111"
#define UUID_OTRO "22222222-2222-4222-8222-222222222222"

/* Copia el primer valor de texto de la consulta (vacio si no hay fila). */
static void consultar_texto(const char *sql, char *salida, size_t tam)
{
    sqlite3_stmt *stmt = NULL;
    salida[0] = '\0';
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_text(stmt, 0))
    {
        snprintf(salida, tam, "%s", (const char *)sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
}

/* Inserta un partido; @p uuid NULL lo deja sin uuid, como la aplicacion. */
static void insertar_partido(int id, const char *uuid)
{
    char sql[320];
    if (uuid)
    {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, "
                 "mes_anio, uuid) VALUES(%d, 1, '2025-05-%02d 20:00', 1, 0, 1, '2025-05', '%s');",
                 id, id, uuid);
    }
    else
    {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, "
                 "mes_anio) VALUES(%d, 1, '2025-05-%02d 20:00', 1, 0, 1, '2025-05');",
                 id, id);
    }
    exec_ok(sql);
}

void setUp(void)
{
    borrar_base(g_db_path);
    TEST_ASSERT_TRUE(db_init());
    exec_ok("INSERT INTO camiseta(id, nombre) VALUES(1, 'Titular');");
    exec_ok("INSERT INTO cancha(id, nombre) VALUES(1, 'Club Norte');");
    TEST_ASSERT_TRUE(sync_init_db());
}

void tearDown(void)
{
    db_close();
    borrar_base(g_db_path);
}

/* backfill_uuid: cada partido sin uuid (NULL o en blanco) recibe uno unico,
 * los que ya tenian se conservan y no queda ninguna transaccion abierta. */
static void test_backfill_asigna_uuid_unico(void)
{
    insertar_partido(1, NULL);
    insertar_partido(2, NULL);
    insertar_partido(3, "   ");
    insertar_partido(4, UUID_OTRO);
    insertar_partido(5, NULL);

    TEST_ASSERT_TRUE(sync_init_db());

    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM partido "
                                           "WHERE uuid IS NULL OR TRIM(uuid) = '';"));
    TEST_ASSERT_EQUAL_INT(5, consultar_int("SELECT COUNT(DISTINCT uuid) FROM partido;"));
    TEST_ASSERT_EQUAL_INT(36, consultar_int("SELECT MIN(LENGTH(uuid)) FROM partido;"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido "
                                           "WHERE id = 4 AND uuid = '" UUID_OTRO "';"));
    TEST_ASSERT_TRUE(sqlite3_get_autocommit(db));

    /* Un segundo arranque no vuelve a tocar los uuid asignados. */
    char antes[512];
    char despues[512];
    consultar_texto("SELECT GROUP_CONCAT(uuid, ',') FROM (SELECT uuid FROM partido ORDER BY id);",
                    antes, sizeof(antes));
    TEST_ASSERT_TRUE(sync_init_db());
    consultar_texto("SELECT GROUP_CONCAT(uuid, ',') FROM (SELECT uuid FROM partido ORDER BY id);",
                    despues, sizeof(despues));
    TEST_ASSERT_EQUAL_STRING(antes, despues);
}

/* Los triggers encolan altas, cambios y bajas; la renumeracion de IDs no. */
static void test_triggers_encolan_cambios(void)
{
    insertar_partido(1, UUID_OTRO);
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM sync_dirty WHERE uuid = '" UUID_OTRO
                                           "' AND operacion = 'upsert' AND estado = 'pendiente' "
                                           "AND partido_id = 1;"));

    exec_ok("UPDATE partido SET goles = 3 WHERE id = 1;");
    TEST_ASSERT_EQUAL_INT(2, consultar_int("SELECT COUNT(*) FROM sync_dirty "
                                           "WHERE operacion = 'upsert';"));

    /* Renumerar no es un cambio de datos. */
    exec_ok("UPDATE partido SET id = 7 WHERE id = 1;");
    TEST_ASSERT_EQUAL_INT(2, consultar_int("SELECT COUNT(*) FROM sync_dirty;"));

    exec_ok("DELETE FROM partido WHERE id = 7;");
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM sync_dirty WHERE uuid = '" UUID_OTRO
                                           "' AND operacion = 'eliminar' AND partido_id = 7;"));
    /* Pendientes se cuentan por uuid, no por fila. */
    TEST_ASSERT_EQUAL_INT(1, sync_queue_count_pending());

    SyncDirty siguiente;
    TEST_ASSERT_TRUE(sync_queue_next(&siguiente));
    TEST_ASSERT_EQUAL_STRING(UUID_OTRO, siguiente.uuid);
    TEST_ASSERT_EQUAL_STRING("upsert", siguiente.operacion);

    TEST_ASSERT_TRUE(sync_queue_mark_done(UUID_OTRO));
    TEST_ASSERT_EQUAL_INT(0, sync_queue_count_pending());
    TEST_ASSERT_FALSE(sync_queue_next(&siguiente));
}

/* Al iniciar, la cola deja una fila por uuid, operacion y estado, y descarta
 * las filas sin uuid (los partidos ya se reencolaron con su uuid nuevo). */
static void test_init_compacta_la_cola(void)
{
    insertar_partido(1, UUID_OTRO);
    exec_ok("UPDATE partido SET goles = 2 WHERE id = 1;");
    exec_ok("UPDATE partido SET goles = 3 WHERE id = 1;");
    insertar_partido(2, NULL);
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM sync_dirty WHERE uuid IS NULL;"));

    TEST_ASSERT_TRUE(sync_init_db());

    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM sync_dirty WHERE uuid = '" UUID_OTRO
                                           "';"));
    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM sync_dirty "
                                           "WHERE uuid IS NULL OR TRIM(uuid) = '';"));
    /* El partido 2 quedo encolado con el uuid del backfill. */
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM sync_dirty d JOIN partido p "
                                           "ON p.uuid = d.uuid WHERE p.id = 2;"));
    TEST_ASSERT_EQUAL_INT(2, sync_queue_count_pending());
}

/* sync_buscar_partido_por_uuid: 1 y el id si existe, 0 si no. */
static void test_buscar_partido_por_uuid(void)
{
    insertar_partido(3, UUID_OTRO);
    long long id = 0;
    TEST_ASSERT_EQUAL_INT(1, sync_test_buscar_partido_por_uuid(UUID_OTRO, &id));
    TEST_ASSERT_EQUAL_INT(3, (int)id);

    id = 0;
    TEST_ASSERT_EQUAL_INT(0, sync_test_buscar_partido_por_uuid(UUID_DUPLICADO, &id));
    TEST_ASSERT_EQUAL_INT(0, (int)id);
}

/* sync_eliminar_local borra todos los partidos con ese uuid (duplicados
 * incluidos) con sus dependencias, saca el uuid de la cola y no toca al resto. */
static void test_eliminar_local_borra_duplicados_y_dependencias(void)
{
    exec_ok("CREATE TABLE IF NOT EXISTS partido_meta (partido_id INTEGER PRIMARY KEY, "
            "favorito INTEGER DEFAULT 0);");
    exec_ok("CREATE TABLE IF NOT EXISTS partido_tag (id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "partido_id INTEGER NOT NULL, tag TEXT NOT NULL, UNIQUE(partido_id, tag));");
    insertar_partido(1, UUID_DUPLICADO);
    insertar_partido(2, UUID_DUPLICADO);
    insertar_partido(3, UUID_OTRO);
    exec_ok("INSERT INTO partido_meta(partido_id, favorito) VALUES(1, 1), (2, 1), (3, 1);");
    exec_ok("INSERT INTO partido_tag(partido_id, tag) VALUES(1, 'final'), (2, 'final'), "
            "(2, 'lluvia'), (3, 'final');");

    TEST_ASSERT_EQUAL_INT(1, sync_test_eliminar_local(UUID_DUPLICADO));

    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM partido WHERE uuid = '"
                                           UUID_DUPLICADO "';"));
    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM partido_meta "
                                           "WHERE partido_id IN (1, 2);"));
    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM partido_tag "
                                           "WHERE partido_id IN (1, 2);"));
    /* El borrado vino del servidor: no se reencola para subir. */
    TEST_ASSERT_EQUAL_INT(0, consultar_int("SELECT COUNT(*) FROM sync_dirty WHERE uuid = '"
                                           UUID_DUPLICADO "';"));
    /* Los IDs borrados no se reutilizan. */
    TEST_ASSERT_EQUAL_INT(2, consultar_int("SELECT ultimo FROM partido_id_secuencia WHERE id = 1;"));

    /* El otro partido sigue intacto, con sus dependencias y en la cola. */
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 3;"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido_meta WHERE partido_id = 3;"));
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido_tag WHERE partido_id = 3;"));
    TEST_ASSERT_EQUAL_INT(1, sync_queue_count_pending());
    TEST_ASSERT_TRUE(sqlite3_get_autocommit(db));

    /* Sin partidos con ese uuid no hay nada que hacer: igual se da por aplicado. */
    TEST_ASSERT_EQUAL_INT(1, sync_test_eliminar_local(UUID_DUPLICADO));
    TEST_ASSERT_EQUAL_INT(0, sync_test_eliminar_local(NULL));
}

/* Dentro de una transaccion del llamador, el SAVEPOINT no la cierra. */
static void test_eliminar_local_dentro_de_transaccion(void)
{
    insertar_partido(1, UUID_DUPLICADO);
    exec_ok("BEGIN;");
    TEST_ASSERT_EQUAL_INT(1, sync_test_eliminar_local(UUID_DUPLICADO));
    TEST_ASSERT_FALSE(sqlite3_get_autocommit(db));
    exec_ok("ROLLBACK;");
    TEST_ASSERT_EQUAL_INT(1, consultar_int("SELECT COUNT(*) FROM partido WHERE id = 1;"));
}

int main(void)
{
    if (!fixture_iniciar(PERFIL_TEST, NULL, 0, NULL))
    {
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_backfill_asigna_uuid_unico);
    RUN_TEST(test_triggers_encolan_cambios);
    RUN_TEST(test_init_compacta_la_cola);
    RUN_TEST(test_buscar_partido_por_uuid);
    RUN_TEST(test_eliminar_local_borra_duplicados_y_dependencias);
    RUN_TEST(test_eliminar_local_dentro_de_transaccion);

    return fixture_terminar();
}
