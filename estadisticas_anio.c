/**
 * @file estadisticas_anio.c
 * @brief Módulo para mostrar estadísticas históricas agrupadas por año.
 *
 * Este archivo contiene funciones para consultar y mostrar estadísticas
 * individuales por camiseta agrupadas por año.
 */

#include "estadisticas_anio.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Muestra estadísticas históricas agrupadas por año
 *
 * Esta función muestra estadísticas individuales por camiseta agrupadas por año,
 * incluyendo partidos jugados, goles, asistencias y promedios por partido.
 */
void mostrar_estadisticas_por_anio()
{
    clear_screen();
    print_header("ESTADISTICAS POR ANIO");
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias "
                       "FROM partido p "
                       "JOIN camiseta c ON p.camiseta_id = c.id "
                       "GROUP BY anio, c.id "
                       "ORDER BY anio DESC, total_goles DESC",
                       -1, &stmt, NULL);

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
            if (hay) printf("\n");
            printf("Anio: %s\n", anio);
            printf("----------------------------------------\n");
            strcpy(current_anio, anio);
        }

        printf("%-30s | PJ: %d | G: %d | A: %d | G/P: %.2f | A/P: %.2f\n",
               camiseta, partidos, total_goles, total_asistencias, avg_goles, avg_asistencias);
        hay = 1;
    }

    if (!hay)
        printf("No hay estadísticas disponibles.\n");

    sqlite3_finalize(stmt);
    pause_console();
}
