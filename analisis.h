/**
 * @file analisis.h
 * @brief Declaraciones de funciones para el análisis de rendimiento en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * el análisis de partidos, incluyendo comparación de últimos partidos con promedios
 * generales y cálculo de rachas.
 */

/**
 * @brief Muestra el análisis de rendimiento
 *
 * Calcula y muestra estadísticas comparativas entre los últimos 5 partidos
 * y los promedios generales, incluyendo rachas de victorias y derrotas.
 * Proporciona mensajes motivacionales y realistas basados en el rendimiento.
 */
void mostrar_analisis();

/**
 * @brief Muestra la evolución temporal del rendimiento del jugador
 *
 * Incluye análisis mensual de goles, asistencias y rendimiento, comparación
 * de mejores/peores meses históricos, inicio vs fin de año, meses fríos vs cálidos,
 * y progreso total del jugador.
 */
void mostrar_evolucion_temporal();

/**
 * @brief Muestra la evolución mensual de goles
 */
void evolucion_mensual_goles();

/**
 * @brief Muestra la evolución mensual de asistencias
 */
void evolucion_mensual_asistencias();

/**
 * @brief Muestra la evolución mensual de rendimiento
 */
void evolucion_mensual_rendimiento();

/**
 * @brief Muestra el mejor mes histórico
 */
void mejor_mes_historico();

/**
 * @brief Muestra el peor mes histórico
 */
void peor_mes_historico();

/**
 * @brief Compara el rendimiento al inicio vs fin de año
 */
void inicio_vs_fin_anio();

/**
 * @brief Compara el rendimiento en meses fríos vs cálidos
 *
 * Meses fríos: junio, julio, agosto, septiembre
 * Meses cálidos: diciembre, enero, febrero, marzo, abril
 */
void meses_frios_vs_calidos();

/**
 * @brief Muestra el progreso total del jugador
 */
void progreso_total_jugador();
