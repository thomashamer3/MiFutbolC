#include "torneo.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "equipo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

/**
 * @file torneo.c
 * @brief Implementación de funciones para la gestión de torneos en MiFutbolC
 */

// Forward declaration to fix implicit function declaration
void generar_fixture(int torneo_id);
void gestionar_tablas_goleadores_asistidores();
void listar_tablas_goleadores_asistidores(int torneo_id);
void agregar_registro_goleador_asistidor(int torneo_id);
void eliminar_registro_goleador_asistidor(int torneo_id);
void modificar_registro_goleador_asistidor(int torneo_id);

/**
 * Traduce valores enumerados de tipos de torneo a nombres legibles para la interfaz de usuario,
 * facilitando la comprensión de las opciones disponibles.
 */
const char* get_nombre_tipo_torneo(TipoTorneos tipo)
{
    switch (tipo)
    {
    case IDA_Y_VUELTA:
        return "Ida y Vuelta";
    case SOLO_IDA:
        return "Solo Ida";
    case ELIMINACION_DIRECTA:
        return "Eliminacion Directa";
    case GRUPOS_Y_ELIMINACION:
        return "Grupos y Eliminacion";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Convierte un formato de torneo enumerado a su nombre textual
 *
 * Esta función toma un valor del enum FormatoTorneos y devuelve la cadena
 * correspondiente en español para mostrar al usuario.
 *
 * @param formato El valor enumerado del formato de torneo
 * @return Cadena constante con el nombre del formato de torneo, o "Desconocido" si no es válido
 */
const char* get_nombre_formato_torneo(FormatoTorneos formato)
{
    switch (formato)
    {
    case ROUND_ROBIN:
        return "Round-robin (sistema liga)";
    case MINI_GRUPO_CON_FINAL:
        return "Mini grupo con final";
    case LIGA_SIMPLE:
        return "Liga simple";
    case LIGA_DOBLE:
        return "Liga doble";
    case GRUPOS_CON_FINAL:
        return "Grupos + final";
    case COPA_SIMPLE:
        return "Copa simple";
    case GRUPOS_ELIMINACION:
        return "Grupos + eliminacion";
    case COPA_REPECHAJE:
        return "Copa + repechaje";
    case LIGA_GRANDE:
        return "Liga grande";
    case MULTIPLES_GRUPOS:
        return "Multiples grupos";
    case ELIMINACION_FASES:
        return "Eliminacion directa por fases";
    default:
        return "Desconocido";
    }
}

/**
 * Muestra información completa de torneo para confirmación del usuario.
 * Necesario porque la estructura interna no es legible para humanos.
 */
void mostrar_torneo(Torneo *torneo)
{
    printf("\n=== INFORMACION DEL TORNEO ===\n");
    printf("Nombre: %s\n", torneo->nombre);
    printf("Tiene equipo fijo: %s\n", torneo->tiene_equipo_fijo ? "Si" : "No");
    if (torneo->tiene_equipo_fijo)
    {
        printf("Equipo fijo ID: %d\n", torneo->equipo_fijo_id);
    }
    printf("Cantidad de equipos: %d\n", torneo->cantidad_equipos);
    printf("Tipo de torneo: %s\n", get_nombre_tipo_torneo(torneo->tipo_torneo));
    printf("Formato de torneo: %s\n", get_nombre_formato_torneo(torneo->formato_torneo));
    printf("\n");
}

/**
 * @brief Asocia equipos a un torneo
 *
 * Esta función permite asociar equipos existentes a un torneo específico.
 * Muestra una lista de equipos disponibles, permite seleccionar uno y lo asocia
 * al torneo en la base de datos.
 *
 * @param torneo_id ID del torneo al que se asociarán los equipos
 */
void asociar_equipos_torneo(int torneo_id)
{
    clear_screen();
    print_header("ASOCIAR EQUIPOS A TORNEO");

    // Mostrar equipos disponibles
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
            printf("No hay equipos registrados para asociar.\n");
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

    int equipo_id = input_int("\nIngrese el ID del equipo a asociar (0 para cancelar): ");

    if (equipo_id == 0) return;

    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        pause_console();
        return;
    }

    // Verificar si ya esta asociado
    const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo WHERE torneo_id = ? AND equipo_id = ?;";
    if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt, 0);
            if (count > 0)
            {
                printf("Este equipo ya esta asociado al torneo.\n");
                sqlite3_finalize(stmt);
                pause_console();
                return;
            }
        }
        sqlite3_finalize(stmt);
    }

    // Asociar equipo a torneo
    const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Equipo asociado al torneo exitosamente.\n");
        }
        else
        {
            printf("Error al asociar equipo al torneo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Crea un equipo fijo para un torneo
 *
 * Esta función crea un nuevo equipo y lo asocia como equipo fijo a un torneo.
 * Si torneo_id es -1, solo crea el equipo sin asociarlo.
 *
 * @param torneo_id ID del torneo al que se asociará el equipo fijo, o -1 para solo crear el equipo
 */
void crear_equipo_fijo_torneo(int torneo_id)
{
    // Crear equipo fijo usando la función existente
    crear_equipo();

    // Obtener el ID del último equipo creado
    sqlite3_stmt *stmt;
    const char *sql = "SELECT last_insert_rowid();";

    int equipo_id = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            equipo_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (equipo_id == -1)
    {
        printf("No se pudo obtener el ID del equipo creado.\n");
        pause_console();
        return;
    }

    // Asociar el equipo al torneo solo si torneo_id es válido
    if (torneo_id != -1)
    {
        const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Equipo fijo creado y asociado al torneo exitosamente.\n");
            }
            else
            {
                printf("Error al asociar equipo al torneo: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Equipo fijo creado exitosamente. ID: %d\n", equipo_id);
    }

    pause_console();
}

/**
 * Solicita al usuario los datos básicos del torneo (nombre, equipo fijo).
 * Maneja la creación de equipo fijo si es necesario.
 * Retorna 0 si se debe cancelar la creación, 1 si continúa.
 */
static int input_torneo_data(Torneo *torneo)
{
    input_string("Ingrese el nombre del torneo: ", torneo->nombre, sizeof(torneo->nombre));

    torneo->tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

    if (torneo->tiene_equipo_fijo)
    {
        listar_equipos();
        int equipo_id = input_int("\nIngrese el ID del equipo fijo (0 para crear nuevo equipo): ");

        if (equipo_id == 0)
        {
            crear_equipo_fijo_torneo(-1);
            return 0; // Cancelar creación del torneo
        }
        else if (existe_id("equipo", equipo_id))
        {
            torneo->equipo_fijo_id = equipo_id;
        }
        else
        {
            printf("ID de equipo invalido.\n");
            pause_console();
            return 0;
        }
    }

    torneo->cantidad_equipos = input_int("Ingrese la cantidad de equipos en el torneo: ");
    return 1;
}

/**
 * Determina el formato y tipo de torneo basado en la cantidad de equipos.
 * Utiliza lógica de rangos para simplificar la selección automática.
 */
static void determine_formato_torneo(Torneo *torneo)
{
    int cantidad = torneo->cantidad_equipos;

    if (cantidad >= 4 && cantidad <= 6)
    {
        printf("\nPara 4-6 equipos, seleccione el formato:\n");
        printf("1. Round-robin (sistema liga)\n");
        printf("2. Mini grupo con final\n");

        int opcion = input_int(">");
        if (opcion == 1)
        {
            torneo->formato_torneo = ROUND_ROBIN;
            torneo->tipo_torneo = IDA_Y_VUELTA;
        }
        else if (opcion == 2)
        {
            torneo->formato_torneo = MINI_GRUPO_CON_FINAL;
            torneo->tipo_torneo = GRUPOS_Y_ELIMINACION;
        }
        else
        {
            printf("Opcion invalida. Se seleccionará Round-robin por defecto.\n");
            torneo->formato_torneo = ROUND_ROBIN;
            torneo->tipo_torneo = IDA_Y_VUELTA;
        }
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        printf("\nPara 7-12 equipos, seleccione el formato:\n");
        printf("1. Liga simple\n");
        printf("2. Liga doble\n");
        printf("3. Grupos + final\n");
        printf("4. Copa simple\n");

        int opcion = input_int(">");
        switch (opcion)
        {
        case 1:
            torneo->formato_torneo = LIGA_SIMPLE;
            torneo->tipo_torneo = SOLO_IDA;
            break;
        case 2:
            torneo->formato_torneo = LIGA_DOBLE;
            torneo->tipo_torneo = IDA_Y_VUELTA;
            break;
        case 3:
            torneo->formato_torneo = GRUPOS_CON_FINAL;
            torneo->tipo_torneo = GRUPOS_Y_ELIMINACION;
            break;
        case 4:
            torneo->formato_torneo = COPA_SIMPLE;
            torneo->tipo_torneo = ELIMINACION_DIRECTA;
            break;
        default:
            printf("Opcion invalida. Se seleccionará Liga simple por defecto.\n");
            torneo->formato_torneo = LIGA_SIMPLE;
            torneo->tipo_torneo = SOLO_IDA;
        }
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        printf("\nPara 13-20 equipos, seleccione el formato:\n");
        printf("1. Grupos (4-5 grupos) + eliminacion\n");
        printf("2. Copa + repechaje\n");
        printf("3. Liga grande\n");

        int opcion = input_int(">");
        switch (opcion)
        {
        case 1:
            torneo->formato_torneo = GRUPOS_ELIMINACION;
            torneo->tipo_torneo = GRUPOS_Y_ELIMINACION;
            break;
        case 2:
            torneo->formato_torneo = COPA_REPECHAJE;
            torneo->tipo_torneo = ELIMINACION_DIRECTA;
            break;
        case 3:
            torneo->formato_torneo = LIGA_GRANDE;
            torneo->tipo_torneo = IDA_Y_VUELTA;
            break;
        default:
            printf("Opcion invalida. Se seleccionará Grupos + eliminacion por defecto.\n");
            torneo->formato_torneo = GRUPOS_ELIMINACION;
            torneo->tipo_torneo = GRUPOS_Y_ELIMINACION;
        }
    }
    else if (cantidad >= 21)
    {
        printf("\nPara 21 o mas equipos, seleccione el formato:\n");
        printf("1. Multiples grupos\n");
        printf("2. Eliminacion directa por fases\n");

        int opcion = input_int(">");
        switch (opcion)
        {
        case 1:
            torneo->formato_torneo = MULTIPLES_GRUPOS;
            torneo->tipo_torneo = GRUPOS_Y_ELIMINACION;
            break;
        case 2:
            torneo->formato_torneo = ELIMINACION_FASES;
            torneo->tipo_torneo = ELIMINACION_DIRECTA;
            break;
        default:
            printf("Opcion invalida. Se seleccionará Multiples grupos por defecto.\n");
            torneo->formato_torneo = MULTIPLES_GRUPOS;
            torneo->tipo_torneo = GRUPOS_Y_ELIMINACION;
        }
    }
    else
    {
        printf("Cantidad de equipos no válida. Se seleccionará formato por defecto.\n");
        torneo->formato_torneo = ROUND_ROBIN;
        torneo->tipo_torneo = IDA_Y_VUELTA;
    }
}

/**
 * Guarda el torneo en la base de datos y maneja asociaciones iniciales.
 * Retorna el ID del torneo creado o -1 si hay error.
 */
static int save_torneo_to_db(Torneo *torneo)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO torneo (nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo) VALUES (?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, torneo->nombre, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, torneo->tiene_equipo_fijo);
    sqlite3_bind_int(stmt, 3, torneo->equipo_fijo_id);
    sqlite3_bind_int(stmt, 4, torneo->cantidad_equipos);
    sqlite3_bind_int(stmt, 5, torneo->tipo_torneo);
    sqlite3_bind_int(stmt, 6, torneo->formato_torneo);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error al guardar el torneo: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    int torneo_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    // Asociar equipo fijo si existe
    if (torneo->tiene_equipo_fijo && torneo->equipo_fijo_id != -1)
    {
        const char *sql_asociar = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (sqlite3_prepare_v2(db, sql_asociar, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, torneo->equipo_fijo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    return torneo_id;
}

void crear_torneo()
{
    clear_screen();
    print_header("CREAR TORNEO");

    Torneo torneo = {0};
    torneo.tiene_equipo_fijo = 0;
    torneo.equipo_fijo_id = -1;

    if (!input_torneo_data(&torneo))
        return;

    determine_formato_torneo(&torneo);

    clear_screen();
    mostrar_torneo(&torneo);

    int torneo_id = save_torneo_to_db(&torneo);
    if (torneo_id == -1)
        return;

    printf("Torneo guardado exitosamente con ID: %d\n", torneo_id);

    if (confirmar("Desea asociar mas equipos a este torneo?"))
    {
        asociar_equipos_torneo(torneo_id);
    }

    pause_console();
}

void listar_torneos()
{
    clear_screen();
    print_header("LISTAR TORNEOS");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== LISTA DE TORNEOS ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            Torneo torneo;
            torneo.id = sqlite3_column_int(stmt, 0);
            strncpy(torneo.nombre, (const char*)sqlite3_column_text(stmt, 1), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 2);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 3);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 4);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 5);
            torneo.formato_torneo = sqlite3_column_int(stmt, 6);

            mostrar_torneo(&torneo);

            // Mostrar equipos asociados
            printf("=== EQUIPOS ASOCIADOS ===\n");
            sqlite3_stmt *stmt_equipos;
            const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e JOIN equipo_torneo et ON e.id = et.equipo_id WHERE et.torneo_id = ? ORDER BY e.id;";

            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_equipos, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_equipos, 1, torneo.id);

                int has_equipos = 0;
                int count = 1;
                while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
                {
                    has_equipos = 1;
                    // int equipo_id = sqlite3_column_int(stmt_equipos, 0); // Unused variable removed
                    const char *equipo_nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
                    printf("%d. %s\n", count++, equipo_nombre);
                }

                if (!has_equipos)
                {
                    printf("No hay equipos asociados a este torneo.\n");
                }

                sqlite3_finalize(stmt_equipos);
            }
            else
            {
                printf("Error al obtener equipos asociados: %s\n", sqlite3_errmsg(db));
            }

            printf("----------------------------------------\n");
        }

        if (!found)
        {
            printf("No hay torneos registrados.\n");
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void modificar_torneo()
{
    clear_screen();
    print_header("MODIFICAR TORNEO");

    // Mostrar lista de torneos primero
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== TORNEOS DISPONIBLES ===\n\n");

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
            printf("No hay torneos registrados para modificar.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }
    sqlite3_finalize(stmt);

    int torneo_id = input_int("\nIngrese el ID del torneo a modificar (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    // Obtener datos actuales del torneo
    Torneo torneo;
    const char *sql_torneo = "SELECT nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(torneo.nombre, (const char*)sqlite3_column_text(stmt, 0), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 1);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 2);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 3);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 4);
            torneo.formato_torneo = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }

    // Mostrar menú de modificación
    printf("\nSeleccione qué desea modificar:\n");
    printf("1. Nombre del torneo\n");
    printf("2. Equipo fijo\n");
    printf("3. Cantidad de equipos\n");
    printf("4. Tipo y formato de torneo\n");
    printf("5. Asociar equipos\n");
    printf("6. Volver\n");

    int opcion = input_int(">");

    switch (opcion)
    {
    case 1:
    {
        char nuevo_nombre[50];
        input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

        const char *sql_update = "UPDATE torneo SET nombre = ? WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, torneo_id);

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
        int nuevo_tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

        if (nuevo_tiene_equipo_fijo)
        {
            // Mostrar equipos disponibles para asociar
            sqlite3_stmt *stmt_equipos;
            const char *sql_equipos = "SELECT id, nombre FROM equipo ORDER BY id;";

            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_equipos, 0) == SQLITE_OK)
            {
                printf("\n=== EQUIPOS DISPONIBLES ===\n\n");

                int found = 0;
                while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
                {
                    found = 1;
                    int id = sqlite3_column_int(stmt_equipos, 0);
                    const char *nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
                    printf("%d. %s\n", id, nombre);
                }

                if (!found)
                {
                    printf("No hay equipos registrados.\n");
                    sqlite3_finalize(stmt_equipos);
                    pause_console();
                    break;
                }
            }
            else
            {
                printf("Error al obtener la lista de equipos: %s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt_equipos);
                pause_console();
                break;
            }
            sqlite3_finalize(stmt_equipos);

            int equipo_id = input_int("\nIngrese el ID del equipo fijo (0 para cancelar): ");

            if (equipo_id == 0) break;

            if (!existe_id("equipo", equipo_id))
            {
                printf("ID de equipo invalido.\n");
                pause_console();
                break;
            }

            // Actualizar equipo fijo
            const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = ?, equipo_fijo_id = ? WHERE id = ?;";
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, 1);
                sqlite3_bind_int(stmt, 2, equipo_id);
                sqlite3_bind_int(stmt, 3, torneo_id);

                if (sqlite3_step(stmt) == SQLITE_DONE)
                {
                    printf("Equipo fijo actualizado exitosamente.\n");
                }
                else
                {
                    printf("Error al actualizar el equipo fijo: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
            }
        }
        else
        {
            // Remover equipo fijo
            const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = 0, equipo_fijo_id = -1 WHERE id = ?;";
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, torneo_id);

                if (sqlite3_step(stmt) == SQLITE_DONE)
                {
                    printf("Equipo fijo removido exitosamente.\n");
                }
                else
                {
                    printf("Error al remover el equipo fijo: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
            }
        }
        break;
    }

    case 3:
    {
        int nueva_cantidad = input_int("Ingrese la nueva cantidad de equipos: ");

        const char *sql_update = "UPDATE torneo SET cantidad_equipos = ? WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, nueva_cantidad);
            sqlite3_bind_int(stmt, 2, torneo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Cantidad de equipos actualizada exitosamente.\n");
            }
            else
            {
                printf("Error al actualizar la cantidad de equipos: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
        break;
    }

    case 4:
    {
        // Mostrar opciones de formato según cantidad de equipos
        int cantidad = torneo.cantidad_equipos;

        if (cantidad >= 4 && cantidad <= 6)
        {
            printf("\nPara 4-6 equipos, seleccione el formato:\n");
            printf("1. Round-robin (sistema liga)\n");
            printf("2. Mini grupo con final\n");

            int opcion_formato = input_int(">");
            TipoTorneos tipo;
            FormatoTorneos formato;

            switch (opcion_formato)
            {
            case 1:
                formato = ROUND_ROBIN;
                tipo = IDA_Y_VUELTA;
                break;
            case 2:
                formato = MINI_GRUPO_CON_FINAL;
                tipo = GRUPOS_Y_ELIMINACION;
                break;
            default:
                printf("Opcion invalida. No se actualizara el formato.\n");
                pause_console();
                return;
            }

            const char *sql_update = "UPDATE torneo SET tipo_torneo = ?, formato_torneo = ? WHERE id = ?;";
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, tipo);
                sqlite3_bind_int(stmt, 2, formato);
                sqlite3_bind_int(stmt, 3, torneo_id);

                if (sqlite3_step(stmt) == SQLITE_DONE)
                {
                    printf("Formato de torneo actualizado exitosamente.\n");
                }
                else
                {
                    printf("Error al actualizar el formato de torneo: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
            }
        }
        else if (cantidad >= 7 && cantidad <= 12)
        {
            printf("\nPara 7-12 equipos, seleccione el formato:\n");
            printf("1. Liga simple\n");
            printf("2. Liga doble\n");
            printf("3. Grupos + final\n");
            printf("4. Copa simple\n");

            int opcion_formato = input_int(">");
            TipoTorneos tipo;
            FormatoTorneos formato;

            switch (opcion_formato)
            {
            case 1:
                formato = LIGA_SIMPLE;
                tipo = SOLO_IDA;
                break;
            case 2:
                formato = LIGA_DOBLE;
                tipo = IDA_Y_VUELTA;
                break;
            case 3:
                formato = GRUPOS_CON_FINAL;
                tipo = GRUPOS_Y_ELIMINACION;
                break;
            case 4:
                formato = COPA_SIMPLE;
                tipo = ELIMINACION_DIRECTA;
                break;
            default:
                printf("Opcion invalida. No se actualizara el formato.\n");
                pause_console();
                return;
            }

            const char *sql_update = "UPDATE torneo SET tipo_torneo = ?, formato_torneo = ? WHERE id = ?;";
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, tipo);
                sqlite3_bind_int(stmt, 2, formato);
                sqlite3_bind_int(stmt, 3, torneo_id);

                if (sqlite3_step(stmt) == SQLITE_DONE)
                {
                    printf("Formato de torneo actualizado exitosamente.\n");
                }
                else
                {
                    printf("Error al actualizar el formato de torneo: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
            }
        }
        else
        {
            printf("La cantidad de equipos no permite cambiar el formato automáticamente.\n");
            printf("Por favor, modifique la cantidad de equipos primero.\n");
        }
        break;
    }

    case 5:
        asociar_equipos_torneo(torneo_id);
        break;

    case 6:
        return;

    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

void eliminar_torneo()
{
    clear_screen();
    print_header("ELIMINAR TORNEO");

    // Mostrar lista de torneos primero
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== TORNEOS DISPONIBLES ===\n\n");

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
            printf("No hay torneos registrados para eliminar.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }
    sqlite3_finalize(stmt);

    int torneo_id = input_int("\nIngrese el ID del torneo a eliminar (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    if (confirmar("Esta seguro que desea eliminar este torneo? Esta accion no se puede deshacer."))
    {
        // Eliminar asociaciones de equipos primero
        const char *sql_delete_equipos = "DELETE FROM equipo_torneo WHERE torneo_id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete_equipos, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Eliminar torneo
        const char *sql_delete_torneo = "DELETE FROM torneo WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete_torneo, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Torneo eliminado exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar el torneo: %s\n", sqlite3_errmsg(db));
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

void administrar_torneo()
{
    clear_screen();
    print_header("ADMINISTRAR TORNEO");

    // Mostrar lista de torneos primero
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== TORNEOS DISPONIBLES ===\n\n");

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
            printf("No hay torneos registrados para administrar.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }
    sqlite3_finalize(stmt);

    int torneo_id = input_int("\nIngrese el ID del torneo a administrar (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    // Menú de administración con switch statement para manejar funciones con parámetros
    while (1)
    {
        clear_screen();
        print_header("ADMINISTRAR TORNEO");

        printf("1. Dashboard del Torneo\n");
        printf("2. Ver Fixture\n");
        printf("3. Ingresar Resultado\n");
        printf("4. Ver Tabla de Posiciones\n");
        printf("5. Estado de Equipos (liga/copa)\n");
        printf("6. Estadísticas de Jugadores\n");
        printf("7. Historial de Equipo\n");
        printf("8. Exportar Tabla de Posiciones\n");
        printf("9. Exportar Estadísticas\n");
        printf("10. Generar Reporte Completo\n");
        printf("11. Finalizar Torneo\n");
        printf("12. Agregar Equipo al Torneo\n");
        printf("13. Eliminar Equipo del Torneo\n");
        printf("14. Modificar Equipo del Torneo\n");
        printf("15. Generar Fixture\n");
        printf("16. Estadísticas del Torneo\n");
        printf("0. Volver\n");

        int opcion = input_int(">");

        switch (opcion)
        {
        case 1:
        {
            // Dashboard del torneo
            clear_screen();
            print_header("DASHBOARD DEL TORNEO");

            const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e "
                                      "JOIN equipo_torneo et ON e.id = et.equipo_id "
                                      "WHERE et.torneo_id = ? ORDER BY e.nombre;";

            printf("\n=== DASHBOARD DEL TORNEO ===\n\n");
            printf("0. Vista general del torneo\n");

            sqlite3_stmt *stmt_dashboard;
            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_dashboard, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_dashboard, 1, torneo_id);

                int count = 1;
                while (sqlite3_step(stmt_dashboard) == SQLITE_ROW)
                {
                    const char *equipo_nombre = (const char*)sqlite3_column_text(stmt_dashboard, 1);
                    printf("%d. Dashboard de %s\n", count++, equipo_nombre);
                }
                sqlite3_finalize(stmt_dashboard);
            }

            int equipo_opcion = input_int("\nSeleccione una opcion: ");

            if (equipo_opcion == 0)
            {
                mostrar_dashboard_torneo(torneo_id, 0);
            }
            else if (equipo_opcion > 0)
            {
                // Obtener el equipo_id real basado en la opcion
                sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_dashboard, 0);
                sqlite3_bind_int(stmt_dashboard, 1, torneo_id);

                int current_count = 1;
                int selected_equipo_id = -1;
                while (sqlite3_step(stmt_dashboard) == SQLITE_ROW)
                {
                    if (current_count == equipo_opcion)
                    {
                        selected_equipo_id = sqlite3_column_int(stmt_dashboard, 0);
                        break;
                    }
                    current_count++;
                }
                sqlite3_finalize(stmt_dashboard);

                if (selected_equipo_id != -1)
                {
                    mostrar_dashboard_torneo(torneo_id, selected_equipo_id);
                }
                else
                {
                    printf("Opcion invalida.\n");
                    pause_console();
                }
            }
            break;
        }

        case 2:
            mostrar_fixture(torneo_id);
            break;

        case 3:
            ingresar_resultado(torneo_id);
            break;

        case 4:
            ver_tabla_posiciones(torneo_id);
            break;

        case 5:
            estado_equipos(torneo_id);
            break;

        case 6:
        {
            // Submenú para estadisticas de jugadores
            clear_screen();
            print_header("ESTADISTICAS DE JUGADORES");

            const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e "
                                      "JOIN equipo_torneo et ON e.id = et.equipo_id "
                                      "WHERE et.torneo_id = ? ORDER BY e.nombre;";

            printf("\n=== EQUIPOS PARTICIPANTES ===\n\n");

            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, torneo_id);

                int count = 1;
                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    const char *equipo_nombre = (const char*)sqlite3_column_text(stmt, 1);
                    printf("%d. %s\n", count++, equipo_nombre);
                }
                sqlite3_finalize(stmt);
            }

            int equipo_opcion = input_int("\nSeleccione un equipo (0 para mejores goleadores): ");
            if (equipo_opcion == 0)
            {
                mostrar_estadisticas_jugador(torneo_id, 0);
            }
            else if (equipo_opcion > 0)
            {
                // Obtener equipo_id de la opcion seleccionada
                sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0);
                sqlite3_bind_int(stmt, 1, torneo_id);

                int current_count = 1;
                int selected_equipo_id = -1;
                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    if (current_count == equipo_opcion)
                    {
                        selected_equipo_id = sqlite3_column_int(stmt, 0);
                        break;
                    }
                    current_count++;
                }
                sqlite3_finalize(stmt);

                if (selected_equipo_id != -1)
                {
                    mostrar_estadisticas_jugador(torneo_id, selected_equipo_id);
                }
                else
                {
                    printf("Opcion invalida.\n");
                    pause_console();
                }
            }
            break;
        }

        case 7:
        {
            // Historial de equipo
            clear_screen();
            print_header("HISTORIAL DE EQUIPO");

            const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e "
                                      "JOIN equipo_torneo et ON e.id = et.equipo_id "
                                      "WHERE et.torneo_id = ? ORDER BY e.nombre;";

            printf("\n=== EQUIPOS PARTICIPANTES ===\n\n");

            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, torneo_id);

                int count = 1;
                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    const char *equipo_nombre = (const char*)sqlite3_column_text(stmt, 1);
                    printf("%d. %s\n", count++, equipo_nombre);
                }
                sqlite3_finalize(stmt);
            }

            int equipo_opcion = input_int("\nSeleccione un equipo: ");
            if (equipo_opcion > 0)
            {
                // Obtener equipo_id de la opcion seleccionada
                sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0);
                sqlite3_bind_int(stmt, 1, torneo_id);

                int current_count = 1;
                int selected_equipo_id = -1;
                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    if (current_count == equipo_opcion)
                    {
                        selected_equipo_id = sqlite3_column_int(stmt, 0);
                        break;
                    }
                    current_count++;
                }
                sqlite3_finalize(stmt);

                if (selected_equipo_id != -1)
                {
                    mostrar_historial_equipo(selected_equipo_id);
                }
                else
                {
                    printf("Opcion invalida.\n");
                    pause_console();
                }
            }
            break;
        }

        case 8:
            exportar_tabla_posiciones(torneo_id);
            break;

        case 9:
        {
            // Submenú para exportar estadisticas
            clear_screen();
            print_header("EXPORTAR ESTADISTICAS");

            const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e "
                                      "JOIN equipo_torneo et ON e.id = et.equipo_id "
                                      "WHERE et.torneo_id = ? ORDER BY e.nombre;";

            printf("\n=== OPCIONES DE EXPORTACIÓN ===\n\n");
            printf("0. Exportar estadisticas de todos los equipos\n");

            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, torneo_id);

                int count = 1;
                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    const char *equipo_nombre = (const char*)sqlite3_column_text(stmt, 1);
                    printf("%d. %s\n", count++, equipo_nombre);
                }
                sqlite3_finalize(stmt);
            }

            int equipo_opcion = input_int("\nSeleccione una opcion: ");
            if (equipo_opcion == 0)
            {
                exportar_estadisticas_jugadores(torneo_id, 0);
            }
            else if (equipo_opcion > 0)
            {
                // Obtener equipo_id de la opcion seleccionada
                sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0);
                sqlite3_bind_int(stmt, 1, torneo_id);

                int current_count = 1;
                int selected_equipo_id = -1;
                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    if (current_count == equipo_opcion)
                    {
                        selected_equipo_id = sqlite3_column_int(stmt, 0);
                        break;
                    }
                    current_count++;
                }
                sqlite3_finalize(stmt);

                if (selected_equipo_id != -1)
                {
                    exportar_estadisticas_jugadores(torneo_id, selected_equipo_id);
                }
                else
                {
                    printf("Opcion invalida.\n");
                    pause_console();
                }
            }
            break;
        }

        case 10:
            generar_reporte_torneo(torneo_id);
            break;

        case 11:
            finalizar_torneo(torneo_id);
            break;

        case 12:
            asociar_equipos_torneo(torneo_id);
            break;

        case 13:
        {
            // Eliminar equipo del torneo
            clear_screen();
            print_header("ELIMINAR EQUIPO DEL TORNEO");

            // Mostrar equipos asociados al torneo
            sqlite3_stmt *stmt_equipos;
            const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e " \
                                      "JOIN equipo_torneo et ON e.id = et.equipo_id " \
                                      "WHERE et.torneo_id = ? ORDER BY e.nombre;";

            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_equipos, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_equipos, 1, torneo_id);

                printf("\n=== EQUIPOS ASOCIADOS AL TORNEO ===\n\n");

                int found = 0;
                int count = 1;
                while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
                {
                    found = 1;
                    // int equipo_id = sqlite3_column_int(stmt_equipos, 0); // Unused variable removed
                    const char *equipo_nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
                    printf("%d. %s\n", count++, equipo_nombre);
                }

                if (!found)
                {
                    printf("No hay equipos asociados a este torneo.\n");
                    sqlite3_finalize(stmt_equipos);
                    pause_console();
                    break;
                }
                sqlite3_finalize(stmt_equipos);

                int equipo_opcion = input_int("\nSeleccione el equipo a eliminar (0 para cancelar): ");
                if (equipo_opcion == 0) break;

                // Obtener el equipo_id real basado en la opción seleccionada
                sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_equipos, 0);
                sqlite3_bind_int(stmt_equipos, 1, torneo_id);

                int current_count = 1;
                int selected_equipo_id = -1;
                while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
                {
                    if (current_count == equipo_opcion)
                    {
                        selected_equipo_id = sqlite3_column_int(stmt_equipos, 0);
                        break;
                    }
                    current_count++;
                }
                sqlite3_finalize(stmt_equipos);

                if (selected_equipo_id != -1)
                {
                    if (confirmar("¿Está seguro que desea eliminar este equipo del torneo?"))
                    {
                        // Eliminar asociacion equipo-torneo
                        const char *sql_delete = "DELETE FROM equipo_torneo WHERE torneo_id = ? AND equipo_id = ?;";
                        if (sqlite3_prepare_v2(db, sql_delete, -1, &stmt, 0) == SQLITE_OK)
                        {
                            sqlite3_bind_int(stmt, 1, torneo_id);
                            sqlite3_bind_int(stmt, 2, selected_equipo_id);

                            if (sqlite3_step(stmt) == SQLITE_DONE)
                            {
                                printf("Equipo eliminado del torneo exitosamente.\n");

                                // También eliminar estadísticas si existen
                                const char *sql_delete_stats = "DELETE FROM equipo_torneo_estadisticas WHERE torneo_id = ? AND equipo_id = ?;";
                                sqlite3_stmt *stmt_stats;
                                if (sqlite3_prepare_v2(db, sql_delete_stats, -1, &stmt_stats, 0) == SQLITE_OK)
                                {
                                    sqlite3_bind_int(stmt_stats, 1, torneo_id);
                                    sqlite3_bind_int(stmt_stats, 2, selected_equipo_id);
                                    sqlite3_step(stmt_stats);
                                    sqlite3_finalize(stmt_stats);
                                }
                            }
                            else
                            {
                                printf("Error al eliminar el equipo: %s\n", sqlite3_errmsg(db));
                            }
                            sqlite3_finalize(stmt);
                        }
                    }
                }
                else
                {
                    printf("Opcion invalida.\n");
                }
            }
            else
            {
                printf("Error al obtener equipos: %s\n", sqlite3_errmsg(db));
            }
            break;
        }

        case 14:
        {
            // Modificar equipo del torneo
            clear_screen();
            print_header("MODIFICAR EQUIPO DEL TORNEO");

            // Mostrar equipos asociados al torneo
            sqlite3_stmt *stmt_equipos;
            const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e " \
                                      "JOIN equipo_torneo et ON e.id = et.equipo_id " \
                                      "WHERE et.torneo_id = ? ORDER BY e.nombre;";

            if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_equipos, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_equipos, 1, torneo_id);

                printf("\n=== EQUIPOS ASOCIADOS AL TORNEO ===\n\n");

                int found = 0;
                int count = 1;
                while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
                {
                    found = 1;
                    // int equipo_id = sqlite3_column_int(stmt_equipos, 0); // Unused variable removed
                    const char *equipo_nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
                    printf("%d. %s\n", count++, equipo_nombre);
                }

                if (!found)
                {
                    printf("No hay equipos asociados a este torneo.\n");
                    sqlite3_finalize(stmt_equipos);
                    pause_console();
                    break;
                }
                sqlite3_finalize(stmt_equipos);

                int equipo_opcion = input_int("\nSeleccione el equipo a modificar (0 para cancelar): ");
                if (equipo_opcion == 0) break;

                // Obtener el equipo_id real basado en la opción seleccionada
                sqlite3_prepare_v2(db, sql_equipos, -1, &stmt_equipos, 0);
                sqlite3_bind_int(stmt_equipos, 1, torneo_id);

                int current_count = 1;
                int selected_equipo_id = -1;
                while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
                {
                    if (current_count == equipo_opcion)
                    {
                        selected_equipo_id = sqlite3_column_int(stmt_equipos, 0);
                        break;
                    }
                    current_count++;
                }
                sqlite3_finalize(stmt_equipos);

                if (selected_equipo_id != -1)
                {
                    printf("\nModificando equipo: %s\n", get_equipo_nombre(selected_equipo_id));
                    printf("1. Cambiar nombre del equipo\n");
                    printf("2. Volver\n");

                    int sub_opcion = input_int("Seleccione una opción: ");

                    if (sub_opcion == 1)
                    {
                        char nuevo_nombre[50];
                        input_string("Ingrese el nuevo nombre del equipo: ", nuevo_nombre, sizeof(nuevo_nombre));

                        const char *sql_update = "UPDATE equipo SET nombre = ? WHERE id = ?;";
                        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
                        {
                            sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
                            sqlite3_bind_int(stmt, 2, selected_equipo_id);

                            if (sqlite3_step(stmt) == SQLITE_DONE)
                            {
                                printf("Nombre del equipo actualizado exitosamente.\n");
                            }
                            else
                            {
                                printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
                            }
                            sqlite3_finalize(stmt);
                        }
                    }
                }
                else
                {
                    printf("Opcion invalida.\n");
                }
            }
            else
            {
                printf("Error al obtener equipos: %s\n", sqlite3_errmsg(db));
            }
            break;
        }

        case 15:
            generar_fixture(torneo_id);
            break;

        case 0:
            return;

        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }
}

