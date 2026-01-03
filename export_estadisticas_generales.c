#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>
/** @name Funciones de exportación de estadísticas detalladas */
/** @{ */

/**
 * @brief Exporta las estadísticas generales a formato CSV
 */
void exportar_estadisticas_generales_csv()
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
        printf("No hay registros de partidos para exportar estadisticas generales.\n");
        return;
    }

    const char *filepath = get_export_path("estadisticas_generales.csv");
    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error creando archivo CSV\n");
        return;
    }

    fprintf(file, "Categoria,Camiseta,Valor\n");

    // Camiseta con mas Goles
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Goles,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Asistencias,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Partidos
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Partidos,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Goles + Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles+p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Goles+Asistencias,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Rendimiento General promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.rendimiento_general), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mejor Rendimiento General Promedio,%s,%.2f\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Estado de Animo promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.estado_animo), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mejor Estado de Animo Promedio,%s,%.2f\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con menos Cansancio promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.cansancio), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 ASC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Menos Cansancio Promedio,%s,%.2f\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Victorias
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 1 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Victorias,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Empates
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 2 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Empates,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Derrotas
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 3 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Derrotas,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta mas Sorteada
    sqlite3_prepare_v2(db, "SELECT c.nombre, c.sorteada FROM camiseta c ORDER BY c.sorteada DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Mas Sorteada,%s,%d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    fclose(file);
    printf("Estadisticas generales exportadas a %s\n", filepath);
}

/**
 * @brief Exporta las estadísticas generales a formato TXT
 */
void exportar_estadisticas_generales_txt()
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
        printf("No hay registros de partidos para exportar estadisticas generales.\n");
        return;
    }

    const char *filepath = get_export_path("estadisticas_generales.txt");
    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error creando archivo TXT\n");
        return;
    }

    fprintf(file, "ESTADISTICAS GENERALES\n");
    fprintf(file, "======================\n\n");

    sqlite3_stmt *stmt;

    // Camiseta con mas Goles
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mas Goles: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mas Asistencias: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Partidos
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mas Partidos: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Goles + Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles+p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mas Goles+Asistencias: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Rendimiento General promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.rendimiento_general), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mejor Rendimiento General Promedio: %s (%.2f)\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Estado de Animo promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.estado_animo), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mejor Estado de Animo Promedio: %s (%.2f)\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con menos Cansancio promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.cansancio), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 ASC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con menos Cansancio Promedio: %s (%.2f)\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Victorias
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 1 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mas Victorias: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Empates
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 2 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mas Empates: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Derrotas
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 3 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta con mas Derrotas: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta mas Sorteada
    sqlite3_prepare_v2(db, "SELECT c.nombre, c.sorteada FROM camiseta c ORDER BY c.sorteada DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "Camiseta mas Sorteada: %s (%d)\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    fclose(file);
    printf("Estadisticas generales exportadas a %s\n", filepath);
}

/**
 * @brief Exporta las estadísticas generales a formato JSON
 */
