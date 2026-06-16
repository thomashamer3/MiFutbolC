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

typedef void (*anio_header_writer_t)(FILE *file, const char *anio, int hay);
typedef void (*anio_row_writer_t)(FILE *file, const EstadisticaAnio *stats);
typedef void (*anio_footer_writer_t)(FILE *file, int hay);

/* ============================================================================
 * HELPER ESTaTICOS
 * ============================================================================ */

static void write_stats_csv(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_CAMISETA))
    {
        return;
    }

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

static void write_stats_txt(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_CAMISETA))
    {
        return;
    }

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

static void write_stats_html(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_CAMISETA))
    {
        return;
    }

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

static void write_stats_json(FILE *file)
{
    cJSON *root = cJSON_CreateArray();
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_CAMISETA))
    {
        cJSON_Delete(root);
        return;
    }

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

    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

static void write_stats_anio_txt_header(FILE *file, const char *anio, int hay)
{
    if (hay) fprintf(file, "\n");
    fprintf(file, "Anio: %s\n", anio);
    fprintf(file, "----------------------------------------\n");
}

static void write_stats_anio_txt_row(FILE *file, const EstadisticaAnio *stats)
{
    fprintf(file, "%-30s | PJ: %d | G: %d | A: %d | G/P: %.2f | A/P: %.2f\n",
            stats->camiseta, stats->partidos, stats->total_goles, stats->total_asistencias, stats->avg_goles, stats->avg_asistencias);
}

static void write_stats_anio_html_header(FILE *file, const char *anio, int hay)
{
    if (hay) fprintf(file, "</table><br>");
    fprintf(file, "<h2>Anio: %s</h2><table border='1'>", anio);
    fprintf(file, "<tr><th>Camiseta</th><th>Partidos</th><th>Goles</th><th>Asistencias</th><th>G/P</th><th>A/P</th></tr>");
}

static void write_stats_anio_html_row(FILE *file, const EstadisticaAnio *stats)
{
    fprintf(file,
            "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td></tr>",
            stats->camiseta, stats->partidos, stats->total_goles, stats->total_asistencias, stats->avg_goles, stats->avg_asistencias);
}

static void write_stats_anio_html_footer(FILE *file, int hay)
{
    if (hay) fprintf(file, "</table>");
}

static void write_stats_anio_common(FILE *file,
                                    anio_header_writer_t write_header,
                                    anio_row_writer_t write_row,
                                    anio_footer_writer_t write_footer)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_ANIO))
    {
        return;
    }

    char current_anio[5] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EstadisticaAnio stats;
        extraer_estadistica_anio(stmt, &stats);

        if (strcmp(current_anio, stats.anio) != 0)
        {
            write_header(file, stats.anio, hay);
            strcpy_s(current_anio, sizeof(current_anio), stats.anio);
        }

        write_row(file, &stats);
        hay = 1;
    }

    sqlite3_finalize(stmt);

    if (write_footer)
    {
        write_footer(file, hay);
    }
}

static void write_stats_anio_csv(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_ANIO))
    {
        return;
    }

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

static void write_stats_anio_txt(FILE *file)
{
    write_stats_anio_common(file,
                            write_stats_anio_txt_header,
                            write_stats_anio_txt_row,
                            NULL);
}

static void write_stats_anio_html(FILE *file)
{
    write_stats_anio_common(file,
                            write_stats_anio_html_header,
                            write_stats_anio_html_row,
                            write_stats_anio_html_footer);
}

static void write_stats_anio_json(FILE *file)
{
    cJSON *root = cJSON_CreateObject();
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_ANIO))
    {
        cJSON_Delete(root);
        return;
    }

    char current_anio[5] = "";
    cJSON *current_anio_array = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EstadisticaAnio stats;
        extraer_estadistica_anio(stmt, &stats);

        if (strcmp(current_anio, stats.anio) != 0)
        {
            if (current_anio_array)
            {
                cJSON_AddItemToObject(root, current_anio, current_anio_array);
            }
            strcpy_s(current_anio, sizeof(current_anio), stats.anio);
            current_anio_array = cJSON_CreateArray();
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", stats.camiseta);
        cJSON_AddNumberToObject(item, "partidos", stats.partidos);
        cJSON_AddNumberToObject(item, "total_goles", stats.total_goles);
        cJSON_AddNumberToObject(item, "total_asistencias", stats.total_asistencias);
        cJSON_AddNumberToObject(item, "avg_goles", stats.avg_goles);
        cJSON_AddNumberToObject(item, "avg_asistencias", stats.avg_asistencias);
        cJSON_AddItemToArray(current_anio_array, item);
    }

    if (current_anio_array)
    {
        cJSON_AddItemToObject(root, current_anio, current_anio_array);
    }

    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

