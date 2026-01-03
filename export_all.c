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
#include <stdio.h>

/**
 * @brief Exporta camisetas en todos los formatos.
 */
static void exportar_camisetas_todo()
{
    printf("Exportando camisetas...\n");
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();
    printf("Exportacion de camisetas completada.\n");
    pause_console();
}

/**
 * @brief Exporta partidos en todos los formatos.
 */
static void exportar_partidos_todo()
{
    printf("Exportando partidos...\n");
    exportar_partidos_csv();
    exportar_partidos_txt();
    exportar_partidos_json();
    exportar_partidos_html();
    printf("Exportacion de partidos completada.\n");
    pause_console();
}

/**
 * @brief Exporta lesiones en todos los formatos.
 */
static void exportar_lesiones_todo()
{
    printf("Exportando lesiones...\n");
    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();
    printf("Exportacion de lesiones completada.\n");
    pause_console();
}

/**
 * @brief Exporta estadísticas en todos los formatos.
 */
static void exportar_estadisticas_todo()
{
    printf("Exportando estadisticas...\n");
    exportar_estadisticas_csv();
    exportar_estadisticas_txt();
    exportar_estadisticas_json();
    exportar_estadisticas_html();
    printf("Exportacion de estadisticas completada.\n");
    pause_console();
}

/**
 * @brief Exporta análisis en todos los formatos.
 */
static void exportar_analisis_todo()
{
    printf("Exportando analisis...\n");
    exportar_analisis_csv();
    exportar_analisis_txt();
    exportar_analisis_json();
    exportar_analisis_html();
    printf("Exportacion de analisis completada.\n");
    pause_console();
}

/**
 * @brief Exporta estadísticas generales en todos los formatos.
 */
static void exportar_estadisticas_generales_todo()
{
    printf("Exportando estadisticas generales...\n");
    exportar_estadisticas_generales_csv();
    exportar_estadisticas_generales_txt();
    exportar_estadisticas_generales_json();
    exportar_estadisticas_generales_html();
    printf("Exportacion de estadisticas generales completada.\n");
    pause_console();
}

/**
 * @brief Exporta estadísticas por mes en todos los formatos.
 */
static void exportar_estadisticas_por_mes_todo()
{
    printf("Exportando estadisticas por mes...\n");
    exportar_estadisticas_por_mes_csv();
    exportar_estadisticas_por_mes_txt();
    exportar_estadisticas_por_mes_json();
    exportar_estadisticas_por_mes_html();
    printf("Exportacion de estadisticas por mes completada.\n");
    pause_console();
}

/**
 * @brief Exporta estadísticas por año en todos los formatos.
 */
static void exportar_estadisticas_por_anio_todo()
{
    printf("Exportando estadisticas por anio...\n");
    exportar_estadisticas_por_anio_csv();
    exportar_estadisticas_por_anio_txt();
    exportar_estadisticas_por_anio_json();
    exportar_estadisticas_por_anio_html();
    printf("Exportacion de estadisticas por anio completada.\n");
    pause_console();
}

/**
 * @brief Exporta récords y rankings en todos los formatos.
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
    pause_console();
}

/**
 * @brief Exporta partido con más goles en todos los formatos.
 */
static void exportar_partido_mas_goles_todo()
{
    printf("Exportando partido con mas goles...\n");
    exportar_partido_mas_goles_csv();
    exportar_partido_mas_goles_txt();
    exportar_partido_mas_goles_json();
    exportar_partido_mas_goles_html();
    printf("Exportacion completada.\n");
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
    pause_console();
}

/**
 * @brief Exporta partido más reciente con menos asistencias en todos los formatos.
 */
static void exportar_partido_menos_asistencias_reciente_todo()
{
    printf("Exportando partido menos asistencias reciente...\n");
    exportar_partido_menos_asistencias_reciente_csv();
    exportar_partido_menos_asistencias_reciente_txt();
    exportar_partido_menos_asistencias_reciente_json();
    exportar_partido_menos_asistencias_reciente_html();
    printf("Exportacion completada.\n");
    pause_console();
}

/**
 * @brief Exporta todos los datos en todos los formatos.
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
