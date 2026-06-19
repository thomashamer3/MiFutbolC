/**
 * @file export_calendario.h
 * @brief Funciones para exportar eventos del calendario
 */

#ifndef EXPORT_CALENDARIO_H
#define EXPORT_CALENDARIO_H

/**
 * @brief Exporta los eventos del calendario a formato CSV
 */
void exportar_calendario_csv(void);

/**
 * @brief Exporta los eventos del calendario a formato TXT
 */
void exportar_calendario_txt(void);

/**
 * @brief Exporta los eventos del calendario a formato JSON
 */
void exportar_calendario_json(void);

/**
 * @brief Exporta los eventos del calendario a formato HTML
 */
void exportar_calendario_html(void);

#endif /* EXPORT_CALENDARIO_H */