static void write_stats_md(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_CAMISETA))
        return;

    fprintf(file, "| Camiseta | Goles | Asistencias | Partidos |"
            " Victorias | Empates | Derrotas |\n");
    fprintf(file, "|----------|-------|-------------|----------|"
            "-----------|---------|----------|\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "| %s | %d | %d | %d | %d | %d | %d |\n",
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

/* ============================================================================
 * EXPORTACIoN ESTADiSTICAS (4 formatos)
 * ============================================================================ */

void exportar_estadisticas_csv()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("­ticas para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas.csv", "Error al crear el archivo CSV");
    if (!f)
    {
        return;
    }

    fprintf(f, "Camiseta,Goles,Asistencias,Partidos,Victorias,Empates,Derrotas\n");
    write_stats_csv(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.csv"));
}

void exportar_estadisticas_txt()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas.txt", "Error al crear el archivo TXT");
    if (!f)
    {
        return;
    }

    write_stats_txt(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.txt"));
}

void exportar_estadisticas_json()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas.json", "Error al crear el archivo JSON");
    if (!f)
    {
        return;
    }

    write_stats_json(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.json"));
}

void exportar_estadisticas_html()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas.html", "Error al crear el archivo HTML");
    if (!f)
    {
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

void exportar_estadisticas_md()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas.md",
                                        "Error al crear el archivo Markdown");
    if (!f)
        return;

    fprintf(f, "# Estadisticas por Camiseta\n\n");
    fprintf(f, "*Generado por MiFutbolC*\n\n");
    write_stats_md(f);
    fprintf(f, "\n");

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.md"));
}

/* ============================================================================
 * EXPORTACIoN ESTADiSTICAS POR AnO (4 formatos)
 * ============================================================================ */

void exportar_estadisticas_por_anio_csv()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas por ano para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas_por_anio.csv", "Error al crear el archivo CSV");
    if (!f)
    {
        return;
    }

    fprintf(f, "Anio,Camiseta,Partidos,Goles,Asistencias,Promedio Goles,Promedio Asistencias\n");
    write_stats_anio_csv(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.csv"));
}

void exportar_estadisticas_por_anio_txt()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas por ano para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas_por_anio.txt", "Error al crear el archivo TXT");
    if (!f)
    {
        return;
    }

    write_stats_anio_txt(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.txt"));
}

void exportar_estadisticas_por_anio_json()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas por ano para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas_por_anio.json", "Error al crear el archivo JSON");
    if (!f)
    {
        return;
    }

    write_stats_anio_json(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.json"));
}

void exportar_estadisticas_por_anio_html()
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas por ano para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("estadisticas_por_anio.html", "Error al crear el archivo HTML");
    if (!f)
    {
        return;
    }

    fprintf(f, "<html><body><h1>Estadisticas por Anio</h1>");

    write_stats_anio_html(f);

    fprintf(f, "</body></html>");

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.html"));
}

/* ============================================================================
 * HELPERS DE FILA (reutilizan stmt externo, sin prepare/finalize)
 * ============================================================================ */

static void stats_camiseta_csv_rows(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "Camiseta,Goles,Asistencias,Partidos,Victorias,Empates,Derrotas\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s,%d,%d,%d,%d,%d,%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
}

static void stats_camiseta_txt_rows(FILE *f, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s | G:%d A:%d P:%d V:%d E:%d D:%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
}

static void stats_camiseta_html_rows(FILE *f, sqlite3_stmt *stmt)
{
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
}

static void stats_camiseta_json_rows(FILE *f, sqlite3_stmt *stmt)
{
    cJSON *root = cJSON_CreateArray();
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
    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

static void stats_camiseta_md_rows(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "| Camiseta | Goles | Asistencias | Partidos | Victorias | Empates | Derrotas |\n");
    fprintf(f, "|----------|-------|-------------|----------|-----------|---------|----------|\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "| %s | %d | %d | %d | %d | %d | %d |\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
}

static void stats_anio_common_from_stmt(FILE *file, sqlite3_stmt *stmt,
                                        anio_header_writer_t write_header,
                                        anio_row_writer_t write_row,
                                        anio_footer_writer_t write_footer)
{
    char current_anio[5] = "";
    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EstadisticaAnio stats;
        extraer_estadistica_anio(stmt, &stats);
        if (strcmp(current_anio, stats.anio) != 0)
        {
            write_header(file, stats.anio, hay);
            strcpy_s(current_anio, sizeof(current_anio), stats.anio);
        }
        write_row(file, &stats);
        hay = 1;
    }
    if (write_footer) write_footer(file, hay);
}

static void stats_anio_csv_rows(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "Anio,Camiseta,Partidos,Goles,Asistencias,Promedio Goles,Promedio Asistencias\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s,%s,%d,%d,%d,%.2f,%.2f\n",
                sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4), sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6));
}

