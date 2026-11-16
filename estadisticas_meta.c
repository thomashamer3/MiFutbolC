
#include "estadisticas_meta.h"
#include "ascii_charts.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <time.h>

static const char *resultado_a_texto(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "Victoria";
    case 2:
        return "Empate";
    case 3:
        return "Derrota";
    default:
        return "Desconocido";
    }
}

// Static helper to execute SQL queries and display results in a formatted way,
// ensuring consistent output format across different statistical analyses without code duplication.
static void query(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;
    char nombre[200];

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols == 1)
        {
            if (sqlite3_column_type(stmt, 0) == SQLITE_FLOAT)
            {
                printf("%-30s : %.4f\n", titulo, sqlite3_column_double(stmt, 0));
            }
            else if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
            {
                printf("%-30s : %d\n", titulo, sqlite3_column_int(stmt, 0));
            }
            else if (sqlite3_column_type(stmt, 0) == SQLITE_NULL)
            {
                printf("%-30s : N/A\n", titulo);
            }
            else
            {
                printf("%-30s : %s\n", titulo, (const char *)sqlite3_column_text(stmt, 0));
            }
        }
        else
        {
            const char *text = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(nombre, sizeof(nombre), "%s", text ? text : "Desconocido");

            if (sqlite3_column_type(stmt, 1) == SQLITE_FLOAT)
            {
                printf("%-30s : %.2f\n", nombre, sqlite3_column_double(stmt, 1));
            }
            else
            {
                printf("%-30s : %d\n", nombre, sqlite3_column_int(stmt, 1));
            }
        }
    }

    sqlite3_finalize(stmt);
}

void mostrar_consistencia_rendimiento(void)
{
    clear_screen();
    print_header("CONSISTENCIA DEL RENDIMIENTO");

    // Calcular estadisticas basicas
    query("Promedio de Rendimiento General",
          "SELECT ROUND(AVG(rendimiento_general), 2) FROM partido");

    // Calcular desviacion estandar
    query("Desviacion Estandar del Rendimiento",
          "SELECT ROUND(SQRT(AVG(rendimiento_general * rendimiento_general) - "
          "AVG(rendimiento_general) * AVG(rendimiento_general)), 2) FROM partido");

    // Calcular coeficiente de variacion
    query("Coeficiente de Variacion (%)",
          "SELECT ROUND((SQRT(AVG(rendimiento_general * rendimiento_general) - "
          "AVG(rendimiento_general) * AVG(rendimiento_general)) / AVG(rendimiento_general) * 100), "
          "2) FROM partido");

    // Mostrar rango de rendimiento
    query("Rango de Rendimiento (Minimo)", "SELECT MIN(rendimiento_general) FROM partido");
    query("Rango de Rendimiento (Maximo)", "SELECT MAX(rendimiento_general) FROM partido");

    pause_console();
}

