/**
 * @file export_camisetas.c
 * @brief Funciones para exportar datos de camisetas a diferentes formatos
 *
 * Este archivo contiene funciones para exportar datos de camisetas
 * a formatos CSV, TXT, JSON y HTML.
 */

#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>

/** @name Funciones de exportación de camisetas */
/** @{ */

/**
 * @brief Exporta las camisetas a un archivo CSV
 *
 * Crea un archivo CSV con todas las camisetas registradas en la base de datos,
 * incluyendo ID, nombre y estadísticas agregadas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.csv"), "w");
    if (!f)
        return;

    fprintf(f, "id,nombre,total_goles,total_asistencias,total_partidos,victorias,empates,derrotas,total_lesiones,rendimiento_promedio,cansancio_promedio,estado_animo_promedio\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.id, c.nombre, "
                       "COALESCE(SUM(p.goles), 0) as total_goles, "
                       "COALESCE(SUM(p.asistencias), 0) as total_asistencias, "
                       "COUNT(p.id) as total_partidos, "
                       "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                       "COUNT(CASE WHEN p.resultado = 0 THEN 1 END) as empates, "
                       "COUNT(CASE WHEN p.resultado = -1 THEN 1 END) as derrotas, "
                       "COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) as total_lesiones, "
                       "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                       "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                       "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio "
                       "FROM camiseta c "
                       "LEFT JOIN partido p ON c.id = p.camiseta_id "
                       "GROUP BY c.id, c.nombre "
                       "ORDER BY c.id", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
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

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.csv"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de todas las camisetas
 * registradas, incluyendo ID, nombre y estadísticas agregadas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.txt"), "w");
    if (!f)
        return;

    fprintf(f, "LISTADO DE CAMISETAS CON ESTADISTICAS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.id, c.nombre, "
                       "COALESCE(SUM(p.goles), 0) as total_goles, "
                       "COALESCE(SUM(p.asistencias), 0) as total_asistencias, "
                       "COUNT(p.id) as total_partidos, "
                       "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                       "COUNT(CASE WHEN p.resultado = 0 THEN 1 END) as empates, "
                       "COUNT(CASE WHEN p.resultado = -1 THEN 1 END) as derrotas, "
                       "COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) as total_lesiones, "
                       "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                       "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                       "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio "
                       "FROM camiseta c "
                       "LEFT JOIN partido p ON c.id = p.camiseta_id "
                       "GROUP BY c.id, c.nombre "
                       "ORDER BY c.id", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
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

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.txt"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando todas las camisetas
 * registradas, incluyendo ID, nombre y estadísticas agregadas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateArray();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.id, c.nombre, "
                       "COALESCE(SUM(p.goles), 0) as total_goles, "
                       "COALESCE(SUM(p.asistencias), 0) as total_asistencias, "
                       "COUNT(p.id) as total_partidos, "
                       "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                       "COUNT(CASE WHEN p.resultado = 0 THEN 1 END) as empates, "
                       "COUNT(CASE WHEN p.resultado = -1 THEN 1 END) as derrotas, "
                       "COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) as total_lesiones, "
                       "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                       "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                       "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio "
                       "FROM camiseta c "
                       "LEFT JOIN partido p ON c.id = p.camiseta_id "
                       "GROUP BY c.id, c.nombre "
                       "ORDER BY c.id", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
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

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.json"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra todas las camisetas
 * registradas, incluyendo ID, nombre y estadísticas agregadas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Camisetas con Estadisticas</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Goles Totales</th><th>Asistencias Totales</th><th>Partidos Totales</th><th>Victorias</th><th>Empates</th><th>Derrotas</th><th>Lesiones Totales</th><th>Rendimiento Promedio</th><th>Cansancio Promedio</th><th>Estado de Animo Promedio</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.id, c.nombre, "
                       "COALESCE(SUM(p.goles), 0) as total_goles, "
                       "COALESCE(SUM(p.asistencias), 0) as total_asistencias, "
                       "COUNT(p.id) as total_partidos, "
                       "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                       "COUNT(CASE WHEN p.resultado = 0 THEN 1 END) as empates, "
                       "COUNT(CASE WHEN p.resultado = -1 THEN 1 END) as derrotas, "
                       "COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) as total_lesiones, "
                       "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                       "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                       "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio "
                       "FROM camiseta c "
                       "LEFT JOIN partido p ON c.id = p.camiseta_id "
                       "GROUP BY c.id, c.nombre "
                       "ORDER BY c.id", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
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

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.html"));
    fclose(f);
}

/** @} */ /* End of Doxygen group */
