/**
 * @file estadisticas.h
 * @brief Declaraciones de funciones para mostrar estadísticas en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * la visualización de estadísticas generales del sistema de gestión de fútbol.
 */

/**
 * @brief Muestra el menú de estadísticas con opciones para ver estadísticas generales, por mes o por año
 */
void menu_estadisticas();

/**
 * @brief Muestra estadísticas generales del sistema
 *
 * Calcula y muestra estadísticas agregadas como total de partidos jugados,
 * total de camisetas registradas, total de lesiones, etc. Utiliza consultas
 * SQL para obtener los conteos de cada tabla principal.
 */
void mostrar_estadisticas_generales();

/**
 * @brief Muestra estadísticas históricas agrupadas por mes
 *
 * Muestra estadísticas individuales por camiseta agrupadas por mes,
 * incluyendo partidos jugados, goles, asistencias y promedios por partido.
 */
void mostrar_estadisticas_por_mes();

/**
 * @brief Muestra estadísticas históricas agrupadas por año
 *
 * Muestra estadísticas individuales por camiseta agrupadas por año,
 * incluyendo partidos jugados, goles, asistencias y promedios por partido.
 */
void mostrar_estadisticas_por_anio();
