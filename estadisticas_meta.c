/**
 * @file estadisticas_meta.c
 * @brief Módulo para estadísticas avanzadas y meta-análisis en partidos de fútbol.
 *
 * Este archivo contiene funciones para analizar consistencia, partidos atípicos,
 * dependencia del contexto, impacto del cansancio y estado de ánimo.
 */

#include "estadisticas_meta.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

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
 * @brief Muestra la consistencia del rendimiento (variabilidad)
 *
 * Analiza la desviación estándar y coeficiente de variación del rendimiento general
 * para evaluar la consistencia del jugador/equipo.
 */
void mostrar_consistencia_rendimiento()
{
    clear_screen();
    print_header("CONSISTENCIA DEL RENDIMIENTO");

    // Calcular estadísticas básicas
    query("Promedio de Rendimiento General",
          "SELECT ROUND(AVG(rendimiento_general), 2) FROM partido");

    // Calcular desviación estándar
    query("Desviación Estándar del Rendimiento",
          "SELECT ROUND(SQRT(AVG(rendimiento_general * rendimiento_general) - AVG(rendimiento_general) * AVG(rendimiento_general)), 2) FROM partido");

    // Calcular coeficiente de variación
    query("Coeficiente de Variación (%)",
          "SELECT ROUND((SQRT(AVG(rendimiento_general * rendimiento_general) - AVG(rendimiento_general) * AVG(rendimiento_general)) / AVG(rendimiento_general) * 100), 2) FROM partido");

    // Mostrar rango de rendimiento
    query("Rango de Rendimiento (Mínimo)",
          "SELECT MIN(rendimiento_general) FROM partido");
    query("Rango de Rendimiento (Máximo)",
          "SELECT MAX(rendimiento_general) FROM partido");

    pause_console();
}

/**
 * @brief Muestra los partidos atípicos (muy buenos/muy malos)
 *
 * Identifica partidos con rendimiento significativamente diferente al promedio.
 */
void mostrar_partidos_outliers()
{
    clear_screen();
    print_header("PARTIDOS ATÍPICOS");

    // Calcular límites para outliers (1.5 * IQR)
    printf("\nPartidos con rendimiento excepcionalmente alto:\n");
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha_hora, rendimiento_general, goles, asistencias "
                      "FROM partido "
                      "WHERE rendimiento_general > (SELECT AVG(rendimiento_general) + 1.5 * (SELECT (PERCENTILE_CONT(0.75) WITHIN GROUP (ORDER BY rendimiento_general) - PERCENTILE_CONT(0.25) WITHIN GROUP (ORDER BY rendimiento_general)) FROM partido) FROM partido) "
                      "ORDER BY rendimiento_general DESC";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Partido ID: %d, Fecha: %s, Rendimiento: %d, Goles: %d, Asistencias: %d\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4));
    }

    sqlite3_finalize(stmt);

    printf("\nPartidos con rendimiento excepcionalmente bajo:\n");
    printf("----------------------------------------\n");

    const char *sql2 = "SELECT id, fecha_hora, rendimiento_general, goles, asistencias "
                       "FROM partido "
                       "WHERE rendimiento_general < (SELECT AVG(rendimiento_general) - 1.5 * (SELECT (PERCENTILE_CONT(0.75) WITHIN GROUP (ORDER BY rendimiento_general) - PERCENTILE_CONT(0.25) WITHIN GROUP (ORDER BY rendimiento_general)) FROM partido) FROM partido) "
                       "ORDER BY rendimiento_general ASC";

    sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Partido ID: %d, Fecha: %s, Rendimiento: %d, Goles: %d, Asistencias: %d\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4));
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra la dependencia del contexto
 *
 * Analiza cómo el rendimiento varía según diferentes contextos (clima, día, etc.)
 */
