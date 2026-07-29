#include "cJSON.h"
#include "export.h"
#include "export_partidos_helpers.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

/* ===================== HELPER FUNCTIONS (STATIC) ===================== */

/**
 * Checks if there are any partido records in the database.
 * Returns 1 if records exist, 0 if no records found.
 * This avoids duplicating the count check in every export function.
 */
int has_partido_records(void)
{
    return check_partido_records();
}

/**
 * Executes the standard partido query and returns the statement.
 * This centralizes the common SQL query used by most export functions.
 */
sqlite3_stmt *execute_partido_query(const char *order_by_clause)
{
    return prepare_partido_query(order_by_clause);
}

/**
 * Writes partido data in CSV format to the given file.
 * Handles the common CSV formatting logic.
 */
static void write_partido_csv(FILE *file, sqlite3_stmt *stmt)
{
    fprintf(file, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,"
            "Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal,"
            "Atajaste_Todo_El_Partido\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        write_partido_csv_row(file, stmt);
    }
}

/**
 * Writes partido data in TXT format to the given file.
 * Handles the common TXT formatting logic.
 */
static void write_partido_txt(FILE *file, sqlite3_stmt *stmt)
{
    fprintf(file, "PARTIDOS\n\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        write_partido_txt_row(file, stmt);
    }
}

/**
 * Writes partido data in JSON format to the given file.
 * Handles the common JSON formatting logic.
 */
static void write_partido_json(FILE *file, sqlite3_stmt *stmt)
{
    fprintf(file, "[\n");
    int first = 1;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
        {
            fprintf(file, ",\n");
        }
        first = 0;

        cJSON *item = cJSON_CreateObject();
        write_partido_json_object(item, stmt);
        char *s = cJSON_PrintUnformatted(item);
        fprintf(file, "  %s", s);
        free(s);
        cJSON_Delete(item);
    }

    fprintf(file, "\n]\n");
}

/**
 * Writes partido data in HTML format to the given file.
 * Handles the common HTML formatting logic.
 */
static void write_partido_html(FILE *file, sqlite3_stmt *stmt)
{
    export_write_html_begin(file, "PARTIDOS");
    fprintf(file, "<table>\n<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</"
            "th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</"
            "th><th>Rendimiento General</th><th>Cansancio</th><th>Estado "
            "Animo</th><th>Comentario Personal</th><th>Atajaste Todo el Partido</th></tr>");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        write_partido_html_row(file, stmt);
    }

    export_write_html_table_footer(file, NULL);
}

/* ===================== PARTIDOS ===================== */

/**
 * Export all partidos to CSV format.
 * Asks user for limit when there are many records.
 */
void exportar_partidos_csv(void)
{
    int limit = obtener_limite_exportacion_partidos();
    export_partidos_generic("partidos.csv", write_partido_csv, limit);
}

/**
 * Export all partidos to TXT format.
 * Asks user for limit when there are many records.
 */
void exportar_partidos_txt(void)
{
    int limit = obtener_limite_exportacion_partidos();
    export_partidos_generic("partidos.txt", write_partido_txt, limit);
}

/**
 * Export all partidos to JSON format.
 * Asks user for limit when there are many records.
 */
void exportar_partidos_json(void)
{
    int limit = obtener_limite_exportacion_partidos();
    export_partidos_generic("partidos.json", write_partido_json, limit);
}

/**
 * Export all partidos to HTML format.
 * Asks user for limit when there are many records.
 */
void exportar_partidos_html(void)
{
    int limit = obtener_limite_exportacion_partidos();
    export_partidos_generic("partidos.html", write_partido_html, limit);
}

/**
 * Export all partidos to all 4 formats using one SQL query.
 * Asks user once for the limit.
 */
