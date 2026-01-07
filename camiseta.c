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
 * Mantiene IDs secuenciales reutilizando espacios de eliminaciones para
 * evitar IDs excesivamente altos y facilitar la navegación del usuario.
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
 * Previene operaciones en datasets vacíos y permite mostrar mensajes
 * informativos apropiados al usuario.
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
 * Permite a los usuarios agregar camisetas para gestión y sorteos,
 * reutilizando IDs eliminados para mantener la secuencia.
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
 * Proporciona visibilidad a los usuarios de las camisetas disponibles
 * para facilitar la toma de decisiones en otras operaciones.
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
 * Permite correcciones en la información de camisetas sin necesidad
 * de eliminar y recrear registros, mejorando la usabilidad.
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
 * Permite remover camisetas que ya no son necesarias mientras
 * mantiene la integridad de los datos con validaciones y confirmaciones.
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
 * @brief Reinicia el estado de sorteo de todas las camisetas
 *
 * Necesario cuando todas las camisetas han sido sorteadas para permitir
 * nuevos sorteos sin requerir recrear registros.
 */
static void reiniciar_sorteo()
{
    sqlite3_exec(db, "UPDATE camiseta SET sorteada = 0", 0, 0, 0);
    printf("Todas las camisetas han sido sorteadas. Reiniciando sorteo...\n\n");
}

/**
 * @brief Obtiene la lista de IDs de camisetas disponibles para sorteo
 *
 * @param ids Array donde almacenar los IDs
 * @param max Tamaño máximo del array
 * @return Número de IDs obtenidos
 */
static int obtener_ids_disponibles(int ids[], int max)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id FROM camiseta WHERE sorteada = 0", -1, &stmt, NULL);

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < max)
    {
        ids[i++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return i;
}

/**
 * @brief Selecciona aleatoriamente un ID de la lista proporcionada
 *
 * @param ids Array de IDs disponibles
 * @param count Número de IDs en el array
 * @return ID seleccionado aleatoriamente
 */
static int seleccionar_id_aleatorio(int ids[], int count)
{
    srand(time(NULL));
    return ids[rand() % count];
}

/**
 * @brief Marca una camiseta como sorteada en la base de datos
 *
 * @param id ID de la camiseta a marcar
 */
static void marcar_camiseta_sorteada(int id)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE camiseta SET sorteada = 1 WHERE id = ?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Obtiene el nombre de una camiseta por su ID
 *
 * @param id ID de la camiseta
 * @return Nombre de la camiseta
 */
static const char* obtener_nombre_camiseta(int id)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT nombre FROM camiseta WHERE id = ?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
    sqlite3_finalize(stmt);
    return nombre;
}

/**
 * @brief Realiza un sorteo aleatorio entre las camisetas disponibles
 *
 * Permite sorteos continuos reutilizando camisetas ya sorteadas cuando
 * se agotan las disponibles, manteniendo la funcionalidad del sistema
 * sin necesidad de intervención manual del usuario.
 */
void sortear_camiseta()
{
    clear_screen();
    print_header("SORTEO DE CAMISETAS");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta WHERE sorteada = 0", -1, &stmt, NULL);
    sqlite3_step(stmt);
    int disponibles = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (disponibles == 0)
    {
        reiniciar_sorteo();
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &stmt, NULL);
        sqlite3_step(stmt);
        disponibles = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    if (disponibles == 0)
    {
        printf("No hay camisetas para sortear.\n");
        pause_console();
        return;
    }

    int ids[150];
    int count = obtener_ids_disponibles(ids, 150);
    int seleccionado = seleccionar_id_aleatorio(ids, count);

    marcar_camiseta_sorteada(seleccionado);
    const char *nombre = obtener_nombre_camiseta(seleccionado);

    printf("¡CAMISETA SORTEADA!\n\n");
    printf("La camiseta seleccionada es: %s\n", nombre);
    printf("Quedan %d camisetas por sortear.\n", disponibles - 1);

    pause_console();
}

/**
 * @brief Muestra el menú principal de gestión de camisetas
 *
 * Proporciona una interfaz estructurada para las operaciones de
 * gestión de camisetas, centralizando el acceso a todas las funcionalidades.
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
