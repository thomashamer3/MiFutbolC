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
#include <stdio.h>

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

        printf("%-30s : %d\n",
               nombre,
               sqlite3_column_int(stmt, 1));
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
void mostrar_estadisticas()
{
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
    pause_console();
}
