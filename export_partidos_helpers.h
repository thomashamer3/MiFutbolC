#ifndef EXPORT_PARTIDOS_HELPERS_H
#define EXPORT_PARTIDOS_HELPERS_H

#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include "sqlite3.h"
#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define EXPORT_PARTIDOS_HELPERS_UNUSED __attribute__((unused))
#else
#define EXPORT_PARTIDOS_HELPERS_UNUSED
#endif

#define PARTIDO_EXPORT_LIMIT 50

/* ===================== DATABASE HELPERS ===================== */

/**
 * Counts total partido records in the database.
 */
static EXPORT_PARTIDOS_HELPERS_UNUSED int contar_partidos_total(void)
{
    sqlite3_stmt *stmt;
    int total = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            total = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return total;
}

/**
 * Asks the user if they want to limit export to last 50 or export all.
 * Returns PARTIDO_EXPORT_LIMIT for limited, 0 for all.
 */
static EXPORT_PARTIDOS_HELPERS_UNUSED int obtener_limite_exportacion_partidos(void)
{
    int total = contar_partidos_total();
    if (total <= PARTIDO_EXPORT_LIMIT)
        return 0;

    printf("Hay %d partidos registrados.\n", total);
    if (confirmar("Desea exportar solo los ultimos 50?"))
        return PARTIDO_EXPORT_LIMIT;
    return 0;
}

/**
 * Checks if there are any partido records in the database.
 * Returns 1 if records exist, 0 if no records found.
 */
static int check_partido_records(void)
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
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
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal,p.atajaste_todo_el_partido "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by_clause ? order_by_clause : "");
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }
    return stmt;
}

/* ===================== FILE HELPERS ===================== */

/**
 * Opens an export file with the given filename.
 * Handles error checking and returns the file pointer.
 */
static FILE* open_export_file(const char* filename)
{
    FILE *f = NULL;
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

/**
 * Generic export function for handling common export patterns.
 * Takes a filename and a write function pointer to handle format-specific writing.
 */
static EXPORT_PARTIDOS_HELPERS_UNUSED void export_partidos_generic(const char* filename, void (*write_function)(FILE*, sqlite3_stmt*), int limit)
{
    if (!check_partido_records())
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = open_export_file(filename);
    if (!f)
        return;

    char order_by[64];
    if (limit > 0)
        snprintf(order_by, sizeof(order_by), "ORDER BY p.fecha_hora DESC LIMIT %d", limit);
    else
        snprintf(order_by, sizeof(order_by), "ORDER BY p.fecha_hora DESC");

    sqlite3_stmt *stmt = prepare_partido_query(order_by);
    if (!stmt)
    {
        close_export_file(f);
        return;
    }
    write_function(f, stmt);
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    close_export_file(f);
}

/**
 * Generic export function for handling specific partido exports.
 * Takes an order by clause, filename, and write function pointer.
 */
static EXPORT_PARTIDOS_HELPERS_UNUSED void export_partido_especifico_generic(const char* order_by_clause, const char* filename, void (*write_function)(FILE*, sqlite3_stmt*))
{
    if (!check_partido_records())
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = open_export_file(filename);
    if (!f)
        return;

    sqlite3_stmt *stmt = prepare_partido_query(order_by_clause);
    if (!stmt)
    {
        close_export_file(f);
        return;
    }
    write_function(f, stmt);
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    close_export_file(f);
}

#endif // EXPORT_PARTIDOS_HELPERS_H
