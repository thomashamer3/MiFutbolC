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

static sqlite3_stmt* obtener_datos_dashboard(int *count)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "  (SELECT COUNT(*) FROM partido) AS total_partidos, "
        "  (SELECT COALESCE(SUM(goles), 0) FROM partido) AS total_goles, "
        "  (SELECT COALESCE(SUM(asistencias), 0) FROM partido) AS total_asistencias, "
        "  (SELECT COUNT(*) FROM equipo) AS total_equipos, "
        "  (SELECT COUNT(*) FROM camiseta) AS total_camisetas, "
        "  (SELECT COUNT(*) FROM cancha) AS total_canchas, "
        "  (SELECT COUNT(*) FROM lesion) AS total_lesiones, "
        "  (SELECT COALESCE(ROUND(AVG(rendimiento_general), 2), 0) FROM partido) AS rendimiento_promedio";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error preparando consulta del dashboard.\n");
        *count = 0;
        return NULL;
    }

    *count = 1;
    return stmt;
}

typedef struct
{
    const char *filename;
    void *context;
    void (*write_header)(FILE *f, void *context);
    void (*write_row)(FILE *f, sqlite3_stmt *stmt, void *context);
    void (*write_footer)(FILE *f, void *context);
} ExportConfig;

static FILE* open_export_file(const char *filename, sqlite3_stmt *stmt)
{
    FILE *f;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || f == NULL)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }
    return f;
}

static void export_dashboard_generic(ExportConfig *config)
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_dashboard(&count);
    if (!stmt) return;

    FILE *f = open_export_file(config->filename, stmt);
    if (!f) return;

    if (config->write_header)
        config->write_header(f, config->context);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        config->write_row(f, stmt, config->context);
    }

    if (config->write_footer)
        config->write_footer(f, config->context);

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path(config->filename));
    fclose(f);
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

static void write_csv_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "metrica,valor\n");
}

static void write_csv_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f, "total_partidos,%d\n", sqlite3_column_int(stmt, 0));
    fprintf(f, "total_goles,%d\n", sqlite3_column_int(stmt, 1));
    fprintf(f, "total_asistencias,%d\n", sqlite3_column_int(stmt, 2));
    fprintf(f, "total_equipos,%d\n", sqlite3_column_int(stmt, 3));
    fprintf(f, "total_camisetas,%d\n", sqlite3_column_int(stmt, 4));
    fprintf(f, "total_canchas,%d\n", sqlite3_column_int(stmt, 5));
    fprintf(f, "total_lesiones,%d\n", sqlite3_column_int(stmt, 6));
    fprintf(f, "rendimiento_promedio,%.2f\n", sqlite3_column_double(stmt, 7));
}

static void write_txt_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "DASHBOARD - RESUMEN DE METRICAS\n\n");
}

static void write_txt_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f, "Total de Partidos: %d\n"
            "Total de Goles: %d\n"
            "Total de Asistencias: %d\n"
            "Total de Equipos: %d\n"
            "Total de Camisetas: %d\n"
            "Total de Canchas: %d\n"
            "Total de Lesiones: %d\n"
            "Rendimiento Promedio: %.2f\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 6),
            sqlite3_column_double(stmt, 7));
}

static void write_json_row(FILE *f, sqlite3_stmt *stmt, void *context) /* NOSONAR */
{
    (void)f;
    cJSON *root = (cJSON *)context;
    cJSON_AddNumberToObject(root, "total_partidos", sqlite3_column_int(stmt, 0));
    cJSON_AddNumberToObject(root, "total_goles", sqlite3_column_int(stmt, 1));
    cJSON_AddNumberToObject(root, "total_asistencias", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(root, "total_equipos", sqlite3_column_int(stmt, 3));
    cJSON_AddNumberToObject(root, "total_camisetas", sqlite3_column_int(stmt, 4));
    cJSON_AddNumberToObject(root, "total_canchas", sqlite3_column_int(stmt, 5));
    cJSON_AddNumberToObject(root, "total_lesiones", sqlite3_column_int(stmt, 6));
    cJSON_AddNumberToObject(root, "rendimiento_promedio", sqlite3_column_double(stmt, 7));
}

static void write_json_footer(FILE *f, void *context)
{
    cJSON *root = (cJSON *)context;
    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

static void write_html_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f,
            "<html><body><h1>Dashboard - Resumen de Metricas</h1>"
            "<table border='1'>"
            "<tr><th>Metrica</th><th>Valor</th></tr>");
}

static void write_html_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f, "<tr><td>Total Partidos</td><td>%d</td></tr>", sqlite3_column_int(stmt, 0));
    fprintf(f, "<tr><td>Total Goles</td><td>%d</td></tr>", sqlite3_column_int(stmt, 1));
    fprintf(f, "<tr><td>Total Asistencias</td><td>%d</td></tr>", sqlite3_column_int(stmt, 2));
    fprintf(f, "<tr><td>Total Equipos</td><td>%d</td></tr>", sqlite3_column_int(stmt, 3));
    fprintf(f, "<tr><td>Total Camisetas</td><td>%d</td></tr>", sqlite3_column_int(stmt, 4));
    fprintf(f, "<tr><td>Total Canchas</td><td>%d</td></tr>", sqlite3_column_int(stmt, 5));
    fprintf(f, "<tr><td>Total Lesiones</td><td>%d</td></tr>", sqlite3_column_int(stmt, 6));
    fprintf(f, "<tr><td>Rendimiento Promedio</td><td>%.2f</td></tr>", sqlite3_column_double(stmt, 7));
}

static void write_html_footer(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "</table></body></html>");
}

/** @} */

/** @name Funciones de exportacion del dashboard */
/** @{ */

void exportar_dashboard_csv()
{
    ExportConfig config =
    {
        .filename = "dashboard.csv",
        .context = NULL,
        .write_header = write_csv_header,
        .write_row = write_csv_row,
        .write_footer = NULL
    };
    export_dashboard_generic(&config);
}

void exportar_dashboard_txt()
{
    ExportConfig config =
    {
        .filename = "dashboard.txt",
        .context = NULL,
        .write_header = write_txt_header,
        .write_row = write_txt_row,
        .write_footer = NULL
    };
    export_dashboard_generic(&config);
}

void exportar_dashboard_json()
{
    cJSON *root = cJSON_CreateObject();
    ExportConfig config =
    {
        .filename = "dashboard.json",
        .context = root,
        .write_header = NULL,
        .write_row = write_json_row,
        .write_footer = write_json_footer
    };
    export_dashboard_generic(&config);
}

void exportar_dashboard_html()
{
    ExportConfig config =
    {
        .filename = "dashboard.html",
        .context = NULL,
        .write_header = write_html_header,
        .write_row = write_html_row,
        .write_footer = write_html_footer
    };
    export_dashboard_generic(&config);
}

/** @} */
