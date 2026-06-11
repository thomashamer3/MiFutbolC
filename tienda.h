/**
 * @file tienda.h
 * @brief API publica para gestion de tiendas en MiFutbolC
 *
 * Define la interfaz para realizar operaciones CRUD sobre la entidad tienda,
 * permitiendo almacenar informacion de locales, webs o cualquier comercio
 * relacionado al futbol (botines, camisetas, indumentaria, etc.).
 */

#ifndef TIENDA_H
#define TIENDA_H

/**
 * @brief Muestra el menu interactivo de gestion de tiendas
 *
 * Presenta opciones para crear, listar, modificar y eliminar tiendas.
 * Gestiona el flujo de navegacion del usuario.
 */
void menu_tiendas();

/**
 * @brief Crea una nueva tienda en la base de datos
 *
 * Solicita al usuario todos los datos de la tienda (nombre, tipo,
 * datos de contacto, productos que vende, rango de precio, etc.)
 * y la persiste en el sistema.
 */
void crear_tienda();

/**
 * @brief Lista todas las tiendas registradas
 *
 * Recupera de la base de datos el listado completo de tiendas y
 * las muestra en formato de tabla en la consola.
 */
void listar_tiendas();

/**
 * @brief Modifica una tienda existente
 *
 * Permite al usuario seleccionar una tienda por su ID y modificar
 * sus campos.
 */
void modificar_tienda();

/**
 * @brief Elimina una tienda de la base de datos
 *
 * Solicita el ID de la tienda a eliminar y, tras confirmar con el usuario,
 * remueve el registro permanentemente de la base de datos.
 */
void eliminar_tienda();

#endif
