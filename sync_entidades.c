#include "sync_entidades.h"
#include "sync_internal.h"

#include "cJSON.h"
#include "db.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COL_CAMISETAS "camisetas_sync"
#define COL_BOTINES "botines_sync"

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
            app_log_event("SYNC-ENTIDADES", error);
            sqlite3_free(error);
        }
        return 0;
    }
    return 1;
}

static void ahora_iso(char *salida, size_t tam)
{
    if (!salida || tam == 0)
    {
        return;
    }
    time_t momento = time(NULL);
    struct tm utc;
#ifdef _WIN32
    gmtime_s(&utc, &momento);
#else
    gmtime_r(&momento, &utc);
#endif
    strftime(salida, tam, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

static int comparar_fechas(const char *a, const char *b)
{
    if (!a || !*a)
    {
        return -1;
    }
    if (!b || !*b)
    {
        return 1;
    }
    char izquierda[64];
    char derecha[64];
    copiar(izquierda, sizeof(izquierda), a);
    copiar(derecha, sizeof(derecha), b);
    for (char *p = izquierda; *p; p++)
    {
        if (*p == ' ')
        {
            *p = 'T';
        }
    }
    for (char *p = derecha; *p; p++)
    {
        if (*p == ' ')
        {
            *p = 'T';
        }
    }
    return strcmp(izquierda, derecha);
}

static const char *texto_columna(sqlite3_stmt *stmt, int indice)
{
    const char *valor = (const char *)sqlite3_column_text(stmt, indice);
    return valor ? valor : "";
}

static const char *json_texto(const cJSON *obj, const char *clave)
{
    const char *valor = sync_json_texto(obj, clave);
    return valor ? valor : "";
}

static int agregar_columna(const char *tabla, const char *columna,
                           const char *definicion)
{
    if (sync_columna_existe(db, tabla, columna))
    {
        return 1;
    }
    char sql[512];
    snprintf(sql, sizeof(sql), "ALTER TABLE %s ADD COLUMN %s %s;", tabla,
             columna, definicion);
    return ejecutar(sql);
}

static int crear_tablas_apoyo(void)
{
    return ejecutar(
               "CREATE TABLE IF NOT EXISTS sync_tombstones ("
               " id INTEGER PRIMARY KEY AUTOINCREMENT,"
               " entidad TEXT NOT NULL,"
               " uuid TEXT NOT NULL,"
               " creado_en TEXT NOT NULL);"
               "CREATE UNIQUE INDEX IF NOT EXISTS idx_sync_tombstone_uuid"
               " ON sync_tombstones(entidad, uuid);"
               "CREATE UNIQUE INDEX IF NOT EXISTS idx_camiseta_uuid"
               " ON camiseta(uuid);"
               "CREATE UNIQUE INDEX IF NOT EXISTS idx_botin_uuid"
               " ON botin(uuid);");
}

static int crear_triggers(void)
{
    return ejecutar(
               "CREATE TRIGGER IF NOT EXISTS trg_camiseta_sync_insert"
               " AFTER INSERT ON camiseta BEGIN"
               "  UPDATE camiseta SET"
               "   uuid = CASE WHEN uuid IS NULL OR TRIM(uuid) = '' THEN lower("
               "     hex(randomblob(4)) || '-' || hex(randomblob(2)) || '-4' ||"
               "     substr(hex(randomblob(2)), 2) || '-' ||"
               "     substr('89ab', abs(random()) % 4 + 1, 1) ||"
               "     substr(hex(randomblob(2)), 2) || '-' || hex(randomblob(6)))"
               "    ELSE uuid END,"
               "   creado_en = CASE WHEN creado_en IS NULL OR TRIM(creado_en) = ''"
               "    THEN strftime('%Y-%m-%dT%H:%M:%fZ','now') ELSE creado_en END,"
               "   actualizado_en = CASE WHEN actualizado_en IS NULL OR"
               "    TRIM(actualizado_en) = ''"
               "    THEN strftime('%Y-%m-%dT%H:%M:%fZ','now') ELSE actualizado_en END"
               "  WHERE id = new.id;"
               " END;"

               "CREATE TRIGGER IF NOT EXISTS trg_camiseta_sync_update"
               " AFTER UPDATE ON camiseta"
               " WHEN COALESCE((SELECT valor FROM sync_config"
               "  WHERE clave = 'aplicando'), '0') <> '1' BEGIN"
               "  UPDATE camiseta SET actualizado_en ="
               "   strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id = new.id;"
               " END;"

               "CREATE TRIGGER IF NOT EXISTS trg_camiseta_sync_delete"
               " AFTER DELETE ON camiseta"
               " WHEN COALESCE((SELECT valor FROM sync_config"
               "  WHERE clave = 'aplicando'), '0') <> '1'"
               "  AND old.uuid IS NOT NULL AND TRIM(old.uuid) <> '' BEGIN"
               "  INSERT INTO sync_tombstones(entidad, uuid, creado_en)"
               "   VALUES('camiseta', old.uuid,"
               "    strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
               "   ON CONFLICT(entidad, uuid) DO UPDATE SET"
               "    creado_en = excluded.creado_en;"
               " END;"

               "CREATE TRIGGER IF NOT EXISTS trg_botin_sync_insert"
               " AFTER INSERT ON botin BEGIN"
               "  UPDATE botin SET"
               "   uuid = CASE WHEN uuid IS NULL OR TRIM(uuid) = '' THEN lower("
               "     hex(randomblob(4)) || '-' || hex(randomblob(2)) || '-4' ||"
               "     substr(hex(randomblob(2)), 2) || '-' ||"
               "     substr('89ab', abs(random()) % 4 + 1, 1) ||"
               "     substr(hex(randomblob(2)), 2) || '-' || hex(randomblob(6)))"
               "    ELSE uuid END,"
               "   creado_en = CASE WHEN creado_en IS NULL OR TRIM(creado_en) = ''"
               "    THEN strftime('%Y-%m-%dT%H:%M:%fZ','now') ELSE creado_en END,"
               "   actualizado_en = CASE WHEN actualizado_en IS NULL OR"
               "    TRIM(actualizado_en) = ''"
               "    THEN strftime('%Y-%m-%dT%H:%M:%fZ','now') ELSE actualizado_en END"
               "  WHERE id = new.id;"
               " END;"

               "CREATE TRIGGER IF NOT EXISTS trg_botin_sync_update"
               " AFTER UPDATE ON botin"
               " WHEN COALESCE((SELECT valor FROM sync_config"
               "  WHERE clave = 'aplicando'), '0') <> '1' BEGIN"
               "  UPDATE botin SET actualizado_en ="
               "   strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id = new.id;"
               " END;"

               "CREATE TRIGGER IF NOT EXISTS trg_botin_sync_delete"
               " AFTER DELETE ON botin"
               " WHEN COALESCE((SELECT valor FROM sync_config"
               "  WHERE clave = 'aplicando'), '0') <> '1'"
               "  AND old.uuid IS NOT NULL AND TRIM(old.uuid) <> '' BEGIN"
               "  INSERT INTO sync_tombstones(entidad, uuid, creado_en)"
               "   VALUES('botin', old.uuid,"
               "    strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
               "   ON CONFLICT(entidad, uuid) DO UPDATE SET"
               "    creado_en = excluded.creado_en;"
               " END;");
}

static int backfill_entidades(void)
{
    return ejecutar(
               "UPDATE camiseta SET uuid = lower("
               " hex(randomblob(4)) || '-' || hex(randomblob(2)) || '-4' ||"
               " substr(hex(randomblob(2)), 2) || '-' ||"
               " substr('89ab', abs(random()) % 4 + 1, 1) ||"
               " substr(hex(randomblob(2)), 2) || '-' || hex(randomblob(6)))"
               " WHERE uuid IS NULL OR TRIM(uuid) = '';"

               "UPDATE camiseta SET creado_en ="
               " strftime('%Y-%m-%dT%H:%M:%fZ','now')"
               " WHERE creado_en IS NULL OR TRIM(creado_en) = '';"

               "UPDATE camiseta SET actualizado_en = creado_en"
               " WHERE actualizado_en IS NULL OR TRIM(actualizado_en) = '';"

               "UPDATE botin SET uuid = lower("
               " hex(randomblob(4)) || '-' || hex(randomblob(2)) || '-4' ||"
               " substr(hex(randomblob(2)), 2) || '-' ||"
               " substr('89ab', abs(random()) % 4 + 1, 1) ||"
               " substr(hex(randomblob(2)), 2) || '-' || hex(randomblob(6)))"
               " WHERE uuid IS NULL OR TRIM(uuid) = '';"

               "UPDATE botin SET creado_en ="
               " strftime('%Y-%m-%dT%H:%M:%fZ','now')"
               " WHERE creado_en IS NULL OR TRIM(creado_en) = '';"

               "UPDATE botin SET actualizado_en = creado_en"
               " WHERE actualizado_en IS NULL OR TRIM(actualizado_en) = '';");
}

int sync_entidades_init_db(void)
{
    if (!db)
    {
        return 0;
    }

    if (!agregar_columna("camiseta", "uuid", "TEXT") ||
            !agregar_columna("camiseta", "creado_en", "TEXT") ||
            !agregar_columna("camiseta", "actualizado_en", "TEXT"))
    {
        return 0;
    }

    if (!agregar_columna("botin", "uuid", "TEXT") ||
            !agregar_columna("botin", "creado_en", "TEXT") ||
            !agregar_columna("botin", "actualizado_en", "TEXT") ||
            !agregar_columna("botin", "marca", "TEXT DEFAULT ''") ||
            !agregar_columna("botin", "modelo", "TEXT DEFAULT ''") ||
            !agregar_columna("botin", "talla", "TEXT DEFAULT ''") ||
            !agregar_columna("botin", "superficie", "TEXT DEFAULT ''") ||
            !agregar_columna("botin", "estado_fisico", "TEXT DEFAULT ''") ||
            !agregar_columna("botin", "fecha_compra", "TEXT DEFAULT ''") ||
            !agregar_columna("botin", "costo_centavos", "INTEGER DEFAULT 0") ||
            !agregar_columna("botin", "imagen_ruta", "TEXT DEFAULT ''") ||
            !agregar_columna("botin", "predeterminado", "INTEGER DEFAULT 0"))
    {
        return 0;
    }

    int ok = crear_tablas_apoyo() && backfill_entidades() && crear_triggers();
    if (ok)
    {
        sync_config_set("aplicando", "0");
    }
    return ok;
}

/* --------------------------------------------------------------- HTTP comun */

/** Datos del registro remoto que corresponde a un uuid. */
typedef struct
{
    char id[64];
    char actualizado[64];
} SyncRemoto;

static int buscar_remoto(const char *base, const char *token,
                         const char *coleccion, const char *uuid,
                         SyncRemoto *remoto)
{
    if (!remoto)
    {
        return -1;
    }
    remoto->id[0] = '\0';
    remoto->actualizado[0] = '\0';
    char url[SYNC_URL_SIZE + 512];
    snprintf(url, sizeof(url),
             "%s/api/collections/%s/records?perPage=1&filter=uuid='%s'", base,
             coleccion, uuid);
    SyncHttp respuesta;
    if (!sync_http_request("GET", url, token, NULL, &respuesta))
    {
        sync_http_free(&respuesta);
        return -1;
    }
    if (respuesta.status == 401 || respuesta.status == 403)
    {
        sync_http_free(&respuesta);
        return -2;
    }
    if (respuesta.status < 200 || respuesta.status >= 300 || !respuesta.body)
    {
        sync_http_free(&respuesta);
        return -1;
    }
    cJSON *json = cJSON_Parse(respuesta.body);
    sync_http_free(&respuesta);
    if (!json)
    {
        return -1;
    }
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(json, "items");
    int encontrado = 0;
    if (cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0)
    {
        const cJSON *item = cJSON_GetArrayItem(items, 0);
        copiar(remoto->id, sizeof(remoto->id),
               sync_json_texto(item, "id"));
        copiar(remoto->actualizado, sizeof(remoto->actualizado),
               sync_json_texto(item, "updated_at_client"));
        encontrado = cJSON_IsObject(item) ? 1 : 0;
    }
    cJSON_Delete(json);
    return encontrado;
}

static int enviar_registro(const char *base, const char *token,
                           const char *coleccion, const char *remoto_id,
                           const cJSON *registro)
{
    char *cuerpo = cJSON_PrintUnformatted(registro);
    if (!cuerpo)
    {
        return -1;
    }
    char url[SYNC_URL_SIZE + 512];
    if (remoto_id && *remoto_id)
    {
        snprintf(url, sizeof(url), "%s/api/collections/%s/records/%s", base,
                 coleccion, remoto_id);
    }
    else
    {
        snprintf(url, sizeof(url), "%s/api/collections/%s/records", base,
                 coleccion);
    }
    SyncHttp respuesta;
    int ok = sync_http_request(remoto_id && *remoto_id ? "PATCH" : "POST", url,
                               token, cuerpo, &respuesta);
    free(cuerpo);
    if (!ok)
    {
        sync_http_free(&respuesta);
        return -1;
    }
    int resultado = 1;
    if (respuesta.status == 401 || respuesta.status == 403)
    {
        resultado = -2;
    }
    else if (respuesta.status < 200 || respuesta.status >= 300)
    {
        resultado = -1;
    }
    sync_http_free(&respuesta);
    return resultado;
}

static cJSON *crear_registro(const char *uuid, const char *owner,
                             const char *nombre, const cJSON *payload,
                             const char *actualizado)
{
    cJSON *registro = cJSON_CreateObject();
    if (!registro)
    {
        return NULL;
    }
    cJSON_AddStringToObject(registro, "uuid", uuid ? uuid : "");
    cJSON_AddStringToObject(registro, "owner", owner ? owner : "");
    cJSON_AddStringToObject(registro, "nombre", nombre ? nombre : "");
    cJSON_AddBoolToObject(registro, "deleted", 0);
    cJSON_AddItemToObject(registro, "payload",
                          payload ? cJSON_Duplicate(payload, 1)
                          : cJSON_CreateObject());
    cJSON_AddStringToObject(registro, "updated_at_client",
                            actualizado ? actualizado : "");
    return registro;
}

/* ------------------------------------------------------------------ Subida */

/* Sube una fila de camisetas. Devuelve 1 si se proceso, -1 si fallo la
   creacion del payload y -2 si la sesion ya no es valida. */
static int subir_fila_camiseta(sqlite3_stmt *stmt, const char *base,
                               const SyncConfig *cfg, SyncResultado *res)
{
    const char *uuid = texto_columna(stmt, 0);
    if (!uuid || !*uuid)
    {
        return 1;
    }
    SyncRemoto remoto_datos;
    int remoto = buscar_remoto(base, cfg->token, COL_CAMISETAS, uuid,
                               &remoto_datos);
    if (remoto == -2)
    {
        return -2;
    }
    const char *actualizado = texto_columna(stmt, 15);
    if (remoto < 0)
    {
        res->errores++;
        return 1;
    }
    if (remoto == 1 &&
            comparar_fechas(remoto_datos.actualizado, actualizado) >= 0)
    {
        return 1;
    }

    cJSON *payload = cJSON_CreateObject();
    if (!payload)
    {
        return -1;
    }
    cJSON_AddStringToObject(payload, "nombre",
                            texto_columna(stmt, 1));
    cJSON_AddStringToObject(payload, "color_principal",
                            texto_columna(stmt, 2));
    cJSON_AddStringToObject(payload, "color_secundario",
                            texto_columna(stmt, 3));
    cJSON_AddStringToObject(payload, "marca",
                            texto_columna(stmt, 4));
    cJSON_AddStringToObject(payload, "modelo",
                            texto_columna(stmt, 5));
    cJSON_AddStringToObject(payload, "temporada",
                            texto_columna(stmt, 6));
    cJSON_AddStringToObject(payload, "estado_fisico",
                            texto_columna(stmt, 7));
    cJSON_AddStringToObject(payload, "fecha_compra",
                            texto_columna(stmt, 8));
    cJSON_AddNumberToObject(payload, "costo_centavos",
                            sqlite3_column_int(stmt, 9));
    cJSON_AddStringToObject(payload, "observaciones",
                            texto_columna(stmt, 10));
    cJSON_AddStringToObject(payload, "proveedor",
                            texto_columna(stmt, 11));
    cJSON_AddBoolToObject(payload, "fue_regalo",
                          sqlite3_column_int(stmt, 12) != 0);
    cJSON_AddStringToObject(payload, "regalo_de",
                            texto_columna(stmt, 13));
    cJSON_AddBoolToObject(payload, "activa",
                          sqlite3_column_int(stmt, 14) != 0);

    cJSON *registro = crear_registro(
                          uuid, cfg->user_id, texto_columna(stmt, 1),
                          payload, actualizado);
    cJSON_Delete(payload);
    int envio = -1;
    if (registro)
    {
        const char *remoto_ref = remoto == 1 ? remoto_datos.id : NULL;
        envio = enviar_registro(base, cfg->token, COL_CAMISETAS, remoto_ref,
                                registro);
    }
    cJSON_Delete(registro);
    if (envio == -2)
    {
        return -2;
    }
    if (envio > 0)
    {
        res->enviados++;
    }
    else
    {
        res->errores++;
    }
    return 1;
}

static int subir_camisetas(const char *base, const SyncConfig *cfg,
                           SyncResultado *res)
{
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(
                db,
                "SELECT uuid, nombre, color_principal, color_secundario, marca,"
                " modelo, temporada, estado_fisico, fecha_compra,"
                " costo_centavos, observaciones, proveedor, fue_regalo,"
                " regalo_de, activa, actualizado_en FROM camiseta;",
                -1, &stmt, NULL) != SQLITE_OK)
    {
        return -1;
    }

    int resultado = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int estado = subir_fila_camiseta(stmt, base, cfg, res);
        if (estado == -2)
        {
            resultado = -2;
            break;
        }
        if (estado == -1)
        {
            resultado = -1;
        }
    }
    sqlite3_finalize(stmt);
    return resultado;
}

