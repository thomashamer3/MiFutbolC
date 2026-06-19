/**
 * @file export_temporada.h
 * @brief Funciones para exportar datos de temporadas
 */

#ifndef EXPORT_TEMPORADA_H
#define EXPORT_TEMPORADA_H

/**
 * @brief Exporta las temporadas a formato CSV
 */
void exportar_temporadas_csv(void);

/**
 * @brief Exporta las temporadas a formato TXT
 */
void exportar_temporadas_txt(void);

/**
 * @brief Exporta las temporadas a formato JSON
 */
void exportar_temporadas_json(void);

/**
 * @brief Exporta las temporadas a formato HTML
 */
void exportar_temporadas_html(void);

/**
 * @brief Exporta las temporadas a los 4 formatos con una sola consulta SQL.
 */
void exportar_temporadas_all(void);

#endif /* EXPORT_TEMPORADA_H */