void exportar_partidos_all(void)
{
    if (!check_partido_records())
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    int limit = obtener_limite_exportacion_partidos();
    char order_by[64];
    if (limit > 0)
        snprintf(order_by, sizeof(order_by), "ORDER BY p.fecha_hora DESC LIMIT %d", limit);
    else
        snprintf(order_by, sizeof(order_by), "ORDER BY p.fecha_hora DESC");

    sqlite3_stmt *stmt = prepare_partido_query(order_by);
    if (!stmt)
    {
        return;
    }

    static const char *fnames[] = {"partidos.csv", "partidos.txt", "partidos.json",
                                   "partidos.html"
                                  };
    void (*writers[])(FILE *, sqlite3_stmt *) = {write_partido_csv, write_partido_txt,
                                                 write_partido_json, write_partido_html
                                                };

    for (int i = 0; i < 4; i++)
    {
        FILE *file = open_export_file(fnames[i]);
        if (!file)
        {
            continue;
        }

        if (i > 0)
        {
            sqlite3_reset(stmt);
        }
        writers[i](file, stmt);
        printf("Archivo exportado a: %s\n", get_export_path(fnames[i]));
        fclose(file);
    }

    sqlite3_finalize(stmt);
}

/* ===================== PARTIDOS ESPECIFICOS ===================== */

void exportar_partido_mas_goles_csv(void)
{
    exportar_partido_especifico_csv("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                                    "partido_mas_goles.csv");
}

void exportar_partido_mas_goles_txt(void)
{
    exportar_partido_especifico_txt("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                                    "partido_mas_goles.txt", "PARTIDO CON MAS GOLES");
}

void exportar_partido_mas_goles_json(void)
{
    export_partido_especifico_generic("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                                      "partido_mas_goles.json", write_partido_json);
}

void exportar_partido_mas_goles_html(void)
{
    export_partido_especifico_generic("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                                      "partido_mas_goles.html", write_partido_html);
}

void exportar_partido_mas_asistencias_csv(void)
{
    exportar_partido_especifico_csv("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                                    "partido_mas_asistencias.csv");
}

void exportar_partido_mas_asistencias_txt(void)
{
    exportar_partido_especifico_txt("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                                    "partido_mas_asistencias.txt", "PARTIDO CON MAS ASISTENCIAS");
}

void exportar_partido_mas_asistencias_json(void)
{
    export_partido_especifico_generic("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                                      "partido_mas_asistencias.json", write_partido_json);
}

void exportar_partido_mas_asistencias_html(void)
{
    export_partido_especifico_generic("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                                      "partido_mas_asistencias.html", write_partido_html);
}

void exportar_partido_menos_goles_reciente_csv(void)
{
    exportar_partido_especifico_csv("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                                    "partido_menos_goles_reciente.csv");
}

void exportar_partido_menos_goles_reciente_txt(void)
{
    exportar_partido_especifico_txt("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                                    "partido_menos_goles_reciente.txt",
                                    "PARTIDO MAS RECIENTE CON MENOS GOLES");
}

void exportar_partido_menos_goles_reciente_json(void)
{
    export_partido_especifico_generic("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                                      "partido_menos_goles_reciente.json", write_partido_json);
}

void exportar_partido_menos_goles_reciente_html(void)
{
    export_partido_especifico_generic("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                                      "partido_menos_goles_reciente.html", write_partido_html);
}

void exportar_partido_menos_asistencias_reciente_csv(void)
{
    exportar_partido_especifico_csv("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                                    "partido_menos_asistencias_reciente.csv");
}

void exportar_partido_menos_asistencias_reciente_txt(void)
{
    exportar_partido_especifico_txt("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                                    "partido_menos_asistencias_reciente.txt",
                                    "PARTIDO MAS RECIENTE CON MENOS ASISTENCIAS");
}

void exportar_partido_menos_asistencias_reciente_json(void)
{
    export_partido_especifico_generic("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                                      "partido_menos_asistencias_reciente.json",
                                      write_partido_json);
}

void exportar_partido_menos_asistencias_reciente_html(void)
{
    export_partido_especifico_generic("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                                      "partido_menos_asistencias_reciente.html",
                                      write_partido_html);
}
