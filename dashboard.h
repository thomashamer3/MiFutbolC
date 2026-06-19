/**
 * @file dashboard.h
 * @brief Dashboard interactivo inicial de MiFutbolC
 *
 * Este módulo proporciona un panel de control inicial que muestra
 * información resumida del sistema: próximos partidos, recordatorios,
 * racha actual, logros, balance financiero y notificaciones.
 */

#ifndef DASHBOARD_H
#define DASHBOARD_H

/**
 * @brief Muestra el dashboard principal con resumen de actividades
 *
 * Presenta un panel visual con:
 * - Próximos partidos programados
 * - Recordatorios del día actual
 * - Racha de victorias/derrotas actuales
 * - Progreso de logros desbloqueados
 * - Balance financiero del mes
 * - Notificaciones de actualizaciones disponibles
 *
 * @note Esta función se puede llamar al inicio de la aplicación
 *       o desde el menú principal como opción independiente
 */
void mostrar_dashboard(void);

/**
 * @brief Obtiene la racha actual de victorias o derrotas
 *
 * Analiza los últimos partidos para determinar la racha actual,
 * ya sea de victorias consecutivas, derrotas o empates.
 *
 * @param tipo_racha Puntero donde se almacenará el tipo ('V', 'D', 'E')
 * @return Número de partidos consecutivos en la racha
 */
int obtener_racha_actual(char *tipo_racha);

/**
 * @brief Cuenta los recordatorios programados para hoy
 *
 * @return Número de recordatorios para la fecha actual
 */
int contar_recordatorios_hoy(void);

/**
 * @brief Cuenta los próximos partidos (futuros)
 *
 * @return Número de partidos programados a futuro
 */
int contar_proximos_partidos(void);

/**
 * @brief Obtiene el balance financiero del mes actual
 *
 * Calcula ingresos menos gastos del mes en curso.
 *
 * @return Balance del mes (puede ser negativo)
 */
int obtener_balance_mes_actual(void);

/**
 * @brief Obtiene el progreso de logros desbloqueados
 *
 * @param total_logros Puntero donde se almacena el total de logros
 * @return Cantidad de logros completados
 */
int obtener_progreso_logros(int *total_logros);

#endif // DASHBOARD_H
