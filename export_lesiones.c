#include "cJSON.h"
#include "db.h"
#include "export.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * CONSULTAS SQL ESTaTICAS - Centralizadas para mantenimiento
 * ============================================================================ */

static const char *SQL_LESIONES = "SELECT id, jugador, tipo, descripcion, fecha FROM lesion";

/* ============================================================================
 * HELPER ESTaTICOS
 * ============================================================================ */

static void write_lesiones_csv(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s,%s,%s\n", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }

    sqlite3_finalize(stmt);
}

static void write_lesiones_txt(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d - %s | %s | %s | %s\n", sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3), sqlite3_column_text(stmt, 4));
    }

    sqlite3_finalize(stmt);
}

static void write_lesiones_html(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }

    sqlite3_finalize(stmt);
}

static void write_lesiones_json(FILE *file)
{
    cJSON *root = cJSON_CreateArray();
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES))
    {
        cJSON_Delete(root);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        export_json_add_lesion_base_fields(item, stmt);
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

/* ============================================================================
 * HELPERS DE FILA (reutilizan stmt externo, sin prepare/finalize)
 * ============================================================================ */

static void write_lesiones_csv_rows(FILE *file, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s,%s,%s\n", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }
}

static void write_lesiones_txt_rows(FILE *file, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d - %s | %s | %s | %s\n", sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3), sqlite3_column_text(stmt, 4));
    }
}

static void write_lesiones_html_rows(FILE *file, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }
}

static void write_lesiones_json_rows(FILE *file, sqlite3_stmt *stmt)
{
    cJSON *root = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        export_json_add_lesion_base_fields(item, stmt);
        cJSON_AddItemToArray(root, item);
    }
    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

/* ============================================================================
 * EXPORTACIoN LESIONES (4 formatos)
 * ============================================================================ */

void exportar_lesiones_csv(void)
{
    exportar_archivo_si_hay_registros("lesion", "lesiones para exportar", "lesiones.csv",
                                      "Error al crear el archivo CSV",
                                      "id,jugador,tipo,descripcion,fecha\n", write_lesiones_csv);
}

void exportar_lesiones_txt(void)
{
    exportar_archivo_si_hay_registros("lesion", "lesiones para exportar", "lesiones.txt",
                                      "Error al crear el archivo TXT", "LISTADO DE LESIONES\n\n",
                                      write_lesiones_txt);
}

void exportar_lesiones_json(void)
{
    exportar_archivo_si_hay_registros("lesion", "lesiones para exportar", "lesiones.json",
                                      "Error al crear el archivo JSON", NULL, write_lesiones_json);
}

void exportar_lesiones_html(void)
{
    if (!has_records("lesion"))
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("lesiones.html", "Error al crear el archivo HTML");
    if (!file)
    {
        return;
    }

    export_write_html_begin(file, "Lesiones");
    fprintf(file, "<table>\n<tr><th>ID</th><th>Jugador</th><th>Tipo</th><th>Descripcion</th><th>Fecha</th></tr>");

    write_lesiones_html(file);

    export_write_html_table_footer(file, NULL);

    fclose(file);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.html"));
}

void exportar_lesiones_all(void)
{
    if (!has_records("lesion"))
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES))
    {
        return;
    }

    FILE *file;

    /* CSV */
    file = abrir_archivo_exportacion("lesiones.csv", "Error al crear CSV");
    if (file)
    {
        fprintf(file, "id,jugador,tipo,descripcion,fecha\n");
        write_lesiones_csv_rows(file, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.csv"));
        fclose(file);
    }

    sqlite3_reset(stmt);

    /* TXT */
    file = abrir_archivo_exportacion("lesiones.txt", "Error al crear TXT");
    if (file)
    {
        fprintf(file, "LISTADO DE LESIONES\n\n");
        write_lesiones_txt_rows(file, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.txt"));
        fclose(file);
    }

    sqlite3_reset(stmt);

    /* JSON */
    file = abrir_archivo_exportacion("lesiones.json", "Error al crear JSON");
    if (file)
    {
        write_lesiones_json_rows(file, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.json"));
        fclose(file);
    }

    sqlite3_reset(stmt);

    /* HTML */
    file = abrir_archivo_exportacion("lesiones.html", "Error al crear HTML");
    if (file)
    {
        export_write_html_begin(file, "Lesiones");
        fprintf(file, "<table>\n<tr><th>ID</th><th>Jugador</th><th>Tipo</th><th>Descripcion</th><th>Fecha</th></tr>");
        write_lesiones_html_rows(file, stmt);
        export_write_html_table_footer(file, NULL);
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.html"));
        fclose(file);
    }

    sqlite3_finalize(stmt);
}
