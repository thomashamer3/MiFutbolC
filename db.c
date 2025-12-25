/**
 * @file db.c
 * @brief Módulo para la gestión de la base de datos SQLite
 *
 * Este archivo contiene las funciones necesarias para inicializar, configurar y cerrar
 * la conexión a la base de datos SQLite utilizada por la aplicación MiFutbolC.
 */

#include "db.h"
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

/** Puntero global a la base de datos SQLite */
sqlite3 *db = NULL;

/** Directorio donde se almacena la base de datos */
static const char *DB_DIR = "data";

/** Ruta completa al archivo de la base de datos */
static const char *DB_PATH = "data/mifutbol.db";

/**
 * @brief Inicializa la base de datos
 *
 * Crea el directorio de datos si no existe, abre la conexión a la base de datos
 * y crea las tablas necesarias (camiseta, partido, lesion) si no existen.
 * También añade la columna 'sorteada' a la tabla camiseta si no está presente.
 *
 * @return 1 si la inicialización fue exitosa, 0 en caso de error
 */
int db_init()
{
    MKDIR(DB_DIR);

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK)
    {
        printf("Error abriendo DB\n");
        return 0;
    }
    const char *sql =
        "CREATE TABLE IF NOT EXISTS camiseta ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " sorteada INTEGER DEFAULT 0);"

        "CREATE TABLE IF NOT EXISTS partido ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " cancha TEXT NOT NULL,"
        " fecha_hora TEXT NOT NULL,"
        " goles INTEGER NOT NULL,"
        " asistencias INTEGER NOT NULL,"
        " camiseta_id INTEGER NOT NULL,"
        " FOREIGN KEY(camiseta_id) REFERENCES camiseta(id));"

        "CREATE TABLE IF NOT EXISTS lesion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " jugador TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " descripcion TEXT NOT NULL,"
        " fecha TEXT NOT NULL);"

        "ALTER TABLE camiseta ADD COLUMN sorteada INTEGER DEFAULT 0;";

    if (sqlite3_exec(db, sql, 0, 0, 0) != SQLITE_OK)
    {
        // Ignore error if column already exists
        if (sqlite3_errcode(db) != SQLITE_ERROR)
        {
            printf("Error creando tablas\n");
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Cierra la conexión a la base de datos
 *
 * Libera los recursos asociados con la conexión a la base de datos SQLite,
 * cerrando la conexión de manera segura si está abierta.
 */
void db_close()
{
    if (db)
        sqlite3_close(db);
}