/* Sube una fila de botines. Mismo contrato que subir_fila_camiseta. */
static int subir_fila_botin(sqlite3_stmt *stmt, const char *base,
                            const SyncConfig *cfg, SyncResultado *res)
{
    const char *uuid = texto_columna(stmt, 0);
    if (!uuid || !*uuid)
    {
        return 1;
    }
    SyncRemoto remoto_datos;
    int remoto = buscar_remoto(base, cfg->token, COL_BOTINES, uuid,
                               &remoto_datos);
    if (remoto == -2)
    {
        return -2;
    }
    const char *actualizado = texto_columna(stmt, 10);
    if (remoto < 0)
    {
        res->errores++;
        return 1;
    }
    if (remoto == 1 &&
            comparar_fechas(remoto_datos.actualizado, actualizado) >= 0)
    {
        return 1;
    }

    cJSON *payload = cJSON_CreateObject();
    if (!payload)
    {
        return -1;
    }
    cJSON_AddStringToObject(payload, "nombre",
                            texto_columna(stmt, 1));
    cJSON_AddStringToObject(payload, "marca",
                            texto_columna(stmt, 2));
    cJSON_AddStringToObject(payload, "modelo",
                            texto_columna(stmt, 3));
    cJSON_AddStringToObject(payload, "talla",
                            texto_columna(stmt, 4));
    cJSON_AddStringToObject(payload, "superficie",
                            texto_columna(stmt, 5));
    cJSON_AddStringToObject(payload, "estado_fisico",
                            texto_columna(stmt, 6));
    cJSON_AddStringToObject(payload, "fecha_compra",
                            texto_columna(stmt, 7));
    cJSON_AddNumberToObject(payload, "costo_centavos",
                            sqlite3_column_int(stmt, 8));
    cJSON_AddBoolToObject(payload, "activa",
                          sqlite3_column_int(stmt, 9) != 0);

    cJSON *registro = crear_registro(
                          uuid, cfg->user_id, texto_columna(stmt, 1),
                          payload, actualizado);
    cJSON_Delete(payload);
    int envio = -1;
    if (registro)
    {
        const char *remoto_ref = remoto == 1 ? remoto_datos.id : NULL;
        envio = enviar_registro(base, cfg->token, COL_BOTINES, remoto_ref,
                                registro);
    }
    cJSON_Delete(registro);
    if (envio == -2)
    {
        return -2;
    }
    if (envio > 0)
    {
        res->enviados++;
    }
    else
    {
        res->errores++;
    }
    return 1;
}

