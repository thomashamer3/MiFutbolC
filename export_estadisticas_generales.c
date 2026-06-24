
#include "cJSON.h"
#include "db.h"
#include "export.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CONSULTAS SQL ESTaTICAS - Centralizadas para mantenimiento
 * ============================================================================ */

static const char *SQL_STATS_MONTH =
    "SELECT substr(fecha_hora, 4, 7), c.nombre, COUNT(*), SUM(goles), SUM(asistencias), "
    "ROUND(AVG(goles), 2), ROUND(AVG(asistencias), 2) "
    "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
    "GROUP BY substr(fecha_hora, 4, 7), c.id "
    "ORDER BY substr(fecha_hora, 4, 7) DESC, SUM(goles) DESC";

typedef struct
{
    const char *json_key;
    const char *metric;
    const char *order;
    const char *label;
} stat_def_t;

typedef struct
{
    const char *month;
    const char *camiseta;
    int partidos;
    int goles;
    int asistencias;
    double avg_goles;
    double avg_asistencias;
} month_stat_row_t;

static const stat_def_t STAT_DEFS[] =
{
    {"mas_goles", "SUM(goles)", "DESC", "Mas Goles"},
    {"mas_asistencias", "SUM(asistencias)", "DESC", "Mas Asistencias"},
    {"mas_partidos", "COUNT(*)", "DESC", "Mas Partidos"},
    {"mas_goles_asistencias", "SUM(goles+asistencias)", "DESC", "Mas Goles+Asistencias"},
    {"mejor_rendimiento", "AVG(rendimiento_general)", "DESC", "Mejor Rendimiento"},
    {"mejor_estado_animo", "AVG(estado_animo)", "DESC", "Mejor Estado Animo"},
    {"menos_cansancio", "AVG(cansancio)", "ASC", "Menos Cansancio"},
    {"mas_victorias", "SUM(CASE WHEN resultado=1 THEN 1 ELSE 0 END)", "DESC", "Mas Victorias"},
    {"mas_empates", "SUM(CASE WHEN resultado=2 THEN 1 ELSE 0 END)", "DESC", "Mas Empates"},
    {"mas_derrotas", "SUM(CASE WHEN resultado=3 THEN 1 ELSE 0 END)", "DESC", "Mas Derrotas"}
};

#define STAT_DEFS_COUNT (sizeof(STAT_DEFS) / sizeof(STAT_DEFS[0]))

/* ============================================================================
 * HELPER ESTaTICOS
 * ============================================================================ */

