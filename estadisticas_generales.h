/**
 * @file estadisticas_generales.h
 * @brief API de consultas estadísticas globales de rendimiento deportivo
 *
 * Define interfaz para análisis cuantitativo de métricas agregadas por partidos,
 * incluyendo segmentación por clima, día semanal, cansancio y estado anímico,
 * utilizando consultas SQL preparadas con funciones de agrupamiento y JOINs.
 */

#ifndef ESTADISTICAS_GENERALES_H
#define ESTADISTICAS_GENERALES_H

/**
 * @brief Muestra las estadísticas principales de las camisetas.
 *
 * Esta función imprime un encabezado y ejecuta varias consultas para mostrar
 * estadísticas como la camiseta con más goles, asistencias, partidos jugados
 * y la suma de goles más asistencias. Al final, pausa la consola.
 */
void mostrar_estadisticas_generales(void);

/**
 * @brief Muestra el total de partidos jugados
 */
void mostrar_total_partidos_jugados(void);

/**
 * @brief Muestra el promedio de goles por partido
 */
void mostrar_promedio_goles_por_partido(void);

/**
 * @brief Muestra el promedio de asistencias por partido
 */
void mostrar_promedio_asistencias_por_partido(void);

/**
 * @brief Muestra el promedio de rendimiento_general
 */
void mostrar_promedio_rendimiento_general(void);

/**
 * @brief Muestra el rendimiento promedio por clima
 */
void mostrar_rendimiento_promedio_por_clima(void);

/**
 * @brief Muestra los goles por clima
 */
void mostrar_goles_por_clima(void);

/**
 * @brief Muestra las asistencias por clima
 */
void mostrar_asistencias_por_clima(void);

/**
 * @brief Muestra el clima donde se rinde mejor
 */
void mostrar_clima_mejor_rendimiento(void);

/**
 * @brief Muestra el clima donde se rinde peor
 */
void mostrar_clima_peor_rendimiento(void);

/**
 * @brief Muestra el mejor día de la semana
 */
void mostrar_mejor_dia_semana(void);

/**
 * @brief Muestra el peor día de la semana
 */
void mostrar_peor_dia_semana(void);

/**
 * @brief Muestra los goles promedio por día
 */
void mostrar_goles_promedio_por_dia(void);

/**
 * @brief Muestra las asistencias promedio por día
 */
void mostrar_asistencias_promedio_por_dia(void);

/**
 * @brief Muestra el rendimiento promedio por día
 */
void mostrar_rendimiento_promedio_por_dia(void);

/**
 * @brief Muestra el rendimiento según nivel de cansancio
 */
void mostrar_rendimiento_por_nivel_cansancio(void);

/**
 * @brief Muestra los goles con cansancio alto vs bajo
 */
void mostrar_goles_cansancio_alto_vs_bajo(void);

/**
 * @brief Muestra los partidos jugados con cansancio alto
 */
void mostrar_partidos_cansancio_alto(void);

/**
 * @brief Muestra la caída de rendimiento por cansancio acumulado
 */
void mostrar_caida_rendimiento_cansancio_acumulado(void);

/**
 * @brief Muestra el rendimiento según estado de ánimo
 */
void mostrar_rendimiento_por_estado_animo(void);

/**
 * @brief Muestra los goles según estado de ánimo
 */
void mostrar_goles_por_estado_animo(void);

/**
 * @brief Muestra las asistencias según estado de ánimo
 */
void mostrar_asistencias_por_estado_animo(void);

/**
 * @brief Muestra el estado de ánimo ideal para jugar
 */
void mostrar_estado_animo_ideal(void);

/**
 * @brief Obtiene el día de la semana para una fecha dada
 * @param dia Día del mes (1-31)
 * @param mes Mes del año (1-12)
 * @param anio Año (ej. 2023)
 * @return Nombre del día de la semana en español
 */
const char *obtener_dia_semana(int dia, int mes, int anio);

#endif /* ESTADISTICAS_GENERALES_H */
