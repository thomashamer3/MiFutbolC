#ifndef EXPORT_PARTIDOS_HELPERS_H
#define EXPORT_PARTIDOS_HELPERS_H

#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

/* ===================== DATABASE HELPERS ===================== */

/**
 * Checks if there are any partido records in the database.
 * Returns 1 if records exist, 0 if no records found.
 */
static int check_partido_records()
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
 * Prepares a partido query with the given order by clause.
 * Centralizes the common SQL query used by most export functions.
 */
static sqlite3_stmt* prepare_partido_query(const char* order_by_clause)
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
 * Gets the count of partido records.
 * Returns the number of records found.
 */
static int get_partido_count()
{
    sqlite3_stmt *count_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &count_stmt, NULL);
    if (sqlite3_step(count_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(count_stmt, 0);
    }
    sqlite3_finalize(count_stmt);
    return count;
}

/* ===================== FILE HELPERS ===================== */

/**
 * Opens an export file with the given filename.
 * Handles error checking and returns the file pointer.
 */
static FILE* open_export_file(const char* filename)
{
    FILE *f;
    fopen_s(&f, get_export_path(filename), "w");
    return f;
}

/**
 * Closes an export file and handles error checking.
 */
static void close_export_file(FILE* file)
{
    if (file)
    {
        fclose(file);
    }
}

/* ===================== DATA PROCESSING HELPERS ===================== */

/**
 * Trims trailing spaces from cancha text.
 * Returns a newly allocated string that must be freed.
 */
static char* trim_cancha_text(const char* text)
{
    char *trimmed = strdup(text);
    trim_trailing_spaces(trimmed);
    return trimmed;
}

/**
 * Extracts partido data from a statement into a structured format.
 * Returns a cJSON object containing all partido fields.
 */
static cJSON* extract_partido_data(sqlite3_stmt* stmt)
{
    cJSON *item = cJSON_CreateObject();

    char *cancha_trimmed = trim_cancha_text((const char *)sqlite3_column_text(stmt, 0));
    cJSON_AddStringToObject(item, "cancha", cancha_trimmed);
    free(cancha_trimmed);

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

    return item;
}

/* ===================== FORMAT-SPECIFIC HELPERS ===================== */

/**
 * Writes CSV header to the given file.
 */
static void write_csv_header(FILE* file)
{
    fprintf(file, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");
}

/**
 * Writes TXT header to the given file with the specified title.
 */
static void write_txt_header(FILE* file, const char* title)
{
    fprintf(file, "%s\n\n", title);
}

/**
 * Writes HTML header to the given file with the specified title.
 */
static void write_html_header(FILE* file, const char* title)
{
    fprintf(file,
            "<html><body><h1>%s</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>",
            title);
}

/**
 * Writes HTML footer to the given file.
 */
static void write_html_footer(FILE* file)
{
    fprintf(file, "</table></body></html>");
}

/* ===================== GENERIC EXPORT HELPERS ===================== */

/**
 * Generic export function for handling common export patterns.
 * Takes a filename and a write function pointer to handle format-specific writing.
 */
static void export_partidos_generic(const char* filename, void (*write_function)(FILE*, sqlite3_stmt*))
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = open_export_file(filename);
    if (!f)
        return;

    sqlite3_stmt *stmt = execute_partido_query(NULL);
    write_function(f, stmt);
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    close_export_file(f);
}

/**
 * Generic export function for handling specific partido exports.
 * Takes an order by clause, filename, and write function pointer.
 */
static void export_partido_especifico_generic(const char* order_by_clause, const char* filename, void (*write_function)(FILE*, sqlite3_stmt*))
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = open_export_file(filename);
    if (!f)
        return;

    sqlite3_stmt *stmt = execute_partido_query(order_by_clause);
    write_function(f, stmt);
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    close_export_file(f);
}

#endif // EXPORT_PARTIDOS_HELPERS_H