static void mostrar_partidos_con_sql(const char *titulo, const char *descripcion, const char *sql)
{
    clear_screen();
    print_header(titulo);

    printf("\n%s\n", descripcion);
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *resultado_str = resultado_a_texto(sqlite3_column_int(stmt, 6));

        printf("ID: %d, Fecha: %s, Cansancio: %d, Rendimiento: %d, Goles: %d, Asistencias: %d, "
               "Resultado: %s\n",
               sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4), sqlite3_column_int(stmt, 5), resultado_str);
        hay = 1;
    }
    if (!hay)
    {
        printf("No se encontraron partidos que cumplan el criterio.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void mostrar_partidos_outliers(void)
{
    clear_screen();
    print_header("PARTIDOS ATIPICOS");

    sqlite3_stmt *stmt;

    /* IQR con posiciones corregidas: Q1 en 25%, Q3 en 75% */
    const char *sql =
        "WITH stats AS ("
        "  SELECT"
        "    AVG(rendimiento_general) AS media,"
        "    AVG(rendimiento_general) + 2.0 * STDDEV(rendimiento_general) AS limite_superior,"
        "    AVG(rendimiento_general) - 2.0 * STDDEV(rendimiento_general) AS limite_inferior"
        "  FROM partido"
        ") "
        "SELECT id, fecha_hora, rendimiento_general, goles, asistencias, "
        "  CASE WHEN rendimiento_general > limite_superior THEN 'alto' ELSE 'bajo' END AS tipo "
        "FROM partido, stats "
        "WHERE rendimiento_general > limite_superior "
        "UNION ALL "
        "SELECT id, fecha_hora, rendimiento_general, goles, asistencias, "
        "  CASE WHEN rendimiento_general < limite_inferior THEN 'bajo' ELSE 'alto' END AS tipo "
        "FROM partido, stats "
        "WHERE rendimiento_general < limite_inferior "
        "ORDER BY tipo DESC, rendimiento_general DESC";

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    int hay_alto = 0;
    int hay_bajo = 0;
    printf("\nPartidos con rendimiento excepcionalmente alto (+2 desviaciones):\n");
    printf("----------------------------------------\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *tipo = (const char *)sqlite3_column_text(stmt, 5);
        if (tipo && tipo[0] == 'b' && hay_alto > 0)
        {
            printf("\nPartidos con rendimiento excepcionalmente bajo (-2 desviaciones):\n");
            printf("----------------------------------------\n");
        }
        printf("Partido ID: %d, Fecha: %s, Rendimiento: %d, Goles: %d, Asistencias: %d\n",
               sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4));
        if (tipo && tipo[0] == 'a')
        {
            hay_alto = 1;
        }
        else
        {
            hay_bajo = 1;
        }
    }
    sqlite3_finalize(stmt);

    if (!hay_alto)
    {
        printf("No se encontraron partidos atipicos altos.\n");
    }
    if (!hay_bajo)
    {
        if (hay_alto)
        {
            printf("\nPartidos con rendimiento excepcionalmente bajo (-2 desviaciones):\n");
            printf("----------------------------------------\n");
        }
        printf("No se encontraron partidos atipicos bajos.\n");
    }

    pause_console();
}

void mostrar_dependencia_contexto(void)
{
    clear_screen();
    print_header("DEPENDENCIA DEL CONTEXTO");

    printf("\nRendimiento por contexto:\n");
    printf("----------------------------------------\n");

    // Rendimiento por clima
    query("Rendimiento por Clima",
          "SELECT clima, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY clima "
          "ORDER BY AVG(rendimiento_general) DESC");

    // Rendimiento por dia de semana
    query("Rendimiento por Dia de Semana",
          "SELECT CASE strftime('%w', fecha_hora) WHEN '0' THEN 'Domingo' WHEN '1' THEN 'Lunes' "
          "WHEN '2' THEN 'Martes' WHEN '3' THEN 'Miercoles' WHEN '4' THEN 'Jueves' WHEN '5' THEN "
          "'Viernes' WHEN '6' THEN 'Sabado' ELSE 'Desconocido' END AS dia, "
          "ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY dia ORDER BY "
          "AVG(rendimiento_general) DESC");

    // Rendimiento por resultado
    query("Rendimiento por Resultado",
          "SELECT CASE resultado WHEN 1 THEN 'Victoria' WHEN 2 THEN 'Empate' WHEN 3 THEN 'Derrota' "
          "ELSE 'Desconocido' END AS resultado, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM "
          "partido GROUP BY resultado ORDER BY AVG(rendimiento_general) DESC");

    pause_console();
}

void mostrar_impacto_real_cansancio(void)
{
    clear_screen();
    print_header("IMPACTO REAL DEL CANSANCIO");

    // Correlacion entre cansancio y rendimiento
    query("Correlacion Cansancio-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(cansancio * rendimiento_general) - SUM(cansancio) * "
          "SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(cansancio * cansancio) - SUM(cansancio) * SUM(cansancio)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * "
          "SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Rendimiento por nivel de cansancio
    query("Rendimiento por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio "
          "(4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(goles), 2) AS goles_promedio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 "
          "THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY rendimiento_promedio DESC");

    // Impacto en resultados
    query("Resultados por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio "
          "(4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
          "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
          "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
          "COUNT(*) AS total "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 "
          "THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END");

    pause_console();
}

void mostrar_impacto_real_estado_animo(void)
{
    clear_screen();
    print_header("IMPACTO REAL DEL ESTADO DE aNIMO");

    // Correlacion entre estado de animo y rendimiento
    query("Correlacion Estado de Animo-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(estado_animo * rendimiento_general) - SUM(estado_animo) * "
          "SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(estado_animo * estado_animo) - SUM(estado_animo) * "
          "SUM(estado_animo)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * "
          "SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Rendimiento por nivel de estado de animo
    query("Rendimiento por Nivel de Estado de Animo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio "
          "(4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(goles), 2) AS goles_promedio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo "
          "<= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY rendimiento_promedio DESC");

    // Impacto en resultados
    query("Resultados por Nivel de Estado de Animo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio "
          "(4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, "
          "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
          "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
          "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
          "COUNT(*) AS total "
          "FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo "
          "<= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END");

    pause_console();
}

void mostrar_eficiencia_goles_vs_rendimiento(void)
{
    clear_screen();
    print_header("EFICIENCIA: GOLES POR PARTIDO VS RENDIMIENTO");

    // Correlacion entre goles y rendimiento
    query("Correlacion Goles-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(goles * rendimiento_general) - SUM(goles) * "
          "SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(goles * goles) - SUM(goles) * SUM(goles)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * "
          "SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Eficiencia por rango de goles
    query("Eficiencia por Rango de Goles",
          "SELECT CASE WHEN goles = 0 THEN '0 goles' WHEN goles <= 2 THEN '1-2 goles' WHEN goles "
          "<= 4 THEN '3-4 goles' ELSE '5+ goles' END AS rango_goles, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN goles = 0 THEN '0 goles' WHEN goles <= 2 THEN '1-2 "
          "goles' WHEN goles <= 4 THEN '3-4 goles' ELSE '5+ goles' END "
          "ORDER BY rendimiento_promedio DESC");

    // Rendimiento por gol (eficiencia)
    query(
        "Rendimiento por Gol (Eficiencia)",
        "SELECT ROUND(AVG(rendimiento_general) / NULLIF(AVG(goles), 0), 2) AS rendimiento_por_gol "
        "FROM partido WHERE goles > 0");

    pause_console();
}

void mostrar_eficiencia_asistencias_vs_cansancio(void)
{
    clear_screen();
    print_header("EFICIENCIA: ASISTENCIAS VS CANSANCIO");

    // Correlacion entre asistencias y cansancio
    query(
        "Correlacion Asistencias-Cansancio",
        "SELECT ROUND((COUNT(*) * SUM(asistencias * cansancio) - SUM(asistencias) * "
        "SUM(cansancio)) / "
        "(SQRT((COUNT(*) * SUM(asistencias * asistencias) - SUM(asistencias) * SUM(asistencias)) * "
        "(COUNT(*) * SUM(cansancio * cansancio) - SUM(cansancio) * SUM(cansancio)))), 4) "
        "FROM partido");

    // Asistencias por nivel de cansancio
    query("Asistencias por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio "
          "(4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "ROUND(AVG(asistencias) / NULLIF(AVG(cansancio), 0), 2) AS "
          "asistencias_por_unidad_cansancio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 "
          "THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY asistencias_promedio DESC");

    pause_console();
}

void mostrar_rendimiento_por_esfuerzo(void)
{
    clear_screen();
    print_header("RENDIMIENTO POR INTENSIDAD");

    query("Correlacion Intensidad-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(intensidad * rendimiento_general) - SUM(intensidad) * "
          "SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(intensidad * intensidad) - SUM(intensidad) * SUM(intensidad)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * "
          "SUM(rendimiento_general)))), 4) "
          "FROM partido WHERE intensidad > 0");

    query("Rendimiento por Nivel de Intensidad",
          "SELECT CASE WHEN intensidad <= 3 THEN 'Baja (1-3)' WHEN intensidad <= 7 THEN 'Media "
          "(4-7)' ELSE 'Alta (8-10)' END AS nivel_intensidad, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio "
          "FROM partido WHERE intensidad > 0 "
          "GROUP BY CASE WHEN intensidad <= 3 THEN 'Baja (1-3)' WHEN intensidad <= 7 THEN 'Media "
          "(4-7)' ELSE 'Alta (8-10)' END "
          "ORDER BY rendimiento_promedio DESC");

    query("Contribucion por Nivel de Intensidad",
          "SELECT CASE WHEN intensidad <= 3 THEN 'Baja (1-3)' WHEN intensidad <= 7 THEN 'Media "
          "(4-7)' ELSE 'Alta (8-10)' END AS nivel_intensidad, "
          "ROUND(AVG(goles + asistencias), 2) AS contribucion_promedio "
          "FROM partido WHERE intensidad > 0 "
          "GROUP BY CASE WHEN intensidad <= 3 THEN 'Baja (1-3)' WHEN intensidad <= 7 THEN 'Media "
          "(4-7)' ELSE 'Alta (8-10)' END "
          "ORDER BY contribucion_promedio DESC");

    pause_console();
}

void mostrar_rendimiento_por_dolor_fisico(void)
{
    clear_screen();
    print_header("RENDIMIENTO POR DOLOR FISICO");

    query("Rendimiento por Nivel de Dolor",
          "SELECT CASE dolor_fisico WHEN 0 THEN '0 Ninguna' WHEN 1 THEN '1 Leve' WHEN 2 THEN '2 "
          "Moderada' WHEN 3 THEN '3 Fuerte' ELSE 'Sin dato' END AS nivel_dolor, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio "
          "FROM partido "
          "GROUP BY dolor_fisico "
          "ORDER BY dolor_fisico ASC");

    query("Contribucion por Nivel de Dolor",
          "SELECT CASE dolor_fisico WHEN 0 THEN '0 Ninguna' WHEN 1 THEN '1 Leve' WHEN 2 THEN '2 "
          "Moderada' WHEN 3 THEN '3 Fuerte' ELSE 'Sin dato' END AS nivel_dolor, "
          "ROUND(AVG(goles + asistencias), 2) AS contribucion_promedio "
          "FROM partido "
          "GROUP BY dolor_fisico "
          "ORDER BY dolor_fisico ASC");

    query("Partidos por Nivel de Dolor",
          "SELECT CASE dolor_fisico WHEN 0 THEN '0 Ninguna' WHEN 1 THEN '1 Leve' WHEN 2 THEN '2 "
          "Moderada' WHEN 3 THEN '3 Fuerte' ELSE 'Sin dato' END AS nivel_dolor, "
          "COUNT(*) AS partidos "
          "FROM partido "
          "GROUP BY dolor_fisico "
          "ORDER BY dolor_fisico ASC");

    pause_console();
}

void mostrar_rendimiento_por_arbitraje(void)
{
    clear_screen();
    print_header("RENDIMIENTO POR ARBITRAJE");

    query("Rendimiento por Calidad de Arbitraje",
          "SELECT CASE arbitraje_score WHEN 1 THEN '1 Muy malo' WHEN 2 THEN '2 Regular' WHEN 3 "
          "THEN '3 Normal' WHEN 4 THEN '4 Bueno' WHEN 5 THEN '5 Excelente' ELSE 'Sin dato' END AS "
          "arbitraje, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio "
          "FROM partido WHERE arbitraje_score BETWEEN 1 AND 5 "
          "GROUP BY arbitraje_score "
          "ORDER BY arbitraje_score ASC");

    query("Victorias por Calidad de Arbitraje (%)",
          "SELECT CASE arbitraje_score WHEN 1 THEN '1 Muy malo' WHEN 2 THEN '2 Regular' WHEN 3 "
          "THEN '3 Normal' WHEN 4 THEN '4 Bueno' WHEN 5 THEN '5 Excelente' ELSE 'Sin dato' END AS "
          "arbitraje, "
          "ROUND(100.0 * AVG(CASE WHEN resultado = 1 THEN 1.0 ELSE 0.0 END), 2) AS victorias_pct "
          "FROM partido WHERE arbitraje_score BETWEEN 1 AND 5 "
          "GROUP BY arbitraje_score "
          "ORDER BY arbitraje_score ASC");

    query("Partidos por Calidad de Arbitraje",
          "SELECT CASE arbitraje_score WHEN 1 THEN '1 Muy malo' WHEN 2 THEN '2 Regular' WHEN 3 "
          "THEN '3 Normal' WHEN 4 THEN '4 Bueno' WHEN 5 THEN '5 Excelente' ELSE 'Sin dato' END AS "
          "arbitraje, "
          "COUNT(*) AS partidos "
          "FROM partido WHERE arbitraje_score BETWEEN 1 AND 5 "
          "GROUP BY arbitraje_score "
          "ORDER BY arbitraje_score ASC");

    pause_console();
}

void mostrar_rendimiento_por_temperatura(void)
{
    clear_screen();
    print_header("RENDIMIENTO POR TEMPERATURA");

    query("Cobertura de Temperatura Registrada (%)",
          "SELECT ROUND(100.0 * SUM(CASE WHEN temperatura_c IS NOT NULL THEN 1 ELSE 0 END) / "
          "NULLIF(COUNT(*), 0), 2) "
          "FROM partido");

    query("Rendimiento por Rango de Temperatura",
          "SELECT CASE WHEN temperatura_c < 10 THEN '<10 C' WHEN temperatura_c < 20 THEN '10-19 C' "
          "WHEN temperatura_c < 30 THEN '20-29 C' ELSE '30+ C' END AS rango_temp, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio "
          "FROM partido WHERE temperatura_c IS NOT NULL "
          "GROUP BY CASE WHEN temperatura_c < 10 THEN '<10 C' WHEN temperatura_c < 20 THEN '10-19 "
          "C' WHEN temperatura_c < 30 THEN '20-29 C' ELSE '30+ C' END "
          "ORDER BY rendimiento_promedio DESC");

    query("Correlacion Temperatura-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(temperatura_c * rendimiento_general) - SUM(temperatura_c) "
          "* SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(temperatura_c * temperatura_c) - SUM(temperatura_c) * "
          "SUM(temperatura_c)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * "
          "SUM(rendimiento_general)))), 4) "
          "FROM partido WHERE temperatura_c IS NOT NULL");

    pause_console();
}

/*
 * Rendimiento por condiciones reales de clima (columnas de Open-Meteo).
 *
 * Solo cuentan los partidos con weather_code no NULL: es la marca de que el
 * clima se descargo de Open-Meteo (las columnas son DEFAULT NULL).
 *
 * Umbrales (los valores guardados son del dia completo del partido, ver
 * openmeteo_client.c):
 * - precip_mm es la SUMA de las 24 horas del dia. Open-Meteo informa con
 *   resolucion de 0.1 mm, asi que < 0.1 es "sin lluvia"; hasta 5 mm en el dia
 *   es lluvia leve (lloviznas o chaparrones cortos) y > 5 mm es moderada o
 *   fuerte (cancha mojada de verdad).
 * - wind_kmh es el PROMEDIO de las horas del dia a 10 m: < 10 calmo,
 *   10 a 25 moderado, > 25 fuerte.
 * - weather_code es el codigo WMO mas frecuente del dia: 0-1 despejado,
 *   2-3 nublado, 45/48 niebla, 51-67 y 80-82 llovizna/lluvia, 71-77 y 85-86
 *   nieve, 95-99 tormenta. Un codigo fuera de esas listas no entra en ningun
 *   grupo de cielo (si en lluvia y viento).
 */

/* Filtro opcional por anio calendario sobre la fecha normalizada (acepta
 * "aaaa-mm-dd" y "dd/mm/aaaa"). El parametro ?1 vale 0 para no filtrar. */
#define CLIMA_REAL_FILTRO_ANIO \
    "(?1 = 0 OR CAST(substr(" SQL_FECHA_ORD("fecha_hora") ", 1, 4) AS INTEGER) = ?1)"

/* Columnas de cada grupo: indice, partidos, y cantidad + promedio de cada
 * metrica (AVG y COUNT ignoran los NULL), resultados cargados y victorias. */
#define CLIMA_REAL_SQL(expr_grupo, columna)                                                        \
    "SELECT g, COUNT(*), COUNT(rendimiento_general), AVG(rendimiento_general), "                  \
    "COUNT(goles), AVG(goles), COUNT(asistencias), AVG(asistencias), "                            \
    "SUM(CASE WHEN resultado IN (1, 2, 3) THEN 1 ELSE 0 END), "                                   \
    "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) "                                             \
    "FROM (SELECT " expr_grupo " AS g, rendimiento_general, goles, asistencias, resultado "       \
    "FROM partido WHERE weather_code IS NOT NULL AND " columna " IS NOT NULL "      \
    "AND " CLIMA_REAL_FILTRO_ANIO ") "                                                            \
    "WHERE g IS NOT NULL GROUP BY g"

static const char *const SQL_CLIMA_REAL_LLUVIA =
    CLIMA_REAL_SQL("CASE WHEN precip_mm < 0.1 THEN 0 WHEN precip_mm <= 5.0 THEN 1 ELSE 2 END",
                   "precip_mm");

static const char *const SQL_CLIMA_REAL_VIENTO =
    CLIMA_REAL_SQL("CASE WHEN wind_kmh < 10.0 THEN 0 WHEN wind_kmh <= 25.0 THEN 1 ELSE 2 END",
                   "wind_kmh");

static const char *const SQL_CLIMA_REAL_CIELO = CLIMA_REAL_SQL(
            "CASE WHEN weather_code BETWEEN 0 AND 1 THEN 0 "
            "WHEN weather_code BETWEEN 2 AND 3 THEN 1 "
            "WHEN weather_code IN (45, 48) THEN 2 "
            "WHEN weather_code BETWEEN 51 AND 67 OR weather_code BETWEEN 80 AND 82 THEN 3 "
            "WHEN weather_code BETWEEN 71 AND 77 OR weather_code BETWEEN 85 AND 86 THEN 4 "
            "WHEN weather_code BETWEEN 95 AND 99 THEN 5 END",
            "weather_code");

static const char *const ETIQUETAS_LLUVIA[CLIMA_REAL_GRUPOS_LLUVIA] =
{
    "Sin lluvia", "Lluvia leve", "Lluvia moderada/fuerte"
};

static const char *const ETIQUETAS_VIENTO[CLIMA_REAL_GRUPOS_VIENTO] =
{
    "Viento calmo", "Viento moderado", "Viento fuerte"
};

static const char *const ETIQUETAS_CIELO[CLIMA_REAL_GRUPOS_CIELO] =
{
    "Despejado", "Nublado", "Niebla", "Llovizna/Lluvia", "Nieve", "Tormenta"
};

static void iniciar_grupos_clima_real(ClimaRealGrupo *grupos, const char *const *etiquetas,
                                      int cantidad)
{
    for (int i = 0; i < cantidad; i++)
    {
        ClimaRealGrupo vacio = {0};
        vacio.etiqueta = etiquetas[i];
        grupos[i] = vacio;
    }
}

static int cargar_grupos_clima_real(const char *sql, ClimaRealGrupo *grupos, int cantidad,
                                    int anio)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt_export(&stmt, sql))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, anio);

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        int indice = sqlite3_column_int(stmt, 0);
        if (indice < 0 || indice >= cantidad)
        {
            continue;
        }
        ClimaRealGrupo *g = &grupos[indice];
        g->partidos = sqlite3_column_int(stmt, 1);
        g->con_rendimiento = sqlite3_column_int(stmt, 2);
        g->rendimiento_promedio = sqlite3_column_double(stmt, 3);
        g->con_goles = sqlite3_column_int(stmt, 4);
        g->goles_promedio = sqlite3_column_double(stmt, 5);
        g->con_asistencias = sqlite3_column_int(stmt, 6);
        g->asistencias_promedio = sqlite3_column_double(stmt, 7);
        g->con_resultado = sqlite3_column_int(stmt, 8);
        int victorias = sqlite3_column_int(stmt, 9);
        g->victorias_pct = g->con_resultado > 0 ? 100.0 * victorias / g->con_resultado : 0.0;
    }

    db_stmt_release(stmt);
    return rc == SQLITE_DONE;
}

