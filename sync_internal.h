#ifndef SYNC_INTERNAL_H
#define SYNC_INTERNAL_H

#include "cJSON.h"
#include "sqlite3.h"
#include "sync.h"

#include <stddef.h>

typedef struct
{
    int status;   /* Codigo HTTP (0 si no hubo respuesta) */
    char *body;   /* Cuerpo de la respuesta (puede ser NULL) */
    char *error;  /* Error de red/curl (puede ser NULL) */
} SyncHttp;

/** Destino de los datos que devuelve una autenticacion correcta. */
typedef struct
{
    char *token;      /* Buffer del token de sesion */
    size_t token_tam; /* Capacidad del buffer del token */
    char *user_id;    /* Buffer del id de usuario */
    size_t user_tam;  /* Capacidad del buffer del id */
    char *error;      /* Buffer del mensaje de error (opcional) */
    size_t error_tam; /* Capacidad del buffer de error */
} SyncAuthDestino;

/** Ejecuta una peticion HTTP con curl.exe. Devuelve 1 si hubo respuesta. */
int sync_http_request(const char *metodo, const char *url, const char *token,
                      const char *body, SyncHttp *salida);

void sync_http_free(SyncHttp *respuesta);

/** Autentica contra PocketBase y devuelve token + id de usuario. */
int sync_http_auth(const char *base, const char *email, const char *password,
                   SyncAuthDestino *destino);

/* Utilidades compartidas. */
int sync_columna_existe(sqlite3 *conexion, const char *tabla, const char *columna);
const char *sync_json_texto(const cJSON *obj, const char *clave);
int sync_json_copiar_texto(const cJSON *obj, const char *clave, char *salida,
                           size_t tam);
char *sync_json_imprimir(const cJSON *obj);
void sync_recortar(char *texto);
int sync_texto_seguro(const char *texto);
int sync_url_esquema_permitido(const char *url);
int sync_entero_desde_json(const cJSON *obj, const char *clave, int defecto);
double sync_double_desde_json(const cJSON *obj, const char *clave, double defecto);

#endif /* SYNC_INTERNAL_H */