void exportar_estadisticas_generales_json()
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
        printf("No hay registros de partidos para exportar estadisticas generales.\n");
        return;
    }

    const char *filepath = get_export_path("estadisticas_generales.json");
    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error creando archivo JSON\n");
        return;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"estadisticas_generales\": {\n");

    sqlite3_stmt *stmt;
    int first = 1;

    // Camiseta con mas Goles
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "    \"mas_goles\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
        first = 0;
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mas_asistencias\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Partidos
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mas_partidos\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Goles + Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles+p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mas_goles_asistencias\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Rendimiento General promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.rendimiento_general), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mejor_rendimiento_general\": {\"camiseta\": \"%s\", \"valor\": %.2f}", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Estado de Animo promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.estado_animo), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mejor_estado_animo\": {\"camiseta\": \"%s\", \"valor\": %.2f}", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con menos Cansancio promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.cansancio), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 ASC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"menos_cansancio\": {\"camiseta\": \"%s\", \"valor\": %.2f}", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Victorias
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 1 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mas_victorias\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Empates
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 2 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mas_empates\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Derrotas
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 3 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mas_derrotas\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta mas Sorteada
    sqlite3_prepare_v2(db, "SELECT c.nombre, c.sorteada FROM camiseta c ORDER BY c.sorteada DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(file, ",\n");
        fprintf(file, "    \"mas_sorteada\": {\"camiseta\": \"%s\", \"valor\": %d}", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    fprintf(file, "\n  }\n");
    fprintf(file, "}\n");

    fclose(file);
    printf("Estadisticas generales exportadas a %s\n", filepath);
}

/**
 * @brief Exporta las estadísticas generales a formato HTML
 */
void exportar_estadisticas_generales_html()
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
        printf("No hay registros de partidos para exportar estadisticas generales.\n");
        return;
    }

    const char *filepath = get_export_path("estadisticas_generales.html");
    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error creando archivo HTML\n");
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html>\n");
    fprintf(file, "<head><title>Estadisticas Generales</title></head>\n");
    fprintf(file, "<body>\n");
    fprintf(file, "<h1>Estadisticas Generales</h1>\n");
    fprintf(file, "<table border='1'>\n");
    fprintf(file, "<tr><th>Categoria</th><th>Camiseta</th><th>Valor</th></tr>\n");

    sqlite3_stmt *stmt;

    // Camiseta con mas Goles
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Goles</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Asistencias</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Partidos
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Partidos</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Goles + Asistencias
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(SUM(p.goles+p.asistencias),0) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Goles+Asistencias</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Rendimiento General promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.rendimiento_general), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mejor Rendimiento General Promedio</td><td>%s</td><td>%.2f</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mejor Estado de Animo promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.estado_animo), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mejor Estado de Animo Promedio</td><td>%s</td><td>%.2f</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con menos Cansancio promedio
    sqlite3_prepare_v2(db, "SELECT c.nombre, IFNULL(ROUND(AVG(p.cansancio), 2), 0.00) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id GROUP BY c.id ORDER BY 2 ASC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Menos Cansancio Promedio</td><td>%s</td><td>%.2f</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Victorias
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 1 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Victorias</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Empates
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 2 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Empates</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta con mas Derrotas
    sqlite3_prepare_v2(db, "SELECT c.nombre, COUNT(*) FROM partido p JOIN camiseta c ON p.camiseta_id=c.id WHERE p.resultado = 3 GROUP BY c.id ORDER BY 2 DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Derrotas</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // Camiseta mas Sorteada
    sqlite3_prepare_v2(db, "SELECT c.nombre, c.sorteada FROM camiseta c ORDER BY c.sorteada DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "<tr><td>Mas Sorteada</td><td>%s</td><td>%d</td></tr>\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);

    fprintf(file, "</table>\n");
    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");

    fclose(file);
    printf("Estadisticas generales exportadas a %s\n", filepath);
}

/**
 * @brief Exporta las estadísticas por mes a formato CSV
 */
void exportar_estadisticas_por_mes_csv()
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
        printf("No hay registros de partidos para exportar estadisticas por mes.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.csv"), "w");
    if (!file)
    {
        printf("Error creando archivo CSV\n");
        return;
    }

    fprintf(file, "Mes-Anio,Camiseta,Partidos,Total Goles,Total Asistencias,Avg Goles,Avg Asistencias\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT substr(fecha_hora, 4, 7) AS mes_anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias FROM partido p JOIN camiseta c ON p.camiseta_id = c.id GROUP BY mes_anio, c.id ORDER BY mes_anio DESC, total_goles DESC", -1, &stmt, NULL);

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
    fclose(file);
    printf("Estadisticas por mes exportadas a %s\n", get_export_path("estadisticas_por_mes.csv"));
}

/**
 * @brief Exporta las estadísticas por mes a formato TXT
 */
void exportar_estadisticas_por_mes_txt()
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
        printf("No hay registros de partidos para exportar estadisticas por mes.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.txt"), "w");
    if (!file)
    {
        printf("Error creando archivo TXT\n");
        return;
    }

    fprintf(file, "ESTADISTICAS POR MES\n");
    fprintf(file, "====================\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT substr(fecha_hora, 4, 7) AS mes_anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias FROM partido p JOIN camiseta c ON p.camiseta_id = c.id GROUP BY mes_anio, c.id ORDER BY mes_anio DESC, total_goles DESC", -1, &stmt, NULL);

    char current_mes[8] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes_anio = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current_mes, mes_anio) != 0)
        {
            strcpy(current_mes, mes_anio);
            fprintf(file, "\n%s:\n", mes_anio);
        }

        fprintf(file, "  %s: %d partidos, %d goles, %d asistencias (Avg: %.2f G, %.2f A)\n",
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6));
    }

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Estadisticas por mes exportadas a %s\n", get_export_path("estadisticas_por_mes.txt"));
}

/**
 * @brief Exporta las estadísticas por mes a formato JSON
 */
