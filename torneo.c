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

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

// Forward declaration to fix implicit function declaration
void generar_fixture(int torneo_id);
void gestionar_tablas_goleadores_asistidores();
void listar_tablas_goleadores_asistidores(int torneo_id);
void agregar_registro_goleador_asistidor(int torneo_id);
void eliminar_registro_goleador_asistidor(int torneo_id);
void modificar_registro_goleador_asistidor(int torneo_id);

// Prototipos estáticos para funciones usadas antes de su definición
static void listar_equipos_asociados(int torneo_id);
static void actualizar_nombre_torneo(int torneo_id);
static void actualizar_equipo_fijo(int torneo_id);
static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad);

// Stubs para funciones no implementadas pero usadas
static void actualizar_cantidad_equipos(int torneo_id)
{
    (void)torneo_id;
    printf("Actualizar cantidad de equipos no completamente implementado.\n");
}

static void aplicar_actualizacion_formato(int torneo_id, int tipo, int formato)
{
    (void)torneo_id;
    (void)tipo;
    (void)formato;
    printf("Aplicar actualización de formato no completamente implementado.\n");
}

static void actualizar_formato_4_6_equipos(int torneo_id)
{
    (void)torneo_id;
    printf("Actualizar formato 4-6 equipos no completamente implementado.\n");
}

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

    if (preparar_stmt(sql, &stmt))
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
            mostrar_no_hay_registros("equipos registrados para asociar");
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
    if (preparar_stmt(sql_check, &stmt))
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
    if (preparar_stmt(sql_insert, &stmt))
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
    crear_equipo();

    sqlite3_stmt *stmt;
    const char *sql = "SELECT last_insert_rowid();";

    int equipo_id = -1;
    if (preparar_stmt(sql, &stmt))
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

    if (torneo_id != -1)
    {
        const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (preparar_stmt(sql_insert, &stmt))
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
            return 0;
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
static int save_torneo_to_db(Torneo const *torneo)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO torneo (nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo) VALUES (?, ?, ?, ?, ?, ?);";

    if (!preparar_stmt(sql, &stmt))
    {
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

    int torneo_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    if (torneo->tiene_equipo_fijo && torneo->equipo_fijo_id != -1)
    {
        const char *sql_asociar = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (preparar_stmt(sql_asociar, &stmt))
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

    if (preparar_stmt(sql, &stmt))
    {
        printf("\n=== LISTA DE TORNEOS ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            Torneo torneo;
            torneo.id = sqlite3_column_int(stmt, 0);
            strncpy_s(torneo.nombre, sizeof(torneo.nombre), (const char*)sqlite3_column_text(stmt, 1), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 2);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 3);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 4);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 5);
            torneo.formato_torneo = sqlite3_column_int(stmt, 6);

            mostrar_torneo(&torneo);
            listar_equipos_asociados(torneo.id);

            printf("----------------------------------------\n");
        }

        if (!found)
        {
            mostrar_no_hay_registros("torneos registrados");
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

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (preparar_stmt(sql, &stmt))
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
            mostrar_no_hay_registros("torneos registrados para modificar");
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

    Torneo torneo = {0};
    const char *sql_torneo = "SELECT nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo WHERE id = ?;";

    if (preparar_stmt(sql_torneo, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy_s(torneo.nombre, sizeof(torneo.nombre), (const char*)sqlite3_column_text(stmt, 0), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 1);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 2);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 3);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 4);
            torneo.formato_torneo = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }

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
        actualizar_nombre_torneo(torneo_id);
        break;
    case 2:
        actualizar_equipo_fijo(torneo_id);
        break;
    case 3:
        actualizar_cantidad_equipos(torneo_id);
        break;
    case 4:
        actualizar_tipo_formato_torneo(torneo_id, torneo.cantidad_equipos);
        break;
    case 5:
        asociar_equipos_torneo(torneo_id);
        break;
    case 6:
        break;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

