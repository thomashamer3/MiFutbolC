#include "sync.h"
#include "sync_internal.h"
#include "sync_entidades.h"

#include "db.h"
#include "menu.h"
#include "random_utils.h"
#include "utils.h"

#include <ctype.h>
#include <stdarg.h>
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
        if (a == b)
        {
            return 0;
        }
        if (a)
        {
            return 1;
        }
        return -1;
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
    size_t largo;

    if (!destino || tam == 0)
    {
        return;
    }
    if (!origen)
    {
        destino[0] = '\0';
        return;
    }
    largo = strnlen_s(origen, tam - 1);
    memcpy(destino, origen, largo);
    destino[largo] = '\0';
}

static void sync_ahora(char *salida, size_t tam)
{
    if (!salida || tam == 0)
    {
        return;
    }
    time_t ahora = time(NULL);
    struct tm tm_ahora;
#ifdef _WIN32
    localtime_s(&tm_ahora, &ahora);
#else
    localtime_r(&ahora, &tm_ahora);
#endif
    strftime(salida, tam, "%Y-%m-%dT%H:%M:%S", &tm_ahora);
}

/** Convierte los separadores de fecha de espacio a 'T' (formato ISO). */
static void sync_fecha_espacio_a_iso(char *texto)
{
    if (!texto)
    {
        return;
    }
    for (char *p = texto; *p; p++)
    {
        if (*p == ' ')
        {
            *p = 'T';
        }
    }
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

/** Indica si la columna corresponde a un identificador que no se exporta. */
static int sync_columna_es_id(const char *nombre)
{
    return strcmp(nombre, "id") == 0 || strcmp(nombre, "cancha_id") == 0 ||
           strcmp(nombre, "camiseta_id") == 0 ||
           strcmp(nombre, "botin_id") == 0 ||
           strcmp(nombre, "equipo_id") == 0;
}

/** Recupera los identificadores relacionados que necesita el payload. */
static void sync_leer_ids_relacionados(sqlite3_stmt *stmt, const char *nombre,
                                       int columna, int *cancha_id,
                                       int *camiseta_id, int *botin_id)
{
    if (strcmp(nombre, "cancha_id") == 0)
    {
        *cancha_id = sqlite3_column_int(stmt, columna);
        return;
    }
    if (strcmp(nombre, "camiseta_id") == 0)
    {
        *camiseta_id = sqlite3_column_int(stmt, columna);
        return;
    }
    if (strcmp(nombre, "botin_id") == 0)
    {
        *botin_id = sqlite3_column_int(stmt, columna);
    }
}

/**
 * Agrega al payload un texto obtenido de una enumeracion. Si el texto no
 * existe solo se agrega cuando hay un valor de respaldo.
 */
static void sync_agregar_texto_enum(cJSON *payload, const char *nombre,
                                    const char *valor, const char *respaldo)
{
    if (!valor)
    {
        valor = respaldo;
    }
    if (valor)
    {
        cJSON_AddStringToObject(payload, nombre, valor);
    }
}

/** Agrega los campos de texto que provienen de una enumeracion. */
static int sync_payload_campo_enum(cJSON *payload, sqlite3_stmt *stmt, int i,
                                   const char *nombre)
{
    if (strcmp(nombre, "resultado") == 0)
    {
        sync_agregar_texto_enum(payload, nombre,
                                resultado_texto(sqlite3_column_int(stmt, i)),
                                NULL);
        return 1;
    }
    if (strcmp(nombre, "clima") == 0)
    {
        sync_agregar_texto_enum(payload, nombre,
                                clima_texto(sqlite3_column_int(stmt, i)), NULL);
        return 1;
    }
    if (strcmp(nombre, "tipo_partido") == 0)
    {
        sync_agregar_texto_enum(payload, nombre,
                                tipo_texto(sqlite3_column_int(stmt, i)),
                                "amistoso");
        return 1;
    }
    if (strcmp(nombre, "tarjeta") == 0)
    {
        sync_agregar_texto_enum(payload, nombre,
                                tarjeta_texto(sqlite3_column_int(stmt, i)),
                                NULL);
        return 1;
    }
    if (strcmp(nombre, "estado_cancha") == 0)
    {
        sync_agregar_texto_enum(
            payload, nombre, estado_cancha_texto(sqlite3_column_int(stmt, i)),
            NULL);
        return 1;
    }
    return 0;
}

/** Agrega los campos especiales que no dependen del tipo de la columna. */
static int sync_payload_campo_especial(cJSON *payload, const char *nombre,
                                       const char *texto, char *uuid_salida,
                                       size_t uuid_tam)
{
    if (strcmp(nombre, "formato_partido") == 0)
    {
        cJSON_AddNumberToObject(payload, "formato_partido",
                                formato_numero(texto));
        cJSON_AddStringToObject(payload, "formato_partido_texto",
                                texto ? texto : "");
        return 1;
    }
    if (strcmp(nombre, "fecha_hora") == 0)
    {
        char iso[64];
        sync_copiar(iso, sizeof(iso), texto ? texto : "");
        sync_fecha_espacio_a_iso(iso);
        cJSON_AddStringToObject(payload, nombre, iso);
        return 1;
    }
    if (strcmp(nombre, "uuid") == 0)
    {
        sync_copiar(uuid_salida, uuid_tam, texto);
        return 1;
    }
    return 0;
}

/** Agrega un campo entero; omite los marcadores negativos de goles. */
static void sync_payload_campo_entero(cJSON *payload, sqlite3_stmt *stmt,
                                      int i, const char *nombre)
{
    const int valor = sqlite3_column_int(stmt, i);

    if ((strcmp(nombre, "goles_equipo") == 0 ||
            strcmp(nombre, "goles_rival") == 0) && valor < 0)
    {
        /* El escritorio usa -1 como "sin marcador". */
        return;
    }
    cJSON_AddNumberToObject(payload, nombre, (double)valor);
}

/**
 * Convierte una columna del partido y la agrega al payload. Cada campo aplica
 * su propia representacion (enumeraciones, fechas y texto libre).
 */
static void sync_payload_agregar_columna(cJSON *payload, sqlite3_stmt *stmt,
        int i, const char *nombre, int tipo,
        char *uuid_salida, size_t uuid_tam)
{
    const char *texto = (const char *)sqlite3_column_text(stmt, i);

    if (sync_payload_campo_enum(payload, stmt, i, nombre))
    {
        return;
    }
    if (sync_payload_campo_especial(payload, nombre, texto, uuid_salida,
                                    uuid_tam))
    {
        return;
    }
    if (tipo == SQLITE_INTEGER)
    {
        sync_payload_campo_entero(payload, stmt, i, nombre);
        return;
    }
    if (tipo == SQLITE_FLOAT)
    {
        cJSON_AddNumberToObject(payload, nombre,
                                sqlite3_column_double(stmt, i));
        return;
    }
    cJSON_AddStringToObject(payload, nombre, texto ? texto : "");
}

/** Asegura que el partido tenga UUID, generandolo y guardandolo si falta. */
static void sync_asegurar_uuid(cJSON *payload, char *uuid_salida,
                               size_t uuid_tam, int partido_id)
{
    if (!uuid_salida || uuid_tam == 0)
    {
        return;
    }
    if (!uuid_salida[0])
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
    if (uuid_salida[0])
    {
        cJSON_AddStringToObject(payload, "uuid", uuid_salida);
    }
}

/** Normaliza `actualizado_en` y lo completa con la fecha si falta. */
static void sync_preparar_actualizado(cJSON *payload)
{
    cJSON *actualizado =
        cJSON_GetObjectItemCaseSensitive(payload, "actualizado_en");
    if (actualizado && cJSON_IsString(actualizado) &&
            actualizado->valuestring)
    {
        sync_fecha_espacio_a_iso(actualizado->valuestring);
    }
    if (!actualizado || !cJSON_IsString(actualizado) ||
            !actualizado->valuestring || !actualizado->valuestring[0])
    {
        const cJSON *fecha =
            cJSON_GetObjectItemCaseSensitive(payload, "fecha_hora");
        cJSON_AddStringToObject(
            payload, "actualizado_en",
            (fecha && cJSON_IsString(fecha)) ? fecha->valuestring
            : "1970-01-01T00:00:00");
    }
}

/** Prepara la consulta del partido local segun el criterio de busqueda. */
static sqlite3_stmt *sync_preparar_consulta_partido(const char *uuid,
        int partido_id)
{
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
    return stmt;
}

/**
 * Recorre las columnas de la fila, agrega cada valor al payload y recoge los
 * identificadores relacionados.
 */
static void sync_agregar_columnas_partido(cJSON *payload, sqlite3_stmt *stmt,
        char *uuid_salida, size_t uuid_tam,
        int *cancha_id, int *camiseta_id,
        int *botin_id)
{
    const int columnas = sqlite3_column_count(stmt);

    for (int i = 0; i < columnas; i++)
    {
        const char *nombre = sqlite3_column_name(stmt, i);
        int tipo = 0;

        if (!nombre)
        {
            continue;
        }
        if (sync_columna_es_id(nombre))
        {
            sync_leer_ids_relacionados(stmt, nombre, i, cancha_id,
                                       camiseta_id, botin_id);
            continue;
        }
        tipo = sqlite3_column_type(stmt, i);
        if (tipo == SQLITE_NULL)
        {
            continue;
        }
        sync_payload_agregar_columna(payload, stmt, i, nombre, tipo,
                                     uuid_salida, uuid_tam);
    }
}

/** Agrega al payload los nombres de las entidades relacionadas. */
static void sync_agregar_nombres_relacionados(cJSON *payload, int cancha_id,
        int camiseta_id, int botin_id)
{
    char cancha[256];
    char camiseta[256];
    char botin[256];

    sync_nombre_por_id("cancha", cancha_id, cancha, sizeof(cancha));
    sync_nombre_por_id("camiseta", camiseta_id, camiseta, sizeof(camiseta));
    sync_nombre_por_id("botin", botin_id, botin, sizeof(botin));
    cJSON_AddStringToObject(payload, "cancha_nombre", cancha);
    cJSON_AddStringToObject(payload, "camiseta_nombre", camiseta);
    cJSON_AddStringToObject(payload, "botin_nombre", botin);
}

/**
 * Construye el payload local de un partido. `por_uuid` elige el criterio de
 * busqueda. Devuelve un objeto cJSON que el llamador debe liberar.
 */
static cJSON *sync_payload_local(const char *uuid, int partido_id,
                                 char *uuid_salida, size_t uuid_tam,
                                 char *error, size_t error_tam)
{
    cJSON *payload = NULL;
    sqlite3_stmt *stmt = NULL;
    int cancha_id = 0;
    int camiseta_id = 0;
    int botin_id = 0;

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
    stmt = sync_preparar_consulta_partido(uuid, partido_id);
    if (!stmt)
    {
        return NULL;
    }
    payload = cJSON_CreateObject();
    if (!payload)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }
    sync_agregar_columnas_partido(payload, stmt, uuid_salida, uuid_tam,
                                  &cancha_id, &camiseta_id, &botin_id);
    sqlite3_finalize(stmt);
    sync_asegurar_uuid(payload, uuid_salida, uuid_tam, partido_id);
    sync_agregar_nombres_relacionados(payload, cancha_id, camiseta_id, botin_id);
    sync_preparar_actualizado(payload);
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

/** Columnas que se persisten en `partido`: nombre SQL y valor JSON de origen. */
typedef struct
{
    const char *nombres[256];
    const cJSON *valores[256];
    int total;
} SyncColumnas;

/** Valores de contexto necesarios para enlazar las columnas de un partido. */
typedef struct
{
    char fecha[64];
    const char *uuid;
    const char *remoto_updated;
    int cancha_id;
    int camiseta_id;
    int botin_id;
    const cJSON *payload;
} SyncBindContexto;

/** Indica si la columna la aporta la sincronizacion y no el JSON remoto. */
static int sync_columna_omitida(const char *nombre)
{
    static const char *const omitidas[] =
    {
        "id", "cancha_id", "camiseta_id", "botin_id", "equipo_id",
        "cancha_nombre", "camiseta_nombre", "botin_nombre",
        "formato_partido_texto", "actualizado_en", "fecha_hora", "uuid"
    };
    for (size_t i = 0; i < sizeof(omitidas) / sizeof(omitidas[0]); i++)
    {
        if (strcmp(nombre, omitidas[i]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

/** Obtiene la copia del payload remoto que se va a persistir. */
static cJSON *sync_payload_desde_registro(const cJSON *registro)
{
    const cJSON *origen = cJSON_GetObjectItemCaseSensitive(registro, "payload");
    if (cJSON_IsObject(origen))
    {
        return cJSON_Duplicate(origen, 1);
    }
    cJSON *payload = cJSON_CreateObject();
    if (!payload)
    {
        return NULL;
    }
    const cJSON *fecha = cJSON_GetObjectItemCaseSensitive(registro, "fecha");
    if (cJSON_IsString(fecha))
    {
        cJSON_AddStringToObject(payload, "fecha_hora", fecha->valuestring);
    }
    cJSON_AddNumberToObject(payload, "goles",
                            sync_entero_desde_json(registro, "goles", 0));
    cJSON_AddNumberToObject(payload, "asistencias",
                            sync_entero_desde_json(registro, "asistencias", 0));
    return payload;
}

/**
 * Comprueba si la version local es mas reciente que la remota. Si procede,
 * libera el payload y devuelve 1 para que el llamador aborte la aplicacion.
 */
/**
 * Indica si la copia local es mas reciente que la remota. Devuelve 1 y libera
 * `payload` cuando hay que descartar el registro remoto, 0 si hay que aplicarlo.
 */
static int sync_local_mas_nuevo(int id_local, const char *remoto_updated,
                                int forzar, cJSON *payload)
{
    if (id_local <= 0 || forzar)
    {
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "SELECT COALESCE(actualizado_en,'') FROM partido"
                           " WHERE id=? LIMIT 1;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    int descartar = 0;
    sqlite3_bind_int(stmt, 1, id_local);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *local_updated =
            (const char *)sqlite3_column_text(stmt, 0);
        if (local_updated && remoto_updated &&
                sync_comparar_fechas(remoto_updated, local_updated) <= 0)
        {
            descartar = 1;
        }
    }
    sqlite3_finalize(stmt);
    if (descartar)
    {
        cJSON_Delete(payload);
    }
    return descartar;
}

/** Resuelve los identificadores de cancha, camiseta y botin del payload. */
static void sync_resolver_ids_relacionados(const cJSON *payload,
        SyncBindContexto *ctx)
{
    const char *cancha_nombre = sync_json_texto(payload, "cancha_nombre");
    const char *camiseta_nombre = sync_json_texto(payload, "camiseta_nombre");
    const char *botin_nombre = sync_json_texto(payload, "botin_nombre");
    ctx->cancha_id = sync_id_por_nombre("cancha", cancha_nombre, "Sin cancha");
    ctx->camiseta_id =
        sync_id_por_nombre("camiseta", camiseta_nombre, "Sin camiseta");
    ctx->botin_id = sync_id_por_nombre("botin", botin_nombre, NULL);
}

/** Obtiene la fecha del payload con respaldo en el registro remoto. */
static int sync_fecha_desde(const cJSON *payload, const cJSON *registro,
                            char *fecha, size_t tam)
{
    sync_json_copiar_texto(payload, "fecha_hora", fecha, tam);
    if (!fecha[0])
    {
        sync_json_copiar_texto(registro, "fecha", fecha, tam);
    }
    if (!fecha[0])
    {
        return 0;
    }
    sync_fecha_iso_a_espacio(fecha);
    return 1;
}

/** Agrega una columna del payload a la lista que se persistira. */
static void sync_agregar_columna_payload(SyncColumnas *columnas,
        const char *nombre,
        const cJSON *item)
{
    if (columnas->total >= 250 || sync_columna_omitida(nombre))
    {
        return;
    }
    if (!sync_columna_existe(db, "partido", nombre))
    {
        return;
    }
    columnas->nombres[columnas->total] = nombre;
    columnas->valores[columnas->total] = item;
    columnas->total++;
}

/** Recorre el payload y arma la lista de columnas que admite la tabla. */
static void sync_columnas_desde_payload(const cJSON *payload,
                                        SyncColumnas *columnas)
{
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, payload)
    {
        if (!item->string)
        {
            continue;
        }
        sync_agregar_columna_payload(columnas, item->string, item);
    }
}

/** Agrega las columnas que siempre aporta la sincronizacion. */
static void sync_columnas_agregar_fijas(SyncColumnas *columnas, int botin_id,
                                        const char *remoto_updated)
{
    static const char *const fijas[] = {"fecha_hora", "uuid", "cancha_id",
                                        "camiseta_id"
                                       };
    for (size_t i = 0; i < sizeof(fijas) / sizeof(fijas[0]); i++)
    {
        columnas->nombres[columnas->total] = fijas[i];
        columnas->valores[columnas->total] = NULL;
        columnas->total++;
    }
    if (sync_columna_existe(db, "partido", "botin_id") && botin_id > 0)
    {
        columnas->nombres[columnas->total] = "botin_id";
        columnas->valores[columnas->total] = NULL;
        columnas->total++;
    }
    if (sync_columna_existe(db, "partido", "actualizado_en") && remoto_updated)
    {
        columnas->nombres[columnas->total] = "actualizado_en";
        columnas->valores[columnas->total] = NULL;
        columnas->total++;
    }
}

/** Agrega texto formateado al final de @p destino sin desbordar el buffer. */
static int sync_agregar_texto(char *destino, size_t tam, size_t *usados,
                              const char *formato, ...)
{
    va_list args;
    int escrito;
    size_t disponible;

    if (*usados >= tam)
    {
        return 0;
    }
    disponible = tam - *usados;
    va_start(args, formato);
    escrito = vsnprintf(destino + *usados, disponible, formato, args);
    va_end(args);
    if (escrito < 0 || (size_t)escrito >= disponible)
    {
        return 0;
    }
    *usados += (size_t)escrito;
    return 1;
}

static int sync_sql_guardar(const SyncColumnas *columnas, int id_local,
                            char *sql, size_t tam)
{
    size_t usados = 0;

    if (id_local > 0)
    {
        if (!sync_agregar_texto(sql, tam, &usados, "UPDATE partido SET "))
        {
            return 0;
        }
        for (int i = 0; i < columnas->total; i++)
        {
            if (!sync_agregar_texto(sql, tam, &usados, "%s\"%s\"=?",
                                    i == 0 ? "" : ", ", columnas->nombres[i]))
            {
                return 0;
            }
        }
        return sync_agregar_texto(sql, tam, &usados, " WHERE id=?;");
    }

    if (!sync_agregar_texto(sql, tam, &usados, "INSERT INTO partido ("))
    {
        return 0;
    }
    for (int i = 0; i < columnas->total; i++)
    {
        if (!sync_agregar_texto(sql, tam, &usados, "%s\"%s\"",
                                i == 0 ? "" : ", ", columnas->nombres[i]))
        {
            return 0;
        }
    }
    if (!sync_agregar_texto(sql, tam, &usados, ") VALUES ("))
    {
        return 0;
    }
    for (int i = 0; i < columnas->total; i++)
    {
        if (!sync_agregar_texto(sql, tam, &usados, "%s?", i == 0 ? "" : ", "))
        {
            return 0;
        }
    }
    return sync_agregar_texto(sql, tam, &usados, ");");
}

/** Texto que se guarda en la columna `formato_partido`. */
static const char *sync_texto_formato(const cJSON *payload, int numero)
{
    const char *texto = sync_json_texto(payload, "formato_partido_texto");
    if (texto && *texto)
    {
        return texto;
    }
    const cJSON *formato =
        cJSON_GetObjectItemCaseSensitive(payload, "formato_partido");
    if (cJSON_IsString(formato) && formato->valuestring)
    {
        return formato->valuestring;
    }
    return formato_texto(numero);
}

/** Une con comas los textos de un arreglo JSON, sin exceder el buffer. */
static void sync_tags_array(const cJSON *valor, char *tags, size_t tam)
{
    if (!valor || !tags || tam == 0)
    {
        return;
    }
    tags[0] = '\0';
    const cJSON *tag = NULL;
    cJSON_ArrayForEach(tag, valor)
    {
        if (!cJSON_IsString(tag) || !tag->valuestring)
        {
            continue;
        }
        size_t usado = strlen(tags);
        if (usado > 0)
        {
            snprintf(tags + usado, tam - usado, ",%s", tag->valuestring);
        }
        else
        {
            snprintf(tags + usado, tam - usado, "%s", tag->valuestring);
        }
    }
}

/** Campos numericos del partido que se guardan como entero. */
static int sync_campo_entero(const char *nombre)
{
    return strcmp(nombre, "goles") == 0 ||
           strcmp(nombre, "asistencias") == 0 ||
           strcmp(nombre, "dia") == 0 || strcmp(nombre, "precio") == 0;
}

/**
 * Enlaza las columnas con conversion propia (fechas, ids y enumeraciones).
 * Devuelve el siguiente indice o 0 si la columna no es especial.
 */
static int sync_bindear_especial(sqlite3_stmt *stmt, int indice,
                                 const char *nombre,
                                 const SyncBindContexto *ctx)
{
    if (strcmp(nombre, "fecha_hora") == 0)
    {
        sqlite3_bind_text(stmt, indice, ctx->fecha, -1, SQLITE_TRANSIENT);
        return indice + 1;
    }
    if (strcmp(nombre, "uuid") == 0)
    {
        sqlite3_bind_text(stmt, indice, ctx->uuid, -1, SQLITE_TRANSIENT);
        return indice + 1;
    }
    if (strcmp(nombre, "cancha_id") == 0)
    {
        sqlite3_bind_int(stmt, indice, ctx->cancha_id);
        return indice + 1;
    }
    if (strcmp(nombre, "camiseta_id") == 0)
    {
        sqlite3_bind_int(stmt, indice, ctx->camiseta_id);
        return indice + 1;
    }
    if (strcmp(nombre, "botin_id") == 0)
    {
        if (ctx->botin_id > 0)
        {
            sqlite3_bind_int(stmt, indice, ctx->botin_id);
        }
        else
        {
            sqlite3_bind_null(stmt, indice);
        }
        return indice + 1;
    }
    if (strcmp(nombre, "actualizado_en") == 0)
    {
        sqlite3_bind_text(stmt, indice, ctx->remoto_updated, -1,
                          SQLITE_TRANSIENT);
        return indice + 1;
    }
    if (strcmp(nombre, "formato_partido") == 0)
    {
        int num = sync_entero_desde_json(ctx->payload, "formato_partido", 0);
        sqlite3_bind_text(stmt, indice,
                          sync_texto_formato(ctx->payload, num), -1,
                          SQLITE_TRANSIENT);
        return indice + 1;
    }
    if (strcmp(nombre, "resultado") == 0)
    {
        sqlite3_bind_int(
            stmt, indice,
            resultado_codigo(sync_json_texto(ctx->payload, "resultado")));
        return indice + 1;
    }
    if (strcmp(nombre, "clima") == 0)
    {
        sqlite3_bind_int(
            stmt, indice, clima_codigo(sync_json_texto(ctx->payload, "clima")));
        return indice + 1;
    }
    if (strcmp(nombre, "tipo_partido") == 0)
    {
        sqlite3_bind_int(
            stmt, indice,
            tipo_codigo(sync_json_texto(ctx->payload, "tipo_partido")));
        return indice + 1;
    }
    if (strcmp(nombre, "tarjeta") == 0)
    {
        sqlite3_bind_int(
            stmt, indice,
            tarjeta_codigo(sync_json_texto(ctx->payload, "tarjeta")));
        return indice + 1;
    }
    if (strcmp(nombre, "estado_cancha") == 0)
    {
        sqlite3_bind_int(
            stmt, indice,
            estado_cancha_codigo(sync_json_texto(ctx->payload, "estado_cancha")));
        return indice + 1;
    }
    return 0;
}

/** Enlaza una columna generica del payload segun su tipo JSON. */
static int sync_bindear_columna(sqlite3_stmt *stmt, int indice,
                                const char *nombre, const cJSON *valor,
                                const SyncBindContexto *ctx)
{
    int siguiente = sync_bindear_especial(stmt, indice, nombre, ctx);
    if (siguiente > 0)
    {
        return siguiente;
    }
    if (!valor || cJSON_IsNull(valor))
    {
        sqlite3_bind_null(stmt, indice);
        return indice + 1;
    }
    if (cJSON_IsString(valor))
    {
        sqlite3_bind_text(stmt, indice, valor->valuestring, -1,
                          SQLITE_TRANSIENT);
        return indice + 1;
    }
    if (cJSON_IsNumber(valor))
    {
        if (sync_campo_entero(nombre))
        {
            sqlite3_bind_int(stmt, indice, (int)valor->valuedouble);
        }
        else
        {
            sqlite3_bind_double(stmt, indice, valor->valuedouble);
        }
        return indice + 1;
    }
    if (cJSON_IsBool(valor))
    {
        sqlite3_bind_int(stmt, indice, cJSON_IsTrue(valor) ? 1 : 0);
        return indice + 1;
    }
    if (cJSON_IsArray(valor))
    {
        char tags[1024];
        sync_tags_array(valor, tags, sizeof(tags));
        sqlite3_bind_text(stmt, indice, tags, -1, SQLITE_TRANSIENT);
        return indice + 1;
    }
    char *texto = sync_json_imprimir(valor);
    sqlite3_bind_text(stmt, indice, texto ? texto : "", -1, SQLITE_TRANSIENT);
    cJSON_free(texto);
    return indice + 1;
}

/** Enlaza todas las columnas y ejecuta la sentencia de persistencia. */
static int sync_guardar_partido(sqlite3_stmt *stmt,
                                const SyncColumnas *columnas,
                                const SyncBindContexto *ctx, int id_local)
{
    int indice = 1;
    for (int i = 0; i < columnas->total; i++)
    {
        indice = sync_bindear_columna(stmt, indice, columnas->nombres[i],
                                      columnas->valores[i], ctx);
    }
    if (id_local > 0)
    {
        sqlite3_bind_int(stmt, indice, id_local);
    }
    return sqlite3_step(stmt) == SQLITE_DONE;
}

static int sync_aplicar_remoto(const cJSON *registro, int forzar)
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

    cJSON *payload = sync_payload_desde_registro(registro);
    if (!payload)
    {
        return 0;
    }
    const char *remoto_updated = sync_json_texto(registro, "updated_at_client");
    const int id_local = sync_id_por_uuid(uuid);
    if (sync_local_mas_nuevo(id_local, remoto_updated, forzar, payload))
    {
        return 0;
    }

    SyncBindContexto ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.uuid = uuid;
    ctx.remoto_updated = remoto_updated;
    ctx.payload = payload;
    sync_resolver_ids_relacionados(payload, &ctx);
    if (!sync_fecha_desde(payload, registro, ctx.fecha, sizeof(ctx.fecha)))
    {
        cJSON_Delete(payload);
        return 0;
    }

    SyncColumnas columnas;
    memset(&columnas, 0, sizeof(columnas));
    sync_columnas_desde_payload(payload, &columnas);
    sync_columnas_agregar_fijas(&columnas, ctx.botin_id, remoto_updated);

    char sql[4096];
    if (!sync_sql_guardar(&columnas, id_local, sql, sizeof(sql)))
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
    int ok = sync_guardar_partido(stmt, &columnas, &ctx, id_local);
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
        if (sync_columna_existe(db, "partido", "actualizado_en") &&
                sqlite3_prepare_v2(
                    db,
                    "UPDATE partido SET actualizado_en=? WHERE uuid=?;",
                    -1, &upd, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(upd, 1, ahora, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(upd, 2, uuid, -1, SQLITE_TRANSIENT);
            sqlite3_step(upd);
            sqlite3_finalize(upd);
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
    const char *p = cursor;
    while (*p)
    {
        if (j + 4 >= tam)
        {
            break;
        }
        unsigned char c = (unsigned char)*p;
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
        p++;
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
        const cJSON *primero = cJSON_GetArrayItem(items, 0);
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

/** Datos necesarios para enviar un borrado logico (tombstone). */
typedef struct
{
    const char *base;
    const char *token;
    const char *owner;
    const char *uuid;
    const char *record_id;
    const char *actualizado;
    const cJSON *remoto;
    char *error;
    size_t error_tam;
} SyncTombstone;

/** Envia un borrado logico (tombstone) para que otros equipos lo reciban. */
static int sync_enviar_tombstone(const SyncTombstone *datos)
{
    if (!datos)
    {
        return -1;
    }
    cJSON *tombstone = cJSON_CreateObject();
    if (!tombstone)
    {
        return -1;
    }
    cJSON_AddStringToObject(tombstone, "uuid", datos->uuid);
    cJSON_AddStringToObject(tombstone, "owner", datos->owner);
    cJSON_AddBoolToObject(tombstone, "deleted", 1);
    cJSON_AddStringToObject(tombstone, "updated_at_client",
                            datos->actualizado ? datos->actualizado : "");
    const cJSON *fecha =
        datos->remoto ? cJSON_GetObjectItemCaseSensitive(datos->remoto, "fecha")
        : NULL;
    if (fecha && cJSON_IsString(fecha) && fecha->valuestring)
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
    int envio = sync_enviar_registro(datos->base, datos->token,
                                     datos->record_id, tombstone, datos->error,
                                     datos->error_tam);
    cJSON_Delete(tombstone);
    return envio;
}

/* ------------------------------------------------------------------ Subida */

/** Contexto comun del bucle de subida. */
typedef struct
{
    const char *base;
    SyncConfig *cfg;
    SyncResultado *res;
    char error[SYNC_ERROR_SIZE];
} SyncSubidaContexto;

/** Normaliza la marca de tiempo pendiente al formato ISO (con 'T'). */
static void sync_marca_iso(const char *creado_en, char *marca, size_t tam)
{
    if (!marca || tam == 0)
    {
        return;
    }
    sync_copiar(marca, tam, creado_en);
    sync_fecha_espacio_a_iso(marca);
    if (!marca[0])
    {
        sync_ahora(marca, tam);
    }
}

/**
 * Registra un conflicto si el servidor tiene una version mas reciente.
 * Devuelve 1 si hubo conflicto y el llamador debe continuar con el siguiente.
 */
static int sync_conflicto_borrado(const SyncDirty *pendiente,
                                  const char *record_id,
                                  const char *remoto_updated,
                                  const char *marca, const cJSON *remoto,
                                  SyncResultado *res)
{
    if (!record_id || !remoto_updated ||
            sync_comparar_fechas(remoto_updated, marca) <= 0)
    {
        return 0;
    }
    cJSON *local = cJSON_CreateObject();
    cJSON_AddStringToObject(local, "uuid", pendiente->uuid);
    cJSON_AddStringToObject(local, "actualizado_en", marca);
    sync_guardar_conflicto(pendiente->uuid, local, remoto);
    cJSON_Delete(local);
    sync_queue_mark_conflict(pendiente->uuid);
    res->conflictos++;
    return 1;
}

/** Envia el tombstone de un registro pendiente de borrado. */
static int sync_enviar_borrado(SyncSubidaContexto *ctx,
                               const SyncDirty *pendiente, const char *marca,
                               const char *record_id, const cJSON *remoto)
{
    SyncTombstone datos =
    {
        .base = ctx->base,
        .token = ctx->cfg->token,
        .owner = ctx->cfg->user_id,
        .uuid = pendiente->uuid,
        .record_id = record_id,
        .actualizado = marca,
        .remoto = remoto,
        .error = ctx->error,
        .error_tam = sizeof(ctx->error)
    };
    return sync_enviar_tombstone(&datos);
}

/** Procesa una operacion pendiente de tipo 'eliminar'. */
static int sync_subir_eliminar(SyncSubidaContexto *ctx,
                               const SyncDirty *pendiente,
                               const char *record_id, const cJSON *remoto)
{
    char marca[64];
    sync_marca_iso(pendiente->creado_en, marca, sizeof(marca));
    const char *remoto_updated =
        remoto ? sync_json_texto(remoto, "updated_at_client") : NULL;
    if (sync_conflicto_borrado(pendiente, record_id, remoto_updated, marca,
                               remoto, ctx->res))
    {
        return 1;
    }
    if (record_id)
    {
        int envio = sync_enviar_borrado(ctx, pendiente, marca, record_id, remoto);
        if (envio == -2)
        {
            if (!sync_refrescar_token(ctx->cfg))
            {
                sync_queue_mark_error(pendiente->id, "Sesion vencida");
                ctx->res->errores++;
                return -2;
            }
            return 1;
        }
        if (envio < 0)
        {
            sync_queue_mark_error(pendiente->id, ctx->error);
            ctx->res->errores++;
            return -1;
        }
    }
    sync_queue_mark_done(pendiente->uuid);
    ctx->res->enviados++;
    return 1;
}

/** Propaga el borrado de un partido que ya no existe en local. */
static int sync_subir_sin_payload(SyncSubidaContexto *ctx,
                                  const SyncDirty *pendiente,
                                  const char *record_id, const cJSON *remoto)
{
    char marca[64];
    sync_marca_iso(pendiente->creado_en, marca, sizeof(marca));
    const char *remoto_updated =
        remoto ? sync_json_texto(remoto, "updated_at_client") : NULL;
    if (sync_conflicto_borrado(pendiente, record_id, remoto_updated, marca,
                               remoto, ctx->res))
    {
        return 1;
    }
    if (record_id)
    {
        int envio = sync_enviar_borrado(ctx, pendiente, marca, record_id, remoto);
        if (envio < 0)
        {
            sync_queue_mark_error(pendiente->id, ctx->error);
            ctx->res->errores++;
            return envio == -2 ? -2 : -1;
        }
    }
    sync_queue_mark_done(pendiente->uuid);
    return 1;
}

/** Envia el payload de un partido y libera el registro intermedio. */
static int sync_enviar_payload(SyncSubidaContexto *ctx, const char *uuid,
                               const char *record_id, const cJSON *payload)
{
    cJSON *registro = sync_registro_remoto(payload, uuid, ctx->cfg->user_id, 0);
    int envio = sync_enviar_registro(ctx->base, ctx->cfg->token, record_id,
                                     registro, ctx->error, sizeof(ctx->error));
    cJSON_Delete(registro);
    return envio;
}

/** Traduce el resultado de la busqueda remota al estado de la cola. */
static int sync_cerrar_busqueda_pendiente(SyncSubidaContexto *ctx,
        const SyncDirty *pendiente,
        int busqueda)
{
    if (busqueda == -2)
    {
        if (sync_refrescar_token(ctx->cfg))
        {
            return 1;
        }
        sync_queue_mark_error(pendiente->id, "Sesion vencida");
        ctx->res->errores++;
        return -2;
    }
    if (busqueda < 0)
    {
        sync_queue_mark_error(pendiente->id,
                              ctx->error[0] ? ctx->error : "Error de red");
        ctx->res->errores++;
        return -1;
    }
    return 0;
}

/**
 * Sobrescribe el uuid del payload con el de la cola y aplica la marca de
 * creacion cuando existe.
 */
static void sync_ajustar_payload_pendiente(cJSON *payload,
        const SyncDirty *pendiente,
        const char *uuid)
{
    cJSON_DeleteItemFromObjectCaseSensitive(payload, "uuid");
    cJSON_AddStringToObject(payload, "uuid", uuid);
    if (pendiente->creado_en[0])
    {
        char marca[64];

        sync_marca_iso(pendiente->creado_en, marca, sizeof(marca));
        cJSON_DeleteItemFromObjectCaseSensitive(payload, "actualizado_en");
        cJSON_AddStringToObject(payload, "actualizado_en", marca);
    }
}

/** Registra un conflicto de actualizacion cuando el remoto es mas nuevo. */
static int sync_conflicto_pendiente(SyncSubidaContexto *ctx,
                                    const cJSON *payload, const cJSON *remoto,
                                    const char *uuid)
{
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
        ctx->res->conflictos++;
        return 1;
    }
    return 0;
}

/** Registra el resultado del envio del payload en la cola de subida. */
static int sync_cerrar_envio_payload(SyncSubidaContexto *ctx,
                                     const SyncDirty *pendiente,
                                     const char *uuid, int envio)
{
    if (envio == -2)
    {
        if (!sync_refrescar_token(ctx->cfg))
        {
            sync_queue_mark_error(pendiente->id, "Sesion vencida");
            ctx->res->errores++;
            return -2;
        }
        return 1;
    }
    if (envio < 0)
    {
        sync_queue_mark_error(pendiente->id, ctx->error);
        ctx->res->errores++;
        return -1;
    }
    sync_queue_mark_done(uuid);
    ctx->res->enviados++;
    return 1;
}

/** Procesa una entrada de la cola de subida. */
static int sync_subir_pendiente(SyncSubidaContexto *ctx,
                                const SyncDirty *pendiente)
{
    cJSON *remoto = NULL;
    cJSON *payload = NULL;
    int busqueda = 0;
    int estado = 0;
    int envio = 0;
    const char *record_id = NULL;
    char uuid[SYNC_UUID_SIZE] = {0};

    if (!pendiente->uuid[0])
    {
        sync_queue_mark_done(pendiente->uuid);
        return 1;
    }

    busqueda = sync_buscar_remoto(ctx->base, ctx->cfg->token, pendiente->uuid,
                                  &remoto, ctx->error, sizeof(ctx->error));
    if (busqueda < 0)
    {
        cJSON_Delete(remoto);
        return sync_cerrar_busqueda_pendiente(ctx, pendiente, busqueda);
    }

    record_id = remoto ? sync_json_texto(remoto, "id") : NULL;
    if (strcmp(pendiente->operacion, "eliminar") == 0)
    {
        estado = sync_subir_eliminar(ctx, pendiente, record_id, remoto);
        cJSON_Delete(remoto);
        return estado;
    }

    payload = sync_payload_local(pendiente->uuid, 0, uuid, sizeof(uuid),
                                 ctx->error, sizeof(ctx->error));
    if (!payload)
    {
        /* El partido ya no existe: propagar borrado logico. */
        estado = sync_subir_sin_payload(ctx, pendiente, record_id, remoto);
        cJSON_Delete(remoto);
        return estado;
    }
    sync_copiar(uuid, sizeof(uuid), pendiente->uuid);
    sync_ajustar_payload_pendiente(payload, pendiente, uuid);
    if (sync_conflicto_pendiente(ctx, payload, remoto, uuid))
    {
        cJSON_Delete(payload);
        cJSON_Delete(remoto);
        return 1;
    }

    envio = sync_enviar_payload(ctx, uuid, record_id, payload);
    cJSON_Delete(payload);
    cJSON_Delete(remoto);
    return sync_cerrar_envio_payload(ctx, pendiente, uuid, envio);
}

static int sync_subir(const char *base, SyncConfig *cfg, SyncResultado *res)
{
    SyncSubidaContexto ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.base = base;
    ctx.cfg = cfg;
    ctx.res = res;
    for (;;)
    {
        SyncDirty pendiente;
        if (!sync_queue_next(&pendiente))
        {
            break;
        }
        ctx.error[0] = '\0';
        int estado = sync_subir_pendiente(&ctx, &pendiente);
        if (estado < 0)
        {
            return estado;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ Bajada */

/** Construye la URL de una pagina de descarga, con cursor o sin el. */
static void sync_url_bajar(const char *base, const char *cursor, int pagina,
                           char *url, size_t tam)
{
    if (cursor[0])
    {
        char escapado[256];
        sync_url_escapar_cursor(cursor, escapado, sizeof(escapado));
        snprintf(url, tam,
                 "%s/api/collections/%s/records?page=%d&perPage=100&"
                 "sort=updated_at_client&"
                 "filter=updated_at_client>%%22%s%%22",
                 base, SYNC_COLECCION, pagina, escapado);
        return;
    }
    snprintf(url, tam,
             "%s/api/collections/%s/records?page=%d&perPage=100&"
             "sort=updated_at_client",
             base, SYNC_COLECCION, pagina);
}

/**
 * Descarga una pagina de registros remotos. Devuelve 1 con el JSON en
 * `salida`, -1 si hubo error de red o -2 si la sesion vencio.
 */
static int sync_bajar_pagina(const char *base, const SyncConfig *cfg,
                             const char *cursor, int pagina, cJSON **salida)
{
    if (!salida)
    {
        return -1;
    }
    *salida = NULL;
    char url[SYNC_URL_SIZE + 1024];
    sync_url_bajar(base, cursor, pagina, url, sizeof(url));
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
    *salida = json;
    return 1;
}

/** Busca en la cola local la ultima operacion pendiente de un partido. */
static int sync_buscar_pendiente(const char *uuid, SyncDirty *pendiente)
{
    sqlite3_stmt *stmt = NULL;
    int hay = 0;
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
            hay = 1;
            pendiente->id = sqlite3_column_int(stmt, 0);
            sync_copiar(pendiente->uuid, sizeof(pendiente->uuid), uuid);
            sync_copiar(pendiente->creado_en, sizeof(pendiente->creado_en),
                        (const char *)sqlite3_column_text(stmt, 5));
        }
        sqlite3_finalize(stmt);
    }
    return hay;
}

/** Resuelve el conflicto de un partido que tambien tiene cambios locales. */
static void sync_bajar_pendiente(const char *uuid, const cJSON *item,
                                 const SyncDirty *pendiente,
                                 SyncResultado *res)
{
    const char *remoto_updated = sync_json_texto(item, "updated_at_client");
    char local_updated[64] = {0};
    if (pendiente->creado_en[0])
    {
        sync_copiar(local_updated, sizeof(local_updated), pendiente->creado_en);
        sync_fecha_espacio_a_iso(local_updated);
    }
    else
    {
        cJSON *payload = sync_payload_local(uuid, 0, NULL, 0, NULL, 0);
        const cJSON *actualizado =
            payload ? cJSON_GetObjectItemCaseSensitive(payload,
                    "actualizado_en")
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
}

/** Aplica un registro remoto de la pagina descargada. */
static void sync_bajar_item(const cJSON *item, char *cursor, size_t cursor_tam,
                            SyncResultado *res)
{
    const char *uuid = sync_json_texto(item, "uuid");
    if (!uuid)
    {
        return;
    }
    const char *updated = sync_json_texto(item, "updated_at_client");
    if (updated && sync_comparar_fechas(updated, cursor) > 0)
    {
        sync_copiar(cursor, cursor_tam, updated);
    }

    SyncDirty pendiente;
    memset(&pendiente, 0, sizeof(pendiente));
    if (sync_buscar_pendiente(uuid, &pendiente))
    {
        sync_bajar_pendiente(uuid, item, &pendiente, res);
        return;
    }

    const cJSON *borrado = cJSON_GetObjectItemCaseSensitive(item, "deleted");
    if (cJSON_IsTrue(borrado))
    {
        res->eliminados += sync_eliminar_local(uuid);
        return;
    }
    res->importados += sync_aplicar_remoto(item, 0);
}

static int sync_bajar(const char *base, SyncConfig *cfg, SyncResultado *res)
{
    int pagina = 1;
    int total_paginas = 1;
    char cursor[128];
    sync_copiar(cursor, sizeof(cursor), cfg->cursor);

    do
    {
        cJSON *json = NULL;
        int estado = sync_bajar_pagina(base, cfg, cursor, pagina, &json);
        if (estado != 1)
        {
            return estado;
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
            sync_bajar_item(item, cursor, sizeof(cursor), res);
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
    if (!cfg || !servidores || !total)
    {
        return 0;
    }
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

/** Reintenta la subida una vez tras refrescar el token de sesion. */
static int sync_subir_con_refresco(const char *servidor, SyncConfig *cfg,
                                   SyncResultado *resultado)
{
    int subida = sync_subir(servidor, cfg, resultado);
    if (subida == -2 && sync_refrescar_token(cfg))
    {
        subida = sync_subir(servidor, cfg, resultado);
    }
    return subida;
}

/** Reintenta la bajada una vez tras refrescar el token de sesion. */
static int sync_bajar_con_refresco(const char *servidor, SyncConfig *cfg,
                                   SyncResultado *resultado)
{
    int bajada = sync_bajar(servidor, cfg, resultado);
    if (bajada == -2 && sync_refrescar_token(cfg))
    {
        bajada = sync_bajar(servidor, cfg, resultado);
    }
    return bajada;
}

/** Sube y baja las entidades adicionales; devuelve 0 si la sesion vencio. */
static int sync_entidades_servidor(const char *servidor, SyncConfig *cfg,
                                   SyncResultado *resultado)
{
    int entidades_bajada = sync_entidades_bajar(servidor, cfg, resultado);
    if (entidades_bajada == -2 && sync_refrescar_token(cfg))
    {
        entidades_bajada = sync_entidades_bajar(servidor, cfg, resultado);
    }
    if (entidades_bajada == -2)
    {
        return 0;
    }
    int entidades = sync_entidades_subir(servidor, cfg, resultado);
    if (entidades == -2 && sync_refrescar_token(cfg))
    {
        entidades = sync_entidades_subir(servidor, cfg, resultado);
    }
    return entidades == -2 ? 0 : 1;
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
        int subida = sync_subir_con_refresco(servidores[i], &cfg, resultado);
        if (subida > 0 &&
                sync_entidades_servidor(servidores[i], &cfg, resultado))
        {
            int bajada =
                sync_bajar_con_refresco(servidores[i], &cfg, resultado);
            if (bajada > 0)
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
        SyncAuthDestino destino =
        {
            .token = token,
            .token_tam = sizeof(token),
            .user_id = user_id,
            .user_tam = sizeof(user_id),
            .error = error,
            .error_tam = sizeof(error)
        };
        if (sync_http_auth(servidores[i], email, password, &destino))
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
