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
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

/**
 * @brief Elimina espacios en blanco al final de una cadena.
 *
 * @param str Cadena a recortar.
 * @return Puntero a la cadena recortada.
 */
static char *trim_trailing_spaces(char *str)
{
    if (!str) return NULL;
    char *end = str + strlen(str) - 1;
    while (end > str && *end == ' ')
    {
        *end = '\0';
        end--;
    }
    return str;
}

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
 * @brief Importa camisetas desde archivo JSON.
 *
 * Lee el archivo JSON de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_json()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\camisetas.json", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de camisetas invalido\n");
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
        sqlite3_prepare_v2(db, "INSERT INTO camiseta(id, nombre, sorteada) VALUES(?, ?, 0)", -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Camiseta '%s' importada correctamente\n", nombre);
    }

    cJSON_Delete(json);
    printf("Importacion de camisetas completada\n");
}

/**
 * @brief Importa partidos desde archivo JSON.
 *
 * Lee el archivo JSON de partidos y los inserta en la base de datos.
 */
void importar_partidos_json()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\partidos.json", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de partidos invalido\n");
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

        // Verificar si ya existe un partido con los mismos datos
        sqlite3_stmt *dup_stmt;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido WHERE cancha_id = ? AND fecha_hora = ? AND camiseta_id = ?", -1, &dup_stmt, NULL);
        sqlite3_bind_int(dup_stmt, 1, cancha_id);
        sqlite3_bind_text(dup_stmt, 2, fecha, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(dup_stmt, 3, camiseta_id);
        sqlite3_step(dup_stmt);
        int exists = sqlite3_column_int(dup_stmt, 0);
        sqlite3_finalize(dup_stmt);

        if (exists)
        {
            printf("Partido ya existe, omitiendo...\n");
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
    printf("Importacion de partidos completada\n");
}



/**
 * @brief Importa lesiones desde archivo JSON.
 *
 * Lee el archivo JSON de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_json()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\lesiones.json", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de lesiones invalido\n");
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
            printf("Lesion ID %d ya existe, omitiendo...\n", id);
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

        printf("Lesion de '%s' importada correctamente\n", jugador);
    }

    cJSON_Delete(json);
    printf("Importacion de lesiones completada\n");
}

/**
 * @brief Importa estadisticas desde archivo JSON.
 *
 * Lee el archivo JSON de estadisticas y las inserta en la base de datos.
 */
void importar_estadisticas_json()
{
    // Crear tabla estadistica si no existe
    const char *create_table_sql = "CREATE TABLE IF NOT EXISTS estadistica ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                   "camiseta_id INTEGER,"
                                   "goles INTEGER,"
                                   "asistencias INTEGER,"
                                   "partidos INTEGER,"
                                   "victorias INTEGER,"
                                   "empates INTEGER,"
                                   "derrotas INTEGER,"
                                   "FOREIGN KEY (camiseta_id) REFERENCES camiseta(id));";
    char *err_msg = NULL;
    if (sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg) != SQLITE_OK)
    {
        printf("Error creando tabla estadistica: %s\n", err_msg);
        sqlite3_free(err_msg);
        return;
    }

    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\estadisticas.json", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de estadisticas invalido\n");
        return;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON de estadisticas debe ser un array\n");
        cJSON_Delete(json);
        return;
    }

    int count = cJSON_GetArraySize(json);
    printf("Importando %d estadisticas...\n", count);

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON *camiseta_json = cJSON_GetObjectItem(item, "camiseta");
        cJSON *goles_json = cJSON_GetObjectItem(item, "goles");
        cJSON *asistencias_json = cJSON_GetObjectItem(item, "asistencias");
        cJSON *partidos_json = cJSON_GetObjectItem(item, "partidos");
        cJSON *victorias_json = cJSON_GetObjectItem(item, "victorias");
        cJSON *empates_json = cJSON_GetObjectItem(item, "empates");
        cJSON *derrotas_json = cJSON_GetObjectItem(item, "derrotas");

        if (!cJSON_IsString(camiseta_json) || !cJSON_IsNumber(goles_json) ||
                !cJSON_IsNumber(asistencias_json) || !cJSON_IsNumber(partidos_json))
            continue;

        const char *camiseta = camiseta_json->valuestring;
        int goles = goles_json->valueint;
        int asistencias = asistencias_json->valueint;
        int partidos = partidos_json->valueint;
        int victorias = victorias_json ? victorias_json->valueint : 0;
        int empates = empates_json ? empates_json->valueint : 0;
        int derrotas = derrotas_json ? derrotas_json->valueint : 0;

        // Obtener ID de camiseta
        sqlite3_stmt *camiseta_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
        sqlite3_bind_text(camiseta_stmt, 1, camiseta, -1, SQLITE_TRANSIENT);
        int camiseta_id = -1;
        if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
        {
            camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
        }
        sqlite3_finalize(camiseta_stmt);

        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo estadística...\n", camiseta);
            continue;
        }

        // Verificar si ya existe estadística para esta camiseta
        sqlite3_stmt *check_stmt;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM estadistica WHERE camiseta_id = ?", -1, &check_stmt, NULL);
        sqlite3_bind_int(check_stmt, 1, camiseta_id);
        sqlite3_step(check_stmt);
        int exists = sqlite3_column_int(check_stmt, 0);
        sqlite3_finalize(check_stmt);

        if (exists)
        {
            printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n", camiseta);
            continue;
        }

        // Insertar estadística
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "INSERT INTO estadistica(camiseta_id, goles, asistencias, partidos, victorias, empates, derrotas) VALUES(?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, camiseta_id);
        sqlite3_bind_int(stmt, 2, goles);
        sqlite3_bind_int(stmt, 3, asistencias);
        sqlite3_bind_int(stmt, 4, partidos);
        sqlite3_bind_int(stmt, 5, victorias);
        sqlite3_bind_int(stmt, 6, empates);
        sqlite3_bind_int(stmt, 7, derrotas);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Estadistica de '%s' importada correctamente\n", camiseta);
    }

    cJSON_Delete(json);
    printf("Importacion de estadisticas completada\n");
}

