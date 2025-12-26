/**
 * @file import.c
 * @brief Módulo para importar datos desde archivos JSON a la base de datos.
 *
 * Este archivo contiene las funciones necesarias para leer archivos JSON
 * y insertar los datos en las tablas correspondientes de la base de datos SQLite.
 */

#include "import.h"
#include "cJSON.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Lee el contenido completo de un archivo de texto.
 *
 * @param filename Ruta del archivo a leer.
 * @return Puntero al contenido del archivo o NULL si hay error.
 */
static char *read_file_content(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *)malloc(length + 1);
    if (!content)
    {
        printf("Error: No se pudo asignar memoria\n");
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);
    return content;
}

/**
 * @brief Solicita al usuario la ruta de un archivo.
 *
 * @param prompt Mensaje a mostrar al usuario.
 * @param buffer Buffer donde almacenar la ruta.
 * @param buffer_size Tamaño del buffer.
 */
static void solicitar_archivo(const char *prompt, char *buffer, size_t buffer_size)
{
    printf("%s: ", prompt);
    fgets(buffer, buffer_size, stdin);
    buffer[strcspn(buffer, "\n")] = 0; // Remover newline
}

/**
 * @brief Importa camisetas desde archivo JSON.
 *
 * Lee el archivo JSON de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_json()
{
    char filename[256];
    solicitar_archivo("Ingrese la ruta del archivo JSON de camisetas", filename, sizeof(filename));

    if (strlen(filename) == 0)
    {
        printf("Operación cancelada.\n");
        return;
    }

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de camisetas inválido\n");
        return;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON de camisetas debe ser un array\n");
        cJSON_Delete(json);
        return;
    }

    int count = cJSON_GetArraySize(json);
    printf("Importando %d camisetas...\n", count);

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON *id_json = cJSON_GetObjectItem(item, "id");
        cJSON *nombre_json = cJSON_GetObjectItem(item, "nombre");

        if (!cJSON_IsNumber(id_json) || !cJSON_IsString(nombre_json))
            continue;

        int id = id_json->valueint;
        const char *nombre = nombre_json->valuestring;

        // Verificar si ya existe
        sqlite3_stmt *check_stmt;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta WHERE id = ?", -1, &check_stmt, NULL);
        sqlite3_bind_int(check_stmt, 1, id);
        sqlite3_step(check_stmt);
        int exists = sqlite3_column_int(check_stmt, 0);
        sqlite3_finalize(check_stmt);

        if (exists)
        {
            printf("Camiseta ID %d ya existe, omitiendo...\n", id);
            continue;
        }

        // Insertar
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "INSERT INTO camiseta(id, nombre) VALUES(?, ?)", -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Camiseta '%s' importada correctamente\n", nombre);
    }

    cJSON_Delete(json);
    printf("Importación de camisetas completada\n");
}

/**
 * @brief Importa partidos desde archivo JSON.
 *
 * Lee el archivo JSON de partidos y los inserta en la base de datos.
 */
