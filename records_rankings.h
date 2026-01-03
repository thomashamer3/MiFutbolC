/**
 * @file records_rankings.h
 * @brief Declaraciones de funciones para mostrar récords y rankings en MiFutbolC
 */

#ifndef RECORDS_RANKINGS_H
#define RECORDS_RANKINGS_H

/**
 * @brief Muestra el menú de récords y rankings
 */
void menu_records_rankings();

/**
 * @brief Muestra el récord de goles en un partido
 */
void mostrar_record_goles_partido();

/**
 * @brief Muestra el récord de asistencias en un partido
 */
void mostrar_record_asistencias_partido();

/**
 * @brief Muestra la mejor combinación cancha + camiseta
 */
void mostrar_mejor_combinacion_cancha_camiseta();

/**
 * @brief Muestra la peor combinación cancha + camiseta
 */
void mostrar_peor_combinacion_cancha_camiseta();

/**
 * @brief Muestra la mejor temporada
 */
void mostrar_mejor_temporada();

/**
 * @brief Muestra la peor temporada
 */
void mostrar_peor_temporada();

/**
 * @brief Muestra el partido con mejor rendimiento_general
 */
void mostrar_partido_mejor_rendimiento_general();

/**
 * @brief Muestra el partido con peor rendimiento_general
 */
void mostrar_partido_peor_rendimiento_general();

/**
 * @brief Muestra el partido con mejor combinación (goles + asistencias)
 */
void mostrar_partido_mejor_combinacion_goles_asistencias();

/**
 * @brief Muestra los partidos sin goles
 */
void mostrar_partidos_sin_goles();

/**
 * @brief Muestra los partidos sin asistencias
 */
void mostrar_partidos_sin_asistencias();

/**
 * @brief Muestra la mejor racha goleadora
 */
void mostrar_mejor_racha_goleadora();

/**
 * @brief Muestra la peor racha
 */
void mostrar_peor_racha();

/**
 * @brief Muestra los partidos consecutivos anotando
 */
void mostrar_partidos_consecutivos_anotando();

/**
 * @brief Muestra la cancha donde hice más goles
 */
void mostrar_cancha_mas_goles();

/**
 * @brief Muestra la cancha donde hice menos goles
 */
void mostrar_cancha_menos_goles();

/**
 * @brief Muestra la cancha con más partidos jugados
 */
void mostrar_cancha_mas_partidos();

/**
 * @brief Muestra la cancha con mejor rendimiento promedio
 */
void mostrar_cancha_mejor_rendimiento();

/**
 * @brief Muestra la cancha con peor rendimiento promedio
 */
void mostrar_cancha_peor_rendimiento();

/**
 * @brief Muestra el ranking de canchas por rendimiento
 */
void mostrar_ranking_canchas_rendimiento();

/**
 * @brief Muestra la cancha donde el cansancio afecta menos
 */
void mostrar_cancha_menos_cansancio();

/**
 * @brief Muestra la cancha donde el cansancio afecta más
 */
void mostrar_cancha_mas_cansancio();

/**
 * @brief Muestra la cancha + clima ideal
 */
void mostrar_cancha_clima_ideal();

/**
 * @brief Muestra la cancha "maldita" (peor combinación general)
 */
void mostrar_cancha_maldita();

#endif /* RECORDS_RANKINGS_H */
