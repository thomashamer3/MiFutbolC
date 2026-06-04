/**
 * @file undo.c
 * @brief Implementacion del sistema de deshacer (undo) para MiFutbolC
 *
 * Almacena un historial circular de las ultimas N operaciones y permite
 * restaurar el estado anterior mediante snapshots JSON y SQL directo.
 * Usa PRAGMA table_info para descubrir columnas de cualquier tabla.
 */

#include "undo.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include "menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

/* ---------------------------------------------------------------
   Estado interno del modulo
   --------------------------------------------------------------- */

static UndoEntry undo_history[MAX_UNDO_HISTORY];
static int undo_count = 0;
static int undo_next_id = 1;

/* ---------------------------------------------------------------
   Helpers internos
   --------------------------------------------------------------- */

/**
 * @brief Construye la ruta completa al archivo de historial undo
 *
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void undo_obtener_ruta(char *buffer, size_t size)
{
    const char *export_dir = get_export_dir();
    if (export_dir)
    {
        snprintf(buffer, size, "%s/undo_history.json", export_dir);
    }
    else
    {
        strncpy_s(buffer, size, UNDO_HISTORY_FILE, _TRUNCATE);
    }
}

/**
 * @brief Asegura que el directorio de exportacion existe
 *
 * @return 1 si el directorio existe o se creo, 0 en caso de error
 */
static int undo_asegurar_directorio(void)
{
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        return 0;
    }

#ifdef _WIN32
    if (_mkdir(export_dir) != 0 && errno != EEXIST)
    {
        return 0;
    }
#else
    if (mkdir(export_dir, 0755) != 0 && errno != EEXIST)
    {
        return 0;
    }
#endif

    return 1;
}

/**
 * @brief Obtiene la fecha/hora actual como string
 *
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void undo_get_timestamp(char *buffer, size_t size)
{
    time_t t = time(NULL);
    struct tm tm_struct;

#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif

    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

/**
 * @brief Desplaza las entradas para eliminar el indice dado
 *
 * @param index Indice a eliminar (0-based)
 */
static void undo_eliminar_entrada(int index)
{
    if (index < 0 || index >= undo_count)
    {
        return;
    }

    for (int i = index; i < undo_count - 1; i++)
    {
        undo_history[i] = undo_history[i + 1];
    }

    undo_count--;
}

/**
 * @brief Convierte el tipo de operacion a string legible
 *
 * @param tipo Tipo de operacion
 * @return Cadena con el nombre del tipo
 */