static void stats_anio_txt_rows(FILE *f, sqlite3_stmt *stmt)
{
    stats_anio_common_from_stmt(f, stmt, write_stats_anio_txt_header, write_stats_anio_txt_row, NULL);
}

static void stats_anio_html_rows(FILE *f, sqlite3_stmt *stmt)
{
    stats_anio_common_from_stmt(f, stmt, write_stats_anio_html_header, write_stats_anio_html_row, write_stats_anio_html_footer);
}

static void stats_anio_json_rows(FILE *f, sqlite3_stmt *stmt)
{
    cJSON *root = cJSON_CreateObject();
    char current_anio[5] = "";
    cJSON *anio_arr = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EstadisticaAnio stats;
        extraer_estadistica_anio(stmt, &stats);
        if (strcmp(current_anio, stats.anio) != 0)
        {
            anio_arr = cJSON_CreateArray();
            cJSON_AddItemToObject(root, stats.anio, anio_arr);
            strcpy_s(current_anio, sizeof(current_anio), stats.anio);
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", stats.camiseta);
        cJSON_AddNumberToObject(item, "partidos", stats.partidos);
        cJSON_AddNumberToObject(item, "goles", stats.total_goles);
        cJSON_AddNumberToObject(item, "asistencias", stats.total_asistencias);
        cJSON_AddNumberToObject(item, "avg_goles", stats.avg_goles);
        cJSON_AddNumberToObject(item, "avg_asistencias", stats.avg_asistencias);
        cJSON_AddItemToArray(anio_arr, item);
    }
    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

/* ============================================================================
 * EXPORTACIoN BATCH (una consulta SQL, todos los formatos)
 * ============================================================================ */

void exportar_estadisticas_all(void)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas para exportar");
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_CAMISETA)) return;
    FILE *f;

    f = abrir_archivo_exportacion("estadisticas.csv", "Error al crear CSV");
    if (f)
    {
        stats_camiseta_csv_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas.csv"));
        fclose(f);
    }

    sqlite3_reset(stmt);
    f = abrir_archivo_exportacion("estadisticas.txt", "Error al crear TXT");
    if (f)
    {
        stats_camiseta_txt_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas.txt"));
        fclose(f);
    }

    sqlite3_reset(stmt);
    f = abrir_archivo_exportacion("estadisticas.json", "Error al crear JSON");
    if (f)
    {
        stats_camiseta_json_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas.json"));
        fclose(f);
    }

    sqlite3_reset(stmt);
    f = abrir_archivo_exportacion("estadisticas.html", "Error al crear HTML");
    if (f)
    {
        fprintf(f, "<html><body><h1>Estadisticas</h1><table border='1'><tr><th>Camiseta</th><th>Goles</th><th>Asistencias</th><th>Partidos</th><th>Victorias</th><th>Empates</th><th>Derrotas</th></tr>");
        stats_camiseta_html_rows(f, stmt);
        fprintf(f, "</table></body></html>");
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas.html"));
        fclose(f);
    }

    sqlite3_reset(stmt);
    f = abrir_archivo_exportacion("estadisticas.md", "Error al crear MD");
    if (f)
    {
        fprintf(f, "# Estadisticas por Camiseta\n\n*Generado por MiFutbolC*\n\n");
        stats_camiseta_md_rows(f, stmt);
        fprintf(f, "\n");
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas.md"));
        fclose(f);
    }

    sqlite3_finalize(stmt);
}

void exportar_estadisticas_por_anio_all(void)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("estadisticas por ano para exportar");
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_STATS_BY_ANIO)) return;
    FILE *f;

    f = abrir_archivo_exportacion("estadisticas_por_anio.csv", "Error al crear CSV");
    if (f)
    {
        stats_anio_csv_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.csv"));
        fclose(f);
    }

    sqlite3_reset(stmt);
    f = abrir_archivo_exportacion("estadisticas_por_anio.txt", "Error al crear TXT");
    if (f)
    {
        stats_anio_txt_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.txt"));
        fclose(f);
    }

    sqlite3_reset(stmt);
    f = abrir_archivo_exportacion("estadisticas_por_anio.json", "Error al crear JSON");
    if (f)
    {
        stats_anio_json_rows(f, stmt);
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.json"));
        fclose(f);
    }

    sqlite3_reset(stmt);
    f = abrir_archivo_exportacion("estadisticas_por_anio.html", "Error al crear HTML");
    if (f)
    {
        fprintf(f, "<html><body><h1>Estadisticas por Anio</h1>");
        stats_anio_html_rows(f, stmt);
        fprintf(f, "</body></html>");
        printf("Archivo exportado a: %s\n", get_export_path("estadisticas_por_anio.html"));
        fclose(f);
    }

    sqlite3_finalize(stmt);
}
