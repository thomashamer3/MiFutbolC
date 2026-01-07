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
    exportar_camisetas_csv_mejorado();
    exportar_camisetas_txt_mejorado();
    exportar_camisetas_json_mejorado();
    exportar_camisetas_html_mejorado();
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
    exportar_lesiones_csv_mejorado();
    exportar_lesiones_txt_mejorado();
    exportar_lesiones_json_mejorado();
    exportar_lesiones_html_mejorado();
    printf("Exportacion de lesiones con analisis avanzado completada.\n");
    pause_console();
}

/**
 * @brief Exportación completa del sistema con análisis avanzado y compatibilidad
 *
 * Función maestra que realiza una exportación exhaustiva de todos los datos del sistema,
 * incluyendo tanto las versiones mejoradas con análisis avanzado como las versiones originales
 * para mantener compatibilidad con versiones anteriores. Esto es esencial para migraciones
 * y para usuarios que necesitan ambos tipos de datos.
 *
 * @details La exportación completa incluye:
 * - Datos mejorados: Análisis avanzado para toma de decisiones
 * - Datos originales: Compatibilidad con sistemas existentes
 * - Múltiples formatos: Flexibilidad para diferentes usos
 * - Todas las categorías: Camisetas y lesiones completas
 *
 * @note Esta función es la más completa pero también la más lenta, ya que
 *       exporta todos los datos en todos los formatos disponibles.
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
 * @brief Interfaz de usuario para exportación de datos mejorados
 *
 * Proporciona un menú interactivo que centraliza el acceso a todas las opciones
 * de exportación mejorada. Esto permite a los usuarios seleccionar fácilmente
 * qué tipo de datos y análisis necesitan exportar, mejorando la experiencia de usuario
 * y facilitando el acceso a las funciones avanzadas de exportación.
 *
 * @details El menú ofrece opciones para:
 * - Exportar datos específicos (camisetas o lesiones) con análisis avanzado
 * - Exportar todos los datos de manera integral
 * - Mantener una interfaz consistente con el resto del sistema
 *
 * @see ejecutar_menu()
 * @see exportar_camisetas_todo_mejorado()
 * @see exportar_lesiones_todo_mejorado()
 * @see exportar_todo_mejorado()
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
