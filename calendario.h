/**
 * @file calendario.h
 * @brief Calendario dinámico visual para MiFutbolC
 *
 * Muestra un calendario mensual con eventos marcados:
 * - Partidos programados
 * - Recordatorios médicos/lesiones
 * - Transacciones financieras
 * - Logros desbloqueados
 */

#ifndef CALENDARIO_H
#define CALENDARIO_H

/**
 * @brief Muestra el calendario del mes actual con eventos
 *
 * Presenta un calendario visual tipo ASCII con iconos
 * que indican eventos importantes en cada día.
 */
void mostrar_calendario(void);

/**
 * @brief Muestra el calendario de un mes específico
 *
 * @param mes Mes (1-12)
 * @param anio Año (ej: 2026)
 */
void mostrar_calendario_mes(int mes, int anio);

/**
 * @brief Muestra el menú de navegación del calendario
 *
 * Permite navegar entre meses y ver detalles de días específicos.
 */
void menu_calendario(void);

/**
 * @brief Muestra eventos de un día específico
 *
 * @param dia Día del mes
 * @param mes Mes (1-12)
 * @param anio Año
 */
void mostrar_eventos_dia(int dia, int mes, int anio);

#endif // CALENDARIO_H
