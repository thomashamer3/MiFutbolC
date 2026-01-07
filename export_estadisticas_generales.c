/**
 * @file export_estadisticas_generales.c
 * @brief Funciones de exportación de estadísticas
 */

#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CONSULTAS SQL ESTÁTICAS - Centralizadas para mantenimiento
 * ============================================================================ */

static const char *SQL_COUNT_PARTIDOS = "SELECT COUNT(*) FROM partido";

static const char *SQL_BEST_BY_METRIC =
    "SELECT c.nombre, %s FROM partido p "
    "JOIN camiseta c ON p.camiseta_id = c.id "
    "GROUP BY c.id ORDER BY 2 %s LIMIT 1";

static const char *SQL_STATS_MONTH =
    "SELECT substr(fecha_hora, 4, 7), c.nombre, COUNT(*), SUM(goles), SUM(asistencias), "
    "ROUND(AVG(goles), 2), ROUND(AVG(asistencias), 2) "
    "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
    "GROUP BY substr(fecha_hora, 4, 7), c.id "
    "ORDER BY substr(fecha_hora, 4, 7) DESC, SUM(goles) DESC";

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

/** @brief Obtiene top 1 por métrica (reutilizable) */
static int get_top_camiseta(const char *metric, const char *orderDir,
                            char *nombre, int *valor)
{
    sqlite3_stmt *stmt = NULL;
    char query[512];
    int result = 0;

    snprintf(query, sizeof(query), SQL_BEST_BY_METRIC, metric, orderDir);

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy(nombre, (const char *)sqlite3_column_text(stmt, 0));
            *valor = sqlite3_column_int(stmt, 1);
            result = 1;
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

/** @brief Escribe estadística en JSON */
static void json_write_stat(cJSON *json, const char *cat,
                            const char *nombre, int valor)
{
    cJSON *stat = cJSON_CreateObject();
    cJSON_AddStringToObject(stat, "camiseta", nombre);
    cJSON_AddNumberToObject(stat, "valor", valor);
    cJSON_AddItemToObject(json, cat, stat);
}

/** @brief Escribe estadísticas en formato CSV */
static void write_stats_csv(FILE *file, const char *category,
                            const char *metric, const char *order,
                            const char *label)
{
    char nombre[256];
    int valor;
    if (get_top_camiseta(metric, order, nombre, &valor))
    {
        fprintf(file, "%s,%s,%d\n", label, nombre, valor);
    }
}

/** @brief Escribe estadísticas en formato TXT */
static void write_stats_txt(FILE *file, const char *category,
                            const char *metric, const char *order,
                            const char *label)
{
    char nombre[256];
    int valor;
    if (get_top_camiseta(metric, order, nombre, &valor))
    {
        fprintf(file, "%s: %s (%d)\n", label, nombre, valor);
    }
}

/** @brief Escribe estadísticas en formato HTML */
static void write_stats_html(FILE *file, const char *category,
                             const char *metric, const char *order,
                             const char *label)
{
    char nombre[256];
    int valor;
    if (get_top_camiseta(metric, order, nombre, &valor))
    {
        fprintf(file, "<tr><td>%s</td><td>%s</td><td>%d</td></tr>\n", label, nombre, valor);
    }
}

/** @brief Construye objeto JSON con estadísticas generales */
static cJSON *json_build_estadisticas(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    char nombre[256];
    int valor;

    if (get_top_camiseta("SUM(goles)", "DESC", nombre, &valor))
        json_write_stat(root, "mas_goles", nombre, valor);
    if (get_top_camiseta("SUM(asistencias)", "DESC", nombre, &valor))
        json_write_stat(root, "mas_asistencias", nombre, valor);
    if (get_top_camiseta("COUNT(*)", "DESC", nombre, &valor))
        json_write_stat(root, "mas_partidos", nombre, valor);
    if (get_top_camiseta("SUM(goles+asistencias)", "DESC", nombre, &valor))
        json_write_stat(root, "mas_goles_asistencias", nombre, valor);
    if (get_top_camiseta("AVG(rendimiento_general)", "DESC", nombre, &valor))
        json_write_stat(root, "mejor_rendimiento", nombre, valor);
    if (get_top_camiseta("AVG(estado_animo)", "DESC", nombre, &valor))
        json_write_stat(root, "mejor_estado_animo", nombre, valor);
    if (get_top_camiseta("AVG(cansancio)", "ASC", nombre, &valor))
        json_write_stat(root, "menos_cansancio", nombre, valor);
    if (get_top_camiseta("SUM(CASE WHEN resultado=1 THEN 1 ELSE 0 END)", "DESC", nombre, &valor))
        json_write_stat(root, "mas_victorias", nombre, valor);
    if (get_top_camiseta("SUM(CASE WHEN resultado=2 THEN 1 ELSE 0 END)", "DESC", nombre, &valor))
        json_write_stat(root, "mas_empates", nombre, valor);
    if (get_top_camiseta("SUM(CASE WHEN resultado=3 THEN 1 ELSE 0 END)", "DESC", nombre, &valor))
        json_write_stat(root, "mas_derrotas", nombre, valor);

    return root;
}

/* ============================================================================
 * EXPORTACIÓN ESTADÍSTICAS GENERALES (4 formatos)
 * ============================================================================ */

void exportar_estadisticas_generales_csv(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    const char *path = get_export_path("estadisticas_generales.csv");
    FILE *file = fopen(path, "w");
    if (!file)
    {
        printf("Error CSV\n");
        return;
    }

    fprintf(file, "Categoria,Camiseta,Valor\n");

    write_stats_csv(file, "goles", "SUM(goles)", "DESC", "Mas Goles");
    write_stats_csv(file, "asistencias", "SUM(asistencias)", "DESC", "Mas Asistencias");
    write_stats_csv(file, "partidos", "COUNT(*)", "DESC", "Mas Partidos");
    write_stats_csv(file, "goles_asistencias", "SUM(goles+asistencias)", "DESC", "Mas Goles+Asistencias");
    write_stats_csv(file, "rendimiento", "AVG(rendimiento_general)", "DESC", "Mejor Rendimiento");
    write_stats_csv(file, "estado_animo", "AVG(estado_animo)", "DESC", "Mejor Estado Animo");
    write_stats_csv(file, "cansancio", "AVG(cansancio)", "ASC", "Menos Cansancio");
    write_stats_csv(file, "victorias", "SUM(CASE WHEN resultado=1 THEN 1 ELSE 0 END)", "DESC", "Mas Victorias");
    write_stats_csv(file, "empates", "SUM(CASE WHEN resultado=2 THEN 1 ELSE 0 END)", "DESC", "Mas Empates");
    write_stats_csv(file, "derrotas", "SUM(CASE WHEN resultado=3 THEN 1 ELSE 0 END)", "DESC", "Mas Derrotas");

    fclose(file);
    printf("Exportado: %s\n", path);
}

void exportar_estadisticas_generales_txt(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    const char *path = get_export_path("estadisticas_generales.txt");
    FILE *file = fopen(path, "w");
    if (!file)
    {
        printf("Error TXT\n");
        return;
    }

    fprintf(file, "ESTADISTICAS GENERALES\n======================\n\n");

    write_stats_txt(file, "goles", "SUM(goles)", "DESC", "Mas Goles");
    write_stats_txt(file, "asistencias", "SUM(asistencias)", "DESC", "Mas Asistencias");
    write_stats_txt(file, "partidos", "COUNT(*)", "DESC", "Mas Partidos");
    write_stats_txt(file, "goles_asistencias", "SUM(goles+asistencias)", "DESC", "Mas Goles+Asistencias");
    write_stats_txt(file, "rendimiento", "AVG(rendimiento_general)", "DESC", "Mejor Rendimiento");
    write_stats_txt(file, "estado_animo", "AVG(estado_animo)", "DESC", "Mejor Estado Animo");
    write_stats_txt(file, "cansancio", "AVG(cansancio)", "ASC", "Menos Cansancio");
    write_stats_txt(file, "victorias", "SUM(CASE WHEN resultado=1 THEN 1 ELSE 0 END)", "DESC", "Mas Victorias");
    write_stats_txt(file, "empates", "SUM(CASE WHEN resultado=2 THEN 1 ELSE 0 END)", "DESC", "Mas Empates");
    write_stats_txt(file, "derrotas", "SUM(CASE WHEN resultado=3 THEN 1 ELSE 0 END)", "DESC", "Mas Derrotas");

    fclose(file);
    printf("Exportado: %s\n", path);
}

void exportar_estadisticas_generales_json(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    const char *path = get_export_path("estadisticas_generales.json");
    FILE *file = fopen(path, "w");
    if (!file)
    {
        printf("Error JSON\n");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *stats = json_build_estadisticas();
    if (stats) cJSON_AddItemToObject(root, "estadisticas_generales", stats);

    char *json_str = cJSON_Print(root);
    fprintf(file, "%s", json_str);

    free(json_str);
    cJSON_Delete(root);
    fclose(file);
    printf("Exportado: %s\n", path);
}

void exportar_estadisticas_generales_html(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    const char *path = get_export_path("estadisticas_generales.html");
    FILE *file = fopen(path, "w");
    if (!file)
    {
        printf("Error HTML\n");
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n<html>\n<head><title>Estadisticas</title></head>\n");
    fprintf(file, "<body>\n<h1>Estadisticas Generales</h1>\n<table border='1'>\n");
    fprintf(file, "<tr><th>Categoria</th><th>Camiseta</th><th>Valor</th></tr>\n");

    write_stats_html(file, "goles", "SUM(goles)", "DESC", "Mas Goles");
    write_stats_html(file, "asistencias", "SUM(asistencias)", "DESC", "Mas Asistencias");
    write_stats_html(file, "partidos", "COUNT(*)", "DESC", "Mas Partidos");
    write_stats_html(file, "goles_asistencias", "SUM(goles+asistencias)", "DESC", "Mas Goles+Asistencias");
    write_stats_html(file, "rendimiento", "AVG(rendimiento_general)", "DESC", "Mejor Rendimiento");
    write_stats_html(file, "estado_animo", "AVG(estado_animo)", "DESC", "Mejor Estado Animo");
    write_stats_html(file, "cansancio", "AVG(cansancio)", "ASC", "Menos Cansancio");
    write_stats_html(file, "victorias", "SUM(CASE WHEN resultado=1 THEN 1 ELSE 0 END)", "DESC", "Mas Victorias");
    write_stats_html(file, "empates", "SUM(CASE WHEN resultado=2 THEN 1 ELSE 0 END)", "DESC", "Mas Empates");
    write_stats_html(file, "derrotas", "SUM(CASE WHEN resultado=3 THEN 1 ELSE 0 END)", "DESC", "Mas Derrotas");

    fprintf(file, "</table>\n</body>\n</html>\n");
    fclose(file);
    printf("Exportado: %s\n", path);
}

/* ============================================================================
 * EXPORTACIÓN POR MES
 * ============================================================================ */

void exportar_estadisticas_por_mes_csv(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.csv"), "w");
    if (!file)
    {
        printf("Error CSV\n");
        return;
    }

    fprintf(file, "Mes,Camiseta,Partidos,Goles,Asist,AvgG,AvgA\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_MONTH, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%s,%d,%d,%d,%.2f,%.2f\n",
                sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4), sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6));
    }

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.csv"));
}

