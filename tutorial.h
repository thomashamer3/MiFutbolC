/**
 * @file tutorial.h
 * @brief API publica para el modo tutorial/guia
 *
 * Guia interactiva para nuevos usuarios que recorre
 * las principales funcionalidades del sistema.
 */

#ifndef TUTORIAL_H
#define TUTORIAL_H

/**
 * @brief Inicia el tutorial completo para nuevos usuarios
 */
void iniciar_tutorial(void);

/**
 * @brief Muestra el menu del tutorial
 */
void menu_tutorial(void);

/**
 * @brief Muestra un paso especifico del tutorial
 * @param paso Numero del paso a mostrar (1-based)
 */
void mostrar_paso_tutorial(int paso);

/**
 * @brief Verifica si el usuario ya completo el tutorial
 * @return 1 si esta completo, 0 si no
 */
int tutorial_completado(void);

/**
 * @brief Reinicia el progreso del tutorial
 */
void reiniciar_tutorial(void);

#endif
