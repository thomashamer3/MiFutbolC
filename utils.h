/**
 * @file utils.h
 * @brief Declaraciones de funciones utilitarias para entrada/salida y manejo de datos.
 *
 * Este archivo de cabecera declara funciones auxiliares para interactuar con el usuario,
 * manejar fechas y horas, limpiar la pantalla, verificar existencia de IDs en la base de datos,
 * y gestionar directorios de exportación.
 */

#ifndef UTILS_H
#define UTILS_H

/**
 * @brief Solicita al usuario un número entero.
 *
 * Muestra el mensaje proporcionado y lee un entero desde la entrada estándar.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @return El número entero ingresado por el usuario.
 */
int input_int(const char *msg);

/**
 * @brief Solicita al usuario una cadena de texto.
 *
 * Muestra el mensaje proporcionado y lee una cadena desde la entrada estándar,
 * eliminando el carácter de nueva línea al final.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @param buffer El buffer donde se almacenará la cadena leída.
 * @param size El tamaño máximo del buffer.
 */
void input_string(const char *msg, char *buffer, int size);

/**
 * @brief Obtiene la fecha y hora actual en formato legible.
 *
 * Formatea la fecha y hora actual en el formato "dd/mm/yyyy hh:mm".
 *
 * @param buffer El buffer donde se almacenará la cadena formateada.
 * @param size El tamaño máximo del buffer.
 */
void get_datetime(char *buffer, int size);

/**
 * @brief Obtiene un timestamp actual en formato compacto.
 *
 * Formatea la fecha y hora actual en el formato "yyyymmdd_hhmm" para usar en nombres de archivos.
 *
 * @param buffer El buffer donde se almacenará la cadena formateada.
 * @param size El tamaño máximo del buffer.
 */
void get_timestamp(char *buffer, int size);

/**
 * @brief Limpia la pantalla de la consola.
 */
void clear_screen();

/**
 * @brief Imprime un encabezado con el título proporcionado.
 *
 * @param titulo El título a mostrar en el encabezado.
 */
void print_header(const char *titulo);

/**
 * @brief Pausa la ejecución del programa hasta que el usuario presione una tecla.
 */
void pause_console();

/**
 * @brief Verifica si existe un ID en una tabla de la base de datos.
 *
 * Ejecuta una consulta SQL para comprobar si un ID específico existe en la tabla indicada.
 *
 * @param tabla El nombre de la tabla a consultar.
 * @param id El ID a verificar.
 * @return 1 si el ID existe, 0 en caso contrario.
 */
int existe_id(const char *tabla, int id);

/**
 * @brief Solicita confirmación al usuario (Sí/No).
 *
 * Muestra el mensaje proporcionado seguido de "(S/N):" y lee la respuesta del usuario.
 * Acepta 's' o 'S' como afirmativo.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @return 1 si el usuario confirma (sí), 0 en caso contrario.
 */
int confirmar(const char *msg);

/**
 * @brief Obtiene la ruta del directorio de exportaciones.
 *
 * Construye y devuelve la ruta completa del directorio donde se almacenan
 * los archivos exportados. El directorio se crea si no existe.
 * En Windows, se ubica en el escritorio del usuario; en sistemas Unix/Linux,
 * en el directorio home del usuario.
 *
 * @return Un puntero a una cadena estática con la ruta del directorio.
 */
char* obtener_directorio_exports();

#endif
