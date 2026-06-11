
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

static sqlite3_stmt* obtener_datos_camisetas(int *count)
{
    sqlite3_stmt *stmt;

    if (!preparar_consulta_con_verificacion(&stmt, "camiseta", "camisetas para exportar",
                                            "SELECT c.id, c.nombre, "
                                            "COALESCE(SUM(p.goles), 0) as total_goles, "
                                            "COALESCE(SUM(p.asistencias), 0) as total_asistencias, "
                                            "COUNT(p.id) as total_partidos, "
                                            "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                                            "COUNT(CASE WHEN p.resultado = 2 THEN 1 END) as empates, "
                                            "COUNT(CASE WHEN p.resultado = 3 THEN 1 END) as derrotas, "
                                            "COALESCE((SELECT COUNT(*) FROM lesion l INNER JOIN partido p2 ON l.partido_id = p2.id WHERE p2.camiseta_id = c.id), 0) as total_lesiones, "
                                            "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                                            "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                                            "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio "
                                            "FROM camiseta c "
                                            "LEFT JOIN partido p ON c.id = p.camiseta_id "
                                            "GROUP BY c.id, c.nombre "
                                            "ORDER BY c.id",
                                            count))
    {
        return NULL;
    }

    return stmt;
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

static void write_csv_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "id,nombre,total_goles,total_asistencias,total_partidos,victorias,empates,derrotas,total_lesiones,rendimiento_promedio,cansancio_promedio,estado_animo_promedio\n");
}

static void write_csv_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f, "%d,%s,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 6),
            sqlite3_column_int(stmt, 7),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_double(stmt, 9),
            sqlite3_column_double(stmt, 10),
            sqlite3_column_double(stmt, 11));
}

static void write_txt_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "LISTADO DE CAMISETAS CON ESTADISTICAS\n\n");
}

static void write_txt_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f, "ID: %d - Nombre: %s\n"
            "  Goles Totales: %d\n"
            "  Asistencias Totales: %d\n"
            "  Partidos Totales: %d\n"
            "  Victorias: %d\n"
            "  Empates: %d\n"
            "  Derrotas: %d\n"
            "  Lesiones Totales: %d\n"
            "  Rendimiento Promedio: %.2f\n"
            "  Cansancio Promedio: %.2f\n"
            "  Estado de Animo Promedio: %.2f\n\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 6),
            sqlite3_column_int(stmt, 7),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_double(stmt, 9),
            sqlite3_column_double(stmt, 10),
            sqlite3_column_double(stmt, 11));
}

static void write_json_row(FILE *f, sqlite3_stmt *stmt, void *context) /* NOSONAR */
{
    (void)f;
    cJSON *root = (cJSON *)context;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "total_goles", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "total_asistencias", sqlite3_column_int(stmt, 3));
    cJSON_AddNumberToObject(item, "total_partidos", sqlite3_column_int(stmt, 4));
    cJSON_AddNumberToObject(item, "victorias", sqlite3_column_int(stmt, 5));
    cJSON_AddNumberToObject(item, "empates", sqlite3_column_int(stmt, 6));
    cJSON_AddNumberToObject(item, "derrotas", sqlite3_column_int(stmt, 7));
    cJSON_AddNumberToObject(item, "total_lesiones", sqlite3_column_int(stmt, 8));
    cJSON_AddNumberToObject(item, "rendimiento_promedio", sqlite3_column_double(stmt, 9));
    cJSON_AddNumberToObject(item, "cansancio_promedio", sqlite3_column_double(stmt, 10));
    cJSON_AddNumberToObject(item, "estado_animo_promedio", sqlite3_column_double(stmt, 11));
    cJSON_AddItemToArray(root, item);
}

static void write_html_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f,
            "<html><body><h1>Camisetas con Estadisticas</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Goles Totales</th><th>Asistencias Totales</th><th>Partidos Totales</th><th>Victorias</th><th>Empates</th><th>Derrotas</th><th>Lesiones Totales</th><th>Rendimiento Promedio</th><th>Cansancio Promedio</th><th>Estado de Animo Promedio</th></tr>");
}

static void write_html_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f,
            "<tr><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td><td>%.2f</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 6),
            sqlite3_column_int(stmt, 7),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_double(stmt, 9),
            sqlite3_column_double(stmt, 10),
            sqlite3_column_double(stmt, 11));
}

/** @} */

/** @name Funciones de exportacion de camisetas */
/** @{ */

EXPORT_FORMAT_ROWS(exportar_camisetas_csv, obtener_datos_camisetas, "camisetas.csv", NULL, write_csv_header, write_csv_row, NULL)
EXPORT_FORMAT_ROWS(exportar_camisetas_txt, obtener_datos_camisetas, "camisetas.txt", NULL, write_txt_header, write_txt_row, NULL)
EXPORT_FORMAT_ROWS(exportar_camisetas_json, obtener_datos_camisetas, "camisetas.json", cJSON_CreateArray(), NULL, write_json_row, export_write_json_footer)
EXPORT_FORMAT_ROWS(exportar_camisetas_html, obtener_datos_camisetas, "camisetas.html", NULL, write_html_header, write_html_row, export_write_html_footer)

void exportar_camisetas_all(void)
{
    ExportConfig configs[] =
    {
        { "camisetas.csv", NULL, write_csv_header, write_csv_row, NULL },
        { "camisetas.txt", NULL, write_txt_header, write_txt_row, NULL },
        { "camisetas.json", cJSON_CreateArray(), NULL, write_json_row, export_write_json_footer },
        { "camisetas.html", NULL, write_html_header, write_html_row, export_write_html_footer }
    };
    export_all_formats(obtener_datos_camisetas, configs, 4);
}

/** @} */ /* End of Doxygen group */