void exportar_estadisticas_por_mes_json()
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
        printf("No hay registros de partidos para exportar estadisticas por mes.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.json"), "w");
    if (!file)
    {
        printf("Error creando archivo JSON\n");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *meses = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT substr(fecha_hora, 4, 7) AS mes_anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias FROM partido p JOIN camiseta c ON p.camiseta_id = c.id GROUP BY mes_anio, c.id ORDER BY mes_anio DESC, total_goles DESC", -1, &stmt, NULL);

    char current_mes[8] = "";
    cJSON *current_mes_obj = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes_anio = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current_mes, mes_anio) != 0)
        {
            strcpy(current_mes, mes_anio);
            current_mes_obj = cJSON_CreateObject();
            cJSON_AddItemToObject(meses, mes_anio, current_mes_obj);
        }

        cJSON *camiseta_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(camiseta_obj, "partidos", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(camiseta_obj, "total_goles", sqlite3_column_int(stmt, 3));
        cJSON_AddNumberToObject(camiseta_obj, "total_asistencias", sqlite3_column_int(stmt, 4));
        cJSON_AddNumberToObject(camiseta_obj, "avg_goles", sqlite3_column_double(stmt, 5));
        cJSON_AddNumberToObject(camiseta_obj, "avg_asistencias", sqlite3_column_double(stmt, 6));
        cJSON_AddItemToObject(current_mes_obj, (const char *)sqlite3_column_text(stmt, 1), camiseta_obj);
    }

    cJSON_AddItemToObject(root, "estadisticas_por_mes", meses);

    char *json_string = cJSON_Print(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    fclose(file);
    printf("Estadisticas por mes exportadas a %s\n", get_export_path("estadisticas_por_mes.json"));
}

/**
 * @brief Exporta las estadísticas por mes a formato HTML
 */
void exportar_estadisticas_por_mes_html()
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
        printf("No hay registros de partidos para exportar estadisticas por mes.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_mes.html"), "w");
    if (!file)
    {
        printf("Error creando archivo HTML\n");
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html>\n");
    fprintf(file, "<head><title>Estadisticas Por Mes</title></head>\n");
    fprintf(file, "<body>\n");
    fprintf(file, "<h1>Estadisticas Por Mes</h1>\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT substr(fecha_hora, 4, 7) AS mes_anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias FROM partido p JOIN camiseta c ON p.camiseta_id = c.id GROUP BY mes_anio, c.id ORDER BY mes_anio DESC, total_goles DESC", -1, &stmt, NULL);

    char current_mes[8] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes_anio = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current_mes, mes_anio) != 0)
        {
            strcpy(current_mes, mes_anio);
            if (current_mes[0] != '\0')
            {
                fprintf(file, "</table>\n");
            }
            fprintf(file, "<h2>%s</h2>\n", mes_anio);
            fprintf(file, "<table border='1'>\n");
            fprintf(file, "<tr><th>Camiseta</th><th>Partidos</th><th>Total Goles</th><th>Total Asistencias</th><th>Avg Goles</th><th>Avg Asistencias</th></tr>\n");
        }

        fprintf(file, "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td></tr>\n",
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6));
    }

    fprintf(file, "</table>\n");
    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Estadisticas por mes exportadas a %s\n", get_export_path("estadisticas_por_mes.html"));
}

/**
 * @brief Exporta las estadísticas por año a formato CSV
 */
void exportar_estadisticas_por_anio_csv()
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
        printf("No hay registros de partidos para exportar estadisticas por anio.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_anio.csv"), "w");
    if (!file)
    {
        printf("Error creando archivo CSV\n");
        return;
    }

    fprintf(file, "Anio,Camiseta,Partidos,Total Goles,Total Asistencias,Avg Goles,Avg Asistencias,Victorias,Empates,Derrotas\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, "
                       "SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, "
                       "ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias, "
                       "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
                       "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
                       "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "GROUP BY anio, c.id ORDER BY anio DESC, total_goles DESC",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%s,%d,%d,%d,%.2f,%.2f,%d,%d,%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9));
    }

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Estadisticas por anio exportadas a %s\n", get_export_path("estadisticas_por_anio.csv"));
}

/**
 * @brief Exporta las estadísticas por año a formato TXT
 */
