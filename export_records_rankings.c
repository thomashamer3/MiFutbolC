/**
 * @file export_records_rankings.c
 * @brief Implementación de exportación de récords y rankings en MiFutbolC
 */

#include "export_records_rankings.h"
#include "db.h"
#include "utils.h"
#include "export.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * Ejecuta una consulta SQL y devuelve el statement.
 * Centraliza la ejecución de consultas para evitar duplicación de código.
 */
static sqlite3_stmt* execute_records_query(const char* sql)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }
    return stmt;
}

/**
 * Función auxiliar para exportar récords a CSV.
 * Centraliza la lógica común de exportación CSV para récords individuales.
 */
static void exportar_record_csv(const char *titulo, const char *sql, const char *filepath)
{
    sqlite3_stmt *stmt;
    FILE *file = fopen(filepath, "w");

    if (!file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Valor,Camiseta,Fecha\n");

    stmt = execute_records_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2));
    }

    if (stmt) sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado a %s\n", filepath);
}

/**
 * Función auxiliar para exportar combinaciones a CSV.
 * Centraliza la lógica común de exportación CSV para combinaciones cancha-camiseta.
 */
static void exportar_combinacion_csv(const char *titulo, const char *sql, const char *filepath)
{
    sqlite3_stmt *stmt;
    FILE *file = fopen(filepath, "w");

    if (!file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Cancha,Camiseta,Rendimiento_Promedio,Partidos_Jugados\n");

    stmt = execute_records_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%s,%.2f,%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_double(stmt, 2),
                sqlite3_column_int(stmt, 3));
    }

    if (stmt) sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado a %s\n", filepath);
}

/**
 * Función auxiliar para exportar temporadas a CSV.
 * Centraliza la lógica común de exportación CSV para temporadas.
 */
static void exportar_temporada_csv(const char *titulo, const char *sql, const char *filepath)
{
    sqlite3_stmt *stmt;
    FILE *file = fopen(filepath, "w");

    if (!file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Anio,Rendimiento_Promedio,Partidos_Jugados\n");

    stmt = execute_records_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%.2f,%d\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_double(stmt, 1),
                sqlite3_column_int(stmt, 2));
    }

    if (stmt) sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado a %s\n", filepath);
}

/**
 * Función auxiliar para obtener datos de récord.
 * Centraliza la lógica de consulta y formateo para récords individuales.
 * Retorna 1 si encontró datos, 0 si no encontró.
 */
static int get_record_data(sqlite3_stmt *stmt, int *valor, const char **camiseta, const char **fecha)
{
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *valor = sqlite3_column_int(stmt, 0);
        *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        *fecha = (const char *)sqlite3_column_text(stmt, 2);
        return 1;
    }
    return 0;
}

/**
 * Función auxiliar para obtener datos de combinación.
 * Centraliza la lógica de consulta y formateo para combinaciones cancha-camiseta.
 * Retorna 1 si encontró datos, 0 si no encontró.
 */
static int get_combinacion_data(sqlite3_stmt *stmt, const char **cancha, const char **camiseta, double *rendimiento, int *partidos)
{
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *cancha = (const char *)sqlite3_column_text(stmt, 0);
        *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        *rendimiento = sqlite3_column_double(stmt, 2);
        *partidos = sqlite3_column_int(stmt, 3);
        return 1;
    }
    return 0;
}

/**
 * Función auxiliar para obtener datos de temporada.
 * Centraliza la lógica de consulta y formateo para temporadas.
 * Retorna 1 si encontró datos, 0 si no encontró.
 */
static int get_temporada_data(sqlite3_stmt *stmt, const char **anio, double *rendimiento, int *partidos)
{
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *anio = (const char *)sqlite3_column_text(stmt, 0);
        *rendimiento = sqlite3_column_double(stmt, 1);
        *partidos = sqlite3_column_int(stmt, 2);
        return 1;
    }
    return 0;
}

/**
 * @brief Exporta el récord de goles en un partido a CSV
 */
