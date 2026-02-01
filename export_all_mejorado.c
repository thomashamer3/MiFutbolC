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

static void exportar_camisetas_mejoradas()
{
    exportar_camisetas_csv_mejorado();
    exportar_camisetas_txt_mejorado();
    exportar_camisetas_json_mejorado();
    exportar_camisetas_html_mejorado();
}

static void exportar_lesiones_mejoradas()
{
    exportar_lesiones_csv_mejorado();
    exportar_lesiones_txt_mejorado();
    exportar_lesiones_json_mejorado();
    exportar_lesiones_html_mejorado();
}

static void exportar_camisetas_basicas()
{
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();
}

static void exportar_lesiones_basicas()
{
    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();
}

/**
 * @brief Exportación completa de datos de camisetas con análisis avanzado
 *
 * Centraliza la exportación de datos de camisetas en todos los formatos mejorados,
 * proporcionando una solución integral para el análisis de rendimiento. Esto es esencial
 * para equipos y analistas que necesitan evaluar múltiples aspectos del desempeño de los jugadores
 * en diferentes formatos para diferentes usos (hojas de cálculo, informes, APIs, visualización web).
 *
 * @details La exportación en múltiples formatos permite:
 * - CSV: Análisis cuantitativo en herramientas como Excel
 * - TXT: Documentación legible para informes
 * - JSON: Integración con aplicaciones y APIs
 * - HTML: Visualización interactiva en navegadores
 *
 * @see exportar_camisetas_csv_mejorado()
 * @see exportar_camisetas_txt_mejorado()
 * @see exportar_camisetas_json_mejorado()
 * @see exportar_camisetas_html_mejorado()
 */
void exportar_camisetas_todo_mejorado()
{
    printf("Exportando camisetas con analisis avanzado...\n");
    exportar_camisetas_mejoradas();
    printf("Exportacion de camisetas con analisis avanzado completada.\n");
    pause_console();
}

/**
 * @brief Exportación completa de datos de lesiones con análisis de impacto
 *
 * Proporciona una exportación integral de datos de lesiones en todos los formatos mejorados,
 * incluyendo análisis de impacto en el rendimiento. Esto es crucial para equipos médicos y
 * entrenadores que necesitan evaluar cómo las lesiones afectan el desempeño de los jugadores
 * y planificar estrategias de recuperación y prevención.
 *
 * @details El análisis de impacto de lesiones ayuda a:
 * - Evaluar la gravedad y consecuencias de las lesiones
 * - Planificar programas de rehabilitación efectivos
 * - Prevenir lesiones futuras mediante la identificación de patrones
 * - Optimizar la gestión del equipo considerando la disponibilidad de jugadores
 *
 * @see exportar_lesiones_csv_mejorado()
 * @see exportar_lesiones_txt_mejorado()
 * @see exportar_lesiones_json_mejorado()
 * @see exportar_lesiones_html_mejorado()
 */
void exportar_lesiones_todo_mejorado()
{
    printf("Exportando lesiones con analisis avanzado...\n");
    exportar_lesiones_mejoradas();
    printf("Exportacion de lesiones con analisis avanzado completada.\n");
    pause_console();
}

void exportar_todo_mejorado()
{
    printf("Exportando todo con analisis avanzado...\n");

    // Exportar datos mejorados
    exportar_camisetas_mejoradas();
    exportar_lesiones_mejoradas();

    // Exportar datos originales para compatibilidad
    exportar_camisetas_basicas();
    exportar_lesiones_basicas();

    printf("Exportacion de todo con analisis avanzado completada.\n");
    pause_console();
}

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
