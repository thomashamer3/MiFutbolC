/**
 * @file formaciones.h
 * @brief Analisis de efectividad de formaciones de futbol (F5/F7/F8/F11)
 *
 * Proporciona funciones para analizar el rendimiento de cada modalidad
 * de futbol, correlacionando tipos de formacion con resultados de partidos.
 */

#ifndef FORMACIONES_H
#define FORMACIONES_H

/**
 * @brief Menu principal de analisis de formaciones
 *
 * Presenta opciones para ver efectividad por formacion, mejor formacion
 * por cancha, tendencias temporales y exportacion CSV.
 */
void menu_analisis_formaciones();

/**
 * @brief Muestra la efectividad (victorias/empates/derrotas) agrupada por
 *        tipo de formacion
 *
 * Consulta la tabla partido agrupando por formato_partido y calcula
 * total de partidos, victorias, empates, derrotas, porcentaje de
 * victorias y promedio de goles.
 */
void mostrar_efectividad_por_formacion();

/**
 * @brief Muestra la mejor formacion para una cancha especifica
 *
 * Filtra los partidos por cancha_id y agrupa por formato_partido
 * para determinar que formacion ha tenido mejor rendimiento en
 * esa cancha.
 *
 * @param cancha_id ID de la cancha a analizar
 */
void mostrar_mejor_formacion_por_cancha(int cancha_id);

/**
 * @brief Muestra la tendencia de rendimiento de las formaciones a lo
 *        largo del tiempo
 *
 * Agrupa partidos por mes y tipo de formacion, mostrando la evolucion
 * del rendimiento general y promedio de goles para cada modalidad.
 */
void mostrar_tendencia_formaciones();

/**
 * @brief Exporta el analisis de formaciones a un archivo CSV
 *
 * Genera un archivo CSV con los datos de efectividad de cada formacion
 * incluyendo total de partidos, resultados y promedios.
 */
void exportar_analisis_formaciones_csv();

#endif