void exportar_estadisticas_por_mes_txt(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.txt"), "w");
    if (!file)
    {
        printf("Error TXT\n");
        return;
    }

    fprintf(file, "ESTADISTICAS POR MES\n====================\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_MONTH, -1, &stmt, NULL);

    char current[8] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *month = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current, month) != 0)
        {
            strcpy(current, month);
            fprintf(file, "\n%s:\n", month);
        }

        fprintf(file, "  %s: %d partidos, %d goles, %d asistencias (Avg: %.2f/%.2f)\n",
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6));
    }

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.txt"));
}

/**
 * @brief Exporta estadísticas por mes en formato JSON
 */
void exportar_estadisticas_por_mes_json(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.json"), "w");
    if (!file)
    {
        printf("Error JSON\n");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_MONTH, -1, &stmt, NULL);

    char current[8] = "";
    cJSON *current_array = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *month = (const char *)sqlite3_column_text(stmt, 0);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int goles = sqlite3_column_int(stmt, 3);
        int asistencias = sqlite3_column_int(stmt, 4);
        double avg_goles = sqlite3_column_double(stmt, 5);
        double avg_asistencias = sqlite3_column_double(stmt, 6);

        if (strcmp(current, month) != 0)
        {
            if (current_array)
            {
                cJSON_AddItemToObject(root, current, current_array);
            }
            strcpy(current, month);
            current_array = cJSON_CreateArray();
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", camiseta);
        cJSON_AddNumberToObject(item, "partidos", partidos);
        cJSON_AddNumberToObject(item, "goles", goles);
        cJSON_AddNumberToObject(item, "asistencias", asistencias);
        cJSON_AddNumberToObject(item, "avg_goles", avg_goles);
        cJSON_AddNumberToObject(item, "avg_asistencias", avg_asistencias);
        cJSON_AddItemToArray(current_array, item);
    }

    if (current_array)
    {
        cJSON_AddItemToObject(root, current, current_array);
    }

    char *json_str = cJSON_Print(root);
    fprintf(file, "%s", json_str);

    free(json_str);
    cJSON_Delete(root);
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.json"));
}

