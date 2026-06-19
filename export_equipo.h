/**
 * @file export_equipo.h
 * @brief Funciones para exportar datos de equipos
 */

#ifndef EXPORT_EQUIPO_H
#define EXPORT_EQUIPO_H

/**
 * @brief Exporta los equipos a formato CSV
 */
void exportar_equipos_csv(void);

/**
 * @brief Exporta los equipos a formato TXT
 */
void exportar_equipos_txt(void);

/**
 * @brief Exporta los equipos a formato JSON
 */
void exportar_equipos_json(void);

/**
 * @brief Exporta los equipos a formato HTML
 */
void exportar_equipos_html(void);

/**
 * @brief Exporta los equipos a los 4 formatos con una sola consulta SQL.
 */
void exportar_equipos_all(void);

#endif /* EXPORT_EQUIPO_H */
