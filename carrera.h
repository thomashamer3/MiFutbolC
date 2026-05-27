/**
 * @file carrera.h
 * @brief Modulo de Carrera Futbolistica del usuario
 *
 * Muestra un resumen integral de toda la trayectoria deportiva del usuario,
 * incluyendo estadisticas acumuladas, historial, mejor anio y promedios.
 */

#ifndef CARRERA_H
#define CARRERA_H

/**
 * @brief Muestra el menu principal de Carrera Futbolistica
 */
void menu_carrera_futbolistica(void);

/**
 * @brief Muestra aviso de recuerdos del dia al iniciar la app
 *
 * Si encuentra recuerdos asociados a la fecha actual, ofrece abrir
 * el Modo Retro de forma opcional.
 */
void carrera_notificar_modo_retro_inicio(void);

#endif