static const char *undo_tipo_str(int tipo)
{
    switch (tipo)
    {
    case UNDO_CREATE:
        return "CREAR";
    case UNDO_UPDATE:
        return "MODIFICAR";
    case UNDO_DELETE:
        return "ELIMINAR";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Convierte el tipo de operacion a string en pasado
 *
 * @param tipo Tipo de operacion
 * @return Cadena con el nombre en pasado
 */
static const char *undo_tipo_pasado_str(int tipo)
{
    switch (tipo)
    {
    case UNDO_CREATE:
        return "Creacion";
    case UNDO_UPDATE:
        return "Modificacion";
    case UNDO_DELETE:
        return "Eliminacion";
    default:
        return "Desconocida";
    }
}

/* ---------------------------------------------------------------
   Restauracion generica desde snapshot JSON
   --------------------------------------------------------------- */

/**
 * @brief Restaura un registro desde un snapshot JSON
 *
 * Descubre las columnas via PRAGMA table_info y construye dinamicamente
 * la sentencia SQL (UPDATE o INSERT) con parametros vinculados.
 *
 * @param entry Entrada undo a restaurar
 * @return 1 si la restauracion fue exitosa, 0 en caso de error
 */
static int restaurar_desde_snapshot(const UndoEntry *entry)
{
    if (!entry || !entry->snapshot_before[0])
    {
        return 0;
    }

    cJSON *json = cJSON_Parse(entry->snapshot_before);
    if (!json)
    {
        printf("Error: No se pudo analizar el snapshot JSON.\n");
        return 0;
    }

    cJSON *id_item = cJSON_GetObjectItem(json, "id");
    int record_id = (id_item && cJSON_IsNumber(id_item)) ? (int)id_item->valuedouble : entry->registro_id;

    if (entry->tipo == UNDO_CREATE)
    {
        /* Reversa de CREATE: DELETE */
        char sql[256];
        snprintf(sql, sizeof(sql), "DELETE FROM \"%s\" WHERE id = ?", entry->tabla);

        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cJSON_Delete(json);
            return 0;
        }

        sqlite3_bind_int(stmt, 1, record_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        cJSON_Delete(json);

        if (rc != SQLITE_DONE)
        {
            printf("Error al deshacer creacion: %s\n", sqlite3_errmsg(db));
            return 0;
        }

        return 1;
    }

    /* Para UPDATE y DELETE (reversa de DELETE): restaurar valores */

    /* Obtener nombres de columnas via PRAGMA */
    char pragma_sql[128];
    snprintf(pragma_sql, sizeof(pragma_sql), "PRAGMA table_info(\"%s\")", entry->tabla);

    sqlite3_stmt *pragma_stmt = NULL;
    if (sqlite3_prepare_v2(db, pragma_sql, -1, &pragma_stmt, NULL) != SQLITE_OK)
    {
        cJSON_Delete(json);
        printf("Error: Tabla \"%s\" no encontrada.\n", entry->tabla);
        return 0;
    }

    char columnas[128][128];
    int num_cols = 0;

    while (sqlite3_step(pragma_stmt) == SQLITE_ROW && num_cols < 128)
    {
        const char *name = (const char *)sqlite3_column_text(pragma_stmt, 1);
        if (name)
        {
            strncpy_s(columnas[num_cols], sizeof(columnas[num_cols]), name, _TRUNCATE);
            num_cols++;
        }
    }
    sqlite3_finalize(pragma_stmt);

    if (num_cols == 0)
    {
        cJSON_Delete(json);
        printf("Error: No se encontraron columnas en la tabla \"%s\".\n", entry->tabla);
        return 0;
    }

    /* Determinar que columnas tienen valor en el JSON */
    int col_tiene_valor[128];
    int cols_con_datos = 0;

    for (int i = 0; i < num_cols; i++)
    {
        if (strcmp(columnas[i], "id") == 0)
        {
            col_tiene_valor[i] = 0;
        }
        else
        {
            cJSON *val = cJSON_GetObjectItem(json, columnas[i]);
            col_tiene_valor[i] = (val != NULL) ? 1 : 0;
            if (col_tiene_valor[i])
            {
                cols_con_datos++;
            }
        }
    }

    if (cols_con_datos == 0)
    {
        cJSON_Delete(json);
        printf("Error: No hay datos en el snapshot para restaurar.\n");
        return 0;
    }

    /* Construir SQL */
    char sql[8192];
    int es_insert = (entry->tipo == UNDO_DELETE);

    if (es_insert)
    {
        /* INSERT INTO tabla (id, col1, col2, ...) VALUES (?, ?, ?, ...) */
        snprintf(sql, sizeof(sql), "INSERT INTO \"%s\" (\"id\"", entry->tabla);
        for (int i = 0; i < num_cols; i++)
        {
            if (col_tiene_valor[i])
            {
                char col_part[256];
                snprintf(col_part, sizeof(col_part), ", \"%.*s\"", (int)(sizeof(col_part) - 8), columnas[i]);
                strcat_s(sql, sizeof(sql), col_part);
            }
        }
        strcat_s(sql, sizeof(sql), ") VALUES (?");
        for (int i = 0; i < num_cols; i++)
        {
            if (col_tiene_valor[i])
            {
                strcat_s(sql, sizeof(sql), ", ?");
            }
        }
        strcat_s(sql, sizeof(sql), ")");
    }
    else
    {
        /* UPDATE tabla SET col1 = ?, col2 = ?, ... WHERE id = ? */
        snprintf(sql, sizeof(sql), "UPDATE \"%s\" SET ", entry->tabla);
        int primero = 1;
        for (int i = 0; i < num_cols; i++)
        {
            if (col_tiene_valor[i])
            {
                if (!primero)
                {
                    strcat_s(sql, sizeof(sql), ", ");
                }
                char set_part[256];
                snprintf(set_part, sizeof(set_part), "\"%.*s\" = ?", (int)(sizeof(set_part) - 10), columnas[i]);
                strcat_s(sql, sizeof(sql), set_part);
                primero = 0;
            }
        }
        strcat_s(sql, sizeof(sql), " WHERE \"id\" = ?");
    }

    /* Preparar statement */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error preparando SQL: %s\nSQL: %s\n", sqlite3_errmsg(db), sql);
        cJSON_Delete(json);
        return 0;
    }

    /* Vincular valores */
    int bind_idx = 1;

    if (es_insert)
    {
        sqlite3_bind_int(stmt, bind_idx++, record_id);
    }

    for (int i = 0; i < num_cols; i++)
    {
        if (!col_tiene_valor[i])
        {
            continue;
        }

        cJSON *val = cJSON_GetObjectItem(json, columnas[i]);
        if (!val)
        {
            continue;
        }

        if (cJSON_IsNull(val))
        {
            sqlite3_bind_null(stmt, bind_idx++);
        }
        else if (cJSON_IsTrue(val))
        {
            sqlite3_bind_int(stmt, bind_idx++, 1);
        }
        else if (cJSON_IsFalse(val))
        {
            sqlite3_bind_int(stmt, bind_idx++, 0);
        }
        else if (cJSON_IsNumber(val))
        {
            double dval = cJSON_GetNumberValue(val);
            if (dval == (int)dval)
            {
                sqlite3_bind_int(stmt, bind_idx++, (int)dval);
            }
            else
            {
                sqlite3_bind_double(stmt, bind_idx++, dval);
            }
        }
        else if (cJSON_IsString(val))
        {
            sqlite3_bind_text(stmt, bind_idx++, val->valuestring, -1, SQLITE_TRANSIENT);
        }
        else
        {
            char *printed = cJSON_Print(val);
            sqlite3_bind_text(stmt, bind_idx++, printed, -1, SQLITE_TRANSIENT);
            cJSON_free(printed);
        }
    }

    if (!es_insert)
    {
        sqlite3_bind_int(stmt, bind_idx, record_id);
    }

    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE)
    {
        printf("Error al restaurar snapshot: %s\n", sqlite3_errmsg(db));
        printf("SQL: %s\n", sql);
        sqlite3_finalize(stmt);
        cJSON_Delete(json);
        return 0;
    }

    sqlite3_finalize(stmt);
    cJSON_Delete(json);
    return 1;
}