static int cargar_cobertura_clima_real(ClimaRealStats *stats, int anio)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt_export(&stmt,
                              "SELECT COUNT(*), "
                              "COALESCE(SUM(CASE WHEN weather_code IS NOT NULL THEN 1 ELSE 0 END), 0) "
                              "FROM partido WHERE " CLIMA_REAL_FILTRO_ANIO))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, anio);

    int ok = sqlite3_step(stmt) == SQLITE_ROW;
    if (ok)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->partidos_con_datos = sqlite3_column_int(stmt, 1);
        stats->cobertura_pct = stats->total_partidos > 0
                               ? 100.0 * stats->partidos_con_datos / stats->total_partidos
                               : 0.0;
    }
    db_stmt_release(stmt);
    return ok;
}

/* Un grupo compite por "mejor condicion" si tiene suficientes partidos y al
 * menos un rendimiento cargado. Empate de rendimiento: gana el de mas partidos;
 * si tambien empatan, el primero en orden logico. */
static void elegir_mejor_clima_real(const ClimaRealGrupo *grupos, int cantidad,
                                    const ClimaRealGrupo **mejor)
{
    for (int i = 0; i < cantidad; i++)
    {
        const ClimaRealGrupo *g = &grupos[i];
        if (g->partidos < CLIMA_REAL_MIN_PARTIDOS_MEJOR || g->con_rendimiento == 0)
        {
            continue;
        }
        if (!*mejor || g->rendimiento_promedio > (*mejor)->rendimiento_promedio ||
                (g->rendimiento_promedio == (*mejor)->rendimiento_promedio &&
                 g->partidos > (*mejor)->partidos))
        {
            *mejor = g;
        }
    }
}