static int subir_botines(const char *base, const SyncConfig *cfg,
                         SyncResultado *res)
{
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(
                db,
                "SELECT uuid, nombre, marca, modelo, talla, superficie,"
                " estado_fisico, fecha_compra, costo_centavos, activa,"
                " actualizado_en FROM botin;",
                -1, &stmt, NULL) != SQLITE_OK)
    {
        return -1;
    }

    int resultado = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int estado = subir_fila_botin(stmt, base, cfg, res);
        if (estado == -2)
        {
            resultado = -2;
            break;
        }
        if (estado == -1)
        {
            resultado = -1;
        }
    }
    sqlite3_finalize(stmt);
    return resultado;
}

/* Envia un tombstone al servidor. Devuelve -2 si la sesion ya no vale,
   1 si quedo resuelto y 0 si hay que reintentarlo. */
static int enviar_tombstone(sqlite3_stmt *stmt, const char *base,
                            const SyncConfig *cfg, const char *entidad,
                            const char *uuid)
{
    const char *coleccion =
        strcmp(entidad, "botin") == 0 ? COL_BOTINES : COL_CAMISETAS;
    SyncRemoto remoto_datos;
    int remoto = buscar_remoto(base, cfg->token, coleccion, uuid,
                               &remoto_datos);
    if (remoto == -2)
    {
        return -2;
    }
    if (remoto < 0)
    {
        return 0;
    }
    if (remoto != 1)
    {
        return 1;
    }

    const char *creado = texto_columna(stmt, 3);
    cJSON *registro = cJSON_CreateObject();
    if (!registro)
    {
        return 0;
    }
    cJSON_AddBoolToObject(registro, "deleted", 1);
    cJSON_AddStringToObject(registro, "updated_at_client",
                            creado ? creado : "");
    int envio = enviar_registro(base, cfg->token, coleccion, remoto_datos.id,
                                registro);
    cJSON_Delete(registro);
    if (envio == -2)
    {
        return -2;
    }
    return envio > 0 ? 1 : 0;
}

