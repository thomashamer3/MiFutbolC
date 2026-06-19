/**
 * @file estadisticas_meta.h
 * @brief Declaraciones de funciones para estadísticas avanzadas y meta-análisis
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * el análisis avanzado de estadísticas de fútbol.
 */

#ifndef ESTADISTICAS_META_H
#define ESTADISTICAS_META_H

/**
 * @brief Muestra la consistencia del rendimiento (variabilidad)
 */
void mostrar_consistencia_rendimiento(void);

/**
 * @brief Muestra los partidos atípicos (muy buenos/muy malos)
 */
void mostrar_partidos_outliers(void);

/**
 * @brief Muestra la dependencia del contexto
 */
void mostrar_dependencia_contexto(void);

/**
 * @brief Muestra el impacto real del cansancio
 */
void mostrar_impacto_real_cansancio(void);

/**
 * @brief Muestra el impacto real del estado de ánimo
 */
void mostrar_impacto_real_estado_animo(void);

/**
 * @brief Muestra la eficiencia: goles por partido vs rendimiento
 */
void mostrar_eficiencia_goles_vs_rendimiento(void);

/**
 * @brief Muestra la eficiencia: asistencias por partido vs cansancio
 */
void mostrar_eficiencia_asistencias_vs_cansancio(void);

/**
 * @brief Muestra el rendimiento obtenido por esfuerzo
 */
void mostrar_rendimiento_por_esfuerzo(void);

/**
 * @brief Muestra el rendimiento agrupado por dolor/molestia física
 */
void mostrar_rendimiento_por_dolor_fisico(void);

/**
 * @brief Muestra el rendimiento agrupado por calidad de arbitraje
 */
void mostrar_rendimiento_por_arbitraje(void);

/**
 * @brief Muestra el rendimiento agrupado por temperatura registrada
 */
void mostrar_rendimiento_por_temperatura(void);

/**
 * @brief Muestra partidos exigentes bien rendidos
 */
void mostrar_partidos_exigentes_bien_rendidos(void);

/**
 * @brief Muestra partidos fáciles mal rendidos
 */
void mostrar_partidos_faciles_mal_rendidos(void);

/**
 * @brief Calculadora de efectividad general: goles/partido, asistencias/partido, efectividad pase, conversion
 */
void mostrar_efectividad_general(void);

/**
 * @brief Muestra tendencia de rendimiento con sparklines en los ultimos 30 partidos
 */
void mostrar_tendencia_rendimiento_sparkline(void);

#endif
