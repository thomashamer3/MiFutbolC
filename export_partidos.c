#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>

/* ===================== HELPER FUNCTIONS (STATIC) ===================== */

/**
 * Checks if there are any partido records in the database.
 * Returns 1 if records exist, 0 if no records found.
 * This avoids duplicating the count check in every export function.
 */
static int has_partido_records()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    return count > 0;
}

/**
 * Executes the standard partido query and returns the statement.
 * This centralizes the common SQL query used by most export functions.
 */
static sqlite3_stmt* execute_partido_query(const char* order_by_clause)
{
    sqlite3_stmt *stmt;
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by_clause ? order_by_clause : "");
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    return stmt;
}



/**
 * Writes partido data in CSV format to the given file.
 * Handles the common CSV formatting logic.
 */
static void write_partido_csv(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }
}

/**
 * Writes partido data in TXT format to the given file.
 * Handles the common TXT formatting logic.
 */
static void write_partido_txt(FILE *f, sqlite3_stmt *stmt, const char *title)
{
    fprintf(f, "%s\n\n", title);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }
}

/**
 * Writes partido data in JSON format to the given file.
 * Handles the common JSON formatting logic.
 */
static void write_partido_json(FILE *f, sqlite3_stmt *stmt, int is_array)
{
    cJSON *root = is_array ? cJSON_CreateArray() : cJSON_CreateObject();

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);

        if (is_array)
        {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "cancha", cancha_trimmed);
            cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 2));
            cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 3));
            cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
            cJSON_AddStringToObject(item, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
            cJSON_AddStringToObject(item, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
            cJSON_AddStringToObject(item, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
            cJSON_AddNumberToObject(item, "rendimiento_general", sqlite3_column_int(stmt, 8));
            cJSON_AddNumberToObject(item, "cansancio", sqlite3_column_int(stmt, 9));
            cJSON_AddNumberToObject(item, "estado_animo", sqlite3_column_int(stmt, 10));
            cJSON_AddStringToObject(item, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
            cJSON_AddItemToArray(root, item);
        }
        else
        {
            cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
            cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
            cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
            cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
            cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
            cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
            cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
            cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
            cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
            cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
            cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        }

        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

/**
 * Writes partido data in HTML format to the given file.
 * Handles the common HTML formatting logic.
 */
static void write_partido_html(FILE *f, sqlite3_stmt *stmt, const char *title)
{
    fprintf(f,
            "<html><body><h1>%s</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>",
            title);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
}

/* ===================== PARTIDOS ===================== */

/**
 * Export all partidos to CSV format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_csv()
{
    if (!has_partido_records())
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.csv"), "w");
    if (!f)
        return;

    sqlite3_stmt *stmt = execute_partido_query(NULL);
    write_partido_csv(f, stmt);
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path("partidos.csv"));
    fclose(f);
}

/**
 * Export all partidos to TXT format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_txt()
{
    if (!has_partido_records())
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.txt"), "w");
    if (!f)
        return;

    sqlite3_stmt *stmt = execute_partido_query(NULL);
    write_partido_txt(f, stmt, "LISTADO DE PARTIDOS");
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path("partidos.txt"));
    fclose(f);
}

/**
 * Export all partidos to JSON format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_json()
{
    if (!has_partido_records())
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.json"), "w");
    if (!f)
        return;

    sqlite3_stmt *stmt = execute_partido_query(NULL);
    write_partido_json(f, stmt, 1); // 1 = is_array
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path("partidos.json"));
    fclose(f);
}

/**
 * Export all partidos to HTML format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_html()
{
    if (!has_partido_records())
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.html"), "w");
    if (!f)
        return;

    sqlite3_stmt *stmt = execute_partido_query(NULL);
    write_partido_html(f, stmt, "Partidos");
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path("partidos.html"));
    fclose(f);
}

/* ===================== PARTIDOS ESPECIFICOS ===================== */

/**
 * @brief Exporta el partido con más goles a un archivo CSV
 */
void exportar_partido_mas_goles_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más goles a un archivo de texto plano
 */
void exportar_partido_mas_goles_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO CON MAS GOLES\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más goles a un archivo JSON
 */
void exportar_partido_mas_goles_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más goles a un archivo HTML
 */
void exportar_partido_mas_goles_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido con Mas Goles</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.html"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo CSV
 */
void exportar_partido_mas_asistencias_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo de texto plano
 */
void exportar_partido_mas_asistencias_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO CON MAS ASISTENCIAS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo JSON
 */
void exportar_partido_mas_asistencias_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo HTML
 */
void exportar_partido_mas_asistencias_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido con Mas Asistencias</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.html"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo CSV
 */
void exportar_partido_menos_goles_reciente_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo de texto plano
 */
void exportar_partido_menos_goles_reciente_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO MAS RECIENTE CON MENOS GOLES\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo JSON
 */
void exportar_partido_menos_goles_reciente_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo HTML
 */
void exportar_partido_menos_goles_reciente_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido Mas Reciente con Menos Goles</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.html"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo CSV
 */
void exportar_partido_menos_asistencias_reciente_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo de texto plano
 */
void exportar_partido_menos_asistencias_reciente_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO MAS RECIENTE CON MENOS ASISTENCIAS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo JSON
 */
void exportar_partido_menos_asistencias_reciente_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo HTML
 */
void exportar_partido_menos_asistencias_reciente_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido Mas Reciente con Menos Asistencias</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.html"));
    fclose(f);
}