int calcular_rendimiento_clima_real(ClimaRealStats *stats)
{
    return calcular_rendimiento_clima_real_anio(stats, 0);
}

int calcular_rendimiento_clima_real_anio(ClimaRealStats *stats, int anio)
{
    if (!stats || anio < 0)
    {
        return 0;
    }

    ClimaRealStats vacio = {0};
    *stats = vacio;
    iniciar_grupos_clima_real(stats->lluvia, ETIQUETAS_LLUVIA, CLIMA_REAL_GRUPOS_LLUVIA);
    iniciar_grupos_clima_real(stats->viento, ETIQUETAS_VIENTO, CLIMA_REAL_GRUPOS_VIENTO);
    iniciar_grupos_clima_real(stats->cielo, ETIQUETAS_CIELO, CLIMA_REAL_GRUPOS_CIELO);

    if (!cargar_cobertura_clima_real(stats, anio) ||
            !cargar_grupos_clima_real(SQL_CLIMA_REAL_LLUVIA, stats->lluvia,
                                      CLIMA_REAL_GRUPOS_LLUVIA, anio) ||
            !cargar_grupos_clima_real(SQL_CLIMA_REAL_VIENTO, stats->viento,
                                      CLIMA_REAL_GRUPOS_VIENTO, anio) ||
            !cargar_grupos_clima_real(SQL_CLIMA_REAL_CIELO, stats->cielo, CLIMA_REAL_GRUPOS_CIELO,
                                      anio))
    {
        return 0;
    }

    elegir_mejor_clima_real(stats->lluvia, CLIMA_REAL_GRUPOS_LLUVIA, &stats->mejor);
    elegir_mejor_clima_real(stats->viento, CLIMA_REAL_GRUPOS_VIENTO, &stats->mejor);
    elegir_mejor_clima_real(stats->cielo, CLIMA_REAL_GRUPOS_CIELO, &stats->mejor);
    return 1;
}

