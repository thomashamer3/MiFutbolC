/**
 * @file undo.h
 * @brief Sistema de deshacer (undo) para MiFutbolC
 *
 * Proporciona un registro de operaciones que almacena las ultimas N
 * operaciones y permite deshacerlas restaurando snapshots JSON.
 * Cada operacion guarda el estado BEFORE y AFTER en formato JSON,
 * y al deshacer se restaura el snapshot BEFORE mediante SQL directo.
 */

#ifndef UNDO_H
#define UNDO_H

/**
 * @brief Maximo numero de operaciones en el historial de undo
 */
#define MAX_UNDO_HISTORY 50

/**
 * @brief Archivo donde se persiste el historial de undo
 */
#define UNDO_HISTORY_FILE "Exportaciones/undo_history.json"

/**
 * @brief Tipos de operacion que se pueden deshacer
 */
typedef enum
{
    UNDO_CREATE, /**< Operacion de creacion */
    UNDO_UPDATE, /**< Operacion de modificacion */
    UNDO_DELETE  /**< Operacion de eliminacion */
} UndoOperationType;

/**
 * @brief Entrada individual en el historial de undo
 *
 * Almacena metadatos de la operacion y dos snapshots JSON:
 * el estado de los datos antes y despues de la operacion.
 */
typedef struct
{
    int id;                         /**< Identificador unico de la entrada undo */
    UndoOperationType tipo;          /**< Tipo de operacion (CREATE/UPDATE/DELETE) */
    char tabla[64];                  /**< Nombre de la tabla afectada */
    int registro_id;                 /**< ID del registro afectado */
    char descripcion[256];           /**< Descripcion legible de la operacion */
    char snapshot_before[4096];      /**< Estado del registro ANTES de la operacion (JSON) */
    char snapshot_after[4096];       /**< Estado del registro DESPUES de la operacion (JSON) */
    char timestamp[32];              /**< Marca de tiempo de la operacion */
} UndoEntry;

/**
 * @brief Inicializa el sistema de undo
 *
 * Carga el historial desde el archivo JSON si existe.
 * Debe llamarse una vez al iniciar la aplicacion.
 */
void undo_init(void);

/**
 * @brief Toma un snapshot JSON del estado actual de un registro
 *
 * Consulta todas las columnas de la tabla para el ID dado y devuelve
 * un objeto JSON con los valores. Usa PRAGMA table_info para descubrir
 * las columnas dinamicamente.
 *
 * @param tabla Nombre de la tabla
 * @param id ID del registro
 * @return Cadena JSON asignada con cJSON_malloc, o NULL si falla.
 *         El llamante debe liberarla con cJSON_free().
 */
char *undo_tomar_snapshot(const char *tabla, int id);

/**
 * @brief Registra una operacion en el historial de undo
 *
 * Agrega una nueva entrada al historial. Si se excede MAX_UNDO_HISTORY,
 * se descarta la entrada mas antigua. Guarda automaticamente al archivo.
 *
 * @param tipo Tipo de operacion (UNDO_CREATE, UNDO_UPDATE, UNDO_DELETE)
 * @param tabla Nombre de la tabla afectada
 * @param registro_id ID del registro afectado
 * @param descripcion Descripcion legible de la operacion
 * @param snapshot_before Estado del registro antes de la operacion (JSON)
 * @param snapshot_after Estado del registro despues de la operacion (JSON)
 */
void undo_registrar(int tipo, const char *tabla, int registro_id,
                    const char *descripcion, const char *snapshot_before,
                    const char *snapshot_after);

/**
 * @brief Ejecuta la operacion de deshacer (undo) de la ultima operacion
 *
 * Muestra la ultima operacion registrada, solicita confirmacion al usuario,
 * restaura el snapshot BEFORE mediante SQL, y elimina la entrada del historial.
 *
 * @return 1 si se deshizo correctamente, 0 si se cancelo o fallo
 */
int undo_ejecutar(void);

/**
 * @brief Muestra el historial completo de operaciones disponibles para deshacer
 *
 * Lista todas las entradas con su indice, tipo, tabla, ID, descripcion
 * y timestamp.
 */
void undo_mostrar_historial(void);

/**
 * @brief Limpia todo el historial de undo
 * Elimina todas las entradas registradas y guarda el archivo vacio.
 * Solicita confirmacion antes de limpiar.
 */
void undo_limpiar(void);

/**
 * @brief Guarda el historial de undo en el archivo JSON
 *
 * Serializa todas las entradas como un array JSON y las escribe
 * en UNDO_HISTORY_FILE dentro del directorio de exportaciones.
 */
void undo_guardar(void);

/**
 * @brief Carga el historial de undo desde el archivo JSON
 *
 * Lee UNDO_HISTORY_FILE y pobla el arreglo interno de entradas.
 * Si el archivo no existe, el historial comienza vacio.
 */
void undo_cargar(void);

#endif
