/**
 * @file sync.h
 * @brief Sincronizacion de partidos entre MiFutbolC y PocketBase.
 *
 * MiFutbolC sigue siendo local-first: SQLite es la fuente de trabajo y
 * PocketBase solo recibe/envia partidos. La cola `sync_dirty` se alimenta de
 * triggers sobre la tabla `partido`, de modo que alta, modificacion, borrado e
 * importaciones quedan pendientes sin tocar cada modulo.
 */

#ifndef SYNC_H
#define SYNC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYNC_URL_SIZE 512
#define SYNC_TOKEN_SIZE 1024
#define SYNC_UUID_SIZE 64
#define SYNC_ERROR_SIZE 512

/** Configuracion persistida en la tabla `sync_config`. */
typedef struct
{
    char url_local[SYNC_URL_SIZE];
    char url_remota[SYNC_URL_SIZE];
    char email[256];
    char token[SYNC_TOKEN_SIZE];
    char user_id[128];
    char cursor[128];
    char servidor_activo[SYNC_URL_SIZE];
    char ultima_sync[64];
    int auto_sync;
} SyncConfig;

/** Operacion pendiente de la cola local. */
typedef struct
{
    int id;
    int partido_id;
    char uuid[SYNC_UUID_SIZE];
    char operacion[16];
    char estado[16];
    char ultimo_error[SYNC_ERROR_SIZE];
    char creado_en[64];
} SyncDirty;

/** Conflicto entre una version local y una remota. */
typedef struct
{
    int id;
    char uuid[SYNC_UUID_SIZE];
    char *local_json;
    char *remoto_json;
} SyncConflicto;

/** Resumen de una sincronizacion. */
typedef struct
{
    int enviados;
    int importados;
    int eliminados;
    int conflictos;
    int errores;
    char servidor[SYNC_URL_SIZE];
} SyncResultado;

/* ------------------------------------------------------------------ Init/BD */

/** Crea tablas, triggers y columnas necesarias. Idempotente. */
int sync_init_db(void);

/** Carga/guarda la configuracion. */
int sync_config_load(SyncConfig *cfg);
int sync_config_save(const SyncConfig *cfg);
int sync_config_set(const char *clave, const char *valor);
int sync_config_get(const char *clave, char *salida, size_t tam);

/* ------------------------------------------------------------------- Cola */

int sync_queue_next(SyncDirty *salida);
int sync_queue_mark_done(const char *uuid);
int sync_queue_mark_error(int id, const char *error);
int sync_queue_mark_conflict(const char *uuid);
int sync_queue_count_pending(void);
int sync_queue_count_conflicts(void);
int sync_queue_enqueue_all(void);

int sync_conflict_first(SyncConflicto *salida);
void sync_conflict_free(SyncConflicto *c);
int sync_conflict_resolve(int id, int conservar_local);

/* --------------------------------------------------------------- Operacion */

/** Sincroniza cola + cambios remotos usando la configuracion guardada. */
int sync_ejecutar(SyncResultado *resultado);

/** Marca todos los partidos locales como pendientes de subida. */
int sync_subir_todos(SyncResultado *resultado);

/** Menu interactivo del modulo (se registra en el menu principal). */
void menu_sincronizacion(void);

/* -------------------------------------------------------------- Utilidades */

/** Normaliza una URL (quita espacios y barras finales). Devuelve 1 si vale. */
int sync_normalizar_url(const char *entrada, char *salida, size_t tam);

/** Genera un UUID v4 en [salida]. */
void sync_uuid_generar(char *salida, size_t tam);

#ifdef __cplusplus
}
#endif

#endif /* SYNC_H */