void importar_partidos_json()
{
    char filename[256];
    solicitar_archivo("Ingrese la ruta del archivo JSON de partidos", filename, sizeof(filename));

    if (strlen(filename) == 0)
    {
        printf("Operación cancelada.\n");
        return;
    }

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de partidos inválido\n");
        return;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON de partidos debe ser un array\n");
        cJSON_Delete(json);
        return;
    }

    int count = cJSON_GetArraySize(json);
    printf("Importando %d partidos...\n", count);

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON *cancha_json = cJSON_GetObjectItem(item, "cancha");
        cJSON *fecha_json = cJSON_GetObjectItem(item, "fecha");
        cJSON *goles_json = cJSON_GetObjectItem(item, "goles");
        cJSON *asistencias_json = cJSON_GetObjectItem(item, "asistencias");
        cJSON *camiseta_json = cJSON_GetObjectItem(item, "camiseta");
        cJSON *resultado_json = cJSON_GetObjectItem(item, "resultado");
        cJSON *clima_json = cJSON_GetObjectItem(item, "clima");
        cJSON *dia_json = cJSON_GetObjectItem(item, "dia");
        cJSON *rendimiento_general_json = cJSON_GetObjectItem(item, "rendimiento_general");
        cJSON *cansancio_json = cJSON_GetObjectItem(item, "cansancio");
        cJSON *estado_animo_json = cJSON_GetObjectItem(item, "estado_animo");
        cJSON *comentario_personal_json = cJSON_GetObjectItem(item, "comentario_personal");

        if (!cJSON_IsString(cancha_json) || !cJSON_IsString(fecha_json) ||
                !cJSON_IsNumber(goles_json) || !cJSON_IsNumber(asistencias_json) ||
                !cJSON_IsString(camiseta_json))
            continue;

        const char *cancha_nombre = cancha_json->valuestring;
        const char *fecha = fecha_json->valuestring;
        int goles = goles_json->valueint;
        int asistencias = asistencias_json->valueint;
        const char *camiseta_nombre = camiseta_json->valuestring;
        int resultado = resultado_json ? resultado_json->valueint : 0;
        int clima = clima_json ? clima_json->valueint : 0;
        int dia = dia_json ? dia_json->valueint : 0;
        int rendimiento_general = rendimiento_general_json ? rendimiento_general_json->valueint : 0;
        int cansancio = cansancio_json ? cansancio_json->valueint : 0;
        int estado_animo = estado_animo_json ? estado_animo_json->valueint : 0;
        const char *comentario_personal = comentario_personal_json ? comentario_personal_json->valuestring : "";

        // Obtener ID de cancha
        sqlite3_stmt *cancha_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM cancha WHERE nombre = ?", -1, &cancha_stmt, NULL);
        sqlite3_bind_text(cancha_stmt, 1, cancha_nombre, -1, SQLITE_TRANSIENT);
        int cancha_id = -1;
        if (sqlite3_step(cancha_stmt) == SQLITE_ROW)
        {
            cancha_id = sqlite3_column_int(cancha_stmt, 0);
        }
        sqlite3_finalize(cancha_stmt);

        if (cancha_id == -1)
        {
            printf("Cancha '%s' no encontrada, creando...\n", cancha_nombre);
            // Crear cancha si no existe
            sqlite3_stmt *insert_cancha;
            sqlite3_prepare_v2(db, "INSERT INTO cancha(nombre) VALUES(?)", -1, &insert_cancha, NULL);
            sqlite3_bind_text(insert_cancha, 1, cancha_nombre, -1, SQLITE_TRANSIENT);
            sqlite3_step(insert_cancha);
            cancha_id = sqlite3_last_insert_rowid(db);
            sqlite3_finalize(insert_cancha);
        }

        // Obtener ID de camiseta
        sqlite3_stmt *camiseta_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
        sqlite3_bind_text(camiseta_stmt, 1, camiseta_nombre, -1, SQLITE_TRANSIENT);
        int camiseta_id = -1;
        if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
        {
            camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
        }
        sqlite3_finalize(camiseta_stmt);

        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo partido...\n", camiseta_nombre);
            continue;
        }

        // Obtener siguiente ID para partido
        int partido_id = 1;
        sqlite3_stmt *max_stmt;
        sqlite3_prepare_v2(db, "SELECT COALESCE(MAX(id), 0) + 1 FROM partido", -1, &max_stmt, NULL);
        if (sqlite3_step(max_stmt) == SQLITE_ROW)
        {
            partido_id = sqlite3_column_int(max_stmt, 0);
        }
        sqlite3_finalize(max_stmt);

        // Insertar partido
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, resultado, clima, dia, rendimiento_general, cansancio, estado_animo, comentario_personal) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, partido_id);
        sqlite3_bind_int(stmt, 2, cancha_id);
        sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, goles);
        sqlite3_bind_int(stmt, 5, asistencias);
        sqlite3_bind_int(stmt, 6, camiseta_id);
        sqlite3_bind_int(stmt, 7, resultado);
        sqlite3_bind_int(stmt, 8, clima);
        sqlite3_bind_int(stmt, 9, dia);
        sqlite3_bind_int(stmt, 10, rendimiento_general);
        sqlite3_bind_int(stmt, 11, cansancio);
        sqlite3_bind_int(stmt, 12, estado_animo);
        sqlite3_bind_text(stmt, 13, comentario_personal, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Partido en '%s' importado correctamente\n", cancha_nombre);
    }

    cJSON_Delete(json);
    printf("Importación de partidos completada\n");
}

