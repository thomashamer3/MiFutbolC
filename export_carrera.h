/**
 * @file export_carrera.h
 * @brief Funciones para exportar datos de carrera futbolistica
 */

#ifndef EXPORT_CARRERA_H
#define EXPORT_CARRERA_H

/**
 * @brief Exporta los datos de carrera a formato CSV
 */
void exportar_carrera_csv(void);

/**
 * @brief Exporta los datos de carrera a formato TXT
 */
void exportar_carrera_txt(void);

/**
 * @brief Exporta los datos de carrera a formato JSON
 */
void exportar_carrera_json(void);

/**
 * @brief Exporta los datos de carrera a formato HTML
 */
void exportar_carrera_html(void);

/**
 * @brief Exporta los datos de carrera a formato PDF
 */
void exportar_carrera_pdf(void);

/** @brief Exporta carrera a csv/txt/json/html/pdf con una sola consulta SQL */
void exportar_carrera_all(void);

#endif /* EXPORT_CARRERA_H */