static int get_top_camiseta(const char *metric, const char *orderDir, char *nombre,
                            size_t nombre_size, int *valor)
{
    sqlite3_stmt *stmt = NULL;
    char query[512];
    int result = 0;

    snprintf(query, sizeof(query),
             "SELECT c.nombre, %s FROM partido p "
             "JOIN camiseta c ON p.camiseta_id = c.id "
             "GROUP BY c.id ORDER BY 2 %s LIMIT 1",
             metric, orderDir);

    if (preparar_stmt_export(&stmt, query))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy_s(nombre, nombre_size, (const char *)sqlite3_column_text(stmt, 0));
            *valor = sqlite3_column_int(stmt, 1);
            result = 1;
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

static void read_month_stat_row(sqlite3_stmt *stmt, month_stat_row_t *row)
{
    row->month = (const char *)sqlite3_column_text(stmt, 0);
    row->camiseta = (const char *)sqlite3_column_text(stmt, 1);
    row->partidos = sqlite3_column_int(stmt, 2);
    row->goles = sqlite3_column_int(stmt, 3);
    row->asistencias = sqlite3_column_int(stmt, 4);
    row->avg_goles = sqlite3_column_double(stmt, 5);
    row->avg_asistencias = sqlite3_column_double(stmt, 6);
}

static void json_write_stat(cJSON *json, const char *cat, const char *nombre, int valor)
{
    cJSON *stat = cJSON_CreateObject();
    cJSON_AddStringToObject(stat, "camiseta", nombre);
    cJSON_AddNumberToObject(stat, "valor", valor);
    cJSON_AddItemToObject(json, cat, stat);
}

static void write_stats_csv(FILE *file, const stat_def_t *def)
{
    char nombre[256];
    int valor;
    if (get_top_camiseta(def->metric, def->order, nombre, sizeof(nombre), &valor))
    {
        fprintf(file, "%s,%s,%d\n", def->label, nombre, valor);
    }
}

static void write_stats_txt(FILE *file, const stat_def_t *def)
{
    char nombre[256];
    int valor;
    if (get_top_camiseta(def->metric, def->order, nombre, sizeof(nombre), &valor))
    {
        fprintf(file, "%s: %s (%d)\n", def->label, nombre, valor);
    }
}

static void write_stats_html(FILE *file, const stat_def_t *def)
{
    char nombre[256];
    int valor;
    if (get_top_camiseta(def->metric, def->order, nombre, sizeof(nombre), &valor))
    {
        fprintf(file, "<tr><td>%s</td><td>%s</td><td>%d</td></tr>\n", def->label, nombre, valor);
    }
}

static void write_stats_md(FILE *file, const stat_def_t *def)
{
    char nombre[256];
    int valor;
    if (get_top_camiseta(def->metric, def->order, nombre, sizeof(nombre), &valor))
    {
        fprintf(file, "| %s | %s | %d |\n", def->label, nombre, valor);
    }
}

static cJSON *json_build_estadisticas(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return NULL;
    }

    char nombre[256];
    int valor;
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        if (get_top_camiseta(STAT_DEFS[i].metric, STAT_DEFS[i].order, nombre, sizeof(nombre),
                             &valor))
        {
            json_write_stat(root, STAT_DEFS[i].json_key, nombre, valor);
        }
    }

    return root;
}

/* ============================================================================
 * EXPORTACIoN ESTADiSTICAS GENERALES (4 formatos)
 * ============================================================================ */

void exportar_estadisticas_generales_csv(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_generales.csv", "Error CSV");
    if (!file)
    {
        return;
    }

    fprintf(file, "Categoria,Camiseta,Valor\n");
    {
        for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
        {
            write_stats_csv(file, &STAT_DEFS[i]);
        }
    }

    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_generales.csv"));
}

void exportar_estadisticas_generales_txt(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_generales.txt", "Error TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "ESTADISTICAS GENERALES\n======================\n\n");
    {
        for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
        {
            write_stats_txt(file, &STAT_DEFS[i]);
        }
    }

    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_generales.txt"));
}

void exportar_estadisticas_generales_json(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_generales.json", "Error JSON");
    if (!file)
    {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *stats = json_build_estadisticas();
    if (stats)
    {
        cJSON_AddItemToObject(root, "estadisticas_generales", stats);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_str);

    free(json_str);
    cJSON_Delete(root);
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_generales.json"));
}

void exportar_estadisticas_generales_html(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_generales.html", "Error HTML");
    if (!file)
    {
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n<html>\n<head><title>Estadisticas</title></head>\n");
    fprintf(file, "<body>\n<h1>Estadisticas Generales</h1>\n<table border='1'>\n");
    fprintf(file, "<tr><th>Categoria</th><th>Camiseta</th><th>Valor</th></tr>\n");
    {
        for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
        {
            write_stats_html(file, &STAT_DEFS[i]);
        }
    }

    fprintf(file, "</table>\n</body>\n</html>\n");
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_generales.html"));
}

void exportar_estadisticas_generales_md(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_generales.md", "Error Markdown");
    if (!file)
    {
        return;
    }

    fprintf(file, "# Estadisticas Generales\n\n");
    fprintf(file, "*Generado por MiFutbolC*\n\n");
    fprintf(file, "| Categoria | Camiseta | Valor |\n");
    fprintf(file, "|-----------|----------|-------|\n");
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        write_stats_md(file, &STAT_DEFS[i]);
        fprintf(file, "\n");
    }

    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_generales.md"));
}

/* ============================================================================
 * EXPORTACIoN POR MES
 * ============================================================================ */

void exportar_estadisticas_por_mes_csv(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_por_mes.csv", "Error CSV");
    if (!file)
    {
        return;
    }

    fprintf(file, "Mes,Camiseta,Partidos,Goles,Asist,AvgG,AvgA\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_MONTH))
    {
        fclose(file);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%s,%d,%d,%d,%.2f,%.2f\n", sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 6));
    }

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.csv"));
}

void exportar_estadisticas_por_mes_txt(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_por_mes.txt", "Error TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "ESTADISTICAS POR MES\n====================\n\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_MONTH))
    {
        fclose(file);
        return;
    }

    char current[8] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *month = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current, month) != 0)
        {
            strcpy_s(current, sizeof(current), month);
            fprintf(file, "\n%s:\n", month);
        }

        fprintf(file, "  %s: %d partidos, %d goles, %d asistencias (Avg: %.2f/%.2f)\n",
                sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 6));
    }

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.txt"));
}

