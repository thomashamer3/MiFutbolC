#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>

/* ============================================================================
 * CONSULTAS SQL ESTÁTICAS - Centralizadas para mantenimiento
 * ============================================================================ */

static const char *SQL_COUNT_PARTIDOS = "SELECT COUNT(*) FROM partido";

static const char *SQL_STATS_BY_CAMISETA =
    "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
    "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
    "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
    "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
    "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
    "GROUP BY c.id";

static const char *SQL_STATS_BY_ANIO =
    "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias "
    "FROM partido p "
    "JOIN camiseta c ON p.camiseta_id = c.id "
    "GROUP BY anio, c.id "
    "ORDER BY anio DESC, total_goles DESC";

/* ============================================================================
 * HELPER ESTÁTICOS
 * ============================================================================ */

/** @brief Verifica si hay partidos para exportar */
static int has_partidos(void)
{
    sqlite3_stmt *stmt;
    int count = 0;
    int result = 0;

    if (sqlite3_prepare_v2(db, SQL_COUNT_PARTIDOS, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        result = count > 0;
    }

    return result;
}

/** @brief Escribe estadísticas en formato CSV */
static void write_stats_csv(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_CAMISETA, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%d,%d,%d,%d,%d,%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe estadísticas en formato TXT */
static void write_stats_txt(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_CAMISETA, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s | G:%d A:%d P:%d V:%d E:%d D:%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe estadísticas en formato HTML */
static void write_stats_html(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_CAMISETA, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe estadísticas en formato JSON */
static void write_stats_json(FILE *file)
{
    cJSON *root = cJSON_CreateArray();
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_CAMISETA, -1, &stmt, NULL);

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
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

/** @brief Escribe estadísticas por año en formato CSV */
static void write_stats_anio_csv(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_ANIO, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%s,%d,%d,%d,%.2f,%.2f\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe estadísticas por año en formato TXT */
static void write_stats_anio_txt(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_ANIO, -1, &stmt, NULL);

    char current_anio[5] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int total_goles = sqlite3_column_int(stmt, 3);
        int total_asistencias = sqlite3_column_int(stmt, 4);
        double avg_goles = sqlite3_column_double(stmt, 5);
        double avg_asistencias = sqlite3_column_double(stmt, 6);

        if (strcmp(current_anio, anio) != 0)
        {
            if (hay) fprintf(file, "\n");
            fprintf(file, "Anio: %s\n", anio);
            fprintf(file, "----------------------------------------\n");
            strcpy(current_anio, anio);
        }

        fprintf(file, "%-30s | PJ: %d | G: %d | A: %d | G/P: %.2f | A/P: %.2f\n",
                camiseta, partidos, total_goles, total_asistencias, avg_goles, avg_asistencias);
        hay = 1;
    }
}

/** @brief Escribe estadísticas por año en formato HTML */
static void write_stats_anio_html(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_ANIO, -1, &stmt, NULL);

    char current_anio[5] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int total_goles = sqlite3_column_int(stmt, 3);
        int total_asistencias = sqlite3_column_int(stmt, 4);
        double avg_goles = sqlite3_column_double(stmt, 5);
        double avg_asistencias = sqlite3_column_double(stmt, 6);

        if (strcmp(current_anio, anio) != 0)
        {
            if (hay) fprintf(file, "</table><br>");
            fprintf(file, "<h2>Anio: %s</h2><table border='1'>", anio);
            fprintf(file, "<tr><th>Camiseta</th><th>Partidos</th><th>Goles</th><th>Asistencias</th><th>G/P</th><th>A/P</th></tr>");
            strcpy(current_anio, anio);
        }

        fprintf(file,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td></tr>",
                camiseta, partidos, total_goles, total_asistencias, avg_goles, avg_asistencias);
        hay = 1;
    }

    if (hay) fprintf(file, "</table>");
}

/** @brief Escribe estadísticas por año en formato JSON */
static void write_stats_anio_json(FILE *file)
{
    cJSON *root = cJSON_CreateObject();
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_BY_ANIO, -1, &stmt, NULL);

    char current_anio[5] = "";
    cJSON *current_anio_array = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int total_goles = sqlite3_column_int(stmt, 3);
        int total_asistencias = sqlite3_column_int(stmt, 4);
        double avg_goles = sqlite3_column_double(stmt, 5);
        double avg_asistencias = sqlite3_column_double(stmt, 6);

        if (strcmp(current_anio, anio) != 0)
        {
            if (current_anio_array)
            {
                cJSON_AddItemToObject(root, current_anio, current_anio_array);
            }
            strcpy(current_anio, anio);
            current_anio_array = cJSON_CreateArray();
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", camiseta);
        cJSON_AddNumberToObject(item, "partidos", partidos);
        cJSON_AddNumberToObject(item, "total_goles", total_goles);
        cJSON_AddNumberToObject(item, "total_asistencias", total_asistencias);
        cJSON_AddNumberToObject(item, "avg_goles", avg_goles);
        cJSON_AddNumberToObject(item, "avg_asistencias", avg_asistencias);
        cJSON_AddItemToArray(current_anio_array, item);
    }

    if (current_anio_array)
    {
        cJSON_AddItemToObject(root, current_anio, current_anio_array);
    }

    char *json_string = cJSON_Print(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

/* ============================================================================
 * EXPORTACIÓN ESTADÍSTICAS (4 formatos)
 * ============================================================================ */

/**
 * @brief Exporta las estadísticas a un archivo CSV
 *
 * Crea un archivo CSV con las estadísticas agrupadas por camiseta,
 * incluyendo nombre, suma de goles, suma de asistencias, número de partidos, victorias, empates y derrotas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_csv()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.csv"), "w");
    if (!f)
    {
        printf("Error al crear el archivo CSV\n");
        return;
    }

    fprintf(f, "Camiseta,Goles,Asistencias,Partidos,Victorias,Empates,Derrotas\n");
    write_stats_csv(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.csv"));
}

/**
 * @brief Exporta las estadísticas a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias y número de partidos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_txt()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.txt"), "w");
    if (!f)
    {
        printf("Error al crear el archivo TXT\n");
        return;
    }

    write_stats_txt(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.txt"));
}

/**
 * @brief Exporta las estadísticas a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias y número de partidos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_json()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.json"), "w");
    if (!f)
    {
        printf("Error al crear el archivo JSON\n");
        return;
    }

    write_stats_json(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.json"));
}

/**
 * @brief Exporta las estadísticas a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias, número de partidos, victorias, empates y derrotas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_html()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.html"), "w");
    if (!f)
    {
        printf("Error al crear el archivo HTML\n");
        return;
    }

    fprintf(f,
            "<html><body><h1>Estadisticas</h1><table border='1'>"
            "<tr><th>Camiseta</th><th>Goles</th><th>Asistencias</th><th>Partidos</th><th>Victorias</th><th>Empates</th><th>Derrotas</th></tr>");

    write_stats_html(f);

    fprintf(f, "</table></body></html>");

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.html"));
}

/* ============================================================================
 * EXPORTACIÓN ESTADÍSTICAS POR AÑO (4 formatos)
 * ============================================================================ */

/**
 * @brief Exporta las estadísticas por año a un archivo CSV
 */
void exportar_estadisticas_por_anio_csv()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas por año para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas_por_anio.csv"), "w");
    if (!f)
    {
        printf("Error al crear el archivo CSV\n");
        return;
    }

    fprintf(f, "Anio,Camiseta,Partidos,Goles,Asistencias,Promedio Goles,Promedio Asistencias\n");
    write_stats_anio_csv(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.csv"));
}

/**
 * @brief Exporta las estadísticas por año a un archivo de texto plano
 */
void exportar_estadisticas_por_anio_txt()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas por año para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas_por_anio.txt"), "w");
    if (!f)
    {
        printf("Error al crear el archivo TXT\n");
        return;
    }

    write_stats_anio_txt(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.txt"));
}

/**
 * @brief Exporta las estadísticas por año a un archivo JSON
 */
void exportar_estadisticas_por_anio_json()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas por año para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas_por_anio.json"), "w");
    if (!f)
    {
        printf("Error al crear el archivo JSON\n");
        return;
    }

    write_stats_anio_json(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.json"));
}

/**
 * @brief Exporta las estadísticas por año a un archivo HTML
 */
void exportar_estadisticas_por_anio_html()
{
    if (!has_partidos())
    {
        printf("No hay registros de estadisticas por año para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas_por_anio.html"), "w");
    if (!f)
    {
        printf("Error al crear el archivo HTML\n");
        return;
    }

    fprintf(f, "<html><body><h1>Estadisticas por Anio</h1>");

    write_stats_anio_html(f);

    fprintf(f, "</body></html>");

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.html"));
}
