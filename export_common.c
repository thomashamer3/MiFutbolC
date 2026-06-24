/**
 * @file export_common.c
 * @brief Implementacion compartida del flujo de exportacion con ExportConfig
 *
 * Centraliza la apertura del archivo, la escritura de cabecera/fila/pie y la
 * finalizacion del stmt, evitando duplicacion en los modulos de exportacion
 * que siguen el patron camiseta/temporada/dashboard/equipo/bienestar/torneo.
 */

#include "cJSON.h"
#include "db.h"
#include "export.h"
#include "utils.h"

#include <stdio.h>

static FILE *open_export_file(const char *filename, sqlite3_stmt *stmt)
{
    FILE *file;
    errno_t err = fopen_s(&file, get_export_path(filename), "w");
    if (err != 0 || file == NULL)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }
    return file;
}

void export_generic_rows(ExportConfig *config, sqlite3_stmt *stmt)
{
    FILE *file = open_export_file(config->filename, stmt);
    if (!file)
    {
        sqlite3_finalize(stmt);
        return;
    }

    if (config->write_header)
    {
        config->write_header(file, config->context);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        config->write_row(file, stmt, config->context);
    }

    if (config->write_footer)
    {
        config->write_footer(file, config->context);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path(config->filename));
    fclose(file);
}

void export_all_formats(ExportDataFn data_fn, ExportConfig configs[], int num_formats)
{
    int count;
    sqlite3_stmt *stmt = data_fn(&count);
    if (!stmt)
    {
        return;
    }

    for (int i = 0; i < num_formats; i++)
    {
        FILE *file;
        errno_t err = fopen_s(&file, get_export_path(configs[i].filename), "w");
        if (err != 0 || file == NULL)
        {
            printf("Error: No se pudo crear %s\n", configs[i].filename);
            continue;
        }

        if (i > 0)
        {
            sqlite3_reset(stmt);
        }

        if (configs[i].write_header)
        {
            configs[i].write_header(file, configs[i].context);
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            configs[i].write_row(file, stmt, configs[i].context);
        }

        if (configs[i].write_footer)
        {
            configs[i].write_footer(file, configs[i].context);
        }

        printf("Archivo exportado a: %s\n", get_export_path(configs[i].filename));
        fclose(file);
    }

    sqlite3_finalize(stmt);
}

void export_generic_single(ExportConfig *config, sqlite3_stmt *stmt)
{
    FILE *file = open_export_file(config->filename, stmt);
    if (!file)
    {
        return;
    }

    if (config->write_header)
    {
        config->write_header(file, config->context);
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        config->write_row(file, stmt, config->context);
    }

    if (config->write_footer)
    {
        config->write_footer(file, config->context);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path(config->filename));
    fclose(file);
}

int escribir_seccion_csv(FILE *file, const char *sql, const char *cabecera,
                         void (*escribir_fila)(FILE *file, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    fprintf(file, "%s\n", cabecera);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        escribir_fila(file, stmt);
    }
    sqlite3_finalize(stmt);
    fprintf(file, "\n");
    return 1;
}

void escribir_seccion_txt(FILE *file, const char *titulo, const char *sql,
                          void (*escribir_fila)(FILE *file, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }
    fprintf(file, "=== %s ===\n\n", titulo);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        escribir_fila(file, stmt);
        count++;
    }
    if (count == 0)
    {
        fprintf(file, "Sin registros.\n\n");
    }
    sqlite3_finalize(stmt);
    fprintf(file, "\n");
}

void escribir_seccion_json(cJSON *arr, const char *sql,
                           void (*escribir_objeto)(cJSON *item, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        escribir_objeto(item, stmt);
        cJSON_AddItemToArray(arr, item);
    }
    sqlite3_finalize(stmt);
}

void escribir_seccion_html(FILE *file, const char *titulo, const char *sql, const char *cabeceras[],
                           void (*escribir_fila)(FILE *file, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }
    fprintf(file, "<h2>%s</h2><table border='1'><tr>", titulo);
    for (int i = 0; cabeceras[i] != NULL; i++)
    {
        fprintf(file, "<th>%s</th>", cabeceras[i]);
    }
    fprintf(file, "</tr>");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        escribir_fila(file, stmt);
    }
    fprintf(file, "</table><br>\n");
    sqlite3_finalize(stmt);
}
