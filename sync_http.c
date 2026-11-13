#include "sync_internal.h"

#include "compat_port.h"
#include "random_utils.h"
#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#include <Windows.h>
#define SYNC_CURL "curl.exe"
#else
#include <unistd.h>
#define SYNC_CURL "curl"
#endif

#define SYNC_BUF 8192
#define SYNC_MARCA "__MIFUTBOL_STATUS__:"

static int caracteres_seguros_url(const char *url)
{
    if (!url || !*url)
    {
        return 0;
    }
    if (!sync_url_esquema_permitido(url))
    {
        return 0;
    }
    for (const char *p = url; *p; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7F)
        {
            return 0;
        }
        /* Se descartan caracteres que rompen la linea de comandos de cmd. */
        if (c == '"' || c == '\\')
        {
            return 0;
        }
    }
    return 1;
}

static int token_seguro(const char *token)
{
    if (!token || !*token)
    {
        return 0;
    }
    for (const char *p = token; *p; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (!(isalnum(c) || c == '.' || c == '-' || c == '_' || c == '~' ||
                c == '+' || c == '/' || c == '='))
        {
            return 0;
        }
    }
    return 1;
}

static char *escribir_temporal(const char *contenido)
{
    char base[1024] = {0};
#ifdef _WIN32
    if (GetTempPathA((DWORD)sizeof(base), base) == 0)
    {
        base[0] = '\0';
    }
    unsigned long pid = (unsigned long)_getpid();
    const char separador = '\\';
#else
    snprintf(base, sizeof(base), "%s", P_tmpdir);
    unsigned long pid = (unsigned long)getpid();
    const char separador = '/';
#endif
    if (!base[0])
    {
        base[0] = '.';
        base[1] = '\0';
    }
    size_t capacidad = sizeof(base) + 128;
    char *ruta = (char *)malloc(capacidad);
    if (!ruta)
    {
        return NULL;
    }
    unsigned int nonce = (unsigned int)time(NULL);
    unsigned char aleatorio[4];
    if (secure_random_bytes(aleatorio, sizeof(aleatorio)) == 0)
    {
        nonce = ((unsigned int)aleatorio[0] << 24) |
                ((unsigned int)aleatorio[1] << 16) |
                ((unsigned int)aleatorio[2] << 8) | (unsigned int)aleatorio[3];
    }
    snprintf(ruta, capacidad, "%s%cmifutbol_sync_%lu_%u.json", base, separador,
             pid, nonce);

    FILE *f = NULL;
    if (fopen_s(&f, ruta, "wb") != 0 || !f)
    {
        free(ruta);
        return NULL;
    }
    if (contenido && *contenido)
    {
        fwrite(contenido, 1, strlen(contenido), f);
    }
    fclose(f);
    return ruta;
}

static void liberar_temporal(char *temporal)
{
    if (temporal)
    {
        remove(temporal);
        free(temporal);
    }
}

static int parsear_respuesta_http(char *datos, int rc, SyncHttp *salida)
{
    char *marca = strstr(datos, SYNC_MARCA);
    if (!marca)
    {
        salida->error = rc == 0 ? strdup("Respuesta HTTP invalida")
                        : strdup("No se pudo conectar al servidor");
        return 0;
    }
    salida->status = atoi(marca + strlen(SYNC_MARCA));
    *marca = '\0';
    size_t len = strlen(datos);
    while (len > 0 && (datos[len - 1] == '\n' || datos[len - 1] == '\r'))
    {
        datos[--len] = '\0';
    }
    salida->body = datos;
    if (salida->status == 0)
    {
        salida->error = (salida->body && *salida->body)
                        ? strdup(salida->body)
                        : strdup("No se pudo conectar al servidor");
    }
    return 1;
}

