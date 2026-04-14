
#include "export_all.h"
#include "export_all_mejorado.h"
#include "export.h"
#include "export_camisetas.h"
#include "export_partidos.h"
#include "export_lesiones.h"
#include "export_estadisticas.h"
#include "export_estadisticas_generales.h"
#include "export_records_rankings.h"
#include "export_pdf.h"
#include "utils.h"
#include "menu.h"
#include "ascii_art.h"
#include "db.h"
#include "settings.h"
#include <stdio.h>

static void exportar_camisetas_all()
{
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();
}

static void exportar_partidos_all()
{
    exportar_partidos_csv();
    exportar_partidos_txt();
    exportar_partidos_json();
    exportar_partidos_html();
}

static void exportar_lesiones_all()
{
    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();
}

static void exportar_estadisticas_all()
{
    exportar_estadisticas_csv();
    exportar_estadisticas_txt();
    exportar_estadisticas_json();
    exportar_estadisticas_html();
    exportar_estadisticas_md();
}

static void exportar_analisis_all()
{
    exportar_analisis_csv();
    exportar_analisis_txt();
    exportar_analisis_json();
    exportar_analisis_html();
}

static void exportar_estadisticas_generales_all()
{
    exportar_estadisticas_generales_csv();
    exportar_estadisticas_generales_txt();
    exportar_estadisticas_generales_json();
    exportar_estadisticas_generales_html();
    exportar_estadisticas_generales_md();
}

static void exportar_estadisticas_por_mes_all()
{
    exportar_estadisticas_por_mes_csv();
    exportar_estadisticas_por_mes_txt();
    exportar_estadisticas_por_mes_json();
    exportar_estadisticas_por_mes_html();
}

static void exportar_estadisticas_por_anio_all()
{
    exportar_estadisticas_por_anio_csv();
    exportar_estadisticas_por_anio_txt();
    exportar_estadisticas_por_anio_json();
    exportar_estadisticas_por_anio_html();
}

static void exportar_records_rankings_all()
{
    exportar_record_goles_partido_csv();
    exportar_record_asistencias_partido_csv();
    exportar_mejor_combinacion_cancha_camiseta_csv();
    exportar_peor_combinacion_cancha_camiseta_csv();
    exportar_mejor_temporada_csv();
    exportar_peor_temporada_csv();
    exportar_records_rankings_txt();
    exportar_records_rankings_json();
    exportar_records_rankings_html();
}

static void exportar_todo_json()
{
    printf("Exportando todo (JSON)...\n");

    exportar_camisetas_json();
    exportar_partidos_json();
    exportar_lesiones_json();
    exportar_estadisticas_json();
    exportar_analisis_json();
    exportar_estadisticas_generales_json();
    exportar_estadisticas_por_mes_json();
    exportar_estadisticas_por_anio_json();
    exportar_records_rankings_json();

    exportar_partido_mas_goles_json();
    exportar_partido_mas_asistencias_json();
    exportar_partido_menos_goles_reciente_json();
    exportar_partido_menos_asistencias_reciente_json();

    printf("Exportacion JSON completada.\n");
    pause_console();
}

static void exportar_todo_csv()
{
    printf("Exportando todo (CSV)...\n");

    exportar_camisetas_csv();
    exportar_partidos_csv();
    exportar_lesiones_csv();
    exportar_estadisticas_csv();
    exportar_analisis_csv();
    exportar_estadisticas_generales_csv();
    exportar_estadisticas_por_mes_csv();
    exportar_estadisticas_por_anio_csv();
    exportar_record_goles_partido_csv();
    exportar_record_asistencias_partido_csv();
    exportar_mejor_combinacion_cancha_camiseta_csv();
    exportar_peor_combinacion_cancha_camiseta_csv();
    exportar_mejor_temporada_csv();
    exportar_peor_temporada_csv();

    exportar_partido_mas_goles_csv();
    exportar_partido_mas_asistencias_csv();
    exportar_partido_menos_goles_reciente_csv();
    exportar_partido_menos_asistencias_reciente_csv();

    printf("Exportacion CSV completada.\n");
    pause_console();
}

static void exportar_camisetas_todo()
{
    printf("Exportando camisetas...\n");
    exportar_camisetas_all();
    printf("Exportacion de camisetas completada.\n");
    pause_console();
}

static void exportar_partidos_todo()
{
    printf("Exportando partidos...\n");
    exportar_partidos_all();
    printf("Exportacion de partidos completada.\n");
    pause_console();
}

