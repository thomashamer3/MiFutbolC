/**
 * @file export_camisetas.h
 * @brief Funciones para exportar datos de camisetas
 */

#ifndef EXPORT_CAMISETAS_H
#define EXPORT_CAMISETAS_H

/**
 * @brief Exporta las camisetas a formato CSV
 */
void exportar_camisetas_csv(void);

/**
 * @brief Exporta las camisetas a formato TXT
 */
void exportar_camisetas_txt(void);

/**
 * @brief Exporta las camisetas a formato JSON
 */
void exportar_camisetas_json(void);

/**
 * @brief Exporta las camisetas a formato HTML
 */
void exportar_camisetas_html(void);

/**
 * @brief Exporta las camisetas a los 4 formatos (CSV/TXT/JSON/HTML)
 *        ejecutando la consulta SQL una sola vez.
 */
void exportar_camisetas_all(void);

/**
 * @brief Exporta el listado filtrado de camisetas a los 4 formatos.
 *
 * Reutiliza el mismo formato/conjunto de datos que exportar_camisetas_all(),
 * pero permite filtrar por estado y por texto de nombre (LIKE).
 *
 * @param filtro_estado 0 = todas, 1 = solo activas, 2 = solo inactivas.
 * @param nombre_like   Texto a buscar en el nombre (NULL o "" = sin filtro).
 */
void exportar_camisetas_filtrado_all(int filtro_estado, const char *nombre_like);

#endif /* EXPORT_CAMISETAS_H */