void mostrar_fixture(int torneo_id)
{
    clear_screen();
    print_header("FIXTURE DEL TORNEO");

    // Obtener información del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre, formato_torneo FROM torneo WHERE id = ?;";

    char nombre_torneo[50];
    FormatoTorneos formato = ROUND_ROBIN;

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
            formato = (FormatoTorneos)sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    printf("Torneo: %s\n", nombre_torneo);
    printf("Formato: %s\n", get_nombre_formato_torneo(formato));
    printf("\n=== PARTIDOS PROGRAMADOS ===\n\n");

    // Mostrar partidos según el formato
    const char *sql_partidos = "SELECT p.id, e1.nombre as equipo1, e2.nombre as equipo2, "
                               "p.fecha, p.goles_equipo1, p.goles_equipo2, p.estado "
                               "FROM partido_torneo p "
                               "JOIN equipo e1 ON p.equipo1_id = e1.id "
                               "JOIN equipo e2 ON p.equipo2_id = e2.id "
                               "WHERE p.torneo_id = ? "
                               "ORDER BY p.fecha, p.id;";

    if (sqlite3_prepare_v2(db, sql_partidos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int partido_id = sqlite3_column_int(stmt, 0);
            const char *equipo1 = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo2 = (const char*)sqlite3_column_text(stmt, 2);
            const char *fecha = (const char*)sqlite3_column_text(stmt, 3);
            int goles1 = sqlite3_column_int(stmt, 4);
            int goles2 = sqlite3_column_int(stmt, 5);
            const char *estado = (const char*)sqlite3_column_text(stmt, 6);

            printf("Partido #%d: %s vs %s\n", partido_id, equipo1, equipo2);
            printf("Fecha: %s\n", fecha ? fecha : "No programada");
            printf("Resultado: %d - %d\n", goles1, goles2);
            printf("Estado: %s\n", estado ? estado : "Pendiente");
            printf("----------------------------------------\n");
        }

        if (!found)
        {
            printf("No hay partidos programados para este torneo.\n");
        }
    }
    else
    {
        printf("Error al obtener los partidos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void ingresar_resultado(int torneo_id)
{
    clear_screen();
    print_header("INGRESAR RESULTADO");

    // Mostrar partidos pendientes
    const char *sql_partidos = "SELECT p.id, e1.nombre as equipo1, e2.nombre as equipo2, "
                               "p.fecha, p.estado "
                               "FROM partido_torneo p "
                               "JOIN equipo e1 ON p.equipo1_id = e1.id "
                               "JOIN equipo e2 ON p.equipo2_id = e2.id "
                               "WHERE p.torneo_id = ? AND (p.estado IS NULL OR p.estado = 'Pendiente') "
                               "ORDER BY p.fecha, p.id;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql_partidos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        printf("\n=== PARTIDOS PENDIENTES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int partido_id = sqlite3_column_int(stmt, 0);
            const char *equipo1 = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo2 = (const char*)sqlite3_column_text(stmt, 2);
            const char *fecha = (const char*)sqlite3_column_text(stmt, 3);

            printf("%d. %s vs %s (%s)\n", partido_id, equipo1, equipo2, fecha ? fecha : "No programada");
        }

        if (!found)
        {
            printf("No hay partidos pendientes para este torneo.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
    }
    else
    {
        printf("Error al obtener los partidos: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }
    sqlite3_finalize(stmt);

    int partido_id = input_int("\nIngrese el ID del partido (0 para cancelar): ");

    if (partido_id == 0) return;

    // Verificar si el partido existe y pertenece al torneo
    const char *sql_verificar = "SELECT equipo1_id, equipo2_id FROM partido_torneo "
                                "WHERE id = ? AND torneo_id = ?;";

    int equipo1_id = -1, equipo2_id = -1;
    if (sqlite3_prepare_v2(db, sql_verificar, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, partido_id);
        sqlite3_bind_int(stmt, 2, torneo_id);

        if (sqlite3_step(stmt) != SQLITE_ROW)
        {
            printf("ID de partido invalido o no pertenece a este torneo.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }

        equipo1_id = sqlite3_column_int(stmt, 0);
        equipo2_id = sqlite3_column_int(stmt, 1);
        sqlite3_finalize(stmt);
    }

    // Ingresar resultado
    printf("\nIngrese el resultado del partido:\n");
    int goles1 = input_int("Goles del equipo local: ");
    int goles2 = input_int("Goles del equipo visitante: ");

    // Actualizar resultado
    const char *sql_update = "UPDATE partido_torneo SET goles_equipo1 = ?, goles_equipo2 = ?, "
                             "estado = ? WHERE id = ?;";

    const char *estado = (goles1 > goles2) ? "Equipo1 Ganador" :
                         (goles2 > goles1) ? "Equipo2 Ganador" : "Empate";

    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, goles1);
        sqlite3_bind_int(stmt, 2, goles2);
        sqlite3_bind_text(stmt, 3, estado, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, partido_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Resultado registrado exitosamente.\n");

            // Actualizar tabla de posiciones
            actualizar_tabla_posiciones(torneo_id, equipo1_id, equipo2_id, goles1, goles2);

            // Preguntar si quiere ingresar estadisticas de jugadores
            if (confirmar("¿Desea ingresar estadisticas individuales de los jugadores?"))
            {
                actualizar_estadisticas_jugadores(torneo_id, equipo1_id, equipo2_id, goles1, goles2);
            }
        }
        else
        {
            printf("Error al registrar el resultado: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

void ver_tabla_posiciones(int torneo_id)
{
    clear_screen();
    print_header("TABLA DE POSICIONES");

    // Obtener información del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre FROM torneo WHERE id = ?;";

    char nombre_torneo[50];
    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
        }
        sqlite3_finalize(stmt);
    }

    printf("Torneo: %s\n", nombre_torneo);
    printf("\n=== TABLA DE POSICIONES ===\n\n");
    printf("Pos. Equipo                PJ  PG  PE  PP  GF  GC  DG  Pts\n");
    printf("------------------------------------------------------------\n");

    // Obtener equipos del torneo con sus estadisticas desde la tabla dedicada
    const char *sql_equipos = "SELECT e.id, e.nombre, "
                              "COALESCE(es.partidos_jugados, 0) as partidos_jugados, "
                              "COALESCE(es.partidos_ganados, 0) as partidos_ganados, "
                              "COALESCE(es.partidos_empatados, 0) as partidos_empatados, "
                              "COALESCE(es.partidos_perdidos, 0) as partidos_perdidos, "
                              "COALESCE(es.goles_favor, 0) as goles_favor, "
                              "COALESCE(es.goles_contra, 0) as goles_contra, "
                              "COALESCE(es.puntos, 0) as puntos "
                              "FROM equipo e "
                              "LEFT JOIN equipo_torneo_estadisticas es ON e.id = es.equipo_id AND es.torneo_id = ? "
                              "WHERE EXISTS (SELECT 1 FROM equipo_torneo et WHERE et.equipo_id = e.id AND et.torneo_id = ?) "
                              "ORDER BY puntos DESC, "
                              "(COALESCE(es.goles_favor, 0) - COALESCE(es.goles_contra, 0)) DESC, "
                              "COALESCE(es.goles_favor, 0) DESC, "
                              "e.nombre ASC;";

    if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, torneo_id);

        int posicion = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);
            int dg = gf - gc;
            int pts = sqlite3_column_int(stmt, 8);

            printf("%-4d %-20s %-3d %-3d %-3d %-3d %-3d %-3d %-3d %-3d\n",
                   posicion++, nombre, pj, pg, pe, pp, gf, gc, dg, pts);
        }
    }
    else
    {
        printf("Error al obtener la tabla de posiciones: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void estado_equipos(int torneo_id)
{
    clear_screen();
    print_header("ESTADO DE EQUIPOS");

    // Obtener información del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre, formato_torneo FROM torneo WHERE id = ?;";

    char nombre_torneo[50];
    int formato = 0;

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
            formato = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    printf("Torneo: %s\n", nombre_torneo);
    printf("Formato: %s\n", get_nombre_formato_torneo(formato));
    printf("\n=== ESTADO DE EQUIPOS ===\n\n");

    // Mostrar información según el tipo de torneo
    if (formato == LIGA_SIMPLE || formato == LIGA_DOBLE || formato == ROUND_ROBIN ||
            formato == LIGA_GRANDE || formato == GRUPOS_CON_FINAL)
    {
        // Torneos de liga
        printf("Tipo: Liga\n");
        printf("Informacion: Todos los equipos juegan entre sí según el formato seleccionado.\n");
        printf("Clasificacion: Basada en puntos (3 por victoria, 1 por empate).\n");
        printf("Desempate: 1) Puntos, 2) Diferencia de goles, 3) Goles a favor.\n");
    }
    else if (formato == COPA_SIMPLE || formato == COPA_REPECHAJE ||
             formato == GRUPOS_ELIMINACION || formato == ELIMINACION_DIRECTA ||
             formato == ELIMINACION_FASES)
    {
        // Torneos de copa/eliminacion
        printf("Tipo: Copa/Eliminacion\n");
        printf("Informacion: Los equipos compiten en formato de eliminacion directa.\n");
        printf("Clasificacion: Avanzan los ganadores de cada partido.\n");
        printf("Desempate: Tiempo extra y penales si es necesario.\n");
    }
    else
    {
        printf("Tipo: Formato mixto\n");
        printf("Informacion: Combina fases de grupos con eliminacion directa.\n");
    }

    // Mostrar equipos participantes
    const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e "
                              "JOIN equipo_torneo et ON e.id = et.equipo_id "
                              "WHERE et.torneo_id = ? ORDER BY e.nombre;";

    if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        printf("\nEquipos participantes:\n");
        int count = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            printf("%d. %s\n", count++, nombre);
        }
    }
    else
    {
        printf("Error al obtener los equipos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Obtiene el nombre de un equipo por su ID
 *
 * Esta función consulta la base de datos para obtener el nombre de un equipo
 * basado en su identificador único.
 *
 * @param equipo_id ID del equipo a buscar
 * @return Nombre del equipo o "Equipo Desconocido" si no existe o hay error
 */
const char* get_equipo_nombre(int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM equipo WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char*)sqlite3_column_text(stmt, 0);
            sqlite3_finalize(stmt);
            return nombre ? nombre : "Equipo Desconocido";
        }
        sqlite3_finalize(stmt);
    }
    return "Equipo Desconocido";
}

/**
 * @brief Actualiza la fase del torneo para torneos de eliminacion
 *
 * Esta función actualiza el estado de los equipos en torneos de eliminacion directa,
 * marcando al perdedor como eliminado y avanzando al ganador a la siguiente fase.
 *
 * @param torneo_id ID del torneo
 * @param equipo1_id ID del primer equipo
 * @param equipo2_id ID del segundo equipo
 * @param goles1 Goles del primer equipo
 * @param goles2 Goles del segundo equipo
 */
void actualizar_fase_torneo(int torneo_id, int equipo1_id, int equipo2_id, int goles1, int goles2)
{
    // Esta función actualiza la fase del torneo para torneos de eliminacion
    sqlite3_stmt *stmt;

    // Obtener formato del torneo
    const char *sql_formato = "SELECT formato_torneo FROM torneo WHERE id = ?;";
    int formato = 0;

    if (sqlite3_prepare_v2(db, sql_formato, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            formato = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Solo actualizar fases para torneos de eliminacion
    if (formato == COPA_SIMPLE || formato == COPA_REPECHAJE ||
            formato == GRUPOS_ELIMINACION || formato == ELIMINACION_DIRECTA ||
            formato == ELIMINACION_FASES)
    {

        int ganador_id = (goles1 > goles2) ? equipo1_id : equipo2_id;
        int perdedor_id = (goles1 > goles2) ? equipo2_id : equipo1_id;

        // Marcar al perdedor como eliminado
        const char *sql_eliminar = "UPDATE equipo_torneo_estadisticas SET estado = 'Eliminado' "
                                   "WHERE torneo_id = ? AND equipo_id = ?;";

        if (sqlite3_prepare_v2(db, sql_eliminar, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, perdedor_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Avanzar al ganador a la siguiente fase
        // Por simplicidad, avanzamos automáticamente a la siguiente fase
        // En una implementación completa, esto debería verificar la estructura de fases

        printf("Equipo %s avanza a la siguiente fase.\n", get_equipo_nombre(ganador_id));
        printf("Equipo %s queda eliminado del torneo.\n", get_equipo_nombre(perdedor_id));
    }
}

void mostrar_estadisticas_jugador(int torneo_id, int equipo_id)
{
    clear_screen();
    print_header("ESTADISTICAS DE JUGADORES");

    // Obtener nombre del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
        }
        sqlite3_finalize(stmt);
    }

    printf("Torneo: %s\n", nombre_torneo);

    if (equipo_id > 0)
    {
        // Mostrar estadisticas de un equipo específico
        printf("Equipo: %s\n\n", get_equipo_nombre(equipo_id));

        const char *sql_stats = "SELECT j.nombre, je.goles, je.asistencias, je.tarjetas_amarillas, "
                                "je.tarjetas_rojas, je.minutos_jugados "
                                "FROM jugador_estadisticas je "
                                "JOIN jugador j ON je.jugador_id = j.id "
                                "WHERE je.torneo_id = ? AND je.equipo_id = ? "
                                "ORDER BY je.goles DESC, je.asistencias DESC;";

        if (sqlite3_prepare_v2(db, sql_stats, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);

            printf("Jugador                    Goles  Asist  TA  TR  Minutos\n");
            printf("----------------------------------------------------------\n");

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char *nombre = (const char*)sqlite3_column_text(stmt, 0);
                int goles = sqlite3_column_int(stmt, 1);
                int asistencias = sqlite3_column_int(stmt, 2);
                int ta = sqlite3_column_int(stmt, 3);
                int tr = sqlite3_column_int(stmt, 4);
                int minutos = sqlite3_column_int(stmt, 5);

                printf("%-25s %-6d %-6d %-3d %-3d %-7d\n",
                       nombre, goles, asistencias, ta, tr, minutos);
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        // Mostrar mejores goleadores del torneo
        printf("=== MEJORES GOLEADORES DEL TORNEO ===\n\n");

        const char *sql_goleadores = "SELECT j.nombre, e.nombre as equipo, je.goles, je.asistencias "
                                     "FROM jugador_estadisticas je "
                                     "JOIN jugador j ON je.jugador_id = j.id "
                                     "JOIN equipo e ON je.equipo_id = e.id "
                                     "WHERE je.torneo_id = ? "
                                     "ORDER BY je.goles DESC, je.asistencias DESC LIMIT 10;";

        if (sqlite3_prepare_v2(db, sql_goleadores, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            printf("Pos. Jugador               Equipo               Goles  Asist\n");
            printf("------------------------------------------------------------\n");

            int posicion = 1;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char *jugador = (const char*)sqlite3_column_text(stmt, 0);
                const char *equipo = (const char*)sqlite3_column_text(stmt, 1);
                int goles = sqlite3_column_int(stmt, 2);
                int asistencias = sqlite3_column_int(stmt, 3);

                printf("%-4d %-20s %-20s %-6d %-6d\n",
                       posicion++, jugador, equipo, goles, asistencias);
            }
            sqlite3_finalize(stmt);
        }
    }

    pause_console();
}

void mostrar_historial_equipo(int equipo_id)
{
    clear_screen();
    print_header("HISTORIAL DEL EQUIPO");

    printf("Equipo: %s\n\n", get_equipo_nombre(equipo_id));

    sqlite3_stmt *stmt;
    const char *sql_historial = "SELECT t.nombre, eh.posicion_final, eh.partidos_jugados, "
                                "eh.partidos_ganados, eh.partidos_empatados, eh.partidos_perdidos, "
                                "eh.goles_favor, eh.goles_contra, eh.mejor_goleador, "
                                "eh.fecha_inicio, eh.fecha_fin "
                                "FROM equipo_historial eh "
                                "JOIN torneo t ON eh.torneo_id = t.id "
                                "WHERE eh.equipo_id = ? "
                                "ORDER BY eh.fecha_inicio DESC;";

    if (sqlite3_prepare_v2(db, sql_historial, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo_id);

        printf("Torneo                    Pos.  PJ  PG  PE  PP  GF  GC   DG  Mejor Goleador          Inicio      Fin\n");
        printf("--------------------------------------------------------------------------------------------------------\n");

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *torneo = (const char*)sqlite3_column_text(stmt, 0);
            int posicion = sqlite3_column_int(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);
            int dg = gf - gc;
            const char *goleador = (const char*)sqlite3_column_text(stmt, 8);
            const char *inicio = (const char*)sqlite3_column_text(stmt, 9);
            const char *fin = (const char*)sqlite3_column_text(stmt, 10);

            printf("%-25s %-5d %-3d %-3d %-3d %-3d %-3d %-3d %-3d %-25s %-11s %-11s\n",
                   torneo, posicion, pj, pg, pe, pp, gf, gc, dg,
                   goleador ? goleador : "N/A",
                   inicio ? inicio : "N/A",
                   fin ? fin : "N/A");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        printf("No hay historial disponible para este equipo.\n");
    }

    pause_console();
}

void finalizar_torneo(int torneo_id)
{
    clear_screen();
    print_header("FINALIZAR TORNEO");

    // Obtener información del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
        }
        sqlite3_finalize(stmt);
    }

    printf("Esta seguro que desea finalizar el torneo '%s'?\n", nombre_torneo);
    printf("Esta accion guardará el historial de todos los equipos participantes.\n\n");

    if (!confirmar("Continuar con la finalizacion"))
    {
        printf("Finalizacion cancelada.\n");
        pause_console();
        return;
    }

    // Obtener equipos participantes y sus estadisticas
    const char *sql_equipos = "SELECT e.id, e.nombre, es.partidos_jugados, es.partidos_ganados, "
                              "es.partidos_empatados, es.partidos_perdidos, es.goles_favor, "
                              "es.goles_contra, es.puntos "
                              "FROM equipo e "
                              "JOIN equipo_torneo_estadisticas es ON e.id = es.equipo_id "
                              "WHERE es.torneo_id = ? "
                              "ORDER BY es.puntos DESC, (es.goles_favor - es.goles_contra) DESC;";

    int posicion = 1;
    if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int equipo_id = sqlite3_column_int(stmt, 0);
            const char *equipo_nombre = (const char*)sqlite3_column_text(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);

            // Obtener mejor goleador del equipo en este torneo
            sqlite3_stmt *stmt_goleador;
            const char *sql_goleador = "SELECT j.nombre, je.goles "
                                       "FROM jugador_estadisticas je "
                                       "JOIN jugador j ON je.jugador_id = j.id "
                                       "WHERE je.torneo_id = ? AND je.equipo_id = ? "
                                       "ORDER BY je.goles DESC LIMIT 1;";

            char mejor_goleador[50] = "N/A";
            int goles_mejor = 0;

            if (sqlite3_prepare_v2(db, sql_goleador, -1, &stmt_goleador, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_goleador, 1, torneo_id);
                sqlite3_bind_int(stmt_goleador, 2, equipo_id);

                if (sqlite3_step(stmt_goleador) == SQLITE_ROW)
                {
                    const char *nombre = (const char*)sqlite3_column_text(stmt_goleador, 0);
                    if (nombre)
                    {
                        strncpy(mejor_goleador, nombre, sizeof(mejor_goleador));
                    }
                    goles_mejor = sqlite3_column_int(stmt_goleador, 1);
                }
                sqlite3_finalize(stmt_goleador);
            }

            // Insertar en historial
            const char *sql_insert_historial = "INSERT INTO equipo_historial "
                                               "(equipo_id, torneo_id, posicion_final, partidos_jugados, "
                                               "partidos_ganados, partidos_empatados, partidos_perdidos, "
                                               "goles_favor, goles_contra, mejor_goleador, goles_mejor_goleador, "
                                               "fecha_inicio, fecha_fin) "
                                               "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, date('now'), date('now'));";

            if (sqlite3_prepare_v2(db, sql_insert_historial, -1, &stmt_goleador, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_goleador, 1, equipo_id);
                sqlite3_bind_int(stmt_goleador, 2, torneo_id);
                sqlite3_bind_int(stmt_goleador, 3, posicion);
                sqlite3_bind_int(stmt_goleador, 4, pj);
                sqlite3_bind_int(stmt_goleador, 5, pg);
                sqlite3_bind_int(stmt_goleador, 6, pe);
                sqlite3_bind_int(stmt_goleador, 7, pp);
                sqlite3_bind_int(stmt_goleador, 8, gf);
                sqlite3_bind_int(stmt_goleador, 9, gc);
                sqlite3_bind_text(stmt_goleador, 10, mejor_goleador, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt_goleador, 11, goles_mejor);
                sqlite3_step(stmt_goleador);
                sqlite3_finalize(stmt_goleador);
            }

            printf("Guardado historial para %s (Posicion %d)\n", equipo_nombre, posicion);
            posicion++;
        }
        sqlite3_finalize(stmt);
    }

    printf("\nTorneo finalizado exitosamente. Historial guardado.\n");
    pause_console();
}

void actualizar_estadisticas_jugadores(int torneo_id, int equipo1_id, int equipo2_id, int goles1, int goles2)
{
    // Esta función permite ingresar estadisticas individuales de jugadores después de un partido

    sqlite3_stmt *stmt;

    // Mostrar jugadores de equipo1
    printf("\n=== ESTADISTICAS JUGADORES - %s ===\n", get_equipo_nombre(equipo1_id));

    const char *sql_jugadores1 = "SELECT id, nombre FROM jugador WHERE equipo_id = ? ORDER BY numero;";
    if (sqlite3_prepare_v2(db, sql_jugadores1, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo1_id);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int jugador_id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);

            printf("\nJugador: %s\n", nombre);

            int goles = input_int("Goles anotados: ");
            int asistencias = input_int("Asistencias: ");
            int tarjetas_amarillas = input_int("Tarjetas amarillas: ");
            int tarjetas_rojas = input_int("Tarjetas rojas: ");
            int minutos = input_int("Minutos jugados: ");

            // Verificar si ya existen estadisticas para este jugador en este torneo
            const char *sql_check = "SELECT COUNT(*) FROM jugador_estadisticas "
                                    "WHERE jugador_id = ? AND torneo_id = ? AND equipo_id = ?;";
            sqlite3_stmt *stmt_check;
            int existe = 0;

            if (sqlite3_prepare_v2(db, sql_check, -1, &stmt_check, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_check, 1, jugador_id);
                sqlite3_bind_int(stmt_check, 2, torneo_id);
                sqlite3_bind_int(stmt_check, 3, equipo1_id);

                if (sqlite3_step(stmt_check) == SQLITE_ROW)
                {
                    existe = sqlite3_column_int(stmt_check, 0);
                }
                sqlite3_finalize(stmt_check);
            }

            if (existe)
            {
                // Actualizar estadisticas existentes
                const char *sql_update = "UPDATE jugador_estadisticas SET "
                                         "goles = goles + ?, "
                                         "asistencias = asistencias + ?, "
                                         "tarjetas_amarillas = tarjetas_amarillas + ?, "
                                         "tarjetas_rojas = tarjetas_rojas + ?, "
                                         "minutos_jugados = minutos_jugados + ? "
                                         "WHERE jugador_id = ? AND torneo_id = ? AND equipo_id = ?;";

                if (sqlite3_prepare_v2(db, sql_update, -1, &stmt_check, 0) == SQLITE_OK)
                {
                    sqlite3_bind_int(stmt_check, 1, goles);
                    sqlite3_bind_int(stmt_check, 2, asistencias);
                    sqlite3_bind_int(stmt_check, 3, tarjetas_amarillas);
                    sqlite3_bind_int(stmt_check, 4, tarjetas_rojas);
                    sqlite3_bind_int(stmt_check, 5, minutos);
                    sqlite3_bind_int(stmt_check, 6, jugador_id);
                    sqlite3_bind_int(stmt_check, 7, torneo_id);
                    sqlite3_bind_int(stmt_check, 8, equipo1_id);
                    sqlite3_step(stmt_check);
                    sqlite3_finalize(stmt_check);
                }
            }
            else
            {
                // Insertar nuevas estadisticas
                const char *sql_insert = "INSERT INTO jugador_estadisticas "
                                         "(jugador_id, torneo_id, equipo_id, goles, asistencias, "
                                         "tarjetas_amarillas, tarjetas_rojas, minutos_jugados) "
                                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

                if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt_check, 0) == SQLITE_OK)
                {
                    sqlite3_bind_int(stmt_check, 1, jugador_id);
                    sqlite3_bind_int(stmt_check, 2, torneo_id);
                    sqlite3_bind_int(stmt_check, 3, equipo1_id);
                    sqlite3_bind_int(stmt_check, 4, goles);
                    sqlite3_bind_int(stmt_check, 5, asistencias);
                    sqlite3_bind_int(stmt_check, 6, tarjetas_amarillas);
                    sqlite3_bind_int(stmt_check, 7, tarjetas_rojas);
                    sqlite3_bind_int(stmt_check, 8, minutos);
                    sqlite3_step(stmt_check);
                    sqlite3_finalize(stmt_check);
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    // Mostrar jugadores de equipo2
    printf("\n=== ESTADISTICAS JUGADORES - %s ===\n", get_equipo_nombre(equipo2_id));

    const char *sql_jugadores2 = "SELECT id, nombre FROM jugador WHERE equipo_id = ? ORDER BY numero;";
    if (sqlite3_prepare_v2(db, sql_jugadores2, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo2_id);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int jugador_id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);

            printf("\nJugador: %s\n", nombre);

            int goles = input_int("Goles anotados: ");
            int asistencias = input_int("Asistencias: ");
            int tarjetas_amarillas = input_int("Tarjetas amarillas: ");
            int tarjetas_rojas = input_int("Tarjetas rojas: ");
            int minutos = input_int("Minutos jugados: ");

            // Verificar si ya existen estadisticas para este jugador en este torneo
            const char *sql_check = "SELECT COUNT(*) FROM jugador_estadisticas "
                                    "WHERE jugador_id = ? AND torneo_id = ? AND equipo_id = ?;";
            sqlite3_stmt *stmt_check;
            int existe = 0;

            if (sqlite3_prepare_v2(db, sql_check, -1, &stmt_check, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_check, 1, jugador_id);
                sqlite3_bind_int(stmt_check, 2, torneo_id);
                sqlite3_bind_int(stmt_check, 3, equipo2_id);

                if (sqlite3_step(stmt_check) == SQLITE_ROW)
                {
                    existe = sqlite3_column_int(stmt_check, 0);
                }
                sqlite3_finalize(stmt_check);
            }

            if (existe)
            {
                // Actualizar estadisticas existentes
                const char *sql_update = "UPDATE jugador_estadisticas SET "
                                         "goles = goles + ?, "
                                         "asistencias = asistencias + ?, "
                                         "tarjetas_amarillas = tarjetas_amarillas + ?, "
                                         "tarjetas_rojas = tarjetas_rojas + ?, "
                                         "minutos_jugados = minutos_jugados + ? "
                                         "WHERE jugador_id = ? AND torneo_id = ? AND equipo_id = ?;";

                if (sqlite3_prepare_v2(db, sql_update, -1, &stmt_check, 0) == SQLITE_OK)
                {
                    sqlite3_bind_int(stmt_check, 1, goles);
                    sqlite3_bind_int(stmt_check, 2, asistencias);
                    sqlite3_bind_int(stmt_check, 3, tarjetas_amarillas);
                    sqlite3_bind_int(stmt_check, 4, tarjetas_rojas);
                    sqlite3_bind_int(stmt_check, 5, minutos);
                    sqlite3_bind_int(stmt_check, 6, jugador_id);
                    sqlite3_bind_int(stmt_check, 7, torneo_id);
                    sqlite3_bind_int(stmt_check, 8, equipo2_id);
                    sqlite3_step(stmt_check);
                    sqlite3_finalize(stmt_check);
                }
            }
            else
            {
                // Insertar nuevas estadisticas
                const char *sql_insert = "INSERT INTO jugador_estadisticas "
                                         "(jugador_id, torneo_id, equipo_id, goles, asistencias, "
                                         "tarjetas_amarillas, tarjetas_rojas, minutos_jugados) "
                                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

                if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt_check, 0) == SQLITE_OK)
                {
                    sqlite3_bind_int(stmt_check, 1, jugador_id);
                    sqlite3_bind_int(stmt_check, 2, torneo_id);
                    sqlite3_bind_int(stmt_check, 3, equipo2_id);
                    sqlite3_bind_int(stmt_check, 4, goles);
                    sqlite3_bind_int(stmt_check, 5, asistencias);
                    sqlite3_bind_int(stmt_check, 6, tarjetas_amarillas);
                    sqlite3_bind_int(stmt_check, 7, tarjetas_rojas);
                    sqlite3_bind_int(stmt_check, 8, minutos);
                    sqlite3_step(stmt_check);
                    sqlite3_finalize(stmt_check);
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    printf("Estadisticas de jugadores actualizadas correctamente.\n");
}

void actualizar_tabla_posiciones(int torneo_id, int equipo1_id, int equipo2_id, int goles1, int goles2)
{
    // Esta función actualiza la tabla de posiciones después de un partido
    // Actualiza las estadisticas de los equipos en la tabla equipo_torneo

    sqlite3_stmt *stmt;

    // Primero, verificar si ya existen registros de estadisticas para estos equipos en este torneo
    // Si no existen, crearlos

    // Obtener o crear estadisticas para equipo1
    const char *sql_check_equipo1 = "SELECT COUNT(*) FROM equipo_torneo_estadisticas "
                                    "WHERE torneo_id = ? AND equipo_id = ?;";
    int equipo1_exists = 0;

    if (sqlite3_prepare_v2(db, sql_check_equipo1, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo1_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            equipo1_exists = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Crear tabla de estadisticas si no existe
    const char *sql_create_table = "CREATE TABLE IF NOT EXISTS equipo_torneo_estadisticas ("
                                   "torneo_id INTEGER NOT NULL,"
                                   "equipo_id INTEGER NOT NULL,"
                                   "partidos_jugados INTEGER DEFAULT 0,"
                                   "partidos_ganados INTEGER DEFAULT 0,"
                                   "partidos_empatados INTEGER DEFAULT 0,"
                                   "partidos_perdidos INTEGER DEFAULT 0,"
                                   "goles_favor INTEGER DEFAULT 0,"
                                   "goles_contra INTEGER DEFAULT 0,"
                                   "puntos INTEGER DEFAULT 0,"
                                   "estado TEXT DEFAULT 'Activo',"
                                   "PRIMARY KEY(torneo_id, equipo_id),"
                                   "FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
                                   "FOREIGN KEY(equipo_id) REFERENCES equipo(id));";

    sqlite3_exec(db, sql_create_table, 0, 0, 0);

    // Inicializar estadisticas para equipo1 si no existen
    if (!equipo1_exists)
    {
        const char *sql_init_equipo1 = "INSERT INTO equipo_torneo_estadisticas "
                                       "(torneo_id, equipo_id, partidos_jugados, partidos_ganados, "
                                       "partidos_empatados, partidos_perdidos, goles_favor, goles_contra, puntos, estado) "
                                       "VALUES (?, ?, 0, 0, 0, 0, 0, 0, 0, 'Activo');";

        if (sqlite3_prepare_v2(db, sql_init_equipo1, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo1_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Verificar y crear estadisticas para equipo2
    const char *sql_check_equipo2 = "SELECT COUNT(*) FROM equipo_torneo_estadisticas "
                                    "WHERE torneo_id = ? AND equipo_id = ?;";
    int equipo2_exists = 0;

    if (sqlite3_prepare_v2(db, sql_check_equipo2, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo2_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            equipo2_exists = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Inicializar estadisticas para equipo2 si no existen
    if (!equipo2_exists)
    {
        const char *sql_init_equipo2 = "INSERT INTO equipo_torneo_estadisticas "
                                       "(torneo_id, equipo_id, partidos_jugados, partidos_ganados, "
                                       "partidos_empatados, partidos_perdidos, goles_favor, goles_contra, puntos, estado) "
                                       "VALUES (?, ?, 0, 0, 0, 0, 0, 0, 0, 'Activo');";

        if (sqlite3_prepare_v2(db, sql_init_equipo2, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo2_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Actualizar estadisticas para equipo1
    if (goles1 > goles2)
    {
        // Equipo1 ganó
        const char *sql_update_equipo1 = "UPDATE equipo_torneo_estadisticas SET "
                                         "partidos_jugados = partidos_jugados + 1, "
                                         "partidos_ganados = partidos_ganados + 1, "
                                         "goles_favor = goles_favor + ?, "
                                         "goles_contra = goles_contra + ?, "
                                         "puntos = puntos + 3 "
                                         "WHERE torneo_id = ? AND equipo_id = ?;";

        if (sqlite3_prepare_v2(db, sql_update_equipo1, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, goles1);
            sqlite3_bind_int(stmt, 2, goles2);
            sqlite3_bind_int(stmt, 3, torneo_id);
            sqlite3_bind_int(stmt, 4, equipo1_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else if (goles1 == goles2)
    {
        // Empate
        const char *sql_update_equipo1 = "UPDATE equipo_torneo_estadisticas SET "
                                         "partidos_jugados = partidos_jugados + 1, "
                                         "partidos_empatados = partidos_empatados + 1, "
                                         "goles_favor = goles_favor + ?, "
                                         "goles_contra = goles_contra + ?, "
                                         "puntos = puntos + 1 "
                                         "WHERE torneo_id = ? AND equipo_id = ?;";

        if (sqlite3_prepare_v2(db, sql_update_equipo1, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, goles1);
            sqlite3_bind_int(stmt, 2, goles2);
            sqlite3_bind_int(stmt, 3, torneo_id);
            sqlite3_bind_int(stmt, 4, equipo1_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        // Equipo1 perdió
        const char *sql_update_equipo1 = "UPDATE equipo_torneo_estadisticas SET "
                                         "partidos_jugados = partidos_jugados + 1, "
                                         "partidos_perdidos = partidos_perdidos + 1, "
                                         "goles_favor = goles_favor + ?, "
                                         "goles_contra = goles_contra + ? "
                                         "WHERE torneo_id = ? AND equipo_id = ?;";

        if (sqlite3_prepare_v2(db, sql_update_equipo1, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, goles1);
            sqlite3_bind_int(stmt, 2, goles2);
            sqlite3_bind_int(stmt, 3, torneo_id);
            sqlite3_bind_int(stmt, 4, equipo1_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Actualizar estadisticas para equipo2
    if (goles2 > goles1)
    {
        // Equipo2 ganó
        const char *sql_update_equipo2 = "UPDATE equipo_torneo_estadisticas SET "
                                         "partidos_jugados = partidos_jugados + 1, "
                                         "partidos_ganados = partidos_ganados + 1, "
                                         "goles_favor = goles_favor + ?, "
                                         "goles_contra = goles_contra + ?, "
                                         "puntos = puntos + 3 "
                                         "WHERE torneo_id = ? AND equipo_id = ?;";

        if (sqlite3_prepare_v2(db, sql_update_equipo2, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, goles2);
            sqlite3_bind_int(stmt, 2, goles1);
            sqlite3_bind_int(stmt, 3, torneo_id);
            sqlite3_bind_int(stmt, 4, equipo2_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else if (goles1 == goles2)
    {
        // Empate
        const char *sql_update_equipo2 = "UPDATE equipo_torneo_estadisticas SET "
                                         "partidos_jugados = partidos_jugados + 1, "
                                         "partidos_empatados = partidos_empatados + 1, "
                                         "goles_favor = goles_favor + ?, "
                                         "goles_contra = goles_contra + ?, "
                                         "puntos = puntos + 1 "
                                         "WHERE torneo_id = ? AND equipo_id = ?;";

        if (sqlite3_prepare_v2(db, sql_update_equipo2, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, goles2);
            sqlite3_bind_int(stmt, 2, goles1);
            sqlite3_bind_int(stmt, 3, torneo_id);
            sqlite3_bind_int(stmt, 4, equipo2_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        // Equipo2 perdió
        const char *sql_update_equipo2 = "UPDATE equipo_torneo_estadisticas SET "
                                         "partidos_jugados = partidos_jugados + 1, "
                                         "partidos_perdidos = partidos_perdidos + 1, "
                                         "goles_favor = goles_favor + ?, "
                                         "goles_contra = goles_contra + ? "
                                         "WHERE torneo_id = ? AND equipo_id = ?;";

        if (sqlite3_prepare_v2(db, sql_update_equipo2, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, goles2);
            sqlite3_bind_int(stmt, 2, goles1);
            sqlite3_bind_int(stmt, 3, torneo_id);
            sqlite3_bind_int(stmt, 4, equipo2_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Actualizar fase del torneo y estado de equipos si es torneo de eliminacion
    actualizar_fase_torneo(torneo_id, equipo1_id, equipo2_id, goles1, goles2);

    printf("Tabla de posiciones actualizada correctamente.\n");
}

void mostrar_dashboard_torneo(int torneo_id, int equipo_id)
{
    clear_screen();
    print_header("DASHBOARD DEL TORNEO");

    // Obtener información del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
        }
        sqlite3_finalize(stmt);
    }

    printf("TORNEO: %s\n", nombre_torneo);
    printf("------------------------------------------------------------\n\n");

    if (equipo_id > 0)
    {
        // Dashboard específico de un equipo
        printf("DASHBOARD DE: %s\n", get_equipo_nombre(equipo_id));
        printf("------------------------------------------------------------\n");

        // Posicion actual
        const char *sql_posicion = "SELECT COUNT(*) + 1 FROM equipo_torneo_estadisticas es1 "
                                   "JOIN equipo_torneo_estadisticas es2 ON es1.torneo_id = es2.torneo_id "
                                   "WHERE es1.torneo_id = ? AND es1.equipo_id = ? AND es1.equipo_id != es2.equipo_id "
                                   "AND (es2.puntos > es1.puntos OR "
                                   "     (es2.puntos = es1.puntos AND (es2.goles_favor - es2.goles_contra) > (es1.goles_favor - es1.goles_contra)) OR "
                                   "     (es2.puntos = es1.puntos AND (es2.goles_favor - es2.goles_contra) = (es1.goles_favor - es1.goles_contra) AND es2.goles_favor > es1.goles_favor));";

        int posicion = 1;
        if (sqlite3_prepare_v2(db, sql_posicion, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                posicion = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        // Estadisticas del equipo
        const char *sql_stats = "SELECT partidos_jugados, partidos_ganados, partidos_empatados, partidos_perdidos, "
                                "goles_favor, goles_contra, puntos "
                                "FROM equipo_torneo_estadisticas WHERE torneo_id = ? AND equipo_id = ?;";

        int pj = 0, pg = 0, pe = 0, pp = 0, gf = 0, gc = 0, pts = 0;
        if (sqlite3_prepare_v2(db, sql_stats, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                pj = sqlite3_column_int(stmt, 0);
                pg = sqlite3_column_int(stmt, 1);
                pe = sqlite3_column_int(stmt, 2);
                pp = sqlite3_column_int(stmt, 3);
                gf = sqlite3_column_int(stmt, 4);
                gc = sqlite3_column_int(stmt, 5);
                pts = sqlite3_column_int(stmt, 6);
            }
            sqlite3_finalize(stmt);
        }

        printf("POSICION ACTUAL: %d\n", posicion);
        printf("ESTADISTICAS: PJ:%d PG:%d PE:%d PP:%d GF:%d GC:%d PTS:%d\n\n", pj, pg, pe, pp, gf, gc, pts);

        // Próximo partido
        printf("PRÓXIMO PARTIDO:\n");
        mostrar_proximos_partidos(torneo_id, equipo_id);

        // ULTIMOS resultados
        printf("\nÚLTIMOS RESULTADOS:\n");
        const char *sql_ultimos = "SELECT p.fecha, e1.nombre, e2.nombre, p.goles_equipo1, p.goles_equipo2, p.estado "
                                  "FROM partido_torneo p "
                                  "JOIN equipo e1 ON p.equipo1_id = e1.id "
                                  "JOIN equipo e2 ON p.equipo2_id = e2.id "
                                  "WHERE p.torneo_id = ? AND (p.equipo1_id = ? OR p.equipo2_id = ?) AND p.estado != 'Pendiente' "
                                  "ORDER BY p.fecha DESC LIMIT 5;";

        if (sqlite3_prepare_v2(db, sql_ultimos, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);
            sqlite3_bind_int(stmt, 3, equipo_id);

            int found = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                found = 1;
                const char *equipo1 = (const char*)sqlite3_column_text(stmt, 1);
                const char *equipo2 = (const char*)sqlite3_column_text(stmt, 2);
                int g1 = sqlite3_column_int(stmt, 3);
                int g2 = sqlite3_column_int(stmt, 4);
                const char *estado = (const char*)sqlite3_column_text(stmt, 5);

                printf("  %s vs %s: %d-%d (%s)\n", equipo1, equipo2, g1, g2, estado);
            }
            if (!found)
            {
                printf("  No hay resultados recientes.\n");
            }
            sqlite3_finalize(stmt);
        }

        // Goleadores del equipo
        printf("\n⚽ GOLEADORES DEL EQUIPO:\n");
        const char *sql_goleadores = "SELECT j.nombre, je.goles "
                                     "FROM jugador_estadisticas je "
                                     "JOIN jugador j ON je.jugador_id = j.id "
                                     "WHERE je.torneo_id = ? AND je.equipo_id = ? "
                                     "ORDER BY je.goles DESC LIMIT 5;";

        if (sqlite3_prepare_v2(db, sql_goleadores, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);

            int found = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                found = 1;
                const char *jugador = (const char*)sqlite3_column_text(stmt, 0);
                int goles = sqlite3_column_int(stmt, 1);
                printf("  %s: %d goles\n", jugador, goles);
            }
            if (!found)
            {
                printf("  No hay estadisticas de goles.\n");
            }
            sqlite3_finalize(stmt);
        }

    }
    else
    {
        // Dashboard general del torneo
        printf("📊 VISTA GENERAL DEL TORNEO\n");
        printf("------------------------------------------------------------\n");

        // Tabla de posiciones resumida (top 5)
        printf("🥇 TABLA DE POSICIONES (TOP 5):\n");
        const char *sql_top = "SELECT e.nombre, es.puntos, es.partidos_jugados, es.partidos_ganados, "
                              "es.partidos_empatados, es.partidos_perdidos "
                              "FROM equipo e "
                              "LEFT JOIN equipo_torneo_estadisticas es ON e.id = es.equipo_id AND es.torneo_id = ? "
                              "WHERE EXISTS (SELECT 1 FROM equipo_torneo et WHERE et.equipo_id = e.id AND et.torneo_id = ?) "
                              "ORDER BY es.puntos DESC, (es.goles_favor - es.goles_contra) DESC LIMIT 5;";

        if (sqlite3_prepare_v2(db, sql_top, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, torneo_id);

            int pos = 1;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char *equipo = (const char*)sqlite3_column_text(stmt, 0);
                int puntos = sqlite3_column_int(stmt, 1);
                int pj = sqlite3_column_int(stmt, 2);
                int pg = sqlite3_column_int(stmt, 3);
                int pe = sqlite3_column_int(stmt, 4);
                int pp = sqlite3_column_int(stmt, 5);

                printf("  %d. %-15s %2d pts (%d-%d-%d-%d)\n", pos++, equipo, puntos, pj, pg, pe, pp);
            }
            sqlite3_finalize(stmt);
        }

        // Próximos partidos destacados
        printf("\n📅 PRÓXIMOS PARTIDOS DESTACADOS:\n");
        const char *sql_proximos = "SELECT e1.nombre, e2.nombre, p.fecha "
                                   "FROM partido_torneo p "
                                   "JOIN equipo e1 ON p.equipo1_id = e1.id "
                                   "JOIN equipo e2 ON p.equipo2_id = e2.id "
                                   "WHERE p.torneo_id = ? AND p.estado = 'Pendiente' "
                                   "ORDER BY p.fecha ASC LIMIT 5;";

        if (sqlite3_prepare_v2(db, sql_proximos, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            int found = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                found = 1;
                const char *equipo1 = (const char*)sqlite3_column_text(stmt, 0);
                const char *equipo2 = (const char*)sqlite3_column_text(stmt, 1);
                const char *fecha = (const char*)sqlite3_column_text(stmt, 2);

                printf("  %s vs %s (%s)\n", equipo1, equipo2, fecha ? fecha : "Sin fecha");
            }
            if (!found)
            {
                printf("  No hay partidos programados.\n");
            }
            sqlite3_finalize(stmt);
        }

        // Mejores goleadores del torneo
        printf("\n⚽ MEJORES GOLEADORES:\n");
        const char *sql_goleadores = "SELECT j.nombre, e.nombre as equipo, je.goles "
                                     "FROM jugador_estadisticas je "
                                     "JOIN jugador j ON je.jugador_id = j.id "
                                     "JOIN equipo e ON je.equipo_id = e.id "
                                     "WHERE je.torneo_id = ? "
                                     "ORDER BY je.goles DESC LIMIT 5;";

        if (sqlite3_prepare_v2(db, sql_goleadores, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            int found = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                found = 1;
                const char *jugador = (const char*)sqlite3_column_text(stmt, 0);
                const char *equipo = (const char*)sqlite3_column_text(stmt, 1);
                int goles = sqlite3_column_int(stmt, 2);
                printf("  %s (%s): %d goles\n", jugador, equipo, goles);
            }
            if (!found)
            {
                printf("  No hay estadisticas disponibles.\n");
            }
            sqlite3_finalize(stmt);
        }
    }

    pause_console();
}

void mostrar_proximos_partidos(int torneo_id, int equipo_id)
{
    const char *sql_proximos = "SELECT p.id, e1.nombre, e2.nombre, p.fecha "
                               "FROM partido_torneo p "
                               "JOIN equipo e1 ON p.equipo1_id = e1.id "
                               "JOIN equipo e2 ON p.equipo2_id = e2.id "
                               "WHERE p.torneo_id = ? AND (p.equipo1_id = ? OR p.equipo2_id = ?) AND p.estado = 'Pendiente' "
                               "ORDER BY p.fecha ASC LIMIT 3;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql_proximos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);
        sqlite3_bind_int(stmt, 3, equipo_id);

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int partido_id = sqlite3_column_int(stmt, 0);
            const char *equipo1 = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo2 = (const char*)sqlite3_column_text(stmt, 2);
            const char *fecha = (const char*)sqlite3_column_text(stmt, 3);

            printf("  #%d: %s vs %s (%s)\n", partido_id, equipo1, equipo2, fecha ? fecha : "Sin fecha");
        }
        if (!found)
        {
            printf("  No hay próximos partidos programados.\n");
        }
        sqlite3_finalize(stmt);
    }
}

void exportar_tabla_posiciones(int torneo_id)
{
    clear_screen();
    print_header("EXPORTAR TABLA DE POSICIONES");

    // Obtener nombre del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
        }
        sqlite3_finalize(stmt);
    }

    // Crear nombre de archivo
    char filename[100];
    sprintf(filename, "tabla_posiciones_%s.txt", nombre_torneo);

    // Obtener ruta de exportación
    const char *export_dir = get_export_dir();
    char filepath[200];
    sprintf(filepath, "%s\\%s", export_dir, filename);

    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error al crear archivo de exportación.\n");
        pause_console();
        return;
    }

    // Escribir encabezado
    fprintf(file, "TABLA DE POSICIONES - %s\n", nombre_torneo);
    fprintf(file, "Generado: %s\n\n", __DATE__);
    fprintf(file, "Pos. Equipo                PJ  PG  PE  PP  GF  GC  DG  Pts\n");
    fprintf(file, "------------------------------------------------------------\n");

    // Obtener y escribir datos
    const char *sql_equipos = "SELECT e.id, e.nombre, "
                              "COALESCE(es.partidos_jugados, 0) as partidos_jugados, "
                              "COALESCE(es.partidos_ganados, 0) as partidos_ganados, "
                              "COALESCE(es.partidos_empatados, 0) as partidos_empatados, "
                              "COALESCE(es.partidos_perdidos, 0) as partidos_perdidos, "
                              "COALESCE(es.goles_favor, 0) as goles_favor, "
                              "COALESCE(es.goles_contra, 0) as goles_contra, "
                              "COALESCE(es.puntos, 0) as puntos "
                              "FROM equipo e "
                              "LEFT JOIN equipo_torneo_estadisticas es ON e.id = es.equipo_id AND es.torneo_id = ? "
                              "WHERE EXISTS (SELECT 1 FROM equipo_torneo et WHERE et.equipo_id = e.id AND et.torneo_id = ?) "
                              "ORDER BY puntos DESC, "
                              "(COALESCE(es.goles_favor, 0) - COALESCE(es.goles_contra, 0)) DESC, "
                              "COALESCE(es.goles_favor, 0) DESC, "
                              "e.nombre ASC;";

    if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, torneo_id);

        int posicion = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);
            int dg = gf - gc;
            int pts = sqlite3_column_int(stmt, 8);

            fprintf(file, "%-4d %-20s %-3d %-3d %-3d %-3d %-3d %-3d %-3d %-3d\n",
                    posicion++, nombre, pj, pg, pe, pp, gf, gc, dg, pts);
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
    printf("Tabla de posiciones exportada exitosamente a: %s\n", filepath);
    pause_console();
}

void exportar_estadisticas_jugadores(int torneo_id, int equipo_id)
{
    clear_screen();
    print_header("EXPORTAR ESTADISTICAS DE JUGADORES");

    // Obtener nombre del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
        }
        sqlite3_finalize(stmt);
    }

    // Crear nombre de archivo
    char filename[100];
    if (equipo_id > 0)
    {
        sprintf(filename, "estadisticas_jugadores_%s_%s.txt", nombre_torneo, get_equipo_nombre(equipo_id));
    }
    else
    {
        sprintf(filename, "estadisticas_jugadores_%s_todos.txt", nombre_torneo);
    }

    // Obtener ruta de exportación
    const char *export_dir = get_export_dir();
    char filepath[200];
    sprintf(filepath, "%s\\%s", export_dir, filename);

    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error al crear archivo de exportación.\n");
        pause_console();
        return;
    }

    // Escribir encabezado
    fprintf(file, "ESTADISTICAS DE JUGADORES - %s\n", nombre_torneo);
    if (equipo_id > 0)
    {
        fprintf(file, "Equipo: %s\n", get_equipo_nombre(equipo_id));
    }
    fprintf(file, "Generado: %s\n\n", __DATE__);

    if (equipo_id > 0)
    {
        // Estadisticas de un equipo específico
        fprintf(file, "Jugador                    Goles  Asist  TA  TR  Minutos\n");
        fprintf(file, "----------------------------------------------------------\n");

        const char *sql_stats = "SELECT j.nombre, je.goles, je.asistencias, je.tarjetas_amarillas, "
                                "je.tarjetas_rojas, je.minutos_jugados "
                                "FROM jugador_estadisticas je "
                                "JOIN jugador j ON je.jugador_id = j.id "
                                "WHERE je.torneo_id = ? AND je.equipo_id = ? "
                                "ORDER BY je.goles DESC, je.asistencias DESC;";

        if (sqlite3_prepare_v2(db, sql_stats, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char *nombre = (const char*)sqlite3_column_text(stmt, 0);
                int goles = sqlite3_column_int(stmt, 1);
                int asistencias = sqlite3_column_int(stmt, 2);
                int ta = sqlite3_column_int(stmt, 3);
                int tr = sqlite3_column_int(stmt, 4);
                int minutos = sqlite3_column_int(stmt, 5);

                fprintf(file, "%-25s %-6d %-6d %-3d %-3d %-7d\n",
                        nombre, goles, asistencias, ta, tr, minutos);
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        // Estadisticas de todos los equipos
        fprintf(file, "MEJORES GOLEADORES DEL TORNEO:\n\n");
        fprintf(file, "Jugador               Equipo               Goles  Asist\n");
        fprintf(file, "-------------------------------------------------------\n");

        const char *sql_goleadores = "SELECT j.nombre, e.nombre as equipo, je.goles, je.asistencias "
                                     "FROM jugador_estadisticas je "
                                     "JOIN jugador j ON je.jugador_id = j.id "
                                     "JOIN equipo e ON je.equipo_id = e.id "
                                     "WHERE je.torneo_id = ? "
                                     "ORDER BY je.goles DESC, je.asistencias DESC;";

        if (sqlite3_prepare_v2(db, sql_goleadores, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char *jugador = (const char*)sqlite3_column_text(stmt, 0);
                const char *equipo = (const char*)sqlite3_column_text(stmt, 1);
                int goles = sqlite3_column_int(stmt, 2);
                int asistencias = sqlite3_column_int(stmt, 3);

                fprintf(file, "%-20s %-20s %-6d %-6d\n",
                        jugador, equipo, goles, asistencias);
            }
            sqlite3_finalize(stmt);
        }
    }

    fclose(file);
    printf("Estadisticas de jugadores exportadas exitosamente a: %s\n", filepath);
    pause_console();
}

void generar_reporte_torneo(int torneo_id)
{
    clear_screen();
    print_header("GENERAR REPORTE DEL TORNEO");

    // Obtener nombre del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre, formato_torneo FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";
    FormatoTorneos formato = ROUND_ROBIN;

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
            formato = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // Crear nombre de archivo
    char filename[100];
    sprintf(filename, "reporte_torneo_%s.txt", nombre_torneo);

    // Obtener ruta de exportación
    const char *export_dir = get_export_dir();
    char filepath[200];
    sprintf(filepath, "%s\\%s", export_dir, filename);

    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error al crear archivo de reporte.\n");
        pause_console();
        return;
    }

    // Escribir reporte completo
    fprintf(file, "═══════════════════════════════════════════════════════════════\n");
    fprintf(file, "                    REPORTE DEL TORNEO\n");
    fprintf(file, "═══════════════════════════════════════════════════════════════\n\n");

    fprintf(file, "TORNEO: %s\n", nombre_torneo);
    fprintf(file, "FORMATO: %s\n", get_nombre_formato_torneo(formato));
    fprintf(file, "GENERADO: %s\n\n", __DATE__);

    // Resumen general
    fprintf(file, "═══════════════════════════════════════════════════════════════\n");
    fprintf(file, "                         RESUMEN GENERAL\n");
    fprintf(file, "═══════════════════════════════════════════════════════════════\n\n");

    // Contar partidos jugados, totales, etc.
    const char *sql_resumen = "SELECT "
                              "COUNT(CASE WHEN p.estado != 'Pendiente' THEN 1 END) as partidos_jugados, "
                              "COUNT(*) as partidos_totales, "
                              "SUM(p.goles_equipo1) + SUM(p.goles_equipo2) as goles_totales "
                              "FROM partido_torneo p WHERE p.torneo_id = ?;";

    int partidos_jugados = 0, partidos_totales = 0, goles_totales = 0;
    if (sqlite3_prepare_v2(db, sql_resumen, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            partidos_jugados = sqlite3_column_int(stmt, 0);
            partidos_totales = sqlite3_column_int(stmt, 1);
            goles_totales = sqlite3_column_int(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(file, "📊 Partidos jugados: %d/%d\n", partidos_jugados, partidos_totales);
    fprintf(file, "⚽ Goles totales: %d\n", goles_totales);
    fprintf(file, "📈 Promedio de goles por partido: %.1f\n\n", partidos_jugados > 0 ? (float)goles_totales / partidos_jugados : 0);

    // Tabla de posiciones completa
    fprintf(file, "═══════════════════════════════════════════════════════════════\n");
    fprintf(file, "                     TABLA DE POSICIONES\n");
    fprintf(file, "═══════════════════════════════════════════════════════════════\n\n");

    fprintf(file, "Pos. Equipo                PJ  PG  PE  PP  GF  GC  DG  Pts\n");
    fprintf(file, "------------------------------------------------------------\n");

    // Obtener tabla de posiciones
    const char *sql_equipos = "SELECT e.id, e.nombre, "
                              "COALESCE(es.partidos_jugados, 0) as partidos_jugados, "
                              "COALESCE(es.partidos_ganados, 0) as partidos_ganados, "
                              "COALESCE(es.partidos_empatados, 0) as partidos_empatados, "
                              "COALESCE(es.partidos_perdidos, 0) as partidos_perdidos, "
                              "COALESCE(es.goles_favor, 0) as goles_favor, "
                              "COALESCE(es.goles_contra, 0) as goles_contra, "
                              "COALESCE(es.puntos, 0) as puntos "
                              "FROM equipo e "
                              "LEFT JOIN equipo_torneo_estadisticas es ON e.id = es.equipo_id AND es.torneo_id = ? "
                              "WHERE EXISTS (SELECT 1 FROM equipo_torneo et WHERE et.equipo_id = e.id AND et.torneo_id = ?) "
                              "ORDER BY puntos DESC, "
                              "(COALESCE(es.goles_favor, 0) - COALESCE(es.goles_contra, 0)) DESC, "
                              "COALESCE(es.goles_favor, 0) DESC, "
                              "e.nombre ASC;";

    if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, torneo_id);

        int posicion = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);
            int dg = gf - gc;
            int pts = sqlite3_column_int(stmt, 8);

            fprintf(file, "%-4d %-20s %-3d %-3d %-3d %-3d %-3d %-3d %-3d %-3d\n",
                    posicion++, nombre, pj, pg, pe, pp, gf, gc, dg, pts);
        }
        sqlite3_finalize(stmt);
    }

    // Estadisticas de jugadores
    fprintf(file, "\n═══════════════════════════════════════════════════════════════\n");
    fprintf(file, "                  ESTADISTICAS DE JUGADORES\n");
    fprintf(file, "═══════════════════════════════════════════════════════════════\n\n");

    fprintf(file, "MEJORES GOLEADORES:\n\n");
    const char *sql_goleadores = "SELECT j.nombre, e.nombre as equipo, je.goles, je.asistencias, "
                                 "je.tarjetas_amarillas, je.tarjetas_rojas "
                                 "FROM jugador_estadisticas je "
                                 "JOIN jugador j ON je.jugador_id = j.id "
                                 "JOIN equipo e ON je.equipo_id = e.id "
                                 "WHERE je.torneo_id = ? "
                                 "ORDER BY je.goles DESC, je.asistencias DESC LIMIT 15;";

    if (sqlite3_prepare_v2(db, sql_goleadores, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        fprintf(file, "Jugador               Equipo               Goles  Asist  TA  TR\n");
        fprintf(file, "----------------------------------------------------------------\n");

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *jugador = (const char*)sqlite3_column_text(stmt, 0);
            const char *equipo = (const char*)sqlite3_column_text(stmt, 1);
            int goles = sqlite3_column_int(stmt, 2);
            int asistencias = sqlite3_column_int(stmt, 3);
            int ta = sqlite3_column_int(stmt, 4);
            int tr = sqlite3_column_int(stmt, 5);

            fprintf(file, "%-20s %-20s %-6d %-6d %-3d %-3d\n",
                    jugador, equipo, goles, asistencias, ta, tr);
        }
        sqlite3_finalize(stmt);
    }

    // Resultados recientes
    fprintf(file, "\n═══════════════════════════════════════════════════════════════\n");
    fprintf(file, "                    ULTIMOS RESULTADOS\n");
    fprintf(file, "═══════════════════════════════════════════════════════════════\n\n");

    const char *sql_resultados = "SELECT p.fecha, e1.nombre, e2.nombre, p.goles_equipo1, p.goles_equipo2, p.estado "
                                 "FROM partido_torneo p "
                                 "JOIN equipo e1 ON p.equipo1_id = e1.id "
                                 "JOIN equipo e2 ON p.equipo2_id = e2.id "
                                 "WHERE p.torneo_id = ? AND p.estado != 'Pendiente' "
                                 "ORDER BY p.fecha DESC LIMIT 10;";

    if (sqlite3_prepare_v2(db, sql_resultados, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *fecha = (const char*)sqlite3_column_text(stmt, 0);
            const char *equipo1 = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo2 = (const char*)sqlite3_column_text(stmt, 2);
            int g1 = sqlite3_column_int(stmt, 3);
            int g2 = sqlite3_column_int(stmt, 4);

            fprintf(file, "%s: %s %d-%d %s\n", fecha ? fecha : "Sin fecha", equipo1, g1, g2, equipo2);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(file, "\n═══════════════════════════════════════════════════════════════\n");
    fprintf(file, "                   FIN DEL REPORTE\n");
    fprintf(file, "═══════════════════════════════════════════════════════════════\n");

    fclose(file);
    printf("Reporte completo generado exitosamente en: %s\n", filepath);
    pause_console();
}

void generar_fixture(int torneo_id)
{
    clear_screen();
    print_header("GENERAR FIXTURE");

    // Obtener información del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre, formato_torneo, cantidad_equipos FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";
    FormatoTorneos formato = ROUND_ROBIN;
    int cantidad_equipos = 0;

    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
            formato = (FormatoTorneos)sqlite3_column_int(stmt, 1);
            cantidad_equipos = sqlite3_column_int(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    printf("Generando fixture para: %s\n", nombre_torneo);
    printf("Formato: %s\n", get_nombre_formato_torneo(formato));
    printf("Equipos: %d\n\n", cantidad_equipos);

    // Verificar si ya existe un fixture
    const char *sql_check = "SELECT COUNT(*) FROM partido_torneo WHERE torneo_id = ?;";
    int partidos_existentes = 0;

    if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            partidos_existentes = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (partidos_existentes > 0)
    {
        printf("Ya existe un fixture generado para este torneo.\n");
        if (!confirmar("¿Desea regenerar el fixture? (Se perderán los resultados existentes)"))
        {
            pause_console();
            return;
        }

        // Eliminar partidos existentes
        const char *sql_delete = "DELETE FROM partido_torneo WHERE torneo_id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Obtener lista de equipos participantes
    const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e "
                              "JOIN equipo_torneo et ON e.id = et.equipo_id "
                              "WHERE et.torneo_id = ? ORDER BY e.nombre;";

    int equipo_ids[20]; // Máximo 20 equipos
    char equipo_nombres[20][50];
    int num_equipos = 0;

    if (sqlite3_prepare_v2(db, sql_equipos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        while (sqlite3_step(stmt) == SQLITE_ROW && num_equipos < 20)
        {
            equipo_ids[num_equipos] = sqlite3_column_int(stmt, 0);
            strncpy(equipo_nombres[num_equipos], (const char*)sqlite3_column_text(stmt, 1), sizeof(equipo_nombres[num_equipos]));
            num_equipos++;
        }
        sqlite3_finalize(stmt);
    }

    if (num_equipos == 0)
    {
        printf("No hay equipos asociados a este torneo.\n");
        pause_console();
        return;
    }

    // Generar fixture según el formato
    int partidos_generados = 0;

    switch (formato)
    {
    case ROUND_ROBIN:
    case LIGA_SIMPLE:
    case LIGA_DOBLE:
    case LIGA_GRANDE:
    {
        // Liga: todos contra todos
        int ida_vuelta = (formato == LIGA_DOBLE || formato == ROUND_ROBIN) ? 2 : 1;

        for (int vuelta = 0; vuelta < ida_vuelta; vuelta++)
        {
            for (int i = 0; i < num_equipos; i++)
            {
                for (int j = i + 1; j < num_equipos; j++)
                {
                    int equipo1 = vuelta == 0 ? equipo_ids[i] : equipo_ids[j];
                    int equipo2 = vuelta == 0 ? equipo_ids[j] : equipo_ids[i];

                    // Insertar partido
                    const char *sql_insert = "INSERT INTO partido_torneo "
                                             "(torneo_id, equipo1_id, equipo2_id, estado, fase) "
                                             "VALUES (?, ?, ?, 'Pendiente', 'Fase de Grupos');";

                    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
                    {
                        sqlite3_bind_int(stmt, 1, torneo_id);
                        sqlite3_bind_int(stmt, 2, equipo1);
                        sqlite3_bind_int(stmt, 3, equipo2);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        partidos_generados++;
                    }
                }
            }
        }
        break;
    }

    case COPA_SIMPLE:
    case COPA_REPECHAJE:
    case ELIMINACION_FASES:
    {
        // Eliminacion directa: crear partidos de octavos, cuartos, semifinales, final
        if (num_equipos >= 4)
        {
            // Para simplificar, asumimos bracket de eliminacion directa
            // En una implementación completa, esto debería ser mas sofisticado

            // Primera ronda (octavos si hay suficientes equipos)
            for (int i = 0; i < num_equipos; i += 2)
            {
                if (i + 1 < num_equipos)
                {
                    const char *sql_insert = "INSERT INTO partido_torneo "
                                             "(torneo_id, equipo1_id, equipo2_id, estado, fase) "
                                             "VALUES (?, ?, ?, 'Pendiente', 'Primera Ronda');";

                    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
                    {
                        sqlite3_bind_int(stmt, 1, torneo_id);
                        sqlite3_bind_int(stmt, 2, equipo_ids[i]);
                        sqlite3_bind_int(stmt, 3, equipo_ids[i + 1]);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        partidos_generados++;
                    }
                }
            }

            // Crear partidos de siguiente ronda (placeholders)
            // En una implementación real, estos se crearían dinámicamente
            // después de que se complete cada ronda
        }
        break;
    }

    case GRUPOS_CON_FINAL:
    case GRUPOS_ELIMINACION:
    case MULTIPLES_GRUPOS:
    {
        // Grupos: dividir equipos en grupos
        int num_grupos = (num_equipos <= 8) ? 2 : (num_equipos <= 12) ? 3 : 4;
        int equipos_por_grupo = num_equipos / num_grupos;

        printf("Creando %d grupos con aproximadamente %d equipos cada uno.\n", num_grupos, equipos_por_grupo);

        for (int grupo = 0; grupo < num_grupos; grupo++)
        {
            char nombre_grupo[20];
            sprintf(nombre_grupo, "Grupo %c", 'A' + grupo);

            int inicio_grupo = grupo * equipos_por_grupo;
            int fin_grupo = (grupo == num_grupos - 1) ? num_equipos : (grupo + 1) * equipos_por_grupo;

            // Crear partidos dentro del grupo
            for (int i = inicio_grupo; i < fin_grupo; i++)
            {
                for (int j = i + 1; j < fin_grupo; j++)
                {
                    const char *sql_insert = "INSERT INTO partido_torneo "
                                             "(torneo_id, equipo1_id, equipo2_id, estado, fase) "
                                             "VALUES (?, ?, ?, 'Pendiente', ?);";

                    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
                    {
                        sqlite3_bind_int(stmt, 1, torneo_id);
                        sqlite3_bind_int(stmt, 2, equipo_ids[i]);
                        sqlite3_bind_int(stmt, 3, equipo_ids[j]);
                        sqlite3_bind_text(stmt, 4, nombre_grupo, -1, SQLITE_STATIC);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        partidos_generados++;
                    }
                }
            }
        }

        // Crear partido final si aplica
        if (formato == GRUPOS_CON_FINAL)
        {
            // Placeholder para la final - se actualizara cuando se conozcan los clasificados
            const char *sql_insert = "INSERT INTO partido_torneo "
                                     "(torneo_id, equipo1_id, equipo2_id, estado, fase) "
                                     "VALUES (?, NULL, NULL, 'Pendiente', 'Final');";

            if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, torneo_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                partidos_generados++;
            }
        }
        break;
    }

    default:
        printf("Formato de torneo no soportado para generación automatica de fixture.\n");
        pause_console();
        return;
    }

    printf("Fixture generado exitosamente: %d partidos programados.\n", partidos_generados);
    pause_console();
}

/**
 * @brief Muestra el menú para gestionar tablas de goleadores y asistidores
 *
 * Permite agregar, eliminar, modificar y listar registros en las tablas de goleadores y asistidores
 */
void gestionar_tablas_goleadores_asistidores()
{
    clear_screen();
    print_header("GESTIONAR TABLAS DE GOLEADORES Y ASISTIDORES");

    // Mostrar lista de torneos primero
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== TORNEOS DISPONIBLES ===\n\n");

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
            printf("No hay torneos registrados.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }
    sqlite3_finalize(stmt);

    int torneo_id = input_int("\nIngrese el ID del torneo (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    // Menú de gestión de tablas
    while (1)
    {
        clear_screen();
        print_header("GESTIONAR TABLAS DE GOLEADORES Y ASISTIDORES");

        printf("1. Listar tablas de goleadores y asistidores\n");
        printf("2. Agregar registro a tablas\n");
        printf("3. Eliminar registro de tablas\n");
        printf("4. Modificar registro de tablas\n");
        printf("0. Volver\n");

        int opcion = input_int(">");

        switch (opcion)
        {
        case 1:
            listar_tablas_goleadores_asistidores(torneo_id);
            break;
        case 2:
            agregar_registro_goleador_asistidor(torneo_id);
            break;
        case 3:
            eliminar_registro_goleador_asistidor(torneo_id);
            break;
        case 4:
            modificar_registro_goleador_asistidor(torneo_id);
            break;
        case 0:
            return;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }
}

/**
 * @brief Lista las tablas de goleadores y asistidores de un torneo
 *
 * @param torneo_id ID del torneo
 */
void listar_tablas_goleadores_asistidores(int torneo_id)
{
    clear_screen();
    print_header("TABLAS DE GOLEADORES Y ASISTIDORES");

    // Obtener nombre del torneo
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre FROM torneo WHERE id = ?;";
    char nombre_torneo[50] = "";
    if (sqlite3_prepare_v2(db, sql_torneo, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy(nombre_torneo, (const char*)sqlite3_column_text(stmt, 0), sizeof(nombre_torneo));
        }
        sqlite3_finalize(stmt);
    }

    printf("Torneo: %s\n\n", nombre_torneo);

    // Mostrar tabla de goleadores
    printf("=== TABLA DE GOLEADORES ===\n\n");
    printf("Pos. Jugador               Equipo               Goles  Asist\n");
    printf("-------------------------------------------------------\n");

    const char *sql_goleadores = "SELECT j.id, j.nombre, e.nombre as equipo, je.goles, je.asistencias "
                                 "FROM jugador_estadisticas je "
                                 "JOIN jugador j ON je.jugador_id = j.id "
                                 "JOIN equipo e ON je.equipo_id = e.id "
                                 "WHERE je.torneo_id = ? "
                                 "ORDER BY je.goles DESC, je.asistencias DESC;";

    if (sqlite3_prepare_v2(db, sql_goleadores, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        int posicion = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            // int jugador_id = sqlite3_column_int(stmt, 0); // Unused variable removed
            const char *jugador = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo = (const char*)sqlite3_column_text(stmt, 2);
            int goles = sqlite3_column_int(stmt, 3);
            int asistencias = sqlite3_column_int(stmt, 4);

            printf("%-4d %-20s %-20s %-6d %-6d\n",
                   posicion++, jugador, equipo, goles, asistencias);
        }
        sqlite3_finalize(stmt);
    }

    // Mostrar tabla de asistidores
    printf("\n=== TABLA DE ASISTIDORES ===\n\n");
    printf("Pos. Jugador               Equipo               Asist  Goles\n");
    printf("-------------------------------------------------------\n");

    const char *sql_asistidores = "SELECT j.id, j.nombre, e.nombre as equipo, je.asistencias, je.goles "
                                  "FROM jugador_estadisticas je "
                                  "JOIN jugador j ON je.jugador_id = j.id "
                                  "JOIN equipo e ON je.equipo_id = e.id "
                                  "WHERE je.torneo_id = ? "
                                  "ORDER BY je.asistencias DESC, je.goles DESC;";

    if (sqlite3_prepare_v2(db, sql_asistidores, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        int posicion = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            // int jugador_id = sqlite3_column_int(stmt, 0); // Unused variable removed
            const char *jugador = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo = (const char*)sqlite3_column_text(stmt, 2);
            int asistencias = sqlite3_column_int(stmt, 3);
            int goles = sqlite3_column_int(stmt, 4);

            printf("%-4d %-20s %-20s %-6d %-6d\n",
                   posicion++, jugador, equipo, asistencias, goles);
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * Muestra equipos disponibles para el torneo.
 * Retorna 0 si no hay equipos o cancelado, 1 si hay equipos.
 */
static int show_equipos_torneo(int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT e.id, e.nombre FROM equipo e "
                      "JOIN equipo_torneo et ON e.id = et.equipo_id "
                      "WHERE et.torneo_id = ? ORDER BY e.nombre;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al obtener equipos: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    printf("\n=== EQUIPOS DEL TORNEO ===\n\n");

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        int equipo_id = sqlite3_column_int(stmt, 0);
        const char *equipo_nombre = (const char*)sqlite3_column_text(stmt, 1);
        printf("%d. %s\n", equipo_id, equipo_nombre);
    }
    sqlite3_finalize(stmt);

    if (!found)
    {
        printf("No hay equipos asociados a este torneo.\n");
        pause_console();
        return 0;
    }

    return 1;
}

/**
 * Verifica si un equipo pertenece al torneo.
 */
static int validar_equipo_torneo(int torneo_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM equipo_torneo WHERE torneo_id = ? AND equipo_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

/**
 * Muestra jugadores de un equipo.
 * Retorna 0 si no hay jugadores, 1 si hay.
 */
static int show_jugadores_equipo(int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM jugador WHERE equipo_id = ? ORDER BY numero;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al obtener jugadores: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    printf("\n=== JUGADORES DEL EQUIPO ===\n\n");

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        int jugador_id = sqlite3_column_int(stmt, 0);
        const char *jugador_nombre = (const char*)sqlite3_column_text(stmt, 1);
        printf("%d. %s\n", jugador_id, jugador_nombre);
    }
    sqlite3_finalize(stmt);

    if (!found)
    {
        printf("No hay jugadores registrados para este equipo.\n");
        pause_console();
        return 0;
    }

    return 1;
}

/**
 * Verifica si un jugador pertenece a un equipo.
 */
static int validar_jugador_equipo(int jugador_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM jugador WHERE id = ? AND equipo_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, jugador_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

/**
 * Verifica si ya existe registro de estadísticas para el jugador en el torneo.
 */
static int existe_estadistica_jugador(int jugador_id, int torneo_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM jugador_estadisticas "
                      "WHERE jugador_id = ? AND torneo_id = ? AND equipo_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, jugador_id);
    sqlite3_bind_int(stmt, 2, torneo_id);
    sqlite3_bind_int(stmt, 3, equipo_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

/**
 * Inserta nuevo registro de estadísticas de jugador.
 */
static void insertar_estadistica_jugador(int jugador_id, int torneo_id, int equipo_id,
        int goles, int asistencias, int ta, int tr, int minutos)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO jugador_estadisticas "
                      "(jugador_id, torneo_id, equipo_id, goles, asistencias, "
                      "tarjetas_amarillas, tarjetas_rojas, minutos_jugados) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar inserción: %s\n", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_int(stmt, 1, jugador_id);
    sqlite3_bind_int(stmt, 2, torneo_id);
    sqlite3_bind_int(stmt, 3, equipo_id);
    sqlite3_bind_int(stmt, 4, goles);
    sqlite3_bind_int(stmt, 5, asistencias);
    sqlite3_bind_int(stmt, 6, ta);
    sqlite3_bind_int(stmt, 7, tr);
    sqlite3_bind_int(stmt, 8, minutos);

    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        printf("Registro agregado exitosamente a las tablas.\n");
    }
    else
    {
        printf("Error al agregar el registro: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

void agregar_registro_goleador_asistidor(int torneo_id)
{
    clear_screen();
    print_header("AGREGAR REGISTRO A TABLAS");

    if (!show_equipos_torneo(torneo_id)) return;

    int equipo_id = input_int("\nIngrese el ID del equipo (0 para cancelar): ");
    if (equipo_id == 0) return;

    if (!validar_equipo_torneo(torneo_id, equipo_id))
    {
        printf("El equipo seleccionado no pertenece a este torneo.\n");
        pause_console();
        return;
    }

    if (!show_jugadores_equipo(equipo_id)) return;

    int jugador_id = input_int("\nIngrese el ID del jugador (0 para cancelar): ");
    if (jugador_id == 0) return;

    if (!validar_jugador_equipo(jugador_id, equipo_id))
    {
        printf("El jugador seleccionado no pertenece a este equipo.\n");
        pause_console();
        return;
    }

    printf("\nIngrese las estadisticas para el jugador:\n");
    int goles = input_int("Goles anotados: ");
    int asistencias = input_int("Asistencias: ");
    int ta = input_int("Tarjetas amarillas: ");
    int tr = input_int("Tarjetas rojas: ");
    int minutos = input_int("Minutos jugados: ");

    if (existe_estadistica_jugador(jugador_id, torneo_id, equipo_id))
    {
        printf("Ya existe un registro para este jugador en este torneo.\n");
        printf("Use la opción de modificar para actualizar las estadísticas.\n");
        pause_console();
        return;
    }

    insertar_estadistica_jugador(jugador_id, torneo_id, equipo_id, goles, asistencias, ta, tr, minutos);
    pause_console();
}

/**
 * @brief Elimina un registro de las tablas de goleadores y asistidores
 *
 * @param torneo_id ID del torneo
 */
void eliminar_registro_goleador_asistidor(int torneo_id)
{
    clear_screen();
    print_header("ELIMINAR REGISTRO DE TABLAS");

    // Mostrar registros existentes
    printf("=== REGISTROS EXISTENTES ===\n\n");

    const char *sql_registros = "SELECT je.id, j.nombre, e.nombre as equipo, je.goles, je.asistencias "
                                "FROM jugador_estadisticas je "
                                "JOIN jugador j ON je.jugador_id = j.id "
                                "JOIN equipo e ON je.equipo_id = e.id "
                                "WHERE je.torneo_id = ? "
                                "ORDER BY je.goles DESC, je.asistencias DESC;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql_registros, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int registro_id = sqlite3_column_int(stmt, 0);
            const char *jugador = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo = (const char*)sqlite3_column_text(stmt, 2);
            int goles = sqlite3_column_int(stmt, 3);
            int asistencias = sqlite3_column_int(stmt, 4);

            printf("%d. %s (%s) - Goles: %d, Asistencias: %d\n",
                   registro_id, jugador, equipo, goles, asistencias);
        }

        if (!found)
        {
            printf("No hay registros en las tablas para este torneo.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }

    int registro_id = input_int("\nIngrese el ID del registro a eliminar (0 para cancelar): ");
    if (registro_id == 0) return;

    // Verificar que el registro existe y pertenece al torneo
    const char *sql_verificar = "SELECT COUNT(*) FROM jugador_estadisticas "
                                "WHERE id = ? AND torneo_id = ?;";
    int registro_valido = 0;
    if (sqlite3_prepare_v2(db, sql_verificar, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, registro_id);
        sqlite3_bind_int(stmt, 2, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            registro_valido = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (!registro_valido)
    {
        printf("ID de registro invalido.\n");
        pause_console();
        return;
    }

    if (confirmar("¿Está seguro que desea eliminar este registro?"))
    {
        const char *sql_delete = "DELETE FROM jugador_estadisticas WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, registro_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Registro eliminado exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar el registro: %s\n", sqlite3_errmsg(db));
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
 * @brief Modifica un registro de las tablas de goleadores y asistidores
 *
 * @param torneo_id ID del torneo
 */
void modificar_registro_goleador_asistidor(int torneo_id)
{
    clear_screen();
    print_header("MODIFICAR REGISTRO DE TABLAS");

    // Mostrar registros existentes
    printf("=== REGISTROS EXISTENTES ===\n\n");

    const char *sql_registros = "SELECT je.id, j.nombre, e.nombre as equipo, je.goles, je.asistencias "
                                "FROM jugador_estadisticas je "
                                "JOIN jugador j ON je.jugador_id = j.id "
                                "JOIN equipo e ON je.equipo_id = e.id "
                                "WHERE je.torneo_id = ? "
                                "ORDER BY je.goles DESC, je.asistencias DESC;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql_registros, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int registro_id = sqlite3_column_int(stmt, 0);
            const char *jugador = (const char*)sqlite3_column_text(stmt, 1);
            const char *equipo = (const char*)sqlite3_column_text(stmt, 2);
            int goles = sqlite3_column_int(stmt, 3);
            int asistencias = sqlite3_column_int(stmt, 4);

            printf("%d. %s (%s) - Goles: %d, Asistencias: %d\n",
                   registro_id, jugador, equipo, goles, asistencias);
        }

        if (!found)
        {
            printf("No hay registros en las tablas para este torneo.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }

    int registro_id = input_int("\nIngrese el ID del registro a modificar (0 para cancelar): ");
    if (registro_id == 0) return;

    // Verificar que el registro existe y pertenece al torneo
    const char *sql_verificar = "SELECT COUNT(*) FROM jugador_estadisticas "
                                "WHERE id = ? AND torneo_id = ?;";
    int registro_valido = 0;
    if (sqlite3_prepare_v2(db, sql_verificar, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, registro_id);
        sqlite3_bind_int(stmt, 2, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            registro_valido = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (!registro_valido)
    {
        printf("ID de registro invalido.\n");
        pause_console();
        return;
    }

    // Obtener datos actuales del registro
    const char *sql_datos = "SELECT goles, asistencias, tarjetas_amarillas, tarjetas_rojas, minutos_jugados "
                            "FROM jugador_estadisticas WHERE id = ?;";
    int goles_actual = 0, asistencias_actual = 0, ta_actual = 0, tr_actual = 0, minutos_actual = 0;
    if (sqlite3_prepare_v2(db, sql_datos, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, registro_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            goles_actual = sqlite3_column_int(stmt, 0);
            asistencias_actual = sqlite3_column_int(stmt, 1);
            ta_actual = sqlite3_column_int(stmt, 2);
            tr_actual = sqlite3_column_int(stmt, 3);
            minutos_actual = sqlite3_column_int(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }

    printf("\nDatos actuales:\n");
    printf("Goles: %d\n", goles_actual);
    printf("Asistencias: %d\n", asistencias_actual);
    printf("Tarjetas amarillas: %d\n", ta_actual);
    printf("Tarjetas rojas: %d\n", tr_actual);
    printf("Minutos jugados: %d\n", minutos_actual);

    printf("\nIngrese los nuevos valores (deje en 0 para mantener el valor actual):\n");
    printf("Goles anotados (actual: %d): ", goles_actual);
    int nuevos_goles = input_int("");
    printf("Asistencias (actual: %d): ", asistencias_actual);
    int nuevas_asistencias = input_int("");
    printf("Tarjetas amarillas (actual: %d): ", ta_actual);
    int nuevas_ta = input_int("");
    printf("Tarjetas rojas (actual: %d): ", tr_actual);
    int nuevas_tr = input_int("");
    printf("Minutos jugados (actual: %d): ", minutos_actual);
    int nuevos_minutos = input_int("");

    // Actualizar solo los valores que se modificaron
    if (nuevos_goles == 0) nuevos_goles = goles_actual;
    if (nuevas_asistencias == 0) nuevas_asistencias = asistencias_actual;
    if (nuevas_ta == 0) nuevas_ta = ta_actual;
    if (nuevas_tr == 0) nuevas_tr = tr_actual;
    if (nuevos_minutos == 0) nuevos_minutos = minutos_actual;

    const char *sql_update = "UPDATE jugador_estadisticas SET "
                             "goles = ?, asistencias = ?, "
                             "tarjetas_amarillas = ?, tarjetas_rojas = ?, minutos_jugados = ? "
                             "WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, nuevos_goles);
        sqlite3_bind_int(stmt, 2, nuevas_asistencias);
        sqlite3_bind_int(stmt, 3, nuevas_ta);
        sqlite3_bind_int(stmt, 4, nuevas_tr);
        sqlite3_bind_int(stmt, 5, nuevos_minutos);
        sqlite3_bind_int(stmt, 6, registro_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Registro modificado exitosamente.\n");
        }
        else
        {
            printf("Error al modificar el registro: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra el menú principal de gestión de torneos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar torneos. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_torneos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_torneo},
        {2, "Listar", listar_torneos},
        {3, "Modificar", modificar_torneo},
        {4, "Eliminar", eliminar_torneo},
        {5, "Administrar", administrar_torneo},
        {0, "Volver", NULL}
    };

    ejecutar_menu("TORNEOS", items, 6);
}
