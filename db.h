/**
 * @file db.h
 * @brief Declaraciones para la gestión de la base de datos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones y variables
 * relacionadas con la inicialización y cierre de la conexión a la base
 * de datos SQLite utilizada por el sistema.
 */

#include "sqlite3.h"

/**
 * @brief Puntero global a la conexión de la base de datos SQLite
 *
 * Esta variable externa mantiene la conexión activa a la base de datos
 * durante toda la ejecución del programa. Se inicializa en db_init()
 * y se cierra en db_close().
 */
extern sqlite3 *db;

/**
 * @brief Inicializa la conexión a la base de datos
 *
 * Abre la conexión a la base de datos SQLite y crea las tablas necesarias
 * si no existen. La base de datos se almacena en el archivo 'data/mifutbol.db'.
 *
 * @return 0 si la inicialización fue exitosa, código de error en caso contrario
 */
int db_init();

/**
 * @brief Cierra la conexión a la base de datos
 *
 * Finaliza la conexión activa a la base de datos SQLite y libera los recursos asociados.
 */
void db_close();

/**
 * @brief Obtiene el nombre del usuario de la base de datos
 *
 * Consulta la tabla 'usuario' y devuelve el nombre del usuario si existe.
 * Si no hay usuario registrado, devuelve NULL.
 *
 * @return Puntero a cadena con el nombre del usuario, o NULL si no existe
 */
char* get_user_name();

/**
 * @brief Establece o actualiza el nombre del usuario en la base de datos
 *
 * Inserta un nuevo registro en la tabla 'usuario' si no existe, o actualiza el existente.
 *
 * @param nombre El nombre del usuario a guardar
 * @return 1 si la operación fue exitosa, 0 en caso de error
 */
int set_user_name(const char* nombre);

/**
 * @brief Obtiene la ruta del directorio de datos
 *
 * @return Puntero a cadena con la ruta del directorio de datos
 */
const char* get_data_dir();