void eliminar_torneo()
{
    clear_screen();
    print_header("ELIMINAR TORNEO");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (preparar_stmt(sql, &stmt))
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
            mostrar_no_hay_registros("torneos registrados para eliminar");
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

    if (!confirmar("¿Está seguro de eliminar este torneo? Se eliminarán datos asociados."))
    {
        printf("Eliminación cancelada.\n");
        pause_console();
        return;
    }

    const char *sqls[] =
    {
        "DELETE FROM equipo_fase WHERE torneo_id = ?;",
        "DELETE FROM torneo_fases WHERE torneo_id = ?;",
        "DELETE FROM jugador_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM partido_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_historial WHERE torneo_id = ?;",
        "DELETE FROM torneo_temporada WHERE torneo_id = ?;",
        "DELETE FROM torneo WHERE id = ?;",
        NULL
    };

    for (int i = 0; sqls[i] != NULL; i++)
    {
        if (preparar_stmt(sqls[i], &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    printf("Torneo eliminado exitosamente.\n");
    pause_console();
}

/**
 * @brief Actualiza el nombre del torneo en la base de datos
 */
static void actualizar_nombre_torneo(int torneo_id)
{
    char nuevo_nombre[50];
    input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE torneo SET nombre = ? WHERE id = ?;";
    if (preparar_stmt(sql_update, &stmt))
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
}

/**
 * @brief Muestra lista de equipos disponibles y retorna ID seleccionado
 * @return ID del equipo seleccionado, o 0 si se cancela
 */
static int seleccionar_equipo_disponible(void)
{
    sqlite3_stmt *stmt_equipos;
    const char *sql_equipos = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (!preparar_stmt(sql_equipos, &stmt_equipos))
    {
        pause_console();
        return 0;
    }

    printf("\n=== EQUIPOS DISPONIBLES ===\n\n");

    int found = 0;
    while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
    {
        found = 1;
        int id = sqlite3_column_int(stmt_equipos, 0);
        const char *nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
        printf("%d. %s\n", id, nombre);
    }

    sqlite3_finalize(stmt_equipos);

    if (!found)
    {
        mostrar_no_hay_registros("equipos registrados");
        pause_console();
        return 0;
    }

    int equipo_id = input_int("\nIngrese el ID del equipo fijo (0 para cancelar): ");
    
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

/**
 * @brief Actualiza el equipo fijo del torneo
 */
static void actualizar_equipo_fijo(int torneo_id)
{
    int nuevo_tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

    if (!nuevo_tiene_equipo_fijo)
    {
        sqlite3_stmt *stmt;
        const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = 0, equipo_fijo_id = -1 WHERE id = ?;";
        if (preparar_stmt(sql_update, &stmt))
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
        return;
    }

    int equipo_id = seleccionar_equipo_disponible();
    if (equipo_id == 0) return;

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = ?, equipo_fijo_id = ? WHERE id = ?;";
    if (preparar_stmt(sql_update, &stmt))
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

/**
 * @brief Obtiene formato y tipo para 7-12 equipos según opción seleccionada
 */
static void obtener_formato_7_12(int opcion, TipoTorneos *tipo, FormatoTorneos *formato)
{
    *tipo = SOLO_IDA;
    *formato = LIGA_SIMPLE;

    switch (opcion)
    {
    case 1:
        break;
    case 2:
        *formato = LIGA_DOBLE;
        *tipo = IDA_Y_VUELTA;
        break;
    case 3:
        *formato = GRUPOS_CON_FINAL;
        *tipo = GRUPOS_Y_ELIMINACION;
        break;
    case 4:
        *formato = COPA_SIMPLE;
        *tipo = ELIMINACION_DIRECTA;
        break;
    default:
        printf("Opcion invalida. Se seleccionará Liga simple por defecto.\n");
    }
}

/**
 * @brief Maneja la actualización de formato para 7-12 equipos
 */
static void actualizar_formato_7_12_equipos(int torneo_id)
{
    printf("\nPara 7-12 equipos, seleccione el formato:\n");
    printf("1. Liga simple\n");
    printf("2. Liga doble\n");
    printf("3. Grupos + final\n");
    printf("4. Copa simple\n");

    int opcion = input_int(">");
    TipoTorneos tipo;
    FormatoTorneos formato;

    obtener_formato_7_12(opcion, &tipo, &formato);
    aplicar_actualizacion_formato(torneo_id, tipo, formato);
}

/**
 * @brief Obtiene formato y tipo para 13-20 equipos según opción seleccionada
 */
static void obtener_formato_13_20(int opcion, TipoTorneos *tipo, FormatoTorneos *formato)
{
    *tipo = GRUPOS_Y_ELIMINACION;
    *formato = GRUPOS_ELIMINACION;

    switch (opcion)
    {
    case 1:
        break;
    case 2:
        *formato = COPA_REPECHAJE;
        *tipo = ELIMINACION_DIRECTA;
        break;
    case 3:
        *formato = LIGA_GRANDE;
        *tipo = IDA_Y_VUELTA;
        break;
    default:
        printf("Opcion invalida. Se seleccionará Grupos + eliminacion por defecto.\n");
    }
}

/**
 * @brief Maneja la actualización de formato para 13-20 equipos
 */
static void actualizar_formato_13_20_equipos(int torneo_id)
{
    printf("\nPara 13-20 equipos, seleccione el formato:\n");
    printf("1. Grupos (4-5 grupos) + eliminacion\n");
    printf("2. Copa + repechaje\n");
    printf("3. Liga grande\n");

    int opcion = input_int(">");
    TipoTorneos tipo;
    FormatoTorneos formato;

    obtener_formato_13_20(opcion, &tipo, &formato);
    aplicar_actualizacion_formato(torneo_id, tipo, formato);
}

/**
 * @brief Obtiene formato y tipo para 21+ equipos según opción seleccionada
 */
static void obtener_formato_21(int opcion, TipoTorneos *tipo, FormatoTorneos *formato)
{
    *tipo = GRUPOS_Y_ELIMINACION;
    *formato = MULTIPLES_GRUPOS;

    switch (opcion)
    {
    case 1:
        break;
    case 2:
        *formato = ELIMINACION_FASES;
        *tipo = ELIMINACION_DIRECTA;
        break;
    default:
        printf("Opcion invalida. Se seleccionará Multiples grupos por defecto.\n");
    }
}

/**
 * @brief Maneja la actualización de formato para 21+ equipos
 */
static void actualizar_formato_21_equipos(int torneo_id)
{
    printf("\nPara 21 o mas equipos, seleccione el formato:\n");
    printf("1. Multiples grupos\n");
    printf("2. Eliminacion directa por fases\n");

    int opcion = input_int(">");
    TipoTorneos tipo;
    FormatoTorneos formato;

    obtener_formato_21(opcion, &tipo, &formato);
    aplicar_actualizacion_formato(torneo_id, tipo, formato);
}

/**
 * @brief Maneja la actualización de tipo y formato de torneo
 */
static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad)
{
    if (cantidad >= 4 && cantidad <= 6)
    {
        actualizar_formato_4_6_equipos(torneo_id);
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        actualizar_formato_7_12_equipos(torneo_id);
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        actualizar_formato_13_20_equipos(torneo_id);
    }
    else if (cantidad >= 21)
    {
        actualizar_formato_21_equipos(torneo_id);
    }
    else
    {
        printf("Cantidad de equipos no válida. No se actualizara el formato.\n");
    }
}

/**
 * @brief Muestra los equipos asociados a un torneo
 */
static void listar_equipos_asociados(int torneo_id)
{
    printf("=== EQUIPOS ASOCIADOS ===\n");
    sqlite3_stmt *stmt_equipos;
    const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e JOIN equipo_torneo et ON e.id = et.equipo_id WHERE et.torneo_id = ? ORDER BY e.id;";

    if (!preparar_stmt(sql_equipos, &stmt_equipos))
    {
        return;
    }

    sqlite3_bind_int(stmt_equipos, 1, torneo_id);

    int has_equipos = 0;
    int count = 1;
    while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
    {
        has_equipos = 1;
        const char *equipo_nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
        printf("%d. %s\n", count++, equipo_nombre);
    }

    if (!has_equipos)
    {
        mostrar_no_hay_registros("equipos asociados a este torneo");
    }

    sqlite3_finalize(stmt_equipos);
}
/**
 * @brief Obtiene el nombre de un equipo por su ID
 * @param equipo_id ID del equipo
 * @return Nombre del equipo o "Desconocido"
 */
const char* get_equipo_nombre(int equipo_id)
{
    static char nombre[50];
    nombre[0] = '\0';
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM equipo WHERE id = ?;";
    
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *text = sqlite3_column_text(stmt, 0);
            if (text)
            {
                strncpy_s(nombre, sizeof(nombre), (const char *)text, sizeof(nombre) - 1);
            }
            else
            {
                strcpy_s(nombre, sizeof(nombre), "Desconocido");
            }
        }
        else
        {
            strcpy_s(nombre, sizeof(nombre), "Desconocido");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        strcpy_s(nombre, sizeof(nombre), "Desconocido");
    }
    
    return nombre;
}

/**
 * @brief Menú principal de gestión de torneos
 */
void menu_torneos()
{
    MenuItem items[] =
    {
        {1, "Crear Torneo", crear_torneo},
        {2, "Listar Torneos", listar_torneos},
        {3, "Modificar Torneo", modificar_torneo},
        {4, "Eliminar Torneo", eliminar_torneo},
        {0, "Volver", NULL}
    };

    ejecutar_menu("TORNEOS", items, 5);
}