void exportar_estadisticas_por_mes_json(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_por_mes.json", "Error JSON");
    if (!file)
    {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_MONTH))
    {
        cJSON_Delete(root);
        fclose(file);
        return;
    }

    char current[8] = "";
    cJSON *current_array = NULL;
    month_stat_row_t row;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        read_month_stat_row(stmt, &row);

        if (strcmp(current, row.month) != 0)
        {
            if (current_array)
            {
                cJSON_AddItemToObject(root, current, current_array);
            }
            strcpy_s(current, sizeof(current), row.month);
            current_array = cJSON_CreateArray();
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", row.camiseta);
        cJSON_AddNumberToObject(item, "partidos", row.partidos);
        cJSON_AddNumberToObject(item, "goles", row.goles);
        cJSON_AddNumberToObject(item, "asistencias", row.asistencias);
        cJSON_AddNumberToObject(item, "avg_goles", row.avg_goles);
        cJSON_AddNumberToObject(item, "avg_asistencias", row.avg_asistencias);
        cJSON_AddItemToArray(current_array, item);
    }

    if (current_array)
    {
        cJSON_AddItemToObject(root, current, current_array);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_str);

    free(json_str);
    cJSON_Delete(root);
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.json"));
}

void exportar_estadisticas_por_mes_html(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    FILE *file = abrir_archivo_exportacion("estadisticas_por_mes.html", "Error HTML");
    if (!file)
    {
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n<html>\n<head><title>Estadisticas por Mes</title></head>\n");
    fprintf(file, "<body>\n<h1>Estadisticas por Mes</h1>\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_MONTH))
    {
        fclose(file);
        return;
    }

    char current[8] = "";
    int hay = 0;
    month_stat_row_t row;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        read_month_stat_row(stmt, &row);

        if (strcmp(current, row.month) != 0)
        {
            if (hay)
            {
                fprintf(file, "</table><br>");
            }
            fprintf(file, "<h2>%s</h2><table border='1'>", row.month);
            fprintf(file, "<tr><th>Camiseta</th><th>Partidos</th><th>Goles</th><th>Asistencias</"
                    "th><th>Avg Goles</th><th>Avg Asistencias</th></tr>");
            strcpy_s(current, sizeof(current), row.month);
        }

        fprintf(file,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td></tr>",
                row.camiseta, row.partidos, row.goles, row.asistencias, row.avg_goles,
                row.avg_asistencias);
        hay = 1;
    }

    if (hay)
    {
        fprintf(file, "</table>");
    }

    fprintf(file, "</body></html>\n");
    fclose(file);
    printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.html"));
}

/* ============================================================================
 * HELPERS BATCH (reutilizan stats precalculados / stmt externo)
 * ============================================================================ */

static struct
{
    char nombre[256];
    int valor;
    int found;
} g_cached_stats[STAT_DEFS_COUNT];

static void cache_all_stats(void)
{
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        g_cached_stats[i].found =
            get_top_camiseta(STAT_DEFS[i].metric, STAT_DEFS[i].order, g_cached_stats[i].nombre,
                             sizeof(g_cached_stats[i].nombre), &g_cached_stats[i].valor);
    }
}

