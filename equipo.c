/**
* @file equipo.c
* @brief Implementación de funciones para la gestión de equipos en MiFutbolC
*/

#include "equipo.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "partido.h"
#include "ascii_art.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include "sqlite3.h"

// Data structures to reduce parameter count in functions
typedef struct
{
    int goles_local;
    int goles_visitante;
    int goles_jugadores_local[11];
    int asistencias_jugadores_local[11];
    int goles_jugadores_visitante[11];
    int asistencias_jugadores_visitante[11];
} PartidoStats;

typedef struct
{
    int jugadores_ids[11];
    char jugadores_nombres[11][50];
    int jugadores_numeros[11];
    int jugadores_posiciones[11];
    int jugadores_capitanes[11];
    int jugador_count;
} EquipoPlayerInfo;

// Function prototypes
void crear_un_equipo_momentaneo();
void gestionar_equipo_momentaneo(Equipo *equipo);
void modificar_jugador_momentaneo(Equipo *equipo);
void agregar_jugador_momentaneo(Equipo *equipo);
void eliminar_jugador_momentaneo(Equipo *equipo);
void cambiar_capitan_momentaneo(Equipo *equipo);
void crear_dos_equipos_momentaneos();
void gestionar_dos_equipos_momentaneos(Equipo *equipo_local, Equipo *equipo_visitante);
void gestionar_equipo_individual(Equipo *equipo, const char *tipo_equipo);
void simular_partido(const Equipo *equipo_local, const Equipo *equipo_visitante);

void insert_jugadores_for_equipo(int equipo_id, const Equipo *equipo);

// Prototipo añadido para evitar declaración implícita
void modificar_jugador_existente(const int *jugadores_ids, char jugadores_nombres[][50],
                                 const int *jugadores_numeros, const int *jugadores_posiciones, const int *jugadores_capitanes, int jugador_count);

// Prototipo para selección de posición (evita declaración implícita al usarla antes)
Posicion select_posicion();

/**
 * @brief Actualiza el nombre de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param new_name Nuevo nombre del jugador
 * @return 1 si se actualizó exitosamente, 0 si hubo error
 */
int update_player_name(int player_id, const char *new_name)
{
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE jugador SET nombre = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, player_id);

    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

/**
 * @brief Actualiza el número de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param new_number Nuevo número del jugador
 * @return 1 si se actualizó exitosamente, 0 si hubo error
 */
int update_player_number(int player_id, int new_number)
{
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE jugador SET numero = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, new_number);
    sqlite3_bind_int(stmt, 2, player_id);

    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

/**
 * @brief Actualiza la posición de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param new_position Nueva posición del jugador
 * @return 1 si se actualizó exitosamente, 0 si hubo error
 */
int update_player_position(int player_id, Posicion new_position)
{
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE jugador SET posicion = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, new_position);
    sqlite3_bind_int(stmt, 2, player_id);

    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

/**
 * @brief Actualiza el estado de capitán de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param is_captain Nuevo estado de capitán (1=capitán, 0=no capitán)
 * @return 1 si se actualizó exitosamente, 0 si hubo error
 */
int update_player_captain_status(int player_id, int is_captain)
{
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE jugador SET es_capitan = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, is_captain);
    sqlite3_bind_int(stmt, 2, player_id);

    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

void agregar_nuevo_jugador(int equipo_id, int jugador_count, const int *jugadores_numeros);
void eliminar_jugador_existente(const int *jugadores_ids, const int *jugadores_numeros, int jugador_count);
/**
 * @brief Cambia el capitán de un equipo en la base de datos
 *
 * Función helper que maneja el cambio de capitán,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param equipo_id ID del equipo
 * @param info Puntero a estructura con información de los jugadores
 */
