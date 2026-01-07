/**
 * @file camiseta.h
 * @brief API pública para gestión de camisetas en MiFutbolC
 *
 * Define la interfaz pública para operaciones CRUD de camisetas
 * y funcionalidades de sorteo.
 */

/**
 * @brief Muestra el menú interactivo de gestión de camisetas
 */
void menu_camisetas();

/**
 * @brief Crea una nueva camiseta en la base de datos
 */
void crear_camiseta();

/**
 * @brief Lista todas las camisetas registradas
 */
void listar_camisetas();

/**
 * @brief Edita el nombre de una camiseta existente
 */
void editar_camiseta();

/**
 * @brief Elimina una camiseta de la base de datos
 */
void eliminar_camiseta();

/**
 * @brief Realiza un sorteo aleatorio entre camisetas disponibles
 */
void sortear_camiseta();
