/**
 * @file metas.h
 * @brief Gestion de metas personales con progreso automatico
 *
 * Permite definir metas y la app muestra el progreso automaticamente
 * contra la tabla de partidos.
 */

#ifndef METAS_H
#define METAS_H

/**
 * @brief Menu principal del modulo de metas
 */
void menu_metas(void);

/**
 * @brief Crea una nueva meta
 */
void metas_crear(void);

/**
 * @brief Lista todas las metas con su progreso
 */
void metas_listar(void);

/**
 * @brief Edita una meta existente
 */
void metas_editar(void);

/**
 * @brief Elimina una meta
 */
void metas_eliminar(void);

/**
 * @brief Recalcula el progreso de todas las metas contra la BD
 */
void metas_recalcular_progreso(void);

/**
 * @brief Muestra el dashboard de progreso de metas
 */
void metas_dashboard(void);

#endif
