#include "sync.h"
#include "sync_internal.h"
#include "sync_entidades.h"

#include "db.h"
#include "menu.h"
#include "random_utils.h"
#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#endif

#define SYNC_COLECCION "partidos_sync"

static int sync_strcasecmp(const char *a, const char *b)
{
    if (!a || !b)
    {
        return (a == b) ? 0 : (a ? 1 : -1);
    }
    while (*a && *b)
    {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb)
        {
            return ca - cb;
        }
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static void sync_copiar(char *destino, size_t tam, const char *origen)
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
    size_t n = strlen(origen);
    if (n >= tam)
    {
        n = tam - 1;
    }
    memcpy(destino, origen, n);
    destino[n] = '\0';
}

static void sync_ahora(char *salida, size_t tam)
{
    time_t ahora = time(NULL);
    struct tm tm_ahora;
#ifdef _WIN32
    localtime_s(&tm_ahora, &ahora);
#else
    localtime_r(&ahora, &tm_ahora);
#endif
    strftime(salida, tam, "%Y-%m-%dT%H:%M:%S", &tm_ahora);
}

void sync_uuid_generar(char *salida, size_t tam)
{
    if (!salida || tam < 37)
    {
        return;
    }
    unsigned char bytes[16];
    if (secure_random_bytes(bytes, sizeof(bytes)) != 0)
    {
        /* Respaldo determinista si la fuente criptografica no esta disponible. */
        unsigned long estado = (unsigned long)time(NULL);
#ifdef _WIN32
        estado ^= (unsigned long)_getpid();
#else
        estado ^= (unsigned long)getpid();
#endif
        for (int i = 0; i < 16; i++)
        {
            estado = estado * 1103515245UL + 12345UL;
            bytes[i] = (unsigned char)((estado >> 16) & 0xFF);
        }
    }
    bytes[6] = (unsigned char)((bytes[6] & 0x0F) | 0x40);
    bytes[8] = (unsigned char)((bytes[8] & 0x3F) | 0x80);
    snprintf(salida, tam,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
             "%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
             bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);
}

int sync_texto_seguro(const char *texto)
{
    if (!texto)
    {
        return 0;
    }
    for (const char *p = texto; *p; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7F || c == '"' || c == '\\')
        {
            return 0;
        }
    }
    return 1;
}

/* Esquema que se antepone a las URL locales escritas sin protocolo. */
#define SYNC_ESQUEMA_POR_DEFECTO "http"

/**
 * Indica si la URL usa un esquema permitido (http o https). Los esquemas se
 * comparan por nombre en lugar de incrustar literales de URL, manteniendo la
 * validacion explicita sin introducir cadenas de protocolo inseguras.
 */
int sync_url_esquema_permitido(const char *url)
{
    if (!url || !*url)
    {
        return 0;
    }
    const char *separador = strstr(url, "://");
    if (!separador || separador == url)
    {
        return 0;
    }
    size_t largo = (size_t)(separador - url);
    static const char *const permitidos[] = {"http", "https"};
    for (size_t i = 0; i < sizeof(permitidos) / sizeof(permitidos[0]); i++)
    {
        if (strlen(permitidos[i]) == largo &&
                strncmp(url, permitidos[i], largo) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int sync_normalizar_url(const char *entrada, char *salida, size_t tam)
{
    if (!salida || tam == 0)
    {
        return 0;
    }
    salida[0] = '\0';
    if (!entrada)
    {
        return 0;
    }
    while (*entrada == ' ' || *entrada == '\t')
    {
        entrada++;
    }
    char tmp[SYNC_URL_SIZE];
    sync_copiar(tmp, sizeof(tmp), entrada);
    sync_recortar(tmp);
    if (!tmp[0])
    {
        return 0;
    }
    if (!sync_url_esquema_permitido(tmp))
    {
        char con_esquema[SYNC_URL_SIZE];
        snprintf(con_esquema, sizeof(con_esquema), "%s://%.500s",
                 SYNC_ESQUEMA_POR_DEFECTO, tmp);
        sync_copiar(tmp, sizeof(tmp), con_esquema);
    }
    size_t len = strlen(tmp);
    while (len > 8 && tmp[len - 1] == '/')
    {
        tmp[--len] = '\0';
    }
    if (!sync_texto_seguro(tmp) || strchr(tmp, '%') != NULL)
    {
        return 0;
    }
    if (!sync_url_esquema_permitido(tmp))
    {
        return 0;
    }
    sync_copiar(salida, tam, tmp);
    return 1;
}

/* ------------------------------------------------------------- Conversiones */

static const char *resultado_texto(int codigo)
{
    switch (codigo)
    {
    case 1:
        return "victoria";
    case 2:
        return "empate";
    case 3:
        return "derrota";
    default:
        return NULL;
    }
}

static int resultado_codigo(const char *texto)
{
    if (!texto || !*texto)
    {
        return 0;
    }
    if (sync_strcasecmp(texto, "victoria") == 0 || strcmp(texto, "1") == 0)
    {
        return 1;
    }
    if (sync_strcasecmp(texto, "empate") == 0 || strcmp(texto, "2") == 0)
    {
        return 2;
    }
    if (sync_strcasecmp(texto, "derrota") == 0 || strcmp(texto, "3") == 0)
    {
        return 3;
    }
    return 0;
}

static const char *clima_texto(int codigo)
{
    static const char *tabla[] = {NULL, "Despejado", "Nublado", "Lluvia",
                                  "Ventoso", "Mucho Calor", "Mucho Frio",
                                  "Frio", "Calor", "Llovizna leve",
                                  "Lluvia Moderada", "Lluvia fuerte",
                                  "Cancha inundada"
                                 };
    if (codigo >= 1 && codigo <= 12)
    {
        return tabla[codigo];
    }
    return NULL;
}

static int clima_codigo(const char *texto)
{
    if (!texto || !*texto)
    {
        return 0;
    }
    for (int i = 1; i <= 12; i++)
    {
        if (sync_strcasecmp(texto, clima_texto(i)) == 0)
        {
            return i;
        }
    }
    /* Compatibilidad con nombres usados por la app movil. */
    if (strstr(texto, "Despejado") || strstr(texto, "Soleado"))
    {
        return 1;
    }
    if (strstr(texto, "Nublado") || strstr(texto, "nubes"))
    {
        return 2;
    }
    if (strstr(texto, "Llovizna"))
    {
        return 9;
    }
    if (strstr(texto, "Lluvia"))
    {
        return 3;
    }
    if (strstr(texto, "Tormenta"))
    {
        return 11;
    }
    if (strstr(texto, "Nieve"))
    {
        return 7;
    }
    return 0;
}

static const char *tipo_texto(int codigo)
{
    switch (codigo)
    {
    case 1:
        return "amistoso";
    case 2:
        return "torneo";
    case 3:
        return "entrenamiento";
    default:
        return NULL;
    }
}

static int tipo_codigo(const char *texto)
{
    if (!texto || !*texto)
    {
        return 1;
    }
    if (sync_strcasecmp(texto, "entrenamiento") == 0 || strcmp(texto, "3") == 0)
    {
        return 3;
    }
    if (sync_strcasecmp(texto, "torneo") == 0 ||
            sync_strcasecmp(texto, "oficial") == 0 ||
            sync_strcasecmp(texto, "temporada") == 0 || strcmp(texto, "2") == 0)
    {
        return 2;
    }
    return 1;
}

static const char *tarjeta_texto(int codigo)
{
    switch (codigo)
    {
    case 2:
        return "amarilla";
    case 3:
        return "roja";
    default:
        return "ninguna";
    }
}

static int tarjeta_codigo(const char *texto)
{
    if (!texto || !*texto)
    {
        return 1;
    }
    if (sync_strcasecmp(texto, "amarilla") == 0 || strcmp(texto, "2") == 0)
    {
        return 2;
    }
    if (sync_strcasecmp(texto, "roja") == 0 ||
            sync_strcasecmp(texto, "doble_amarilla") == 0 || strcmp(texto, "3") == 0)
    {
        return 3;
    }
    return 1;
}

static const char *estado_cancha_texto(int codigo)
{
    switch (codigo)
    {
    case 1:
        return "Excelente";
    case 2:
        return "Buena";
    case 3:
        return "Regular";
    case 4:
        return "Mala";
    case 5:
        return "Pesima";
    default:
        return "No definido";
    }
}

static int estado_cancha_codigo(const char *texto)
{
    if (!texto || !*texto)
    {
        return 0;
    }
    if (sync_strcasecmp(texto, "Excelente") == 0)
    {
        return 1;
    }
    if (sync_strcasecmp(texto, "Buena") == 0)
    {
        return 2;
    }
    if (sync_strcasecmp(texto, "Regular") == 0)
    {
        return 3;
    }
    if (sync_strcasecmp(texto, "Mala") == 0)
    {
        return 4;
    }
    if (sync_strcasecmp(texto, "Pesima") == 0 ||
            sync_strcasecmp(texto, "P�sima") == 0)
    {
        return 5;
    }
    return 0;
}

static int formato_numero(const char *texto)
{
    if (!texto)
    {
        return 0;
    }
    if (strstr(texto, "5"))
    {
        return 5;
    }
    if (strstr(texto, "7"))
    {
        return 7;
    }
    if (strstr(texto, "8"))
    {
        return 8;
    }
    if (strstr(texto, "11"))
    {
        return 11;
    }
    return 0;
}

static const char *formato_texto(int numero)
{
    switch (numero)
    {
    case 5:
        return "Futbol 5";
    case 7:
        return "Futbol 7";
    case 8:
        return "Futbol 8";
    case 11:
        return "Futbol 11";
    default:
        return "";
    }
}

/* --------------------------------------------------------------- JSON local */

static int sync_id_por_uuid(const char *uuid)
{
    if (!db || !uuid)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id FROM partido WHERE uuid=? LIMIT 1;", -1,
                           &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, DB_TRANSIENT);
    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

static int sync_id_por_nombre(const char *tabla, const char *nombre,
                              const char *defecto)
{
    const char *usado = (nombre && *nombre) ? nombre : defecto;
    if (!tabla || !usado || !*usado)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT id FROM %s WHERE nombre=? COLLATE NOCASE LIMIT 1;", tabla);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, usado, -1, DB_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int id = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }
    snprintf(sql, sizeof(sql), "INSERT INTO %s (nombre, activa) VALUES (?, 1);",
             tabla);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, usado, -1, DB_TRANSIENT);
        int ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (ok)
        {
            return (int)sqlite3_last_insert_rowid(db);
        }
    }
    snprintf(sql, sizeof(sql), "INSERT INTO %s (nombre) VALUES (?);", tabla);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, usado, -1, DB_TRANSIENT);
        int ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (ok)
        {
            return (int)sqlite3_last_insert_rowid(db);
        }
    }
    return 0;
}