/**
 * @brief Importa camisetas desde archivo JSON con pausa.
 */
static void importar_camisetas_json_con_pausa()
{
    printf("Importando camisetas desde JSON...\n");
    importar_camisetas_json();
    printf("Importacion de camisetas completada.\n");
    pause_console();
}

/**
 * @brief Importa partidos desde archivo JSON con pausa.
 */
static void importar_partidos_json_con_pausa()
{
    printf("Importando partidos desde JSON...\n");
    importar_partidos_json();
    printf("Importacion de partidos completada.\n");
    pause_console();
}

/**
 * @brief Importa lesiones desde archivo JSON con pausa.
 */
static void importar_lesiones_json_con_pausa()
{
    printf("Importando lesiones desde JSON...\n");
    importar_lesiones_json();
    printf("Importacion de lesiones completada.\n");
    pause_console();
}

/**
 * @brief Importa estadisticas desde archivo JSON con pausa.
 */
static void importar_estadisticas_json_con_pausa()
{
    printf("Importando estadisticas desde JSON...\n");
    importar_estadisticas_json();
    printf("Importacion de estadisticas completada.\n");
    pause_console();
}

/**
 * @brief Importa camisetas desde archivo TXT con pausa.
 */
static void importar_camisetas_txt_con_pausa()
{
    printf("Importando camisetas desde TXT...\n");
    importar_camisetas_txt();
    printf("Importacion de camisetas completada.\n");
    pause_console();
}

/**
 * @brief Importa partidos desde archivo TXT con pausa.
 */
static void importar_partidos_txt_con_pausa()
{
    printf("Importando partidos desde TXT...\n");
    importar_partidos_txt();
    printf("Importacion de partidos completada.\n");
    pause_console();
}

/**
 * @brief Importa lesiones desde archivo TXT con pausa.
 */
static void importar_lesiones_txt_con_pausa()
{
    printf("Importando lesiones desde TXT...\n");
    importar_lesiones_txt();
    printf("Importacion de lesiones completada.\n");
    pause_console();
}

/**
 * @brief Importa estadisticas desde archivo TXT con pausa.
 */
static void importar_estadisticas_txt_con_pausa()
{
    printf("Importando estadisticas desde TXT...\n");
    importar_estadisticas_txt();
    printf("Importacion de estadisticas completada.\n");
    pause_console();
}

/**
 * @brief Importa camisetas desde archivo CSV con pausa.
 */
static void importar_camisetas_csv_con_pausa()
{
    printf("Importando camisetas desde CSV...\n");
    importar_camisetas_csv();
    printf("Importacion de camisetas completada.\n");
    pause_console();
}

/**
 * @brief Importa partidos desde archivo CSV con pausa.
 */
static void importar_partidos_csv_con_pausa()
{
    printf("Importando partidos desde CSV...\n");
    importar_partidos_csv();
    printf("Importacion de partidos completada.\n");
    pause_console();
}

/**
 * @brief Importa lesiones desde archivo CSV con pausa.
 */
static void importar_lesiones_csv_con_pausa()
{
    printf("Importando lesiones desde CSV...\n");
    importar_lesiones_csv();
    printf("Importacion de lesiones completada.\n");
    pause_console();
}

/**
 * @brief Importa estadisticas desde archivo CSV con pausa.
 */
static void importar_estadisticas_csv_con_pausa()
{
    printf("Importando estadisticas desde CSV...\n");
    importar_estadisticas_csv();
    printf("Importacion de estadisticas completada.\n");
    pause_console();
}

/**
 * @brief Importa todos los datos desde archivos CSV con pausa.
 */
static void importar_todo_csv_con_pausa()
{
    printf("Importando todo desde CSV...\n");
    importar_camisetas_csv();
    importar_partidos_csv();
    importar_lesiones_csv();
    importar_estadisticas_csv();
    printf("Importacion de todo desde CSV completada.\n");
    pause_console();
}

/**
 * @brief Importa camisetas desde archivo HTML con pausa.
 */
static void importar_camisetas_html_con_pausa()
{
    printf("Importando camisetas desde HTML...\n");
    importar_camisetas_html();
    printf("Importacion de camisetas completada.\n");
    pause_console();
}

/**
 * @brief Importa partidos desde archivo HTML con pausa.
 */
static void importar_partidos_html_con_pausa()
{
    printf("Importando partidos desde HTML...\n");
    importar_partidos_html();
    printf("Importacion de partidos completada.\n");
    pause_console();
}

/**
 * @brief Importa lesiones desde archivo HTML con pausa.
 */
static void importar_lesiones_html_con_pausa()
{
    printf("Importando lesiones desde HTML...\n");
    importar_lesiones_html();
    printf("Importacion de lesiones completada.\n");
    pause_console();
}

/**
 * @brief Importa estadisticas desde archivo HTML con pausa.
 */
static void importar_estadisticas_html_con_pausa()
{
    printf("Importando estadisticas desde HTML...\n");
    importar_estadisticas_html();
    printf("Importacion de estadisticas completada.\n");
    pause_console();
}

/**
 * @brief Importa todos los datos desde archivos HTML con pausa.
 */
static void importar_todo_html_con_pausa()
{
    printf("Importando todo desde HTML...\n");
    importar_camisetas_html();
    importar_partidos_html();
    importar_lesiones_html();
    importar_estadisticas_html();
    printf("Importacion de todo desde HTML completada.\n");
    pause_console();
}

/**
 * @brief Importa todos los datos desde archivos TXT con pausa.
 */
