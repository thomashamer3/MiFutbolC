/**
 * @file equipamiento_historial.h
 * @brief Historial de equipamiento (botines, camisetas, etc.)
 *
 * Registra lo que ya compraste y usaste con estadisticas de uso,
 * rating personal y notas.
 */

#ifndef EQUIPAMIENTO_HISTORIAL_H
#define EQUIPAMIENTO_HISTORIAL_H

/**
 * @brief Menu principal del modulo de historial de equipamiento
 */
void menu_equipamiento_historial(void);

/**
 * @brief Registra un nuevo item de equipamiento
 */
void equipamiento_historial_crear(void);

/**
 * @brief Lista todos los items de equipamiento
 */
void equipamiento_historial_listar(void);

/**
 * @brief Edita un item de equipamiento
 */
void equipamiento_historial_editar(void);

/**
 * @brief Elimina un item de equipamiento
 */
void equipamiento_historial_eliminar(void);

/**
 * @brief Muestra estadisticas de uso del equipamiento
 */
void equipamiento_historial_estadisticas(void);

#endif