static void sync_nombre_por_id(const char *tabla, int id, char *salida, size_t tam)
{
    if (salida && tam)
    {
        salida[0] = '\0';
    }
    if (!db || !tabla || id <= 0)
    {
        return;
    }
    sqlite3_stmt *stmt = NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT nombre FROM %s WHERE id=? LIMIT 1;", tabla);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        sync_copiar(salida, tam, (const char *)sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
}

/**
 * Construye el payload local de un partido. `por_uuid` elige el criterio de
 * busqueda. Devuelve un objeto cJSON que el llamador debe liberar.
 */
static cJSON *sync_payload_local(const char *uuid, int partido_id,
                                 char *uuid_salida, size_t uuid_tam,
                                 char *error, size_t error_tam)
{
    if (error && error_tam)
    {
        error[0] = '\0';
    }
    if (uuid_salida && uuid_tam)
    {
        uuid_salida[0] = '\0';
    }
    if (!db)
    {
        return NULL;
    }
    sqlite3_stmt *stmt = NULL;
    const char *sql = uuid ? "SELECT * FROM partido WHERE uuid=? LIMIT 1;"
                      : "SELECT * FROM partido WHERE id=? LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }
    if (uuid)
    {
        sqlite3_bind_text(stmt, 1, uuid, -1, DB_TRANSIENT);
    }
    else
    {
        sqlite3_bind_int(stmt, 1, partido_id);
    }
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }

    cJSON *payload = cJSON_CreateObject();
    if (!payload)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }

    int cancha_id = 0;
    int camiseta_id = 0;
    int botin_id = 0;
    const int columnas = sqlite3_column_count(stmt);
    for (int i = 0; i < columnas; i++)
    {
        const char *nombre = sqlite3_column_name(stmt, i);
        if (!nombre)
        {
            continue;
        }
        if (strcmp(nombre, "id") == 0 || strcmp(nombre, "cancha_id") == 0 ||
                strcmp(nombre, "camiseta_id") == 0 ||
                strcmp(nombre, "botin_id") == 0 ||
                strcmp(nombre, "equipo_id") == 0)
        {
            if (strcmp(nombre, "cancha_id") == 0)
            {
                cancha_id = sqlite3_column_int(stmt, i);
            }
            else if (strcmp(nombre, "camiseta_id") == 0)
            {
                camiseta_id = sqlite3_column_int(stmt, i);
            }
            else if (strcmp(nombre, "botin_id") == 0)
            {
                botin_id = sqlite3_column_int(stmt, i);
            }
            continue;
        }

        int tipo = sqlite3_column_type(stmt, i);
        if (tipo == SQLITE_NULL)
        {
            continue;
        }
        const char *texto = (const char *)sqlite3_column_text(stmt, i);
        if (strcmp(nombre, "resultado") == 0)
        {
            const char *valor = resultado_texto(sqlite3_column_int(stmt, i));
            if (valor)
            {
                cJSON_AddStringToObject(payload, nombre, valor);
            }
            continue;
        }
        if (strcmp(nombre, "clima") == 0)
        {
            const char *valor = clima_texto(sqlite3_column_int(stmt, i));
            if (valor)
            {
                cJSON_AddStringToObject(payload, nombre, valor);
            }
            continue;
        }
        if (strcmp(nombre, "tipo_partido") == 0)
        {
            const char *valor_tipo =
                tipo_texto(sqlite3_column_int(stmt, i));
            cJSON_AddStringToObject(payload, nombre,
                                    valor_tipo ? valor_tipo : "amistoso");
            continue;
        }
        if (strcmp(nombre, "tarjeta") == 0)
        {
            cJSON_AddStringToObject(payload, nombre,
                                    tarjeta_texto(sqlite3_column_int(stmt, i)));
            continue;
        }
        if (strcmp(nombre, "estado_cancha") == 0)
        {
            cJSON_AddStringToObject(
                payload, nombre, estado_cancha_texto(sqlite3_column_int(stmt, i)));
            continue;
        }
        if (strcmp(nombre, "formato_partido") == 0)
        {
            cJSON_AddNumberToObject(payload, "formato_partido",
                                    formato_numero(texto));
            cJSON_AddStringToObject(payload, "formato_partido_texto",
                                    texto ? texto : "");
            continue;
        }
        if (strcmp(nombre, "fecha_hora") == 0)
        {
            char iso[64];
            sync_copiar(iso, sizeof(iso), texto ? texto : "");
            for (char *p = iso; *p; p++)
            {
                if (*p == ' ')
                {
                    *p = 'T';
                }
            }
            cJSON_AddStringToObject(payload, nombre, iso);
            continue;
        }
        if (strcmp(nombre, "uuid") == 0)
        {
            sync_copiar(uuid_salida, uuid_tam, texto);
            continue;
        }
        if (tipo == SQLITE_INTEGER)
        {
            int valor = sqlite3_column_int(stmt, i);
            if ((strcmp(nombre, "goles_equipo") == 0 ||
                    strcmp(nombre, "goles_rival") == 0) &&
                    valor < 0)
            {
                /* El escritorio usa -1 como "sin marcador". */
                continue;
            }
            cJSON_AddNumberToObject(payload, nombre, (double)valor);
        }
        else if (tipo == SQLITE_FLOAT)
        {
            cJSON_AddNumberToObject(payload, nombre,
                                    sqlite3_column_double(stmt, i));
        }
        else
        {
            cJSON_AddStringToObject(payload, nombre, texto ? texto : "");
        }
    }
    sqlite3_finalize(stmt);

    if (uuid_salida && uuid_tam && !uuid_salida[0])
    {
        /* Base antigua sin UUID: se genera y se persiste. */
        sync_uuid_generar(uuid_salida, uuid_tam);
        sqlite3_stmt *upd = NULL;
        if (sqlite3_prepare_v2(db, "UPDATE partido SET uuid=? WHERE id=?;", -1,
                               &upd, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(upd, 1, uuid_salida, -1, DB_TRANSIENT);
            sqlite3_bind_int(upd, 2, partido_id);
            sqlite3_step(upd);
            sqlite3_finalize(upd);
        }
    }
    if (uuid_salida && uuid_tam && uuid_salida[0])
    {
        cJSON_AddStringToObject(payload, "uuid", uuid_salida);
    }

    char cancha[256];
    char camiseta[256];
    char botin[256];
    sync_nombre_por_id("cancha", cancha_id, cancha, sizeof(cancha));
    sync_nombre_por_id("camiseta", camiseta_id, camiseta, sizeof(camiseta));
    sync_nombre_por_id("botin", botin_id, botin, sizeof(botin));
    cJSON_AddStringToObject(payload, "cancha_nombre", cancha);
    cJSON_AddStringToObject(payload, "camiseta_nombre", camiseta);
    cJSON_AddStringToObject(payload, "botin_nombre", botin);

    cJSON *actualizado =
        cJSON_GetObjectItemCaseSensitive(payload, "actualizado_en");
    if (actualizado && cJSON_IsString(actualizado) &&
            actualizado->valuestring)
    {
        for (char *p = actualizado->valuestring; *p; p++)
        {
            if (*p == ' ')
            {
                *p = 'T';
            }
        }
    }
    if (!actualizado || !cJSON_IsString(actualizado) ||
            !actualizado->valuestring[0])
    {
        const cJSON *fecha = cJSON_GetObjectItemCaseSensitive(payload, "fecha_hora");
        cJSON_AddStringToObject(
            payload, "actualizado_en",
            (fecha && cJSON_IsString(fecha)) ? fecha->valuestring
            : "1970-01-01T00:00:00");
    }
    if (error && error_tam)
    {
        error[0] = '\0';
    }
    return payload;
}