/**
 * @brief Importa estadísticas desde archivo JSON.
 *
 * Lee el archivo data/estadisticas.json y procesa las estadísticas.
 * Nota: Las estadísticas son calculadas dinámicamente, no se almacenan.
 */
void importar_estadisticas_json()
{
    printf("Las estadísticas se calculan dinámicamente desde los partidos.\n");
    printf("No es necesario importar estadísticas desde JSON.\n");
}

/**
 * @brief Importa lesiones desde archivo JSON.
 *
 * Lee el archivo JSON de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_json()
{
    char filename[256];
    solicitar_archivo("Ingrese la ruta del archivo JSON de lesiones", filename, sizeof(filename));

    if (strlen(filename) == 0)
    {
        printf("Operación cancelada.\n");
        return;
    }

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de lesiones inválido\n");
        return;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON de lesiones debe ser un array\n");
        cJSON_Delete(json);
        return;
    }

    int count = cJSON_GetArraySize(json);
    printf("Importando %d lesiones...\n", count);

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON *id_json = cJSON_GetObjectItem(item, "id");
        cJSON *jugador_json = cJSON_GetObjectItem(item, "jugador");
        cJSON *tipo_json = cJSON_GetObjectItem(item, "tipo");
        cJSON *descripcion_json = cJSON_GetObjectItem(item, "descripcion");
        cJSON *fecha_json = cJSON_GetObjectItem(item, "fecha");

        if (!cJSON_IsNumber(id_json) || !cJSON_IsString(jugador_json) ||
                !cJSON_IsString(tipo_json) || !cJSON_IsString(descripcion_json) ||
                !cJSON_IsString(fecha_json))
            continue;

        int id = id_json->valueint;
        const char *jugador = jugador_json->valuestring;
        const char *tipo = tipo_json->valuestring;
        const char *descripcion = descripcion_json->valuestring;
        const char *fecha = fecha_json->valuestring;

        // Verificar si ya existe
        sqlite3_stmt *check_stmt;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion WHERE id = ?", -1, &check_stmt, NULL);
        sqlite3_bind_int(check_stmt, 1, id);
        sqlite3_step(check_stmt);
        int exists = sqlite3_column_int(check_stmt, 0);
        sqlite3_finalize(check_stmt);

        if (exists)
        {
            printf("Lesión ID %d ya existe, omitiendo...\n", id);
            continue;
        }

        // Insertar lesión
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "INSERT INTO lesion(id, jugador, tipo, descripcion, fecha) VALUES(?, ?, ?, ?, ?)", -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, jugador, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, tipo, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, descripcion, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, fecha, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Lesión de '%s' importada correctamente\n", jugador);
    }

    cJSON_Delete(json);
    printf("Importación de lesiones completada\n");
}

/**
 * @brief Importa todos los datos desde archivos JSON.
 *
 * Ejecuta todas las funciones de importación para cargar todos los datos.
 */
void importar_todo_json()
{
    print_header("IMPORTANDO DATOS DESDE JSON");

    importar_camisetas_json();
    importar_partidos_json();
    importar_estadisticas_json();
    importar_lesiones_json();

    printf("\nImportaciones Completadas Correctamente.\n");
    pause_console();
}