void cambiar_capitan_equipo(int equipo_id, EquipoPlayerInfo *info)
{
    printf("\nSeleccione el nuevo capitán:\n");
    for (int i = 0; i < info->jugador_count; i++)
    {
        printf("%d. %s (Actual: %s)\n", info->jugadores_numeros[i], info->jugadores_nombres[i],
               info->jugadores_capitanes[i] ? "CAPITAN" : "No");
    }

    int nuevo_capitan_num = input_int("Ingrese el número del nuevo capitán: ");

    // Buscar el jugador
    int nuevo_capitan_idx = -1;
    for (int i = 0; i < info->jugador_count; i++)
    {
        if (info->jugadores_numeros[i] == nuevo_capitan_num)
        {
            nuevo_capitan_idx = i;
            break;
        }
    }

    if (nuevo_capitan_idx == -1)
    {
        printf("Número de jugador no encontrado.\n");
        pause_console();
        return;
    }

    // Primero, quitar el capitán actual
    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE jugador SET es_capitan = 0 WHERE equipo_id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Luego, establecer el nuevo capitán
    sql_update = "UPDATE jugador SET es_capitan = 1 WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, info->jugadores_ids[nuevo_capitan_idx]);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Capitán cambiado exitosamente.\n");
        }
        else
        {
            printf("Error al cambiar el capitán: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Asigna un equipo a un partido existente
 *
 * Función helper que maneja la lógica de asignación de un equipo a un partido,
 * reduciendo la complejidad de anidamiento en handle_modify_team_assignment().
 *
 * @param equipo_id ID del equipo a asignar
 */
void assign_team_to_party(int equipo_id)
{
    listar_partidos();
    int partido_id = input_int("Ingrese el ID del partido (0 para cancelar): ");
    if (partido_id <= 0 || !existe_id("partido", partido_id))
        return;

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE equipo SET partido_id = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar consulta: %s\n", sqlite3_errmsg(db));
        return;
    }
    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        printf("Equipo asignado al partido exitosamente.\n");
    }
    else
    {
        printf("Error al asignar equipo a partido: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Remueve la asignación de partido de un equipo
 *
 * Función helper que maneja la lógica de remover la asignación de partido,
 * reduciendo la complejidad de anidamiento en handle_modify_team_assignment().
 *
 * @param equipo_id ID del equipo a desasignar
 */
void remove_team_from_party(int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE equipo SET partido_id = -1 WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar consulta: %s\n", sqlite3_errmsg(db));
        return;
    }
    sqlite3_bind_int(stmt, 1, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        printf("Asignacion de partido removida exitosamente.\n");
    }
    else
    {
        printf("Error al remover asignacion de partido: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

// Helper functions for reducing complexity in modificar_equipo
void show_available_teams_for_modification();
void handle_modify_team_name(int equipo_id);
void handle_modify_team_type(int equipo_id);
void handle_modify_team_assignment(int equipo_id);
void handle_modify_players(int equipo_id);

/**
 * @brief Obtiene el ID de un equipo para modificar desde entrada del usuario
 * @return ID del equipo seleccionado o 0 si se cancela
 */
int get_equipo_id_to_modify()
{
    int equipo_id = input_int("\nIngrese el ID del equipo a modificar (0 para cancelar): ");
    
    if (equipo_id == 0)
        return 0;
    
    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        pause_console();
        return 0;
    }
    
    return equipo_id;
}

// Helper functions for reducing complexity in modificar_jugador_existente
void handle_modify_player_name(int player_id);
void handle_modify_player_number(int player_id, const int *all_numbers, int count);
void handle_modify_player_position(int player_id);
void handle_toggle_player_captain(int player_id);

/**
 * @brief Muestra la lista de equipos disponibles para modificación
 *
 * Función helper que extrae la lógica de mostrar equipos disponibles,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 */
void show_available_teams_for_modification()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== EQUIPOS DISPONIBLES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            printf("%d. %s\n", id, nombre);
        }

        if (!found)
        {
            printf("No hay equipos registrados para modificar.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Maneja la modificación del nombre del equipo
 *
 * Función helper que extrae la lógica de modificación del nombre del equipo,
 * reduciendo la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo a modificar
 */
void handle_modify_team_name(int equipo_id)
{
    char nuevo_nombre[50];
    input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE equipo SET nombre = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, equipo_id);

    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        printf("Nombre actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Maneja la modificación del tipo de fútbol del equipo
 *
 * Función helper que extrae la lógica de modificación del tipo de fútbol,
 * reduciendo la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo a modificar
 */
void handle_modify_team_type(int equipo_id)
{
    printf("\nSeleccione el nuevo tipo de futbol:\n");
    printf("1. Futbol 5\n");
    printf("2. Futbol 7\n");
    printf("3. Futbol 8\n");
    printf("4. Futbol 11\n");

    int nuevo_tipo = input_int(">");
    TipoFutbol tipo_futbol;
    switch (nuevo_tipo)
    {
    case 1:
        tipo_futbol = FUTBOL_5;
        break;
    case 2:
        tipo_futbol = FUTBOL_7;
        break;
    case 3:
        tipo_futbol = FUTBOL_8;
        break;
    case 4:
        tipo_futbol = FUTBOL_11;
        break;
    default:
        printf("Opcion invalida.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE equipo SET tipo_futbol = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, tipo_futbol);
        sqlite3_bind_int(stmt, 2, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Tipo de futbol actualizado exitosamente.\n");
        }
        else
        {
            printf("Error al actualizar el tipo de futbol: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Maneja la modificación de la asignación a partido del equipo
 *
 * Función helper que extrae la lógica de modificación de asignación a partido,
 * reduciendo la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo a modificar
 */
void handle_modify_team_assignment(int equipo_id)
{
    if (confirmar("¿Desea asignar este equipo a un partido?"))
    {
        assign_team_to_party(equipo_id);
    }
    else
    {
        remove_team_from_party(equipo_id);
    }
}

/**
 * @brief Maneja el menú de modificación de jugadores del equipo
 *
 * Función helper que extrae la lógica completa de modificación de jugadores,
 * reduciendo significativamente la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo cuyos jugadores se van a modificar
 */
void handle_modify_players(int equipo_id)
{
    printf("\n=== MODIFICAR JUGADORES ===\n");

    // Obtener jugadores actuales del equipo
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT id, nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero;";

    if (sqlite3_prepare_v2(db, sql_jugadores, -1, &stmt_jugadores, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

        printf("\nJugadores actuales:\n");
        int jugador_count = 0;
        int jugadores_ids[11]; // Máximo para fútbol 11
        char jugadores_nombres[11][50];
        int jugadores_numeros[11];
        int jugadores_posiciones[11];
        int jugadores_capitanes[11];

        while (sqlite3_step(stmt_jugadores) == SQLITE_ROW)
        {
            jugadores_ids[jugador_count] = sqlite3_column_int(stmt_jugadores, 0);
            snprintf(jugadores_nombres[jugador_count], sizeof(jugadores_nombres[jugador_count]), "%s", (const char*)sqlite3_column_text(stmt_jugadores, 1));
            jugadores_numeros[jugador_count] = sqlite3_column_int(stmt_jugadores, 2);
            jugadores_posiciones[jugador_count] = sqlite3_column_int(stmt_jugadores, 3);
            jugadores_capitanes[jugador_count] = sqlite3_column_int(stmt_jugadores, 4);

            printf("%d. %s (Numero: %d, Posicion: %s)%s\n",
                   jugadores_numeros[jugador_count],
                   jugadores_nombres[jugador_count],
                   jugadores_numeros[jugador_count],
                   get_nombre_posicion(jugadores_posiciones[jugador_count]),
                   jugadores_capitanes[jugador_count] ? " [CAPITAN]" : "");

            jugador_count++;
        }
        sqlite3_finalize(stmt_jugadores);

        if (jugador_count == 0)
        {
            printf("No hay jugadores registrados para este equipo.\n");
            pause_console();
            return;
        }

        // Mostrar opciones de modificación
        printf("\nSeleccione que desea hacer:\n");
        printf("1. Modificar un jugador existente\n");
        printf("2. Agregar un nuevo jugador\n");
        printf("3. Eliminar un jugador\n");
        printf("4. Cambiar capitán\n");
        printf("5. Volver\n");

        int opcion_jugador = input_int(">");

        switch (opcion_jugador)
        {
        case 1:
            modificar_jugador_existente(jugadores_ids, jugadores_nombres,
                                        jugadores_numeros, jugadores_posiciones, jugadores_capitanes, jugador_count);
            break;
        case 2:
            agregar_nuevo_jugador(equipo_id, jugador_count, jugadores_numeros);
            break;
        case 3:
            eliminar_jugador_existente(jugadores_ids, jugadores_numeros, jugador_count);
            break;
        case 4:
        {
            EquipoPlayerInfo info =
            {
                .jugador_count = jugador_count
            };
            memcpy(info.jugadores_ids, jugadores_ids, sizeof(int) * jugador_count);
            memcpy(info.jugadores_nombres, jugadores_nombres, sizeof(jugadores_nombres));
            memcpy(info.jugadores_numeros, jugadores_numeros, sizeof(int) * jugador_count);
            memcpy(info.jugadores_posiciones, jugadores_posiciones, sizeof(int) * jugador_count);
            memcpy(info.jugadores_capitanes, jugadores_capitanes, sizeof(int) * jugador_count);

            cambiar_capitan_equipo(equipo_id, &info);
            break;
        }
        case 5:
            break;
        default:
            printf("Opción inválida.\n");
            pause_console();
        }
    }
}

/**
 * @brief Maneja la modificación del nombre de un jugador
 *
 * Función helper que extrae la lógica de modificación del nombre,
 * reduciendo la complejidad del switch en modificar_jugador_existente().
 *
 * @param player_id ID del jugador a modificar
 */
void handle_modify_player_name(int player_id)
{
    char nuevo_nombre[50];
    input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

    if (update_player_name(player_id, nuevo_nombre))
    {
        printf("Nombre del jugador actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Maneja la modificación del número de un jugador
 *
 * Función helper que extrae la lógica de modificación del número,
 * incluyendo validación de duplicados, reduciendo la complejidad del switch.
 *
 * @param player_id ID del jugador a modificar
 * @param all_numbers Array con todos los números de jugadores del equipo
 * @param count Número total de jugadores en el equipo
 */
void handle_modify_player_number(int player_id, const int *all_numbers, int count)
{
    int nuevo_numero = input_int("Ingrese el nuevo número: ");

    // Verificar si el número ya existe
    int numero_existe = 0;
    for (int i = 0; i < count; i++)
    {
        if (all_numbers[i] == nuevo_numero)
        {
            numero_existe = 1;
            break;
        }
    }

    if (numero_existe)
    {
        printf("El número ya está en uso por otro jugador.\n");
        pause_console();
        return;
    }

    if (update_player_number(player_id, nuevo_numero))
    {
        printf("Número del jugador actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el número: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Maneja la modificación de la posición de un jugador
 *
 * Función helper que extrae la lógica de selección y actualización de posición,
 * reduciendo la complejidad del switch en modificar_jugador_existente().
 *
 * @param player_id ID del jugador a modificar
 */
void handle_modify_player_position(int player_id)
{
    printf("Seleccione la nueva posición:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int nueva_posicion = input_int(">");
    Posicion posicion;
    switch (nueva_posicion)
    {
    case 1:
        posicion = ARQUERO;
        break;
    case 2:
        posicion = DEFENSOR;
        break;
    case 3:
        posicion = MEDIOCAMPISTA;
        break;
    case 4:
        posicion = DELANTERO;
        break;
    default:
        printf("Posición inválida.\n");
        pause_console();
        return;
    }

    if (update_player_position(player_id, posicion))
    {
        printf("Posición del jugador actualizada exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar la posición: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Obtiene el estado actual de capitán de un jugador
 *
 * Función helper que consulta la base de datos para obtener el estado
 * de capitán actual de un jugador específico.
 *
 * @param player_id ID del jugador
 * @return 1 si es capitán, 0 si no lo es, -1 en caso de error
 */
int get_player_captain_status(int player_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT es_capitan FROM jugador WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, player_id);

    int status = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        status = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return status;
}

/**
 * @brief Maneja el cambio del estado de capitán de un jugador
 *
 * Función helper que extrae la lógica de toggle del estado de capitán,
 * reduciendo la complejidad del switch en modificar_jugador_existente().
 *
 * @param player_id ID del jugador a modificar
 */
void handle_toggle_player_captain(int player_id)
{
    // Cambiar estado de capitán
    int current_status = get_player_captain_status(player_id);
    if (current_status == -1)
    {
        printf("Error al obtener el estado actual de capitán: %s\n", sqlite3_errmsg(db));
        return;
    }

    int nuevo_capitan = !current_status;

    if (update_player_captain_status(player_id, nuevo_capitan))
    {
        printf("Estado de capitán actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el estado de capitán: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Inserta un registro de equipo en la base de datos
 *
 * Función helper que maneja la inserción del registro del equipo,
 * retornando el ID del equipo insertado o -1 en caso de error.
 *
 * @param equipo Puntero al equipo a insertar
 * @return ID del equipo insertado, o -1 si hay error
 */
int insert_equipo_record(const Equipo *equipo)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO equipo (nombre, tipo, tipo_futbol, num_jugadores, partido_id) VALUES (?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, equipo->nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, equipo->tipo);
        sqlite3_bind_int(stmt, 3, equipo->tipo_futbol);
        sqlite3_bind_int(stmt, 4, equipo->num_jugadores);
        sqlite3_bind_int(stmt, 5, equipo->partido_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            int equipo_id = (int)sqlite3_last_insert_rowid(db);
            sqlite3_finalize(stmt);
            return equipo_id;
        }
        else
        {
            printf("Error al guardar el equipo: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }
    }
    else
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return -1;
    }
}

/**
 * @brief Inserta todos los jugadores de un equipo en la base de datos
 *
 * Función helper que maneja la inserción de todos los jugadores
 * asociados a un equipo específico.
 *
 * @param equipo_id ID del equipo al que pertenecen los jugadores
 * @param equipo Puntero al equipo que contiene los jugadores
 */
void insert_jugadores_for_equipo(int equipo_id, const Equipo *equipo)
{
    sqlite3_stmt *stmt_jugador;
    const char *sql_jugador = "INSERT INTO jugador (equipo_id, nombre, numero, posicion, es_capitan) VALUES (?, ?, ?, ?, ?);";

    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        Jugador const *jugador = &equipo->jugadores[i];

        if (sqlite3_prepare_v2(db, sql_jugador, -1, &stmt_jugador, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt_jugador, 1, equipo_id);
            sqlite3_bind_text(stmt_jugador, 2, jugador->nombre, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt_jugador, 3, jugador->numero);
            sqlite3_bind_int(stmt_jugador, 4, jugador->posicion);
            sqlite3_bind_int(stmt_jugador, 5, jugador->es_capitan);

            sqlite3_step(stmt_jugador);
            sqlite3_finalize(stmt_jugador);
        }
    }
}

/**
 * @brief Maneja la asignación opcional de un equipo a un partido
 *
 * Función helper que pregunta al usuario si desea asignar el equipo
 * a un partido existente y maneja la lógica correspondiente.
 *
 * @param equipo_id ID del equipo a asignar
 */
void handle_party_assignment(int equipo_id)
{
    if (!confirmar("Desea asignar este equipo a un partido existente?"))
        return;

    listar_partidos();
    int partido_id = input_int("Ingrese el ID del partido (0 para cancelar): ");
    if (partido_id <= 0 || !existe_id("partido", partido_id))
        return;

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE equipo SET partido_id = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) != SQLITE_OK)
        return;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_int(stmt, 2, equipo_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error al asignar equipo a partido: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        printf("Equipo asignado al partido %d exitosamente.\n", partido_id);
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Genera cancha de fútbol animada con balón en movimiento
 */
void mostrar_cancha_animada(int minuto, int evento_tipo)
{
    // Posiciones del balón basadas en el minuto y tipo de evento
    char *posiciones[12] =
    {
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n         O|  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n          ============                \n          |  CENTRO  |O                \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |O AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL O|               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n         O============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |O CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO O|                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          =========O==                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n        O+-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+O              \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |O           \n         +-------------+               "
    };

    printf("=======================================\n");
    printf("           CANCHA DE FUTBOL            \n");
    printf("=======================================\n");

    // Mostrar la posición del balón basada en el minuto y tipo de evento
    int posicion_index = (minuto + evento_tipo) % 12;
    printf("%s\n", posiciones[posicion_index]);

    printf("=======================================\n");
}

/**
 * @brief Convierte una posición enumerada a su nombre textual
 *
 * Proporciona representación legible de posiciones para interfaz de usuario,
 * facilitando la comprensión y selección de roles de jugadores.
 *
 * @param posicion El valor enumerado de la posición
 * @return Cadena constante con el nombre de la posición, o "Desconocido" si no es válida
 */
const char* get_nombre_posicion(Posicion posicion)
{
    switch (posicion)
    {
    case ARQUERO:
        return "Arquero";
    case DEFENSOR:
        return "Defensor";
    case MEDIOCAMPISTA:
        return "Mediocampista";
    case DELANTERO:
        return "Delantero";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Convierte un tipo de fútbol enumerado a su nombre textual
 *
 * Facilita la presentación de modalidades deportivas en interfaz de usuario,
 * permitiendo selección y visualización clara de formatos de juego disponibles.
 *
 * @param tipo El valor enumerado del tipo de fútbol
 * @return Cadena constante con el nombre del tipo de fútbol, o "Desconocido" si no es válido
 */
const char* get_nombre_tipo_futbol(TipoFutbol tipo)
{
    switch (tipo)
    {
    case FUTBOL_5:
        return "Futbol 5";
    case FUTBOL_7:
        return "Futbol 7";
    case FUTBOL_8:
        return "Futbol 8";
    case FUTBOL_11:
        return "Futbol 11";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Solicita y valida la entrada de datos para un jugador
 *
 * Esta función helper centraliza la lógica de entrada de datos para jugadores,
 * reduciendo la complejidad cognitiva en funciones más grandes al extraer
 * la lógica repetitiva de validación de nombre y selección de posición.
 *
 * @param jugador Puntero al jugador que se va a modificar
 * @param numero_auto Si es verdadero, asigna números automáticamente
 */
void input_jugador_data(Jugador *jugador, int numero_auto)
{
    // Nombre del jugador
    char nombre_temp[50];
    do
    {
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        if (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0)
        {
            printf("El nombre no puede estar vacio. Intente nuevamente.\n");
        }
    }
    while (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0);
    snprintf(jugador->nombre, sizeof(jugador->nombre), "%s", nombre_temp);

    // Número del jugador
    if (numero_auto)
    {
        // Auto-asignar números secuenciales (se asignará después)
        jugador->numero = 0;
    }
    else
    {
        jugador->numero = input_int("Numero: ");
    }

    // Posición del jugador
    jugador->posicion = select_posicion();
    jugador->es_capitan = 0;
}

/**
 * @brief Muestra menú de selección de posición y retorna la posición elegida
 *
 * Función helper que extrae la lógica de selección de posición, reduciendo
 * duplicación de código y mejorando la mantenibilidad.
 *
 * @return La posición seleccionada por el usuario
 */
Posicion select_posicion()
{
    printf("Posicion:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int opcion_posicion = input_int(">");
    switch (opcion_posicion)
    {
    case 1:
        return ARQUERO;
    case 2:
        return DEFENSOR;
    case 3:
        return MEDIOCAMPISTA;
    case 4:
        return DELANTERO;
    default:
        printf("Posicion invalida. Se asignara como Delantero.\n");
        return DELANTERO;
    }
}

/**
 * @brief Muestra lista de jugadores y permite seleccionar capitán
 *
 * Función helper que centraliza la lógica de selección de capitán,
 * reduciendo complejidad en funciones que crean equipos.
 *
 * @param equipo Puntero al equipo
 * @return Índice del capitán seleccionado, o -1 si no se selecciona
 */
int select_capitan(Equipo *equipo)
{
    printf("\nSeleccione el capitan del equipo (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo->jugadores[i].nombre);
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < equipo->num_jugadores)
    {
        equipo->jugadores[capitan_idx].es_capitan = 1;
        return capitan_idx;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
        return -1;
    }
}

/**
 * @brief Guarda un equipo completo en la base de datos
 *
 * Función helper que encapsula toda la lógica de guardado de equipo
 * y jugadores en la base de datos, reduciendo complejidad en crear_equipo_fijo.
 * Utiliza funciones helper para mantener bajo nivel de complejidad cognitiva.
 *
 * @param equipo Puntero al equipo a guardar
 */
void save_equipo_to_db(const Equipo *equipo)
{
    int equipo_id = insert_equipo_record(equipo);

    if (equipo_id != -1)
    {
        insert_jugadores_for_equipo(equipo_id, equipo);
        printf("Equipo guardado exitosamente con ID: %d\n", equipo_id);
        handle_party_assignment(equipo_id);
    }
}

/**
 * @brief Inicializa los datos básicos de un equipo
 *
 * Función helper que configura nombre, tipo de fútbol y número de jugadores,
 * reduciendo duplicación de código entre diferentes funciones de creación de equipos.
 *
 * @param equipo Puntero al equipo a inicializar
 * @param tipo_futbol Tipo de fútbol seleccionado
 * @param num_jugadores Número de jugadores calculado
 */
void input_equipo_basico(Equipo *equipo, TipoFutbol tipo_futbol, int num_jugadores)
{
    equipo->tipo_futbol = tipo_futbol;
    equipo->num_jugadores = num_jugadores;

    // Solicitar nombre del equipo
    input_string("Ingrese el nombre del equipo: ", equipo->nombre, sizeof(equipo->nombre));
}

/**
 * @brief Crea los jugadores para un equipo solicitando datos al usuario
 *
 * Función helper que maneja la creación de todos los jugadores de un equipo,
 * reduciendo la complejidad de anidamiento en funciones de creación de equipos.
 *
 * @param equipo Puntero al equipo cuyos jugadores se van a crear
 * @param auto_numero Si es verdadero, asigna números automáticamente secuenciales
 * @param prefix Prefijo para los mensajes de jugador (ej. "EQUIPO LOCAL - ")
 */
void crear_jugadores_equipo(Equipo *equipo, int auto_numero, const char *prefix)
{
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        clear_screen();
        printf("\n%sJugador %d de %d:\n", prefix, i + 1, equipo->num_jugadores);
        input_jugador_data(&equipo->jugadores[i], auto_numero);

        if (auto_numero)
        {
            equipo->jugadores[i].numero = i + 1;
        }
    }
}

/**
 * @brief Muestra menú para seleccionar tipo de fútbol y retorna la selección
 *
 * Función helper que extrae la lógica de selección de tipo de fútbol,
 * reduciendo duplicación de código y mejorando mantenibilidad.
 *
 * @return El tipo de fútbol seleccionado, o -1 si el usuario cancela
 */
TipoFutbol seleccionar_tipo_futbol()
{
    printf("\nSeleccione el tipo de futbol:\n");
    printf("1. Futbol 5\n");
    printf("2. Futbol 7\n");
    printf("3. Futbol 8\n");
    printf("4. Futbol 11\n");
    printf("5. Volver\n");

    int opcion_tipo = input_int(">");
    switch (opcion_tipo)
    {
    case 1:
        return FUTBOL_5;
    case 2:
        return FUTBOL_7;
    case 3:
        return FUTBOL_8;
    case 4:
        return FUTBOL_11;
    case 5:
        return -1; // Cancelar
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return -1;
    }
}

/**
 * @brief Retorna el número de jugadores correspondiente a un tipo de fútbol
 *
 * Función helper que encapsula la lógica de conversión de tipo de fútbol
 * a número de jugadores, centralizando esta regla de negocio.
 *
 * @param tipo_futbol El tipo de fútbol
 * @return Número de jugadores para ese tipo de fútbol
 */
int get_num_jugadores_por_tipo(TipoFutbol tipo_futbol)
{
    switch (tipo_futbol)
    {
    case FUTBOL_5:
        return 5;
    case FUTBOL_7:
        return 7;
    case FUTBOL_8:
        return 8;
    case FUTBOL_11:
        return 11;
    default:
        return 5; // Default a fútbol 5
    }
}

/**
 * @brief Modifica un jugador existente en la base de datos
 *
 * Función helper que maneja la modificación de campos específicos de un jugador,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param jugadores_ids Array con los IDs de los jugadores
 * @param jugadores_nombres Array con los nombres de los jugadores
 * @param jugadores_numeros Array con los números de los jugadores
 * @param jugadores_capitanes Array con el estado de capitán de los jugadores
 * @param jugador_count Número total de jugadores
 */
void modificar_jugador_existente(const int *jugadores_ids, char jugadores_nombres[][50],
                                 const int *jugadores_numeros, const int *jugadores_posiciones, const int *jugadores_capitanes, int jugador_count)
{
    int jugador_num = input_int("Ingrese el número del jugador a modificar: ");

    // Buscar el jugador
    int jugador_idx = -1;
    for (int i = 0; i < jugador_count; i++)
    {
        if (jugadores_numeros[i] == jugador_num)
        {
            jugador_idx = i;
            break;
        }
    }

    if (jugador_idx == -1)
    {
        printf("Número de jugador no encontrado.\n");
        pause_console();
        return;
    }

    // Mostrar información actual
    printf("\nModificando jugador: %s\n", jugadores_nombres[jugador_idx]);
    printf("1. Nombre: %s\n", jugadores_nombres[jugador_idx]);
    printf("2. Número: %d\n", jugadores_numeros[jugador_idx]);
    printf("3. Posición: %s\n", get_nombre_posicion(jugadores_posiciones[jugador_idx]));
    printf("4. Capitán: %s\n", jugadores_capitanes[jugador_idx] ? "Sí" : "No");
    printf("5. Volver\n");

    int campo_modificar = input_int("Seleccione el campo a modificar: ");

    switch (campo_modificar)
    {
    case 1:
        handle_modify_player_name(jugadores_ids[jugador_idx]);
        break;
    case 2:
        handle_modify_player_number(jugadores_ids[jugador_idx], jugadores_numeros, jugador_count);
        break;
    case 3:
        handle_modify_player_position(jugadores_ids[jugador_idx]);
        break;
    case 4:
        handle_toggle_player_captain(jugadores_ids[jugador_idx]);
        break;
    case 5:
        break;
    default:
        printf("Opción inválida.\n");
    }

    pause_console();
}

/**
 * @brief Agrega un nuevo jugador a un equipo en la base de datos
 *
 * Función helper que maneja la lógica de agregar un jugador,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param equipo_id ID del equipo
 * @param jugador_count Número actual de jugadores
 * @param jugadores_numeros Array con los números de los jugadores existentes
 */
void agregar_nuevo_jugador(int equipo_id, int jugador_count, const int *jugadores_numeros)
{
    if (jugador_count >= 11)
    {
        printf("El equipo ya tiene el máximo de jugadores (11).\n");
        pause_console();
        return;
    }

    Jugador nuevo_jugador;

    // Nombre del jugador
    char nombre_temp[50];
    do
    {
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        if (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0)
        {
            printf("El nombre no puede estar vacio. Intente nuevamente.\n");
        }
    }
    while (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0);
    snprintf(nuevo_jugador.nombre, sizeof(nuevo_jugador.nombre), "%s", nombre_temp);

    // Número del jugador
    int numero_valido = 0;
    while (!numero_valido)
    {
        nuevo_jugador.numero = input_int("Numero: ");

        // Verificar si el número ya existe
        int numero_existe = 0;
        for (int i = 0; i < jugador_count; i++)
        {
            if (jugadores_numeros[i] == nuevo_jugador.numero)
            {
                numero_existe = 1;
                break;
            }
        }

        if (numero_existe)
        {
            printf("El número ya está en uso. Por favor, elija otro número.\n");
        }
        else
        {
            numero_valido = 1;
        }
    }

    // Posición del jugador
    printf("Posicion:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int opcion_posicion = input_int(">");
    switch (opcion_posicion)
    {
    case 1:
        nuevo_jugador.posicion = ARQUERO;
        break;
    case 2:
        nuevo_jugador.posicion = DEFENSOR;
        break;
    case 3:
        nuevo_jugador.posicion = MEDIOCAMPISTA;
        break;
    case 4:
        nuevo_jugador.posicion = DELANTERO;
        break;
    default:
        printf("Posición inválida. Se asignará como Delantero.\n");
        nuevo_jugador.posicion = DELANTERO;
    }

    nuevo_jugador.es_capitan = 0;

    // Insertar nuevo jugador
    sqlite3_stmt *stmt;
    const char *sql_insert = "INSERT INTO jugador (equipo_id, nombre, numero, posicion, es_capitan) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        sqlite3_bind_text(stmt, 2, nuevo_jugador.nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, nuevo_jugador.numero);
        sqlite3_bind_int(stmt, 4, nuevo_jugador.posicion);
        sqlite3_bind_int(stmt, 5, nuevo_jugador.es_capitan);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Jugador agregado exitosamente.\n");
        }
        else
        {
            printf("Error al agregar el jugador: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Agrega un jugador nuevo a un equipo momentáneo (si hay espacio)
 */
void agregar_jugador_momentaneo(Equipo *equipo)
{
    int max_jugadores = 0;
    switch (equipo->tipo_futbol)
    {
    case FUTBOL_5:
        max_jugadores = 5;
        break;
    case FUTBOL_7:
        max_jugadores = 7;
        break;
    case FUTBOL_8:
        max_jugadores = 8;
        break;
    case FUTBOL_11:
        max_jugadores = 11;
        break;
    }

    if (equipo->num_jugadores >= max_jugadores)
    {
        printf("El equipo ya tiene el maximo de jugadores (%d).\n", max_jugadores);
        pause_console();
        return;
    }

    Jugador *nuevo_jugador = &equipo->jugadores[equipo->num_jugadores];

    // Nombre del jugador
    char nombre_temp[50];
    do
    {
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        if (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0)
        {
            printf("El nombre no puede estar vacio. Intente nuevamente.\n");
        }
    }
    while (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0);
    snprintf(nuevo_jugador->nombre, sizeof(nuevo_jugador->nombre), "%s", nombre_temp);

    // Número del jugador
    int numero_valido = 0;
    while (!numero_valido)
    {
        nuevo_jugador->numero = input_int("Numero: ");

        // Verificar si el número ya existe
        int numero_existe = 0;
        for (int i = 0; i < equipo->num_jugadores; i++)
        {
            if (equipo->jugadores[i].numero == nuevo_jugador->numero)
            {
                numero_existe = 1;
                break;
            }
        }

        if (numero_existe)
        {
            printf("El numero ya esta en uso. Por favor, elija otro numero.\n");
        }
        else
        {
            numero_valido = 1;
        }
    }

    // Posición del jugador
    printf("Posicion:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int opcion_posicion = input_int(">");
    switch (opcion_posicion)
    {
    case 1:
        nuevo_jugador->posicion = ARQUERO;
        break;
    case 2:
        nuevo_jugador->posicion = DEFENSOR;
        break;
    case 3:
        nuevo_jugador->posicion = MEDIOCAMPISTA;
        break;
    case 4:
        nuevo_jugador->posicion = DELANTERO;
        break;
    default:
        printf("Posición inválida. Se asignará como Delantero.\n");
        nuevo_jugador->posicion = DELANTERO;
    }

    nuevo_jugador->es_capitan = 0;
    equipo->num_jugadores++;

    printf("Jugador agregado exitosamente.\n");
    pause_console();
}

/**
 * @brief Elimina un jugador existente de la base de datos
 *
 * Función helper que maneja la eliminación de un jugador,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param jugadores_ids Array con los IDs de los jugadores
 * @param jugadores_numeros Array con los números de los jugadores
 * @param jugador_count Número total de jugadores
 */
void eliminar_jugador_existente(const int *jugadores_ids, const int *jugadores_numeros, int jugador_count)
{
    int jugador_num = input_int("Ingrese el número del jugador a eliminar: ");

    // Buscar el jugador
    int jugador_idx = -1;
    for (int i = 0; i < jugador_count; i++)
    {
        if (jugadores_numeros[i] == jugador_num)
        {
            jugador_idx = i;
            break;
        }
    }

    if (jugador_idx == -1)
    {
        printf("Número de jugador no encontrado.\n");
        pause_console();
        return;
    }

    if (confirmar("¿Está seguro que desea eliminar este jugador?"))
    {
        sqlite3_stmt *stmt;
        const char *sql_delete = "DELETE FROM jugador WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, jugadores_ids[jugador_idx]);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Jugador eliminado exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar el jugador: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }

    pause_console();
}

/**
 * @brief Muestra los jugadores de un equipo específico
 *
 * Función helper que extrae la lógica de mostrar jugadores de un equipo,
 * reduciendo la complejidad de anidamiento en listar_equipos().
 *
 * @param equipo_id ID del equipo cuyos jugadores se van a mostrar
 */
void mostrar_jugadores_equipo(int equipo_id)
{
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero;";

    if (sqlite3_prepare_v2(db, sql_jugadores, -1, &stmt_jugadores, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

        int has_jugadores = 0;
        while (sqlite3_step(stmt_jugadores) == SQLITE_ROW)
        {
            has_jugadores = 1;
            const char *jugador_nombre = (const char*)sqlite3_column_text(stmt_jugadores, 0);
            int jugador_numero = sqlite3_column_int(stmt_jugadores, 1);
            int jugador_posicion = sqlite3_column_int(stmt_jugadores, 2);
            int es_capitan = sqlite3_column_int(stmt_jugadores, 3);

            printf("%d. %s (Numero: %d, Posicion: %s)%s\n",
                   jugador_numero,
                   jugador_nombre,
                   jugador_numero,
                   get_nombre_posicion(jugador_posicion),
                   es_capitan ? " [CAPITAN]" : "");
        }

        if (!has_jugadores)
        {
            printf("No hay jugadores registrados para este equipo.\n");
        }

        sqlite3_finalize(stmt_jugadores);
    }
    else
    {
        printf("Error al obtener jugadores: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Muestra por pantalla toda la información detallada de un equipo
 *
 * Esta función presenta en consola la información completa de un equipo,
 * incluyendo sus datos básicos, tipo de fútbol, número de jugadores y
 * la lista completa de jugadores con sus posiciones y roles.
 *
 * @param equipo Puntero al equipo cuya información se va a mostrar
 */
void mostrar_equipo(const Equipo *equipo)
{
    printf("\n=== INFORMACION DEL EQUIPO ===\n");
    printf("Nombre: %s\n", equipo->nombre);
    printf("Tipo: %s\n", equipo->tipo == FIJO ? "Fijo" : "Momentaneo");
    printf("Tipo de Futbol: %s\n", get_nombre_tipo_futbol(equipo->tipo_futbol));
    printf("Numero de Jugadores: %d\n", equipo->num_jugadores);
    printf("Asignado a Partido: %s\n", equipo->partido_id == -1 ? "No" : "Si");

    printf("\n=== JUGADORES ===\n");
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        const Jugador *jugador = &equipo->jugadores[i];
        printf("%d. %s (Numero: %d, Posicion: %s)%s\n",
               i + 1,
               jugador->nombre,
               jugador->numero,
               get_nombre_posicion(jugador->posicion),
               jugador->es_capitan ? " [CAPITAN]" : "");
    }
    printf("\n");
}

/**
 * @brief Crea un nuevo equipo fijo que se guarda permanentemente en la base de datos
 *
 * Esta función guía al usuario a través del proceso completo de creación de un equipo
 * permanente. Solicita el nombre, tipo de fútbol, información de cada jugador y
 * selección de capitán. El equipo y sus jugadores se guardan en la base de datos
 * y opcionalmente se puede asignar a un partido existente.
 */
void crear_equipo_fijo()
{
    Equipo equipo;
    equipo.tipo = FIJO;
    equipo.partido_id = -1;

    // Determinar tipo de fútbol y número de jugadores
    TipoFutbol tipo_futbol = seleccionar_tipo_futbol();
    if (tipo_futbol == -1) return; // Usuario canceló

    int num_jugadores = get_num_jugadores_por_tipo(tipo_futbol);
    input_equipo_basico(&equipo, tipo_futbol, num_jugadores);

    // Solicitar información de cada jugador
    crear_jugadores_equipo(&equipo, 0, "");

    // Seleccionar capitán
    select_capitan(&equipo);

    // Mostrar equipo creado y guardar
    clear_screen();
    mostrar_equipo(&equipo);
    save_equipo_to_db(&equipo);
}

/**
 * @brief Crea un nuevo equipo momentáneo que no se guarda en la base de datos
 *
 * Esta función guía al usuario a través del proceso de creación de un equipo
 * temporal. Solicita el nombre, tipo de fútbol, información de cada jugador y
 * selección de capitán. El equipo se crea en memoria pero no se persiste,
 * siendo útil para partidos puntuales o simulaciones.
 */
void crear_equipo_momentaneo()
{
    clear_screen();
    print_header("CREAR EQUIPO MOMENTANEO");

    // Preguntar si quiere crear 1 o 2 equipos
    printf("Seleccione cuantos equipos momentaneos desea crear:\n");
    printf("1. Un solo equipo\n");
    printf("2. Dos equipos (Local y Visitante)\n");
    printf("3. Volver\n");

    int opcion_equipos = input_int(">");

    switch (opcion_equipos)
    {
    case 1:
        crear_un_equipo_momentaneo();
        break;
    case 2:
        crear_dos_equipos_momentaneos();
        break;
    case 3:
        return;
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return;
    }
}

/**
 * @brief Crea un solo equipo momentáneo
 */
void crear_un_equipo_momentaneo()
{
    Equipo equipo;
    equipo.tipo = MOMENTANEO;
    equipo.partido_id = -1;

    // Determinar tipo de fútbol y número de jugadores
    TipoFutbol tipo_futbol = seleccionar_tipo_futbol();
    if (tipo_futbol == -1) return; // Usuario canceló

    int num_jugadores = get_num_jugadores_por_tipo(tipo_futbol);
    input_equipo_basico(&equipo, tipo_futbol, num_jugadores);

    // Solicitar información de cada jugador con números auto-asignados
    crear_jugadores_equipo(&equipo, 1, "");

    // Seleccionar capitán
    select_capitan(&equipo);

    // Mostrar equipo creado y ofrecer opciones de gestión
    gestionar_equipo_momentaneo(&equipo);
}

/**
 * @brief Muestra el equipo y ofrece opciones de gestión de jugadores
 */
void gestionar_equipo_momentaneo(Equipo *equipo)
{
    int salir = 0;

    while (!salir)
    {
        clear_screen();
        mostrar_equipo(equipo);

        printf("Opciones de gestion:\n");
        printf("1. Modificar un jugador\n");
        printf("2. Agregar un jugador nuevo (si hay espacio)\n");
        printf("3. Eliminar un jugador\n");
        printf("4. Cambiar capitán\n");
        printf("5. Finalizar\n");

        int opcion = input_int("Seleccione una opcion: ");

        switch (opcion)
        {
        case 1:
            modificar_jugador_momentaneo(equipo);
            break;
        case 2:
            agregar_jugador_momentaneo(equipo);
            break;
        case 3:
            eliminar_jugador_momentaneo(equipo);
            break;
        case 4:
            cambiar_capitan_momentaneo(equipo);
            break;
        case 5:
            salir = 1;
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }

    printf("Este equipo es momentaneo y no se guardara.\n");
    pause_console();
}

/**
 * @brief Modifica un jugador existente en un equipo momentáneo
 */
void modificar_jugador_momentaneo(Equipo *equipo)
{
    printf("\nSeleccione el jugador a modificar (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo->jugadores[i].nombre);
    }

    int jugador_idx = input_int(">") - 1;
    if (jugador_idx < 0 || jugador_idx >= equipo->num_jugadores)
    {
        printf("Seleccion invalida.\n");
        pause_console();
        return;
    }

    Jugador *jugador = &equipo->jugadores[jugador_idx];

    printf("\nModificando jugador: %s\n", jugador->nombre);
    printf("1. Nombre: %s\n", jugador->nombre);
    printf("2. Numero: %d\n", jugador->numero);
    printf("3. Posicion: %s\n", get_nombre_posicion(jugador->posicion));
    printf("4. Volver\n");

    int campo = input_int("Seleccione el campo a modificar: ");

    switch (campo)
    {
    case 1:
    {
        char nuevo_nombre[50];
        input_string("Nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));
        snprintf(jugador->nombre, sizeof(jugador->nombre), "%s", nuevo_nombre);
        printf("Nombre actualizado.\n");
        break;
    }
    case 2:
    {
        int nuevo_numero = input_int("Nuevo numero: ");

        // Verificar si el número ya existe
        int numero_existe = 0;
        for (int i = 0; i < equipo->num_jugadores; i++)
        {
            if (i != jugador_idx && equipo->jugadores[i].numero == nuevo_numero)
            {
                numero_existe = 1;
                break;
            }
        }

        if (numero_existe)
        {
            printf("El numero ya esta en uso.\n");
        }
        else
        {
            jugador->numero = nuevo_numero;
            printf("Numero actualizado.\n");
        }
        break;
    }
    case 3:
    {
        printf("Seleccione nueva posicion:\n");
        printf("1. Arquero\n");
        printf("2. Defensor\n");
        printf("3. Mediocampista\n");
        printf("4. Delantero\n");

        int opcion_posicion = input_int(">");
        switch (opcion_posicion)
        {
        case 1:
            jugador->posicion = ARQUERO;
            break;
        case 2:
            jugador->posicion = DEFENSOR;
            break;
        case 3:
            jugador->posicion = MEDIOCAMPISTA;
            break;
        case 4:
            jugador->posicion = DELANTERO;
            break;
        default:
            printf("Posicion invalida.\n");
        }
        printf("Posicion actualizada.\n");
        break;
    }
    case 4:
        return;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

/**
 * @brief Elimina un jugador de un equipo momentáneo
 */
void eliminar_jugador_momentaneo(Equipo *equipo)
{
    printf("\nSeleccione el jugador a eliminar (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo->jugadores[i].nombre);
    }

    int jugador_idx = input_int(">") - 1;
    if (jugador_idx < 0 || jugador_idx >= equipo->num_jugadores)
    {
        printf("Seleccion invalida.\n");
        pause_console();
        return;
    }

    if (confirmar("¿Esta seguro que desea eliminar este jugador?"))
    {
        // Si es el capitán, quitar la marca de capitán
        if (equipo->jugadores[jugador_idx].es_capitan)
        {
            equipo->jugadores[jugador_idx].es_capitan = 0;
        }

        // Mover los jugadores restantes para llenar el espacio
        for (int i = jugador_idx; i < equipo->num_jugadores - 1; i++)
        {
            equipo->jugadores[i] = equipo->jugadores[i + 1];
        }

        equipo->num_jugadores--;
        printf("Jugador eliminado exitosamente.\n");
    }

    pause_console();
}

/**
 * @brief Cambia el capitán de un equipo momentáneo
 */
void cambiar_capitan_momentaneo(Equipo *equipo)
{
    printf("\nSeleccione el nuevo capitán (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s %s\n", i + 1, equipo->jugadores[i].nombre,
               equipo->jugadores[i].es_capitan ? "[CAPITAN ACTUAL]" : "");
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx < 0 || capitan_idx >= equipo->num_jugadores)
    {
        printf("Seleccion invalida.\n");
        pause_console();
        return;
    }

    // Quitar capitán actual
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        equipo->jugadores[i].es_capitan = 0;
    }

    // Asignar nuevo capitán
    equipo->jugadores[capitan_idx].es_capitan = 1;
    printf("Capitan cambiado exitosamente.\n");

    pause_console();
}

/**
 * @brief Crea dos equipos momentáneos (Local y Visitante)
 */
void crear_dos_equipos_momentaneos()
{
    Equipo equipo_local;
    Equipo equipo_visitante;

    // Configurar equipos básicos
    equipo_local.tipo = MOMENTANEO;
    equipo_local.partido_id = -1;
    equipo_visitante.tipo = MOMENTANEO;
    equipo_visitante.partido_id = -1;

    // Solicitar tipo de fútbol (común para ambos equipos)
    printf("\nSeleccione el tipo de futbol:\n");
    printf("1. Futbol 5\n");
    printf("2. Futbol 7\n");
    printf("3. Futbol 8\n");
    printf("4. Futbol 11\n");
    printf("5. Volver\n");

    int opcion_tipo = input_int(">");
    TipoFutbol tipo_futbol;
    int num_jugadores = 0;

    switch (opcion_tipo)
    {
    case 1:
        tipo_futbol = FUTBOL_5;
        num_jugadores = 5;
        break;
    case 2:
        tipo_futbol = FUTBOL_7;
        num_jugadores = 7;
        break;
    case 3:
        tipo_futbol = FUTBOL_8;
        num_jugadores = 8;
        break;
    case 4:
        tipo_futbol = FUTBOL_11;
        num_jugadores = 11;
        break;
    case 5:
        return;
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return;
    }

    equipo_local.tipo_futbol = tipo_futbol;
    equipo_local.num_jugadores = num_jugadores;
    equipo_visitante.tipo_futbol = tipo_futbol;
    equipo_visitante.num_jugadores = num_jugadores;

    // Solicitar nombre del equipo local
    input_string("Ingrese el nombre del equipo LOCAL: ", equipo_local.nombre, sizeof(equipo_local.nombre));

    // Solicitar información de jugadores para equipo local
    crear_jugadores_equipo(&equipo_local, 1, "EQUIPO LOCAL - ");

    // Seleccionar capitán para equipo local
    printf("\nSeleccione el capitan del equipo LOCAL (1-%d):\n", num_jugadores);
    for (int i = 0; i < num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo_local.jugadores[i].nombre);
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < num_jugadores)
    {
        equipo_local.jugadores[capitan_idx].es_capitan = 1;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
    }

    // Solicitar nombre del equipo visitante
    input_string("Ingrese el nombre del equipo VISITANTE: ", equipo_visitante.nombre, sizeof(equipo_visitante.nombre));

    // Solicitar información de jugadores para equipo visitante
    crear_jugadores_equipo(&equipo_visitante, 1, "EQUIPO VISITANTE - ");

    // Seleccionar capitán para equipo visitante
    printf("\nSeleccione el capitan del equipo VISITANTE (1-%d):\n", num_jugadores);
    for (int i = 0; i < num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo_visitante.jugadores[i].nombre);
    }

    capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < num_jugadores)
    {
        equipo_visitante.jugadores[capitan_idx].es_capitan = 1;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
    }

    // Gestionar ambos equipos
    gestionar_dos_equipos_momentaneos(&equipo_local, &equipo_visitante);
}

/**
 * @brief Muestra ambos equipos y ofrece opciones de gestión de jugadores
 */
void gestionar_dos_equipos_momentaneos(Equipo *equipo_local, Equipo *equipo_visitante)
{
    int salir = 0;

    while (!salir)
    {
        clear_screen();
        printf("\n=== EQUIPO LOCAL ===\n");
        mostrar_equipo(equipo_local);
        printf("\n=== EQUIPO VISITANTE ===\n");
        mostrar_equipo(equipo_visitante);

        printf("Opciones de gestion:\n");
        printf("1. Gestionar equipo LOCAL\n");
        printf("2. Gestionar equipo VISITANTE\n");
        printf("3. Simular partido\n");
        printf("4. Finalizar\n");

        int opcion = input_int("Seleccione una opcion: ");

        switch (opcion)
        {
        case 1:
            gestionar_equipo_individual(equipo_local, "LOCAL");
            break;
        case 2:
            gestionar_equipo_individual(equipo_visitante, "VISITANTE");
            break;
        case 3:
            simular_partido(equipo_local, equipo_visitante);
            break;
        case 4:
            salir = 1;
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }

    printf("Estos equipos son momentaneos y no se guardaran.\n");
    pause_console();
}

/**
 * @brief Gestiona un equipo individual dentro del contexto de dos equipos
 */
void gestionar_equipo_individual(Equipo *equipo, const char *tipo_equipo)
{
    int salir = 0;

    while (!salir)
    {
        clear_screen();
        printf("\n=== EQUIPO %s ===\n", tipo_equipo);
        mostrar_equipo(equipo);

        printf("Opciones de gestion para equipo %s:\n", tipo_equipo);
        printf("1. Modificar un jugador\n");
        printf("2. Agregar un jugador nuevo (si hay espacio)\n");
        printf("3. Eliminar un jugador\n");
        printf("4. Cambiar capitán\n");
        printf("5. Volver\n");

        int opcion = input_int("Seleccione una opcion: ");

        switch (opcion)
        {
        case 1:
            modificar_jugador_momentaneo(equipo);
            break;
        case 2:
            agregar_jugador_momentaneo(equipo);
            break;
        case 3:
            eliminar_jugador_momentaneo(equipo);
            break;
        case 4:
            cambiar_capitan_momentaneo(equipo);
            break;
        case 5:
            salir = 1;
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }
}

/**
 * @brief Función principal para crear equipos
 *
 * Muestra un menú que permite al usuario elegir entre crear un equipo fijo
 * (que se guarda en la base de datos) o un equipo momentáneo (que no se guarda).
 * Delegada a las funciones específicas según la opción seleccionada.
 */
void crear_equipo()
{
    clear_screen();
    print_header("CREAR EQUIPO");

    printf("Seleccione el tipo de equipo:\n");
    printf("1. Fijo\n");
    printf("2. Momentaneo\n");
    printf("3. Volver\n");

    int opcion = input_int(">");

    switch (opcion)
    {
    case 1:
        crear_equipo_fijo();
        break;
    case 2:
        crear_equipo_momentaneo();
        break;
    case 3:
        return;
    default:
        printf("Opcion invalida.\n");
        pause_console();
    }
}

/**
 * @brief Muestra un listado completo de todos los equipos registrados en el sistema
 *
 * Esta función consulta la base de datos y presenta en pantalla todos los equipos
 * con sus respectivos datos, incluyendo información detallada de cada jugador.
 * Muestra el ID, nombre, tipo, tipo de fútbol, número de jugadores y asignación
 * a partidos para cada equipo registrado.
 */
void listar_equipos()
{
    clear_screen();
    print_header("LISTAR EQUIPOS");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre, tipo, tipo_futbol, num_jugadores, partido_id FROM equipo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== LISTA DE EQUIPOS ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            int tipo = sqlite3_column_int(stmt, 2);
            int tipo_futbol = sqlite3_column_int(stmt, 3);
            int num_jugadores = sqlite3_column_int(stmt, 4);
            int partido_id = sqlite3_column_int(stmt, 5);

            printf("ID: %d\n", id);
            printf("Nombre: %s\n", nombre);
            printf("Tipo: %s\n", tipo == FIJO ? "Fijo" : "Momentaneo");
            printf("Tipo de Futbol: %s\n", get_nombre_tipo_futbol(tipo_futbol));
            printf("Numero de Jugadores: %d\n", num_jugadores);
            printf("Asignado a Partido: %s\n", partido_id == -1 ? "No" : "Si");

            // Mostrar jugadores del equipo
            printf("\n=== JUGADORES ===\n");
            mostrar_jugadores_equipo(id);

            printf("----------------------------------------\n");
        }

        if (!found)
        {
            printf("No hay equipos registrados.\n");
        }
    }
    else
    {
        printf("Error al obtener la lista de equipos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Permite modificar los datos de un equipo existente en la base de datos
 *
 * Esta función presenta un menú interactivo que permite al usuario modificar
 * diversos aspectos de un equipo existente, incluyendo su nombre, tipo de fútbol,
 * asignación a partidos y gestión completa de jugadores (modificar, agregar,
 * eliminar o cambiar capitán). Muestra primero la lista de equipos disponibles
 * y solicita confirmación antes de aplicar cualquier cambio.
 *
 * La complejidad cognitiva se ha reducido significativamente mediante el uso
 * de funciones helper que encapsulan lógica específica, siguiendo el principio
 * de responsabilidad única y evitando anidamiento profundo.
 */
void modificar_equipo()
{
    clear_screen();
    print_header("MODIFICAR EQUIPO");

    // Mostrar lista de equipos disponibles
    show_available_teams_for_modification();

    // Obtener y validar ID del equipo
    int equipo_id = get_equipo_id_to_modify();
    if (equipo_id <= 0) return; // Usuario canceló o error

    // Mostrar menú de opciones de modificación
    printf("\nSeleccione que desea modificar:\n");
    printf("1. Nombre del equipo\n");
    printf("2. Tipo de futbol\n");
    printf("3. Asignacion a partido\n");
    printf("4. Jugadores\n");
    printf("5. Volver\n");

    int opcion = input_int(">");

    // Procesar opción seleccionada usando funciones helper
    switch (opcion)
    {
    case 1:
        handle_modify_team_name(equipo_id);
        break;
    case 2:
        handle_modify_team_type(equipo_id);
        break;
    case 3:
        handle_modify_team_assignment(equipo_id);
        break;
    case 4:
        handle_modify_players(equipo_id);
        break;
    case 5:
        return; // Usuario canceló
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

/**
 * @brief Elimina un equipo existente de la base de datos
 *
 * Esta función permite al usuario eliminar permanentemente un equipo y todos sus
 * jugadores asociados. Muestra primero la lista de equipos disponibles, solicita
 * confirmación del ID a eliminar y requiere confirmación explícita antes de
 * proceder con la eliminación. Primero elimina todos los jugadores asociados
 * y luego elimina el registro del equipo.
 */
void eliminar_equipo()
{
    clear_screen();
    print_header("ELIMINAR EQUIPO");

    // Mostrar lista de equipos primero
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== EQUIPOS DISPONIBLES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            printf("%d. %s\n", id, nombre);
        }

        if (!found)
        {
            printf("No hay equipos registrados para eliminar.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
    }
    else
    {
        printf("Error al obtener la lista de equipos: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }
    sqlite3_finalize(stmt);

    int equipo_id = input_int("\nIngrese el ID del equipo a eliminar (0 para cancelar): ");

    if (equipo_id == 0) return;

    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        pause_console();
        return;
    }

    if (confirmar("Esta seguro que desea eliminar este equipo? Esta accion no se puede deshacer."))
    {
        // Eliminar jugadores primero
        const char *sql_delete_jugadores = "DELETE FROM jugador WHERE equipo_id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete_jugadores, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, equipo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Eliminar equipo
        const char *sql_delete_equipo = "DELETE FROM equipo WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete_equipo, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, equipo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Equipo eliminado exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar el equipo: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Eliminacion cancelada.\n");
    }

    pause_console();
}

/**
 * @brief Inicializa las estadísticas del partido
 *
 * @param stats Puntero a estructura de estadísticas del partido
 */
void inicializar_estadisticas_partido(PartidoStats *stats)
{
    stats->goles_local = 0;
    stats->goles_visitante = 0;

    for (int i = 0; i < 11; i++)
    {
        stats->goles_jugadores_local[i] = 0;
        stats->asistencias_jugadores_local[i] = 0;
        stats->goles_jugadores_visitante[i] = 0;
        stats->asistencias_jugadores_visitante[i] = 0;
    }
}

/**
 * @brief Muestra la información inicial del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
void mostrar_informacion_inicial(const Equipo *equipo_local, const Equipo *equipo_visitante)
{
    printf("=== PARTIDO ENTRE %s VS %s ===\n\n", equipo_local->nombre, equipo_visitante->nombre);

    // Mostrar cancha inicial
    mostrar_cancha_animada(0, 0);

    // Mostrar equipos alineados
    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_local->jugadores[i].numero, equipo_local->jugadores[i].nombre);
        if (equipo_local->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_visitante->jugadores[i].numero, equipo_visitante->jugadores[i].nombre);
        if (equipo_visitante->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\n*** INICIO DEL PARTIDO ***\n");
    printf("La simulacion comenzara automaticamente en 3 segundos...\n");
    Sleep(3000); // Esperar 3 segundos antes de comenzar
}

/**
 * @brief Genera un evento aleatorio y retorna el tipo
 *
 * @return Tipo de evento (0=normal, 1=gol, 2=oportunidad, 3=falta)
 */
int generar_evento_aleatorio()
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    int evento_aleatorio = random_value % 100;
    if (evento_aleatorio < 3) return 1; // gol local
    if (evento_aleatorio < 6) return 2; // gol visitante
    if (evento_aleatorio < 15) return 3; // oportunidad
    if (evento_aleatorio < 25) return 4; // falta
    return 0; // normal
}

/**
 * @brief Maneja un gol del equipo local
 *
 * @param equipo_local Puntero al equipo local
 * @param minuto_actual Minuto actual del partido
 * @param goles_local Puntero a contador de goles local
 * @param goles_jugadores_local Array de goles por jugador local
 * @param asistencias_jugadores_local Array de asistencias por jugador local
 * @return Tipo de evento para la cancha animada
 */
int manejar_gol_local(const Equipo *equipo_local, int minuto_actual, int *goles_local,
                      int goles_jugadores_local[], int asistencias_jugadores_local[])
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    int jugador_gol = random_value % equipo_local->num_jugadores;
    random_value = (unsigned int)rand();
    int jugador_asistencia = random_value % equipo_local->num_jugadores;

    // Evitar que un jugador se asista a sí mismo
    while (jugador_asistencia == jugador_gol && equipo_local->num_jugadores > 1)
    {
        random_value = (unsigned int)rand();
        jugador_asistencia = random_value % equipo_local->num_jugadores;
    }

    (*goles_local)++;
    goles_jugadores_local[jugador_gol]++;

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto_actual);
    printf("   Gol de %s (%d) para %s\n",
           equipo_local->jugadores[jugador_gol].nombre,
           equipo_local->jugadores[jugador_gol].numero,
           equipo_local->nombre);

    if (jugador_asistencia != jugador_gol)
    {
        asistencias_jugadores_local[jugador_asistencia]++;
        printf("   Asistencia de %s (%d)\n",
               equipo_local->jugadores[jugador_asistencia].nombre,
               equipo_local->jugadores[jugador_asistencia].numero);
    }

    return 1; // tipo_evento para cancha
}

/**
 * @brief Maneja un gol del equipo visitante
 *
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @param goles_visitante Puntero a contador de goles visitante
 * @param goles_jugadores_visitante Array de goles por jugador visitante
 * @param asistencias_jugadores_visitante Array de asistencias por jugador visitante
 * @return Tipo de evento para la cancha animada
 */
int manejar_gol_visitante(const Equipo *equipo_visitante, int minuto_actual, int *goles_visitante,
                          int goles_jugadores_visitante[], int asistencias_jugadores_visitante[])
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    int jugador_gol = random_value % equipo_visitante->num_jugadores;
    random_value = (unsigned int)rand();
    int jugador_asistencia = random_value % equipo_visitante->num_jugadores;

    // Evitar que un jugador se asista a sí mismo
    while (jugador_asistencia == jugador_gol && equipo_visitante->num_jugadores > 1)
    {
        random_value = (unsigned int)rand();
        jugador_asistencia = random_value % equipo_visitante->num_jugadores;
    }

    (*goles_visitante)++;
    goles_jugadores_visitante[jugador_gol]++;

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto_actual);
    printf("   Gol de %s (%d) para %s\n",
           equipo_visitante->jugadores[jugador_gol].nombre,
           equipo_visitante->jugadores[jugador_gol].numero,
           equipo_visitante->nombre);

    if (jugador_asistencia != jugador_gol)
    {
        asistencias_jugadores_visitante[jugador_asistencia]++;
        printf("   Asistencia de %s (%d)\n",
               equipo_visitante->jugadores[jugador_asistencia].nombre,
               equipo_visitante->jugadores[jugador_asistencia].numero);
    }

    return 1; // tipo_evento para cancha
}

/**
 * @brief Maneja una oportunidad de gol
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @return Tipo de evento para la cancha animada
 */
int manejar_oportunidad_gol(const Equipo *equipo_local, const Equipo *equipo_visitante, int minuto_actual)
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    if (random_value % 2 == 0)
    {
        printf("*** Oportunidad de gol para %s (Minuto %d) ***\n", equipo_local->nombre, minuto_actual);
    }
    else
    {
        printf("*** Oportunidad de gol para %s (Minuto %d) ***\n", equipo_visitante->nombre, minuto_actual);
    }
    return 2; // tipo_evento para cancha
}

/**
 * @brief Maneja una falta
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @return Tipo de evento para la cancha animada
 */
int manejar_falta(const Equipo *equipo_local, const Equipo *equipo_visitante, int minuto_actual)
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    if (random_value % 2 == 0)
    {
        printf("*** Falta cometida por %s (Minuto %d) ***\n", equipo_local->nombre, minuto_actual);
    }
    else
    {
        printf("*** Falta cometida por %s (Minuto %d) ***\n", equipo_visitante->nombre, minuto_actual);
    }
    return 3; // tipo_evento para cancha
}

/**
 * @brief Maneja un evento normal del partido
 *
 * @param minuto_actual Minuto actual del partido
 * @return Tipo de evento para la cancha animada
 */
int manejar_evento_normal(int minuto_actual)
{
    printf("*** El partido continúa... (Minuto %d) ***\n", minuto_actual);
    return 0; // tipo_evento para cancha
}

/**
 * @brief Procesa un evento aleatorio del partido
 *
 * @param tipo_evento Tipo de evento a procesar
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @param stats Puntero a estructura de estadísticas del partido
 * @return Tipo de evento para la cancha animada
 */
int procesar_evento(int tipo_evento, const Equipo *equipo_local, const Equipo *equipo_visitante, int minuto_actual, PartidoStats *stats)
{
    switch (tipo_evento)
    {
    case 1: // gol local
        return manejar_gol_local(equipo_local, minuto_actual, &stats->goles_local,
                                 stats->goles_jugadores_local, stats->asistencias_jugadores_local);
    case 2: // gol visitante
        return manejar_gol_visitante(equipo_visitante, minuto_actual, &stats->goles_visitante,
                                     stats->goles_jugadores_visitante, stats->asistencias_jugadores_visitante);
    case 3: // oportunidad
        return manejar_oportunidad_gol(equipo_local, equipo_visitante, minuto_actual);
    case 4: // falta
        return manejar_falta(equipo_local, equipo_visitante, minuto_actual);
    default: // normal
        return manejar_evento_normal(minuto_actual);
    }
}

/**
 * @brief Simula un minuto del partido
 *
 * @param minuto_actual Minuto actual del partido
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param stats Puntero a estructura de estadísticas del partido
 */
void simular_minuto_partido(int minuto_actual, const Equipo *equipo_local, const Equipo *equipo_visitante, PartidoStats *stats)
{
    clear_screen();
    print_header("SIMULACION DE PARTIDO");

    printf("=== %s %d - %d %s ===\n\n",
           equipo_local->nombre, stats->goles_local, stats->goles_visitante, equipo_visitante->nombre);

    printf("MINUTO: %d\n\n", minuto_actual);

    // Generar y procesar evento aleatorio
    int tipo_evento = generar_evento_aleatorio();
    int tipo_evento_cancha = procesar_evento(tipo_evento, equipo_local, equipo_visitante, minuto_actual, stats);

    // Mostrar cancha animada con balón en movimiento
    mostrar_cancha_animada(minuto_actual, tipo_evento_cancha);

    // Pausa automática de 1 segundo para simular el tiempo real
    Sleep(1000);
}

/**
 * @brief Muestra el resultado final del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param goles_local Goles del equipo local
 * @param goles_visitante Goles del equipo visitante
 */
void mostrar_resultado_final(const Equipo *equipo_local, const Equipo *equipo_visitante,
                             int goles_local, int goles_visitante)
{
    printf("*** RESULTADO FINAL ***\n\n");
    printf("*** 60 MINUTOS COMPLETADOS ***\n\n");

    printf("*** %s %d - %d %s ***\n\n",
           equipo_local->nombre, goles_local, goles_visitante, equipo_visitante->nombre);

    // Determinar resultado
    if (goles_local > goles_visitante)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_local->nombre);
    }
    else if (goles_visitante > goles_local)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_visitante->nombre);
    }
    else
    {
        printf("*** ¡EMPATE! ***\n\n");
    }
}

/**
 * @brief Muestra las estadísticas finales de los jugadores
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param stats Puntero a estructura de estadísticas del partido
 */
void mostrar_estadisticas_jugadores(const Equipo *equipo_local, const Equipo *equipo_visitante, const PartidoStats *stats)
{
    printf("*** ESTADISTICAS DEL PARTIDO ***\n\n");

    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    int tiene_estadisticas_local = 0;
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        if (stats->goles_jugadores_local[i] > 0 || stats->asistencias_jugadores_local[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_local->jugadores[i].nombre,
                   equipo_local->jugadores[i].numero,
                   stats->goles_jugadores_local[i],
                   stats->asistencias_jugadores_local[i]);
            tiene_estadisticas_local = 1;
        }
    }
    if (!tiene_estadisticas_local)
    {
        printf("  Sin goles ni asistencias\n");
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    int tiene_estadisticas_visitante = 0;
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        if (stats->goles_jugadores_visitante[i] > 0 || stats->asistencias_jugadores_visitante[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_visitante->jugadores[i].nombre,
                   equipo_visitante->jugadores[i].numero,
                   stats->goles_jugadores_visitante[i],
                   stats->asistencias_jugadores_visitante[i]);
            tiene_estadisticas_visitante = 1;
        }
    }
    if (!tiene_estadisticas_visitante)
    {
        printf("  Sin goles ni asistencias\n");
    }
}

/**
 * @brief Simula un partido entre dos equipos en ASCII art
 *
 * Esta función simula un partido de fútbol de 60 minutos entre dos equipos momentáneos.
 * Muestra la cancha en ASCII, los jugadores de ambos equipos, genera eventos aleatorios
 * como goles y asistencias, y muestra el marcador en tiempo real.
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
void simular_partido(const Equipo *equipo_local, const Equipo *equipo_visitante)
{
    clear_screen();
    printf("%s\n", ASCII_SIMULACION);
    printf("                    SIMULACION DE PARTIDO\n\n");

    // Inicializar estadísticas
    PartidoStats stats;
    inicializar_estadisticas_partido(&stats);

    // Inicializar generador de números aleatorios (una vez)
    static int _seeded = 0;
    if (!_seeded)
    {
        srand((unsigned)time(NULL));
        _seeded = 1;
    }

    // Mostrar información inicial
    mostrar_informacion_inicial(equipo_local, equipo_visitante);

    // Simulación de 60 minutos
    for (int minuto_actual = 1; minuto_actual <= 60; minuto_actual++)
    {
        simular_minuto_partido(minuto_actual, equipo_local, equipo_visitante, &stats);
    }

    // Fin del partido
    clear_screen();
    print_header("FIN DEL PARTIDO");

    mostrar_resultado_final(equipo_local, equipo_visitante, stats.goles_local, stats.goles_visitante);
    mostrar_estadisticas_jugadores(equipo_local, equipo_visitante, &stats);

    printf("\nPresione Enter para volver al menu...");
    getchar();
}

/**
 * @brief Muestra el menú principal de gestión de equipos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar equipos. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 * Este es el punto de entrada principal para todas las operaciones relacionadas
 * con equipos en el sistema MiFutbolC.
 */
void menu_equipos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_equipo},
        {2, "Listar", listar_equipos},
        {3, "Modificar", modificar_equipo},
        {4, "Eliminar", eliminar_equipo},
        {0, "Volver", NULL}
    };

    ejecutar_menu("EQUIPOS", items, 5);
}