void exportar_estadisticas_por_anio_txt()
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
        printf("No hay registros de partidos para exportar estadisticas por anio.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_anio.txt"), "w");
    if (!file)
    {
        printf("Error creando archivo TXT\n");
        return;
    }

    fprintf(file, "ESTADISTICAS POR ANIO\n");
    fprintf(file, "=====================\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, "
                       "SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, "
                       "ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias, "
                       "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
                       "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
                       "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "GROUP BY anio, c.id ORDER BY anio DESC, total_goles DESC",
                       -1, &stmt, NULL);

    char current_anio[5] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current_anio, anio) != 0)
        {
            strcpy(current_anio, anio);
            fprintf(file, "\n%s:\n", anio);
        }

        fprintf(file, "  %s: %d partidos, %d goles, %d asistencias (Avg: %.2f G, %.2f A), %dV %dE %dD\n",
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9));
    }

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Estadisticas por anio exportadas a %s\n", get_export_path("estadisticas_por_anio.txt"));
}

/**
 * @brief Exporta las estadísticas por año a formato JSON
 */
void exportar_estadisticas_por_anio_json()
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
        printf("No hay registros de partidos para exportar estadisticas por anio.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_anio.json"), "w");
    if (!file)
    {
        printf("Error creando archivo JSON\n");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *anios = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, "
                       "SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, "
                       "ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias, "
                       "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
                       "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
                       "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "GROUP BY anio, c.id ORDER BY anio DESC, total_goles DESC",
                       -1, &stmt, NULL);

    char current_anio[5] = "";
    cJSON *current_anio_obj = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current_anio, anio) != 0)
        {
            strcpy(current_anio, anio);
            current_anio_obj = cJSON_CreateObject();
            cJSON_AddItemToObject(anios, anio, current_anio_obj);
        }

        cJSON *camiseta_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(camiseta_obj, "partidos", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(camiseta_obj, "total_goles", sqlite3_column_int(stmt, 3));
        cJSON_AddNumberToObject(camiseta_obj, "total_asistencias", sqlite3_column_int(stmt, 4));
        cJSON_AddNumberToObject(camiseta_obj, "avg_goles", sqlite3_column_double(stmt, 5));
        cJSON_AddNumberToObject(camiseta_obj, "avg_asistencias", sqlite3_column_double(stmt, 6));
        cJSON_AddNumberToObject(camiseta_obj, "victorias", sqlite3_column_int(stmt, 7));
        cJSON_AddNumberToObject(camiseta_obj, "empates", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(camiseta_obj, "derrotas", sqlite3_column_int(stmt, 9));
        cJSON_AddItemToObject(current_anio_obj, (const char *)sqlite3_column_text(stmt, 1), camiseta_obj);
    }

    cJSON_AddItemToObject(root, "estadisticas_por_anio", anios);

    char *json_string = cJSON_Print(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    fclose(file);
    printf("Estadisticas por anio exportadas a %s\n", get_export_path("estadisticas_por_anio.json"));
}

/**
 * @brief Exporta las estadísticas por año a formato HTML
 */
void exportar_estadisticas_por_anio_html()
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
        printf("No hay registros de partidos para exportar estadisticas por anio.\n");
        return;
    }

    FILE *file = fopen(get_export_path("estadisticas_por_anio.html"), "w");
    if (!file)
    {
        printf("Error creando archivo HTML\n");
        return;
    }

    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html>\n");
    fprintf(file, "<head><title>Estadisticas Por Anio</title></head>\n");
    fprintf(file, "<body>\n");
    fprintf(file, "<h1>Estadisticas Por Anio</h1>\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, "
                       "SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, "
                       "ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias, "
                       "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
                       "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
                       "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "GROUP BY anio, c.id ORDER BY anio DESC, total_goles DESC",
                       -1, &stmt, NULL);

    char current_anio[5] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(current_anio, anio) != 0)
        {
            strcpy(current_anio, anio);
            if (current_anio[0] != '\0')
            {
                fprintf(file, "</table>\n");
            }
            fprintf(file, "<h2>%s</h2>\n", anio);
            fprintf(file, "<table border='1'>\n");
            fprintf(file, "<tr><th>Camiseta</th><th>Partidos</th><th>Total Goles</th><th>Total Asistencias</th><th>Avg Goles</th><th>Avg Asistencias</th><th>Victorias</th><th>Empates</th><th>Derrotas</th></tr>\n");
        }

        fprintf(file, "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td><td>%d</td><td>%d</td><td>%d</td></tr>\n",
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_double(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9));
    }

    fprintf(file, "</table>\n");
    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");

    sqlite3_finalize(stmt);
    fclose(file);
    printf("Estadisticas por anio exportadas a %s\n", get_export_path("estadisticas_por_anio.html"));
}
