/**
 * @file estadisticas_canchas.h
 * @brief API de analisis estadistico de canchas y su uso en partidos
 *
 * Define interfaz para consultas de agregacion sobre la tabla cancha
 * combinada con partido, incluyendo metricas de uso, rendimiento promedio,
 * distribucion por tipo, superficie y franja horaria.
 */

#ifndef ESTADISTICAS_CANCHAS_H
#define ESTADISTICAS_CANCHAS_H

/**
 * @brief Submenu de estadisticas de canchas.
 */
void menu_estadisticas_canchas(void);

/**
 * @brief Resumen general de canchas (totales, activas, uso).
 */
void mostrar_resumen_canchas(void);

/**
 * @brief Ranking de las canchas mas usadas (por partidos jugados).
 */
void mostrar_ranking_canchas_mas_usadas(void);

/**
 * @brief Canchas con menos partidos jugados (incluye canchas sin uso).
 */
void mostrar_canchas_menos_usadas(void);

/**
 * @brief Rendimiento general promedio agrupado por cancha.
 */
void mostrar_rendimiento_por_cancha(void);

/**
 * @brief Total de goles y asistencias agrupado por cancha.
 */
void mostrar_goles_asistencias_por_cancha(void);

/**
 * @brief Rendimiento y uso promedio agrupado por tipo de cancha.
 */
void mostrar_rendimiento_por_tipo_cancha(void);

/**
 * @brief Rendimiento y uso promedio agrupado por superficie.
 */
void mostrar_rendimiento_por_superficie(void);

/**
 * @brief Comparativa entre canchas techadas y descubiertas.
 */
void mostrar_canchas_techadas_vs_descubiertas(void);

/**
 * @brief Partidos jugados agrupados por localidad de la cancha.
 */
void mostrar_partidos_por_localidad(void);

/**
 * @brief Distribucion de partidos por franja horaria.
 */
void mostrar_partidos_por_franja_horaria(void);

#endif /* ESTADISTICAS_CANCHAS_H */
