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

#include <stdio.h>

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
