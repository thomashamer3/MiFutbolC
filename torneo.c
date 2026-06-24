#include "torneo.h"
#include "db.h"
#include "equipo.h"
#include "menu.h"
#include "sqlite3.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return db_prepare_stmt(stmt, sql);
}

// Forward declaration to fix implicit function declaration
void generar_fixture(int torneo_id);
void gestionar_tablas_goleadores_asistidores(void);
void listar_tablas_goleadores_asistidores(int torneo_id);
void agregar_registro_goleador_asistidor(int torneo_id);
void eliminar_registro_goleador_asistidor(int torneo_id);
void modificar_registro_goleador_asistidor(int torneo_id);

// Prototipos estaticos para funciones usadas antes de su definicion
static void listar_equipos_asociados(int torneo_id);
static void actualizar_nombre_torneo(int torneo_id);
static void actualizar_equipo_fijo(int torneo_id);
static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad);

static void actualizar_cantidad_equipos(int torneo_id)
{
    int nueva_cantidad = input_int("Ingrese la nueva cantidad de equipos: ");
    if (nueva_cantidad < 2)
    {
        printf("La cantidad minima de equipos es 2.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE torneo SET cantidad_equipos = ? WHERE id = ?;";
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, nueva_cantidad);
        sqlite3_bind_int(stmt, 2, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Cantidad de equipos actualizada a %d.\n", nueva_cantidad);
        }
        else
        {
            printf("Error al actualizar cantidad de equipos: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    pause_console();
}

static void aplicar_actualizacion_formato(int torneo_id, int tipo, int formato)
{
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE torneo SET tipo_torneo = ?, formato_torneo = ? WHERE id = ?;";
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, tipo);
        sqlite3_bind_int(stmt, 2, formato);
        sqlite3_bind_int(stmt, 3, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Tipo y formato de torneo actualizados exitosamente.\n");
        }
        else
        {
            printf("Error al actualizar formato: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    pause_console();
}

/**
 * Traduce valores enumerados de tipos de torneo a nombres legibles para la interfaz de usuario,
 * facilitando la comprension de las opciones disponibles.
 */
const char *get_nombre_tipo_torneo(TipoTorneos tipo)
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

const char *get_nombre_formato_torneo(FormatoTorneos formato)
{
    static const char *formatos[] = {"Round-robin (sistema liga)",
                                     "Mini grupo con final",
                                     "Liga simple",
                                     "Liga doble",
                                     "Grupos + final",
                                     "Copa simple",
                                     "Grupos + eliminacion",
                                     "Copa + repechaje",
                                     "Liga grande",
                                     "Multiples grupos",
                                     "Eliminacion directa por fases"
                                    };
    int idx = (int)formato;
    if (idx >= 0 && idx < (int)(sizeof(formatos) / sizeof(formatos[0])))
    {
        return formatos[idx];
    }
    return "Desconocido";
}

static int listar_torneos_generico(const char *no_records_msg)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    ui_printf_centered_line("=== TORNEOS DISPONIBLES ===");
    ui_printf("\n");

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        ui_printf_centered_line("%d. %s", id, nombre);
    }

    sqlite3_finalize(stmt);

    if (!found)
    {
        mostrar_no_hay_registros(no_records_msg);
        return 0;
    }

    return 1;
}

static void asignar_formato_4_a_6(int opcion, TipoTorneos *tipo, FormatoTorneos *formato)
{
    if (opcion == 1)
    {
        *formato = ROUND_ROBIN;
        *tipo = IDA_Y_VUELTA;
    }
    else if (opcion == 2)
    {
        *formato = MINI_GRUPO_CON_FINAL;
        *tipo = GRUPOS_Y_ELIMINACION;
    }
}

static void asignar_formato_7_a_12(int opcion, TipoTorneos *tipo, FormatoTorneos *formato)
{
    switch (opcion)
    {
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
        break;
    }
}

static void asignar_formato_13_a_20(int opcion, TipoTorneos *tipo, FormatoTorneos *formato)
{
    *tipo = GRUPOS_Y_ELIMINACION;
    *formato = GRUPOS_ELIMINACION;

    switch (opcion)
    {
    case 2:
        *formato = COPA_REPECHAJE;
        *tipo = ELIMINACION_DIRECTA;
        break;
    case 3:
        *formato = LIGA_GRANDE;
        *tipo = IDA_Y_VUELTA;
        break;
    default:
        break;
    }
}

static void asignar_formato_21_o_mas(int opcion, TipoTorneos *tipo, FormatoTorneos *formato)
{
    *tipo = GRUPOS_Y_ELIMINACION;
    *formato = MULTIPLES_GRUPOS;

    if (opcion == 2)
    {
        *formato = ELIMINACION_FASES;
        *tipo = ELIMINACION_DIRECTA;
    }
}

static void obtener_formato_por_cantidad(int opcion, int cantidad, TipoTorneos *tipo,
        FormatoTorneos *formato)
{
    *tipo = SOLO_IDA;
    *formato = LIGA_SIMPLE;

    if (cantidad >= 4 && cantidad <= 6)
    {
        asignar_formato_4_a_6(opcion, tipo, formato);
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        asignar_formato_7_a_12(opcion, tipo, formato);
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        asignar_formato_13_a_20(opcion, tipo, formato);
    }
    else if (cantidad >= 21)
    {
        asignar_formato_21_o_mas(opcion, tipo, formato);
    }
}

/**
 * Muestra informacion completa de torneo para confirmacion del usuario.
 * Necesario porque la estructura interna no es legible para humanos.
 */
void mostrar_torneo(Torneo *torneo)
{
    ui_printf_centered_line("=== INFORMACION DEL TORNEO ===");
    ui_printf_centered_line("Nombre: %s", torneo->nombre);
    ui_printf_centered_line("Tiene equipo fijo: %s", torneo->tiene_equipo_fijo ? "Si" : "No");
    if (torneo->tiene_equipo_fijo)
    {
        ui_printf_centered_line("Equipo fijo ID: %d", torneo->equipo_fijo_id);
    }
    ui_printf_centered_line("Cantidad de equipos: %d", torneo->cantidad_equipos);
    ui_printf_centered_line("Tipo de torneo: %s", get_nombre_tipo_torneo(torneo->tipo_torneo));
    ui_printf_centered_line("Formato de torneo: %s",
                            get_nombre_formato_torneo(torneo->formato_torneo));
    ui_printf("\n");
}

static void agregar_equipo_nombre_torneo(int torneo_id)
{
    char nombre[50] = {0};
    input_string("Nombre del equipo: ", nombre, sizeof(nombre));
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_check =
        "SELECT COUNT(*) FROM equipo_torneo_nombre WHERE torneo_id = ? AND nombre = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0)
        {
            printf("Ya existe un equipo con ese nombre en el torneo.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }

    const char *sql_insert = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Equipo '%s' agregado al torneo.\n", nombre);
        }
        else
        {
            printf("Error al agregar equipo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    pause_console();
}

static void agregar_equipos_nombres_torneo(int torneo_id)
{
    clear_screen();
    print_header("AGREGAR EQUIPOS POR NOMBRE");
    printf("Escriba los nombres de los equipos (uno por linea, linea vacia para terminar):\n\n");

    int count = 0;
    for (;;)
    {
        char nombre[50] = {0};
        printf("Equipo %d (Enter para terminar): ", count + 1);
        if (fgets(nombre, sizeof(nombre), stdin))
        {
            // trim newline
            size_t len = strlen_s(nombre, sizeof(nombre));
            while (len > 0 && (nombre[len - 1] == '\n' || nombre[len - 1] == '\r'))
            {
                nombre[--len] = '\0';
            }
        }
        if (nombre[0] == '\0')
        {
            break;
        }

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                count++;
            }
            else
            {
                printf("Error al agregar '%s': %s\n", nombre, sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    printf("\n%d equipo(s) agregado(s) al torneo.\n", count);
    pause_console();
}

void asociar_equipos_torneo(int torneo_id)
{
    clear_screen();
    print_header("ASOCIAR EQUIPOS A TORNEO");
    sqlite3_stmt *stmt;
    int equipo_id = select_team_id("\nIngrese el ID del equipo a asociar (0 para cancelar): ",
                                   "equipos registrados para asociar", 1);
    if (equipo_id == 0)
    {
        return;
    }

    // Verificar si ya esta asociado
    const char *sql_check =
        "SELECT COUNT(*) FROM equipo_torneo WHERE torneo_id = ? AND equipo_id = ?;";
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
            mostrar_alerta_operacion("Torneo", "Equipo Asociado", NULL);
        }
        else
        {
            printf("Error al asociar equipo al torneo: %s\n", sqlite3_errmsg(db));
            pause_console();
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        pause_console();
    }
}

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
 * Solicita al usuario los datos basicos del torneo (nombre, equipo fijo).
 * Maneja la creacion de equipo fijo si es necesario.
 * Retorna 0 si se debe cancelar la creacion, 1 si continua.
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
 * Utiliza logica de rangos para simplificar la seleccion automatica.
 */
/* Helper: mostrar opciones y obtener la opcion segun la cantidad de equipos */
static int prompt_formato_por_cantidad(int cantidad)
{
    int opcion = -1;
    if (cantidad >= 4 && cantidad <= 6)
    {
        printf("\nPara 4-6 equipos, seleccione el formato:\n");
        printf("1. Round-robin (sistema liga)\n");
        printf("2. Mini grupo con final\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        printf("\nPara 7-12 equipos, seleccione el formato:\n");
        printf("1. Liga simple\n");
        printf("2. Liga doble\n");
        printf("3. Grupos + final\n");
        printf("4. Copa simple\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        printf("\nPara 13-20 equipos, seleccione el formato:\n");
        printf("1. Grupos (4-5 grupos) + eliminacion\n");
        printf("2. Copa + repechaje\n");
        printf("3. Liga grande\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 21)
    {
        printf("\nPara 21 o mas equipos, seleccione el formato:\n");
        printf("1. Multiples grupos\n");
        printf("2. Eliminacion directa por fases\n");
        opcion = input_int(">");
    }
    return opcion;
}

static void determine_formato_torneo(Torneo *torneo)
{
    int cantidad = torneo->cantidad_equipos;
    int opcion = prompt_formato_por_cantidad(cantidad);
    if (opcion < 0)
    {
        printf("Cantidad de equipos no valida. Se seleccionara formato por defecto.\n");
        torneo->formato_torneo = ROUND_ROBIN;
        torneo->tipo_torneo = IDA_Y_VUELTA;
        return;
    }
    obtener_formato_por_cantidad(opcion, cantidad, &torneo->tipo_torneo, &torneo->formato_torneo);
}

/**
 * Guarda el torneo en la base de datos y maneja asociaciones iniciales.
 * Retorna el ID del torneo creado o -1 si hay error.
 */
static int save_torneo_to_db(Torneo const *torneo)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO torneo (nombre, tiene_equipo_fijo, equipo_fijo_id, "
                      "cantidad_equipos, tipo_torneo, formato_torneo) VALUES (?, ?, ?, ?, ?, ?);";

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

void crear_torneo(void)
{
    clear_screen();
    print_header("CREAR TORNEO");

    Torneo torneo = {0};
    torneo.tiene_equipo_fijo = 0;
    torneo.equipo_fijo_id = -1;

    if (!input_torneo_data(&torneo))
    {
        return;
    }

    determine_formato_torneo(&torneo);

    clear_screen();
    mostrar_torneo(&torneo);

    int torneo_id = save_torneo_to_db(&torneo);
    if (torneo_id == -1)
    {
        return;
    }

    char info[100];
    snprintf(info, sizeof(info), "%.*s - ID: %d", (int)(sizeof(info) - 20), torneo.nombre,
             torneo_id);
    mostrar_alerta_operacion("Torneo", "Guardado", info);

    printf("\nAgregar equipos al torneo:\n");
    printf("1. Agregar equipos por nombre (solo nombres para llaves)\n");
    printf("2. Asociar equipos guardados\n");
    printf("0. Continuar sin agregar\n");
    int opc_eq = input_int(">");
    if (opc_eq == 1)
    {
        agregar_equipos_nombres_torneo(torneo_id);
    }
    else if (opc_eq == 2)
    {
        asociar_equipos_torneo(torneo_id);
    }

    pause_console();
}

void listar_torneos(void)
{
    clear_screen();
    print_header("LISTAR TORNEOS");

    if (!listar_torneos_generico("torneos registrados"))
    {
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, "
                      "tipo_torneo, formato_torneo FROM torneo ORDER BY id;";

    if (preparar_stmt(sql, &stmt))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Torneo torneo;
            torneo.id = sqlite3_column_int(stmt, 0);
            strncpy_s(torneo.nombre, sizeof(torneo.nombre),
                      (const char *)sqlite3_column_text(stmt, 1), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 2);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 3);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 4);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 5);
            torneo.formato_torneo = sqlite3_column_int(stmt, 6);

            mostrar_torneo(&torneo);
            listar_equipos_asociados(torneo.id);

            printf("----------------------------------------\n");
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void modificar_torneo(void)
{
    clear_screen();
    print_header("MODIFICAR TORNEO");

    if (!listar_torneos_generico("torneos registrados para modificar"))
    {
        pause_console();
        return;
    }

    int torneo_id = input_int("\nIngrese el ID del torneo a modificar (0 para cancelar): ");

    if (torneo_id == 0)
    {
        return;
    }

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    Torneo torneo = {0};
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, "
                             "tipo_torneo, formato_torneo FROM torneo WHERE id = ?;";

    if (preparar_stmt(sql_torneo, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy_s(torneo.nombre, sizeof(torneo.nombre),
                      (const char *)sqlite3_column_text(stmt, 0), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 1);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 2);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 3);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 4);
            torneo.formato_torneo = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }

    printf("\nSeleccione que desea modificar:\n");
    printf("1. Nombre del torneo\n");
    printf("2. Equipo fijo\n");
    printf("3. Cantidad de equipos\n");
    printf("4. Tipo y formato de torneo\n");
    printf("5. Asociar equipos guardados\n");
    printf("6. Agregar equipos por nombre\n");
    printf("7. Agregar un equipo por nombre\n");
    printf("8. Volver\n");

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
        agregar_equipos_nombres_torneo(torneo_id);
        break;
    case 7:
        agregar_equipo_nombre_torneo(torneo_id);
        break;
    case 8:
        break;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

void eliminar_torneo(void)
{
    clear_screen();
    print_header("ELIMINAR TORNEO");

    if (!listar_torneos_generico("torneos registrados para eliminar"))
    {
        pause_console();
        return;
    }

    int torneo_id = input_int("\nIngrese el ID del torneo a eliminar (0 para cancelar): ");

    if (torneo_id == 0)
    {
        return;
    }

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    if (!confirmar("Esta seguro de eliminar este torneo? Se eliminaran datos asociados."))
    {
        printf("Eliminacion cancelada.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sqls[] = {"DELETE FROM equipo_fase WHERE torneo_id = ?;",
                          "DELETE FROM torneo_fases WHERE torneo_id = ?;",
                          "DELETE FROM jugador_estadisticas WHERE torneo_id = ?;",
                          "DELETE FROM equipo_torneo_estadisticas WHERE torneo_id = ?;",
                          "DELETE FROM partido_torneo WHERE torneo_id = ?;",
                          "DELETE FROM equipo_torneo WHERE torneo_id = ?;",
                          "DELETE FROM equipo_torneo_nombre WHERE torneo_id = ?;",
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

    mostrar_alerta_operacion("Torneo", "Eliminado", NULL);
}

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
            mostrar_alerta_operacion("Torneo", "Nombre Actualizado", nuevo_nombre);
        }
        else
        {
            printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
            pause_console();
        }
        sqlite3_finalize(stmt);
    }
}

static int seleccionar_equipo_disponible(void)
{
    return select_team_id(
               "\nIngrese el ID del equipo fijo (0 para cancelar): ", "equipos registrados", 1);
}

static void actualizar_equipo_fijo(int torneo_id)
{
    int nuevo_tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

    if (!nuevo_tiene_equipo_fijo)
    {
        sqlite3_stmt *stmt;
        const char *sql_update =
            "UPDATE torneo SET tiene_equipo_fijo = 0, equipo_fijo_id = -1 WHERE id = ?;";
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
    if (equipo_id == 0)
    {
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_update =
        "UPDATE torneo SET tiene_equipo_fijo = ?, equipo_fijo_id = ? WHERE id = ?;";
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

static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad)
{
    int opcion = prompt_formato_por_cantidad(cantidad);
    if (opcion < 0)
    {
        printf("Cantidad de equipos no valida. No se actualizara el formato.\n");
        return;
    }
    TipoTorneos tipo;
    FormatoTorneos formato;
    obtener_formato_por_cantidad(opcion, cantidad, &tipo, &formato);
    aplicar_actualizacion_formato(torneo_id, tipo, formato);
}

static void listar_equipos_asociados(int torneo_id)
{
    printf("=== EQUIPOS ASOCIADOS ===\n");
    int has_equipos = 0;
    int count = 1;

    // Equipos guardados (de tabla equipo)
    sqlite3_stmt *stmt_equipos;
    const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e JOIN equipo_torneo et ON e.id = "
                              "et.equipo_id WHERE et.torneo_id = ? ORDER BY e.id;";
    if (preparar_stmt(sql_equipos, &stmt_equipos))
    {
        sqlite3_bind_int(stmt_equipos, 1, torneo_id);
        while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
        {
            has_equipos = 1;
            const char *equipo_nombre = (const char *)sqlite3_column_text(stmt_equipos, 1);
            printf("%d. %s\n", count++, equipo_nombre);
        }
        sqlite3_finalize(stmt_equipos);
    }

    // Equipos por nombre (solo nombre, sin equipo completo)
    sqlite3_stmt *stmt_nombres;
    const char *sql_nombres =
        "SELECT nombre FROM equipo_torneo_nombre WHERE torneo_id = ? ORDER BY id;";
    if (preparar_stmt(sql_nombres, &stmt_nombres))
    {
        sqlite3_bind_int(stmt_nombres, 1, torneo_id);
        while (sqlite3_step(stmt_nombres) == SQLITE_ROW)
        {
            has_equipos = 1;
            const char *nombre = (const char *)sqlite3_column_text(stmt_nombres, 0);
            printf("%d. %s\n", count++, nombre);
        }
        sqlite3_finalize(stmt_nombres);
    }

    if (!has_equipos)
    {
        mostrar_no_hay_registros("equipos asociados a este torneo");
    }
}

const char *get_equipo_nombre(int equipo_id)
{
    static char nombre[50];
    strcpy_s(nombre, sizeof(nombre), "Desconocido");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM equipo WHERE id = ?;";
    if (!preparar_stmt(sql, &stmt))
    {
        return nombre;
    }
    sqlite3_bind_int(stmt, 1, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        if (text)
        {
            strncpy_s(nombre, sizeof(nombre), (const char *)text, sizeof(nombre) - 1);
        }
    }
    sqlite3_finalize(stmt);
    return nombre;
}

static void obtener_nombre_torneo_db(int torneo_id, char *nombre, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM torneo WHERE id = ?;";
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *txt = sqlite3_column_text(stmt, 0);
            if (txt)
            {
                strncpy_s(nombre, size, (const char *)txt, size - 1);
            }
        }
        sqlite3_finalize(stmt);
    }
}

static void print_fixture_match(int pid, const char *n1, const char *n2, int g1, int g2,
                                const char *estado, const char *fecha)
{
    if (estado && strcmp(estado, "Jugado") == 0)
    {
        printf("Partido %d: %s %d - %d %s (Jugado)\n", pid, n1, g1, g2, n2);
    }
    else if (fecha && fecha[0])
    {
        printf("Partido %d: %s vs %s (Fecha: %s)\n", pid, n1, n2, fecha);
    }
    else
    {
        printf("Partido %d: %s vs %s (Pendiente)\n", pid, n1, n2);
    }
}

static void print_proximo_partido(int pid, int e1, int e2, const char *fecha)
{
    if (fecha && fecha[0])
    {
        printf("  Partido %d: %s vs %s (%s)\n", pid, get_equipo_nombre(e1), get_equipo_nombre(e2),
               fecha);
    }
    else
    {
        printf("  Partido %d: %s vs %s\n", pid, get_equipo_nombre(e1), get_equipo_nombre(e2));
    }
}

static void process_match_fixture(sqlite3_stmt *stmt, int torneo_id, int home, int away,
                                  int tipo_torneo, int *num_matches)
{
    if (home == -1 || away == -1)
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, home);
    sqlite3_bind_int(stmt, 3, away);
    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        (*num_matches)++;
    }
    sqlite3_reset(stmt);

    if (tipo_torneo == IDA_Y_VUELTA)
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, away);
        sqlite3_bind_int(stmt, 3, home);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            (*num_matches)++;
        }
        sqlite3_reset(stmt);
    }
}

static void rotate_circle(int *temp, int total)
{
    if (total <= 1)
    {
        return;
    }
    int last = temp[total - 1];
    for (int i = total - 1; i > 1; i--)
    {
        temp[i] = temp[i - 1];
    }
    temp[1] = last;
}

static void obtener_mejor_goleador_equipo(int torneo_id, int equipo_id, char *goleador,
        size_t goleador_size, int *goles_goleador)
{
    goleador[0] = '\0';
    *goles_goleador = 0;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT j.nombre, SUM(js.goles) FROM jugador_estadisticas js "
                      "JOIN jugador j ON j.id = js.jugador_id "
                      "WHERE js.torneo_id = ? AND js.equipo_id = ? "
                      "GROUP BY js.jugador_id ORDER BY SUM(js.goles) DESC LIMIT 1;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *txt = sqlite3_column_text(stmt, 0);
        if (txt)
        {
            strncpy_s(goleador, goleador_size, (const char *)txt, goleador_size - 1);
            *goles_goleador = sqlite3_column_int(stmt, 1);
        }
    }
    sqlite3_finalize(stmt);
}

static int generate_round_robin_fixture(int torneo_id, const int *equipos, int total,
                                        int tipo_torneo)
{
#define MAX_TEMP 100
    if (total <= 0 || total > MAX_TEMP)
    {
        return 0;
    }
    int rounds = (total % 2 == 0) ? total - 1 : total;
    int mid = total / 2;
    int temp[MAX_TEMP];
    for (int i = 0; i < total; i++)
    {
        temp[i] = equipos[i];
    }
    int actual_total = total;
    int num_matches = 0;

    if (actual_total % 2 != 0)
    {
        temp[actual_total] = -1;
        actual_total++;
        mid = actual_total / 2;
    }

    const char *sql_insert = "INSERT INTO partido_torneo (torneo_id, equipo1_id, equipo2_id, fase) "
                             "VALUES (?, ?, ?, 'Fase de Grupos');";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql_insert, &stmt))
    {
        return 0;
    }

    for (int r = 0; r < rounds; r++)
    {
        for (int i = 0; i < mid; i++)
        {
            process_match_fixture(stmt, torneo_id, temp[i], temp[actual_total - 1 - i], tipo_torneo,
                                  &num_matches);
        }
        rotate_circle(temp, actual_total);
    }
    sqlite3_finalize(stmt);
    return num_matches;
}