void exportar_record_goles_partido_csv()
{
    exportar_record_csv("Record de Goles en un Partido",
                        "SELECT p.goles, c.nombre, p.fecha_hora "
                        "FROM partido p "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "ORDER BY p.goles DESC LIMIT 1",
                        get_export_path("record_goles_partido.csv"));
}

/**
 * @brief Exporta el récord de asistencias en un partido a CSV
 */
void exportar_record_asistencias_partido_csv()
{
    exportar_record_csv("Record de Asistencias en un Partido",
                        "SELECT p.asistencias, c.nombre, p.fecha_hora "
                        "FROM partido p "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "ORDER BY p.asistencias DESC LIMIT 1",
                        get_export_path("record_asistencias_partido.csv"));
}

/**
 * @brief Exporta la mejor combinación cancha + camiseta a CSV
 */
void exportar_mejor_combinacion_cancha_camiseta_csv()
{
    exportar_combinacion_csv("Mejor Combinacion Cancha + Camiseta",
                             "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                             "FROM partido p "
                             "JOIN cancha ca ON p.cancha_id = ca.id "
                             "JOIN camiseta c ON p.camiseta_id = c.id "
                             "GROUP BY p.cancha_id, p.camiseta_id "
                             "ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1",
                             get_export_path("mejor_combinacion_cancha_camiseta.csv"));
}

/**
 * @brief Exporta la peor combinación cancha + camiseta a CSV
 */
void exportar_peor_combinacion_cancha_camiseta_csv()
{
    exportar_combinacion_csv("Peor Combinacion Cancha + Camiseta",
                             "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                             "FROM partido p "
                             "JOIN cancha ca ON p.cancha_id = ca.id "
                             "JOIN camiseta c ON p.camiseta_id = c.id "
                             "GROUP BY p.cancha_id, p.camiseta_id "
                             "ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1",
                             get_export_path("peor_combinacion_cancha_camiseta.csv"));
}

/**
 * @brief Exporta la mejor temporada a CSV
 */
void exportar_mejor_temporada_csv()
{
    exportar_temporada_csv("Mejor Temporada",
                           "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                           "FROM partido p "
                           "GROUP BY strftime('%Y', p.fecha_hora) "
                           "ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1",
                           get_export_path("mejor_temporada.csv"));
}

/**
 * @brief Exporta la peor temporada a CSV
 */
void exportar_peor_temporada_csv()
{
    exportar_temporada_csv("Peor Temporada",
                           "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                           "FROM partido p "
                           "GROUP BY strftime('%Y', p.fecha_hora) "
                           "ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1",
                           get_export_path("peor_temporada.csv"));
}

/**
 * Exporta récords y rankings a TXT.
 * Usa funciones auxiliares para mantener el código conciso y dentro del límite de líneas.
 */