static int subir_tombstones(const char *base, const SyncConfig *cfg,
                            SyncResultado *res)
{
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(
                db,
                "SELECT id, entidad, uuid, creado_en FROM sync_tombstones"
                " ORDER BY id ASC;",
                -1, &stmt, NULL) != SQLITE_OK)
    {
        return -1;
    }

    char ids[2048][16];
    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && total < 2048)
    {
        const char *entidad = texto_columna(stmt, 1);
        const char *uuid = texto_columna(stmt, 2);
        int id_fila = sqlite3_column_int(stmt, 0);
        if (!entidad || !uuid || !*uuid)
        {
            snprintf(ids[total], sizeof(ids[total]), "%d", id_fila);
            total++;
            continue;
        }
        int estado = enviar_tombstone(stmt, base, cfg, entidad, uuid);
        if (estado == -2)
        {
            sqlite3_finalize(stmt);
            return -2;
        }
        if (estado == 0)
        {
            res->errores++;
            continue;
        }
        snprintf(ids[total], sizeof(ids[total]), "%d", id_fila);
        total++;
        res->enviados++;
    }
    sqlite3_finalize(stmt);

    for (int i = 0; i < total; i++)
    {
        sqlite3_stmt *borrar = NULL;
        if (sqlite3_prepare_v2(db, "DELETE FROM sync_tombstones WHERE id=?;",
                               -1, &borrar, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int(borrar, 1, atoi(ids[i]));
            sqlite3_step(borrar);
            sqlite3_finalize(borrar);
        }
    }
    return 1;
}

