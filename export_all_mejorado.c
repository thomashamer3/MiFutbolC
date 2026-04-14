
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

void exportar_camisetas_todo_mejorado()
{
    printf("Exportando camisetas con analisis avanzado...\n");
    exportar_camisetas_mejoradas();
    printf("Exportacion de camisetas con analisis avanzado completada.\n");
    pause_console();
}

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
            {0, "Volver", NULL}};
    ejecutar_menu("EXPORTAR DATOS MEJORADOS", items, 4);
}
