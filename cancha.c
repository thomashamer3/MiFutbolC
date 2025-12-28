#include "cancha.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>

/**
 * @brief Obtiene el siguiente ID disponible para una nueva cancha
 *
 * Busca el ID más pequeño disponible reutilizando espacios de IDs eliminados.
 * Utiliza una consulta SQL que encuentra el primer hueco en la secuencia de IDs.
 *
 * @return El ID disponible más pequeño (comenzando desde 1 si la tabla está vacía)
 */
static int obtener_siguiente_id_cancha()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "WITH RECURSIVE seq(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM seq WHERE id < (SELECT COALESCE(MAX(id),0)+1 FROM cancha)) SELECT MIN(id) FROM seq WHERE id NOT IN (SELECT id FROM cancha)",
                       -1, &stmt, NULL);

    int id = 1; // Default si la tabla está vacía
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief Verifica si hay canchas registradas en la base de datos
 *
 * @return 1 si hay al menos una cancha, 0 si no hay ninguna
 */
static int hay_canchas()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM cancha", -1, &stmt, NULL);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

/**
 * @brief Crea una nueva cancha en la base de datos
 *
 * Solicita al usuario el nombre de la cancha y lo inserta en la tabla 'cancha'.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_cancha()
{
    char nombre[100];
    input_string("Nombre de la cancha: ", nombre, 100);

    int id = obtener_siguiente_id_cancha();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "INSERT INTO cancha(id, nombre) VALUES(?, ?)",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Cancha creada correctamente\n");
    pause_console();
}

/**
 * @brief Muestra un listado de todas las canchas registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las canchas
 * con sus respectivos datos: ID y nombre.
 *
 * @note Si no hay canchas registradas, muestra un mensaje informativo
 */
void listar_canchas()
{
    clear_screen();
    print_header("LISTADO DE CANCHAS");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT id, nombre FROM cancha ORDER BY id",
                       -1, &stmt, NULL);

    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d | %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        printf("No hay canchas cargadas.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Elimina una cancha de la base de datos.
 *
 * Esta función permite al usuario eliminar una cancha existente. Primero muestra
 * la lista de canchas disponibles, solicita el ID de la cancha a eliminar,
 * verifica que la cancha exista, solicita confirmación al usuario y finalmente
 * elimina el registro de la base de datos si se confirma.
 *
 * @note Si la cancha no existe, muestra un mensaje de error y no realiza la eliminación.
 * @note Si el usuario no confirma la eliminación, la operación se cancela.
 */
void eliminar_cancha()
{
    clear_screen();
    print_header("ELIMINAR CANCHA");

    if (!hay_canchas())
    {
        printf("No hay canchas para eliminar.\n");
        pause_console();
        return;
    }

    listar_canchas();
    printf("\n");

    int id = input_int("ID Cancha a Eliminar (0 para cancelar): ");

    if (!existe_id("cancha", id))
    {
        printf("La Cancha no Existe\n");
        return;
    }

    if (!confirmar("¿Seguro que desea eliminar esta cancha?"))
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "DELETE FROM cancha WHERE id = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Cancha Eliminada Correctamente\n");
    pause_console();
}

/**
 * @brief Permite modificar el nombre de una cancha existente
 *
 * Muestra la lista de canchas disponibles, solicita el ID a modificar,
 * verifica que exista y permite cambiar el nombre de la cancha.
 */
void modificar_cancha()
{
    clear_screen();
    print_header("MODIFICAR CANCHA");

    if (!hay_canchas())
    {
        printf("No hay canchas para modificar.\n");
        pause_console();
        return;
    }

    listar_canchas();
    printf("\n");

    int id = input_int("ID Cancha a Modificar (0 para cancelar): ");

    if (!existe_id("cancha", id))
    {
        printf("La Cancha no Existe\n");
        return;
    }

    char nombre[100];
    input_string("Nuevo nombre de la cancha: ", nombre, sizeof(nombre));

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "UPDATE cancha SET nombre = ? WHERE id = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Cancha Modificada Correctamente\n");
    pause_console();
}

/**
 * @brief Muestra el menú principal de gestión de canchas
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar canchas. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_canchas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_cancha},
        {2, "Listar", listar_canchas},
        {3, "Modificar", modificar_cancha},
        {4, "Eliminar", eliminar_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu("CANCHAS", items, 5);
}
