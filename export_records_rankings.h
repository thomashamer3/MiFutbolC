/**
 * @file export_records_rankings.h
 * @brief Declaraciones de funciones para exportar récords y rankings en MiFutbolC
 */

#ifndef EXPORT_RECORDS_RANKINGS_H
#define EXPORT_RECORDS_RANKINGS_H

/**
 * @brief Exporta el récord de goles en un partido a CSV
 */
void exportar_record_goles_partido_csv(void);
/**
 * @brief Exporta el récord de asistencias en un partido a CSV
 */
void exportar_record_asistencias_partido_csv(void);
/**
 * @brief Exporta la mejor combinación cancha + camiseta a CSV
 */
void exportar_mejor_combinacion_cancha_camiseta_csv(void);
/**
 * @brief Exporta la peor combinación cancha + camiseta a CSV
 */
void exportar_peor_combinacion_cancha_camiseta_csv(void);
/**
 * @brief Exporta la mejor temporada a CSV
 */
void exportar_mejor_temporada_csv(void);
/**
 * @brief Exporta la peor temporada a CSV
 */
void exportar_peor_temporada_csv(void);
/**
 * @brief Exporta récords y rankings a TXT
 */
void exportar_records_rankings_txt(void);
/**
 * @brief Exporta récords y rankings a JSON
 */
void exportar_records_rankings_json(void);
/**
 * @brief Exporta récords y rankings a HTML
 */
void exportar_records_rankings_html(void);
#endif /* EXPORT_RECORDS_RANKINGS_H */