/* ---------------------------------------------------------------
   Funciones publicas
   --------------------------------------------------------------- */

char *undo_tomar_snapshot(const char *tabla, int id)
{
    if (!tabla || id <= 0 || !db)
    {
        return NULL;
    }

    /* Descubrir columnas via PRAGMA table_info */
    char pragma_sql[128];
    snprintf(pragma_sql, sizeof(pragma_sql), "PRAGMA table_info(\"%s\")", tabla);

    sqlite3_stmt *pragma_stmt = NULL;
    if (sqlite3_prepare_v2(db, pragma_sql, -1, &pragma_stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }

    char columnas[128][128];
    int num_cols = 0;

    while (sqlite3_step(pragma_stmt) == SQLITE_ROW && num_cols < 128)
    {
        const char *name = (const char *)sqlite3_column_text(pragma_stmt, 1);
        if (name)
        {
            strncpy_s(columnas[num_cols], sizeof(columnas[num_cols]), name, _TRUNCATE);
            num_cols++;
        }
    }
    sqlite3_finalize(pragma_stmt);

    if (num_cols == 0)
    {
        return NULL;
    }

    /* Construir SELECT con columnas escapadas */
    char select_sql[8192];
    snprintf(select_sql, sizeof(select_sql), "SELECT \"%s\"", columnas[0]);
    for (int i = 1; i < num_cols; i++)
    {
        char col_part[256];
        snprintf(col_part, sizeof(col_part), ", \"%s\"", columnas[i]);
        strcat_s(select_sql, sizeof(select_sql), col_part);
    }
    {
        char from_part[256];
        snprintf(from_part, sizeof(from_part), " FROM \"%s\" WHERE \"id\" = ?", tabla);
        strcat_s(select_sql, sizeof(select_sql), from_part);
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }

    /* Construir objeto JSON */
    cJSON *json = cJSON_CreateObject();

    for (int i = 0; i < num_cols; i++)
    {
        int type = sqlite3_column_type(stmt, i);

        switch (type)
        {
        case SQLITE_NULL:
            cJSON_AddNullToObject(json, columnas[i]);
            break;

        case SQLITE_INTEGER:
            cJSON_AddNumberToObject(json, columnas[i], sqlite3_column_int64(stmt, i));
            break;

        case SQLITE_FLOAT:
            cJSON_AddNumberToObject(json, columnas[i], sqlite3_column_double(stmt, i));
            break;

        case SQLITE_TEXT:
        default:
        {
            const char *text = (const char *)sqlite3_column_text(stmt, i);
            cJSON_AddStringToObject(json, columnas[i], text ? text : "");
            break;
        }
        }
    }

    sqlite3_finalize(stmt);

    char *json_str = cJSON_Print(json);
    cJSON_Delete(json);
    return json_str;
}

void undo_init(void)
{
    undo_count = 0;
    undo_next_id = 1;
    undo_cargar();
}

void undo_registrar(int tipo, const char *tabla, int registro_id,
                    const char *descripcion, const char *snapshot_before,
                    const char *snapshot_after)
{
    if (!tabla || registro_id <= 0)
    {
        return;
    }

    /* Si el historial esta lleno, descartar el mas antiguo */
    if (undo_count >= MAX_UNDO_HISTORY)
    {
        undo_eliminar_entrada(0);
    }

    UndoEntry *entry = &undo_history[undo_count];
    memset(entry, 0, sizeof(UndoEntry));

    entry->id = undo_next_id++;
    entry->tipo = (UndoOperationType)tipo;
    entry->registro_id = registro_id;

    strncpy_s(entry->tabla, sizeof(entry->tabla), tabla, _TRUNCATE);

    if (descripcion)
    {
        strncpy_s(entry->descripcion, sizeof(entry->descripcion), descripcion, _TRUNCATE);
    }

    if (snapshot_before)
    {
        strncpy_s(entry->snapshot_before, sizeof(entry->snapshot_before), snapshot_before, _TRUNCATE);
    }

    if (snapshot_after)
    {
        strncpy_s(entry->snapshot_after, sizeof(entry->snapshot_after), snapshot_after, _TRUNCATE);
    }

    undo_get_timestamp(entry->timestamp, sizeof(entry->timestamp));

    undo_count++;
    undo_guardar();
}

int undo_ejecutar(void)
{
    if (undo_count == 0)
    {
        printf("No hay operaciones para deshacer.\n");
        pause_console();
        return 0;
    }

    /* La ultima operacion es el ultimo indice */
    int index = undo_count - 1;
    UndoEntry *entry = &undo_history[index];

    mostrar_pantalla("DESHACER OPERACION");

    printf("  Operacion: %s\n", undo_tipo_pasado_str(entry->tipo));
    printf("  Tabla: %s\n", entry->tabla);
    printf("  ID: %d\n", entry->registro_id);
    printf("  Descripcion: %s\n", entry->descripcion);
    printf("  Fecha: %s\n", entry->timestamp);

    if (!confirmar("  Desea deshacer esta operacion"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return 0;
    }

    if (!restaurar_desde_snapshot(entry))
    {
        printf("Error: No se pudo deshacer la operacion.\n");

        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Undo fallido para %s ID %d en tabla %s",
                 undo_tipo_str(entry->tipo), entry->registro_id, entry->tabla);
        app_log_event("UNDO", log_msg);

        pause_console();
        return 0;
    }

    printf("Operacion deshecha exitosamente: %s de %s ID %d.\n",
           undo_tipo_pasado_str(entry->tipo), entry->tabla, entry->registro_id);

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Undo exitoso: %s de %s ID %d",
             undo_tipo_str(entry->tipo), entry->tabla, entry->registro_id);
    app_log_event("UNDO", log_msg);

    undo_eliminar_entrada(index);
    undo_guardar();

    pause_console();
    return 1;
}

