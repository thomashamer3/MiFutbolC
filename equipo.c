/**
 * @file equipo.c
 * @brief Implementación de funciones para la gestión de equipos en MiFutbolC
 */

#include "equipo.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "partido.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

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
 * @brief Muestra por pantalla toda la información detallada de un equipo
 *
 * Esta función presenta en consola la información completa de un equipo,
 * incluyendo sus datos básicos, tipo de fútbol, número de jugadores y
 * la lista completa de jugadores con sus posiciones y roles.
 *
 * @param equipo Puntero al equipo cuya información se va a mostrar
 */
void mostrar_equipo(Equipo *equipo)
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
        Jugador *jugador = &equipo->jugadores[i];
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

    // Solicitar nombre del equipo
    input_string("Ingrese el nombre del equipo: ", equipo.nombre, sizeof(equipo.nombre));

    // Solicitar tipo de fútbol
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
        equipo.tipo_futbol = FUTBOL_5;
        break;
    case 2:
        equipo.tipo_futbol = FUTBOL_7;
        break;
    case 3:
        equipo.tipo_futbol = FUTBOL_8;
        break;
    case 4:
        equipo.tipo_futbol = FUTBOL_11;
        break;
    case 5:
        return;
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return;
    }

    // Determinar número de jugadores según tipo de fútbol
    int num_jugadores = 0;
    switch (equipo.tipo_futbol)
    {
    case FUTBOL_5:
        num_jugadores = 5;
        break;
    case FUTBOL_7:
        num_jugadores = 7;
        break;
    case FUTBOL_8:
        num_jugadores = 8;
        break;
    case FUTBOL_11:
        num_jugadores = 11;
        break;
    }
    equipo.num_jugadores = num_jugadores;

    // Solicitar información de cada jugador
    for (int i = 0; i < num_jugadores; i++)
    {
        Jugador *jugador = &equipo.jugadores[i];

        clear_screen();
        printf("\nJugador %d de %d:\n", i + 1, num_jugadores);

        // Nombre del jugador
        char nombre_temp[50];
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        strncpy(jugador->nombre, nombre_temp, sizeof(jugador->nombre));

        // Número del jugador
        jugador->numero = input_int("Numero: ");

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
            printf("Posicion invalida. Se asignara como Delantero.\n");
            jugador->posicion = DELANTERO;
        }

        jugador->es_capitan = 0;
    }

    // Seleccionar capitán
    printf("\nSeleccione el capitan del equipo (1-%d):\n", num_jugadores);
    for (int i = 0; i < num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo.jugadores[i].nombre);
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < num_jugadores)
    {
        equipo.jugadores[capitan_idx].es_capitan = 1;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
    }

    // Mostrar equipo creado
    clear_screen();
    mostrar_equipo(&equipo);

    // Guardar en base de datos
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO equipo (nombre, tipo, tipo_futbol, num_jugadores, partido_id) VALUES (?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, equipo.nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, equipo.tipo);
        sqlite3_bind_int(stmt, 3, equipo.tipo_futbol);
        sqlite3_bind_int(stmt, 4, equipo.num_jugadores);
        sqlite3_bind_int(stmt, 5, equipo.partido_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            int equipo_id = sqlite3_last_insert_rowid(db);

            // Guardar jugadores
            sqlite3_stmt *stmt_jugador;
            const char *sql_jugador = "INSERT INTO jugador (equipo_id, nombre, numero, posicion, es_capitan) VALUES (?, ?, ?, ?, ?);";

            for (int i = 0; i < equipo.num_jugadores; i++)
            {
                Jugador *jugador = &equipo.jugadores[i];

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

            printf("Equipo guardado exitosamente con ID: %d\n", equipo_id);

            // Preguntar si asignar a partido
            if (confirmar("Desea asignar este equipo a un partido existente?"))
            {
                listar_partidos();
                int partido_id = input_int("Ingrese el ID del partido (0 para cancelar): ");
                if (partido_id > 0 && existe_id("partido", partido_id))
                {
                    // Actualizar equipo con partido_id
                    const char *sql_update = "UPDATE equipo SET partido_id = ? WHERE id = ?;";
                    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
                    {
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
                }
            }
        }
        else
        {
            printf("Error al guardar el equipo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
    }
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
    Equipo equipo;
    equipo.tipo = MOMENTANEO;
    equipo.partido_id = -1;

    // Solicitar nombre del equipo
    input_string("Ingrese el nombre del equipo: ", equipo.nombre, sizeof(equipo.nombre));

    // Solicitar tipo de fútbol
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
        equipo.tipo_futbol = FUTBOL_5;
        break;
    case 2:
        equipo.tipo_futbol = FUTBOL_7;
        break;
    case 3:
        equipo.tipo_futbol = FUTBOL_8;
        break;
    case 4:
        equipo.tipo_futbol = FUTBOL_11;
        break;
    case 5:
        return;
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return;
    }

    // Determinar número de jugadores según tipo de fútbol
    int num_jugadores = 0;
    switch (equipo.tipo_futbol)
    {
    case FUTBOL_5:
        num_jugadores = 5;
        break;
    case FUTBOL_7:
        num_jugadores = 7;
        break;
    case FUTBOL_8:
        num_jugadores = 8;
        break;
    case FUTBOL_11:
        num_jugadores = 11;
        break;
    }
    equipo.num_jugadores = num_jugadores;

    // Solicitar información de cada jugador
    for (int i = 0; i < num_jugadores; i++)
    {
        Jugador *jugador = &equipo.jugadores[i];

        clear_screen();
        printf("\nJugador %d de %d:\n", i + 1, num_jugadores);

        // Nombre del jugador
        char nombre_temp[50];
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        strncpy(jugador->nombre, nombre_temp, sizeof(jugador->nombre));

        // Número del jugador
        jugador->numero = i + 1; // Auto-asignar números secuenciales

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
            printf("Posicion invalida. Se asignara como Delantero.\n");
            jugador->posicion = DELANTERO;
        }

        jugador->es_capitan = 0;
    }

    // Seleccionar capitán
    printf("\nSeleccione el capitan del equipo (1-%d):\n", num_jugadores);
    for (int i = 0; i < num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo.jugadores[i].nombre);
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < num_jugadores)
    {
        equipo.jugadores[capitan_idx].es_capitan = 1;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
    }

    // Mostrar equipo creado
    clear_screen();
    mostrar_equipo(&equipo);

    printf("Este equipo es momentaneo y no se guardara.\n");
    pause_console();
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
            sqlite3_stmt *stmt_jugadores;
            const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero;";

            if (sqlite3_prepare_v2(db, sql_jugadores, -1, &stmt_jugadores, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_jugadores, 1, id);

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
 */
void modificar_equipo()
{
    clear_screen();
    print_header("MODIFICAR EQUIPO");

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
            printf("No hay equipos registrados para modificar.\n");
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

    int equipo_id = input_int("\nIngrese el ID del equipo a modificar (0 para cancelar): ");

    if (equipo_id == 0) return;

    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        pause_console();
        return;
    }

    // Obtener datos actuales del equipo
    Equipo equipo;
    const char *sql_equipo = "SELECT nombre, tipo, tipo_futbol, num_jugadores, partido_id FROM equipo WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql_equipo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(equipo.nombre, (const char*)sqlite3_column_text(stmt, 0), sizeof(equipo.nombre));
            equipo.tipo = sqlite3_column_int(stmt, 1);
            equipo.tipo_futbol = sqlite3_column_int(stmt, 2);
            equipo.num_jugadores = sqlite3_column_int(stmt, 3);
            equipo.partido_id = sqlite3_column_int(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }

    // Mostrar menu de modificación
    printf("\nSeleccione que desea modificar:\n");
    printf("1. Nombre del equipo\n");
    printf("2. Tipo de futbol\n");
    printf("3. Asignacion a partido\n");
    printf("4. Jugadores\n");
    printf("5. Volver\n");

    int opcion = input_int(">");

    switch (opcion)
    {
    case 1:
    {
        char nuevo_nombre[50];
        input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

        const char *sql_update = "UPDATE equipo SET nombre = ? WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
        {
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
        break;
    }

    case 2:
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
        break;
    }

    case 3:
    {
        if (confirmar("¿Desea asignar este equipo a un partido?"))
        {
            listar_partidos();
            int partido_id = input_int("Ingrese el ID del partido (0 para cancelar): ");
            if (partido_id > 0 && existe_id("partido", partido_id))
            {
                const char *sql_update = "UPDATE equipo SET partido_id = ? WHERE id = ?;";
                if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
                {
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
            }
        }
        else
        {
            const char *sql_update = "UPDATE equipo SET partido_id = -1 WHERE id = ?;";
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
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
        }
        break;
    }

    case 4:
    {
        // Implementar modificación de jugadores
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
                strncpy(jugadores_nombres[jugador_count], (const char*)sqlite3_column_text(stmt_jugadores, 1), sizeof(jugadores_nombres[jugador_count]));
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
                break;
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
            {
                // Modificar jugador existente
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
                    break;
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
                {
                    char nuevo_nombre[50];
                    input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

                    const char *sql_update = "UPDATE jugador SET nombre = ? WHERE id = ?;";
                    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
                    {
                        sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
                        sqlite3_bind_int(stmt, 2, jugadores_ids[jugador_idx]);

                        if (sqlite3_step(stmt) == SQLITE_DONE)
                        {
                            printf("Nombre del jugador actualizado exitosamente.\n");
                        }
                        else
                        {
                            printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
                        }
                        sqlite3_finalize(stmt);
                    }
                    break;
                }

                case 2:
                {
                    int nuevo_numero = input_int("Ingrese el nuevo número: ");

                    // Verificar si el número ya existe
                    int numero_existe = 0;
                    for (int i = 0; i < jugador_count; i++)
                    {
                        if (i != jugador_idx && jugadores_numeros[i] == nuevo_numero)
                        {
                            numero_existe = 1;
                            break;
                        }
                    }

                    if (numero_existe)
                    {
                        printf("El número ya está en uso por otro jugador.\n");
                        pause_console();
                        break;
                    }

                    const char *sql_update = "UPDATE jugador SET numero = ? WHERE id = ?;";
                    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
                    {
                        sqlite3_bind_int(stmt, 1, nuevo_numero);
                        sqlite3_bind_int(stmt, 2, jugadores_ids[jugador_idx]);

                        if (sqlite3_step(stmt) == SQLITE_DONE)
                        {
                            printf("Número del jugador actualizado exitosamente.\n");
                        }
                        else
                        {
                            printf("Error al actualizar el número: %s\n", sqlite3_errmsg(db));
                        }
                        sqlite3_finalize(stmt);
                    }
                    break;
                }

                case 3:
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

                    const char *sql_update = "UPDATE jugador SET posicion = ? WHERE id = ?;";
                    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
                    {
                        sqlite3_bind_int(stmt, 1, posicion);
                        sqlite3_bind_int(stmt, 2, jugadores_ids[jugador_idx]);

                        if (sqlite3_step(stmt) == SQLITE_DONE)
                        {
                            printf("Posición del jugador actualizada exitosamente.\n");
                        }
                        else
                        {
                            printf("Error al actualizar la posición: %s\n", sqlite3_errmsg(db));
                        }
                        sqlite3_finalize(stmt);
                    }
                    break;
                }

                case 4:
                {
                    // Cambiar estado de capitán
                    int nuevo_capitan = !jugadores_capitanes[jugador_idx];

                    const char *sql_update = "UPDATE jugador SET es_capitan = ? WHERE id = ?;";
                    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
                    {
                        sqlite3_bind_int(stmt, 1, nuevo_capitan);
                        sqlite3_bind_int(stmt, 2, jugadores_ids[jugador_idx]);

                        if (sqlite3_step(stmt) == SQLITE_DONE)
                        {
                            printf("Estado de capitán actualizado exitosamente.\n");
                        }
                        else
                        {
                            printf("Error al actualizar el estado de capitán: %s\n", sqlite3_errmsg(db));
                        }
                        sqlite3_finalize(stmt);
                    }
                    break;
                }

                case 5:
                    break;

                default:
                    printf("Opción inválida.\n");
                }
                break;
            }

            case 2:
            {
                // Agregar nuevo jugador
                if (jugador_count >= 11)
                {
                    printf("El equipo ya tiene el máximo de jugadores (11).\n");
                    pause_console();
                    break;
                }

                Jugador nuevo_jugador;

                // Nombre del jugador
                input_string("Nombre: ", nuevo_jugador.nombre, sizeof(nuevo_jugador.nombre));

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
                break;
            }

            case 3:
            {
                // Eliminar jugador
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
                    break;
                }

                if (confirmar("¿Está seguro que desea eliminar este jugador?"))
                {
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
                break;
            }

            case 4:
            {
                // Cambiar capitán
                printf("\nSeleccione el nuevo capitán:\n");
                for (int i = 0; i < jugador_count; i++)
                {
                    printf("%d. %s (Actual: %s)\n", jugadores_numeros[i], jugadores_nombres[i],
                           jugadores_capitanes[i] ? "CAPITAN" : "No");
                }

                int nuevo_capitan_num = input_int("Ingrese el número del nuevo capitán: ");

                // Buscar el jugador
                int nuevo_capitan_idx = -1;
                for (int i = 0; i < jugador_count; i++)
                {
                    if (jugadores_numeros[i] == nuevo_capitan_num)
                    {
                        nuevo_capitan_idx = i;
                        break;
                    }
                }

                if (nuevo_capitan_idx == -1)
                {
                    printf("Número de jugador no encontrado.\n");
                    pause_console();
                    break;
                }

                // Primero, quitar el capitán actual
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
                    sqlite3_bind_int(stmt, 1, jugadores_ids[nuevo_capitan_idx]);

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
                break;
            }

            case 5:
                break;

            default:
                printf("Opción inválida.\n");
            }
        }
        break;
    }

    case 5:
        return;

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
