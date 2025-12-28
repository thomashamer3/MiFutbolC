/**
 * @file estadisticas.c
 * @brief Módulo para mostrar estadísticas de camisetas en partidos de fútbol.
 *
 * Este archivo contiene funciones para consultar y mostrar estadísticas
 * relacionadas con camisetas, como goles, asistencias y partidos jugados.
 */

#include "estadisticas.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Función auxiliar para ejecutar consultas SQL y mostrar resultados.
 *
 * Esta función ejecuta una consulta SQL preparada y muestra los resultados
 * en formato de tabla con el título proporcionado.
 *
 * @param titulo El título a mostrar antes de los resultados.
 * @param sql La consulta SQL a ejecutar.
 */
static void query(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;
    char nombre[200];

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        snprintf(nombre, sizeof(nombre), "%s",
                 sqlite3_column_text(stmt, 0));

        // Check if the second column is integer or real
        if (sqlite3_column_type(stmt, 1) == SQLITE_INTEGER)
        {
            printf("%-30s : %d\n",
                   nombre,
                   sqlite3_column_int(stmt, 1));
        }
        else if (sqlite3_column_type(stmt, 1) == SQLITE_FLOAT)
        {
            printf("%-30s : %.2f\n",
                   nombre,
                   sqlite3_column_double(stmt, 1));
        }
        else
        {
            // Fallback to int
            printf("%-30s : %d\n",
                   nombre,
                   sqlite3_column_int(stmt, 1));
        }
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra las estadísticas principales de las camisetas.
 *
 * Esta función imprime un encabezado y ejecuta varias consultas para mostrar
 * estadísticas como la camiseta con más goles, asistencias, partidos jugados
 * y la suma de goles más asistencias. Al final, pausa la consola.
 */
void mostrar_estadisticas_generales()
{
    clear_screen();
    print_header("ESTADISTICAS");
    query("Camiseta con mas Goles",
          "SELECT c.nombre, IFNULL(SUM(p.goles),0) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con mas Asistencias",
          "SELECT c.nombre, IFNULL(SUM(p.asistencias),0) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con mas Partidos",
          "SELECT c.nombre, COUNT(*) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con mas Goles + Asistencias",
          "SELECT c.nombre, IFNULL(SUM(p.goles+p.asistencias),0) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con mejor Rendimiento General promedio",
          "SELECT c.nombre, IFNULL(ROUND(AVG(p.rendimiento_general), 2), 0.00) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con mejor Estado de Animo promedio",
          "SELECT c.nombre, IFNULL(ROUND(AVG(p.estado_animo), 2), 0.00) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con menos Cansancio promedio",
          "SELECT c.nombre, IFNULL(ROUND(AVG(p.cansancio), 2), 0.00) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "GROUP BY c.id "
          "ORDER BY 2 ASC LIMIT 1");

    query("Camiseta con mas Victorias",
          "SELECT c.nombre, COUNT(*) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "WHERE p.resultado = 1 "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con mas Empates",
          "SELECT c.nombre, COUNT(*) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "WHERE p.resultado = 2 "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    query("Camiseta con mas Derrotas",
          "SELECT c.nombre, COUNT(*) "
          "FROM partido p "
          "JOIN camiseta c ON p.camiseta_id=c.id "
          "WHERE p.resultado = 3 "
          "GROUP BY c.id "
          "ORDER BY 2 DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra estadísticas históricas agrupadas por mes
 *
 * Esta función muestra estadísticas individuales por camiseta agrupadas por mes,
 * incluyendo partidos jugados, goles, asistencias y promedios por partido.
 */
void mostrar_estadisticas_por_mes()
{
    clear_screen();
    print_header("ESTADISTICAS POR MES");
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT substr(fecha_hora, 4, 7) AS mes_anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias "
                       "FROM partido p "
                       "JOIN camiseta c ON p.camiseta_id = c.id "
                       "GROUP BY mes_anio, c.id "
                       "ORDER BY mes_anio DESC, total_goles DESC",
                       -1, &stmt, NULL);

    char current_mes[8] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes_anio = (const char *)sqlite3_column_text(stmt, 0);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int total_goles = sqlite3_column_int(stmt, 3);
        int total_asistencias = sqlite3_column_int(stmt, 4);
        double avg_goles = sqlite3_column_double(stmt, 5);
        double avg_asistencias = sqlite3_column_double(stmt, 6);

        if (strcmp(current_mes, mes_anio) != 0)
        {
            if (hay) printf("\n");
            printf("Mes: %s\n", mes_anio);
            printf("----------------------------------------\n");
            strcpy(current_mes, mes_anio);
        }

        printf("%-30s | PJ: %d | G: %d | A: %d | G/P: %.2f | A/P: %.2f\n",
               camiseta, partidos, total_goles, total_asistencias, avg_goles, avg_asistencias);
        hay = 1;
    }

    if (!hay)
        printf("No hay estadisticas disponibles.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

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

/**
 * @brief Muestra el menú de estadísticas con opciones para ver estadísticas generales, por mes o por año
 */
void menu_estadisticas()
{
    MenuItem items[] =
    {
        {1, "Generales", mostrar_estadisticas_generales},
        {2, "Por Mes", mostrar_estadisticas_por_mes},
        {3, "Por Anio", mostrar_estadisticas_por_anio},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS", items, 4);
}
