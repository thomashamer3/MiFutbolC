#include "sync_internal.h"

#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
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
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
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
    const char *base = getenv("TEMP");
    if (!base || !*base)
    {
        base = getenv("TMP");
    }
#ifdef _WIN32
    if (!base || !*base)
    {
        base = ".";
    }
    unsigned long pid = (unsigned long)_getpid();
#else
    if (!base || !*base)
    {
        base = "/tmp";
    }
    unsigned long pid = (unsigned long)getpid();
#endif
    char *ruta = (char *)malloc(1024);
    if (!ruta)
    {
        return NULL;
    }
    snprintf(ruta, 1024, "%s%cmifutbol_sync_%lu_%lu.json", base,
#ifdef _WIN32
             '\\',
#else
             '/',
#endif
             pid, (unsigned long)time(NULL) ^ (unsigned long)rand());

    FILE *f = fopen(ruta, "wb");
    if (!f)
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

static char *leer_popen(FILE *fp, int *codigo_salida)
{
    size_t capacidad = SYNC_BUF;
    size_t total = 0;
    char *datos = (char *)malloc(capacidad);
    if (!datos)
    {
        pclose(fp);
        return NULL;
    }
    size_t n;
    while ((n = fread(datos + total, 1, capacidad - total, fp)) > 0)
    {
        total += n;
        if (capacidad - total < 1024)
        {
            capacidad *= 2;
            char *tmp = (char *)realloc(datos, capacidad);
            if (!tmp)
            {
                free(datos);
                pclose(fp);
                return NULL;
            }
            datos = tmp;
        }
    }
    int rc = pclose(fp);
    if (codigo_salida)
    {
        *codigo_salida = rc;
    }
    if (total + 1 >= capacidad)
    {
        char *tmp = (char *)realloc(datos, total + 2);
        if (!tmp)
        {
            free(datos);
            return NULL;
        }
        datos = tmp;
    }
    datos[total] = '\0';
    return datos;
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

    char cmd[SYNC_BUF * 2];
    int usados = snprintf(cmd, sizeof(cmd),
                          "%s --silent --show-error --location --max-time 25 "
                          "--compressed -X %s -H \"Accept: application/json\" ",
                          SYNC_CURL, metodo);
    if (usados < 0 || (size_t)usados >= sizeof(cmd))
    {
        free(temporal);
        salida->error = strdup("Peticion HTTP demasiado larga");
        return 0;
    }
    if (token && *token)
    {
        usados += snprintf(cmd + usados, sizeof(cmd) - (size_t)usados,
                           "-H \"Authorization: %s\" ", token);
    }
    if (temporal)
    {
        usados += snprintf(cmd + usados, sizeof(cmd) - (size_t)usados,
                           "-H \"Content-Type: application/json\" "
                           "--data-binary \"@%s\" ",
                           temporal);
    }
    usados += snprintf(cmd + usados, sizeof(cmd) - (size_t)usados,
                       "-w \"\\n%s%%{http_code}\" \"%s\" 2>&1", SYNC_MARCA, url);
    if (usados < 0 || (size_t)usados >= sizeof(cmd))
    {
        if (temporal)
        {
            remove(temporal);
            free(temporal);
        }
        salida->error = strdup("Peticion HTTP demasiado larga");
        return 0;
    }

    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        if (temporal)
        {
            remove(temporal);
            free(temporal);
        }
        salida->error = strdup("No se pudo ejecutar curl");
        return 0;
    }
    int rc = 0;
    char *datos = leer_popen(fp, &rc);
    if (temporal)
    {
        remove(temporal);
        free(temporal);
    }
    if (!datos)
    {
        salida->error = strdup("No se pudo leer la respuesta HTTP");
        return 0;
    }

    char *marca = strstr(datos, SYNC_MARCA);
    if (marca)
    {
        salida->status = atoi(marca + strlen(SYNC_MARCA));
        *marca = '\0';
        size_t len = strlen(datos);
        while (len > 0 && (datos[len - 1] == '\n' || datos[len - 1] == '\r'))
        {
            datos[--len] = '\0';
        }
        salida->body = datos;
    }
    else
    {
        free(datos);
        salida->error = rc == 0 ? strdup("Respuesta HTTP invalida")
                        : strdup("No se pudo conectar al servidor");
        return 0;
    }

    if (salida->status == 0)
    {
        if (salida->body && *salida->body)
        {
            salida->error = strdup(salida->body);
        }
        else
        {
            salida->error = strdup("No se pudo conectar al servidor");
        }
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

int sync_http_auth(const char *base, const char *email, const char *password,
                   char *token, size_t token_tam, char *user_id, size_t user_tam,
                   char *error, size_t error_tam)
{
    if (!base || !email || !password || !token || !user_id)
    {
        return 0;
    }
    token[0] = '\0';
    user_id[0] = '\0';

    cJSON *cuerpo = cJSON_CreateObject();
    if (!cuerpo)
    {
        return 0;
    }
    cJSON_AddStringToObject(cuerpo, "identity", email);
    cJSON_AddStringToObject(cuerpo, "password", password);
    char *json = sync_json_imprimir(cuerpo);
    cJSON_Delete(cuerpo);
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
        if (error && error_tam)
        {
            snprintf(error, error_tam, "%s",
                     respuesta.error ? respuesta.error : "Error de red");
        }
        sync_http_free(&respuesta);
        return 0;
    }

    if (respuesta.status < 200 || respuesta.status >= 300)
    {
        if (error && error_tam)
        {
            if (respuesta.status == 400 || respuesta.status == 401)
            {
                snprintf(error, error_tam,
                         "Correo o contrasena incorrectos");
            }
            else
            {
                snprintf(error, error_tam,
                         "El servidor rechazo el inicio de sesion (%d)",
                         respuesta.status);
            }
        }
        sync_http_free(&respuesta);
        return 0;
    }

    cJSON *raiz = respuesta.body ? cJSON_Parse(respuesta.body) : NULL;
    if (!raiz)
    {
        if (error && error_tam)
        {
            snprintf(error, error_tam, "Respuesta de autenticacion invalida");
        }
        sync_http_free(&respuesta);
        return 0;
    }
    const char *tok = sync_json_texto(raiz, "token");
    const cJSON *record = cJSON_GetObjectItemCaseSensitive(raiz, "record");
    const char *uid = record ? sync_json_texto(record, "id") : NULL;
    if (!tok || !uid)
    {
        if (error && error_tam)
        {
            snprintf(error, error_tam, "El servidor no devolvio token/usuario");
        }
    }
    else
    {
        snprintf(token, token_tam, "%s", tok);
        snprintf(user_id, user_tam, "%s", uid);
        ok = 1;
    }
    cJSON_Delete(raiz);
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
