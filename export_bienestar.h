/**
 * @file export_bienestar.h
 * @brief Funciones para exportar datos de bienestar
 */

#ifndef EXPORT_BIENESTAR_H
#define EXPORT_BIENESTAR_H

/**
 * @brief Exporta los datos de bienestar a formato CSV
 */
void exportar_bienestar_csv(void);

/**
 * @brief Exporta los datos de bienestar a formato TXT
 */
void exportar_bienestar_txt(void);

/**
 * @brief Exporta los datos de bienestar a formato JSON
 */
void exportar_bienestar_json(void);

/**
 * @brief Exporta los datos de bienestar a formato HTML
 */
void exportar_bienestar_html(void);

/**
 * @brief Exporta los datos de bienestar a los 4 formatos con una sola consulta SQL.
 */
void exportar_bienestar_all(void);

#endif /* EXPORT_BIENESTAR_H */
