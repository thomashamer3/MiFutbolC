/**
 * @file camiseta.h
 * @brief Declaraciones de funciones para la gestión de camisetas en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * la gestión de camisetas de fútbol, incluyendo creación, listado, edición
 * y eliminación de registros de camisetas.
 */

/**
 * @brief Muestra el menú principal de gestión de camisetas
 *
 * Presenta un menú interactivo con opciones para crear, listar, editar
 * y eliminar camisetas. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_camisetas();

/**
 * @brief Crea una nueva camiseta en la base de datos
 *
 * Solicita al usuario el nombre de la camiseta y la inserta en la tabla 'camiseta'.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 * La columna 'sorteada' se inicializa en 0 por defecto.
 */
void crear_camiseta();

/**
 * @brief Muestra un listado de todas las camisetas registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las camisetas
 * con sus respectivos IDs y nombres. Si no hay camisetas, muestra un mensaje informativo.
 */
void listar_camisetas();

/**
 * @brief Permite editar el nombre de una camiseta existente
 *
 * Muestra la lista de camisetas disponibles, solicita el ID a editar,
 * verifica que exista y permite cambiar el nombre.
 */
void editar_camiseta();

/**
 * @brief Elimina una camiseta de la base de datos
 *
 * Muestra la lista de camisetas disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de eliminar.
 */
void eliminar_camiseta();

