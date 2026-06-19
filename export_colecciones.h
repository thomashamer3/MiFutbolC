/**
 * @file export_colecciones.h
 * @brief Funciones para exportar datos de colecciones e inventario
 */

#ifndef EXPORT_COLECCIONES_H
#define EXPORT_COLECCIONES_H

/**
 * @brief Exporta los datos de colecciones a formato CSV
 */
void exportar_colecciones_csv(void);

/**
 * @brief Exporta los datos de colecciones a formato TXT
 */
void exportar_colecciones_txt(void);

/**
 * @brief Exporta los datos de colecciones a formato JSON
 */
void exportar_colecciones_json(void);

/**
 * @brief Exporta los datos de colecciones a formato HTML
 */
void exportar_colecciones_html(void);

/** @brief Exporta colecciones a los 4 formatos con una sola consulta SQL */
void exportar_colecciones_all(void);

#endif /* EXPORT_COLECCIONES_H */