int sync_entidades_subir(const char *base, SyncConfig *cfg, SyncResultado *res)
{
    if (!base || !cfg || !res || !db)
    {
        return 0;
    }
    int camisetas = subir_camisetas(base, cfg, res);
    if (camisetas == -2)
    {
        return -2;
    }
    int botines = subir_botines(base, cfg, res);
    if (botines == -2)
    {
        return -2;
    }
    int tombstones = subir_tombstones(base, cfg, res);
    if (tombstones == -2)
    {
        return -2;
    }
    return 1;
}

/* ------------------------------------------------------------------ Bajada */

static int id_por_nombre(const char *tabla, const char *nombre,
                         char *actualizado, size_t actualizado_tam)
{
    if (actualizado && actualizado_tam)
    {
        actualizado[0] = '\0';
    }
    if (!nombre || !*nombre)
    {
        return 0;
    }
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT id, actualizado_en FROM %s"
             " WHERE nombre = ? COLLATE NOCASE LIMIT 1;",
             tabla);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
        copiar(actualizado, actualizado_tam,
               (const char *)sqlite3_column_text(stmt, 1));
    }
    sqlite3_finalize(stmt);
    return id;
}

static int adoptar_uuid(const char *tabla, int id, const char *uuid)
{
    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE %s SET uuid=? WHERE id=?;", tabla);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static int existe_local(const char *tabla, const char *uuid,
                        char *actualizado, size_t actualizado_tam)
{
    if (actualizado && actualizado_tam)
    {
        actualizado[0] = '\0';
    }
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT actualizado_en FROM %s WHERE uuid=? LIMIT 1;", tabla);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
    int existe = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        existe = 1;
        const char *valor = texto_columna(stmt, 0);
        copiar(actualizado, actualizado_tam, valor);
    }
    sqlite3_finalize(stmt);
    return existe;
}

