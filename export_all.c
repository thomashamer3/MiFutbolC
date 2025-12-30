/**
 * @file export_all.c
 * @brief Módulo para exportar todos los datos del sistema en múltiples formatos.
 *
 * Este archivo contiene la función principal para exportar camisetas, partidos,
 * lesiones y estadísticas en formatos CSV, TXT, JSON y HTML.
 */

#include "export_all.h"
#include "export.h"
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
 * @brief Exporta todos los datos en todos los formatos.
 */
static void exportar_todo()
{
    printf("Exportando todo...\n");
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();

    exportar_partidos_csv();
    exportar_partidos_txt();
    exportar_partidos_json();
    exportar_partidos_html();

    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();

    exportar_estadisticas_csv();
    exportar_estadisticas_txt();
    exportar_estadisticas_json();
    exportar_estadisticas_html();

    exportar_analisis_csv();
    exportar_analisis_txt();
    exportar_analisis_json();
    exportar_analisis_html();
    printf("Exportacion de todo completada.\n");
    pause_console();
}

/**
 * @brief Menu para exportar datos del sistema en múltiples formatos según selección del usuario.
 *
 * Esta función muestra un menú para que el usuario seleccione qué datos exportar:
 * camisetas, partidos, lesiones, estadísticas, análisis, todo o volver.
 * Llama a las funciones de exportación correspondientes en formatos CSV, TXT, JSON y HTML.
 * Al final, muestra un mensaje de confirmación y pausa la consola.
 */
void menu_exportar()
{
    MenuItem items[] =
    {
        {1, "Camisetas", exportar_camisetas_todo},
        {2, "Partidos", exportar_partidos_todo},
        {3, "Lesiones", exportar_lesiones_todo},
        {4, "Estadisticas", exportar_estadisticas_todo},
        {5, "Analisis", exportar_analisis_todo},
        {6, "Todo", exportar_todo},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR DATOS", items, 7);
}