/**
 * @brief Exporta estadísticas por mes en formato HTML
 */
void exportar_estadisticas_por_mes_html(void)
{
    if (!has_partidos())
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.html"), "w");
    if (!file)
    {
        printf("Error HTML\n");
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n<html>\n<head><title>Estadisticas por Mes</title></head>\n");
    fprintf(file, "<body>\n<h1>Estadisticas por Mes</h1>\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_STATS_MONTH, -1, &stmt, NULL);

    char current[8] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *month = (const char *)sqlite3_column_text(stmt, 0);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int goles = sqlite3_column_int(stmt, 3);
        int asistencias = sqlite3_column_int(stmt, 4);
        double avg_goles = sqlite3_column_double(stmt, 5);
        double avg_asistencias = sqlite3_column_double(stmt, 6);

        if (strcmp(current, month) != 0)
        {
            if (hay) fprintf(file, "</table><br>");
            fprintf(file, "<h2>%s</h2><table border='1'>", month);
            fprintf(file, "<tr><th>Camiseta</th><th>Partidos</th><th>Goles</th><th>Asistencias</th><th>Avg Goles</th><th>Avg Asistencias</th></tr>");
            strcpy(current, month);
        }

        fprintf(file,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td></tr>",
                camiseta, partidos, goles, asistencias, avg_goles, avg_asistencias);
        hay = 1;
    }

    if (hay) fprintf(file, "</table>");

    fprintf(file, "</body></html>\n");
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.html"));
}
