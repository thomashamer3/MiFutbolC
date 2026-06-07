/**
 * @file export_common.c
 * @brief Implementacion compartida del flujo de exportacion con ExportConfig
 *
 * Centraliza la apertura del archivo, la escritura de cabecera/fila/pie y la
 * finalizacion del stmt, evitando duplicacion en los modulos de exportacion
 * que siguen el patron camiseta/temporada/dashboard/equipo/bienestar/torneo.
 */

#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *open_export_file(const char *filename, sqlite3_stmt *stmt)
{
    FILE *f;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || f == NULL)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }
    return f;
}

void export_generic_rows(ExportConfig *config, sqlite3_stmt *stmt)
{
    FILE *f = open_export_file(config->filename, stmt);
    if (!f) return;

    if (config->write_header)
        config->write_header(f, config->context);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        config->write_row(f, stmt, config->context);

    if (config->write_footer)
        config->write_footer(f, config->context);

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path(config->filename));
    fclose(f);
}

void export_generic_single(ExportConfig *config, sqlite3_stmt *stmt)
{
    FILE *f = open_export_file(config->filename, stmt);
    if (!f) return;

    if (config->write_header)
        config->write_header(f, config->context);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        config->write_row(f, stmt, config->context);

    if (config->write_footer)
        config->write_footer(f, config->context);

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path(config->filename));
    fclose(f);
}

int escribir_seccion_csv(FILE *f, const char *sql, const char *cabecera,
                         void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt))
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return 0;
    fprintf(f, "%s\n", cabecera);
    while (sqlite3_step(st) == SQLITE_ROW)
        escribir_fila(f, st);
    sqlite3_finalize(st);
    fprintf(f, "\n");
    return 1;
}

void escribir_seccion_txt(FILE *f, const char *titulo, const char *sql,
                          void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt))
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return;
    fprintf(f, "=== %s ===\n\n", titulo);
    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW)
    {
        escribir_fila(f, st);
        count++;
    }
    if (count == 0)
        fprintf(f, "Sin registros.\n\n");
    sqlite3_finalize(st);
    fprintf(f, "\n");
}

void escribir_seccion_json(cJSON *arr, const char *sql,
                           void (*escribir_objeto)(cJSON *item, sqlite3_stmt *stmt))
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return;
    while (sqlite3_step(st) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        escribir_objeto(item, st);
        cJSON_AddItemToArray(arr, item);
    }
    sqlite3_finalize(st);
}

void escribir_seccion_html(FILE *f, const char *titulo, const char *sql,
                           const char *cabeceras[],
                           void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt))
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return;
    fprintf(f, "<h2>%s</h2><table border='1'><tr>", titulo);
    for (int i = 0; cabeceras[i] != NULL; i++)
        fprintf(f, "<th>%s</th>", cabeceras[i]);
    fprintf(f, "</tr>");
    while (sqlite3_step(st) == SQLITE_ROW)
        escribir_fila(f, st);
    fprintf(f, "</table><br>\n");
    sqlite3_finalize(st);
}
