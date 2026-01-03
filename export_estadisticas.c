#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>
/**
 * @brief Exporta las estadísticas a un archivo CSV
 *
 * Crea un archivo CSV con las estadísticas agrupadas por camiseta,
 * incluyendo nombre, suma de goles, suma de asistencias, número de partidos, victorias, empates y derrotas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Camiseta,Goles,Asistencias,Partidos,Victorias,Empates,Derrotas\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s,%d,%d,%d,%d,%d,%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.csv"));
    fclose(f);
}

/**
 * @brief Exporta las estadísticas a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias y número de partidos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.txt"), "w");
    if (!f)
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s | G:%d A:%d P:%d V:%d E:%d D:%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.txt"));
    fclose(f);
}

/**
 * @brief Exporta las estadísticas a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias y número de partidos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateArray();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 0));
        cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 1));
        cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(item, "partidos", sqlite3_column_int(stmt, 3));
        cJSON_AddNumberToObject(item, "victorias", sqlite3_column_int(stmt, 4));
        cJSON_AddNumberToObject(item, "empates", sqlite3_column_int(stmt, 5));
        cJSON_AddNumberToObject(item, "derrotas", sqlite3_column_int(stmt, 6));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.json"));
    fclose(f);
}

/**
 * @brief Exporta las estadísticas a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias, número de partidos, victorias, empates y derrotas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Estadisticas</h1><table border='1'>"
            "<tr><th>Camiseta</th><th>Goles</th><th>Asistencias</th><th>Partidos</th><th>Victorias</th><th>Empates</th><th>Derrotas</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.html"));
    fclose(f);
}