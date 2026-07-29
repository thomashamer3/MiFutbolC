/**
 * @file ranking_amigos.h
 * @brief API publica para ranking de amigos
 *
 * Importa perfiles de companeros y genera rankings
 * comparando estadisticas entre ellos.
 */

#ifndef RANKING_AMIGOS_H
#define RANKING_AMIGOS_H

/**
 * @brief Muestra el menu de ranking de amigos
 */
void menu_ranking_amigos(void);

/**
 * @brief Agrega un amigo al ranking
 */
void agregar_amigo(void);

/**
 * @brief Lista todos los amigos registrados
 */
void listar_amigos(void);

/**
 * @brief Modifica un amigo existente
 */
void modificar_amigo(void);

/**
 * @brief Elimina un amigo del ranking
 */
void eliminar_amigo(void);

/**
 * @brief Muestra el ranking completo de amigos
 */
void mostrar_ranking(void);

/**
 * @brief Compara tu estadistica con la de un amigo
 */
void comparar_con_amigo(void);

#endif