void undo_mostrar_historial(void)
{
    if (undo_count == 0)
    {
        mostrar_no_hay_registros("operaciones para deshacer");
        pause_console();
        return;
    }

    mostrar_pantalla("HISTORIAL DE OPERACIONES (UNDO)");

    for (int i = undo_count - 1; i >= 0; i--)
    {
        UndoEntry *entry = &undo_history[i];
        int num = undo_count - i;

        printf("  %d.\n", num);
        printf("     Tipo: %s\n", undo_tipo_str(entry->tipo));
        printf("     Tabla: %s\n", entry->tabla);
        printf("     ID: %d\n", entry->registro_id);
        printf("     Descripcion: %s\n", entry->descripcion);
        printf("     Fecha: %s\n", entry->timestamp);
        printf("     ----------------------------------------\n");
    }

    printf("\nTotal: %d operacion(es) disponible(s) para deshacer.\n", undo_count);
    printf("Use la opcion 'Deshacer' para revertir la ultima operacion.\n");
    pause_console();
}

void undo_limpiar(void)
{
    if (undo_count == 0)
    {
        printf("El historial ya esta vacio.\n");
        pause_console();
        return;
    }

    if (!confirmar("  Limpiar todo el historial de undo"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    undo_count = 0;
    undo_guardar();
    printf("Historial de undo limpiado exitosamente.\n");
    app_log_event("UNDO", "Historial de undo limpiado");
    pause_console();
}

void undo_guardar(void)
{
    if (!undo_asegurar_directorio())
    {
        return;
    }

    char filepath[1024];
    undo_obtener_ruta(filepath, sizeof(filepath));

    cJSON *array = cJSON_CreateArray();

    for (int i = 0; i < undo_count; i++)
    {
        UndoEntry *entry = &undo_history[i];

        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "id", entry->id);
        cJSON_AddNumberToObject(obj, "tipo", (int)entry->tipo);
        cJSON_AddStringToObject(obj, "tabla", entry->tabla);
        cJSON_AddNumberToObject(obj, "registro_id", entry->registro_id);
        cJSON_AddStringToObject(obj, "descripcion", entry->descripcion);
        cJSON_AddStringToObject(obj, "snapshot_before", entry->snapshot_before);
        cJSON_AddStringToObject(obj, "snapshot_after", entry->snapshot_after);
        cJSON_AddStringToObject(obj, "timestamp", entry->timestamp);
        cJSON_AddItemToArray(array, obj);
    }

    char *json_str = cJSON_Print(array);
    cJSON_Delete(array);

    if (!json_str)
    {
        return;
    }

    FILE *f = NULL;
    if (fopen_s(&f, filepath, "w") != 0 || !f)
    {
        cJSON_free(json_str);
        return;
    }

    fprintf(f, "%s\n", json_str);
    fclose(f);
    cJSON_free(json_str);
}

void undo_cargar(void)
{
    char filepath[1024];
    undo_obtener_ruta(filepath, sizeof(filepath));

    FILE *f = NULL;
    if (fopen_s(&f, filepath, "rb") != 0 || !f)
    {
        /* El archivo no existe: historial vacio */
        undo_count = 0;
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0)
    {
        fclose(f);
        undo_count = 0;
        return;
    }

    char *content = (char *)malloc((size_t)len + 1);
    if (!content)
    {
        fclose(f);
        undo_count = 0;
        return;
    }

    size_t read_len = fread(content, 1, (size_t)len, f);
    fclose(f);
    content[read_len] = '\0';

    cJSON *array = cJSON_Parse(content);
    free(content);

    if (!array || !cJSON_IsArray(array))
    {
        if (array)
        {
            cJSON_Delete(array);
        }
        undo_count = 0;
        return;
    }

    undo_count = 0;
    undo_next_id = 1;

    int count = cJSON_GetArraySize(array);
    for (int i = 0; i < count && undo_count < MAX_UNDO_HISTORY; i++)
    {
        cJSON *obj = cJSON_GetArrayItem(array, i);
        if (!obj)
        {
            continue;
        }

        UndoEntry *entry = &undo_history[undo_count];
        memset(entry, 0, sizeof(UndoEntry));

        cJSON *id_val = cJSON_GetObjectItem(obj, "id");
        entry->id = (id_val && cJSON_IsNumber(id_val)) ? (int)id_val->valuedouble : undo_next_id;

        cJSON *tipo_val = cJSON_GetObjectItem(obj, "tipo");
        entry->tipo = (tipo_val && cJSON_IsNumber(tipo_val)) ?
                      (UndoOperationType)(int)tipo_val->valuedouble : UNDO_CREATE;

        cJSON *tabla_val = cJSON_GetObjectItem(obj, "tabla");
        if (tabla_val && cJSON_IsString(tabla_val))
        {
            strncpy_s(entry->tabla, sizeof(entry->tabla), tabla_val->valuestring, _TRUNCATE);
        }

        cJSON *rid_val = cJSON_GetObjectItem(obj, "registro_id");
        entry->registro_id = (rid_val && cJSON_IsNumber(rid_val)) ? (int)rid_val->valuedouble : 0;

        cJSON *desc_val = cJSON_GetObjectItem(obj, "descripcion");
        if (desc_val && cJSON_IsString(desc_val))
        {
            strncpy_s(entry->descripcion, sizeof(entry->descripcion), desc_val->valuestring, _TRUNCATE);
        }

        cJSON *sb_val = cJSON_GetObjectItem(obj, "snapshot_before");
        if (sb_val && cJSON_IsString(sb_val))
        {
            strncpy_s(entry->snapshot_before, sizeof(entry->snapshot_before), sb_val->valuestring, _TRUNCATE);
        }

        cJSON *sa_val = cJSON_GetObjectItem(obj, "snapshot_after");
        if (sa_val && cJSON_IsString(sa_val))
        {
            strncpy_s(entry->snapshot_after, sizeof(entry->snapshot_after), sa_val->valuestring, _TRUNCATE);
        }

        cJSON *ts_val = cJSON_GetObjectItem(obj, "timestamp");
        if (ts_val && cJSON_IsString(ts_val))
        {
            strncpy_s(entry->timestamp, sizeof(entry->timestamp), ts_val->valuestring, _TRUNCATE);
        }

        if (entry->id >= undo_next_id)
        {
            undo_next_id = entry->id + 1;
        }

        undo_count++;
    }

    cJSON_Delete(array);
}
