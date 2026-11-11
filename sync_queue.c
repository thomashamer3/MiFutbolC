#include "sync_internal.h"
#include "sync_entidades.h"

#include "db.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copiar(char *destino, size_t tam, const char *origen)
{
    if (!destino || tam == 0)
    {
        return;
    }
    if (!origen)
    {
        destino[0] = '\0';
        return;
    }
    snprintf(destino, tam, "%s", origen);
}

static int ejecutar(const char *sql)
{
    if (!db || !sql)
    {
        return 0;
    }
    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (rc != SQLITE_OK)
    {
        if (error)
        {
            app_log_event("SYNC", error);
            sqlite3_free(error);
        }
        return 0;
    }
    return 1;
}

static int asegurar_tablas(void)
{
    if (!db)
    {
        return 0;
    }
    if (!sync_columna_existe(db, "partido", "uuid") &&
            !ejecutar("ALTER TABLE partido ADD COLUMN uuid TEXT;"))
    {
        return 0;
    }
    if (!sync_columna_existe(db, "partido", "actualizado_en") &&
            !ejecutar("ALTER TABLE partido ADD COLUMN actualizado_en TEXT;"))
    {
        return 0;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS sync_config ("
        " clave TEXT PRIMARY KEY,"
        " valor TEXT,"
        " actualizado_en TEXT);"

        "CREATE TABLE IF NOT EXISTS sync_dirty ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " partido_id INTEGER,"
        " uuid TEXT,"
        " operacion TEXT NOT NULL DEFAULT 'upsert',"
        " estado TEXT NOT NULL DEFAULT 'pendiente',"
        " ultimo_error TEXT,"
        " creado_en TEXT NOT NULL);"

        "CREATE INDEX IF NOT EXISTS idx_sync_dirty_estado "
        "ON sync_dirty (estado, id);"
        "CREATE INDEX IF NOT EXISTS idx_sync_dirty_uuid ON sync_dirty (uuid);"

        "CREATE TABLE IF NOT EXISTS sync_conflicto ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " uuid TEXT NOT NULL UNIQUE,"
        " local_json TEXT,"
        " remoto_json TEXT,"
        " descripcion TEXT,"
        " creado_en TEXT NOT NULL);"

        "CREATE TRIGGER IF NOT EXISTS trg_partido_sync_insert "
        "AFTER INSERT ON partido BEGIN "
        " INSERT INTO sync_dirty (partido_id, uuid, operacion, estado, creado_en)"
        " VALUES (new.id, new.uuid, 'upsert', 'pendiente',"
        " datetime('now','localtime'));"
        " END;"

        "CREATE TRIGGER IF NOT EXISTS trg_partido_sync_update "
        "AFTER UPDATE ON partido BEGIN "
        " INSERT INTO sync_dirty (partido_id, uuid, operacion, estado, creado_en)"
        " VALUES (new.id, new.uuid, 'upsert', 'pendiente',"
        " datetime('now','localtime'));"
        " END;"

        "CREATE TRIGGER IF NOT EXISTS trg_partido_sync_delete "
        "AFTER DELETE ON partido BEGIN "
        " INSERT INTO sync_dirty (partido_id, uuid, operacion, estado, creado_en)"
        " VALUES (old.id, old.uuid, 'eliminar', 'pendiente',"
        " datetime('now','localtime'));"
        " END;";

    return ejecutar(sql);
}

static void backfill_uuid(void)
{
    if (!db)
    {
        return;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "SELECT id FROM partido "
                           "WHERE uuid IS NULL OR TRIM(uuid) = '';",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }
    int ids[2048];
    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && total < 2048)
    {
        ids[total++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    for (int i = 0; i < total; i++)
    {
        char uuid[SYNC_UUID_SIZE];
        sync_uuid_generar(uuid, sizeof(uuid));
        sqlite3_stmt *upd = NULL;
        if (sqlite3_prepare_v2(db, "UPDATE partido SET uuid=? WHERE id=?;", -1,
                               &upd, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(upd, 1, uuid, -1, DB_TRANSIENT);
            sqlite3_bind_int(upd, 2, ids[i]);
            sqlite3_step(upd);
            sqlite3_finalize(upd);
        }
    }
}

int sync_init_db(void)
{
    if (!db)
    {
        return 0;
    }
    if (!asegurar_tablas())
    {
        return 0;
    }
    backfill_uuid();
    if (!sync_entidades_init_db())
    {
        return 0;
    }

    char valor[SYNC_URL_SIZE];
    if ((sync_config_get("url_local", valor, sizeof(valor)) != 1 || !valor[0]) &&
            utils_get_env_var_copy("MIFUTBOL_SYNC_URL", valor, sizeof(valor)) &&
            valor[0])
    {
        sync_config_set("url_local", valor);
    }
    if ((sync_config_get("email", valor, sizeof(valor)) != 1 || !valor[0]) &&
            utils_get_env_var_copy("MIFUTBOL_SYNC_EMAIL", valor, sizeof(valor)) &&
            valor[0])
    {
        sync_config_set("email", valor);
    }
    return 1;
}

int sync_config_get(const char *clave, char *salida, size_t tam)
{
    if (salida && tam)
    {
        salida[0] = '\0';
    }
    if (!db || !clave || !salida)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "SELECT valor FROM sync_config WHERE clave=? LIMIT 1;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, clave, -1, DB_TRANSIENT);
    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        copiar(salida, tam, (const char *)sqlite3_column_text(stmt, 0));
        ok = 1;
    }
    sqlite3_finalize(stmt);
    return ok;
}

int sync_config_set(const char *clave, const char *valor)
{
    if (!db || !clave)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO sync_config (clave, valor, actualizado_en)"
                           " VALUES (?, ?, datetime('now','localtime'))"
                           " ON CONFLICT(clave) DO UPDATE SET valor=excluded.valor,"
                           " actualizado_en=excluded.actualizado_en;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, clave, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 2, valor ? valor : "", -1, DB_TRANSIENT);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int sync_config_load(SyncConfig *cfg)
{
    if (!cfg)
    {
        return 0;
    }
    memset(cfg, 0, sizeof(*cfg));
    sync_config_get("url_local", cfg->url_local, sizeof(cfg->url_local));
    sync_config_get("url_remota", cfg->url_remota, sizeof(cfg->url_remota));
    sync_config_get("email", cfg->email, sizeof(cfg->email));
    sync_config_get("token", cfg->token, sizeof(cfg->token));
    sync_config_get("user_id", cfg->user_id, sizeof(cfg->user_id));
    sync_config_get("cursor", cfg->cursor, sizeof(cfg->cursor));
    sync_config_get("servidor_activo", cfg->servidor_activo,
                    sizeof(cfg->servidor_activo));
    sync_config_get("ultima_sync", cfg->ultima_sync, sizeof(cfg->ultima_sync));
    char auto_sync[8];
    sync_config_get("auto_sync", auto_sync, sizeof(auto_sync));
    cfg->auto_sync = atoi(auto_sync) == 1;
    return 1;
}

int sync_config_save(const SyncConfig *cfg)
{
    if (!cfg)
    {
        return 0;
    }
    char auto_sync[8];
    snprintf(auto_sync, sizeof(auto_sync), "%d", cfg->auto_sync ? 1 : 0);
    return sync_config_set("url_local", cfg->url_local) &&
           sync_config_set("url_remota", cfg->url_remota) &&
           sync_config_set("email", cfg->email) &&
           sync_config_set("token", cfg->token) &&
           sync_config_set("user_id", cfg->user_id) &&
           sync_config_set("cursor", cfg->cursor) &&
           sync_config_set("servidor_activo", cfg->servidor_activo) &&
           sync_config_set("ultima_sync", cfg->ultima_sync) &&
           sync_config_set("auto_sync", auto_sync);
}

int sync_queue_next(SyncDirty *salida)
{
    if (!db || !salida)
    {
        return 0;
    }
    memset(salida, 0, sizeof(*salida));
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "SELECT id, partido_id, uuid, operacion, estado,"
                           " COALESCE(ultimo_error,''), COALESCE(creado_en,'')"
                           " FROM sync_dirty"
                           " WHERE estado IN ('pendiente','error')"
                           " ORDER BY id ASC LIMIT 1;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        salida->id = sqlite3_column_int(stmt, 0);
        salida->partido_id = sqlite3_column_int(stmt, 1);
        copiar(salida->uuid, sizeof(salida->uuid),
               (const char *)sqlite3_column_text(stmt, 2));
        copiar(salida->operacion, sizeof(salida->operacion),
               (const char *)sqlite3_column_text(stmt, 3));
        copiar(salida->estado, sizeof(salida->estado),
               (const char *)sqlite3_column_text(stmt, 4));
        copiar(salida->ultimo_error, sizeof(salida->ultimo_error),
               (const char *)sqlite3_column_text(stmt, 5));
        copiar(salida->creado_en, sizeof(salida->creado_en),
               (const char *)sqlite3_column_text(stmt, 6));
        ok = 1;
    }
    sqlite3_finalize(stmt);
    return ok;
}

int sync_queue_mark_done(const char *uuid)
{
    if (!db || !uuid)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM sync_dirty WHERE uuid=?;", -1,
                           &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, DB_TRANSIENT);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int sync_queue_mark_error(int id, const char *error)
{
    if (!db)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "UPDATE sync_dirty SET estado='error', ultimo_error=?"
                           " WHERE uuid=(SELECT uuid FROM sync_dirty WHERE id=?);",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, error ? error : "Error de sincronizacion", -1,
                      DB_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int sync_queue_mark_conflict(const char *uuid)
{
    if (!db || !uuid)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "UPDATE sync_dirty SET estado='conflicto' WHERE uuid=?;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, DB_TRANSIENT);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int sync_queue_count_pending(void)
{
    if (!db)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "SELECT COUNT(DISTINCT uuid) FROM sync_dirty"
                           " WHERE estado IN ('pendiente','error');",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return total;
}

int sync_queue_count_conflicts(void)
{
    if (!db)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sync_conflicto;", -1, &stmt,
                           NULL) != SQLITE_OK)
    {
        return 0;
    }
    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return total;
}

int sync_queue_enqueue_all(void)
{
    if (!db)
    {
        return 0;
    }
    return ejecutar(
               "INSERT INTO sync_dirty (partido_id, uuid, operacion, estado, creado_en)"
               " SELECT id, uuid, 'upsert', 'pendiente', datetime('now','localtime')"
               " FROM partido WHERE uuid IS NOT NULL AND TRIM(uuid) <> '';");
}

int sync_conflict_first(SyncConflicto *salida)
{
    if (!db || !salida)
    {
        return 0;
    }
    memset(salida, 0, sizeof(*salida));
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "SELECT id, uuid, COALESCE(local_json,''),"
                           " COALESCE(remoto_json,'')"
                           " FROM sync_conflicto ORDER BY id ASC LIMIT 1;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        salida->id = sqlite3_column_int(stmt, 0);
        copiar(salida->uuid, sizeof(salida->uuid),
               (const char *)sqlite3_column_text(stmt, 1));
        const char *local = (const char *)sqlite3_column_text(stmt, 2);
        const char *remoto = (const char *)sqlite3_column_text(stmt, 3);
        salida->local_json = local ? strdup(local) : NULL;
        salida->remoto_json = remoto ? strdup(remoto) : NULL;
        ok = 1;
    }
    sqlite3_finalize(stmt);
    return ok;
}

void sync_conflict_free(SyncConflicto *c)
{
    if (!c)
    {
        return;
    }
    free(c->local_json);
    free(c->remoto_json);
    c->local_json = NULL;
    c->remoto_json = NULL;
}
