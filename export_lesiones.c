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
        cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(item, "jugador", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(item, "tipo", (const char *)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(item, "descripcion", (const char *)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

/* ============================================================================
 * EXPORTACIoN LESIONES (4 formatos)
 * ============================================================================ */

void exportar_lesiones_csv()
{
    if (!has_records("lesion"))
    {
        mostrar_no_hay_registros("lesiones para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones.csv", "Error al crear el archivo CSV");
    if (!f)
    {
        return;
    }

    fprintf(f, "id,jugador,tipo,descripcion,fecha\n");
    write_lesiones_csv(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.csv"));
}

void exportar_lesiones_txt()
{
    if (!has_records("lesion"))
    {
        mostrar_no_hay_registros("lesiones para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones.txt", "Error al crear el archivo TXT");
    if (!f)
    {
        return;
    }

    fprintf(f, "LISTADO DE LESIONES\n\n");
    write_lesiones_txt(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.txt"));
}

void exportar_lesiones_json()
{
    if (!has_records("lesion"))
    {
        mostrar_no_hay_registros("lesiones para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones.json", "Error al crear el archivo JSON");
    if (!f)
    {
        return;
    }

    write_lesiones_json(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.json"));
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
