/**
 * @file export_all.c
 * @brief Módulo para exportar todos los datos del sistema en múltiples formatos.
 *
 * Este archivo contiene la función principal para exportar camisetas, partidos,
 * lesiones y estadísticas en formatos CSV, TXT, JSON y HTML.
 */

#include "export_all.h"
#include "export_all_mejorado.h"
#include "export.h"
#include "export_camisetas.h"
#include "export_partidos.h"
#include "export_lesiones.h"
#include "export_estadisticas.h"
#include "export_estadisticas_generales.h"
#include "export_records_rankings.h"
#include "utils.h"
#include "menu.h"
#include "ascii_art.h"
#include <stdio.h>

/**
 * @brief Exportación integral de datos de camisetas
 *
 * Centraliza la exportación de datos de camisetas en todos los formatos disponibles,
 * proporcionando una solución completa para el análisis de rendimiento de jugadores.
 * Esto es esencial para equipos que necesitan evaluar múltiples aspectos del desempeño
 * en diferentes formatos para diferentes usos (hojas de cálculo, informes, APIs, visualización web).
 */
static void exportar_camisetas_todo()
{
    printf("Exportando camisetas...\n");
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();
    printf("Exportacion de camisetas completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de datos de partidos
 *
 * Proporciona una exportación completa de todos los partidos registrados en el sistema,
 * permitiendo un análisis exhaustivo del historial de encuentros. Esto es crucial para
 * evaluar el rendimiento del equipo a lo largo del tiempo y bajo diferentes condiciones.
 */
static void exportar_partidos_todo()
{
    printf("Exportando partidos...\n");
    exportar_partidos_csv();
    exportar_partidos_txt();
    exportar_partidos_json();
    exportar_partidos_html();
    printf("Exportacion de partidos completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de datos de lesiones
 *
 * Centraliza la exportación de datos de lesiones en todos los formatos, proporcionando
 * información crucial para el análisis médico y de rendimiento. Esto permite a los equipos
 * médicos y entrenadores evaluar el impacto de las lesiones en el desempeño de los jugadores.
 */
static void exportar_lesiones_todo()
{
    printf("Exportando lesiones...\n");
    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();
    printf("Exportacion de lesiones completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de estadísticas básicas
 *
 * Proporciona una exportación completa de estadísticas básicas en todos los formatos,
 * ofreciendo una visión general del rendimiento del equipo. Esto es útil para informes
 * rápidos y análisis preliminares del desempeño general.
 */
static void exportar_estadisticas_todo()
{
    printf("Exportando estadisticas...\n");
    exportar_estadisticas_csv();
    exportar_estadisticas_txt();
    exportar_estadisticas_json();
    exportar_estadisticas_html();
    printf("Exportacion de estadisticas completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de análisis avanzados
 *
 * Centraliza la exportación de análisis avanzados en todos los formatos, proporcionando
 * información detallada para la toma de decisiones estratégicas. Esto es esencial para
 * equipos que necesitan un análisis profundo del rendimiento y patrones de juego.
 */
static void exportar_analisis_todo()
{
    printf("Exportando analisis...\n");
    exportar_analisis_csv();
    exportar_analisis_txt();
    exportar_analisis_json();
    exportar_analisis_html();
    printf("Exportacion de analisis completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de estadísticas generales
 *
 * Proporciona una exportación completa de estadísticas generales en todos los formatos,
 * ofreciendo una visión agregada del rendimiento del equipo. Esto es útil para informes
 * ejecutivos y análisis de alto nivel del desempeño general del equipo.
 */
static void exportar_estadisticas_generales_todo()
{
    printf("Exportando estadisticas generales...\n");
    exportar_estadisticas_generales_csv();
    exportar_estadisticas_generales_txt();
    exportar_estadisticas_generales_json();
    exportar_estadisticas_generales_html();
    printf("Exportacion de estadisticas generales completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de estadísticas mensuales
 *
 * Centraliza la exportación de estadísticas por mes en todos los formatos, permitiendo
 * un análisis temporal detallado del rendimiento. Esto es esencial para identificar
 * patrones estacionales y evaluar el progreso mensual del equipo.
 */
static void exportar_estadisticas_por_mes_todo()
{
    printf("Exportando estadisticas por mes...\n");
    exportar_estadisticas_por_mes_csv();
    exportar_estadisticas_por_mes_txt();
    exportar_estadisticas_por_mes_json();
    exportar_estadisticas_por_mes_html();
    printf("Exportacion de estadisticas por mes completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de estadísticas anuales
 *
 * Proporciona una exportación completa de estadísticas por año en todos los formatos,
 * permitiendo un análisis de largo plazo del rendimiento del equipo. Esto es crucial
 * para evaluar el progreso anual y planificar estrategias a largo plazo.
 */
static void exportar_estadisticas_por_anio_todo()
{
    printf("Exportando estadisticas por anio...\n");
    exportar_estadisticas_por_anio_csv();
    exportar_estadisticas_por_anio_txt();
    exportar_estadisticas_por_anio_json();
    exportar_estadisticas_por_anio_html();
    printf("Exportacion de estadisticas por anio completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral de récords y rankings
 *
 * Centraliza la exportación de récords y rankings en todos los formatos, proporcionando
 * información sobre los mejores y peores desempeños. Esto es esencial para identificar
 * patrones de éxito y áreas de mejora, así como para celebrar logros y establecer metas.
 */
static void exportar_records_rankings_todo()
{
    printf("Exportando records & rankings...\n");
    exportar_record_goles_partido_csv();
    exportar_record_asistencias_partido_csv();
    exportar_mejor_combinacion_cancha_camiseta_csv();
    exportar_peor_combinacion_cancha_camiseta_csv();
    exportar_mejor_temporada_csv();
    exportar_peor_temporada_csv();
    exportar_records_rankings_txt();
    exportar_records_rankings_json();
    exportar_records_rankings_html();
    printf("Exportacion de records & rankings completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral del partido con más goles
 *
 * Proporciona una exportación completa del partido con mayor anotación en todos los formatos,
 * permitiendo analizar las condiciones y factores que llevaron al mejor desempeño ofensivo.
 * Esto es útil para replicar estrategias exitosas en futuros encuentros.
 */
static void exportar_partido_mas_goles_todo()
{
    printf("Exportando partido con mas goles...\n");
    exportar_partido_mas_goles_csv();
    exportar_partido_mas_goles_txt();
    exportar_partido_mas_goles_json();
    exportar_partido_mas_goles_html();
    printf("Exportacion completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exporta partido con más asistencias en todos los formatos.
 */
static void exportar_partido_mas_asistencias_todo()
{
    printf("Exportando partido con mas asistencias...\n");
    exportar_partido_mas_asistencias_csv();
    exportar_partido_mas_asistencias_txt();
    exportar_partido_mas_asistencias_json();
    exportar_partido_mas_asistencias_html();
    printf("Exportacion completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exporta partido más reciente con menos goles en todos los formatos.
 */
static void exportar_partido_menos_goles_reciente_todo()
{
    printf("Exportando partido menos goles reciente...\n");
    exportar_partido_menos_goles_reciente_csv();
    exportar_partido_menos_goles_reciente_txt();
    exportar_partido_menos_goles_reciente_json();
    exportar_partido_menos_goles_reciente_html();
    printf("Exportacion completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación integral del partido más reciente con menos asistencias
 *
 * Proporciona una exportación completa del partido más reciente con menor número de asistencias
 * en todos los formatos, permitiendo analizar las condiciones que llevaron a un bajo desempeño
 * en la creación de oportunidades de gol. Esto es útil para identificar áreas de mejora
 * en el juego en equipo y la estrategia ofensiva.
 */
static void exportar_partido_menos_asistencias_reciente_todo()
{
    printf("Exportando partido menos asistencias reciente...\n");
    exportar_partido_menos_asistencias_reciente_csv();
    exportar_partido_menos_asistencias_reciente_txt();
    exportar_partido_menos_asistencias_reciente_json();
    exportar_partido_menos_asistencias_reciente_html();
    printf("Exportacion completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Exportación completa de todos los datos del sistema
 *
 * Función maestra que realiza una exportación exhaustiva de todos los datos disponibles
 * en el sistema, incluyendo camisetas, partidos, lesiones, estadísticas y análisis.
 * Esta es la función más completa y genera una copia de seguridad integral de todos
 * los datos en múltiples formatos para diferentes usos y aplicaciones.
 *
 * @details La exportación completa incluye:
 * - Todos los tipos de datos: camisetas, partidos, lesiones, estadísticas
 * - Todos los formatos: CSV, TXT, JSON, HTML
 * - Datos específicos: partidos destacados, récords, rankings
 * - Análisis temporales: por mes, por año
 *
 * @note Esta función es la más completa pero también la más lenta y consume más recursos,
 *       ya que exporta todos los datos disponibles en el sistema. Se recomienda usarla
 *       para copias de seguridad completas o cuando se necesiten todos los datos.
 */
static void exportar_todo()
{
    printf("Exportando todo...\n");

    // Exportar camisetas en todos los formatos
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();

    // Exportar partidos en todos los formatos
    exportar_partidos_csv();
    exportar_partidos_txt();
    exportar_partidos_json();
    exportar_partidos_html();

    // Exportar partidos específicos
    exportar_partido_mas_goles_csv();
    exportar_partido_mas_goles_txt();
    exportar_partido_mas_goles_json();
    exportar_partido_mas_goles_html();

    exportar_partido_mas_asistencias_csv();
    exportar_partido_mas_asistencias_txt();
    exportar_partido_mas_asistencias_json();
    exportar_partido_mas_asistencias_html();

    exportar_partido_menos_goles_reciente_csv();
    exportar_partido_menos_goles_reciente_txt();
    exportar_partido_menos_goles_reciente_json();
    exportar_partido_menos_goles_reciente_html();

    exportar_partido_menos_asistencias_reciente_csv();
    exportar_partido_menos_asistencias_reciente_txt();
    exportar_partido_menos_asistencias_reciente_json();
    exportar_partido_menos_asistencias_reciente_html();

    // Exportar lesiones en todos los formatos
    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();

    // Exportar estadísticas básicas
    exportar_estadisticas_csv();
    exportar_estadisticas_txt();
    exportar_estadisticas_json();
    exportar_estadisticas_html();

    // Exportar análisis
    exportar_analisis_csv();
    exportar_analisis_txt();
    exportar_analisis_json();
    exportar_analisis_html();

    // Exportar estadísticas generales
    exportar_estadisticas_generales_csv();
    exportar_estadisticas_generales_txt();
    exportar_estadisticas_generales_json();
    exportar_estadisticas_generales_html();

    // Exportar estadísticas por mes
    exportar_estadisticas_por_mes_csv();
    exportar_estadisticas_por_mes_txt();
    exportar_estadisticas_por_mes_json();
    exportar_estadisticas_por_mes_html();

    // Exportar estadísticas por año
    exportar_estadisticas_por_anio_csv();
    exportar_estadisticas_por_anio_txt();
    exportar_estadisticas_por_anio_json();
    exportar_estadisticas_por_anio_html();

    // Exportar récords y rankings
    exportar_record_goles_partido_csv();
    exportar_record_asistencias_partido_csv();
    exportar_mejor_combinacion_cancha_camiseta_csv();
    exportar_peor_combinacion_cancha_camiseta_csv();
    exportar_mejor_temporada_csv();
    exportar_peor_temporada_csv();
    exportar_records_rankings_txt();
    exportar_records_rankings_json();
    exportar_records_rankings_html();

    printf("Exportacion de todo completada.\n");
    printf("%s\n", ASCII_EXPORT_EXITOSO);
    pause_console();
}

/**
 * @brief Sub-menú para exportar partidos.
 */
static void menu_exportar_partidos()
{
    MenuItem items[] =
    {
        {1, "Todos los Partidos", exportar_partidos_todo},
        {2, "Partido con Mas Goles", exportar_partido_mas_goles_todo},
        {3, "Partido con Mas Asistencias", exportar_partido_mas_asistencias_todo},
        {4, "Partido Menos Goles Reciente", exportar_partido_menos_goles_reciente_todo},
        {5, "Partido Menos Asistencias Reciente", exportar_partido_menos_asistencias_reciente_todo},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR PARTIDOS", items, 6);
}

/**
 * @brief Sub-menú para exportar estadísticas generales.
 */
static void menu_exportar_estadisticas_generales()
{
    MenuItem items[] =
    {
        {1, "Estadisticas Generales", exportar_estadisticas_generales_todo},
        {2, "Estadisticas Por Mes", exportar_estadisticas_por_mes_todo},
        {3, "Estadisticas Por Anio", exportar_estadisticas_por_anio_todo},
        {4, "Records & Rankings", exportar_records_rankings_todo},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR ESTADISTICAS GENERALES", items, 5);
}

/**
 * @brief Menu para exportar datos del sistema en múltiples formatos según selección del usuario.
 *
 * Esta función muestra un menú para que el usuario seleccione qué datos exportar:
 * camisetas, partidos, lesiones, estadísticas, análisis, estadísticas detalladas, todo o volver.
 * Llama a las funciones de exportación correspondientes en formatos CSV, TXT, JSON y HTML.
 * Al final, muestra un mensaje de confirmación y pausa la consola.
 */
void menu_exportar()
{
    MenuItem items[] =
    {
        {1, "Camisetas", exportar_camisetas_todo},
        {2, "Partidos", menu_exportar_partidos},
        {3, "Lesiones", exportar_lesiones_todo},
        {4, "Estadisticas", exportar_estadisticas_todo},
        {5, "Analisis", exportar_analisis_todo},
        {6, "Estadisticas Generales", menu_exportar_estadisticas_generales},
        {7, "Analisis Avanzado", menu_exportar_mejorado},
        {8, "Todo", exportar_todo},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR DATOS", items, 9);
}