static void write_cached_csv(FILE *file)
{
    fprintf(file, "Categoria,Camiseta,Valor\n");
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        if (g_cached_stats[i].found)
        {
            fprintf(file, "%s,%s,%d\n", STAT_DEFS[i].label, g_cached_stats[i].nombre,
                    g_cached_stats[i].valor);
        }
    }
}

static void write_cached_txt(FILE *file)
{
    fprintf(file, "ESTADISTICAS GENERALES\n======================\n\n");
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        if (g_cached_stats[i].found)
        {
            fprintf(file, "%s: %s (%d)\n", STAT_DEFS[i].label, g_cached_stats[i].nombre,
                    g_cached_stats[i].valor);
        }
    }
}

static void write_cached_json(FILE *file)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *stats = cJSON_CreateObject();
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        if (g_cached_stats[i].found)
        {
            cJSON *stat = cJSON_CreateObject();
            cJSON_AddStringToObject(stat, "camiseta", g_cached_stats[i].nombre);
            cJSON_AddNumberToObject(stat, "valor", g_cached_stats[i].valor);
            cJSON_AddItemToObject(stats, STAT_DEFS[i].json_key, stat);
        }
    }
    cJSON_AddItemToObject(root, "estadisticas_generales", stats);
    char *json_str = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_str);
    free(json_str);
    cJSON_Delete(root);
}

static void write_cached_html(FILE *file)
{
    fprintf(file, "<!DOCTYPE html>\n<html>\n<head><title>Estadisticas</title></head>\n");
    fprintf(file, "<body>\n<h1>Estadisticas Generales</h1>\n<table border='1'>\n");
    fprintf(file, "<tr><th>Categoria</th><th>Camiseta</th><th>Valor</th></tr>\n");
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        if (g_cached_stats[i].found)
        {
            fprintf(file, "<tr><td>%s</td><td>%s</td><td>%d</td></tr>\n", STAT_DEFS[i].label,
                    g_cached_stats[i].nombre, g_cached_stats[i].valor);
        }
    }
    fprintf(file, "</table>\n</body>\n</html>\n");
}

static void write_cached_md(FILE *file)
{
    fprintf(file, "# Estadisticas Generales\n\n*Generado por MiFutbolC*\n\n");
    fprintf(file, "| Categoria | Camiseta | Valor |\n|-----------|----------|-------|\n");
    for (size_t i = 0; i < STAT_DEFS_COUNT; ++i)
    {
        if (g_cached_stats[i].found)
        {
            fprintf(file, "| %s | %s | %d |\n", STAT_DEFS[i].label, g_cached_stats[i].nombre,
                    g_cached_stats[i].valor);
        }
    }
    fprintf(file, "\n");
}

static void stats_mes_csv_rows(FILE *file, sqlite3_stmt *stmt)
{
    fprintf(file, "Mes,Camiseta,Partidos,Goles,Asist,AvgG,AvgA\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%s,%d,%d,%d,%.2f,%.2f\n", sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 6));
    }
}

static void stats_mes_txt_rows(FILE *file, sqlite3_stmt *stmt)
{
    fprintf(file, "ESTADISTICAS POR MES\n====================\n\n");
    char current[8] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *month = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current, month) != 0)
        {
            strcpy_s(current, sizeof(current), month);
            fprintf(file, "\n%s:\n", month);
        }
        fprintf(file, "  %s: %d partidos, %d goles, %d asistencias (Avg: %.2f/%.2f)\n",
                sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 6));
    }
}

static void stats_mes_json_rows(FILE *file, sqlite3_stmt *stmt)
{
    cJSON *root = cJSON_CreateObject();
    char current[8] = "";
    cJSON *current_array = NULL;
    month_stat_row_t row;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        read_month_stat_row(stmt, &row);
        if (strcmp(current, row.month) != 0)
        {
            if (current_array)
            {
                cJSON_AddItemToObject(root, current, current_array);
            }
            strcpy_s(current, sizeof(current), row.month);
            current_array = cJSON_CreateArray();
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", row.camiseta);
        cJSON_AddNumberToObject(item, "partidos", row.partidos);
        cJSON_AddNumberToObject(item, "goles", row.goles);
        cJSON_AddNumberToObject(item, "asistencias", row.asistencias);
        cJSON_AddNumberToObject(item, "avg_goles", row.avg_goles);
        cJSON_AddNumberToObject(item, "avg_asistencias", row.avg_asistencias);
        cJSON_AddItemToArray(current_array, item);
    }
    if (current_array)
    {
        cJSON_AddItemToObject(root, current, current_array);
    }
    char *json_str = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_str);
    free(json_str);
    cJSON_Delete(root);
}

