/**
 * @file export_partidos.h
 * @brief Declaraciones de funciones para exportar datos de partidos
 *
 * Este archivo contiene las declaraciones de las funciones de exportación
 * para partidos en diferentes formatos: CSV, TXT, JSON y HTML.
 */

#ifndef EXPORT_PARTIDOS_H
#define EXPORT_PARTIDOS_H

/** @name Funciones de exportación de partidos */
/** @{ */

/**
 * @brief Exporta la lista de partidos a formato CSV
 *
 * Genera un archivo CSV con todos los registros de partidos,
 * incluyendo cancha, fecha, goles, asistencias, camiseta y otros campos.
 */
void exportar_partidos_csv(void);
/**
 * @brief Exporta la lista de partidos a formato TXT
 *
 * Genera un archivo de texto plano con la lista formateada de partidos.
 */
void exportar_partidos_txt(void);
/**
 * @brief Exporta la lista de partidos a formato JSON
 *
 * Genera un archivo JSON con un array de objetos representando los partidos.
 */
void exportar_partidos_json(void);
/**
 * @brief Exporta la lista de partidos a formato HTML
 *
 * Genera una página HTML con una tabla que muestra todos los partidos.
 */
void exportar_partidos_html(void);
/** @} */ /* End of Doxygen group */

/** @name Funciones de exportación de partidos específicos */
/** @{ */

/**
 * @brief Exporta el partido con más goles a formato CSV
 */
void exportar_partido_mas_goles_csv(void);
/**
 * @brief Exporta el partido con más goles a formato TXT
 */
void exportar_partido_mas_goles_txt(void);
/**
 * @brief Exporta el partido con más goles a formato JSON
 */
void exportar_partido_mas_goles_json(void);
/**
 * @brief Exporta el partido con más goles a formato HTML
 */
void exportar_partido_mas_goles_html(void);
/**
 * @brief Exporta el partido con más asistencias a formato CSV
 */
void exportar_partido_mas_asistencias_csv(void);
/**
 * @brief Exporta el partido con más asistencias a formato TXT
 */
void exportar_partido_mas_asistencias_txt(void);
/**
 * @brief Exporta el partido con más asistencias a formato JSON
 */
void exportar_partido_mas_asistencias_json(void);
/**
 * @brief Exporta el partido con más asistencias a formato HTML
 */
void exportar_partido_mas_asistencias_html(void);
/**
 * @brief Exporta el partido más reciente con menos goles a formato CSV
 */
void exportar_partido_menos_goles_reciente_csv(void);
/**
 * @brief Exporta el partido más reciente con menos goles a formato TXT
 */
void exportar_partido_menos_goles_reciente_txt(void);
/**
 * @brief Exporta el partido más reciente con menos goles a formato JSON
 */
void exportar_partido_menos_goles_reciente_json(void);
/**
 * @brief Exporta el partido más reciente con menos goles a formato HTML
 */
void exportar_partido_menos_goles_reciente_html(void);
/**
 * @brief Exporta el partido más reciente con menos asistencias a formato CSV
 */
void exportar_partido_menos_asistencias_reciente_csv(void);
/**
 * @brief Exporta el partido más reciente con menos asistencias a formato TXT
 */
void exportar_partido_menos_asistencias_reciente_txt(void);
/**
 * @brief Exporta el partido más reciente con menos asistencias a formato JSON
 */
void exportar_partido_menos_asistencias_reciente_json(void);
/**
 * @brief Exporta el partido más reciente con menos asistencias a formato HTML
 */
void exportar_partido_menos_asistencias_reciente_html(void);
/** @} */ /* End of Doxygen group */

/**
 * @brief Exporta todos los partidos a los 4 formatos con una sola consulta SQL.
 */
void exportar_partidos_all(void);

#endif /* EXPORT_PARTIDOS_H */
