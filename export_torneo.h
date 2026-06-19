/**
 * @file export_torneo.h
 * @brief Funciones para exportar datos de torneos
 */

#ifndef EXPORT_TORNEO_H
#define EXPORT_TORNEO_H

/**
 * @brief Exporta los torneos a formato CSV
 */
void exportar_torneos_csv(void);

/**
 * @brief Exporta los torneos a formato TXT
 */
void exportar_torneos_txt(void);

/**
 * @brief Exporta los torneos a formato JSON
 */
void exportar_torneos_json(void);

/**
 * @brief Exporta los torneos a formato HTML
 */
void exportar_torneos_html(void);

/**
 * @brief Exporta los torneos a los 4 formatos con una sola consulta SQL.
 */
void exportar_torneos_all(void);

#endif /* EXPORT_TORNEO_H */