void mostrar_dependencia_contexto()
{
    clear_screen();
    print_header("DEPENDENCIA DEL CONTEXTO");

    printf("\nRendimiento por contexto:\n");
    printf("----------------------------------------\n");

    // Rendimiento por clima
    query("Rendimiento por Clima",
          "SELECT clima, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY clima ORDER BY AVG(rendimiento_general) DESC");

    // Rendimiento por día de semana
    query("Rendimiento por Día de Semana",
          "SELECT CASE strftime('%w', fecha_hora) WHEN '0' THEN 'Domingo' WHEN '1' THEN 'Lunes' WHEN '2' THEN 'Martes' WHEN '3' THEN 'Miércoles' WHEN '4' THEN 'Jueves' WHEN '5' THEN 'Viernes' WHEN '6' THEN 'Sábado' END AS dia, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY strftime('%w', fecha_hora) ORDER BY AVG(rendimiento_general) DESC");

    // Rendimiento por resultado
    query("Rendimiento por Resultado",
          "SELECT CASE resultado WHEN 1 THEN 'Victoria' WHEN 2 THEN 'Empate' WHEN 3 THEN 'Derrota' ELSE 'Desconocido' END AS resultado, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY resultado ORDER BY AVG(rendimiento_general) DESC");

    pause_console();
}

/**
 * @brief Muestra el impacto real del cansancio
 *
 * Analiza la correlación entre cansancio y rendimiento
 */