void generar_fixture(int torneo_id)
{
    sqlite3_stmt *stmt;
    int tipo_torneo = SOLO_IDA;

    const char *sql_tipo = "SELECT tipo_torneo FROM torneo WHERE id = ?;";
    if (preparar_stmt(sql_tipo, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            tipo_torneo = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    int equipos[100];
    int n = 0;
    const char *sql_eq =
        "SELECT equipo_id FROM equipo_torneo WHERE torneo_id = ? ORDER BY equipo_id;";
    if (preparar_stmt(sql_eq, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        while (sqlite3_step(stmt) == SQLITE_ROW && n < 100)
        {
            equipos[n++] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (n < 2)
    {
        printf(
            "Se necesitan al menos 2 equipos registrados en el torneo para generar el fixture.\n");
        pause_console();
        return;
    }

    const char *sql_del = "DELETE FROM partido_torneo WHERE torneo_id = ?;";
    if (preparar_stmt(sql_del, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    int jornadas_ida = (n % 2 == 0) ? n - 1 : n;
    int max_jornadas = (tipo_torneo == IDA_Y_VUELTA) ? jornadas_ida * 2 : jornadas_ida;
    int num_matches = generate_round_robin_fixture(torneo_id, equipos, n, tipo_torneo);

    printf("Fixture generado exitosamente con %d partidos en %d jornadas.\n", num_matches,
           max_jornadas);
    pause_console();
}

void mostrar_fixture(int torneo_id)
{
    clear_screen();
    print_header("FIXTURE DEL TORNEO");

    sqlite3_stmt *stmt;
    int count = 0;
    const char *sql = "SELECT id, equipo1_id, equipo2_id, goles_equipo1, goles_equipo2, estado, "
                      "fecha FROM partido_torneo WHERE torneo_id = ? ORDER BY id;";
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count++;
            int pid = sqlite3_column_int(stmt, 0);
            int e1 = sqlite3_column_int(stmt, 1);
            int e2 = sqlite3_column_int(stmt, 2);
            int g1 = sqlite3_column_int(stmt, 3);
            int g2 = sqlite3_column_int(stmt, 4);
            const char *estado = (const char *)sqlite3_column_text(stmt, 5);
            const char *fecha = (const char *)sqlite3_column_text(stmt, 6);

            const char *n1 = get_equipo_nombre(e1);
            const char *n2 = get_equipo_nombre(e2);

            print_fixture_match(pid, n1, n2, g1, g2, estado, fecha);
        }
        sqlite3_finalize(stmt);
    }

    if (count == 0)
    {
        mostrar_no_hay_registros("partidos en el fixture");
    }
    pause_console();
}

static int listar_pendientes_mostrar(int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql_list = "SELECT id, equipo1_id, equipo2_id FROM partido_torneo WHERE torneo_id "
                           "= ? AND (estado IS NULL OR estado != 'Jugado') ORDER BY id;";
    if (!preparar_stmt(sql_list, &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        printf("Partido %d: %s vs %s\n", sqlite3_column_int(stmt, 0),
               get_equipo_nombre(sqlite3_column_int(stmt, 1)),
               get_equipo_nombre(sqlite3_column_int(stmt, 2)));
    }
    sqlite3_finalize(stmt);
    return found;
}

static int obtener_equipos_partido_db(int partido_id, int torneo_id, int *eq1, int *eq2)
{
    sqlite3_stmt *stmt;
    const char *sql_get =
        "SELECT equipo1_id, equipo2_id FROM partido_torneo WHERE id = ? AND torneo_id = ?;";
    if (!preparar_stmt(sql_get, &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_int(stmt, 2, torneo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *eq1 = sqlite3_column_int(stmt, 0);
        *eq2 = sqlite3_column_int(stmt, 1);
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

void ingresar_resultado(int torneo_id)
{
    clear_screen();
    print_header("INGRESAR RESULTADO");

    if (!listar_pendientes_mostrar(torneo_id))
    {
        mostrar_no_hay_registros("partidos pendientes");
        pause_console();
        return;
    }

    int partido_id = input_int("\nIngrese el ID del partido: ");
    int goles1 = input_int("Goles del equipo local: ");
    int goles2 = input_int("Goles del equipo visitante: ");

    int equipo1_id = 0;
    int equipo2_id = 0;
    if (!obtener_equipos_partido_db(partido_id, torneo_id, &equipo1_id, &equipo2_id))
    {
        printf("Partido no encontrado.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_upd = "UPDATE partido_torneo SET goles_equipo1 = ?, goles_equipo2 = ?, estado "
                          "= 'Jugado' WHERE id = ? AND torneo_id = ?;";
    if (preparar_stmt(sql_upd, &stmt))
    {
        sqlite3_bind_int(stmt, 1, goles1);
        sqlite3_bind_int(stmt, 2, goles2);
        sqlite3_bind_int(stmt, 3, partido_id);
        sqlite3_bind_int(stmt, 4, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Resultado registrado: %s %d - %d %s\n", get_equipo_nombre(equipo1_id), goles1,
                   goles2, get_equipo_nombre(equipo2_id));
            actualizar_tabla_posiciones(torneo_id, equipo1_id, equipo2_id, goles1, goles2);
            actualizar_estadisticas_jugadores(torneo_id, equipo1_id, equipo2_id, goles1, goles2);
            actualizar_fase_torneo(torneo_id, equipo1_id, equipo2_id, goles1, goles2);
        }
        else
        {
            printf("Error al registrar resultado: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    pause_console();
}

static void update_single_team_standings(int torneo_id, int eid, int gf, int gc)
{
    int pg = 0;
    int pe = 0;
    int pp = 0;
    int pts = 0;
    if (gf > gc)
    {
        pg = 1;
        pts = 3;
    }
    else if (gf == gc)
    {
        pe = 1;
        pts = 1;
    }
    else
    {
        pp = 1;
        pts = 0;
    }

    sqlite3_stmt *stmt;
    int exists = 0;
    const char *sql_check =
        "SELECT COUNT(*) FROM equipo_torneo_estadisticas WHERE torneo_id = ? AND equipo_id = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, eid);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
    }

    if (exists)
    {
        const char *sql_upd = "UPDATE equipo_torneo_estadisticas SET "
                              "partidos_jugados = partidos_jugados + 1, "
                              "partidos_ganados = partidos_ganados + ?, "
                              "partidos_empatados = partidos_empatados + ?, "
                              "partidos_perdidos = partidos_perdidos + ?, "
                              "goles_favor = goles_favor + ?, "
                              "goles_contra = goles_contra + ?, "
                              "puntos = puntos + ? "
                              "WHERE torneo_id = ? AND equipo_id = ?;";
        if (preparar_stmt(sql_upd, &stmt))
        {
            sqlite3_bind_int(stmt, 1, pg);
            sqlite3_bind_int(stmt, 2, pe);
            sqlite3_bind_int(stmt, 3, pp);
            sqlite3_bind_int(stmt, 4, gf);
            sqlite3_bind_int(stmt, 5, gc);
            sqlite3_bind_int(stmt, 6, pts);
            sqlite3_bind_int(stmt, 7, torneo_id);
            sqlite3_bind_int(stmt, 8, eid);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        const char *sql_ins =
            "INSERT INTO equipo_torneo_estadisticas "
            "(torneo_id, equipo_id, partidos_jugados, partidos_ganados, partidos_empatados, "
            "partidos_perdidos, goles_favor, goles_contra, puntos) "
            "VALUES (?, ?, 1, ?, ?, ?, ?, ?, ?);";
        if (preparar_stmt(sql_ins, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, eid);
            sqlite3_bind_int(stmt, 3, pg);
            sqlite3_bind_int(stmt, 4, pe);
            sqlite3_bind_int(stmt, 5, pp);
            sqlite3_bind_int(stmt, 6, gf);
            sqlite3_bind_int(stmt, 7, gc);
            sqlite3_bind_int(stmt, 8, pts);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}

void actualizar_tabla_posiciones(int torneo_id, int equipo1_id, int equipo2_id, int goles1,
                                 int goles2)
{
    int ids[2] = {equipo1_id, equipo2_id};
    int goles[2] = {goles1, goles2};
    for (int i = 0; i < 2; i++)
    {
        update_single_team_standings(torneo_id, ids[i], goles[i], goles[1 - i]);
    }
}

void ver_tabla_posiciones(int torneo_id)
{
    clear_screen();
    print_header("TABLA DE POSICIONES");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT e.nombre, et.puntos, et.partidos_jugados, "
        "et.partidos_ganados, et.partidos_empatados, et.partidos_perdidos, "
        "et.goles_favor, et.goles_contra "
        "FROM equipo_torneo_estadisticas et "
        "JOIN equipo e ON e.id = et.equipo_id "
        "WHERE et.torneo_id = ? "
        "ORDER BY et.puntos DESC, (et.goles_favor - et.goles_contra) DESC, et.goles_favor DESC;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        int pos = 0;
        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            pos++;
            const char *nom = (const char *)sqlite3_column_text(stmt, 0);
            int pts = sqlite3_column_int(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);
            int dg = gf - gc;
            printf("%2d. %-20s Pts:%3d PJ:%2d PG:%2d PE:%2d PP:%2d GF:%2d GC:%2d DG:%+3d\n", pos,
                   nom ? nom : "?", pts, pj, pg, pe, pp, gf, gc, dg);
        }
        sqlite3_finalize(stmt);

        if (!found)
        {
            mostrar_no_hay_registros("posiciones registradas");
        }
    }
    pause_console();
}

static int mostrar_estado_por_fase(int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql_fase =
        "SELECT ef.equipo_id, ef.grupo, ef.posicion_en_grupo, ef.clasificado, ef.eliminado, "
        "e.nombre FROM equipo_fase ef JOIN equipo e ON e.id = ef.equipo_id WHERE ef.torneo_id = ? "
        "ORDER BY ef.grupo, ef.posicion_en_grupo;";
    if (!preparar_stmt(sql_fase, &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    int found = 0;
    char last_grupo[50] = "";
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        const char *grupo = (const char *)sqlite3_column_text(stmt, 1);
        int clas = sqlite3_column_int(stmt, 3);
        int elim = sqlite3_column_int(stmt, 4);
        const char *nom = (const char *)sqlite3_column_text(stmt, 5);
        if (grupo && strcmp(grupo, last_grupo) != 0)
        {
            strncpy_s(last_grupo, sizeof(last_grupo), grupo, sizeof(last_grupo) - 1);
            printf("\n--- Grupo %s ---\n", grupo);
        }
        printf("  %s", nom ? nom : "?");
        if (clas)
        {
            printf(" [CLASIFICADO]");
        }
        if (elim)
        {
            printf(" [ELIMINADO]");
        }
        printf("\n");
    }
    sqlite3_finalize(stmt);
    return found;
}

static void mostrar_estado_por_estadisticas(int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql_est = "SELECT e.nombre, et.puntos, et.partidos_jugados, et.estado "
                          "FROM equipo_torneo_estadisticas et "
                          "JOIN equipo e ON e.id = et.equipo_id "
                          "WHERE et.torneo_id = ? "
                          "ORDER BY et.puntos DESC;";
    if (!preparar_stmt(sql_est, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        const char *nom = (const char *)sqlite3_column_text(stmt, 0);
        int pts = sqlite3_column_int(stmt, 1);
        int pj = sqlite3_column_int(stmt, 2);
        const char *est = (const char *)sqlite3_column_text(stmt, 3);
        printf("%-20s Pts:%d PJ:%d Estado: %s\n", nom ? nom : "?", pts, pj, est ? est : "Activo");
    }
    sqlite3_finalize(stmt);
    if (!found)
    {
        mostrar_no_hay_registros("equipos en el torneo");
    }
}

void estado_equipos(int torneo_id)
{
    clear_screen();
    print_header("ESTADO DE EQUIPOS");

    if (mostrar_estado_por_fase(torneo_id))
    {
        pause_console();
        return;
    }

    mostrar_estado_por_estadisticas(torneo_id);
    pause_console();
}

static void mostrar_posicion_equipo(int torneo_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql_pos = "SELECT COUNT(*) + 1 FROM equipo_torneo_estadisticas e1 "
                          "WHERE e1.torneo_id = ? AND e1.puntos > (SELECT e2.puntos FROM "
                          "equipo_torneo_estadisticas e2 "
                          "WHERE e2.torneo_id = ? AND e2.equipo_id = ?);";
    if (!preparar_stmt(sql_pos, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, torneo_id);
    sqlite3_bind_int(stmt, 3, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Posicion actual: %d\n", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
}

static void mostrar_estadisticas_equipo_dashboard(int torneo_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql_est = "SELECT partidos_jugados, partidos_ganados, partidos_empatados, "
                          "partidos_perdidos, goles_favor, goles_contra, puntos "
                          "FROM equipo_torneo_estadisticas WHERE torneo_id = ? AND equipo_id = ?;";
    if (!preparar_stmt(sql_est, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf(
            "Partidos: %d | G: %d | E: %d | P: %d | GF: %d | GC: %d | DG: %d | Pts: %d\n",
            sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4), sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 4) - sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
    }
    sqlite3_finalize(stmt);
}

void mostrar_dashboard_torneo(int torneo_id, int equipo_id)
{
    clear_screen();
    print_header("DASHBOARD DEL TORNEO");

    char nombre_torneo[128] = "";
    obtener_nombre_torneo_db(torneo_id, nombre_torneo, sizeof(nombre_torneo));
    printf("Torneo: %s (ID: %d)\n\n", nombre_torneo[0] ? nombre_torneo : "?", torneo_id);

    if (equipo_id > 0)
    {
        printf("Equipo: %s\n\n", get_equipo_nombre(equipo_id));
        mostrar_posicion_equipo(torneo_id, equipo_id);
        mostrar_estadisticas_equipo_dashboard(torneo_id, equipo_id);
        printf("\n");
    }

    mostrar_proximos_partidos(torneo_id, equipo_id);
    pause_console();
}

static void mostrar_proximos_partidos_equipo(int torneo_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, equipo1_id, equipo2_id, fecha FROM partido_torneo "
                      "WHERE torneo_id = ? AND (equipo1_id = ? OR equipo2_id = ?) "
                      "AND (estado IS NULL OR estado != 'Jugado') ORDER BY id;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    sqlite3_bind_int(stmt, 3, equipo_id);
    printf("Proximos partidos:\n");
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        int pid = sqlite3_column_int(stmt, 0);
        int e1 = sqlite3_column_int(stmt, 1);
        int e2 = sqlite3_column_int(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);
        print_proximo_partido(pid, e1, e2, fecha);
    }
    sqlite3_finalize(stmt);
    if (!found)
    {
        printf("  No hay partidos pendientes.\n");
    }
}

static void mostrar_proximos_partidos_general(int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, equipo1_id, equipo2_id, fecha FROM partido_torneo "
        "WHERE torneo_id = ? AND (estado IS NULL OR estado != 'Jugado') ORDER BY id LIMIT 10;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    printf("Proximos partidos (generales):\n");
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        int pid = sqlite3_column_int(stmt, 0);
        int e1 = sqlite3_column_int(stmt, 1);
        int e2 = sqlite3_column_int(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);
        print_proximo_partido(pid, e1, e2, fecha);
    }
    sqlite3_finalize(stmt);
    if (!found)
    {
        printf("  No hay partidos pendientes.\n");
    }
}

void mostrar_proximos_partidos(int torneo_id, int equipo_id)
{
    if (equipo_id > 0)
    {
        mostrar_proximos_partidos_equipo(torneo_id, equipo_id);
    }
    else
    {
        mostrar_proximos_partidos_general(torneo_id);
    }
}

static void mostrar_estadisticas_por_equipo(int torneo_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT j.nombre, js.goles, js.asistencias, js.tarjetas_amarillas, js.tarjetas_rojas "
        "FROM jugador_estadisticas js "
        "JOIN jugador j ON j.id = js.jugador_id "
        "WHERE js.torneo_id = ? AND js.equipo_id = ? "
        "ORDER BY js.goles DESC;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    printf("Jugadores de %s:\n", get_equipo_nombre(equipo_id));
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        const char *nom = (const char *)sqlite3_column_text(stmt, 0);
        int g = sqlite3_column_int(stmt, 1);
        int a = sqlite3_column_int(stmt, 2);
        int am = sqlite3_column_int(stmt, 3);
        int rj = sqlite3_column_int(stmt, 4);
        printf("  %-20s Goles:%d Asist:%d TA:%d TR:%d\n", nom ? nom : "?", g, a, am, rj);
    }
    sqlite3_finalize(stmt);
    if (!found)
    {
        printf("  Sin estadisticas registradas.\n");
    }
}

static void mostrar_goleadores_torneo(int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT j.nombre, e.nombre, js.goles, js.asistencias "
                      "FROM jugador_estadisticas js "
                      "JOIN jugador j ON j.id = js.jugador_id "
                      "JOIN equipo e ON e.id = js.equipo_id "
                      "WHERE js.torneo_id = ? "
                      "ORDER BY js.goles DESC LIMIT 20;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    printf("Mejores goleadores del torneo:\n");
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        const char *nom = (const char *)sqlite3_column_text(stmt, 0);
        const char *eq = (const char *)sqlite3_column_text(stmt, 1);
        int g = sqlite3_column_int(stmt, 2);
        int a = sqlite3_column_int(stmt, 3);
        printf("  %-20s (%-15s) Goles:%d Asist:%d\n", nom ? nom : "?", eq ? eq : "?", g, a);
    }
    sqlite3_finalize(stmt);
    if (!found)
    {
        printf("  Sin estadisticas registradas.\n");
    }
}

void mostrar_estadisticas_jugador(int torneo_id, int equipo_id)
{
    clear_screen();
    print_header("ESTADISTICAS DE JUGADORES");

    if (equipo_id > 0)
    {
        mostrar_estadisticas_por_equipo(torneo_id, equipo_id);
    }
    else
    {
        mostrar_goleadores_torneo(torneo_id);
    }

    pause_console();
}

void mostrar_historial_equipo(int equipo_id)
{
    clear_screen();
    print_header("HISTORIAL DEL EQUIPO");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT t.nombre, eh.posicion_final, eh.partidos_jugados, "
                      "eh.partidos_ganados, eh.partidos_empatados, eh.partidos_perdidos, "
                      "eh.goles_favor, eh.goles_contra, eh.mejor_goleador, eh.goles_mejor_goleador "
                      "FROM equipo_historial eh "
                      "JOIN torneo t ON t.id = eh.torneo_id "
                      "WHERE eh.equipo_id = ? ORDER BY eh.id DESC;";
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            const char *tname = (const char *)sqlite3_column_text(stmt, 0);
            int pos = sqlite3_column_int(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);
            const char *gol = (const char *)sqlite3_column_text(stmt, 8);
            int goles_gol = sqlite3_column_int(stmt, 9);

            printf("Torneo: %s\n", tname ? tname : "?");
            printf("  Posicion: %d | PJ: %d | PG: %d | PE: %d | PP: %d | GF: %d | GC: %d\n", pos,
                   pj, pg, pe, pp, gf, gc);
            if (gol && gol[0])
            {
                printf("  Mejor goleador: %s (%d goles)\n", gol, goles_gol);
            }
            printf("\n");
        }
        sqlite3_finalize(stmt);
        if (!found)
        {
            mostrar_no_hay_registros("historial del equipo");
        }
    }
    pause_console();
}

void actualizar_estadisticas_jugadores(int torneo_id, int equipo1_id, int equipo2_id, int goles1,
                                       int goles2)
{
    // This function updates goal/assist stats for players.
    // In a real scenario, you'd ask which players scored/assisted.
    // For now, we distribute goals to existing player stats proportionally.
    sqlite3_stmt *stmt;
    int ids[2] = {equipo1_id, equipo2_id};
    int goles[2] = {goles1, goles2};

    for (int i = 0; i < 2; i++)
    {
        if (goles[i] <= 0)
        {
            continue;
        }

        // Create stats entries for players who don't have them yet
        const char *sql_ins =
            "INSERT OR IGNORE INTO jugador_estadisticas (jugador_id, torneo_id, equipo_id, goles, "
            "asistencias) "
            "SELECT j.id, ?, j.equipo_id, 0, 0 FROM jugador j WHERE j.equipo_id = ?;";
        if (preparar_stmt(sql_ins, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, ids[i]);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Distribute goals among players (simple: assign to first player or proportionally)
        if (goles[i] > 0)
        {
            const char *sql_upd = "UPDATE jugador_estadisticas SET goles = goles + ? "
                                  "WHERE torneo_id = ? AND equipo_id = ? AND jugador_id IN (SELECT "
                                  "id FROM jugador WHERE equipo_id = ? LIMIT 1);";
            if (preparar_stmt(sql_upd, &stmt))
            {
                sqlite3_bind_int(stmt, 1, goles[i]);
                sqlite3_bind_int(stmt, 2, torneo_id);
                sqlite3_bind_int(stmt, 3, ids[i]);
                sqlite3_bind_int(stmt, 4, ids[i]);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
    }
}

void actualizar_fase_torneo(int torneo_id, int equipo1_id, int equipo2_id, int goles1, int goles2)
{
    // For elimination tournaments, determine winner and update fase
    sqlite3_stmt *stmt;

    // Get tournament type
    int tipo_torneo = SOLO_IDA;
    const char *sql_tipo = "SELECT tipo_torneo FROM torneo WHERE id = ?;";
    if (preparar_stmt(sql_tipo, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            tipo_torneo = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (tipo_torneo != ELIMINACION_DIRECTA)
    {
        return;
    }

    // Determine winner
    int winner_id = 0;
    int loser_id = 0;
    if (goles1 > goles2)
    {
        winner_id = equipo1_id;
        loser_id = equipo2_id;
    }
    else if (goles2 > goles1)
    {
        winner_id = equipo2_id;
        loser_id = equipo1_id;
    }
    else
    {
        return; // Draw in elimination -> not handled here (would need extra time)
    }

    // Mark loser as eliminated in equipo_fase
    const char *sql_elim =
        "UPDATE equipo_fase SET eliminado = 1 WHERE torneo_id = ? AND equipo_id = ?;";
    if (preparar_stmt(sql_elim, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, loser_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Mark winner as clasificado
    const char *sql_clas =
        "UPDATE equipo_fase SET clasificado = 1 WHERE torneo_id = ? AND equipo_id = ?;";
    if (preparar_stmt(sql_clas, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, winner_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

typedef struct
{
    int torneo_id;
    int eid;
    int pos;
    int pj;
    int pg;
    int pe;
    int pp;
    int gf;
    int gc;
} HistorialEquipoParams;

static void guardar_historial_equipo(const HistorialEquipoParams *p)
{
    char goleador[100] = "";
    int goles_goleador = 0;
    obtener_mejor_goleador_equipo(p->torneo_id, p->eid, goleador, sizeof(goleador),
                                  &goles_goleador);

    sqlite3_stmt *stmt2;
    const char *sql_hist =
        "INSERT INTO equipo_historial "
        "(equipo_id, torneo_id, posicion_final, partidos_jugados, partidos_ganados, "
        "partidos_empatados, partidos_perdidos, goles_favor, goles_contra, mejor_goleador, "
        "goles_mejor_goleador) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    if (!preparar_stmt(sql_hist, &stmt2))
    {
        return;
    }
    sqlite3_bind_int(stmt2, 1, p->eid);
    sqlite3_bind_int(stmt2, 2, p->torneo_id);
    sqlite3_bind_int(stmt2, 3, p->pos);
    sqlite3_bind_int(stmt2, 4, p->pj);
    sqlite3_bind_int(stmt2, 5, p->pg);
    sqlite3_bind_int(stmt2, 6, p->pe);
    sqlite3_bind_int(stmt2, 7, p->pp);
    sqlite3_bind_int(stmt2, 8, p->gf);
    sqlite3_bind_int(stmt2, 9, p->gc);
    sqlite3_bind_text(stmt2, 10, goleador, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt2, 11, goles_goleador);
    sqlite3_step(stmt2);
    sqlite3_finalize(stmt2);
}

void finalizar_torneo(int torneo_id)
{
    clear_screen();
    print_header("FINALIZAR TORNEO");

    if (!confirmar("Esta seguro de finalizar este torneo?"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_est =
        "SELECT equipo_id, partidos_jugados, partidos_ganados, partidos_empatados, "
        "partidos_perdidos, goles_favor, goles_contra, puntos "
        "FROM equipo_torneo_estadisticas WHERE torneo_id = ? ORDER BY puntos DESC;";
    if (!preparar_stmt(sql_est, &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, torneo_id);
    int pos = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        pos++;
        HistorialEquipoParams hep = {.torneo_id = torneo_id,
                                     .eid = sqlite3_column_int(stmt, 0),
                                     .pos = pos,
                                     .pj = sqlite3_column_int(stmt, 1),
                                     .pg = sqlite3_column_int(stmt, 2),
                                     .pe = sqlite3_column_int(stmt, 3),
                                     .pp = sqlite3_column_int(stmt, 4),
                                     .gf = sqlite3_column_int(stmt, 5),
                                     .gc = sqlite3_column_int(stmt, 6)
                                    };
        guardar_historial_equipo(&hep);
    }
    sqlite3_finalize(stmt);

    mostrar_alerta_operacion("Torneo", "Finalizado", NULL);
    printf("Historial de %d equipo(s) guardado.\n", pos);
    pause_console();
}

void exportar_tabla_posiciones(int torneo_id)
{
    const char *export_dir = get_export_dir();

    char filename[512];
    snprintf(filename, sizeof(filename), "%s/tabla_posiciones_%d.txt", export_dir, torneo_id);

    FILE *f = NULL;
    if (fopen_s(&f, filename, "w") != 0)
    {
        printf("Error al crear archivo de exportacion.\n");
        pause_console();
        return;
    }

    fprintf(f, "=== TABLA DE POSICIONES ===\n\n");
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT e.nombre, et.puntos, et.partidos_jugados, "
        "et.partidos_ganados, et.partidos_empatados, et.partidos_perdidos, "
        "et.goles_favor, et.goles_contra "
        "FROM equipo_torneo_estadisticas et "
        "JOIN equipo e ON e.id = et.equipo_id "
        "WHERE et.torneo_id = ? "
        "ORDER BY et.puntos DESC, (et.goles_favor - et.goles_contra) DESC, et.goles_favor DESC;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        int pos = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            pos++;
            const char *nom = (const char *)sqlite3_column_text(stmt, 0);
            int pts = sqlite3_column_int(stmt, 1);
            int pj = sqlite3_column_int(stmt, 2);
            int pg = sqlite3_column_int(stmt, 3);
            int pe = sqlite3_column_int(stmt, 4);
            int pp = sqlite3_column_int(stmt, 5);
            int gf = sqlite3_column_int(stmt, 6);
            int gc = sqlite3_column_int(stmt, 7);
            fprintf(f, "%d. %s - Pts:%d PJ:%d PG:%d PE:%d PP:%d GF:%d GC:%d DG:%+d\n", pos,
                    nom ? nom : "?", pts, pj, pg, pe, pp, gf, gc, gf - gc);
        }
        sqlite3_finalize(stmt);
    }

    fclose(f);
    printf("Tabla de posiciones exportada a: %s\n", filename);
    pause_console();
}

static void exportar_estadisticas_por_equipo(FILE *f, int torneo_id, int equipo_id)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT j.nombre, js.goles, js.asistencias, js.tarjetas_amarillas, js.tarjetas_rojas "
        "FROM jugador_estadisticas js "
        "JOIN jugador j ON j.id = js.jugador_id "
        "WHERE js.torneo_id = ? AND js.equipo_id = ? "
        "ORDER BY js.goles DESC;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    sqlite3_bind_int(stmt, 2, equipo_id);
    fprintf(f, "Equipo: %s\n\n", get_equipo_nombre(equipo_id));
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nom = (const char *)sqlite3_column_text(stmt, 0);
        int g = sqlite3_column_int(stmt, 1);
        int a = sqlite3_column_int(stmt, 2);
        int am = sqlite3_column_int(stmt, 3);
        int rj = sqlite3_column_int(stmt, 4);
        fprintf(f, "%s - Goles:%d Asist:%d TA:%d TR:%d\n", nom ? nom : "?", g, a, am, rj);
    }
    sqlite3_finalize(stmt);
}

static void exportar_estadisticas_todas(FILE *f, int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT j.nombre, e.nombre, js.goles, js.asistencias "
                      "FROM jugador_estadisticas js "
                      "JOIN jugador j ON j.id = js.jugador_id "
                      "JOIN equipo e ON e.id = js.equipo_id "
                      "WHERE js.torneo_id = ? "
                      "ORDER BY js.goles DESC;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nom = (const char *)sqlite3_column_text(stmt, 0);
        const char *eq = (const char *)sqlite3_column_text(stmt, 1);
        int g = sqlite3_column_int(stmt, 2);
        int a = sqlite3_column_int(stmt, 3);
        fprintf(f, "%s (%s) - Goles:%d Asist:%d\n", nom ? nom : "?", eq ? eq : "?", g, a);
    }
    sqlite3_finalize(stmt);
}

void exportar_estadisticas_jugadores(int torneo_id, int equipo_id)
{
    const char *export_dir = get_export_dir();

    char filename[512];
    if (equipo_id > 0)
    {
        snprintf(filename, sizeof(filename), "%s/estadisticas_jugadores_%d_%d.txt", export_dir,
                 torneo_id, equipo_id);
    }
    else
    {
        snprintf(filename, sizeof(filename), "%s/estadisticas_jugadores_%d.txt", export_dir,
                 torneo_id);
    }

    FILE *f = NULL;
    if (fopen_s(&f, filename, "w") != 0)
    {
        printf("Error al crear archivo de exportacion.\n");
        pause_console();
        return;
    }

    fprintf(f, "=== ESTADISTICAS DE JUGADORES ===\n\n");

    if (equipo_id > 0)
    {
        exportar_estadisticas_por_equipo(f, torneo_id, equipo_id);
    }
    else
    {
        exportar_estadisticas_todas(f, torneo_id);
    }

    fclose(f);
    printf("Estadisticas exportadas a: %s\n", filename);
    pause_console();
}

static void escribir_tabla_posiciones_reporte(FILE *f, int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT e.nombre, et.puntos, et.partidos_jugados, "
        "et.partidos_ganados, et.partidos_empatados, et.partidos_perdidos, "
        "et.goles_favor, et.goles_contra "
        "FROM equipo_torneo_estadisticas et "
        "JOIN equipo e ON e.id = et.equipo_id "
        "WHERE et.torneo_id = ? "
        "ORDER BY et.puntos DESC, (et.goles_favor - et.goles_contra) DESC, et.goles_favor DESC;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    int pos = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        pos++;
        const char *nom = (const char *)sqlite3_column_text(stmt, 0);
        int pts = sqlite3_column_int(stmt, 1);
        int pj = sqlite3_column_int(stmt, 2);
        int pg = sqlite3_column_int(stmt, 3);
        int pe = sqlite3_column_int(stmt, 4);
        int pp = sqlite3_column_int(stmt, 5);
        int gf = sqlite3_column_int(stmt, 6);
        int gc = sqlite3_column_int(stmt, 7);
        fprintf(f, "%d. %s - Pts:%d PJ:%d PG:%d PE:%d PP:%d GF:%d GC:%d DG:%+d\n", pos,
                nom ? nom : "?", pts, pj, pg, pe, pp, gf, gc, gf - gc);
    }
    sqlite3_finalize(stmt);
}

static void escribir_goleadores_reporte(FILE *f, int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT j.nombre, e.nombre, js.goles "
                      "FROM jugador_estadisticas js "
                      "JOIN jugador j ON j.id = js.jugador_id "
                      "JOIN equipo e ON e.id = js.equipo_id "
                      "WHERE js.torneo_id = ? "
                      "ORDER BY js.goles DESC LIMIT 10;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nom = (const char *)sqlite3_column_text(stmt, 0);
        const char *eq = (const char *)sqlite3_column_text(stmt, 1);
        int g = sqlite3_column_int(stmt, 2);
        fprintf(f, "%s (%s) - %d goles\n", nom ? nom : "?", eq ? eq : "?", g);
    }
    sqlite3_finalize(stmt);
}

static void escribir_resultados_reporte(FILE *f, int torneo_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT equipo1_id, equipo2_id, goles_equipo1, goles_equipo2, estado "
                      "FROM partido_torneo WHERE torneo_id = ? ORDER BY id;";
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, torneo_id);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int e1 = sqlite3_column_int(stmt, 0);
        int e2 = sqlite3_column_int(stmt, 1);
        int g1 = sqlite3_column_int(stmt, 2);
        int g2 = sqlite3_column_int(stmt, 3);
        const char *est = (const char *)sqlite3_column_text(stmt, 4);
        fprintf(f, "%s vs %s - ", get_equipo_nombre(e1), get_equipo_nombre(e2));
        if (est && strcmp(est, "Jugado") == 0)
        {
            fprintf(f, "%d - %d\n", g1, g2);
        }
        else
        {
            fprintf(f, "Pendiente\n");
        }
    }
    sqlite3_finalize(stmt);
}

void generar_reporte_torneo(int torneo_id)
{
    const char *export_dir = get_export_dir();
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/reporte_torneo_%d.txt", export_dir, torneo_id);

    FILE *f = NULL;
    if (fopen_s(&f, filename, "w") != 0)
    {
        printf("Error al crear archivo de reporte.\n");
        pause_console();
        return;
    }

    char nombre[128] = "";
    obtener_nombre_torneo_db(torneo_id, nombre, sizeof(nombre));

    fprintf(f, "========================================\n");
    fprintf(f, "  REPORTE DEL TORNEO: %s\n", nombre[0] ? nombre : "?");
    fprintf(f, "========================================\n\n");

    fprintf(f, "--- TABLA DE POSICIONES ---\n\n");
    escribir_tabla_posiciones_reporte(f, torneo_id);

    fprintf(f, "\n--- MEJORES GOLEADORES ---\n\n");
    escribir_goleadores_reporte(f, torneo_id);

    fprintf(f, "\n--- RESULTADOS ---\n\n");
    escribir_resultados_reporte(f, torneo_id);

    fclose(f);
    printf("Reporte generado: %s\n", filename);
    pause_console();
}

static int pedir_id_equipo_opcional(const char *sufijo)
{
    char prompt[96];
    snprintf(prompt, sizeof(prompt), "Ingrese ID del equipo %s: ", sufijo);
    return input_int(prompt);
}

static int mostrar_menu_administrar_torneo(int torneo_id)
{
    char nombre[128] = "";
    obtener_nombre_torneo_db(torneo_id, nombre, sizeof(nombre));
    printf("Torneo: %s (ID: %d)\n\n", nombre[0] ? nombre : "?", torneo_id);

    printf("1. Generar Fixture\n");
    printf("2. Mostrar Fixture\n");
    printf("3. Ingresar Resultado\n");
    printf("4. Ver Tabla de Posiciones\n");
    printf("5. Estado de Equipos\n");
    printf("6. Dashboard del Torneo\n");
    printf("7. Estadisticas de Jugadores\n");
    printf("8. Historial del Equipo\n");
    printf("9. Exportar Tabla de Posiciones\n");
    printf("10. Exportar Estadisticas de Jugadores\n");
    printf("11. Generar Reporte del Torneo\n");
    printf("12. Finalizar Torneo\n");
    printf("0. Volver\n");

    return input_int("> ");
}

static void ejecutar_opcion_torneo(int opcion, int torneo_id)
{
    switch (opcion)
    {
    case 1:
        generar_fixture(torneo_id);
        break;
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
        mostrar_dashboard_torneo(torneo_id, pedir_id_equipo_opcional("(0 para vista general)"));
        break;
    case 7:
        mostrar_estadisticas_jugador(torneo_id, pedir_id_equipo_opcional("(0 para todos)"));
        break;
    case 8:
        mostrar_historial_equipo(pedir_id_equipo_opcional(""));
        break;
    case 9:
        exportar_tabla_posiciones(torneo_id);
        break;
    case 10:
        exportar_estadisticas_jugadores(torneo_id, pedir_id_equipo_opcional("(0 para todos)"));
        break;
    case 11:
        generar_reporte_torneo(torneo_id);
        break;
    case 12:
        finalizar_torneo(torneo_id);
        break;
    case 0:
        break;
    default:
        printf("Opcion invalida.\n");
        pause_console();
        break;
    }
}

void administrar_torneo(void)
{
    clear_screen();
    print_header("ADMINISTRAR TORNEO");

    if (!listar_torneos_generico("torneos registrados para administrar"))
    {
        pause_console();
        return;
    }

    int torneo_id = input_int("\nIngrese el ID del torneo a administrar (0 para cancelar): ");
    if (torneo_id == 0)
    {
        return;
    }

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    int opcion;
    do
    {
        clear_screen();
        print_header("ADMINISTRACION DE TORNEO");
        opcion = mostrar_menu_administrar_torneo(torneo_id);
        ejecutar_opcion_torneo(opcion, torneo_id);
    }
    while (opcion != 0);
}

void menu_torneos(void)
{
    MenuItem items[] = {{1, "Crear Torneo", &crear_torneo},
        {2, "Listar Torneos", &listar_torneos},
        {3, "Modificar Torneo", &modificar_torneo},
        {4, "Eliminar Torneo", &eliminar_torneo},
        {5, "Administrar Torneo", &administrar_torneo},
        {0, "Volver", NULL}
    };

    ejecutar_menu("TORNEOS", items, 6);
}
