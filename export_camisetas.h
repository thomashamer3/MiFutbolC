/**
 * @file export_camisetas.h
 * @brief Funciones para exportar datos de camisetas
 */

#ifndef EXPORT_CAMISETAS_H
#define EXPORT_CAMISETAS_H

/**
 * @brief Exporta las camisetas a formato CSV
 */
void exportar_camisetas_csv();

/**
 * @brief Exporta las camisetas a formato TXT
 */
void exportar_camisetas_txt();

/**
 * @brief Exporta las camisetas a formato JSON
 */
void exportar_camisetas_json();

/**
 * @brief Exporta las camisetas a formato HTML
 */
void exportar_camisetas_html();

/**
 * @brief Exporta las camisetas a los 4 formatos (CSV/TXT/JSON/HTML)
 *        ejecutando la consulta SQL una sola vez.
 */
void exportar_camisetas_all();

#endif /* EXPORT_CAMISETAS_H */
