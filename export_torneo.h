/**
 * @file export_torneo.h
 * @brief Funciones para exportar datos de torneos
 */

#ifndef EXPORT_TORNEO_H
#define EXPORT_TORNEO_H

/**
 * @brief Exporta los torneos a formato CSV
 */
void exportar_torneos_csv();

/**
 * @brief Exporta los torneos a formato TXT
 */
void exportar_torneos_txt();

/**
 * @brief Exporta los torneos a formato JSON
 */
void exportar_torneos_json();

/**
 * @brief Exporta los torneos a formato HTML
 */
void exportar_torneos_html();

/**
 * @brief Exporta los torneos a los 4 formatos con una sola consulta SQL.
 */
void exportar_torneos_all();

#endif /* EXPORT_TORNEO_H */