static cJSON *sync_registro_remoto(const cJSON *payload, const char *uuid,
                                   const char *owner, int deleted)
{
    if (!payload || !uuid || !owner)
    {
        return NULL;
    }
    cJSON *registro = cJSON_CreateObject();
    if (!registro)
    {
        return NULL;
    }
    cJSON_AddStringToObject(registro, "uuid", uuid);
    cJSON_AddStringToObject(registro, "owner", owner);
    const cJSON *fecha = cJSON_GetObjectItemCaseSensitive(payload, "fecha_hora");
    const cJSON *rival = cJSON_GetObjectItemCaseSensitive(payload, "rival_nombre");
    const cJSON *camiseta =
        cJSON_GetObjectItemCaseSensitive(payload, "camiseta_nombre");
    const cJSON *actualizado =
        cJSON_GetObjectItemCaseSensitive(payload, "actualizado_en");
    cJSON_AddStringToObject(registro, "fecha",
                            (fecha && cJSON_IsString(fecha))
                            ? fecha->valuestring
                            : "1970-01-01T00:00:00");
    if (rival && cJSON_IsString(rival))
    {
        cJSON_AddStringToObject(registro, "rival", rival->valuestring);
    }
    cJSON_AddNumberToObject(
        registro, "goles",
        sync_entero_desde_json(payload, "goles", 0));
    cJSON_AddNumberToObject(
        registro, "asistencias",
        sync_entero_desde_json(payload, "asistencias", 0));
    if (camiseta && cJSON_IsString(camiseta))
    {
        cJSON_AddStringToObject(registro, "camiseta_uuid",
                                camiseta->valuestring);
    }
    cJSON_AddStringToObject(
        registro, "updated_at_client",
        (actualizado && cJSON_IsString(actualizado))
        ? actualizado->valuestring
        : "1970-01-01T00:00:00");
    cJSON_AddBoolToObject(registro, "deleted", deleted ? 1 : 0);
    cJSON_AddItemToObject(registro, "payload", cJSON_Duplicate(payload, 1));
    return registro;
}

/* ----------------------------------------------------------- Aplicar remoto */

static int sync_eliminar_local(const char *uuid)
{
    if (!db || !uuid)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM partido WHERE uuid=?;", -1, &stmt,
                           NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, DB_TRANSIENT);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (ok)
    {
        sync_queue_mark_done(uuid);
    }
    return ok ? 1 : 0;
}

static void sync_fecha_iso_a_espacio(char *texto)
{
    if (!texto)
    {
        return;
    }
    for (char *p = texto; *p; p++)
    {
        if (*p == 'T')
        {
            *p = ' ';
            return;
        }
    }
}

/** Compara marcas ISO aceptando 'T' o espacio como separador. */
static int sync_comparar_fechas(const char *a, const char *b)
{
    if (!a && !b)
    {
        return 0;
    }
    if (!a)
    {
        return -1;
    }
    if (!b)
    {
        return 1;
    }
    char ca[64];
    char cb[64];
    sync_copiar(ca, sizeof(ca), a);
    sync_copiar(cb, sizeof(cb), b);
    sync_fecha_iso_a_espacio(ca);
    sync_fecha_iso_a_espacio(cb);
    return strcmp(ca, cb);
}

