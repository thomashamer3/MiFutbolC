/**
 * @file atajos_config.c
 * @brief Modulo de configuracion de atajos de teclado
 *
 * Permite al usuario personalizar las teclas de acceso rapido a las
 * diferentes funcionalidades del sistema. Los atajos se almacenan
 * en la tabla atajo_config de la base de datos SQLite.
 *
 * Tabla: atajo_config (id, tecla, accion, descripcion)
 */

#include "atajos_config.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Asigna los atajos por defecto si la tabla esta vacia
 *
 * Inserta los atajos predeterminados (D, B, C, N, S, H, Q) en la
 * tabla atajo_config cuando no existe ningun registro.
 */
static void asegurar_atajos_default(void)
{
    sqlite3_stmt *stmt_count = NULL;
    const char *sql_count = "SELECT COUNT(*) FROM atajo_config;";

    if (!db_prepare_stmt_with_error(&stmt_count, sql_count,
                                    "Error al consultar atajos"))
    {
        return;
    }

    int count = 0;
    if (sqlite3_step(stmt_count) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt_count, 0);
    }
    sqlite3_finalize(stmt_count);

    if (count > 0)
    {
        return;
    }

    const char *sql_insert =
        "INSERT INTO atajo_config (tecla, accion, descripcion) "
        "VALUES (?, ?, ?);";

    static const char *default_teclas[] = {"D", "B", "C", "N", "S", "H", "Q"};
    static const char *default_acciones[] =
    {
        "Dashboard", "Busqueda", "Calendario", "Nuevo Partido",
        "Estadisticas", "Ayuda", "Salir"
    };
    static const char *default_descripciones[] =
    {
        "Panel principal", "Busqueda global", "Calendario de partidos",
        "Registrar nuevo partido", "Estadisticas y graficas",
        "Mostrar ayuda de atajos", "Cerrar la aplicacion"
    };

    for (int i = 0; i < 7; i++)
    {
        sqlite3_stmt *stmt = NULL;
        if (db_prepare_stmt_with_error(&stmt, sql_insert,
                                       "Error al insertar atajo por defecto"))
        {
            sqlite3_bind_text(stmt, 1, default_teclas[i], -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, default_acciones[i], -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, default_descripciones[i], -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    app_log_event("ATAJOS_CONFIG", "Atajos por defecto insertados");
}

/**
 * @brief Lista todos los atajos configurados en formato de tabla
 */
void listar_atajos_config(void)
{
    clear_screen();
    print_header("ATAJOS DE TECLADO");

    asegurar_atajos_default();

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT tecla, accion, descripcion "
        "FROM atajo_config ORDER BY id ASC;";

    if (!db_prepare_stmt_with_error(&stmt, sql, "Error al listar atajos"))
    {
        return;
    }

    printf("  %-8s %-18s %s\n", "TECLA", "ACCION", "DESCRIPCION");
    printf("  %-8s %-18s %s\n", "------", "------------------",
           "--------------------");

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *tecla = (const char *)sqlite3_column_text(stmt, 0);
        const char *accion = (const char *)sqlite3_column_text(stmt, 1);
        const char *descripcion = (const char *)sqlite3_column_text(stmt, 2);

        printf("  [%-6s] %-18s %s\n",
               tecla ? tecla : "?",
               accion ? accion : "",
               descripcion ? descripcion : "");
        found = 1;
    }

    if (!found)
    {
        mostrar_no_hay_registros("atajos configurados");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Modifica la tecla asignada a una accion existente
 *
 * Solicita el nombre de la accion y la nueva tecla, verificando
 * que no exista conflicto con otra accion antes de actualizar.
 */
void modificar_atajo_config(void)
{
    clear_screen();
    print_header("MODIFICAR ATAJO");

    asegurar_atajos_default();

    sqlite3_stmt *stmt_list = NULL;
    const char *sql_list =
        "SELECT id, tecla, accion FROM atajo_config ORDER BY id ASC;";

    if (!db_prepare_stmt_with_error(&stmt_list, sql_list,
                                    "Error al listar atajos"))
    {
        return;
    }

    printf("  %-8s %-8s %-18s\n", "ID", "TECLA", "ACCION");
    printf("  %-8s %-8s %-18s\n", "------", "------", "------------------");

    while (sqlite3_step(stmt_list) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt_list, 0);
        const char *tecla = (const char *)sqlite3_column_text(stmt_list, 1);
        const char *accion = (const char *)sqlite3_column_text(stmt_list, 2);

        printf("  %-8d [%-6s] %-18s\n", id,
               tecla ? tecla : "?",
               accion ? accion : "");
    }
    sqlite3_finalize(stmt_list);

    char accion_buscada[64] = {0};
    input_string("\nNombre de la accion a modificar: ", accion_buscada,
                 sizeof(accion_buscada));

    if (accion_buscada[0] == '\0')
    {
        printf("Accion no puede estar vacia.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt_check = NULL;
    const char *sql_check =
        "SELECT id FROM atajo_config WHERE accion = ?;";

    if (!db_prepare_stmt_with_error(&stmt_check, sql_check,
                                    "Error al buscar accion"))
    {
        return;
    }

    sqlite3_bind_text(stmt_check, 1, accion_buscada, -1, SQLITE_STATIC);

    int existe = 0;
    if (sqlite3_step(stmt_check) == SQLITE_ROW)
    {
        existe = 1;
    }
    sqlite3_finalize(stmt_check);

    if (!existe)
    {
        printf("No se encontro la accion '%s'.\n", accion_buscada);
        pause_console();
        return;
    }

    char nueva_tecla[16] = {0};
    input_string("Nueva tecla (un caracter): ", nueva_tecla, sizeof(nueva_tecla));

    if (nueva_tecla[0] == '\0')
    {
        printf("La tecla no puede estar vacia.\n");
        pause_console();
        return;
    }

    char tecla_upper = (char)((nueva_tecla[0] >= 'a' && nueva_tecla[0] <= 'z')
                              ? nueva_tecla[0] - 32
                              : nueva_tecla[0]);

    sqlite3_stmt *stmt_conflict = NULL;
    const char *sql_conflict =
        "SELECT accion FROM atajo_config WHERE tecla = ? AND accion != ?;";

    if (!db_prepare_stmt_with_error(&stmt_conflict, sql_conflict,
                                    "Error al verificar conflicto"))
    {
        return;
    }

    char tecla_upper_str[2] = {tecla_upper, '\0'};
    sqlite3_bind_text(stmt_conflict, 1, tecla_upper_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_conflict, 2, accion_buscada, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt_conflict) == SQLITE_ROW)
    {
        const char *conflict_accion =
            (const char *)sqlite3_column_text(stmt_conflict, 0);
        printf("La tecla '%c' ya esta asignada a '%s'.\n",
               tecla_upper, conflict_accion ? conflict_accion : "");
        sqlite3_finalize(stmt_conflict);
        pause_console();
        return;
    }
    sqlite3_finalize(stmt_conflict);

    sqlite3_stmt *stmt_update = NULL;
    const char *sql_update =
        "UPDATE atajo_config SET tecla = ? WHERE accion = ?;";

    if (db_prepare_stmt_with_error(&stmt_update, sql_update,
                                   "Error al actualizar atajo"))
    {
        sqlite3_bind_text(stmt_update, 1, tecla_upper_str, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_update, 2, accion_buscada, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt_update) == SQLITE_DONE)
        {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                     "Atajo modificado: accion=%.40s tecla=%c",
                     accion_buscada, tecla_upper);
            app_log_event("ATAJOS_CONFIG", log_msg);
            printf("Atajo actualizado: [%c] -> %s\n", tecla_upper,
                   accion_buscada);
        }
        else
        {
            printf("Error al actualizar el atajo.\n");
        }

        sqlite3_finalize(stmt_update);
    }

    pause_console();
}

/**
 * @brief Restaura todos los atajos a los valores por defecto
 *
 * Elimina todos los registros de la tabla atajo_config y vuelve
 * a insertar los atajos predeterminados.
 */
void restaurar_atajos_default(void)
{
    clear_screen();
    print_header("RESTAURAR ATAJOS POR DEFECTO");

    printf("Se eliminaran todos los atajos personalizados y se\n");
    printf("restauraran los valores por defecto.\n\n");
    printf("D = Dashboard\n");
    printf("B = Busqueda\n");
    printf("C = Calendario\n");
    printf("N = Nuevo Partido\n");
    printf("S = Estadisticas\n");
    printf("H = Ayuda\n");
    printf("Q = Salir\n\n");

    printf("(S/N): ");
    char confirm[16] = {0};
    fgets(confirm, sizeof(confirm), stdin);

    if (confirm[0] != 's' && confirm[0] != 'S')
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt_delete = NULL;
    const char *sql_delete = "DELETE FROM atajo_config;";

    if (db_prepare_stmt_with_error(&stmt_delete, sql_delete,
                                   "Error al eliminar atajos"))
    {
        sqlite3_step(stmt_delete);
        sqlite3_finalize(stmt_delete);
    }

    asegurar_atajos_default();

    app_log_event("ATAJOS_CONFIG", "Atajos restaurados a valores por defecto");
    printf("Atajos restaurados correctamente.\n");
    pause_console();
}

/**
 * @brief Obtiene la tecla configurada para una accion especifica
 *
 * @param accion Nombre de la accion a buscar (ej: "Dashboard")
 * @return Caracter de la tecla asignada en mayuscula, o 0 si no existe
 */
char obtener_atajo_para(const char *accion)
{
    if (!accion || accion[0] == '\0')
    {
        return 0;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT tecla FROM atajo_config WHERE accion = ? LIMIT 1;";

    if (!db_prepare_stmt_with_error(&stmt, sql, "Error al obtener atajo"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, accion, -1, SQLITE_STATIC);

    char resultado = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *tecla = (const char *)sqlite3_column_text(stmt, 0);
        if (tecla && tecla[0] != '\0')
        {
            resultado = tecla[0];
            if (resultado >= 'a' && resultado <= 'z')
            {
                resultado = (char)(resultado - 32);
            }
        }
    }

    sqlite3_finalize(stmt);
    return resultado;
}

/**
 * @brief Muestra el menu de configuracion de atajos de teclado
 *
 * Permite listar, modificar o restaurar los atajos de acceso rapido.
 */
void menu_atajos_config(void)
{
    MenuItem items[] =
    {
        {1, "Listar atajos", &listar_atajos_config},
        {2, "Modificar atajo", &modificar_atajo_config},
        {3, "Restaurar defaults", &restaurar_atajos_default},
        {0, "Volver", NULL}
    };

    ejecutar_menu("CONFIGURAR ATAJOS", items,
                  (int)(sizeof(items) / sizeof(items[0])));
}
