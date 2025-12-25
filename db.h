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