static void importar_todo_txt_con_pausa()
{
    printf("Importando todo desde TXT...\n");
    importar_camisetas_txt();
    importar_partidos_txt();
    importar_lesiones_txt();
    importar_estadisticas_txt();
    printf("Importacion de todo desde TXT completada.\n");
    pause_console();
}

/* ===================== IMPORTACIÓN DESDE TXT ===================== */

/**
 * @brief Importa camisetas desde archivo TXT.
 *
 * Lee el archivo TXT de camisetas y las inserta en la base de datos.
 * El formato esperado es: ID - NOMBRE
 */
void importar_camisetas_txt()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\camisetas.txt", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando camisetas desde TXT...\n");
    char line[1024];
    int count = 0;

    // Saltar la primera línea (LISTADO DE CAMISETAS)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "ID - NOMBRE"
        int id;
        char nombre[256];

        if (sscanf(line, "%d - %[^\n]", &id, nombre) == 2)
        {
            trim_trailing_spaces(nombre);
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
            sqlite3_prepare_v2(db, "INSERT INTO camiseta(id, nombre, sorteada) VALUES(?, ?, 0)", -1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            printf("Camiseta '%s' importada correctamente\n", nombre);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de camisetas desde TXT completada. %d camisetas importadas\n", count);
}

/**
 * @brief Importa partidos desde archivo TXT.
 *
 * Lee el archivo TXT de partidos y los inserta en la base de datos.
 * El formato esperado es complejo con múltiples campos separados por |
 */
void importar_partidos_txt()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\partidos.txt", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando partidos desde TXT...\n");
    char line[2048];
    int count = 0;

    // Saltar la primera línea (LISTADO DE PARTIDOS)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea compleja
        char cancha[256], fecha[256], camiseta[256], resultado_str[32], clima_str[32], dia_str[32], comentario[512];
        int goles, asistencias, rendimiento_general, cansancio, estado_animo;

        // Formato: CANCHA | FECHA | G:Goles A:Asistencias | CAMISETA | Res:Resultado Cli:Clima Dia:Dia RG:Rendimiento Can:Cansancio EA:EstadoAnimo | Comentario
        if (sscanf(line, "%[^|] | %[^|] | G:%d A:%d | %[^|] | Res:%[^ ] Cli:%[^ ] Dia:%[^ ] RG:%d Can:%d EA:%d | %[^\n]",
                   cancha, fecha, &goles, &asistencias, camiseta,
                   resultado_str, clima_str, dia_str, &rendimiento_general, &cansancio, &estado_animo, comentario) == 12)
        {
            // Convertir strings a números
            int resultado = 0;
            if (strcmp(resultado_str, "VICTORIA") == 0) resultado = 1;
            else if (strcmp(resultado_str, "EMPATE") == 0) resultado = 2;
            else if (strcmp(resultado_str, "DERROTA") == 0) resultado = 3;

            int clima = 0;
            if (strcmp(clima_str, "Despejado") == 0) clima = 1;
            else if (strcmp(clima_str, "Nublado") == 0) clima = 2;
            else if (strcmp(clima_str, "Lluvia") == 0) clima = 3;
            else if (strcmp(clima_str, "Ventoso") == 0) clima = 4;
            else if (strcmp(clima_str, "Mucho") == 0) clima = 5; // Mucho Calor o Mucho Frio
            else if (strcmp(clima_str, "Frio") == 0) clima = 6;

            int dia = 0;
            if (strcmp(dia_str, "Dia") == 0) dia = 1;
            else if (strcmp(dia_str, "Tarde") == 0) dia = 2;
            else if (strcmp(dia_str, "Noche") == 0) dia = 3;

            // Obtener ID de cancha
            sqlite3_stmt *cancha_stmt;
            sqlite3_prepare_v2(db, "SELECT id FROM cancha WHERE nombre = ?", -1, &cancha_stmt, NULL);
            sqlite3_bind_text(cancha_stmt, 1, cancha, -1, SQLITE_TRANSIENT);
            int cancha_id = -1;
            if (sqlite3_step(cancha_stmt) == SQLITE_ROW)
            {
                cancha_id = sqlite3_column_int(cancha_stmt, 0);
            }
            sqlite3_finalize(cancha_stmt);

            if (cancha_id == -1)
            {
                printf("Cancha '%s' no encontrada, creando...\n", cancha);
                // Crear cancha si no existe
                sqlite3_stmt *insert_cancha;
                sqlite3_prepare_v2(db, "INSERT INTO cancha(nombre) VALUES(?)", -1, &insert_cancha, NULL);
                sqlite3_bind_text(insert_cancha, 1, cancha, -1, SQLITE_TRANSIENT);
                sqlite3_step(insert_cancha);
                cancha_id = sqlite3_last_insert_rowid(db);
                sqlite3_finalize(insert_cancha);
            }

            // Obtener ID de camiseta
            sqlite3_stmt *camiseta_stmt;
            sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
            sqlite3_bind_text(camiseta_stmt, 1, camiseta, -1, SQLITE_TRANSIENT);
            int camiseta_id = -1;
            if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
            {
                camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
            }
            sqlite3_finalize(camiseta_stmt);

            if (camiseta_id == -1)
            {
                printf("Camiseta '%s' no encontrada, omitiendo partido...\n", camiseta);
                continue;
            }

            // Verificar si ya existe un partido con los mismos datos
            sqlite3_stmt *dup_stmt;
            sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido WHERE cancha_id = ? AND fecha_hora = ? AND camiseta_id = ?", -1, &dup_stmt, NULL);
            sqlite3_bind_int(dup_stmt, 1, cancha_id);
            sqlite3_bind_text(dup_stmt, 2, fecha, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(dup_stmt, 3, camiseta_id);
            sqlite3_step(dup_stmt);
            int exists = sqlite3_column_int(dup_stmt, 0);
            sqlite3_finalize(dup_stmt);

            if (exists)
            {
                printf("Partido ya existe, omitiendo...\n");
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
            sqlite3_bind_text(stmt, 13, comentario, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            printf("Partido en '%s' importado correctamente\n", cancha);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de partidos desde TXT completada. %d partidos importados\n", count);
}

/**
 * @brief Importa lesiones desde archivo TXT.
 *
 * Lee el archivo TXT de lesiones y las inserta en la base de datos.
 * El formato esperado es: ID - JUGADOR | TIPO | DESCRIPCION | FECHA
 */
void importar_lesiones_txt()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\lesiones.txt", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando lesiones desde TXT...\n");
    char line[1024];
    int count = 0;

    // Saltar la primera línea (LISTADO DE LESIONES)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "ID - JUGADOR | TIPO | DESCRIPCION | FECHA"
        int id;
        char jugador[256], tipo[256], descripcion[512], fecha[256];

        if (sscanf(line, "%d - %[^|] | %[^|] | %[^|] | %[^\n]", &id, jugador, tipo, descripcion, fecha) == 5)
        {
            // Verificar si ya existe
            sqlite3_stmt *check_stmt;
            sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion WHERE id = ?", -1, &check_stmt, NULL);
            sqlite3_bind_int(check_stmt, 1, id);
            sqlite3_step(check_stmt);
            int exists = sqlite3_column_int(check_stmt, 0);
            sqlite3_finalize(check_stmt);

            if (exists)
            {
                printf("Lesion ID %d ya existe, omitiendo...\n", id);
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

            printf("Lesion de '%s' importada correctamente\n", jugador);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de lesiones desde TXT completada. %d lesiones importadas\n", count);
}

/**
 * @brief Importa estadisticas desde archivo TXT.
 *
 * Lee el archivo TXT de estadisticas y las inserta en la base de datos.
 * El formato esperado es: CAMISETA | G:Goles A:Asistencias P:Partidos V:Victorias E:Empates D:Derrotas
 */
void importar_estadisticas_txt()
{
    // Crear tabla estadistica si no existe
    const char *create_table_sql = "CREATE TABLE IF NOT EXISTS estadistica ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                   "camiseta_id INTEGER,"
                                   "goles INTEGER,"
                                   "asistencias INTEGER,"
                                   "partidos INTEGER,"
                                   "victorias INTEGER,"
                                   "empates INTEGER,"
                                   "derrotas INTEGER,"
                                   "FOREIGN KEY (camiseta_id) REFERENCES camiseta(id));";
    char *err_msg = NULL;
    if (sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg) != SQLITE_OK)
    {
        printf("Error creando tabla estadistica: %s\n", err_msg);
        sqlite3_free(err_msg);
        return;
    }

    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\estadisticas.txt", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando estadisticas desde TXT...\n");
    char line[1024];
    int count = 0;

    // Saltar la primera línea (LISTADO DE ESTADISTICAS)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "CAMISETA | G:Goles A:Asistencias P:Partidos V:Victorias E:Empates D:Derrotas"
        char camiseta[256];
        int goles, asistencias, partidos, victorias, empates, derrotas;

        if (sscanf(line, "%[^|] | G:%d A:%d P:%d V:%d E:%d D:%d", camiseta, &goles, &asistencias, &partidos, &victorias, &empates, &derrotas) == 7)
        {
            // Obtener ID de camiseta
            sqlite3_stmt *camiseta_stmt;
            sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
            sqlite3_bind_text(camiseta_stmt, 1, camiseta, -1, SQLITE_TRANSIENT);
            int camiseta_id = -1;
            if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
            {
                camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
            }
            sqlite3_finalize(camiseta_stmt);

            if (camiseta_id == -1)
            {
                printf("Camiseta '%s' no encontrada, omitiendo estadística...\n", camiseta);
                continue;
            }

            // Verificar si ya existe estadística para esta camiseta
            sqlite3_stmt *check_stmt;
            sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM estadistica WHERE camiseta_id = ?", -1, &check_stmt, NULL);
            sqlite3_bind_int(check_stmt, 1, camiseta_id);
            sqlite3_step(check_stmt);
            int exists = sqlite3_column_int(check_stmt, 0);
            sqlite3_finalize(check_stmt);

            if (exists)
            {
                printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n", camiseta);
                continue;
            }

            // Insertar estadística
            sqlite3_stmt *stmt;
            sqlite3_prepare_v2(db, "INSERT INTO estadistica(camiseta_id, goles, asistencias, partidos, victorias, empates, derrotas) VALUES(?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, camiseta_id);
            sqlite3_bind_int(stmt, 2, goles);
            sqlite3_bind_int(stmt, 3, asistencias);
            sqlite3_bind_int(stmt, 4, partidos);
            sqlite3_bind_int(stmt, 5, victorias);
            sqlite3_bind_int(stmt, 6, empates);
            sqlite3_bind_int(stmt, 7, derrotas);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            printf("Estadistica de '%s' importada correctamente\n", camiseta);
            count++;
        }
        else
        {
            printf("Error parsing line: %s", line);
        }
    }

    fclose(file);
    printf("Importacion de estadisticas desde TXT completada. %d estadisticas importadas\n", count);
}

/* ===================== IMPORTACIÓN DESDE CSV ===================== */

/**
 * @brief Importa camisetas desde archivo CSV.
 *
 * Lee el archivo CSV de camisetas y las inserta en la base de datos.
 * El formato esperado es: id,nombre
 */
void importar_camisetas_csv()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\camisetas.csv", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando camisetas desde CSV...\n");
    char line[1024];
    int count = 0;

    // Saltar la primera línea (cabecera)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "id,nombre"
        int id;
        char nombre[256];

        if (sscanf(line, "%d,%[^\n]", &id, nombre) == 2)
        {
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
            sqlite3_prepare_v2(db, "INSERT INTO camiseta(id, nombre, sorteada) VALUES(?, ?, 0)", -1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            printf("Camiseta '%s' importada correctamente\n", nombre);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de camisetas desde CSV completada. %d camisetas importadas\n", count);
}

/**
 * @brief Importa partidos desde archivo CSV.
 *
 * Lee el archivo CSV de partidos y los inserta en la base de datos.
 * El formato esperado es complejo con múltiples campos separados por coma.
 */
void importar_partidos_csv()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\partidos.csv", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando partidos desde CSV...\n");
    char line[2048];
    int count = 0;

    // Saltar la primera línea (cabecera)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea compleja
        char cancha[256], fecha[256], camiseta[256], resultado_str[32], clima_str[32], dia_str[32], comentario[512];
        int goles, asistencias, rendimiento_general, cansancio, estado_animo;

        // Formato: cancha,fecha,goles,asistencias,camiseta,resultado,clima,dia,rendimiento_general,cansancio,estado_animo,comentario
        if (sscanf(line, "%[^,],%[^,],%d,%d,%[^,],%[^,],%[^,],%[^,],%d,%d,%d,%[^\n]",
                   cancha, fecha, &goles, &asistencias, camiseta,
                   resultado_str, clima_str, dia_str, &rendimiento_general, &cansancio, &estado_animo, comentario) == 12)
        {
            // Convertir strings a números
            int resultado = 0;
            if (strcmp(resultado_str, "VICTORIA") == 0) resultado = 1;
            else if (strcmp(resultado_str, "EMPATE") == 0) resultado = 2;
            else if (strcmp(resultado_str, "DERROTA") == 0) resultado = 3;

            int clima = 0;
            if (strcmp(clima_str, "Despejado") == 0) clima = 1;
            else if (strcmp(clima_str, "Nublado") == 0) clima = 2;
            else if (strcmp(clima_str, "Lluvia") == 0) clima = 3;
            else if (strcmp(clima_str, "Ventoso") == 0) clima = 4;
            else if (strcmp(clima_str, "Mucho") == 0) clima = 5;
            else if (strcmp(clima_str, "Frio") == 0) clima = 6;

            int dia = 0;
            if (strcmp(dia_str, "Dia") == 0) dia = 1;
            else if (strcmp(dia_str, "Tarde") == 0) dia = 2;
            else if (strcmp(dia_str, "Noche") == 0) dia = 3;

            // Obtener ID de cancha
            sqlite3_stmt *cancha_stmt;
            sqlite3_prepare_v2(db, "SELECT id FROM cancha WHERE nombre = ?", -1, &cancha_stmt, NULL);
            sqlite3_bind_text(cancha_stmt, 1, cancha, -1, SQLITE_TRANSIENT);
            int cancha_id = -1;
            if (sqlite3_step(cancha_stmt) == SQLITE_ROW)
            {
                cancha_id = sqlite3_column_int(cancha_stmt, 0);
            }
            sqlite3_finalize(cancha_stmt);

            if (cancha_id == -1)
            {
                printf("Cancha '%s' no encontrada, creando...\n", cancha);
                // Crear cancha si no existe
                sqlite3_stmt *insert_cancha;
                sqlite3_prepare_v2(db, "INSERT INTO cancha(nombre) VALUES(?)", -1, &insert_cancha, NULL);
                sqlite3_bind_text(insert_cancha, 1, cancha, -1, SQLITE_TRANSIENT);
                sqlite3_step(insert_cancha);
                cancha_id = sqlite3_last_insert_rowid(db);
                sqlite3_finalize(insert_cancha);
            }

            // Obtener ID de camiseta
            sqlite3_stmt *camiseta_stmt;
            sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
            sqlite3_bind_text(camiseta_stmt, 1, camiseta, -1, SQLITE_TRANSIENT);
            int camiseta_id = -1;
            if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
            {
                camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
            }
            sqlite3_finalize(camiseta_stmt);

            if (camiseta_id == -1)
            {
                printf("Camiseta '%s' no encontrada, omitiendo partido...\n", camiseta);
                continue;
            }

            // Verificar si ya existe un partido con los mismos datos
            sqlite3_stmt *dup_stmt;
            sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido WHERE cancha_id = ? AND fecha_hora = ? AND camiseta_id = ?", -1, &dup_stmt, NULL);
            sqlite3_bind_int(dup_stmt, 1, cancha_id);
            sqlite3_bind_text(dup_stmt, 2, fecha, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(dup_stmt, 3, camiseta_id);
            sqlite3_step(dup_stmt);
            int exists = sqlite3_column_int(dup_stmt, 0);
            sqlite3_finalize(dup_stmt);

            if (exists)
            {
                printf("Partido ya existe, omitiendo...\n");
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
            sqlite3_bind_text(stmt, 13, comentario, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            printf("Partido en '%s' importado correctamente\n", cancha);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de partidos desde CSV completada. %d partidos importados\n", count);
}

/**
 * @brief Importa lesiones desde archivo CSV.
 *
 * Lee el archivo CSV de lesiones y las inserta en la base de datos.
 * El formato esperado es: id,jugador,tipo,descripcion,fecha
 */
void importar_lesiones_csv()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\lesiones.csv", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando lesiones desde CSV...\n");
    char line[1024];
    int count = 0;

    // Saltar la primera línea (cabecera)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "id,jugador,tipo,descripcion,fecha"
        int id;
        char jugador[256], tipo[256], descripcion[512], fecha[256];

        if (sscanf(line, "%d,%[^,],%[^,],%[^,],%[^\n]", &id, jugador, tipo, descripcion, fecha) == 5)
        {
            // Verificar si ya existe
            sqlite3_stmt *check_stmt;
            sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion WHERE id = ?", -1, &check_stmt, NULL);
            sqlite3_bind_int(check_stmt, 1, id);
            sqlite3_step(check_stmt);
            int exists = sqlite3_column_int(check_stmt, 0);
            sqlite3_finalize(check_stmt);

            if (exists)
            {
                printf("Lesion ID %d ya existe, omitiendo...\n", id);
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

            printf("Lesion de '%s' importada correctamente\n", jugador);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de lesiones desde CSV completada. %d lesiones importadas\n", count);
}

/**
 * @brief Importa estadisticas desde archivo CSV.
 *
 * Lee el archivo CSV de estadisticas y las inserta en la base de datos.
 * El formato esperado es: camiseta,goles,asistencias,partidos,victorias,empates,derrotas
 */
void importar_estadisticas_csv()
{
    // Crear tabla estadistica si no existe
    const char *create_table_sql = "CREATE TABLE IF NOT EXISTS estadistica ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                   "camiseta_id INTEGER,"
                                   "goles INTEGER,"
                                   "asistencias INTEGER,"
                                   "partidos INTEGER,"
                                   "victorias INTEGER,"
                                   "empates INTEGER,"
                                   "derrotas INTEGER,"
                                   "FOREIGN KEY (camiseta_id) REFERENCES camiseta(id));";
    char *err_msg = NULL;
    if (sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg) != SQLITE_OK)
    {
        printf("Error creando tabla estadistica: %s\n", err_msg);
        sqlite3_free(err_msg);
        return;
    }

    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\estadisticas.csv", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return;
    }

    printf("Importando estadisticas desde CSV...\n");
    char line[1024];
    int count = 0;

    // Saltar la primera línea (cabecera)
    if (fgets(line, sizeof(line), file) == NULL)
    {
        printf("Error: Archivo vacío o formato incorrecto\n");
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "camiseta,goles,asistencias,partidos,victorias,empates,derrotas"
        char camiseta[256];
        int goles, asistencias, partidos, victorias, empates, derrotas;

        if (sscanf(line, "%[^,],%d,%d,%d,%d,%d,%d", camiseta, &goles, &asistencias, &partidos, &victorias, &empates, &derrotas) == 7)
        {
            // Obtener ID de camiseta
            sqlite3_stmt *camiseta_stmt;
            sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
            sqlite3_bind_text(camiseta_stmt, 1, camiseta, -1, SQLITE_TRANSIENT);
            int camiseta_id = -1;
            if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
            {
                camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
            }
            sqlite3_finalize(camiseta_stmt);

            if (camiseta_id == -1)
            {
                printf("Camiseta '%s' no encontrada, omitiendo estadística...\n", camiseta);
                continue;
            }

            // Verificar si ya existe estadística para esta camiseta
            sqlite3_stmt *check_stmt;
            sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM estadistica WHERE camiseta_id = ?", -1, &check_stmt, NULL);
            sqlite3_bind_int(check_stmt, 1, camiseta_id);
            sqlite3_step(check_stmt);
            int exists = sqlite3_column_int(check_stmt, 0);
            sqlite3_finalize(check_stmt);

            if (exists)
            {
                printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n", camiseta);
                continue;
            }

            // Insertar estadística
            sqlite3_stmt *stmt;
            sqlite3_prepare_v2(db, "INSERT INTO estadistica(camiseta_id, goles, asistencias, partidos, victorias, empates, derrotas) VALUES(?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, camiseta_id);
            sqlite3_bind_int(stmt, 2, goles);
            sqlite3_bind_int(stmt, 3, asistencias);
            sqlite3_bind_int(stmt, 4, partidos);
            sqlite3_bind_int(stmt, 5, victorias);
            sqlite3_bind_int(stmt, 6, empates);
            sqlite3_bind_int(stmt, 7, derrotas);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            printf("Estadistica de '%s' importada correctamente\n", camiseta);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de estadisticas desde CSV completada. %d estadisticas importadas\n", count);
}

/* ===================== IMPORTACIÓN DESDE HTML ===================== */

/**
 * @brief Importa camisetas desde archivo HTML.
 *
 * Lee el archivo HTML de camisetas y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML con <td> para id y nombre.
 */
void importar_camisetas_html()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\camisetas.html", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando camisetas desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar <td> tags
    while ((ptr = strstr(ptr, "<td>")) != NULL)
    {
        ptr += 4; // Saltar <td>
        char *end = strstr(ptr, "</td>");
        if (!end) break;

        *end = '\0';
        int id = atoi(ptr);

        // Siguiente <td> para nombre
        ptr = strstr(end + 5, "<td>");
        if (!ptr) break;
        ptr += 4;
        end = strstr(ptr, "</td>");
        if (!end) break;
        *end = '\0';
        char nombre[256];
        strcpy(nombre, ptr);

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
            ptr = end + 5;
            continue;
        }

        // Insertar
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "INSERT INTO camiseta(id, nombre, sorteada) VALUES(?, ?, 0)", -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Camiseta '%s' importada correctamente\n", nombre);
        count++;
        ptr = end + 5;
    }

    free(content);
    printf("Importacion de camisetas desde HTML completada. %d camisetas importadas\n", count);
}

/**
 * @brief Importa partidos desde archivo HTML.
 *
 * Lee el archivo HTML de partidos y los inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_partidos_html()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\partidos.html", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando partidos desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>
        char cancha[256], fecha[256], camiseta[256], resultado_str[32], clima_str[32], dia_str[32], comentario[512];
        int goles, asistencias, rendimiento_general, cansancio, estado_animo;

        // Extraer celdas
        for (int i = 0; i < 12; i++)
        {
            char *td = strstr(ptr, "<td>");
            if (!td) break;
            td += 4;
            char *end = strstr(td, "</td>");
            if (!end) break;
            *end = '\0';

            switch (i)
            {
            case 0:
                strcpy(cancha, td);
                break;
            case 1:
                strcpy(fecha, td);
                break;
            case 2:
                goles = atoi(td);
                break;
            case 3:
                asistencias = atoi(td);
                break;
            case 4:
                strcpy(camiseta, td);
                break;
            case 5:
                strcpy(resultado_str, td);
                break;
            case 6:
                strcpy(clima_str, td);
                break;
            case 7:
                strcpy(dia_str, td);
                break;
            case 8:
                rendimiento_general = atoi(td);
                break;
            case 9:
                cansancio = atoi(td);
                break;
            case 10:
                estado_animo = atoi(td);
                break;
            case 11:
                strcpy(comentario, td);
                break;
            }
            ptr = end + 5;
        }

        // Convertir strings a números
        int resultado = 0;
        if (strcmp(resultado_str, "VICTORIA") == 0) resultado = 1;
        else if (strcmp(resultado_str, "EMPATE") == 0) resultado = 2;
        else if (strcmp(resultado_str, "DERROTA") == 0) resultado = 3;

        int clima = 0;
        if (strcmp(clima_str, "Despejado") == 0) clima = 1;
        else if (strcmp(clima_str, "Nublado") == 0) clima = 2;
        else if (strcmp(clima_str, "Lluvia") == 0) clima = 3;
        else if (strcmp(clima_str, "Ventoso") == 0) clima = 4;
        else if (strcmp(clima_str, "Mucho") == 0) clima = 5;
        else if (strcmp(clima_str, "Frio") == 0) clima = 6;

        int dia = 0;
        if (strcmp(dia_str, "Dia") == 0) dia = 1;
        else if (strcmp(dia_str, "Tarde") == 0) dia = 2;
        else if (strcmp(dia_str, "Noche") == 0) dia = 3;

        // Obtener ID de cancha
        sqlite3_stmt *cancha_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM cancha WHERE nombre = ?", -1, &cancha_stmt, NULL);
        sqlite3_bind_text(cancha_stmt, 1, cancha, -1, SQLITE_TRANSIENT);
        int cancha_id = -1;
        if (sqlite3_step(cancha_stmt) == SQLITE_ROW)
        {
            cancha_id = sqlite3_column_int(cancha_stmt, 0);
        }
        sqlite3_finalize(cancha_stmt);

        if (cancha_id == -1)
        {
            printf("Cancha '%s' no encontrada, creando...\n", cancha);
            // Crear cancha si no existe
            sqlite3_stmt *insert_cancha;
            sqlite3_prepare_v2(db, "INSERT INTO cancha(nombre) VALUES(?)", -1, &insert_cancha, NULL);
            sqlite3_bind_text(insert_cancha, 1, cancha, -1, SQLITE_TRANSIENT);
            sqlite3_step(insert_cancha);
            cancha_id = sqlite3_last_insert_rowid(db);
            sqlite3_finalize(insert_cancha);
        }

        // Obtener ID de camiseta
        sqlite3_stmt *camiseta_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
        sqlite3_bind_text(camiseta_stmt, 1, camiseta, -1, SQLITE_TRANSIENT);
        int camiseta_id = -1;
        if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
        {
            camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
        }
        sqlite3_finalize(camiseta_stmt);

        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo partido...\n", camiseta);
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
        sqlite3_bind_text(stmt, 13, comentario, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Partido en '%s' importado correctamente\n", cancha);
        count++;
    }

    free(content);
    printf("Importacion de partidos desde HTML completada. %d partidos importados\n", count);
}

/**
 * @brief Importa lesiones desde archivo HTML.
 *
 * Lee el archivo HTML de lesiones y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_lesiones_html()
{
    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\lesiones.html", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando lesiones desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>
        int id;
        char jugador[256], tipo[256], descripcion[512], fecha[256];

        // Extraer celdas
        for (int i = 0; i < 5; i++)
        {
            char *td = strstr(ptr, "<td>");
            if (!td) break;
            td += 4;
            char *end = strstr(td, "</td>");
            if (!end) break;
            *end = '\0';

            switch (i)
            {
            case 0:
                id = atoi(td);
                break;
            case 1:
                strcpy(jugador, td);
                break;
            case 2:
                strcpy(tipo, td);
                break;
            case 3:
                strcpy(descripcion, td);
                break;
            case 4:
                strcpy(fecha, td);
                break;
            }
            ptr = end + 5;
        }

        // Verificar si ya existe
        sqlite3_stmt *check_stmt;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion WHERE id = ?", -1, &check_stmt, NULL);
        sqlite3_bind_int(check_stmt, 1, id);
        sqlite3_step(check_stmt);
        int exists = sqlite3_column_int(check_stmt, 0);
        sqlite3_finalize(check_stmt);

        if (exists)
        {
            printf("Lesion ID %d ya existe, omitiendo...\n", id);
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

        printf("Lesion de '%s' importada correctamente\n", jugador);
        count++;
    }

    free(content);
    printf("Importacion de lesiones desde HTML completada. %d lesiones importadas\n", count);
}

/**
 * @brief Importa estadisticas desde archivo HTML.
 *
 * Lee el archivo HTML de estadisticas y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_estadisticas_html()
{
    // Crear tabla estadistica si no existe
    const char *create_table_sql = "CREATE TABLE IF NOT EXISTS estadistica ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                   "camiseta_id INTEGER,"
                                   "goles INTEGER,"
                                   "asistencias INTEGER,"
                                   "partidos INTEGER,"
                                   "victorias INTEGER,"
                                   "empates INTEGER,"
                                   "derrotas INTEGER,"
                                   "FOREIGN KEY (camiseta_id) REFERENCES camiseta(id));";
    char *err_msg = NULL;
    if (sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg) != SQLITE_OK)
    {
        printf("Error creando tabla estadistica: %s\n", err_msg);
        sqlite3_free(err_msg);
        return;
    }

    char filename[1024];
    strcpy(filename, get_import_dir());
    strncat(filename, "\\estadisticas.html", sizeof(filename) - strlen(filename) - 1);

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando estadisticas desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>
        char camiseta[256];
        int goles, asistencias, partidos, victorias, empates, derrotas;

        // Extraer celdas
        for (int i = 0; i < 7; i++)
        {
            char *td = strstr(ptr, "<td>");
            if (!td) break;
            td += 4;
            char *end = strstr(td, "</td>");
            if (!end) break;
            *end = '\0';

            switch (i)
            {
            case 0:
                strcpy(camiseta, td);
                break;
            case 1:
                goles = atoi(td);
                break;
            case 2:
                asistencias = atoi(td);
                break;
            case 3:
                partidos = atoi(td);
                break;
            case 4:
                victorias = atoi(td);
                break;
            case 5:
                empates = atoi(td);
                break;
            case 6:
                derrotas = atoi(td);
                break;
            }
            ptr = end + 5;
        }

        // Obtener ID de camiseta
        sqlite3_stmt *camiseta_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE nombre = ?", -1, &camiseta_stmt, NULL);
        sqlite3_bind_text(camiseta_stmt, 1, camiseta, -1, SQLITE_TRANSIENT);
        int camiseta_id = -1;
        if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
        {
            camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
        }
        sqlite3_finalize(camiseta_stmt);

        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo estadística...\n", camiseta);
            continue;
        }

        // Verificar si ya existe estadística para esta camiseta
        sqlite3_stmt *check_stmt;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM estadistica WHERE camiseta_id = ?", -1, &check_stmt, NULL);
        sqlite3_bind_int(check_stmt, 1, camiseta_id);
        sqlite3_step(check_stmt);
        int exists = sqlite3_column_int(check_stmt, 0);
        sqlite3_finalize(check_stmt);

        if (exists)
        {
            printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n", camiseta);
            continue;
        }

        // Insertar estadística
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "INSERT INTO estadistica(camiseta_id, goles, asistencias, partidos, victorias, empates, derrotas) VALUES(?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, camiseta_id);
        sqlite3_bind_int(stmt, 2, goles);
        sqlite3_bind_int(stmt, 3, asistencias);
        sqlite3_bind_int(stmt, 4, partidos);
        sqlite3_bind_int(stmt, 5, victorias);
        sqlite3_bind_int(stmt, 6, empates);
        sqlite3_bind_int(stmt, 7, derrotas);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        printf("Estadistica de '%s' importada correctamente\n", camiseta);
        count++;
    }

    free(content);
    printf("Importacion de estadisticas desde HTML completada. %d estadisticas importadas\n", count);
}

/**
 * @brief Importa todos los datos desde archivos JSON con pausa.
 */
static void importar_todo_con_pausa()
{
    printf("Importando todo...\n");
    importar_camisetas_json();
    importar_partidos_json();
    importar_lesiones_json();
    importar_estadisticas_json();
    printf("Importacion de todo completada.\n");
    pause_console();
}

/**
 * @brief Submenú para importar datos desde archivos JSON.
 */
static void submenu_importar_json()
{
    MenuItem items[] =
    {
        {1, "Camisetas", importar_camisetas_json_con_pausa},
        {2, "Partidos", importar_partidos_json_con_pausa},
        {3, "Lesiones", importar_lesiones_json_con_pausa},
        {4, "Estadisticas", importar_estadisticas_json_con_pausa},
        {5, "Todo", importar_todo_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE JSON", items, 6);
}

/**
 * @brief Submenú para importar datos desde archivos TXT.
 */
static void submenu_importar_txt()
{
    MenuItem items[] =
    {
        {1, "Camisetas", importar_camisetas_txt_con_pausa},
        {2, "Partidos", importar_partidos_txt_con_pausa},
        {3, "Lesiones", importar_lesiones_txt_con_pausa},
        {4, "Estadisticas", importar_estadisticas_txt_con_pausa},
        {5, "Todo", importar_todo_txt_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE TXT", items, 6);
}

/**
 * @brief Submenú para importar datos desde archivos CSV.
 */
static void submenu_importar_csv()
{
    MenuItem items[] =
    {
        {1, "Camisetas", importar_camisetas_csv_con_pausa},
        {2, "Partidos", importar_partidos_csv_con_pausa},
        {3, "Lesiones", importar_lesiones_csv_con_pausa},
        {4, "Estadisticas", importar_estadisticas_csv_con_pausa},
        {5, "Todo", importar_todo_csv_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE CSV", items, 6);
}

/**
 * @brief Submenú para importar datos desde archivos HTML.
 */
static void submenu_importar_html()
{
    MenuItem items[] =
    {
        {1, "Camisetas", importar_camisetas_html_con_pausa},
        {2, "Partidos", importar_partidos_html_con_pausa},
        {3, "Lesiones", importar_lesiones_html_con_pausa},
        {4, "Estadisticas", importar_estadisticas_html_con_pausa},
        {5, "Todo", importar_todo_html_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE HTML", items, 6);
}

/**
 * @brief Menu principal para importar datos desde archivos según selección del usuario.
 *
 * Esta función muestra un menú principal para que el usuario seleccione el formato
 * de archivo desde el cual importar: JSON, TXT, CSV o HTML.
 * Cada opción lleva a un submenú específico para ese formato.
 */
void menu_importar()
{
    MenuItem items[] =
    {
        {1, "Importar desde JSON", submenu_importar_json},
        {2, "Importar desde TXT", submenu_importar_txt},
        {3, "Importar desde CSV", submenu_importar_csv},
        {4, "Importar desde HTML", submenu_importar_html},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS", items, 5);
}
