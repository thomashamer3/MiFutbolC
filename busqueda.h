/**
 * @file busqueda.h
 * @brief Sistema de búsqueda global para MiFutbolC
 *
 * Permite buscar en múltiples tablas simultáneamente:
 * partidos, equipos, jugadores, camisetas, canchas, etc.
 */

#ifndef BUSQUEDA_H
#define BUSQUEDA_H

/**
 * @brief Menú principal de búsqueda global
 *
 * Presenta interfaz de búsqueda donde el usuario puede ingresar
 * un término y buscar en todas las tablas del sistema.
 */
void menu_busqueda_global();

/**
 * @brief Busca un término en la tabla de partidos
 *
 * @param termino Término de búsqueda
 * @return Número de resultados encontrados
 */
int buscar_en_partidos(const char *termino);

/**
 * @brief Busca un término en la tabla de equipos
 *
 * @param termino Término de búsqueda
 * @return Número de resultados encontrados
 */
int buscar_en_equipos(const char *termino);

/**
 * @brief Busca un término en la tabla de camisetas
 *
 * @param termino Término de búsqueda
 * @return Número de resultados encontrados
 */
int buscar_en_camisetas(const char *termino);

/**
 * @brief Busca un término en la tabla de canchas
 *
 * @param termino Término de búsqueda
 * @return Número de resultados encontrados
 */
int buscar_en_canchas(const char *termino);

/**
 * @brief Busca un término en todas las tablas
 *
 * @param termino Término de búsqueda
 */
void buscar_global(const char *termino);

#endif // BUSQUEDA_H
