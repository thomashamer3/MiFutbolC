/**
 * @file export_dashboard.h
 * @brief Funciones para exportar resumen de metricas del dashboard
 */

#ifndef EXPORT_DASHBOARD_H
#define EXPORT_DASHBOARD_H

/**
 * @brief Exporta las metricas del dashboard a formato CSV
 */
void exportar_dashboard_csv(void);

/**
 * @brief Exporta las metricas del dashboard a formato TXT
 */
void exportar_dashboard_txt(void);

/**
 * @brief Exporta las metricas del dashboard a formato JSON
 */
void exportar_dashboard_json(void);

/**
 * @brief Exporta las metricas del dashboard a formato HTML
 */
void exportar_dashboard_html(void);

/**
 * @brief Exporta el dashboard a los 4 formatos con una sola consulta SQL.
 */
void exportar_dashboard_all(void);

#endif /* EXPORT_DASHBOARD_H */
