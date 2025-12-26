/**
 * @file cancha.h
 * @brief Declaraciones de funciones para la gestión de canchas en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * la gestión de canchas de fútbol, incluyendo creación, listado, modificación
 * y eliminación de registros de canchas.
 */

/**
 * @brief Muestra el menú principal de gestión de canchas
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar canchas. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_canchas();

/**
 * @brief Crea una nueva cancha en la base de datos
 *
 * Solicita al usuario el nombre de la cancha y lo inserta en la tabla 'cancha'.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_cancha();

/**
 * @brief Muestra un listado de todas las canchas registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las canchas
 * con sus respectivos datos: ID y nombre.
 *
 * @note Si no hay canchas registradas, muestra un mensaje informativo
 */
void listar_canchas();

/**
 * @brief Permite eliminar una cancha existente
 *
 * Muestra la lista de canchas disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de proceder con
 * la eliminación del registro de la base de datos.
 */
void eliminar_cancha();

/**
 * @brief Permite modificar el nombre de una cancha existente
 *
 * Muestra la lista de canchas disponibles, solicita el ID a modificar,
 * verifica que exista y permite cambiar el nombre de la cancha.
 */
void modificar_cancha();
