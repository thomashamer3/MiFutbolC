/**
 * @file camiseta.c
 * @brief Funciones para gestionar camisetas (jerseys)
 * @author Usuario
 * @date 2025
 */

#include "camiseta.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * @brief Obtiene el siguiente ID disponible para una nueva camiseta
 *
 * Busca el ID más pequeño disponible reutilizando espacios de IDs eliminados.
 * Utiliza una consulta SQL que encuentra el primer hueco en la secuencia de IDs.
 *
 * @return El ID disponible más pequeño (comenzando desde 1 si la tabla está vacía)
 */
static int obtener_siguiente_id_camiseta()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "WITH RECURSIVE seq(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM seq WHERE id < (SELECT COALESCE(MAX(id),0)+1 FROM camiseta)) SELECT MIN(id) FROM seq WHERE id NOT IN (SELECT id FROM camiseta)",
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
 * @brief Verifica si hay camisetas registradas en la base de datos
 *
 * @return 1 si hay al menos una camiseta, 0 si no hay ninguna
 */
static int hay_camisetas()
{
    if (db == NULL) return 0;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, "SELECT id FROM camiseta LIMIT 1", -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    int has = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return has;
}

/**
 * @brief Crea una nueva camiseta en la base de datos
 *
 * Solicita al usuario el nombre y número de la camiseta y la inserta en la tabla 'camiseta'.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 * La columna 'sorteada' se inicializa en 0 por defecto.
 */
void crear_camiseta()
{
    clear_screen();
    char nombre[50];
    input_string("Nombre y Numero: ", nombre, 50);

    int id = obtener_siguiente_id_camiseta();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "INSERT INTO camiseta(id, nombre) VALUES(?, ?)",
                       -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Muestra un listado de todas las camisetas registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las camisetas
 * con sus respectivos IDs y nombres. Si no hay camisetas, muestra un mensaje informativo.
 */
void listar_camisetas()
{
    clear_screen();
    print_header("LISTADO DE CAMISETAS");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT id, nombre FROM camiseta",
                       -1, &stmt, NULL);

    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d - %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        printf("No hay camisetas cargadas\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Permite editar el nombre de una camiseta existente
 *
 * Muestra la lista de camisetas disponibles, solicita el ID a editar,
 * verifica que exista y permite cambiar el nombre.
 */
void editar_camiseta()
{
    clear_screen();
    print_header("EDITAR CAMISETA");

    if (!hay_camisetas())
    {
        printf("No hay camisetas para editar.\n");
        pause_console();
        return;
    }

    printf("Camisetas disponibles:\n\n");
    listar_camisetas();

    int id = input_int("\nID a editar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    char nombre[100];
    input_string("Nuevo nombre: ", nombre, sizeof(nombre));

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "UPDATE camiseta SET nombre=? WHERE id=?",
                       -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("\nCamiseta actualizada correctamente\n");
    pause_console();
}

/**
 * @brief Elimina una camiseta de la base de datos
 *
 * Muestra la lista de camisetas disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de eliminar.
 */
void eliminar_camiseta()
{
    clear_screen();
    print_header("ELIMINAR CAMISETA");

    if (!hay_camisetas())
    {
        printf("No hay camisetas para eliminar.\n");
        pause_console();
        return;
    }

    printf("Camisetas disponibles:\n\n");
    listar_camisetas();

    int id = input_int("\nID a eliminar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    if (!confirmar("Seguro que desea eliminar esta camiseta?"))
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "DELETE FROM camiseta WHERE id=?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("\nCamiseta eliminada correctamente\n");
    pause_console();
}

/**
 * @brief Realiza un sorteo aleatorio entre las camisetas disponibles
 *
 * Selecciona aleatoriamente una camiseta que no haya sido sorteada previamente.
 * Si todas las camisetas han sido sorteadas, reinicia el sorteo marcando todas como no sorteadas.
 * Muestra el resultado del sorteo y actualiza el estado de la camiseta seleccionada.
 *
 * @note Utiliza la función srand() con time(NULL) para generar números aleatorios
 * @note Limita el sorteo a un máximo de 100 camisetas por simplicidad
 */
void sortear_camiseta()
{
    clear_screen();
    print_header("SORTEO DE CAMISETAS");

    // Contar camisetas no sorteadas
    sqlite3_stmt *stmt_count;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta WHERE sorteada = 0", -1, &stmt_count, NULL);
    sqlite3_step(stmt_count);
    int disponibles = sqlite3_column_int(stmt_count, 0);
    sqlite3_finalize(stmt_count);

    if (disponibles == 0)
    {
        // Resetear todas las camisetas
        sqlite3_exec(db, "UPDATE camiseta SET sorteada = 0", 0, 0, 0);
        printf("Todas las camisetas han sido sorteadas. Reiniciando sorteo...\n\n");
        // Recalcular disponibles después del reset
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &stmt_count, NULL);
        sqlite3_step(stmt_count);
        disponibles = sqlite3_column_int(stmt_count, 0);
        sqlite3_finalize(stmt_count);
    }

    if (disponibles == 0)
    {
        printf("No hay camisetas para sortear.\n");
        pause_console();
        return;
    }

    // Obtener IDs de camisetas no sorteadas
    int ids[100]; // Asumiendo máximo 100 camisetas
    sqlite3_stmt *stmt_ids;
    sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE sorteada = 0", -1, &stmt_ids, NULL);

    int i = 0;
    while (sqlite3_step(stmt_ids) == SQLITE_ROW && i < 100)
    {
        ids[i++] = sqlite3_column_int(stmt_ids, 0);
    }
    sqlite3_finalize(stmt_ids);

    // Seleccionar aleatoriamente
    srand(time(NULL));
    int seleccionado = ids[rand() % disponibles];

    // Marcar como sorteada y obtener nombre
    sqlite3_stmt *stmt_update;
    sqlite3_prepare_v2(db, "UPDATE camiseta SET sorteada = 1 WHERE id = ?", -1, &stmt_update, NULL);
    sqlite3_bind_int(stmt_update, 1, seleccionado);
    sqlite3_step(stmt_update);
    sqlite3_finalize(stmt_update);

    // Obtener nombre de la camiseta sorteada
    sqlite3_stmt *stmt_nombre;
    sqlite3_prepare_v2(db, "SELECT nombre FROM camiseta WHERE id = ?", -1, &stmt_nombre, NULL);
    sqlite3_bind_int(stmt_nombre, 1, seleccionado);
    sqlite3_step(stmt_nombre);
    const char *nombre = (const char *)sqlite3_column_text(stmt_nombre, 0);

    printf("¡CAMISETA SORTEADA!\n\n");
    printf("La camiseta seleccionada es: %s\n", nombre);
    printf("Quedan %d camisetas por sortear.\n", disponibles - 1);

    sqlite3_finalize(stmt_nombre);
    pause_console();
}

/**
 * @brief Muestra el menú principal de gestión de camisetas
 *
 * Presenta un menú interactivo con opciones para crear, listar, editar,
 * eliminar y sortear camisetas. Utiliza la función ejecutar_menu para
 * manejar la navegación del menú.
 */
void menu_camisetas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_camiseta},
        {2, "Listar", listar_camisetas},
        {3, "Editar", editar_camiseta},
        {4, "Eliminar", eliminar_camiseta},
        {5, "Sortear", sortear_camiseta},
        {0, "Volver", NULL}
    };
    ejecutar_menu("CAMISETAS", items, 6);
}