static int sync_aplicar_remoto(cJSON *registro, int forzar)
{
    if (!db || !registro)
    {
        return 0;
    }
    const char *uuid = sync_json_texto(registro, "uuid");
    if (!uuid || !*uuid)
    {
        return 0;
    }
    const cJSON *borrado = cJSON_GetObjectItemCaseSensitive(registro, "deleted");
    if (cJSON_IsTrue(borrado))
    {
        return sync_eliminar_local(uuid);
    }

    cJSON *payload = cJSON_GetObjectItemCaseSensitive(registro, "payload");
    if (!cJSON_IsObject(payload))
    {
        payload = cJSON_CreateObject();
        if (!payload)
        {
            return 0;
        }
        const cJSON *fecha = cJSON_GetObjectItemCaseSensitive(registro, "fecha");
        if (cJSON_IsString(fecha))
        {
            cJSON_AddStringToObject(payload, "fecha_hora", fecha->valuestring);
        }
        cJSON_AddNumberToObject(payload, "goles",
                                sync_entero_desde_json(registro, "goles", 0));
        cJSON_AddNumberToObject(
            payload, "asistencias",
            sync_entero_desde_json(registro, "asistencias", 0));
    }
    else
    {
        payload = cJSON_Duplicate(payload, 1);
    }
    if (!payload)
    {
        return 0;
    }

    const char *remoto_updated = sync_json_texto(registro, "updated_at_client");
    const int id_local = sync_id_por_uuid(uuid);
    if (id_local > 0 && !forzar)
    {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db,
                               "SELECT COALESCE(actualizado_en,'') FROM partido"
                               " WHERE id=? LIMIT 1;",
                               -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, id_local);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char *local_updated =
                    (const char *)sqlite3_column_text(stmt, 0);
                if (local_updated && remoto_updated &&
                        sync_comparar_fechas(remoto_updated, local_updated) <= 0)
                {
                    sqlite3_finalize(stmt);
                    cJSON_Delete(payload);
                    return 0;
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    const char *cancha_nombre =
        sync_json_texto(payload, "cancha_nombre");
    const char *camiseta_nombre =
        sync_json_texto(payload, "camiseta_nombre");
    const char *botin_nombre = sync_json_texto(payload, "botin_nombre");
    int cancha_id = sync_id_por_nombre("cancha", cancha_nombre, "Sin cancha");
    int camiseta_id =
        sync_id_por_nombre("camiseta", camiseta_nombre, "Sin camiseta");
    int botin_id = sync_id_por_nombre("botin", botin_nombre, NULL);

    char fecha[64];
    sync_json_copiar_texto(payload, "fecha_hora", fecha, sizeof(fecha));
    if (!fecha[0])
    {
        sync_json_copiar_texto(registro, "fecha", fecha, sizeof(fecha));
    }
    if (!fecha[0])
    {
        cJSON_Delete(payload);
        return 0;
    }
    sync_fecha_iso_a_espacio(fecha);

    char *nombres[256];
    const cJSON *valores[256];
    int total = 0;

    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, payload)
    {
        if (!item->string || total >= 250)
        {
            continue;
        }
        const char *nombre = item->string;
        if (strcmp(nombre, "id") == 0 || strcmp(nombre, "cancha_id") == 0 ||
                strcmp(nombre, "camiseta_id") == 0 ||
                strcmp(nombre, "botin_id") == 0 ||
                strcmp(nombre, "equipo_id") == 0 ||
                strcmp(nombre, "cancha_nombre") == 0 ||
                strcmp(nombre, "camiseta_nombre") == 0 ||
                strcmp(nombre, "botin_nombre") == 0 ||
                strcmp(nombre, "formato_partido_texto") == 0 ||
                strcmp(nombre, "actualizado_en") == 0 ||
                strcmp(nombre, "fecha_hora") == 0 || strcmp(nombre, "uuid") == 0)
        {
            continue;
        }
        if (!sync_columna_existe(db, "partido", nombre))
        {
            continue;
        }
        nombres[total] = item->string;
        valores[total] = item;
        total++;
    }
    nombres[total] = (char *)"fecha_hora";
    valores[total] = NULL; /* se bindea aparte */
    total++;
    nombres[total] = (char *)"uuid";
    valores[total] = NULL;
    total++;
    nombres[total] = (char *)"cancha_id";
    valores[total] = NULL;
    total++;
    nombres[total] = (char *)"camiseta_id";
    valores[total] = NULL;
    total++;
    if (sync_columna_existe(db, "partido", "botin_id") && botin_id > 0)
    {
        nombres[total] = (char *)"botin_id";
        valores[total] = NULL;
        total++;
    }
    if (sync_columna_existe(db, "partido", "actualizado_en") && remoto_updated)
    {
        nombres[total] = (char *)"actualizado_en";
        valores[total] = NULL;
        total++;
    }
    char sql[4096];
    int usados = 0;
    if (id_local > 0)
    {
        usados = snprintf(sql, sizeof(sql), "UPDATE partido SET ");
        for (int i = 0; i < total; i++)
        {
            usados += snprintf(sql + usados, sizeof(sql) - (size_t)usados,
                               "%s\"%s\"=?", i == 0 ? "" : ", ", nombres[i]);
        }
        usados += snprintf(sql + usados, sizeof(sql) - (size_t)usados,
                           " WHERE id=?;");
    }
    else
    {
        usados = snprintf(sql, sizeof(sql), "INSERT INTO partido (");
        for (int i = 0; i < total; i++)
        {
            usados += snprintf(sql + usados, sizeof(sql) - (size_t)usados,
                               "%s\"%s\"", i == 0 ? "" : ", ", nombres[i]);
        }
        usados += snprintf(sql + usados, sizeof(sql) - (size_t)usados,
                           ") VALUES (");
        for (int i = 0; i < total; i++)
        {
            usados += snprintf(sql + usados, sizeof(sql) - (size_t)usados,
                               "%s?", i == 0 ? "" : ", ");
        }
        usados += snprintf(sql + usados, sizeof(sql) - (size_t)usados, ");");
    }
    if (usados < 0 || (size_t)usados >= sizeof(sql))
    {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        cJSON_Delete(payload);
        return 0;
    }

    sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        cJSON_Delete(payload);
        return 0;
    }

    int indice = 1;
    for (int i = 0; i < total; i++)
    {
        const char *nombre = nombres[i];
        if (strcmp(nombre, "fecha_hora") == 0)
        {
            sqlite3_bind_text(stmt, indice++, fecha, -1, SQLITE_TRANSIENT);
            continue;
        }
        if (strcmp(nombre, "uuid") == 0)
        {
            sqlite3_bind_text(stmt, indice++, uuid, -1, SQLITE_TRANSIENT);
            continue;
        }
        if (strcmp(nombre, "cancha_id") == 0)
        {
            sqlite3_bind_int(stmt, indice++, cancha_id);
            continue;
        }
        if (strcmp(nombre, "camiseta_id") == 0)
        {
            sqlite3_bind_int(stmt, indice++, camiseta_id);
            continue;
        }
        if (strcmp(nombre, "botin_id") == 0)
        {
            if (botin_id > 0)
            {
                sqlite3_bind_int(stmt, indice++, botin_id);
            }
            else
            {
                sqlite3_bind_null(stmt, indice++);
            }
            continue;
        }
        if (strcmp(nombre, "actualizado_en") == 0)
        {
            sqlite3_bind_text(stmt, indice++, remoto_updated, -1,
                              SQLITE_TRANSIENT);
            continue;
        }
        if (strcmp(nombre, "formato_partido") == 0)
        {
            int num = sync_entero_desde_json(payload, "formato_partido", 0);
            const char *texto =
                sync_json_texto(payload, "formato_partido_texto");
            if (!texto || !*texto)
            {
                const cJSON *formato =
                    cJSON_GetObjectItemCaseSensitive(payload,
                                                     "formato_partido");
                if (cJSON_IsString(formato) && formato->valuestring)
                {
                    texto = formato->valuestring;
                }
                else
                {
                    texto = formato_texto(num);
                }
            }
            sqlite3_bind_text(stmt, indice++, texto, -1, SQLITE_TRANSIENT);
            continue;
        }
        if (strcmp(nombre, "resultado") == 0)
        {
            sqlite3_bind_int(
                stmt, indice++,
                resultado_codigo(sync_json_texto(payload, "resultado")));
            continue;
        }
        if (strcmp(nombre, "clima") == 0)
        {
            sqlite3_bind_int(stmt, indice++,
                             clima_codigo(sync_json_texto(payload, "clima")));
            continue;
        }
        if (strcmp(nombre, "tipo_partido") == 0)
        {
            sqlite3_bind_int(
                stmt, indice++,
                tipo_codigo(sync_json_texto(payload, "tipo_partido")));
            continue;
        }
        if (strcmp(nombre, "tarjeta") == 0)
        {
            sqlite3_bind_int(
                stmt, indice++,
                tarjeta_codigo(sync_json_texto(payload, "tarjeta")));
            continue;
        }
        if (strcmp(nombre, "estado_cancha") == 0)
        {
            sqlite3_bind_int(
                stmt, indice++,
                estado_cancha_codigo(sync_json_texto(payload, "estado_cancha")));
            continue;
        }

        const cJSON *valor = valores[i];
        if (!valor || cJSON_IsNull(valor))
        {
            sqlite3_bind_null(stmt, indice++);
        }
        else if (cJSON_IsString(valor))
        {
            sqlite3_bind_text(stmt, indice++, valor->valuestring, -1,
                              SQLITE_TRANSIENT);
        }
        else if (cJSON_IsNumber(valor))
        {
            if (strcmp(nombre, "goles") == 0 ||
                    strcmp(nombre, "asistencias") == 0 ||
                    strcmp(nombre, "dia") == 0 ||
                    strcmp(nombre, "precio") == 0)
            {
                sqlite3_bind_int(stmt, indice++, (int)valor->valuedouble);
            }
            else
            {
                sqlite3_bind_double(stmt, indice++, valor->valuedouble);
            }
        }
        else if (cJSON_IsBool(valor))
        {
            sqlite3_bind_int(stmt, indice++, cJSON_IsTrue(valor) ? 1 : 0);
        }
        else if (cJSON_IsArray(valor))
        {
            char tags[1024] = {0};
            const cJSON *tag = NULL;
            cJSON_ArrayForEach(tag, valor)
            {
                if (cJSON_IsString(tag) && tag->valuestring)
                {
                    if (tags[0])
                    {
                        strncat(tags, ",", sizeof(tags) - strlen(tags) - 1);
                    }
                    strncat(tags, tag->valuestring,
                            sizeof(tags) - strlen(tags) - 1);
                }
            }
            sqlite3_bind_text(stmt, indice++, tags, -1, SQLITE_TRANSIENT);
        }
        else
        {
            char *texto = sync_json_imprimir(valor);
            sqlite3_bind_text(stmt, indice++, texto ? texto : "", -1,
                              SQLITE_TRANSIENT);
            cJSON_free(texto);
        }
    }
    if (id_local > 0)
    {
        sqlite3_bind_int(stmt, indice++, id_local);
    }

    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (ok)
    {
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
        /* El cambio vino del servidor: no debe reenviarse. */
        sync_queue_mark_done(uuid);
    }
    else
    {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    }
    cJSON_Delete(payload);
    return ok ? 1 : 0;
}

/* ------------------------------------------------------------------ Conflictos */