static int aplicar_camiseta(const cJSON *item, const char *uuid,
                            const char *actualizado, const char *creado)
{
    cJSON *payload = cJSON_GetObjectItemCaseSensitive(item, "payload");
    if (!cJSON_IsObject(payload))
    {
        payload = cJSON_CreateObject();
        if (!payload)
        {
            return 0;
        }
    }
    else
    {
        payload = cJSON_Duplicate(payload, 1);
        if (!payload)
        {
            return 0;
        }
    }

    const char *nombre = json_texto(payload, "nombre");
    if (!nombre || !*nombre)
    {
        nombre = json_texto(item, "nombre");
    }
    if (!nombre)
    {
        nombre = "";
    }

    char local_actualizado[64];
    int existe =
        existe_local("camiseta", uuid, local_actualizado,
                     sizeof(local_actualizado));
    if (!existe)
    {
        int local_id = id_por_nombre("camiseta", nombre, local_actualizado,
                                     sizeof(local_actualizado));
        if (local_id > 0 && adoptar_uuid("camiseta", local_id, uuid))
        {
            existe = 1;
        }
    }
    if (existe && comparar_fechas(local_actualizado, actualizado) > 0)
    {
        cJSON_Delete(payload);
        return 1;
    }
    int ok = 0;
    if (existe)
    {
        static const char *sql =
            "UPDATE camiseta SET nombre=?, color_principal=?,"
            " color_secundario=?, marca=?, modelo=?, temporada=?,"
            " estado_fisico=?, fecha_compra=?, costo_centavos=?,"
            " observaciones=?, proveedor=?, fue_regalo=?, regalo_de=?,"
            " activa=?, actualizado_en=? WHERE uuid=?;";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, json_texto(payload,
                                                  "color_principal"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, json_texto(payload,
                                                  "color_secundario"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, json_texto(payload, "marca"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, json_texto(payload, "modelo"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, json_texto(payload, "temporada"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, json_texto(payload,
                                                  "estado_fisico"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 8, json_texto(payload,
                                                  "fecha_compra"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 9,
                             sync_entero_desde_json(payload,
                                                    "costo_centavos", 0));
            sqlite3_bind_text(stmt, 10, json_texto(payload,
                                                   "observaciones"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 11, json_texto(payload,
                                                   "proveedor"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 12,
                             sync_entero_desde_json(payload, "fue_regalo", 0));
            sqlite3_bind_text(stmt, 13, json_texto(payload, "regalo_de"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 14,
                             sync_entero_desde_json(payload, "activa", 1));
            sqlite3_bind_text(stmt, 15, actualizado, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 16, uuid, -1, SQLITE_TRANSIENT);
            ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        static const char *sql =
            "INSERT INTO camiseta (uuid, nombre, color_principal,"
            " color_secundario, marca, modelo, temporada, estado_fisico,"
            " fecha_compra, costo_centavos, observaciones, proveedor,"
            " fue_regalo, regalo_de, activa, creado_en, actualizado_en)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, json_texto(payload,
                                                  "color_principal"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, json_texto(payload,
                                                  "color_secundario"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, json_texto(payload, "marca"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, json_texto(payload, "modelo"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, json_texto(payload, "temporada"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 8, json_texto(payload,
                                                  "estado_fisico"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 9, json_texto(payload,
                                                  "fecha_compra"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 10,
                             sync_entero_desde_json(payload,
                                                    "costo_centavos", 0));
            sqlite3_bind_text(stmt, 11, json_texto(payload,
                                                   "observaciones"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 12, json_texto(payload,
                                                   "proveedor"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 13,
                             sync_entero_desde_json(payload, "fue_regalo", 0));
            sqlite3_bind_text(stmt, 14, json_texto(payload, "regalo_de"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 15,
                             sync_entero_desde_json(payload, "activa", 1));
            sqlite3_bind_text(stmt, 16, creado, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 17, actualizado, -1, SQLITE_TRANSIENT);
            ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        }
    }
    cJSON_Delete(payload);
    return ok;
}

static int aplicar_botin(const cJSON *item, const char *uuid,
                         const char *actualizado, const char *creado)
{
    cJSON *payload = cJSON_GetObjectItemCaseSensitive(item, "payload");
    if (!cJSON_IsObject(payload))
    {
        payload = cJSON_CreateObject();
        if (!payload)
        {
            return 0;
        }
    }
    else
    {
        payload = cJSON_Duplicate(payload, 1);
        if (!payload)
        {
            return 0;
        }
    }

    const char *nombre = json_texto(payload, "nombre");
    if (!nombre || !*nombre)
    {
        nombre = json_texto(item, "nombre");
    }
    if (!nombre)
    {
        nombre = "";
    }

    char local_actualizado[64];
    int existe =
        existe_local("botin", uuid, local_actualizado,
                     sizeof(local_actualizado));
    if (!existe)
    {
        int local_id = id_por_nombre("botin", nombre, local_actualizado,
                                     sizeof(local_actualizado));
        if (local_id > 0 && adoptar_uuid("botin", local_id, uuid))
        {
            existe = 1;
        }
    }
    if (existe && comparar_fechas(local_actualizado, actualizado) > 0)
    {
        cJSON_Delete(payload);
        return 1;
    }
    int ok = 0;
    if (existe)
    {
        static const char *sql =
            "UPDATE botin SET nombre=?, marca=?, modelo=?, talla=?,"
            " superficie=?, estado_fisico=?, fecha_compra=?,"
            " costo_centavos=?, activa=?, actualizado_en=? WHERE uuid=?;";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, json_texto(payload, "marca"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, json_texto(payload, "modelo"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, json_texto(payload, "talla"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, json_texto(payload, "superficie"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, json_texto(payload,
                                                  "estado_fisico"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, json_texto(payload,
                                                  "fecha_compra"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 8,
                             sync_entero_desde_json(payload,
                                                    "costo_centavos", 0));
            sqlite3_bind_int(stmt, 9,
                             sync_entero_desde_json(payload, "activa", 1));
            sqlite3_bind_text(stmt, 10, actualizado, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 11, uuid, -1, SQLITE_TRANSIENT);
            ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        static const char *sql =
            "INSERT INTO botin (uuid, nombre, marca, modelo, talla,"
            " superficie, estado_fisico, fecha_compra, costo_centavos,"
            " activa, creado_en, actualizado_en)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, json_texto(payload, "marca"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, json_texto(payload, "modelo"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, json_texto(payload, "talla"), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, json_texto(payload, "superficie"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, json_texto(payload,
                                                  "estado_fisico"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 8, json_texto(payload,
                                                  "fecha_compra"),
                              -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 9,
                             sync_entero_desde_json(payload,
                                                    "costo_centavos", 0));
            sqlite3_bind_int(stmt, 10,
                             sync_entero_desde_json(payload, "activa", 1));
            sqlite3_bind_text(stmt, 11, creado, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 12, actualizado, -1, SQLITE_TRANSIENT);
            ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        }
    }
    cJSON_Delete(payload);
    return ok;
}

/* Aplica un borrado remoto. Devuelve 1 si el registro era un borrado,
   0 si no lo era. */
static int aplicar_borrado(const cJSON *item, const char *uuid,
                           const char *actualizado, int es_camiseta,
                           SyncResultado *res)
{
    const cJSON *borrado = cJSON_GetObjectItemCaseSensitive(item, "deleted");
    if (!cJSON_IsTrue(borrado))
    {
        return 0;
    }
    char local_actualizado[64];
    int existe = existe_local(es_camiseta ? "camiseta" : "botin", uuid,
                              local_actualizado, sizeof(local_actualizado));
    if (existe && comparar_fechas(local_actualizado, actualizado) > 0)
    {
        return 1;
    }
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE uuid=?;",
             es_camiseta ? "camiseta" : "botin");
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        res->eliminados++;
    }
    return 1;
}

/* Aplica un registro recibido del servidor a la base local. */
static void procesar_item_descarga(const cJSON *item, int es_camiseta,
                                   SyncResultado *res)
{
    const char *uuid = sync_json_texto(item, "uuid");
    if (!uuid || !*uuid)
    {
        return;
    }
    const char *updated = sync_json_texto(item, "updated_at_client");
    char actualizado[64];
    if (updated && *updated)
    {
        copiar(actualizado, sizeof(actualizado), updated);
    }
    else
    {
        ahora_iso(actualizado, sizeof(actualizado));
    }
    char creado[64];
    copiar(creado, sizeof(creado), actualizado);

    if (aplicar_borrado(item, uuid, actualizado, es_camiseta, res))
    {
        return;
    }

    char local_actualizado[64];
    int existe = existe_local(es_camiseta ? "camiseta" : "botin", uuid,
                              local_actualizado, sizeof(local_actualizado));
    if (existe && comparar_fechas(local_actualizado, actualizado) >= 0)
    {
        return;
    }

    int aplicado = es_camiseta
                   ? aplicar_camiseta(item, uuid, actualizado, creado)
                   : aplicar_botin(item, uuid, actualizado, creado);
    if (aplicado)
    {
        res->importados++;
    }
    else
    {
        res->errores++;
    }
}

/* Descarga una pagina. Devuelve -2 sin sesion, -1 error, 0 fin de paginacion
   y 1 si hay mas paginas que procesar. */
static int bajar_pagina(const char *base, const SyncConfig *cfg,
                        SyncResultado *res, const char *coleccion,
                        int es_camiseta, int pagina, int *total_paginas)
{
    char url[SYNC_URL_SIZE + 512];
    snprintf(url, sizeof(url),
             "%s/api/collections/%s/records?page=%d&perPage=100&"
             "sort=updated",
             base, coleccion, pagina);
    SyncHttp respuesta;
    if (!sync_http_request("GET", url, cfg->token, NULL, &respuesta))
    {
        sync_http_free(&respuesta);
        return -1;
    }
    if (respuesta.status == 401 || respuesta.status == 403)
    {
        sync_http_free(&respuesta);
        return -2;
    }
    if (respuesta.status < 200 || respuesta.status >= 300 ||
            !respuesta.body)
    {
        sync_http_free(&respuesta);
        return -1;
    }
    cJSON *json = cJSON_Parse(respuesta.body);
    sync_http_free(&respuesta);
    if (!json)
    {
        return -1;
    }
    *total_paginas = sync_entero_desde_json(json, "totalPages", 1);
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(json, "items");
    if (!cJSON_IsArray(items))
    {
        cJSON_Delete(json);
        return 0;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, items)
    {
        procesar_item_descarga(item, es_camiseta, res);
    }
    cJSON_Delete(json);
    return 1;
}

static int bajar_coleccion(const char *base, const SyncConfig *cfg,
                           SyncResultado *res, const char *coleccion,
                           int es_camiseta)
{
    int pagina = 1;
    int total_paginas = 1;
    while (pagina <= total_paginas)
    {
        int estado = bajar_pagina(base, cfg, res, coleccion, es_camiseta,
                                  pagina, &total_paginas);
        if (estado == -2)
        {
            return -2;
        }
        if (estado == -1)
        {
            return -1;
        }
        if (estado == 0)
        {
            break;
        }
        pagina++;
    }
    return 1;
}

int sync_entidades_bajar(const char *base, SyncConfig *cfg, SyncResultado *res)
{
    if (!base || !cfg || !res || !db)
    {
        return 0;
    }
    sync_config_set("aplicando", "1");
    int camisetas = bajar_coleccion(base, cfg, res, COL_CAMISETAS, 1);
    int botines = camisetas == -2
                  ? -2
                  : bajar_coleccion(base, cfg, res, COL_BOTINES, 0);
    sync_config_set("aplicando", "0");
    if (camisetas == -2 || botines == -2)
    {
        return -2;
    }
    return 1;
}