static void imprimir_promedio_clima_real(int cantidad, double valor)
{
    if (cantidad > 0)
    {
        printf(" %7.2f", valor);
    }
    else
    {
        printf(" %7s", "N/A");
    }
}

static void imprimir_grupos_clima_real(const char *titulo, const ClimaRealGrupo *grupos,
                                       int cantidad)
{
    printf("\n%s\n", titulo);
    printf("----------------------------------------------------------------\n");
    printf("%-24s %4s %7s %7s %7s %7s\n", "Condicion", "PJ", "Rend.", "Goles", "Asist.", "%Vict.");

    int mostrados = 0;
    for (int i = 0; i < cantidad; i++)
    {
        const ClimaRealGrupo *g = &grupos[i];
        if (g->partidos == 0)
        {
            continue;
        }
        printf("%-24s %4d", g->etiqueta, g->partidos);
        imprimir_promedio_clima_real(g->con_rendimiento, g->rendimiento_promedio);
        imprimir_promedio_clima_real(g->con_goles, g->goles_promedio);
        imprimir_promedio_clima_real(g->con_asistencias, g->asistencias_promedio);
        if (g->con_resultado > 0)
        {
            printf(" %6.1f%%\n", g->victorias_pct);
        }
        else
        {
            printf(" %7s\n", "N/A");
        }
        mostrados++;
    }

    if (mostrados == 0)
    {
        printf("Sin datos para este grupo.\n");
    }
}

