/**
 * @file botin.h
 * @brief API publica para gestion de botines en MiFutbolC
 *
 * Define la interfaz para realizar operaciones CRUD (Crear, Leer, Actualizar, Borrar)
 * sobre la entidad botin, ademas de funcionalidad para fijar un botin predeterminado.
 */

#ifndef BOTIN_H
#define BOTIN_H

/**
 * @brief Muestra el menu interactivo de gestion de botines
 *
 * Presenta opciones para crear, listar, modificar, eliminar botines y
 * fijar un botin predeterminado.
 */
void menu_botines(void);

/**
 * @brief Crea un nuevo botin en la base de datos
 *
 * Solicita al usuario el nombre del botin y lo persiste en el sistema.
 */
void crear_botin(void);

/**
 * @brief Lista todos los botines registrados
 *
 * Recupera de la base de datos el listado completo de botines y
 * los muestra en formato de tabla en la consola.
 */
void listar_botines(void);

/**
 * @brief Edita el nombre de un botin existente
 *
 * Permite al usuario seleccionar un botin por su ID y modificar su nombre.
 */
void editar_botin(void);

/**
 * @brief Elimina un botin de la base de datos
 *
 * Solicita el ID del botin a eliminar y, tras confirmar con el usuario,
 * remueve el registro permanentemente de la base de datos.
 */
void eliminar_botin(void);

/**
 * @brief Fija o cambia el botin predeterminado
 *
 * Permite al usuario seleccionar que botin se usara siempre por defecto
 * al cargar un nuevo partido.
 */
void fijar_botin_predeterminado(void);

/**
 * @brief Obtiene el ID del botin predeterminado configurado
 *
 * @return ID del botin predeterminado, o 0 si no hay configurado
 */
int botin_obtener_predeterminado(void);

/**
 * @brief Realiza un sorteo aleatorio entre botines disponibles
 *
 * Selecciona al azar uno de los botines registrados en el sistema.
 */
void sortear_botin(void);

/**
 * @brief Carga una imagen asociada a un botin
 *
 * Permite seleccionar una imagen del sistema, copiarla a la carpeta
 * Imagenes de la app y guardar su ruta relativa en la base de datos.
 */
void cargar_imagen_botin(void);

/**
 * @brief Abre la imagen asociada a un botin
 *
 * Recupera la ruta de imagen registrada en DB y la abre con el visor
 * predeterminado del sistema operativo.
 */
void ver_imagen_botin(void);

#endif