int sync_http_request(const char *metodo, const char *url, const char *token,
                      const char *body, SyncHttp *salida)
{
    if (!salida)
    {
        return 0;
    }
    memset(salida, 0, sizeof(*salida));
    if (!metodo || !url || !caracteres_seguros_url(url))
    {
        salida->error = strdup("URL de servidor invalida");
        return 0;
    }
    if (token && *token && !token_seguro(token))
    {
        salida->error = strdup("Token de sesion invalido");
        return 0;
    }

    char *temporal = NULL;
    if (body && *body)
    {
        temporal = escribir_temporal(body);
        if (!temporal)
        {
            salida->error = strdup("No se pudo preparar el cuerpo HTTP");
            return 0;
        }
    }

    char encabezado_auth[SYNC_BUF];
    char argumento_datos[SYNC_BUF];
    char formato_salida[64];
    const char *argv[24];
    int argc = 0;

    argv[argc++] = SYNC_CURL;
    argv[argc++] = "--silent";
    argv[argc++] = "--show-error";
    argv[argc++] = "--location";
    argv[argc++] = "--max-time";
    argv[argc++] = "25";
    argv[argc++] = "--compressed";
    argv[argc++] = "-X";
    argv[argc++] = metodo;
    argv[argc++] = "-H";
    argv[argc++] = "Accept: application/json";
    if (token && *token)
    {
        snprintf(encabezado_auth, sizeof(encabezado_auth), "Authorization: %s", token);
        argv[argc++] = "-H";
        argv[argc++] = encabezado_auth;
    }
    if (temporal)
    {
        snprintf(argumento_datos, sizeof(argumento_datos), "@%s", temporal);
        argv[argc++] = "-H";
        argv[argc++] = "Content-Type: application/json";
        argv[argc++] = "--data-binary";
        argv[argc++] = argumento_datos;
    }
    snprintf(formato_salida, sizeof(formato_salida), "\n%s%%{http_code}", SYNC_MARCA);
    argv[argc++] = "-w";
    argv[argc++] = formato_salida;
    argv[argc++] = url;
    argv[argc] = NULL;

    char *datos = NULL;
    int rc = 0;
    if (!app_executar_captura(argv, &datos, &rc))
    {
        liberar_temporal(temporal);
        salida->error = strdup("No se pudo ejecutar curl");
        return 0;
    }
    liberar_temporal(temporal);
    if (!parsear_respuesta_http(datos, rc, salida))
    {
        free(datos);
        return 0;
    }
    return 1;
}

void sync_http_free(SyncHttp *respuesta)
{
    if (!respuesta)
    {
        return;
    }
    free(respuesta->body);
    free(respuesta->error);
    respuesta->body = NULL;
    respuesta->error = NULL;
}

static void auth_error(SyncAuthDestino *destino, const char *mensaje)
{
    if (destino && destino->error && destino->error_tam)
    {
        snprintf(destino->error, destino->error_tam, "%s", mensaje);
    }
}

static void auth_error_status(SyncAuthDestino *destino, int status)
{
    if (!destino || !destino->error || !destino->error_tam)
    {
        return;
    }
    if (status == 400 || status == 401)
    {
        snprintf(destino->error, destino->error_tam,
                 "Correo o contrasena incorrectos");
    }
    else
    {
        snprintf(destino->error, destino->error_tam,
                 "El servidor rechazo el inicio de sesion (%d)", status);
    }
}

static char *auth_cuerpo_json(const char *email, const char *password)
{
    cJSON *cuerpo = cJSON_CreateObject();
    if (!cuerpo)
    {
        return NULL;
    }
    cJSON_AddStringToObject(cuerpo, "identity", email);
    cJSON_AddStringToObject(cuerpo, "password", password);
    char *json = sync_json_imprimir(cuerpo);
    cJSON_Delete(cuerpo);
    return json;
}

static int auth_extraer_campos(const char *body, SyncAuthDestino *destino)
{
    cJSON *raiz = body ? cJSON_Parse(body) : NULL;
    if (!raiz)
    {
        auth_error(destino, "Respuesta de autenticacion invalida");
        return 0;
    }
    const char *tok = sync_json_texto(raiz, "token");
    const cJSON *record = cJSON_GetObjectItemCaseSensitive(raiz, "record");
    const char *uid = record ? sync_json_texto(record, "id") : NULL;
    int ok = 0;
    if (!tok || !uid)
    {
        auth_error(destino, "El servidor no devolvio token/usuario");
    }
    else
    {
        snprintf(destino->token, destino->token_tam, "%s", tok);
        snprintf(destino->user_id, destino->user_tam, "%s", uid);
        ok = 1;
    }
    cJSON_Delete(raiz);
    return ok;
}