void mostrar_rendimiento_por_clima_real(void)
{
    clear_screen();
    print_header("RENDIMIENTO POR CONDICIONES REALES (OPEN-METEO)");

    ClimaRealStats stats;
    if (!calcular_rendimiento_clima_real(&stats))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    printf("\nCobertura de clima real: %d de %d partidos (%.2f%%)\n", stats.partidos_con_datos,
           stats.total_partidos, stats.cobertura_pct);

    if (stats.partidos_con_datos == 0)
    {
        printf("\n");
        mostrar_no_hay_registros("partidos con clima real");
        printf("El clima se completa automaticamente al crear partidos en una cancha "
               "geolocalizada.\n");
        pause_console();
        return;
    }

    imprimir_grupos_clima_real("Por Lluvia del Dia", stats.lluvia, CLIMA_REAL_GRUPOS_LLUVIA);
    imprimir_grupos_clima_real("Por Viento Promedio del Dia", stats.viento,
                               CLIMA_REAL_GRUPOS_VIENTO);
    imprimir_grupos_clima_real("Por Condicion del Cielo", stats.cielo, CLIMA_REAL_GRUPOS_CIELO);

    if (stats.mejor)
    {
        printf("\nTu mejor condicion: %s (rendimiento %.2f, %d partidos)\n", stats.mejor->etiqueta,
               stats.mejor->rendimiento_promedio, stats.mejor->partidos);
    }
    else
    {
        printf("\nTu mejor condicion: sin datos suficientes (hace falta un grupo con al menos %d "
               "partidos).\n",
               CLIMA_REAL_MIN_PARTIDOS_MEJOR);
    }

    pause_console();
}