static void stats_mes_html_rows(FILE *file, sqlite3_stmt *stmt)
{
    fprintf(file, "<!DOCTYPE html>\n<html>\n<head><title>Estadisticas por Mes</title></head>\n");
    fprintf(file, "<body>\n<h1>Estadisticas por Mes</h1>\n");
    char current[8] = "";
    int hay = 0;
    month_stat_row_t row;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        read_month_stat_row(stmt, &row);
        if (strcmp(current, row.month) != 0)
        {
            if (hay)
            {
                fprintf(file, "</table><br>");
            }
            fprintf(file, "<h2>%s</h2><table border='1'>", row.month);
            fprintf(file, "<tr><th>Camiseta</th><th>Partidos</th><th>Goles</th><th>Asistencias</"
                    "th><th>Avg Goles</th><th>Avg Asistencias</th></tr>");
            strcpy_s(current, sizeof(current), row.month);
        }
        fprintf(file,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td></tr>",
                row.camiseta, row.partidos, row.goles, row.asistencias, row.avg_goles,
                row.avg_asistencias);
        hay = 1;
    }
    if (hay)
    {
        fprintf(file, "</table>");
    }
    fprintf(file, "</body></html>\n");
}

/* ============================================================================
 * EXPORTACIoN BATCH (SQL ejecutado una vez, todos los formatos)
 * ============================================================================ */

void exportar_estadisticas_generales_all(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    cache_all_stats();
    FILE *file;

    file = abrir_archivo_exportacion("estadisticas_generales.csv", "Error CSV");
    if (file)
    {
        write_cached_csv(file);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_generales.csv"));
    }

    file = abrir_archivo_exportacion("estadisticas_generales.txt", "Error TXT");
    if (file)
    {
        write_cached_txt(file);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_generales.txt"));
    }

    file = abrir_archivo_exportacion("estadisticas_generales.json", "Error JSON");
    if (file)
    {
        write_cached_json(file);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_generales.json"));
    }

    file = abrir_archivo_exportacion("estadisticas_generales.html", "Error HTML");
    if (file)
    {
        write_cached_html(file);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_generales.html"));
    }

    file = abrir_archivo_exportacion("estadisticas_generales.md", "Error MD");
    if (file)
    {
        write_cached_md(file);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_generales.md"));
    }
}

void exportar_estadisticas_por_mes_all(void)
{
    if (!has_records("partido"))
    {
        printf("No hay registros.\n");
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_MONTH))
    {
        return;
    }
    FILE *file;

    file = abrir_archivo_exportacion("estadisticas_por_mes.csv", "Error CSV");
    if (file)
    {
        stats_mes_csv_rows(file, stmt);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.csv"));
    }

    sqlite3_reset(stmt);
    file = abrir_archivo_exportacion("estadisticas_por_mes.txt", "Error TXT");
    if (file)
    {
        stats_mes_txt_rows(file, stmt);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.txt"));
    }

    sqlite3_reset(stmt);
    file = abrir_archivo_exportacion("estadisticas_por_mes.json", "Error JSON");
    if (file)
    {
        stats_mes_json_rows(file, stmt);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.json"));
    }

    sqlite3_reset(stmt);
    file = abrir_archivo_exportacion("estadisticas_por_mes.html", "Error HTML");
    if (file)
    {
        stats_mes_html_rows(file, stmt);
        fclose(file);
        printf("Exportado: %s\n", get_export_path("estadisticas_por_mes.html"));
    }

    sqlite3_finalize(stmt);
}