int sync_http_auth(const char *base, const char *email, const char *password,
                   SyncAuthDestino *destino)
{
    if (!base || !email || !password || !destino || !destino->token ||
            !destino->user_id)
    {
        return 0;
    }
    destino->token[0] = '\0';
    destino->user_id[0] = '\0';

    char *json = auth_cuerpo_json(email, password);
    if (!json)
    {
        return 0;
    }

    char url[SYNC_URL_SIZE + 128];
    snprintf(url, sizeof(url), "%s/api/collections/users/auth-with-password",
             base);
    SyncHttp respuesta;
    int ok = sync_http_request("POST", url, NULL, json, &respuesta);
    free(json);
    if (!ok)
    {
        auth_error(destino, respuesta.error ? respuesta.error : "Error de red");
        sync_http_free(&respuesta);
        return 0;
    }

    if (respuesta.status < 200 || respuesta.status >= 300)
    {
        auth_error_status(destino, respuesta.status);
        sync_http_free(&respuesta);
        return 0;
    }

    ok = auth_extraer_campos(respuesta.body, destino);
    sync_http_free(&respuesta);
    return ok;
}

const char *sync_json_texto(const cJSON *obj, const char *clave)
{
    if (!obj || !clave)
    {
        return NULL;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, clave);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

int sync_json_copiar_texto(const cJSON *obj, const char *clave, char *salida,
                           size_t tam)
{
    if (!salida || tam == 0)
    {
        return 0;
    }
    salida[0] = '\0';
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, clave);
    if (!item)
    {
        return 0;
    }
    if (cJSON_IsString(item) && item->valuestring)
    {
        snprintf(salida, tam, "%s", item->valuestring);
        return 1;
    }
    if (cJSON_IsNumber(item))
    {
        snprintf(salida, tam, "%g", item->valuedouble);
        return 1;
    }
    if (cJSON_IsBool(item))
    {
        snprintf(salida, tam, "%s", cJSON_IsTrue(item) ? "1" : "0");
        return 1;
    }
    return 0;
}

char *sync_json_imprimir(const cJSON *obj)
{
    return obj ? cJSON_PrintUnformatted(obj) : NULL;
}

int sync_entero_desde_json(const cJSON *obj, const char *clave, int defecto)
{
    const cJSON *item = obj ? cJSON_GetObjectItemCaseSensitive(obj, clave) : NULL;
    if (!item)
    {
        return defecto;
    }
    if (cJSON_IsNumber(item))
    {
        return (int)item->valuedouble;
    }
    if (cJSON_IsString(item) && item->valuestring)
    {
        return atoi(item->valuestring);
    }
    if (cJSON_IsBool(item))
    {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return defecto;
}

double sync_double_desde_json(const cJSON *obj, const char *clave, double defecto)
{
    const cJSON *item = obj ? cJSON_GetObjectItemCaseSensitive(obj, clave) : NULL;
    if (!item)
    {
        return defecto;
    }
    if (cJSON_IsNumber(item))
    {
        return item->valuedouble;
    }
    if (cJSON_IsString(item) && item->valuestring)
    {
        return atof(item->valuestring);
    }
    return defecto;
}

void sync_recortar(char *texto)
{
    if (!texto)
    {
        return;
    }
    size_t len = strlen(texto);
    while (len > 0 && (texto[len - 1] == '\n' || texto[len - 1] == '\r' ||
                       texto[len - 1] == ' '))
    {
        texto[--len] = '\0';
    }
}

int sync_columna_existe(sqlite3 *conexion, const char *tabla, const char *columna)
{
    if (!conexion || !tabla || !columna)
    {
        return 0;
    }
    char sql[256];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", tabla);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conexion, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    int existe = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *nombre = sqlite3_column_text(stmt, 1);
        if (nombre && strcmp((const char *)nombre, columna) == 0)
        {
            existe = 1;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return existe;
}