void mostrar_partidos_exigentes_bien_rendidos(void)
{
    mostrar_partidos_con_sql(
        "PARTIDOS EXIGENTES BIEN RENDIDOS", "Partidos con alto cansancio y buen rendimiento:",
        "SELECT id, fecha_hora, cansancio, rendimiento_general, goles, asistencias, resultado "
        "FROM partido "
        "WHERE cansancio > 7 AND rendimiento_general > (SELECT AVG(rendimiento_general) FROM "
        "partido) "
        "ORDER BY rendimiento_general DESC, cansancio DESC");
}

void mostrar_partidos_faciles_mal_rendidos(void)
{
    mostrar_partidos_con_sql(
        "PARTIDOS FACILES MAL RENDIDOS", "Partidos con bajo cansancio y bajo rendimiento:",
        "SELECT id, fecha_hora, cansancio, rendimiento_general, goles, asistencias, resultado "
        "FROM partido "
        "WHERE cansancio <= 3 AND rendimiento_general < (SELECT AVG(rendimiento_general) FROM "
        "partido) "
        "ORDER BY rendimiento_general ASC, cansancio ASC");
}

void mostrar_tendencia_rendimiento_sparkline(void)
{
    clear_screen();
    print_header("TENDENCIA DE RENDIMIENTO (SPARKLINE)");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha_hora, rendimiento_general, goles, asistencias, resultado "
                      "FROM partido WHERE resultado > 0 ORDER BY fecha_hora DESC LIMIT 30";

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    double valores[30];
    int ids[30];
    char fechas[30][12];
    int goles_arr[30];
    int asis_arr[30];
    int resultados[30];
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < 30)
    {
        ids[count] = sqlite3_column_int(stmt, 0);
        const char *f = (const char *)sqlite3_column_text(stmt, 1);
        snprintf(fechas[count], sizeof(fechas[count]), "%s", f ? f : "");
        fechas[count][10] = '\0';
        valores[count] = (double)sqlite3_column_int(stmt, 2);
        goles_arr[count] = sqlite3_column_int(stmt, 3);
        asis_arr[count] = sqlite3_column_int(stmt, 4);
        resultados[count] = sqlite3_column_int(stmt, 5);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
    {
        printf("No hay partidos registrados.\n");
        pause_console();
        return;
    }

    // Mostrar datos en orden cronologico (el mas antiguo primero)
    printf("\nUltimos %d partidos - Tendencia de Rendimiento:\n\n", count);
    for (int i = count - 1; i >= 0; i--)
    {
        const char *res = "?";
        if (resultados[i] == 1)
        {
            res = "V";
        }
        else if (resultados[i] == 2)
        {
            res = "E";
        }
        else if (resultados[i] == 3)
        {
            res = "D";
        }
        printf("  #%d %s | G:%d A:%d | R:%d %s\n", ids[i], fechas[i], goles_arr[i], asis_arr[i],
               (int)valores[i], res);
    }

    double suma = 0;
    for (int i = 0; i < count; i++)
    {
        suma += valores[i];
    }

    // Sparkline
    printf("\n  Tendencia: ");
    dibujar_minigrafico(valores, count, count > 20 ? 20 : count);
    printf("\n\n  ▁ Min: %.0f  █ Max: %.0f  Promedio: %.1f\n", chart_min(valores, count),
           chart_max(valores, count), count > 0 ? suma / count : 0.0);

    pause_console();
}

