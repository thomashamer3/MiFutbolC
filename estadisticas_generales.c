/**
 * @file estadisticas_generales.c
 * @brief Módulo para mostrar estadísticas generales de camisetas en partidos de fútbol.
 *
 * Este archivo contiene funciones para consultar y mostrar estadísticas
 * generales relacionadas con camisetas, como goles, asistencias y partidos jugados.
 */

#include "estadisticas_generales.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Array de días de la semana en español
const char* dias[] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};

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
    int num_cols;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols == 1)
        {
            if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
            {
                printf("%d\n", sqlite3_column_int(stmt, 0));
            }
            else if (sqlite3_column_type(stmt, 0) == SQLITE_FLOAT)
            {
                printf("%.2f\n", sqlite3_column_double(stmt, 0));
            }
            else
            {
                snprintf(nombre, sizeof(nombre), "%s", sqlite3_column_text(stmt, 0));
                printf("%s\n", nombre);
            }
        }
        else
        {
            snprintf(nombre, sizeof(nombre), "%s", sqlite3_column_text(stmt, 0));

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

    query("Camiseta mas Sorteada",
          "SELECT c.nombre, c.sorteada "
          "FROM camiseta c "
          "ORDER BY c.sorteada DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra el total de partidos jugados
 */
void mostrar_total_partidos_jugados()
{
    clear_screen();
    print_header("TOTAL DE PARTIDOS JUGADOS");

    query("Total de Partidos Jugados",
          "SELECT COUNT(*) FROM partido");

    pause_console();
}

/**
 * @brief Muestra el promedio de goles por partido
 */
void mostrar_promedio_goles_por_partido()
{
    clear_screen();
    print_header("PROMEDIO DE GOLES POR PARTIDO");

    query("Promedio de Goles por Partido",
          "SELECT ROUND(AVG(goles), 2) FROM partido");

    pause_console();
}

/**
 * @brief Muestra el promedio de asistencias por partido
 */
void mostrar_promedio_asistencias_por_partido()
{
    clear_screen();
    print_header("PROMEDIO DE ASISTENCIAS POR PARTIDO");

    query("Promedio de Asistencias por Partido",
          "SELECT ROUND(AVG(asistencias), 2) FROM partido");

    pause_console();
}

/**
 * @brief Muestra el promedio de rendimiento_general
 */
void mostrar_promedio_rendimiento_general()
{
    clear_screen();
    print_header("PROMEDIO DE RENDIMIENTO_GENERAL");

    query("Promedio de Rendimiento General",
          "SELECT ROUND(AVG(rendimiento_general), 2) FROM partido");

    pause_console();
}

/**
 * @brief Muestra el rendimiento promedio por clima
 */
void mostrar_rendimiento_promedio_por_clima()
{
    clear_screen();
    print_header("RENDIMIENTO PROMEDIO POR CLIMA");

    query("Rendimiento Promedio por Clima",
          "SELECT CASE WHEN clima = 1 THEN 'Despejado' WHEN clima = 2 THEN 'Nublado' WHEN clima = 3 THEN 'Lluvia' WHEN clima = 4 THEN 'Ventoso' WHEN clima = 5 THEN 'Mucho Calor' WHEN clima = 6 THEN 'Mucho Frio' END AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM partido GROUP BY clima ORDER BY clima");

    pause_console();
}

/**
 * @brief Muestra los goles por clima
 */
void mostrar_goles_por_clima()
{
    clear_screen();
    print_header("GOLES POR CLIMA");

    query("Goles por Clima",
          "SELECT CASE clima WHEN 1 THEN 'Despejado' WHEN 2 THEN 'Nublado' WHEN 3 THEN 'Lluvia' WHEN 4 THEN 'Ventoso' WHEN 5 THEN 'Mucho Calor' WHEN 6 THEN 'Mucho Frio' END AS clima_texto, SUM(goles) FROM partido GROUP BY clima ORDER BY clima");

    pause_console();
}

/**
 * @brief Muestra las asistencias por clima
 */
void mostrar_asistencias_por_clima()
{
    clear_screen();
    print_header("ASISTENCIAS POR CLIMA");

    query("Asistencias por Clima",
          "SELECT CASE clima WHEN 1 THEN 'Despejado' WHEN 2 THEN 'Nublado' WHEN 3 THEN 'Lluvia' WHEN 4 THEN 'Ventoso' WHEN 5 THEN 'Mucho Calor' WHEN 6 THEN 'Mucho Frio' END AS clima_texto, SUM(asistencias) FROM partido GROUP BY clima ORDER BY clima");

    pause_console();
}

/**
 * @brief Muestra el clima donde se rinde mejor
 */
void mostrar_clima_mejor_rendimiento()
{
    clear_screen();
    print_header("CLIMA DONDE SE RINDE MEJOR");

    query("Clima con Mejor Rendimiento Promedio",
          "SELECT CASE clima WHEN 1 THEN 'Despejado' WHEN 2 THEN 'Nublado' WHEN 3 THEN 'Lluvia' WHEN 4 THEN 'Ventoso' WHEN 5 THEN 'Mucho Calor' WHEN 6 THEN 'Mucho Frio' END AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM partido GROUP BY clima ORDER BY AVG(rendimiento_general) DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra el clima donde se rinde peor
 */
void mostrar_clima_peor_rendimiento()
{
    clear_screen();
    print_header("CLIMA DONDE SE RINDE PEOR");

    query("Clima con Peor Rendimiento Promedio",
          "SELECT CASE clima WHEN 1 THEN 'Despejado' WHEN 2 THEN 'Nublado' WHEN 3 THEN 'Lluvia' WHEN 4 THEN 'Ventoso' WHEN 5 THEN 'Mucho Calor' WHEN 6 THEN 'Mucho Frio' END AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM partido GROUP BY clima ORDER BY AVG(rendimiento_general) ASC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra el mejor día de la semana
 */
void mostrar_mejor_dia_semana()
{
    clear_screen();
    print_header("MEJOR DIA DE LA SEMANA");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Mejor Dia de la Semana"));
    printf("----------------------------------------\n");

    // Crear una consulta que garantice que todos los días de la semana aparezcan
    sqlite3_stmt *stmt;
    const char *sql = "WITH dias_semana AS ("
                      "SELECT 0 AS dia_num, 'Domingo' AS dia_nombre UNION ALL "
                      "SELECT 1, 'Lunes' UNION ALL "
                      "SELECT 2, 'Martes' UNION ALL "
                      "SELECT 3, 'Miercoles' UNION ALL "
                      "SELECT 4, 'Jueves' UNION ALL "
                      "SELECT 5, 'Viernes' UNION ALL "
                      "SELECT 6, 'Sabado'"
                      ") "
                      "SELECT ds.dia_nombre, "
                      "ROUND(COALESCE(AVG(p.rendimiento_general), 0), 2) AS promedio_rendimiento "
                      "FROM dias_semana ds "
                      "LEFT JOIN partido p ON CAST(strftime('%w', substr(p.fecha_hora, 7, 4) || '-' || substr(p.fecha_hora, 4, 2) || '-' || substr(p.fecha_hora, 1, 2)) AS INTEGER) = ds.dia_num "
                      "AND p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
                      "GROUP BY ds.dia_num, ds.dia_nombre "
                      "ORDER BY promedio_rendimiento DESC LIMIT 1";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *dia = (const char *)sqlite3_column_text(stmt, 0);
        double promedio = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f\n", remover_tildes(dia), promedio);
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra el peor día de la semana
 */
void mostrar_peor_dia_semana()
{
    clear_screen();
    print_header("PEOR DIA DE LA SEMANA");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Peor Dia de la Semana"));
    printf("----------------------------------------\n");

    // Crear una consulta que garantice que todos los días de la semana aparezcan
    sqlite3_stmt *stmt;
    const char *sql = "WITH dias_semana AS ("
                      "SELECT 0 AS dia_num, 'Domingo' AS dia_nombre UNION ALL "
                      "SELECT 1, 'Lunes' UNION ALL "
                      "SELECT 2, 'Martes' UNION ALL "
                      "SELECT 3, 'Miercoles' UNION ALL "
                      "SELECT 4, 'Jueves' UNION ALL "
                      "SELECT 5, 'Viernes' UNION ALL "
                      "SELECT 6, 'Sabado'"
                      ") "
                      "SELECT ds.dia_nombre, "
                      "ROUND(COALESCE(AVG(p.rendimiento_general), 0), 2) AS promedio_rendimiento "
                      "FROM dias_semana ds "
                      "LEFT JOIN partido p ON CAST(strftime('%w', substr(p.fecha_hora, 7, 4) || '-' || substr(p.fecha_hora, 4, 2) || '-' || substr(p.fecha_hora, 1, 2)) AS INTEGER) = ds.dia_num "
                      "AND p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
                      "GROUP BY ds.dia_num, ds.dia_nombre "
                      "ORDER BY promedio_rendimiento ASC LIMIT 1";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *dia = (const char *)sqlite3_column_text(stmt, 0);
        double promedio = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f\n", remover_tildes(dia), promedio);
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra los goles promedio por día
 */
void mostrar_goles_promedio_por_dia()
{
    clear_screen();
    print_header("GOLES PROMEDIO POR DIA");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Goles Promedio por Dia"));
    printf("----------------------------------------\n");

    // Crear una consulta que garantice que todos los días de la semana aparezcan
    sqlite3_stmt *stmt;
    const char *sql = "WITH dias_semana AS ("
                      "SELECT 0 AS dia_num, 'Domingo' AS dia_nombre UNION ALL "
                      "SELECT 1, 'Lunes' UNION ALL "
                      "SELECT 2, 'Martes' UNION ALL "
                      "SELECT 3, 'Miercoles' UNION ALL "
                      "SELECT 4, 'Jueves' UNION ALL "
                      "SELECT 5, 'Viernes' UNION ALL "
                      "SELECT 6, 'Sabado'"
                      ") "
                      "SELECT ds.dia_nombre, "
                      "ROUND(COALESCE(AVG(p.goles), 0), 2) AS promedio_goles "
                      "FROM dias_semana ds "
                      "LEFT JOIN partido p ON CAST(strftime('%w', substr(p.fecha_hora, 7, 4) || '-' || substr(p.fecha_hora, 4, 2) || '-' || substr(p.fecha_hora, 1, 2)) AS INTEGER) = ds.dia_num "
                      "AND p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
                      "GROUP BY ds.dia_num, ds.dia_nombre "
                      "ORDER BY ds.dia_num";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *dia = (const char *)sqlite3_column_text(stmt, 0);
        double promedio = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f\n", remover_tildes(dia), promedio);
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra las asistencias promedio por día
 */
void mostrar_asistencias_promedio_por_dia()
{
    clear_screen();
    print_header("ASISTENCIAS PROMEDIO POR DIA");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Asistencias Promedio por Dia"));
    printf("----------------------------------------\n");

    // Crear una consulta que garantice que todos los días de la semana aparezcan
    sqlite3_stmt *stmt;
    const char *sql = "WITH dias_semana AS ("
                      "SELECT 0 AS dia_num, 'Domingo' AS dia_nombre UNION ALL "
                      "SELECT 1, 'Lunes' UNION ALL "
                      "SELECT 2, 'Martes' UNION ALL "
                      "SELECT 3, 'Miercoles' UNION ALL "
                      "SELECT 4, 'Jueves' UNION ALL "
                      "SELECT 5, 'Viernes' UNION ALL "
                      "SELECT 6, 'Sabado'"
                      ") "
                      "SELECT ds.dia_nombre, "
                      "ROUND(COALESCE(AVG(p.asistencias), 0), 2) AS promedio_asistencias "
                      "FROM dias_semana ds "
                      "LEFT JOIN partido p ON CAST(strftime('%w', substr(p.fecha_hora, 7, 4) || '-' || substr(p.fecha_hora, 4, 2) || '-' || substr(p.fecha_hora, 1, 2)) AS INTEGER) = ds.dia_num "
                      "AND p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
                      "GROUP BY ds.dia_num, ds.dia_nombre "
                      "ORDER BY ds.dia_num";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *dia = (const char *)sqlite3_column_text(stmt, 0);
        double promedio = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f\n", remover_tildes(dia), promedio);
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra el rendimiento promedio por día
 */
void mostrar_rendimiento_promedio_por_dia()
{
    clear_screen();
    print_header("RENDIMIENTO PROMEDIO POR DIA");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Rendimiento Promedio por Dia"));
    printf("----------------------------------------\n");

    // Crear una consulta que garantice que todos los días de la semana aparezcan
    sqlite3_stmt *stmt;
    const char *sql = "WITH dias_semana AS ("
                      "SELECT 0 AS dia_num, 'Domingo' AS dia_nombre UNION ALL "
                      "SELECT 1, 'Lunes' UNION ALL "
                      "SELECT 2, 'Martes' UNION ALL "
                      "SELECT 3, 'Miercoles' UNION ALL "
                      "SELECT 4, 'Jueves' UNION ALL "
                      "SELECT 5, 'Viernes' UNION ALL "
                      "SELECT 6, 'Sabado'"
                      ") "
                      "SELECT ds.dia_nombre, "
                      "ROUND(COALESCE(AVG(p.rendimiento_general), 0), 2) AS promedio_rendimiento "
                      "FROM dias_semana ds "
                      "LEFT JOIN partido p ON CAST(strftime('%w', substr(p.fecha_hora, 7, 4) || '-' || substr(p.fecha_hora, 4, 2) || '-' || substr(p.fecha_hora, 1, 2)) AS INTEGER) = ds.dia_num "
                      "AND p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
                      "GROUP BY ds.dia_num, ds.dia_nombre "
                      "ORDER BY ds.dia_num";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *dia = (const char *)sqlite3_column_text(stmt, 0);
        double promedio = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f\n", remover_tildes(dia), promedio);
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra el rendimiento según nivel de cansancio
 */
void mostrar_rendimiento_por_nivel_cansancio()
{
    clear_screen();
    print_header("RENDIMIENTO POR NIVEL DE CANSANCIO");

    query("Rendimiento por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, COUNT(*) AS partidos FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END ORDER BY rendimiento_promedio DESC");

    pause_console();
}

/**
 * @brief Muestra los goles con cansancio alto vs bajo
 */
void mostrar_goles_cansancio_alto_vs_bajo()
{
    clear_screen();
    print_header("GOLES CON CANSANCIO ALTO VS BAJO");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Goles con Cansancio Alto vs Bajo"));
    printf("----------------------------------------\n");

    // Query modificada para mostrar el formato esperado: Alto: 1, Bajo: 0
    sqlite3_stmt *stmt;
    int num_cols;

    // Consulta para cansancio alto (>7) vs bajo (<=7)
    const char *sql = "SELECT CASE WHEN cansancio > 7 THEN 'Alto' ELSE 'Bajo' END AS nivel_cansancio, "
                      "SUM(goles) AS total_goles, ROUND(AVG(goles), 2) AS promedio_goles, COUNT(*) AS partidos "
                      "FROM partido GROUP BY CASE WHEN cansancio > 7 THEN 'Alto' ELSE 'Bajo' END";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols >= 2)
        {
            const char *nivel = (const char *)sqlite3_column_text(stmt, 0);
            int total_goles = sqlite3_column_int(stmt, 1);
            double promedio_goles = sqlite3_column_double(stmt, 2);

            // Mostrar en el formato especificado en la tarea
            printf("%-30s : %d", remover_tildes(nivel), total_goles);

            // Agregar nota para cansancio bajo si hay caida de rendimiento
            if (strcmp(nivel, "Bajo") == 0 && promedio_goles < 1.0)
            {
                printf(", Caida de Rendimiento por Cansancio Acumulado");
            }
            printf("\n");
        }
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra los partidos jugados con cansancio alto
 */
void mostrar_partidos_cansancio_alto()
{
    clear_screen();
    print_header("PARTIDOS JUGADOS CON CANSANCIO ALTO");

    query("Partidos con Cansancio Alto (>7)",
          "SELECT COUNT(*) AS partidos_cansancio_alto FROM partido WHERE cansancio > 7");

    pause_console();
}

/**
 * @brief Muestra la caída de rendimiento por cansancio acumulado
 */
void mostrar_caida_rendimiento_cansancio_acumulado()
{
    clear_screen();
    print_header("CAIDA DE RENDIMIENTO POR CANSANCIO ACUMULADO");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Caida de Rendimiento por Cansancio Acumulado"));
    printf("----------------------------------------\n");

    // Comparar rendimiento en partidos recientes vs antiguos con alto cansancio
    sqlite3_stmt *stmt;
    const char *sql = "SELECT 'Recientes (ultimos 5)' AS periodo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio FROM (SELECT rendimiento_general FROM partido WHERE cansancio > 7 ORDER BY fecha_hora DESC LIMIT 5) UNION ALL SELECT 'Antiguos (primeros 5)' AS periodo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio FROM (SELECT rendimiento_general FROM partido WHERE cansancio > 7 ORDER BY fecha_hora ASC LIMIT 5)";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
        double rendimiento = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f", remover_tildes(periodo), rendimiento);
        printf("\n");
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra el rendimiento según estado de ánimo
 */
void mostrar_rendimiento_por_estado_animo()
{
    clear_screen();
    print_header("RENDIMIENTO POR ESTADO DE ANIMO");

    query("Rendimiento por Estado de Animo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, COUNT(*) AS partidos FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END ORDER BY rendimiento_promedio DESC");

    pause_console();
}

/**
 * @brief Muestra los goles según estado de ánimo
 */
void mostrar_goles_por_estado_animo()
{
    clear_screen();
    print_header("GOLES POR ESTADO DE ANIMO");

    query("Goles por Estado de Animo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, SUM(goles) AS total_goles, ROUND(AVG(goles), 2) AS promedio_goles, COUNT(*) AS partidos FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END ORDER BY promedio_goles DESC");

    pause_console();
}

/**
 * @brief Muestra las asistencias según estado de ánimo
 */
void mostrar_asistencias_por_estado_animo()
{
    clear_screen();
    print_header("ASISTENCIAS POR ESTADO DE ANIMO");

    query("Asistencias por Estado de Animo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, SUM(asistencias) AS total_asistencias, ROUND(AVG(asistencias), 2) AS promedio_asistencias, COUNT(*) AS partidos FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END ORDER BY promedio_asistencias DESC");

    pause_console();
}

/**
 * @brief Muestra el estado de ánimo ideal para jugar
 */
void mostrar_estado_animo_ideal()
{
    clear_screen();
    print_header("ESTADO DE ANIMO IDEAL PARA JUGAR");

    query("Estado de Animo Ideal",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END ORDER BY rendimiento_promedio DESC LIMIT 1");

    pause_console();
}
/**
 * @brief Obtiene el día de la semana para una fecha dada
 * @param dia Día del mes (1-31)
 * @param mes Mes del año (1-12)
 * @param anio Año (ej. 2023)
 * @return Nombre del día de la semana en español
 */
const char* obtener_dia_semana(int dia, int mes, int anio)
{
    struct tm fecha = {0};
    fecha.tm_mday = dia;
    fecha.tm_mon  = mes - 1;      // Meses: 0-11
    fecha.tm_year = anio - 2023;  // Años desde 1900

    mktime(&fecha); // Calcula el día de la semana

    return dias[fecha.tm_wday];
}