void mostrar_impacto_real_cansancio()
{
    clear_screen();
    print_header("IMPACTO REAL DEL CANSANCIO");

    // Correlación entre cansancio y rendimiento
    query("Correlación Cansancio-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(cansancio * rendimiento_general) - SUM(cansancio) * SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(cansancio * cansancio) - SUM(cansancio) * SUM(cansancio)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Rendimiento por nivel de cansancio
    query("Rendimiento por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(goles), 2) AS goles_promedio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY rendimiento_promedio DESC");

    // Impacto en resultados
    query("Resultados por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
          "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
          "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
          "COUNT(*) AS total "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END");

    pause_console();
}

/**
 * @brief Muestra el impacto real del estado de ánimo
 *
 * Analiza la correlación entre estado de ánimo y rendimiento
 */
void mostrar_impacto_real_estado_animo()
{
    clear_screen();
    print_header("IMPACTO REAL DEL ESTADO DE ÁNIMO");

    // Correlación entre estado de ánimo y rendimiento
    query("Correlación Estado de Ánimo-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(estado_animo * rendimiento_general) - SUM(estado_animo) * SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(estado_animo * estado_animo) - SUM(estado_animo) * SUM(estado_animo)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Rendimiento por nivel de estado de ánimo
    query("Rendimiento por Nivel de Estado de Ánimo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(goles), 2) AS goles_promedio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY rendimiento_promedio DESC");

    // Impacto en resultados
    query("Resultados por Nivel de Estado de Ánimo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, "
          "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
          "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
          "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
          "COUNT(*) AS total "
          "FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END");

    pause_console();
}

/**
 * @brief Muestra la eficiencia: goles por partido vs rendimiento
 *
 * Analiza la relación entre producción de goles y rendimiento general
 */
void mostrar_eficiencia_goles_vs_rendimiento()
{
    clear_screen();
    print_header("EFICIENCIA: GOLES POR PARTIDO VS RENDIMIENTO");

    // Correlación entre goles y rendimiento
    query("Correlación Goles-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(goles * rendimiento_general) - SUM(goles) * SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(goles * goles) - SUM(goles) * SUM(goles)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Eficiencia por rango de goles
    query("Eficiencia por Rango de Goles",
          "SELECT CASE WHEN goles = 0 THEN '0 goles' WHEN goles <= 2 THEN '1-2 goles' WHEN goles <= 4 THEN '3-4 goles' ELSE '5+ goles' END AS rango_goles, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN goles = 0 THEN '0 goles' WHEN goles <= 2 THEN '1-2 goles' WHEN goles <= 4 THEN '3-4 goles' ELSE '5+ goles' END "
          "ORDER BY rendimiento_promedio DESC");

    // Rendimiento por gol (eficiencia)
    query("Rendimiento por Gol (Eficiencia)",
          "SELECT ROUND(AVG(rendimiento_general) / NULLIF(AVG(goles), 0), 2) AS rendimiento_por_gol "
          "FROM partido WHERE goles > 0");

    pause_console();
}

/**
 * @brief Muestra la eficiencia: asistencias por partido vs cansancio
 *
 * Analiza cómo el cansancio afecta la capacidad de asistir
 */
void mostrar_eficiencia_asistencias_vs_cansancio()
{
    clear_screen();
    print_header("EFICIENCIA: ASISTENCIAS VS CANSANCIO");

    // Correlación entre asistencias y cansancio
    query("Correlación Asistencias-Cansancio",
          "SELECT ROUND((COUNT(*) * SUM(asistencias * cansancio) - SUM(asistencias) * SUM(cansancio)) / "
          "(SQRT((COUNT(*) * SUM(asistencias * asistencias) - SUM(asistencias) * SUM(asistencias)) * "
          "(COUNT(*) * SUM(cansancio * cansancio) - SUM(cansancio) * SUM(cansancio)))), 4) "
          "FROM partido");

    // Asistencias por nivel de cansancio
    query("Asistencias por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "ROUND(AVG(asistencias) / NULLIF(AVG(cansancio), 0), 2) AS asistencias_por_unidad_cansancio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY asistencias_promedio DESC");

    pause_console();
}

/**
 * @brief Muestra el rendimiento obtenido por esfuerzo
 *
 * Analiza la relación entre esfuerzo (cansancio) y resultados
 */
void mostrar_rendimiento_por_esfuerzo()
{
    clear_screen();
    print_header("RENDIMIENTO OBTENIDO POR ESFUERZO");

    // Rendimiento por unidad de cansancio
    query("Rendimiento por Unidad de Cansancio",
          "SELECT ROUND(AVG(rendimiento_general) / NULLIF(AVG(cansancio), 0), 2) AS rendimiento_por_cansancio "
          "FROM partido WHERE cansancio > 0");

    // Eficiencia por nivel de esfuerzo
    query("Eficiencia por Nivel de Esfuerzo",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo esfuerzo (1-3)' WHEN cansancio <= 7 THEN 'Esfuerzo medio (4-7)' ELSE 'Alto esfuerzo (8-10)' END AS nivel_esfuerzo, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(rendimiento_general) / NULLIF(AVG(cansancio), 0), 2) AS rendimiento_por_unidad_esfuerzo, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo esfuerzo (1-3)' WHEN cansancio <= 7 THEN 'Esfuerzo medio (4-7)' ELSE 'Alto esfuerzo (8-10)' END "
          "ORDER BY rendimiento_por_unidad_esfuerzo DESC");

    pause_console();
}

/**
 * @brief Muestra partidos exigentes bien rendidos
 *
 * Identifica partidos difíciles con buen rendimiento
 */
void mostrar_partidos_exigentes_bien_rendidos()
{
    clear_screen();
    print_header("PARTIDOS EXIGENTES BIEN RENDIDOS");

    printf("\nPartidos con alto cansancio y buen rendimiento:\n");
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha_hora, cansancio, rendimiento_general, goles, asistencias, resultado "
                      "FROM partido "
                      "WHERE cansancio > 7 AND rendimiento_general > (SELECT AVG(rendimiento_general) FROM partido) "
                      "ORDER BY rendimiento_general DESC, cansancio DESC";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *resultado_str;
        switch (sqlite3_column_int(stmt, 6))
        {
        case 1:
            resultado_str = "Victoria";
            break;
        case 2:
            resultado_str = "Empate";
            break;
        case 3:
            resultado_str = "Derrota";
            break;
        default:
            resultado_str = "Desconocido";
            break;
        }

        printf("ID: %d, Fecha: %s, Cansancio: %d, Rendimiento: %d, Goles: %d, Asistencias: %d, Resultado: %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_int(stmt, 5),
               resultado_str);
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra partidos fáciles mal rendidos
 *
 * Identifica partidos fáciles con bajo rendimiento
 */
void mostrar_partidos_faciles_mal_rendidos()
{
    clear_screen();
    print_header("PARTIDOS FÁCILES MAL RENDIDOS");

    printf("\nPartidos con bajo cansancio y bajo rendimiento:\n");
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha_hora, cansancio, rendimiento_general, goles, asistencias, resultado "
                      "FROM partido "
                      "WHERE cansancio <= 3 AND rendimiento_general < (SELECT AVG(rendimiento_general) FROM partido) "
                      "ORDER BY rendimiento_general ASC, cansancio ASC";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *resultado_str;
        switch (sqlite3_column_int(stmt, 6))
        {
        case 1:
            resultado_str = "Victoria";
            break;
        case 2:
            resultado_str = "Empate";
            break;
        case 3:
            resultado_str = "Derrota";
            break;
        default:
            resultado_str = "Desconocido";
            break;
        }

        printf("ID: %d, Fecha: %s, Cansancio: %d, Rendimiento: %d, Goles: %d, Asistencias: %d, Resultado: %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_int(stmt, 5),
               resultado_str);
    }

    sqlite3_finalize(stmt);

    pause_console();
}