static void exportar_lesiones_todo()
{
    printf("Exportando lesiones...\n");
    exportar_lesiones_all();
    printf("Exportacion de lesiones completada.\n");
    pause_console();
}

static void exportar_estadisticas_todo()
{
    printf("Exportando estadisticas...\n");
    exportar_estadisticas_all();
    printf("Exportacion de estadisticas completada.\n");
    pause_console();
}

static void exportar_analisis_todo()
{
    printf("Exportando analisis...\n");
    exportar_analisis_all();
    printf("Exportacion de analisis completada.\n");
    pause_console();
}

static void exportar_estadisticas_generales_todo()
{
    printf("Exportando estadisticas generales...\n");
    exportar_estadisticas_generales_all();
    printf("Exportacion de estadisticas generales completada.\n");
    pause_console();
}

static void exportar_estadisticas_por_mes_todo()
{
    printf("Exportando estadisticas por mes...\n");
    exportar_estadisticas_por_mes_all();
    printf("Exportacion de estadisticas por mes completada.\n");
    pause_console();
}

static void exportar_estadisticas_por_anio_todo()
{
    printf("Exportando estadisticas por anio...\n");
    exportar_estadisticas_por_anio_all();
    printf("Exportacion de estadisticas por anio completada.\n");
    pause_console();
}

static void exportar_records_rankings_todo()
{
    printf("Exportando records & rankings...\n");
    exportar_records_rankings_all();
    printf("Exportacion de records & rankings completada.\n");
    pause_console();
}

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

static void exportar_todo()
{
    printf("Exportando todo...\n");

    // Exportar camisetas en todos los formatos
    exportar_camisetas_all();

    // Exportar partidos en todos los formatos
    exportar_partidos_all();

    // Exportar partidos especificos
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
    exportar_lesiones_all();

    // Exportar estadisticas basicas
    exportar_estadisticas_all();

    // Exportar analisis
    exportar_analisis_all();

    // Exportar estadisticas generales
    exportar_estadisticas_generales_all();

    // Exportar estadisticas por mes
    exportar_estadisticas_por_mes_all();

    // Exportar estadisticas por ano
    exportar_estadisticas_por_anio_all();

    // Exportar records y rankings
    exportar_records_rankings_all();

    printf("Exportacion de todo completada.\n");
    pause_console();
}

static void exportar_informe_total_pdf()
{
    printf("Generando informe total en PDF...\n");
    generar_informe_total_pdf();
    pause_console();
}

static void menu_exportar_partidos()
{
    MenuItem items[] =
    {
        {1, get_text("export_todos_partidos"), exportar_partidos_todo},
        {2, get_text("export_partido_mas_goles"), exportar_partido_mas_goles_todo},
        {3, get_text("export_partido_mas_asistencias"), exportar_partido_mas_asistencias_todo},
        {4, get_text("export_partido_menos_goles_reciente"), exportar_partido_menos_goles_reciente_todo},
        {5, get_text("export_partido_menos_asistencias_reciente"), exportar_partido_menos_asistencias_reciente_todo},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu(get_text("export_partidos_menu_title"), items, 6);
}

static void menu_exportar_estadisticas_generales()
{
    MenuItem items[] =
    {
        {1, get_text("export_estadisticas_generales_item"), exportar_estadisticas_generales_todo},
        {2, get_text("export_estadisticas_por_mes"), exportar_estadisticas_por_mes_todo},
        {3, get_text("export_estadisticas_por_anio"), exportar_estadisticas_por_anio_todo},
        {4, get_text("export_records_rankings"), exportar_records_rankings_todo},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu(get_text("export_estadisticas_generales_menu_title"), items, 5);
}

void menu_exportar()
{
    MenuItem items[] =
    {
        {1, get_text("export_camisetas"), exportar_camisetas_todo},
        {2, get_text("export_partidos"), menu_exportar_partidos},
        {3, get_text("export_lesiones"), exportar_lesiones_todo},
        {4, get_text("export_estadisticas"), exportar_estadisticas_todo},
        {5, get_text("export_analisis"), exportar_analisis_todo},
        {6, get_text("export_estadisticas_generales"), menu_exportar_estadisticas_generales},
        {7, get_text("export_analisis_avanzado"), menu_exportar_mejorado},
        {8, get_text("export_base_datos"), exportar_base_datos},
        {9, get_text("export_todo"), exportar_todo},
        {10, get_text("export_todo_json"), exportar_todo_json},
        {11, get_text("export_todo_csv"), exportar_todo_csv},
        {12, get_text("export_informe_total_pdf"), exportar_informe_total_pdf},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu(get_text("export_menu_title"), items, 13);
}