void mostrar_efectividad_general(void)
{
    clear_screen();
    print_header("CALCULADORA DE EFECTIVIDAD");

    query("Promedio de Goles por Partido",
          "SELECT ROUND(AVG(goles), 2) FROM partido WHERE resultado > 0");

    query("Promedio de Asistencias por Partido",
          "SELECT ROUND(AVG(asistencias), 2) FROM partido WHERE resultado > 0");

    query("Promedio de Goles + Asistencias por Partido",
          "SELECT ROUND(AVG(goles + asistencias), 2) FROM partido WHERE resultado > 0");

    query("Efectividad de Conversion (Goles por Oportunidad)",
          "SELECT ROUND(CAST(SUM(goles) AS REAL) / NULLIF(SUM(goles + asistencias), 0), 4) "
          "FROM partido WHERE resultado > 0 AND (goles + asistencias) > 0");

    query("Tasa de Victoria General (%)", "SELECT ROUND(100.0 * SUM(CASE WHEN resultado = 1 THEN 1 "
          "ELSE 0 END) / NULLIF(COUNT(*), 0), 2) "
          "FROM partido WHERE resultado > 0");

    query("Efectividad Ofensiva (Goles + Asistencias por Rendimiento)",
          "SELECT ROUND(AVG(goles + asistencias) / NULLIF(AVG(rendimiento_general), 0), 4) "
          "FROM partido WHERE resultado > 0 AND rendimiento_general > 0");

    query("Efectividad por Rango de Rendimiento",
          "SELECT CASE WHEN rendimiento_general <= 3 THEN 'Bajo (1-3)' "
          "WHEN rendimiento_general <= 6 THEN 'Medio (4-6)' ELSE 'Alto (7-10)' END, "
          "ROUND(AVG(goles), 2), ROUND(AVG(asistencias), 2), "
          "ROUND(100.0 * AVG(CASE WHEN resultado = 1 THEN 1.0 ELSE 0.0 END), 2) "
          "FROM partido WHERE resultado > 0 "
          "GROUP BY CASE WHEN rendimiento_general <= 3 THEN 'Bajo (1-3)' "
          "WHEN rendimiento_general <= 6 THEN 'Medio (4-6)' ELSE 'Alto (7-10)' END "
          "ORDER BY rendimiento_general ASC");

    query("Partidos con Mayor Efectividad Ofensiva (Top 5)",
          "SELECT p.fecha_hora, p.goles, p.asistencias, (p.goles + p.asistencias) "
          "FROM partido p WHERE p.resultado > 0 "
          "ORDER BY (p.goles + p.asistencias) DESC LIMIT 5");

    pause_console();
}