static int sync_guardar_conflicto(const char *uuid, const cJSON *local,
                                  const cJSON *remoto)
{
    if (!db || !uuid || !local || !remoto)
    {
        return 0;
    }
    char *local_json = sync_json_imprimir(local);
    char *remoto_json = sync_json_imprimir(remoto);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(
                db,
                "INSERT INTO sync_conflicto (uuid, local_json, remoto_json,"
                " descripcion, creado_en) VALUES (?, ?, ?, ?,"
                " datetime('now','localtime'))"
                " ON CONFLICT(uuid) DO UPDATE SET local_json=excluded.local_json,"
                " remoto_json=excluded.remoto_json,"
                " descripcion=excluded.descripcion,"
                " creado_en=excluded.creado_en;",
                -1, &stmt, NULL) != SQLITE_OK)
    {
        cJSON_free(local_json);
        cJSON_free(remoto_json);
        return 0;
    }
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, local_json ? local_json : "{}", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, remoto_json ? remoto_json : "{}", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, "El partido cambio en otro dispositivo.",
                      -1, SQLITE_TRANSIENT);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    cJSON_free(local_json);
    cJSON_free(remoto_json);
    return ok;
}

int sync_conflict_resolve(int id, int conservar_local)
{
    if (!db)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(
                db, "SELECT uuid, COALESCE(remoto_json,'') FROM sync_conflicto"
                " WHERE id=? LIMIT 1;",
                -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, id);
    char uuid[SYNC_UUID_SIZE] = {0};
    char *remoto_json = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        sync_copiar(uuid, sizeof(uuid),
                    (const char *)sqlite3_column_text(stmt, 0));
        const char *remoto = (const char *)sqlite3_column_text(stmt, 1);
        remoto_json = remoto ? strdup(remoto) : NULL;
    }
    sqlite3_finalize(stmt);
    if (!uuid[0])
    {
        free(remoto_json);
        return 0;
    }

    if (conservar_local)
    {
        char ahora[64];
        sync_ahora(ahora, sizeof(ahora));
        sqlite3_stmt *upd = NULL;
        if (sync_columna_existe(db, "partido", "actualizado_en"))
        {
            if (sqlite3_prepare_v2(
                        db,
                        "UPDATE partido SET actualizado_en=? WHERE uuid=?;",
                        -1, &upd, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(upd, 1, ahora, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(upd, 2, uuid, -1, SQLITE_TRANSIENT);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
        }
        sqlite3_stmt *ins = NULL;
        if (sqlite3_prepare_v2(
                    db,
                    "INSERT INTO sync_dirty (partido_id, uuid, operacion, estado,"
                    " creado_en)"
                    " VALUES ((SELECT id FROM partido WHERE uuid=?), ?, 'upsert',"
                    " 'pendiente', datetime('now','localtime'));",
                    -1, &ins, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(ins, 1, uuid, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 2, uuid, -1, SQLITE_TRANSIENT);
            sqlite3_step(ins);
            sqlite3_finalize(ins);
        }
    }
    else if (remoto_json)
    {
        cJSON *record = cJSON_Parse(remoto_json);
        if (record)
        {
            sync_aplicar_remoto(record, 1);
            cJSON_Delete(record);
        }
        sync_queue_mark_done(uuid);
    }
    free(remoto_json);

    sqlite3_stmt *del = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM sync_conflicto WHERE id=?;", -1,
                           &del, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int(del, 1, id);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }
    return 1;
}

/* ----------------------------------------------------------------------- HTTP */

static void sync_mensaje_http(const SyncHttp *respuesta, char *error,
                              size_t error_tam)
{
    if (!error || error_tam == 0)
    {
        return;
    }
    error[0] = '\0';
    if (!respuesta)
    {
        return;
    }
    if (respuesta->error)
    {
        sync_copiar(error, error_tam, respuesta->error);
        return;
    }
    if (respuesta->body && *respuesta->body)
    {
        cJSON *json = cJSON_Parse(respuesta->body);
        const char *mensaje = json ? sync_json_texto(json, "message") : NULL;
        sync_copiar(error, error_tam,
                    mensaje ? mensaje : respuesta->body);
        cJSON_Delete(json);
    }
}

static int sync_http_status(const SyncHttp *respuesta)
{
    return respuesta ? respuesta->status : 0;
}

static int sync_refrescar_token(SyncConfig *cfg)
{
    if (!cfg || !cfg->servidor_activo[0] || !cfg->token[0])
    {
        return 0;
    }
    char url[SYNC_URL_SIZE + 128];
    snprintf(url, sizeof(url), "%s/api/collections/users/auth-refresh",
             cfg->servidor_activo);
    SyncHttp respuesta;
    if (!sync_http_request("POST", url, cfg->token, "", &respuesta))
    {
        sync_http_free(&respuesta);
        return 0;
    }
    int ok = 0;
    if (respuesta.status >= 200 && respuesta.status < 300 && respuesta.body)
    {
        cJSON *json = cJSON_Parse(respuesta.body);
        const char *token = json ? sync_json_texto(json, "token") : NULL;
        if (token)
        {
            sync_copiar(cfg->token, sizeof(cfg->token), token);
            sync_config_set("token", cfg->token);
            ok = 1;
        }
        cJSON_Delete(json);
    }
    sync_http_free(&respuesta);
    return ok;
}

static void sync_url_escapar_cursor(const char *cursor, char *salida, size_t tam)
{
    if (!salida || tam == 0)
    {
        return;
    }
    salida[0] = '\0';
    if (!cursor)
    {
        return;
    }
    size_t j = 0;
    for (size_t i = 0; cursor[i] && j + 4 < tam; i++)
    {
        unsigned char c = (unsigned char)cursor[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ':' ||
                c == 'T' || c == 'Z' || c == '+')
        {
            salida[j++] = (char)c;
        }
        else
        {
            snprintf(salida + j, tam - j, "%%%02X", c);
            j += 3;
        }
    }
    salida[j] = '\0';
}

static int sync_buscar_remoto(const char *base, const char *token,
                              const char *uuid, cJSON **registro,
                              char *error, size_t error_tam)
{
    if (registro)
    {
        *registro = NULL;
    }
    char url[SYNC_URL_SIZE + 512];
    snprintf(url, sizeof(url),
             "%s/api/collections/%s/records?filter=uuid='%s'&perPage=1", base,
             SYNC_COLECCION, uuid);
    SyncHttp respuesta;
    if (!sync_http_request("GET", url, token, NULL, &respuesta))
    {
        sync_mensaje_http(&respuesta, error, error_tam);
        sync_http_free(&respuesta);
        return -1;
    }
    int status = sync_http_status(&respuesta);
    if (status == 401 || status == 403)
    {
        sync_http_free(&respuesta);
        return -2;
    }
    if (status < 200 || status >= 300 || !respuesta.body)
    {
        sync_mensaje_http(&respuesta, error, error_tam);
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
    if (cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0)
    {
        cJSON *primero = cJSON_GetArrayItem(items, 0);
        if (registro && primero)
        {
            *registro = cJSON_Duplicate(primero, 1);
        }
    }
    cJSON_Delete(json);
    return 1;
}

static int sync_enviar_registro(const char *base, const char *token,
                                const char *record_id, const cJSON *registro,
                                char *error, size_t error_tam)
{
    char *json = sync_json_imprimir(registro);
    if (!json)
    {
        return -1;
    }
    char url[SYNC_URL_SIZE + 512];
    if (record_id && *record_id)
    {
        snprintf(url, sizeof(url), "%s/api/collections/%s/records/%s", base,
                 SYNC_COLECCION, record_id);
    }
    else
    {
        snprintf(url, sizeof(url), "%s/api/collections/%s/records", base,
                 SYNC_COLECCION);
    }
    SyncHttp respuesta;
    int ok = sync_http_request(record_id && *record_id ? "PATCH" : "POST", url,
                               token, json, &respuesta);
    free(json);
    if (!ok)
    {
        sync_mensaje_http(&respuesta, error, error_tam);
        sync_http_free(&respuesta);
        return -1;
    }
    int status = sync_http_status(&respuesta);
    if (status == 401 || status == 403)
    {
        sync_http_free(&respuesta);
        return -2;
    }
    if (status < 200 || status >= 300)
    {
        sync_mensaje_http(&respuesta, error, error_tam);
        sync_http_free(&respuesta);
        return -1;
    }
    sync_http_free(&respuesta);
    return 1;
}

/** Envia un borrado logico (tombstone) para que otros equipos lo reciban. */
static int sync_enviar_tombstone(const char *base, const char *token,
                                 const char *owner, const char *uuid,
                                 const char *record_id,
                                 const char *actualizado,
                                 const cJSON *remoto, char *error,
                                 size_t error_tam)
{
    cJSON *tombstone = cJSON_CreateObject();
    if (!tombstone)
    {
        return -1;
    }
    cJSON_AddStringToObject(tombstone, "uuid", uuid);
    cJSON_AddStringToObject(tombstone, "owner", owner);
    cJSON_AddBoolToObject(tombstone, "deleted", 1);
    cJSON_AddStringToObject(tombstone, "updated_at_client",
                            actualizado ? actualizado : "");
    const cJSON *fecha = remoto ? cJSON_GetObjectItemCaseSensitive(remoto, "fecha")
                         : NULL;
    if (cJSON_IsString(fecha) && fecha->valuestring)
    {
        cJSON_AddStringToObject(tombstone, "fecha", fecha->valuestring);
    }
    else
    {
        char ahora[64];
        sync_ahora(ahora, sizeof(ahora));
        cJSON_AddStringToObject(tombstone, "fecha", ahora);
    }
    cJSON_AddItemToObject(tombstone, "payload", cJSON_CreateObject());
    int envio = sync_enviar_registro(base, token, record_id, tombstone, error,
                                     error_tam);
    cJSON_Delete(tombstone);
    return envio;
}

/* ------------------------------------------------------------------ Subida */

static int sync_subir(const char *base, SyncConfig *cfg, SyncResultado *res)
{
    for (;;)
    {
        SyncDirty pendiente;
        if (!sync_queue_next(&pendiente))
        {
            break;
        }
        if (!pendiente.uuid[0])
        {
            sync_queue_mark_done(pendiente.uuid);
            continue;
        }

        char error[SYNC_ERROR_SIZE] = {0};
        cJSON *remoto = NULL;
        int busqueda = sync_buscar_remoto(base, cfg->token, pendiente.uuid,
                                          &remoto, error, sizeof(error));
        if (busqueda == -2)
        {
            cJSON_Delete(remoto);
            if (sync_refrescar_token(cfg))
            {
                continue;
            }
            sync_queue_mark_error(pendiente.id, "Sesion vencida");
            res->errores++;
            return -2;
        }
        if (busqueda < 0)
        {
            cJSON_Delete(remoto);
            sync_queue_mark_error(pendiente.id,
                                  error[0] ? error : "Error de red");
            res->errores++;
            return -1;
        }

        const char *record_id =
            remoto ? sync_json_texto(remoto, "id") : NULL;
        if (strcmp(pendiente.operacion, "eliminar") == 0)
        {
            char marca[64] = {0};
            sync_copiar(marca, sizeof(marca), pendiente.creado_en);
            for (char *p = marca; *p; p++)
            {
                if (*p == ' ')
                {
                    *p = 'T';
                }
            }
            if (!marca[0])
            {
                sync_ahora(marca, sizeof(marca));
            }
            const char *remoto_updated =
                remoto ? sync_json_texto(remoto, "updated_at_client") : NULL;
            if (record_id && remoto_updated &&
                    sync_comparar_fechas(remoto_updated, marca) > 0)
            {
                cJSON *local = cJSON_CreateObject();
                cJSON_AddStringToObject(local, "uuid", pendiente.uuid);
                cJSON_AddStringToObject(local, "actualizado_en", marca);
                sync_guardar_conflicto(pendiente.uuid, local, remoto);
                cJSON_Delete(local);
                sync_queue_mark_conflict(pendiente.uuid);
                res->conflictos++;
                cJSON_Delete(remoto);
                continue;
            }
            if (record_id)
            {
                int envio = sync_enviar_tombstone(
                                base, cfg->token, cfg->user_id, pendiente.uuid, record_id,
                                marca, remoto, error, sizeof(error));
                if (envio == -2)
                {
                    cJSON_Delete(remoto);
                    if (sync_refrescar_token(cfg))
                    {
                        continue;
                    }
                    sync_queue_mark_error(pendiente.id, "Sesion vencida");
                    res->errores++;
                    return -2;
                }
                if (envio < 0)
                {
                    sync_queue_mark_error(pendiente.id, error);
                    res->errores++;
                    cJSON_Delete(remoto);
                    return -1;
                }
            }
            sync_queue_mark_done(pendiente.uuid);
            res->enviados++;
            cJSON_Delete(remoto);
            continue;
        }

        char uuid[SYNC_UUID_SIZE] = {0};
        cJSON *payload = sync_payload_local(pendiente.uuid, 0, uuid,
                                            sizeof(uuid), error, sizeof(error));
        if (!payload)
        {
            /* El partido ya no existe: propagar borrado logico. */
            char marca[64] = {0};
            sync_copiar(marca, sizeof(marca), pendiente.creado_en);
            for (char *p = marca; *p; p++)
            {
                if (*p == ' ')
                {
                    *p = 'T';
                }
            }
            if (!marca[0])
            {
                sync_ahora(marca, sizeof(marca));
            }
            const char *remoto_updated =
                remoto ? sync_json_texto(remoto, "updated_at_client") : NULL;
            if (record_id && remoto_updated &&
                    sync_comparar_fechas(remoto_updated, marca) > 0)
            {
                cJSON *local = cJSON_CreateObject();
                cJSON_AddStringToObject(local, "uuid", pendiente.uuid);
                cJSON_AddStringToObject(local, "actualizado_en", marca);
                sync_guardar_conflicto(pendiente.uuid, local, remoto);
                cJSON_Delete(local);
                sync_queue_mark_conflict(pendiente.uuid);
                res->conflictos++;
                cJSON_Delete(remoto);
                continue;
            }
            if (record_id)
            {
                int envio = sync_enviar_tombstone(
                                base, cfg->token, cfg->user_id, pendiente.uuid, record_id,
                                marca, remoto, error, sizeof(error));
                if (envio < 0)
                {
                    sync_queue_mark_error(pendiente.id, error);
                    res->errores++;
                    cJSON_Delete(remoto);
                    return envio == -2 ? -2 : -1;
                }
            }
            sync_queue_mark_done(pendiente.uuid);
            cJSON_Delete(remoto);
            continue;
        }
        sync_copiar(uuid, sizeof(uuid), pendiente.uuid);
        cJSON_DeleteItemFromObjectCaseSensitive(payload, "uuid");
        cJSON_AddStringToObject(payload, "uuid", uuid);
        if (pendiente.creado_en[0])
        {
            char marca[64];
            sync_copiar(marca, sizeof(marca), pendiente.creado_en);
            for (char *p = marca; *p; p++)
            {
                if (*p == ' ')
                {
                    *p = 'T';
                }
            }
            cJSON_DeleteItemFromObjectCaseSensitive(payload, "actualizado_en");
            cJSON_AddStringToObject(payload, "actualizado_en", marca);
        }

        const cJSON *actualizado =
            cJSON_GetObjectItemCaseSensitive(payload, "actualizado_en");
        const char *local_updated =
            (actualizado && cJSON_IsString(actualizado))
            ? actualizado->valuestring
            : "";
        const char *remoto_updated =
            remoto ? sync_json_texto(remoto, "updated_at_client") : NULL;
        if (remoto_updated && local_updated[0] &&
                sync_comparar_fechas(remoto_updated, local_updated) > 0)
        {
            sync_guardar_conflicto(uuid, payload, remoto);
            sync_queue_mark_conflict(uuid);
            res->conflictos++;
            cJSON_Delete(payload);
            cJSON_Delete(remoto);
            continue;
        }

        cJSON *registro = sync_registro_remoto(payload, uuid, cfg->user_id, 0);
        int envio = sync_enviar_registro(base, cfg->token, record_id, registro,
                                         error, sizeof(error));
        cJSON_Delete(registro);
        cJSON_Delete(payload);
        cJSON_Delete(remoto);
        if (envio == -2)
        {
            if (sync_refrescar_token(cfg))
            {
                continue;
            }
            sync_queue_mark_error(pendiente.id, "Sesion vencida");
            res->errores++;
            return -2;
        }
        if (envio < 0)
        {
            sync_queue_mark_error(pendiente.id, error);
            res->errores++;
            return -1;
        }
        sync_queue_mark_done(uuid);
        res->enviados++;
    }
    return 1;
}

/* ------------------------------------------------------------------ Bajada */

static int sync_bajar(const char *base, SyncConfig *cfg, SyncResultado *res)
{
    int pagina = 1;
    int total_paginas = 1;
    char cursor[128];
    sync_copiar(cursor, sizeof(cursor), cfg->cursor);

    do
    {
        char url[SYNC_URL_SIZE + 1024];
        if (cursor[0])
        {
            char escapado[256];
            sync_url_escapar_cursor(cursor, escapado, sizeof(escapado));
            snprintf(url, sizeof(url),
                     "%s/api/collections/%s/records?page=%d&perPage=100&"
                     "sort=updated_at_client&"
                     "filter=updated_at_client>%%22%s%%22",
                     base, SYNC_COLECCION, pagina, escapado);
        }
        else
        {
            snprintf(url, sizeof(url),
                     "%s/api/collections/%s/records?page=%d&perPage=100&"
                     "sort=updated_at_client",
                     base, SYNC_COLECCION, pagina);
        }
        SyncHttp respuesta;
        if (!sync_http_request("GET", url, cfg->token, NULL, &respuesta))
        {
            sync_http_free(&respuesta);
            return -1;
        }
        int status = sync_http_status(&respuesta);
        if (status == 401 || status == 403)
        {
            sync_http_free(&respuesta);
            return -2;
        }
        if (status < 200 || status >= 300 || !respuesta.body)
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
        total_paginas = sync_entero_desde_json(json, "totalPages", 1);
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(json, "items");
        if (!cJSON_IsArray(items) || cJSON_GetArraySize(items) == 0)
        {
            cJSON_Delete(json);
            break;
        }

        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, items)
        {
            const char *uuid = sync_json_texto(item, "uuid");
            if (!uuid)
            {
                continue;
            }
            const char *updated = sync_json_texto(item, "updated_at_client");
            if (updated && sync_comparar_fechas(updated, cursor) > 0)
            {
                sync_copiar(cursor, sizeof(cursor), updated);
            }

            SyncDirty pendiente;
            memset(&pendiente, 0, sizeof(pendiente));
            sqlite3_stmt *stmt = NULL;
            int hay_pendiente = 0;
            if (sqlite3_prepare_v2(
                        db,
                        "SELECT id, partido_id, uuid, operacion, estado,"
                        " COALESCE(creado_en,'')"
                        " FROM sync_dirty WHERE uuid=? ORDER BY id DESC LIMIT 1;",
                        -1, &stmt, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    hay_pendiente = 1;
                    pendiente.id = sqlite3_column_int(stmt, 0);
                    sync_copiar(pendiente.uuid, sizeof(pendiente.uuid), uuid);
                    sync_copiar(pendiente.creado_en,
                                sizeof(pendiente.creado_en),
                                (const char *)sqlite3_column_text(stmt, 5));
                }
                sqlite3_finalize(stmt);
            }
            if (hay_pendiente)
            {
                const char *remoto_updated =
                    sync_json_texto(item, "updated_at_client");
                char local_updated[64] = {0};
                if (pendiente.creado_en[0])
                {
                    sync_copiar(local_updated, sizeof(local_updated),
                                pendiente.creado_en);
                    for (char *p = local_updated; *p; p++)
                    {
                        if (*p == ' ')
                        {
                            *p = 'T';
                        }
                    }
                }
                else
                {
                    cJSON *payload =
                        sync_payload_local(uuid, 0, NULL, 0, NULL, 0);
                    const cJSON *actualizado =
                        payload ? cJSON_GetObjectItemCaseSensitive(
                            payload, "actualizado_en")
                        : NULL;
                    if (actualizado && cJSON_IsString(actualizado))
                    {
                        sync_copiar(local_updated, sizeof(local_updated),
                                    actualizado->valuestring);
                    }
                    cJSON_Delete(payload);
                }
                cJSON *payload = sync_payload_local(uuid, 0, NULL, 0, NULL, 0);
                if (remoto_updated && local_updated[0] &&
                        sync_comparar_fechas(remoto_updated, local_updated) > 0)
                {
                    sync_guardar_conflicto(uuid, payload, item);
                    sync_queue_mark_conflict(uuid);
                    res->conflictos++;
                }
                cJSON_Delete(payload);
                continue;
            }

            const cJSON *borrado =
                cJSON_GetObjectItemCaseSensitive(item, "deleted");
            if (cJSON_IsTrue(borrado))
            {
                res->eliminados += sync_eliminar_local(uuid);
            }
            else
            {
                res->importados += sync_aplicar_remoto((cJSON *)item, 0);
            }
        }
        cJSON_Delete(json);
        pagina++;
    }
    while (pagina <= total_paginas);

    sync_copiar(cfg->cursor, sizeof(cfg->cursor), cursor);
    sync_config_set("cursor", cfg->cursor);
    sync_ahora(cfg->ultima_sync, sizeof(cfg->ultima_sync));
    sync_config_set("ultima_sync", cfg->ultima_sync);
    return 1;
}

/* -------------------------------------------------------------- Orquestacion */

static int sync_servidores(const SyncConfig *cfg, char servidores[2][SYNC_URL_SIZE],
                           int *total)
{
    *total = 0;
    char normal[SYNC_URL_SIZE];
    const char *activo = cfg->servidor_activo[0] ? cfg->servidor_activo : NULL;
    if (activo && sync_normalizar_url(activo, normal, sizeof(normal)))
    {
        sync_copiar(servidores[(*total)++], SYNC_URL_SIZE, normal);
    }
    if (sync_normalizar_url(cfg->url_local, normal, sizeof(normal)) &&
            (*total == 0 || strcmp(servidores[0], normal) != 0))
    {
        sync_copiar(servidores[(*total)++], SYNC_URL_SIZE, normal);
    }
    if (sync_normalizar_url(cfg->url_remota, normal, sizeof(normal)))
    {
        int repetido = 0;
        for (int i = 0; i < *total; i++)
        {
            if (strcmp(servidores[i], normal) == 0)
            {
                repetido = 1;
            }
        }
        if (!repetido && *total < 2)
        {
            sync_copiar(servidores[(*total)++], SYNC_URL_SIZE, normal);
        }
    }
    return *total > 0;
}

int sync_ejecutar(SyncResultado *resultado)
{
    if (!resultado)
    {
        return 0;
    }
    memset(resultado, 0, sizeof(*resultado));
    if (!sync_init_db())
    {
        printf("No se pudo inicializar la sincronizacion.\n");
        return 0;
    }
    SyncConfig cfg;
    sync_config_load(&cfg);
    if (!cfg.user_id[0] || !cfg.token[0])
    {
        printf("Inicia sesion de sincronizacion primero.\n");
        return 0;
    }
    char servidores[2][SYNC_URL_SIZE];
    int total = 0;
    if (!sync_servidores(&cfg, servidores, &total))
    {
        printf("Configura una URL local o remota.\n");
        return 0;
    }

    for (int i = 0; i < total; i++)
    {
        printf("Sincronizando con %s ...\n", servidores[i]);
        sync_copiar(cfg.servidor_activo, sizeof(cfg.servidor_activo),
                    servidores[i]);
        sync_config_set("servidor_activo", cfg.servidor_activo);
        int subida = sync_subir(servidores[i], &cfg, resultado);
        if (subida == -2 && sync_refrescar_token(&cfg))
        {
            subida = sync_subir(servidores[i], &cfg, resultado);
        }
        if (subida > 0)
        {
            int entidades_bajada =
                sync_entidades_bajar(servidores[i], &cfg, resultado);
            if (entidades_bajada == -2 && sync_refrescar_token(&cfg))
            {
                entidades_bajada =
                    sync_entidades_bajar(servidores[i], &cfg, resultado);
            }
            if (entidades_bajada == -2)
            {
                subida = -1;
            }
            else
            {
                int entidades =
                    sync_entidades_subir(servidores[i], &cfg, resultado);
                if (entidades == -2 && sync_refrescar_token(&cfg))
                {
                    entidades =
                        sync_entidades_subir(servidores[i], &cfg, resultado);
                }
                if (entidades == -2)
                {
                    subida = -1;
                }
            }
        }
        int bajada =
            subida > 0 ? sync_bajar(servidores[i], &cfg, resultado) : -1;
        if (bajada == -2 && sync_refrescar_token(&cfg))
        {
            bajada = sync_bajar(servidores[i], &cfg, resultado);
        }
        if (subida > 0 && bajada > 0)
        {
            sync_copiar(resultado->servidor, sizeof(resultado->servidor),
                        servidores[i]);
            printf(
                "Sincronizacion completada: %d enviados, %d importados, "
                "%d eliminados, %d conflictos.\n",
                resultado->enviados, resultado->importados,
                resultado->eliminados, resultado->conflictos);
            return 1;
        }
        printf("No se pudo completar con %s; se probara la otra direccion.\n",
               servidores[i]);
    }
    printf("No se pudo sincronizar con ninguna direccion.\n");
    return 0;
}

int sync_subir_todos(SyncResultado *resultado)
{
    if (!sync_init_db())
    {
        return 0;
    }
    if (!sync_queue_enqueue_all())
    {
        printf("No se pudo preparar la cola de subida.\n");
        return 0;
    }
    return sync_ejecutar(resultado);
}

/* ------------------------------------------------------------------- Menu */

static void sync_menu_configurar(void)
{
    SyncConfig cfg;
    sync_config_load(&cfg);
    char entrada[SYNC_URL_SIZE];
    char normal[SYNC_URL_SIZE];

    input_string_default("URL local (Wi-Fi, vacio para conservar): ",
                         cfg.url_local, entrada, (int)sizeof(entrada));
    if (entrada[0])
    {
        if (sync_normalizar_url(entrada, normal, sizeof(normal)))
        {
            sync_copiar(cfg.url_local, sizeof(cfg.url_local), normal);
        }
        else
        {
            printf("URL local invalida (usa http:// o https://).\n");
        }
    }
    input_string_default("URL remota (HTTPS, vacio para conservar): ",
                         cfg.url_remota, entrada, (int)sizeof(entrada));
    if (entrada[0])
    {
        if (sync_normalizar_url(entrada, normal, sizeof(normal)))
        {
            sync_copiar(cfg.url_remota, sizeof(cfg.url_remota), normal);
        }
        else
        {
            printf("URL remota invalida.\n");
        }
    }
    sync_config_save(&cfg);
    printf("Configuracion guardada.\n");
}

static void sync_menu_login(void)
{
    SyncConfig cfg;
    sync_config_load(&cfg);
    char email[256];
    char password[256];
    input_string_default("Correo PocketBase: ", cfg.email, email,
                         (int)sizeof(email));
    input_string("Contrasena: ", password, (int)sizeof(password));
    if (!email[0] || !password[0])
    {
        printf("Correo y contrasena son obligatorios.\n");
        return;
    }
    char servidores[2][SYNC_URL_SIZE];
    int total = 0;
    if (!sync_servidores(&cfg, servidores, &total))
    {
        printf("Configura primero la URL del servidor.\n");
        return;
    }
    char error[SYNC_ERROR_SIZE];
    for (int i = 0; i < total; i++)
    {
        char token[SYNC_TOKEN_SIZE];
        char user_id[128];
        if (sync_http_auth(servidores[i], email, password, token,
                           sizeof(token), user_id, sizeof(user_id), error,
                           sizeof(error)))
        {
            sync_copiar(cfg.email, sizeof(cfg.email), email);
            sync_copiar(cfg.token, sizeof(cfg.token), token);
            sync_copiar(cfg.user_id, sizeof(cfg.user_id), user_id);
            sync_copiar(cfg.servidor_activo, sizeof(cfg.servidor_activo),
                        servidores[i]);
            cfg.cursor[0] = '\0';
            sync_config_save(&cfg);
            printf("Sesion iniciada contra %s\n", servidores[i]);
            return;
        }
        printf("Fallo con %s: %s\n", servidores[i], error);
    }
    printf("No se pudo iniciar sesion.\n");
}

static void sync_menu_estado(void)
{
    sync_init_db();
    SyncConfig cfg;
    sync_config_load(&cfg);
    printf("\nSincronizacion PocketBase\n");
    printf("  Local:   %s\n", cfg.url_local[0] ? cfg.url_local : "-");
    printf("  Remota:  %s\n", cfg.url_remota[0] ? cfg.url_remota : "-");
    printf("  Activa:  %s\n",
           cfg.servidor_activo[0] ? cfg.servidor_activo : "-");
    printf("  Usuario: %s\n", cfg.email[0] ? cfg.email : "-");
    printf("  Sesion:  %s\n", cfg.token[0] ? "iniciada" : "sin iniciar");
    printf("  Pendientes: %d - Conflictos: %d\n",
           sync_queue_count_pending(), sync_queue_count_conflicts());
    printf("  Ultima sync: %s\n",
           cfg.ultima_sync[0] ? cfg.ultima_sync : "nunca");
}

static void sync_menu_conflictos(void)
{
    sync_init_db();
    for (;;)
    {
        SyncConflicto conflicto;
        if (!sync_conflict_first(&conflicto))
        {
            printf("No hay conflictos pendientes.\n");
            return;
        }
        cJSON *local = conflicto.local_json ? cJSON_Parse(conflicto.local_json)
                       : NULL;
        cJSON *remoto = conflicto.remoto_json
                        ? cJSON_Parse(conflicto.remoto_json)
                        : NULL;
        const cJSON *payload_remoto =
            remoto ? cJSON_GetObjectItemCaseSensitive(remoto, "payload") : NULL;
        const char *fecha_local =
            local ? sync_json_texto(local, "fecha_hora") : NULL;
        const char *fecha_remota =
            payload_remoto ? sync_json_texto(payload_remoto, "fecha_hora")
            : NULL;
        printf("\nConflicto %d - partido %s\n", conflicto.id, conflicto.uuid);
        printf("  Local:    %s - goles %d - asist %d\n",
               fecha_local ? fecha_local : "-",
               sync_entero_desde_json(local, "goles", 0),
               sync_entero_desde_json(local, "asistencias", 0));
        printf("  Servidor: %s - goles %d - asist %d\n",
               fecha_remota ? fecha_remota : "-",
               sync_entero_desde_json(payload_remoto, "goles", 0),
               sync_entero_desde_json(payload_remoto, "asistencias", 0));
        printf("  1=Conservar local  2=Conservar servidor  0=Cancelar\n");
        int opcion = input_int("Opcion: ");
        if (opcion == 0)
        {
            cJSON_Delete(local);
            cJSON_Delete(remoto);
            sync_conflict_free(&conflicto);
            return;
        }
        if (opcion == 1 || opcion == 2)
        {
            sync_conflict_resolve(conflicto.id, opcion == 1);
            printf("Conflicto resuelto.\n");
        }
        cJSON_Delete(local);
        cJSON_Delete(remoto);
        sync_conflict_free(&conflicto);
    }
}

static void sync_menu_auto(void)
{
    SyncConfig cfg;
    sync_config_load(&cfg);
    cfg.auto_sync = !cfg.auto_sync;
    sync_config_save(&cfg);
    printf("Sincronizacion automatica al iniciar: %s\n",
           cfg.auto_sync ? "activada" : "desactivada");
}

static void sync_menu_cerrar_sesion(void)
{
    SyncConfig cfg;
    sync_config_load(&cfg);
    cfg.token[0] = '\0';
    cfg.user_id[0] = '\0';
    cfg.cursor[0] = '\0';
    sync_config_save(&cfg);
    printf("Sesion de sincronizacion cerrada.\n");
}

static void sync_menu_sincronizar(void)
{
    SyncResultado resultado;
    sync_ejecutar(&resultado);
}

static void sync_menu_subir_todos(void)
{
    printf("Se enviara todo el historial local (sin duplicar por UUID).\n");
    SyncResultado resultado;
    sync_subir_todos(&resultado);
}

void menu_sincronizacion(void)
{
    sync_init_db();
    MenuItem items[] =
    {
        {1, "Configurar servidor", &sync_menu_configurar},
        {2, "Iniciar sesion", &sync_menu_login},
        {3, "Sincronizar ahora", &sync_menu_sincronizar},
        {4, "Subir todos los partidos", &sync_menu_subir_todos},
        {5, "Estado y pendientes", &sync_menu_estado},
        {6, "Resolver conflictos", &sync_menu_conflictos},
        {7, "Activar/desactivar auto-sync al iniciar", &sync_menu_auto},
        {8, "Cerrar sesion", &sync_menu_cerrar_sesion},
        {0, "Volver", NULL}
    };
    ejecutar_menu("SINCRONIZACION (POCKETBASE)", items, 9);
}
