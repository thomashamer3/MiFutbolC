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
#include <stdio.h>

/**
 * @brief Exporta todos los datos del sistema en múltiples formatos.
 *
 * Esta función imprime un encabezado y llama a todas las funciones de exportación
 * para camisetas, partidos, lesiones y estadísticas en formatos CSV, TXT, JSON y HTML.
 * Al final, muestra un mensaje de confirmación y pausa la consola.
 */
void exportar_todo()
{
    print_header("EXPORTANDO TODO");

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

    exportar_estadisticas_txt();
    exportar_estadisticas_json();
    exportar_estadisticas_html();

    printf("\nExportaciones Completadas Correctamente.\n");
    pause_console();
}
