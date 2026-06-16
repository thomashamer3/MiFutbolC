#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include "direct.h"
#endif
#include <string.h>

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
        fprintf(file, "%d,%s,%s,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
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
        fprintf(file, "%d - %s | %s | %s | %s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
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
        fprintf(file,
                "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
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

static void write_lesiones_csv_rows(FILE *f, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(f, "%d,%s,%s,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }
}

static void write_lesiones_txt_rows(FILE *f, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(f, "%d - %s | %s | %s | %s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }
}

static void write_lesiones_html_rows(FILE *f, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(f,
                "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }
}

static void write_lesiones_json_rows(FILE *f, sqlite3_stmt *stmt)
{
    cJSON *root = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        export_json_add_lesion_base_fields(item, stmt);
        cJSON_AddItemToArray(root, item);
    }
    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

/* ============================================================================
 * EXPORTACIoN LESIONES (4 formatos)
 * ============================================================================ */

void exportar_lesiones_csv()
{
    exportar_archivo_si_hay_registros("lesion",
                                      "lesiones para exportar",
                                      "lesiones.csv",
                                      "Error al crear el archivo CSV",
                                      "id,jugador,tipo,descripcion,fecha\n",
                                      write_lesiones_csv);
}

void exportar_lesiones_txt()
{
    exportar_archivo_si_hay_registros("lesion",
                                      "lesiones para exportar",
                                      "lesiones.txt",
                                      "Error al crear el archivo TXT",
                                      "LISTADO DE LESIONES\n\n",
                                      write_lesiones_txt);
}

void exportar_lesiones_json()
{
    exportar_archivo_si_hay_registros("lesion",
                                      "lesiones para exportar",
                                      "lesiones.json",
                                      "Error al crear el archivo JSON",
                                      NULL,
                                      write_lesiones_json);
}

void exportar_lesiones_html()
{
    if (!has_records("lesion"))
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones.html", "Error al crear el archivo HTML");
    if (!f)
    {
        return;
    }

    fprintf(f,
            "<html><body><h1>Lesiones</h1><table border='1'>"
            "<tr><th>ID</th><th>Jugador</th><th>Tipo</th><th>Descripcion</th><th>Fecha</th></tr>");

    write_lesiones_html(f);

    fprintf(f, "</table></body></html>");

    fclose(f);
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
    if (!preparar_stmt_export(&stmt, SQL_LESIONES)) return;

    FILE *f;

    /* CSV */
    f = abrir_archivo_exportacion("lesiones.csv", "Error al crear CSV");
    if (f)
    {
        fprintf(f, "id,jugador,tipo,descripcion,fecha\n");
        write_lesiones_csv_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.csv"));
        fclose(f);
    }

    sqlite3_reset(stmt);

    /* TXT */
    f = abrir_archivo_exportacion("lesiones.txt", "Error al crear TXT");
    if (f)
    {
        fprintf(f, "LISTADO DE LESIONES\n\n");
        write_lesiones_txt_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.txt"));
        fclose(f);
    }

    sqlite3_reset(stmt);

    /* JSON */
    f = abrir_archivo_exportacion("lesiones.json", "Error al crear JSON");
    if (f)
    {
        write_lesiones_json_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.json"));
        fclose(f);
    }

    sqlite3_reset(stmt);

    /* HTML */
    f = abrir_archivo_exportacion("lesiones.html", "Error al crear HTML");
    if (f)
    {
        fprintf(f,
                "<html><body><h1>Lesiones</h1><table border='1'>"
                "<tr><th>ID</th><th>Jugador</th><th>Tipo</th><th>Descripcion</th><th>Fecha</th></tr>");
        write_lesiones_html_rows(f, stmt);
        fprintf(f, "</table></body></html>");
        printf("Archivo exportado a: %s\n", get_export_path("lesiones.html"));
        fclose(f);
    }

    sqlite3_finalize(stmt);
}