void exportar_records_rankings_txt()
{
    FILE *file = fopen(get_export_path("records_rankings.txt"), "w");

    if (!file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "RECORDS & RANKINGS\n");
    fprintf(file, "==================\n\n");

    sqlite3_stmt *stmt;
    int valor;
    const char *camiseta, *fecha;
    const char *cancha;
    double rendimiento;
    int partidos;
    const char *anio;

    // Record de goles
    stmt = execute_records_query("SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "Record de Goles en un Partido: %d (Camiseta: %s, Fecha: %s)\n", valor, camiseta, fecha);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Record de asistencias
    stmt = execute_records_query("SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "Record de Asistencias en un Partido: %d (Camiseta: %s, Fecha: %s)\n", valor, camiseta, fecha);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor combinación cancha + camiseta
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "Mejor Combinacion Cancha + Camiseta: Cancha: %s, Camiseta: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", cancha, camiseta, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor combinación cancha + camiseta
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "Peor Combinacion Cancha + Camiseta: Cancha: %s, Camiseta: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", cancha, camiseta, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor temporada
    stmt = execute_records_query("SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "Mejor Temporada: Anio: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", anio, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor temporada
    stmt = execute_records_query("SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "Peor Temporada: Anio: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", anio, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.txt"));
}

/**
 * Exporta récords y rankings a JSON.
 * Usa funciones auxiliares para mantener el código conciso y dentro del límite de líneas.
 */
void exportar_records_rankings_json()
{
    FILE *file = fopen(get_export_path("records_rankings.json"), "w");

    if (!file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"records_rankings\": {\n");

    sqlite3_stmt *stmt;
    int valor;
    const char *camiseta, *fecha;
    const char *cancha;
    double rendimiento;
    int partidos;
    const char *anio;

    // Record de goles
    fprintf(file, "    \"record_goles\": ");
    stmt = execute_records_query("SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "{\"valor\": %d, \"camiseta\": \"%s\", \"fecha\": \"%s\"}", valor, camiseta, fecha);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Record de asistencias
    fprintf(file, ",\n    \"record_asistencias\": ");
    stmt = execute_records_query("SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "{\"valor\": %d, \"camiseta\": \"%s\", \"fecha\": \"%s\"}", valor, camiseta, fecha);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor combinación cancha + camiseta
    fprintf(file, ",\n    \"mejor_combinacion\": ");
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "{\"cancha\": \"%s\", \"camiseta\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}", cancha, camiseta, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor combinación cancha + camiseta
    fprintf(file, ",\n    \"peor_combinacion\": ");
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "{\"cancha\": \"%s\", \"camiseta\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}", cancha, camiseta, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor temporada
    fprintf(file, ",\n    \"mejor_temporada\": ");
    stmt = execute_records_query("SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "{\"anio\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}", anio, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor temporada
    fprintf(file, ",\n    \"peor_temporada\": ");
    stmt = execute_records_query("SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "{\"anio\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}", anio, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt) sqlite3_finalize(stmt);

    fprintf(file, "\n  }\n");
    fprintf(file, "}\n");

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.json"));
}

/**
 * Exporta récords y rankings a HTML.
 * Usa funciones auxiliares para mantener el código conciso y dentro del límite de líneas.
 */
void exportar_records_rankings_html()
{
    FILE *file = fopen(get_export_path("records_rankings.html"), "w");

    if (!file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html>\n");
    fprintf(file, "<head><title>Records & Rankings</title></head>\n");
    fprintf(file, "<body>\n");
    fprintf(file, "<h1>RECORDS & RANKINGS</h1>\n");

    sqlite3_stmt *stmt;
    int valor;
    const char *camiseta, *fecha;
    const char *cancha;
    double rendimiento;
    int partidos;
    const char *anio;

    // Record de goles
    fprintf(file, "<h2>Record de Goles en un Partido</h2>\n");
    stmt = execute_records_query("SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "<p><strong>%d</strong> (Camiseta: %s, Fecha: %s)</p>\n", valor, camiseta, fecha);
    }
    else
    {
        fprintf(file, "<p>No hay datos disponibles</p>\n");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Record de asistencias
    fprintf(file, "<h2>Record de Asistencias en un Partido</h2>\n");
    stmt = execute_records_query("SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "<p><strong>%d</strong> (Camiseta: %s, Fecha: %s)</p>\n", valor, camiseta, fecha);
    }
    else
    {
        fprintf(file, "<p>No hay datos disponibles</p>\n");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor combinación cancha + camiseta
    fprintf(file, "<h2>Mejor Combinacion Cancha + Camiseta</h2>\n");
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "<p>Cancha: <strong>%s</strong>, Camiseta: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n", cancha, camiseta, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "<p>No hay datos disponibles</p>\n");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor combinación cancha + camiseta
    fprintf(file, "<h2>Peor Combinacion Cancha + Camiseta</h2>\n");
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "<p>Cancha: <strong>%s</strong>, Camiseta: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n", cancha, camiseta, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "<p>No hay datos disponibles</p>\n");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor temporada
    fprintf(file, "<h2>Mejor Temporada</h2>\n");
    stmt = execute_records_query("SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "<p>Anio: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n", anio, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "<p>No hay datos disponibles</p>\n");
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor temporada
    fprintf(file, "<h2>Peor Temporada</h2>\n");
    stmt = execute_records_query("SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "<p>Anio: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n", anio, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "<p>No hay datos disponibles</p>\n");
    }
    if (stmt) sqlite3_finalize(stmt);

    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.html"));
}
