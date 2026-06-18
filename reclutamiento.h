/**
 * @file reclutamiento.h
 * @brief Pipeline de reclutamiento de jugadores
 *
 * Gestiona el proceso de reclutamiento personal: Visto -> Prospecto ->
 * En Seguimiento -> Contactado -> Reclutado / Descartado.
 */

#ifndef RECLUTAMIENTO_H
#define RECLUTAMIENTO_H

/**
 * @brief Menu principal del modulo de reclutamiento
 */
void menu_reclutamiento(void);

/**
 * @brief Crea un nuevo prospecto en el pipeline
 */
void reclutamiento_crear(void);

/**
 * @brief Lista todos los prospectos
 */
void reclutamiento_listar(void);

/**
 * @brief Edita un prospecto existente
 */
void reclutamiento_editar(void);

/**
 * @brief Avanza un prospecto al siguiente estado del pipeline
 */
void reclutamiento_avanzar(void);

/**
 * @brief Retrocede un prospecto al estado anterior del pipeline
 */
void reclutamiento_retroceder(void);

/**
 * @brief Elimina un prospecto del pipeline
 */
void reclutamiento_eliminar(void);

/**
 * @brief Muestra estadisticas del pipeline de reclutamiento
 */
void reclutamiento_estadisticas(void);

#endif
