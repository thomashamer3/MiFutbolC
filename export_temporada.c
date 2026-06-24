#include "cJSON.h"
#include "db.h"
#include "export.h"
#include "utils.h"
#include <stdio.h>

static sqlite3_stmt *obtener_datos_temporadas(int *count)
{
    sqlite3_stmt *stmt;

    if (!preparar_consulta_con_verificacion(&stmt, "temporada", "temporadas para exportar",
                                            "SELECT id, nombre, fecha_inicio, fecha_fin, "
                                            "presupuesto_total, estado, objetivos_cumplidos "
                                            "FROM temporada ORDER BY id",
                                            count))
    {
        return NULL;
    }

    return stmt;
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

static void write_csv_header(FILE *file, void *context)
{
    (void)context;
    fprintf(file,
            "id,nombre,fecha_inicio,fecha_fin,presupuesto_total,estado,objetivos_cumplidos\n");
}

static void write_csv_row(FILE *file, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(file, "%d,%s,%s,%s,%.2f,%d,%d\n", sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3), sqlite3_column_double(stmt, 4),
            sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
}

static void write_txt_header(FILE *file, void *context)
{
    (void)context;
    fprintf(file, "LISTADO DE TEMPORADAS\n\n");
}

static const char *estado_to_text(int estado)
{
    switch (estado)
    {
    case 0:
        return "Activa";
    case 1:
        return "Finalizada";
    case 2:
        return "Planificada";
    default:
        return "Desconocido";
    }
}

static void write_txt_row(FILE *file, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    int estado = sqlite3_column_int(stmt, 5);
    fprintf(file,
            "ID: %d - Nombre: %s\n"
            "  Fecha Inicio: %s\n"
            "  Fecha Fin: %s\n"
            "  Presupuesto Total: %.2f\n"
            "  Estado: %s\n"
            "  Objetivos Cumplidos: %d\n\n",
            sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3), sqlite3_column_double(stmt, 4), estado_to_text(estado),
            sqlite3_column_int(stmt, 6));
}

static void write_json_row(FILE *file, sqlite3_stmt *stmt, void *context) /* NOSONAR */
{
    (void)file;
    cJSON *root = (cJSON *)context;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "fecha_inicio", (const char *)sqlite3_column_text(stmt, 2));
    cJSON_AddStringToObject(item, "fecha_fin", (const char *)sqlite3_column_text(stmt, 3));
    cJSON_AddNumberToObject(item, "presupuesto_total", sqlite3_column_double(stmt, 4));
    cJSON_AddNumberToObject(item, "estado", sqlite3_column_int(stmt, 5));
    cJSON_AddNumberToObject(item, "objetivos_cumplidos", sqlite3_column_int(stmt, 6));
    cJSON_AddItemToArray(root, item);
}

static void write_html_header(FILE *file, void *context)
{
    (void)context;
    fprintf(file, "<html><body><h1>Temporadas</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Fecha Inicio</th><th>Fecha Fin</th>"
            "<th>Presupuesto Total</th><th>Estado</th><th>Objetivos Cumplidos</th></tr>");
}

static void write_html_row(FILE *file, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    int estado = sqlite3_column_int(stmt, 5);
    fprintf(
        file,
        "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%.2f</td><td>%s</td><td>%d</td></tr>",
        sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2),
        sqlite3_column_text(stmt, 3), sqlite3_column_double(stmt, 4), estado_to_text(estado),
        sqlite3_column_int(stmt, 6));
}

/** @} */

/** @name Funciones de exportacion de temporadas */
/** @{ */

EXPORT_FORMAT_ROWS(exportar_temporadas_csv, obtener_datos_temporadas, "temporadas.csv", NULL,
                   write_csv_header, write_csv_row, NULL)
EXPORT_FORMAT_ROWS(exportar_temporadas_txt, obtener_datos_temporadas, "temporadas.txt", NULL,
                   write_txt_header, write_txt_row, NULL)
EXPORT_FORMAT_ROWS(exportar_temporadas_json, obtener_datos_temporadas, "temporadas.json",
                   cJSON_CreateArray(), NULL, write_json_row, export_write_json_footer)
EXPORT_FORMAT_ROWS(exportar_temporadas_html, obtener_datos_temporadas, "temporadas.html", NULL,
                   write_html_header, write_html_row, export_write_html_footer)

void exportar_temporadas_all(void)
{
    ExportConfig configs[] =
    {
        {"temporadas.csv", NULL, write_csv_header, write_csv_row, NULL},
        {"temporadas.txt", NULL, write_txt_header, write_txt_row, NULL},
        {"temporadas.json", cJSON_CreateArray(), NULL, write_json_row, export_write_json_footer},
        {"temporadas.html", NULL, write_html_header, write_html_row, export_write_html_footer}
    };
    export_all_formats(obtener_datos_temporadas, configs, 4);
}

/** @} */
