/**
 * @file estadisticas.c
 * @brief Módulo principal para el menú de estadísticas en partidos de fútbol.
 *
 * Este archivo contiene la función principal del menú de estadísticas
 * que permite acceder a las diferentes opciones de visualización de estadísticas.
 */

#include "estadisticas.h"
#include "estadisticas_generales.h"
#include "estadisticas_mes.h"
#include "estadisticas_anio.h"
#include "records_rankings.h"
#include "estadisticas_meta.h"
#include "menu.h"
#include <stdlib.h>

/**
 * @brief Muestra el menú de estadísticas con opciones para ver estadísticas generales, por mes o por año
 */
void menu_estadisticas()
{
    MenuItem items[] =
    {
        {1, "Generales", menu_estadisticas_generales},
        {2, "Partidos", menu_estadisticas_partidos},
        {3, "Goles", menu_estadisticas_goles},
        {4, "Asistencias", menu_estadisticas_asistencias},
        {5, "Rendimiento", menu_estadisticas_rendimiento},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS", items, 6);
}

/**
 * @brief Muestra el sub-menú de estadísticas generales
 */
void menu_estadisticas_generales()
{
    MenuItem items[] =
    {
        {1, "Generales", mostrar_estadisticas_generales},
        {2, "Por Mes", mostrar_estadisticas_por_mes},
        {3, "Por Anio", mostrar_estadisticas_por_anio},
        {4, "Records & Rankings", menu_records_rankings},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS GENERALES", items, 5);
}

/**
 * @brief Muestra el sub-menú de estadísticas de partidos
 */
void menu_estadisticas_partidos()
{
    MenuItem items[] =
    {
        {1, "Total de Partidos Jugados", mostrar_total_partidos_jugados},
        {2, "Partidos con Cansancio Alto", mostrar_partidos_cansancio_alto},
        {3, "Partidos Atipicos", mostrar_partidos_outliers},
        {4, "Partidos Exigentes Bien Rendidos", mostrar_partidos_exigentes_bien_rendidos},
        {5, "Partidos Faciles Mal Rendidos", mostrar_partidos_faciles_mal_rendidos},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS DE PARTIDOS", items, 6);
}

/**
 * @brief Muestra el sub-menú de estadísticas de goles
 */
void menu_estadisticas_goles()
{
    MenuItem items[] =
    {
        {1, "Promedio de Goles por Partido", mostrar_promedio_goles_por_partido},
        {2, "Goles por Clima", mostrar_goles_por_clima},
        {3, "Goles promedio por dia", mostrar_goles_promedio_por_dia},
        {4, "Goles con Cansancio Alto vs Bajo", mostrar_goles_cansancio_alto_vs_bajo},
        {5, "Goles por Estado de Animo", mostrar_goles_por_estado_animo},
        {6, "Eficiencia: Goles vs Rendimiento", mostrar_eficiencia_goles_vs_rendimiento},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS DE GOLES", items, 7);
}

/**
 * @brief Muestra el sub-menú de estadísticas de asistencias
 */
void menu_estadisticas_asistencias()
{
    MenuItem items[] =
    {
        {1, "Promedio de Asistencias por Partido", mostrar_promedio_asistencias_por_partido},
        {2, "Asistencias por Clima", mostrar_asistencias_por_clima},
        {3, "Asistencias promedio por dia", mostrar_asistencias_promedio_por_dia},
        {4, "Asistencias por Estado de Animo", mostrar_asistencias_por_estado_animo},
        {5, "Eficiencia: Asistencias vs Cansancio", mostrar_eficiencia_asistencias_vs_cansancio},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS DE ASISTENCIAS", items, 6);
}

/**
 * @brief Muestra el sub-menú de estadísticas de rendimiento
 */
void menu_estadisticas_rendimiento()
{
    MenuItem items[] =
    {
        {1, "Promedio de Rendimiento General", mostrar_promedio_rendimiento_general},
        {2, "Rendimiento Promedio por Clima", mostrar_rendimiento_promedio_por_clima},
        {3, "Clima donde se rinde mejor", mostrar_clima_mejor_rendimiento},
        {4, "Clima donde se rinde peor", mostrar_clima_peor_rendimiento},
        {5, "Mejor dia de la semana", mostrar_mejor_dia_semana},
        {6, "Peor dia de la semana", mostrar_peor_dia_semana},
        {7, "Rendimiento promedio por dia", mostrar_rendimiento_promedio_por_dia},
        {8, "Rendimiento por Nivel de Cansancio", mostrar_rendimiento_por_nivel_cansancio},
        {9, "Caida de Rendimiento por Cansancio Acumulado", mostrar_caida_rendimiento_cansancio_acumulado},
        {10, "Rendimiento por Estado de Animo", mostrar_rendimiento_por_estado_animo},
        {11, "Estado de Animo Ideal para Jugar", mostrar_estado_animo_ideal},
        {12, "Consistencia del Rendimiento", mostrar_consistencia_rendimiento},
        {13, "Dependencia del Contexto", mostrar_dependencia_contexto},
        {14, "Impacto Real del Cansancio", mostrar_impacto_real_cansancio},
        {15, "Impacto Real del Estado de Animo", mostrar_impacto_real_estado_animo},
        {16, "Rendimiento por Esfuerzo", mostrar_rendimiento_por_esfuerzo},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS DE RENDIMIENTO", items, 17);
}
