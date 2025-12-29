#include "lesion.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

static int current_lesion_id;

/**
 * @brief Obtiene el siguiente ID disponible para una nueva lesión
 *
 * Busca el ID más pequeño disponible reutilizando espacios de IDs eliminados.
 * Utiliza una consulta SQL que encuentra el primer hueco en la secuencia de IDs.
 *
 * @return El ID disponible más pequeño (comenzando desde 1 si la tabla está vacía)
 */
static int obtener_siguiente_id_lesion()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "WITH RECURSIVE seq(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM seq WHERE id < (SELECT COALESCE(MAX(id),0)+1 FROM lesion)) SELECT MIN(id) FROM seq WHERE id NOT IN (SELECT id FROM lesion)",
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
 * @brief Verifica si hay lesiones registradas en la base de datos
 *
 * @return 1 si hay al menos una lesión, 0 si no hay ninguna
 */
static int hay_lesiones()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion", -1, &stmt, NULL);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

/**
 * @brief Crea una nueva lesión en la base de datos
 *
 * Solicita al usuario el tipo y descripción de la lesión, obtiene la fecha actual
 * y la inserta en la tabla 'lesion'. El nombre del jugador se establece automáticamente
 * como "Hamer". Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_lesion()
{
    clear_screen();
    char tipo[100], descripcion[200], fecha[20];

    input_string("Tipo de lesion: ", tipo, sizeof(tipo));
    input_string("Descripcion: ", descripcion, sizeof(descripcion));
    get_datetime(fecha, sizeof(fecha));

    int id = obtener_siguiente_id_lesion();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "INSERT INTO lesion(id, jugador, tipo, descripcion, fecha) VALUES(?,?,?,?,?)",
                       -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, "Hamer", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("\nLesion creada correctamente\n");
    pause_console();
}

/**
 * @brief Muestra un listado de todas las lesiones registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las lesiones
 * con sus respectivos datos: ID, tipo, descripción y fecha.
 * Si no hay lesiones registradas, muestra un mensaje informativo.
 */
void listar_lesiones()
{
    clear_screen();
    print_header("LISTADO DE LESIONES");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT id, tipo, descripcion, fecha FROM lesion",
                       -1, &stmt, NULL);

    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d - |Tipo Lesion:%s |Descripcion:%s |Fecha:%s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_text(stmt, 3));
        hay = 1;
    }

    if (!hay)
        printf("No hay lesiones cargadas\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Modifica el tipo de una lesión existente
 */
static void modificar_tipo_lesion()
{
    char tipo[100];
    input_string("Nuevo tipo de lesion: ", tipo, sizeof(tipo));
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE lesion SET tipo=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_lesion_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Tipo modificado correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la descripción de una lesión existente
 */
static void modificar_descripcion_lesion()
{
    char descripcion[200];
    input_string("Nueva descripcion: ", descripcion, sizeof(descripcion));
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE lesion SET descripcion=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_lesion_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Descripcion modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la fecha de una lesión existente
 */
static void modificar_fecha_lesion()
{
    char fecha[20], hora[10], fecha_hora[30];
    printf("Nueva fecha (dd/mm/yyyy): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;
    printf("Nueva hora (hh:mm): ");
    fgets(hora, sizeof(hora), stdin);
    hora[strcspn(hora, "\n")] = 0;
    sprintf(fecha_hora, "%s %s", fecha, hora);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE lesion SET fecha=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, fecha_hora, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_lesion_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Fecha modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica todos los campos de una lesión existente
 */
static void modificar_todo_lesion()
{
    char tipo[100], descripcion[200], fecha[20];
    input_string("Nuevo tipo de lesion: ", tipo, sizeof(tipo));
    input_string("Nueva descripcion: ", descripcion, sizeof(descripcion));
    input_string("Nueva fecha (DD/MM/YYYY HH:MM): ", fecha, sizeof(fecha));
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE lesion SET tipo=?, descripcion=?, fecha=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, current_lesion_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Lesion modificada correctamente\n");
    pause_console();
}

/**
 * @brief Permite modificar una lesión existente
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a modificar,
 * verifica que exista y muestra un menú con opciones para modificar campos individuales o todos.
 */
void modificar_lesion()
{
    clear_screen();
    print_header("MODIFICAR LESION");

    if (!hay_lesiones())
    {
        printf("No hay lesiones para modificar.\n");
        pause_console();
        return;
    }

    printf("Lesiones disponibles:\n\n");
    listar_lesiones();

    int id = input_int("\nID Lesion a Modificar (0 para cancelar): ");

    if (!existe_id("lesion", id))
    {
        printf("La Lesion no Existe\n");
        return;
    }

    current_lesion_id = id;

    MenuItem items[] =
    {
        {1, "Tipo", modificar_tipo_lesion},
        {2, "Descripcion", modificar_descripcion_lesion},
        {3, "Fecha", modificar_fecha_lesion},
        {4, "Modificar Todo", modificar_todo_lesion},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR LESION", items, 5);
}

/**
 * @brief Elimina una lesión de la base de datos
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de eliminar.
 * Una vez eliminada, el ID queda disponible para reutilización.
 */
void eliminar_lesion()
{
    clear_screen();
    print_header("ELIMINAR LESION");

    if (!hay_lesiones())
    {
        printf("No hay lesiones para eliminar.\n");
        pause_console();
        return;
    }

    printf("Lesiones disponibles:\n\n");
    listar_lesiones();

    int id = input_int("\nID a eliminar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("lesion", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    if (!confirmar("¿Seguro que desea eliminar esta lesion?"))
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "DELETE FROM lesion WHERE id=?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("\nLesion eliminada correctamente\n");
    pause_console();
}

/**
 * @brief Muestra el menú principal de gestión de lesiones
 *
 * Presenta un menú interactivo con opciones para crear, listar, editar
 * y eliminar lesiones. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_lesiones()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_lesion},
        {2, "Listar", listar_lesiones},
        {3, "Modificar", modificar_lesion},
        {4, "Eliminar", eliminar_lesion},
        {0, "Volver", NULL}
    };
    ejecutar_menu("LESIONES", items, 5);
}
