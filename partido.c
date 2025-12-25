#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include <stdio.h>

/**
 * @brief Obtiene el siguiente ID disponible para un nuevo partido
 *
 * Busca el ID más pequeño disponible reutilizando espacios de IDs eliminados.
 * Utiliza una consulta SQL que encuentra el primer hueco en la secuencia de IDs.
 *
 * @return El ID disponible más pequeño (comenzando desde 1 si la tabla está vacía)
 */
static int obtener_siguiente_id_partido()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT COALESCE(MIN(t1.id + 1), 1) "
                       "FROM partido t1 "
                       "LEFT JOIN partido t2 ON t1.id + 1 = t2.id "
                       "WHERE t2.id IS NULL",
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
 * @brief Crea un nuevo partido en la base de datos
 *
 * Solicita al usuario los datos del partido (cancha, goles, asistencias, camiseta)
 * y lo inserta en la tabla 'partido'. Obtiene la fecha y hora actual automáticamente.
 * Verifica que la camiseta seleccionada exista antes de insertar.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_partido()
{
    char cancha[100], fecha[20];
    int goles, asistencias, camiseta;
    input_string("Cancha: ", cancha, 100);
    goles = input_int("Goles: ");
    asistencias = input_int("Asistencias: ");
    listar_camisetas();
    camiseta = input_int("ID Camiseta: ");

    if (!existe_id("camiseta", camiseta)) return;

    get_datetime(fecha, sizeof(fecha));

    int id = obtener_siguiente_id_partido();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "INSERT INTO partido(id, cancha,fecha_hora,goles,asistencias,camiseta_id)"
                       "VALUES(?,?,?,?,?,?)",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt,1,id);
    sqlite3_bind_text(stmt,2,cancha,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,fecha,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,4,goles);
    sqlite3_bind_int(stmt,5,asistencias);
    sqlite3_bind_int(stmt,6,camiseta);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra un listado de todos los partidos registrados
 *
 * Consulta la base de datos y muestra en pantalla todos los partidos
 * con sus respectivos datos: ID, cancha, fecha/hora, goles, asistencias
 * y nombre de la camiseta utilizada. Realiza un JOIN con la tabla camiseta
 * para obtener el nombre de la camiseta.
 *
 * @note Si no hay partidos registrados, muestra un mensaje informativo
 */
void listar_partidos()
{
    clear_screen();
    print_header("LISTADO DE PARTIDOS");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.id, cancha, fecha_hora, goles, asistencias, c.nombre "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id",
                       -1, &stmt, NULL);

    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d | %s | %s | G:%d A:%d | %s\n",
               sqlite3_column_int(stmt,0),
               sqlite3_column_text(stmt,1),
               sqlite3_column_text(stmt,2),
               sqlite3_column_int(stmt,3),
               sqlite3_column_int(stmt,4),
               sqlite3_column_text(stmt,5));
        hay = 1;
    }

    if (!hay)
        printf("No hay partidos cargados.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Elimina un partido de la base de datos.
 *
 * Esta función permite al usuario eliminar un partido existente. Primero muestra
 * la lista de partidos disponibles, solicita el ID del partido a eliminar,
 * verifica que el partido exista, solicita confirmación al usuario y finalmente
 * elimina el registro de la base de datos si se confirma.
 *
 * @note Si el partido no existe, muestra un mensaje de error y no realiza la eliminación.
 * @note Si el usuario no confirma la eliminación, la operación se cancela.
 */
void eliminar_partido()
{
    print_header("ELIMINAR PARTIDO");

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Eliminar: ");

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        return;
    }

    if (!confirmar("�Seguro que desea eliminar este partido?"))
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "DELETE FROM partido WHERE id = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Partido Eliminado Correctamente\n");
}
/**
 * @brief Permite modificar los datos de un partido existente
 *
 * Muestra la lista de partidos disponibles, solicita el ID a modificar,
 * verifica que exista y permite cambiar todos los campos del partido:
 * cancha, goles, asistencias y camiseta utilizada.
 * Verifica que la camiseta especificada exista antes de actualizar.
 */
void modificar_partido()
{
    print_header("MODIFICAR PARTIDO");

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Modificar: ");

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        return;
    }

    char cancha[100];
    int goles, asistencias, camiseta;

    input_string("Nueva cancha: ", cancha, sizeof(cancha));
    goles = input_int("Nuevos goles: ");
    asistencias = input_int("Nuevas asistencias: ");
    camiseta = input_int("Nuevo ID camiseta: ");

    if (!existe_id("camiseta", camiseta))
    {
        printf("La camiseta no existe\n");
        return;
    }

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "UPDATE partido "
                       "SET cancha=?, goles=?, asistencias=?, camiseta_id=? "
                       "WHERE id=?",
                       -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, cancha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, goles);
    sqlite3_bind_int(stmt, 3, asistencias);
    sqlite3_bind_int(stmt, 4, camiseta);
    sqlite3_bind_int(stmt, 5, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Partido Modificado Correctamente\n");
}
/**
 * @brief Muestra el menú principal de gestión de partidos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar partidos. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_partidos()
{
    MenuItem items[] =
    {
        {1, "Crear",    crear_partido},
        {2, "Listar",   listar_partidos},
        {3, "Modificar", modificar_partido},
        {4, "Eliminar", eliminar_partido},
        {0, "Volver",   NULL}
    };

    ejecutar_menu("PARTIDOS", items, 5);
}

