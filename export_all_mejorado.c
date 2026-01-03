/**
 * @file export_all_mejorado.c
 * @brief Funciones mejoradas para exportar todos los datos con análisis avanzado
 *
 * Este archivo contiene funciones mejoradas para exportar todos los datos
 * con estadísticas avanzadas y análisis integrado.
 */

#include "export_all_mejorado.h"
#include "export_camisetas_mejorado.h"
#include "export_lesiones_mejorado.h"
#include "export_camisetas.h"
#include "export_lesiones.h"
#include "export.h"
#include "utils.h"
#include "menu.h"
#include <stdio.h>

/**
 * @brief Exporta camisetas con análisis avanzado en todos los formatos mejorados
 *
 * Esta función exporta los datos de camisetas en todos los formatos disponibles
 * (CSV, TXT, JSON, HTML) con estadísticas avanzadas y análisis de rendimiento.
 * Incluye métricas como eficiencia de goles, asistencias, porcentaje de victorias
 * y análisis de lesiones por partido.
 *
 * @see exportar_camisetas_csv_mejorado()
 * @see exportar_camisetas_txt_mejorado()
 * @see exportar_camisetas_json_mejorado()
 * @see exportar_camisetas_html_mejorado()
 */
void exportar_camisetas_todo_mejorado()
{
    printf("Exportando camisetas con analisis avanzado...\n");
    exportar_camisetas_csv_mejorado();
    exportar_camisetas_txt_mejorado();
    exportar_camisetas_json_mejorado();
    exportar_camisetas_html_mejorado();
    printf("Exportacion de camisetas con analisis avanzado completada.\n");
    pause_console();
}

/**
 * @brief Exporta lesiones con análisis avanzado en todos los formatos mejorados
 *
 * Esta función exporta los datos de lesiones en todos los formatos disponibles
 * (CSV, TXT, JSON, HTML) con estadísticas avanzadas y análisis de impacto.
 * Incluye métricas como impacto en rendimiento, partidos antes y después de la lesión,
 * y comparación de rendimiento.
 *
 * @see exportar_lesiones_csv_mejorado()
 * @see exportar_lesiones_txt_mejorado()
 * @see exportar_lesiones_json_mejorado()
 * @see exportar_lesiones_html_mejorado()
 */
void exportar_lesiones_todo_mejorado()
{
    printf("Exportando lesiones con analisis avanzado...\n");
    exportar_lesiones_csv_mejorado();
    exportar_lesiones_txt_mejorado();
    exportar_lesiones_json_mejorado();
    exportar_lesiones_html_mejorado();
    printf("Exportacion de lesiones con analisis avanzado completada.\n");
    pause_console();
}

/**
 * @brief Exporta todos los datos con análisis avanzado en todos los formatos mejorados
 *
 * Esta función exporta todos los datos disponibles (camisetas y lesiones)
 * en todos los formatos soportados (CSV, TXT, JSON, HTML), incluyendo tanto
 * las versiones mejoradas con análisis avanzado como las versiones originales
 * para mantener compatibilidad con versiones anteriores.
 *
 * @see exportar_camisetas_todo_mejorado()
 * @see exportar_lesiones_todo_mejorado()
 */
void exportar_todo_mejorado()
{
    printf("Exportando todo con analisis avanzado...\n");

    // Exportar datos mejorados
    exportar_camisetas_csv_mejorado();
    exportar_camisetas_txt_mejorado();
    exportar_camisetas_json_mejorado();
    exportar_camisetas_html_mejorado();

    exportar_lesiones_csv_mejorado();
    exportar_lesiones_txt_mejorado();
    exportar_lesiones_json_mejorado();
    exportar_lesiones_html_mejorado();

    // Exportar datos originales para compatibilidad
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();

    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();

    printf("Exportacion de todo con analisis avanzado completada.\n");
    pause_console();
}

/**
 * @brief Menú para exportar datos mejorados del sistema
 *
 * Esta función muestra un menú interactivo que permite al usuario
 * seleccionar entre diferentes opciones de exportación mejorada:
 * - Exportar camisetas con análisis avanzado
 * - Exportar lesiones con análisis de impacto
 * - Exportar todos los datos con análisis avanzado
 * - Volver al menú principal
 *
 * @see ejecutar_menu()
 */
void menu_exportar_mejorado()
{
    MenuItem items[] =
    {
        {1, "Camisetas con Analisis Avanzado", exportar_camisetas_todo_mejorado},
        {2, "Lesiones con Analisis de Impacto", exportar_lesiones_todo_mejorado},
        {3, "Todo con Analisis Avanzado", exportar_todo_mejorado},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR DATOS MEJORADOS", items, 4);
}
