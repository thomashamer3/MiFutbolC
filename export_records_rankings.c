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
 * @brief Función auxiliar para exportar récords a CSV
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

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "%d,%s,%s\n",
                    sqlite3_column_int(stmt, 0),
                    sqlite3_column_text(stmt, 1),
                    sqlite3_column_text(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
    printf("Exportado a %s\n", filepath);
}

/**
 * @brief Función auxiliar para exportar combinaciones a CSV
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

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "%s,%s,%.2f,%d\n",
                    sqlite3_column_text(stmt, 0),
                    sqlite3_column_text(stmt, 1),
                    sqlite3_column_double(stmt, 2),
                    sqlite3_column_int(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
    printf("Exportado a %s\n", filepath);
}

/**
 * @brief Función auxiliar para exportar temporadas a CSV
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

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "%d,%.2f,%d\n",
                    sqlite3_column_int(stmt, 0),
                    sqlite3_column_double(stmt, 1),
                    sqlite3_column_int(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
    printf("Exportado a %s\n", filepath);
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
 * @brief Exporta récords y rankings a TXT
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

    // Record de goles
    if (sqlite3_prepare_v2(db, "SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "Record de Goles en un Partido: %d (Camiseta: %s, Fecha: %s)\n",
                    sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }

    // Record de asistencias
    if (sqlite3_prepare_v2(db, "SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "Record de Asistencias en un Partido: %d (Camiseta: %s, Fecha: %s)\n",
                    sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }

    // Mejor combinación cancha + camiseta
    if (sqlite3_prepare_v2(db, "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "Mejor Combinacion Cancha + Camiseta: Cancha: %s, Camiseta: %s, Rendimiento Promedio: %.2f, Partidos: %d\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2), sqlite3_column_int(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }

    // Peor combinación cancha + camiseta
    if (sqlite3_prepare_v2(db, "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "Peor Combinacion Cancha + Camiseta: Cancha: %s, Camiseta: %s, Rendimiento Promedio: %.2f, Partidos: %d\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2), sqlite3_column_int(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }

    // Mejor temporada
    if (sqlite3_prepare_v2(db, "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "Mejor Temporada: Anio: %s, Rendimiento Promedio: %.2f, Partidos: %d\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1), sqlite3_column_int(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }

    // Peor temporada
    if (sqlite3_prepare_v2(db, "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "Peor Temporada: Anio: %s, Rendimiento Promedio: %.2f, Partidos: %d\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1), sqlite3_column_int(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.txt"));
}

/**
 * @brief Exporta récords y rankings a JSON
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

    // Record de goles
    fprintf(file, "    \"record_goles\": ");
    if (sqlite3_prepare_v2(db, "SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "{\"valor\": %d, \"camiseta\": \"%s\", \"fecha\": \"%s\"}",
                    sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
        }
        else
        {
            fprintf(file, "null");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(file, "null");
    }

    // Record de asistencias
    fprintf(file, ",\n    \"record_asistencias\": ");
    if (sqlite3_prepare_v2(db, "SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "{\"valor\": %d, \"camiseta\": \"%s\", \"fecha\": \"%s\"}",
                    sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
        }
        else
        {
            fprintf(file, "null");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(file, "null");
    }

    // Mejor combinación cancha + camiseta
    fprintf(file, ",\n    \"mejor_combinacion\": ");
    if (sqlite3_prepare_v2(db, "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "{\"cancha\": \"%s\", \"camiseta\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}",
                    sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2), sqlite3_column_int(stmt, 3));
        }
        else
        {
            fprintf(file, "null");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(file, "null");
    }

    // Peor combinación cancha + camiseta
    fprintf(file, ",\n    \"peor_combinacion\": ");
    if (sqlite3_prepare_v2(db, "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "{\"cancha\": \"%s\", \"camiseta\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}",
                    sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2), sqlite3_column_int(stmt, 3));
        }
        else
        {
            fprintf(file, "null");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(file, "null");
    }

    // Mejor temporada
    fprintf(file, ",\n    \"mejor_temporada\": ");
    if (sqlite3_prepare_v2(db, "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "{\"anio\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}",
                    sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1), sqlite3_column_int(stmt, 2));
        }
        else
        {
            fprintf(file, "null");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(file, "null");
    }

    // Peor temporada
    fprintf(file, ",\n    \"peor_temporada\": ");
    if (sqlite3_prepare_v2(db, "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "{\"anio\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}",
                    sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1), sqlite3_column_int(stmt, 2));
        }
        else
        {
            fprintf(file, "null");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(file, "null");
    }

    fprintf(file, "\n  }\n");
    fprintf(file, "}\n");

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.json"));
}

/**
 * @brief Exporta récords y rankings a HTML
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

    // Record de goles
    fprintf(file, "<h2>Record de Goles en un Partido</h2>\n");
    if (sqlite3_prepare_v2(db, "SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "<p><strong>%d</strong> (Camiseta: %s, Fecha: %s)</p>\n",
                    sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
        }
        else
        {
            fprintf(file, "<p>No hay datos disponibles</p>\n");
        }
        sqlite3_finalize(stmt);
    }

    // Record de asistencias
    fprintf(file, "<h2>Record de Asistencias en un Partido</h2>\n");
    if (sqlite3_prepare_v2(db, "SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "<p><strong>%d</strong> (Camiseta: %s, Fecha: %s)</p>\n",
                    sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
        }
        else
        {
            fprintf(file, "<p>No hay datos disponibles</p>\n");
        }
        sqlite3_finalize(stmt);
    }

    // Mejor combinación cancha + camiseta
    fprintf(file, "<h2>Mejor Combinacion Cancha + Camiseta</h2>\n");
    if (sqlite3_prepare_v2(db, "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "<p>Cancha: <strong>%s</strong>, Camiseta: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2), sqlite3_column_int(stmt, 3));
        }
        else
        {
            fprintf(file, "<p>No hay datos disponibles</p>\n");
        }
        sqlite3_finalize(stmt);
    }

    // Peor combinación cancha + camiseta
    fprintf(file, "<h2>Peor Combinacion Cancha + Camiseta</h2>\n");
    if (sqlite3_prepare_v2(db, "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "<p>Cancha: <strong>%s</strong>, Camiseta: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2), sqlite3_column_int(stmt, 3));
        }
        else
        {
            fprintf(file, "<p>No hay datos disponibles</p>\n");
        }
        sqlite3_finalize(stmt);
    }

    // Mejor temporada
    fprintf(file, "<h2>Mejor Temporada</h2>\n");
    if (sqlite3_prepare_v2(db, "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "<p>Anio: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1), sqlite3_column_int(stmt, 2));
        }
        else
        {
            fprintf(file, "<p>No hay datos disponibles</p>\n");
        }
        sqlite3_finalize(stmt);
    }

    // Peor temporada
    fprintf(file, "<h2>Peor Temporada</h2>\n");
    if (sqlite3_prepare_v2(db, "SELECT strftime('%Y', p.fecha_hora), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p GROUP BY strftime('%Y', p.fecha_hora) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(file, "<p>Anio: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n",
                    sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1), sqlite3_column_int(stmt, 2));
        }
        else
        {
            fprintf(file, "<p>No hay datos disponibles</p>\n");
        }
        sqlite3_finalize(stmt);
    }

    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.html"));
}
