#include "camiseta.h"
#include "db.h"
#include "menu.h"
#include "random_utils.h"
#include "settings.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
#endif

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql);
static void listar_camisetas_simple(void);
static int cargar_imagen_para_camiseta_id(int id);

#ifndef _WIN32
extern char **environ;

static int run_command_posix_argv(const char *const argv[])
{
    if (!argv || !argv[0] || argv[0][0] == '\0')
    {
        return 0;
    }

    pid_t pid = (pid_t)0;
    if (posix_spawnp(&pid, argv[0], NULL, NULL, (char *const *)argv, environ) != 0)
    {
        return 0;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int is_safe_package_name(const char *package_name)
{
    if (!package_name || package_name[0] == '\0')
    {
        return 0;
    }

    const unsigned char *p = (const unsigned char *)package_name;
    while (*p != '\0')
    {
        if (!(isalnum(*p) || *p == '_' || *p == '-' || *p == '+' || *p == '.'))
        {
            return 0;
        }
        p++;
    }

    return 1;
}
#endif

static void asegurar_fila_settings(void)
{
    sqlite3_exec(db,
                 "INSERT OR IGNORE INTO settings(id, theme, language, mode, "
                 "text_size, image_viewer) "
                 "VALUES(1, 0, 0, 0, 1, '');",
                 NULL, NULL, NULL);
}

static int obtener_visor_preferido(char *buffer, size_t size)
{
    if (!buffer || size == 0)
    {
        return 0;
    }

    asegurar_fila_settings();

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT image_viewer FROM settings WHERE id = 1"))
    {
        return 0;
    }

    int flag = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *flag_char = sqlite3_column_text(stmt, 0);
        if (flag_char && strncpy_s(buffer, size, (const char *)flag_char, _TRUNCATE) == 0)
        {
            trim_whitespace(buffer);
            flag = 1;
        }
    }

    sqlite3_finalize(stmt);
    return flag;
}

static int guardar_visor_preferido(const char *viewer)
{
    asegurar_fila_settings();

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE settings SET image_viewer = ? WHERE id = 1"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, viewer ? viewer : "", -1, DB_TRANSIENT);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE;
}

#ifndef _WIN32
static int instalar_paquete_linux(const char *package_name)
{
    if (!is_safe_package_name(package_name))
    {
        return 0;
    }

    if (app_command_exists("apt-get"))
    {
        const char *update_argv[] = {"sudo", "apt-get", "update", NULL};
        const char *install_argv[] = {"sudo", "apt-get", "install", "-y", package_name, NULL};
        return run_command_posix_argv(update_argv) && run_command_posix_argv(install_argv);
    }

    if (app_command_exists("dnf"))
    {
        const char *argv[] = {"sudo", "dnf", "install", "-y", package_name, NULL};
        return run_command_posix_argv(argv);
    }

    if (app_command_exists("pacman"))
    {
        const char *argv[] = {"sudo", "pacman", "-Sy", "--noconfirm", package_name, NULL};
        return run_command_posix_argv(argv);
    }

    if (app_command_exists("zypper"))
    {
        const char *argv[] = {"sudo", "zypper", "--non-interactive", "install", package_name, NULL};
        return run_command_posix_argv(argv);
    }

    return 0;
}
#endif

static int construir_ruta_absoluta_imagen_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!db_get_image_path_by_id("camiseta", id, ruta_db, sizeof(ruta_db)))
    {
        return 0;
    }

    return db_resolve_image_absolute_path(ruta_db, ruta_absoluta, size);
}

static int abrir_imagen_en_sistema(const char *ruta)
{
    if (!ruta || ruta[0] == '\0')
    {
        return 0;
    }

    if (!app_validate_file_exists(ruta))
    {
        return 0;
    }

#ifdef _WIN32
    char viewer[64] = {0};
    obtener_visor_preferido(viewer, sizeof(viewer));

    if (viewer[0] != '\0' && _stricmp(viewer, "auto") != 0)
    {
        if (_stricmp(viewer, "mspaint") == 0)
        {
            if (app_open_with_command("mspaint", ruta))
            {
                return 1;
            }
        }
        else
        {
            {
                const char *cfmt = get_text("camiseta_visor_no_soportado_windows");
                char cbuf[256];
                snprintf(cbuf, sizeof(cbuf), cfmt, viewer);
                printf("%s", cbuf);
            }
        }
    }

    if (app_open_with_default_app(ruta))
    {
        return 1;
    }

    return app_open_with_command("mspaint", ruta);
#else
    char viewer[64] = {0};
    obtener_visor_preferido(viewer, sizeof(viewer));

    const char *visores[] = {"xdg-open", "gio", "feh", "eog", "gwenview", NULL};
    const char *visor = NULL;

    if (viewer[0] != '\0' && strcmp(viewer, "auto") != 0)
    {
        if (app_command_exists(viewer))
        {
            visor = viewer;
        }
        else
        {
            {
                const char *cfmt = get_text("camiseta_visor_no_instalado");
                char cbuf[256];
                snprintf(cbuf, sizeof(cbuf), cfmt, viewer);
                printf("%s", cbuf);
            }
        }
    }

    for (int i = 0; visores[i] != NULL; i++)
    {
        if (visor)
        {
            break;
        }

        if (app_command_exists(visores[i]))
        {
            visor = visores[i];
            break;
        }
    }

    if (!visor)
    {
        if (!confirmar(get_text("camiseta_instalar_visor_prompt")))
        {
            return 0;
        }

        printf("%s", get_text("camiseta_instalando_visor"));
        if (!instalar_paquete_linux("feh"))
        {
            printf("%s", get_text("camiseta_no_instalar_visor"));
            return 0;
        }

        visor = "feh";
        if (viewer[0] == '\0' || strcmp(viewer, "auto") == 0)
        {
            guardar_visor_preferido("feh");
        }
    }

    return app_open_with_command(visor, ruta);
#endif
}

static void configurar_visor_preferido_imagen(void)
{
    clear_screen();
    print_header(get_text("camiseta_config_visor_titulo"));

    char actual[64] = {0};
    obtener_visor_preferido(actual, sizeof(actual));
    {
        const char *cfmt = get_text("camiseta_visor_actual");
        char cbuf[320];
        snprintf(cbuf, sizeof(cbuf), cfmt, actual[0] ? actual : "auto");
        printf("%s", cbuf);
    }

    fputs(get_text("camiseta_visor_escribir"), stdout);
#ifdef _WIN32
    fputs(get_text("camiseta_visor_opciones_windows"), stdout);
#else
    printf("%s", get_text("camiseta_visor_opciones_linux"));
#endif

    char nuevo[64] = {0};
    input_string(get_text("camiseta_visor_input"), nuevo, (int)sizeof(nuevo));
    trim_whitespace(nuevo);

    if (nuevo[0] == '\0')
    {
        printf("No se realizaron cambios.\n");
        pause_console();
        return;
    }

#ifdef _WIN32
    if (_stricmp(nuevo, "auto") != 0 && _stricmp(nuevo, "mspaint") != 0)
    {
        printf("Visor no soportado en Windows. Usa 'auto' o 'mspaint'.\n");
        pause_console();
        return;
    }
#else
    if (strcasecmp(nuevo, "auto") != 0 && !app_command_exists(nuevo))
    {
        printf("No se encontro ese comando en el sistema.\n");
        pause_console();
        return;
    }
#endif

    const char *valor =
#ifdef _WIN32
        (_stricmp(nuevo, "auto") == 0) ? "" : nuevo;
#else
        (strcasecmp(nuevo, "auto") == 0) ? "" : nuevo;
#endif

    if (!guardar_visor_preferido(valor))
    {
        printf("No se pudo guardar la configuracion del visor.\n");
        pause_console();
        return;
    }

    printf("Visor guardado: %s\n", valor[0] ? valor : "auto");
    pause_console();
}

static int pedir_imagen_camiseta_y_resolver_ruta(char *ruta_absoluta, size_t size)
{
    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        return 0;
    }

    listar_camisetas_simple();
    int id = input_int("\nID de camiseta (0 para cancelar): ");
    if (id == 0)
    {
        return 0;
    }

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente.\n");
        return 0;
    }

    if (!construir_ruta_absoluta_imagen_por_id(id, ruta_absoluta, size))
    {
        printf("No se encontro imagen cargada en disco para esa camiseta.\n");
        return 0;
    }

    return 1;
}

static void previsualizar_imagen_camiseta_consola(void)
{
    clear_screen();
    print_header("PREVISUALIZAR IMAGEN (CONSOLA)");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_camiseta_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
        pause_console();
        return;
    }

    if (!app_validate_file_exists(ruta_absoluta))
    {
        printf("Ruta de imagen invalida o no existe.\n");
        pause_console();
        return;
    }

#ifdef _WIN32
    (void)ruta_absoluta;
    printf("La previsualizacion en consola con 'chafa' solo esta disponible en "
           "Linux/macOS.\n");
#else
    if (!app_command_exists("chafa"))
    {
        if (!confirmar("No se detecto 'chafa'. Desea instalarlo automaticamente?"))
        {
            pause_console();
            return;
        }

        printf("Instalando 'chafa'...\n");
        if (!instalar_paquete_linux("chafa"))
        {
            printf("No se pudo instalar 'chafa'.\n");
            pause_console();
            return;
        }
    }

    const char *chafa_argv[] = {"chafa", "--size", "80x40", ruta_absoluta, NULL};
    if (!run_command_posix_argv(chafa_argv))
    {
        printf("No se pudo previsualizar con chafa.\n");
        pause_console();
        return;
    }
#endif

    pause_console();
}

static void probar_visor_imagen_actual(void)
{
    clear_screen();
    print_header("PROBAR VISOR DE IMAGEN");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_camiseta_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
        pause_console();
        return;
    }

    if (!abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen con el visor actual.\n");
        pause_console();
        return;
    }

    printf("Visor ejecutado correctamente.\n");
    pause_console();
}

static void menu_ajustes_imagen_camiseta(void)
{
    MenuItem items[] = {{1, "Configurar visor", &configurar_visor_preferido_imagen},
        {2, "Probar visor", &probar_visor_imagen_actual},
        {3, "Previsualizar en consola", &previsualizar_imagen_camiseta_consola},
        {0, "Volver", NULL}
    };
    ejecutar_menu("AJUSTES IMAGEN", items, 4);
}

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static int obtener_total(const char *sql)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_step(stmt);
    int total = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

static int contar_partidos_por_camiseta(int camiseta_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM partido WHERE camiseta_id = ?"))
    {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, camiseta_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

static int contar_total_camisetas_activas(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM camiseta WHERE IFNULL(activa, 1) = 1"))
    {
        return -1;
    }

    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return total;
}

static int camiseta_esta_activa(int camiseta_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT IFNULL(activa, 1) FROM camiseta WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, camiseta_id);
    int activa = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activa = sqlite3_column_int(stmt, 0) == 1;
    }
    sqlite3_finalize(stmt);
    return activa;
}

static int
actualizar_estado_camiseta(int camiseta_id, // NOLINT(bugprone-easily-swappable-parameters)
                           int activa)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET activa = ? WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, activa ? 1 : 0);
    sqlite3_bind_int(stmt, 2, camiseta_id);
    int flag = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return flag;
}

static void listar_camisetas_excluyendo(int camiseta_excluida)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM camiseta WHERE id <> ? AND "
                       "IFNULL(activa, 1) = 1 ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, camiseta_excluida);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d - %s", sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
    {
        mostrar_no_hay_registros("camisetas destino");
    }
    sqlite3_finalize(stmt);
}

static int ejecutar_sql_simple(const char *sql)
{
    char *errmsg = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        if (errmsg)
        {
            printf("Error SQL: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
        return 0;
    }
    return 1;
}

static int confirmar_borrado_irreversible_camiseta(int camiseta_id, const char *detalle)
{
    char esperado[64] = {0};
    char ingreso[64] = {0};

    printf("\n================ ADVERTENCIA =================\n");
    printf("Esta operacion es IRREVERSIBLE.\n");
    if (detalle && detalle[0] != '\0')
    {
        printf("%s\n", detalle);
    }
    printf("=============================================\n\n");

    if (!confirmar("Desea continuar?"))
    {
        return 0;
    }

    snprintf(esperado, sizeof(esperado), "BORRAR CAMISETA %d", camiseta_id);
    printf("Para confirmar, escriba exactamente: %s\n", esperado);
    input_string("Confirmacion: ", ingreso, sizeof(ingreso));
    trim_whitespace(ingreso);

    if (strcmp(ingreso, esperado) != 0)
    {
        printf("Confirmacion incorrecta. Operacion cancelada.\n");
        pause_console();
        return 0;
    }

    return 1;
}

static int reasignar_partidos_y_eliminar_camiseta(int camiseta_origen, int camiseta_destino)
{
    if (!ejecutar_sql_simple("BEGIN IMMEDIATE TRANSACTION;"))
    {
        return 0;
    }

    sqlite3_stmt *stmt_update = NULL;
    sqlite3_stmt *stmt_delete = NULL;

    if (!preparar_stmt(&stmt_update, "UPDATE partido SET camiseta_id = ? WHERE camiseta_id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_update, 1, camiseta_destino);
    sqlite3_bind_int(stmt_update, 2, camiseta_origen);
    if (sqlite3_step(stmt_update) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_update);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_update);

    if (!preparar_stmt(&stmt_delete, "DELETE FROM camiseta WHERE id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete, 1, camiseta_origen);
    if (sqlite3_step(stmt_delete) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_delete);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_delete);

    if (!ejecutar_sql_simple("COMMIT;"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }

    return 1;
}

static int eliminar_camiseta_y_partidos_asociados(int camiseta_id)
{
    if (!ejecutar_sql_simple("BEGIN IMMEDIATE TRANSACTION;"))
    {
        return 0;
    }

    sqlite3_stmt *stmt_delete_partidos = NULL;
    sqlite3_stmt *stmt_delete_camiseta = NULL;

    if (!preparar_stmt(&stmt_delete_partidos, "DELETE FROM partido WHERE camiseta_id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete_partidos, 1, camiseta_id);
    if (sqlite3_step(stmt_delete_partidos) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_delete_partidos);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_delete_partidos);

    if (!preparar_stmt(&stmt_delete_camiseta, "DELETE FROM camiseta WHERE id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete_camiseta, 1, camiseta_id);
    if (sqlite3_step(stmt_delete_camiseta) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_delete_camiseta);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_delete_camiseta);

    if (!ejecutar_sql_simple("COMMIT;"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }

    return 1;
}

static void solicitar_nombre_camiseta(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string_extended(prompt, buffer, size);
        trim_whitespace(buffer);

        if (buffer[0] != '\0')
        {
            return;
        }
        printf("El nombre no puede estar vacio.\n");
    }
}

typedef struct
{
    char nombre[100];
    char color_principal[50];
    char color_secundario[50];
    char marca[60];
    char modelo[80];
    char temporada[40];
    char estado_fisico[40];
    char fecha_compra[20];
    int costo_centavos;
    char observaciones[240];
    char proveedor[120];
    int fue_regalo;
    char regalo_de[100];
    int activa;
} CamisetaInfoDetalle;

static const char *texto_o_defecto(const char *valor, const char *defecto)
{
    if (valor && valor[0] != '\0')
    {
        return valor;
    }
    return defecto;
}

static void solicitar_campo_no_vacio(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string_extended(prompt, buffer, size);
        trim_whitespace(buffer);

        if (buffer[0] != '\0')
        {
            return;
        }

        printf("El campo no puede estar vacio.\n");
    }
}

static int solicitar_si_no(const char *titulo)
{
    while (1)
    {
        printf("%s\n", titulo);
        printf("1) Si\n");
        printf("2) No\n");
        int opcion = input_int("Opcion: ");
        if (opcion == 1)
        {
            return 1;
        }
        if (opcion == 2)
        {
            return 0;
        }
        printf("Opcion invalida.\n");
    }
}

static void solicitar_estado_fisico(char *buffer, int size)
{
    while (1)
    {
        printf("Estado fisico de la camiseta:\n");
        printf("1) Nueva\n");
        printf("2) Buena\n");
        printf("3) Desgastada\n");
        printf("4) Rota\n");
        printf("5) Otra\n");

        int opcion = input_int("Opcion: ");
        switch (opcion)
        {
        case 1:
            snprintf(buffer, size, "Nueva");
            return;
        case 2:
            snprintf(buffer, size, "Buena");
            return;
        case 3:
            snprintf(buffer, size, "Desgastada");
            return;
        case 4:
            snprintf(buffer, size, "Rota");
            return;
        case 5:
            solicitar_campo_no_vacio("Estado fisico (texto): ", buffer, size);
            return;
        default:
            printf("Opcion invalida.\n");
            break;
        }
    }
}

static int solicitar_costo_centavos(const char *prompt)
{
    while (1)
    {
        double monto = input_double(prompt);
        if (monto < 0.0)
        {
            printf("El monto no puede ser negativo.\n");
            continue;
        }
        return (int)(monto * 100.0 + 0.5);
    }
}

static int cargar_info_camiseta_detalle(int id, CamisetaInfoDetalle *info)
{
    if (!info)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT nombre, IFNULL(color_principal, ''), "
                       "IFNULL(color_secundario, ''), IFNULL(marca, ''), "
                       "IFNULL(modelo, ''), IFNULL(temporada, ''), IFNULL(estado_fisico, "
                       "''), IFNULL(fecha_compra, ''), "
                       "IFNULL(costo_centavos, 0), IFNULL(observaciones, ''), "
                       "IFNULL(proveedor, ''), "
                       "IFNULL(fue_regalo, 0), IFNULL(regalo_de, ''), IFNULL(activa, 1) "
                       "FROM camiseta WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    snprintf(info->nombre, sizeof(info->nombre), "%s", (const char *)sqlite3_column_text(stmt, 0));
    snprintf(info->color_principal, sizeof(info->color_principal), "%s",
             (const char *)sqlite3_column_text(stmt, 1));
    snprintf(info->color_secundario, sizeof(info->color_secundario), "%s",
             (const char *)sqlite3_column_text(stmt, 2));
    snprintf(info->marca, sizeof(info->marca), "%s", (const char *)sqlite3_column_text(stmt, 3));
    snprintf(info->modelo, sizeof(info->modelo), "%s", (const char *)sqlite3_column_text(stmt, 4));
    snprintf(info->temporada, sizeof(info->temporada), "%s",
             (const char *)sqlite3_column_text(stmt, 5));
    snprintf(info->estado_fisico, sizeof(info->estado_fisico), "%s",
             (const char *)sqlite3_column_text(stmt, 6));
    snprintf(info->fecha_compra, sizeof(info->fecha_compra), "%s",
             (const char *)sqlite3_column_text(stmt, 7));
    info->costo_centavos = sqlite3_column_int(stmt, 8);
    snprintf(info->observaciones, sizeof(info->observaciones), "%s",
             (const char *)sqlite3_column_text(stmt, 9));
    snprintf(info->proveedor, sizeof(info->proveedor), "%s",
             (const char *)sqlite3_column_text(stmt, 10));
    info->fue_regalo = sqlite3_column_int(stmt, 11) == 1;
    snprintf(info->regalo_de, sizeof(info->regalo_de), "%s",
             (const char *)sqlite3_column_text(stmt, 12));
    info->activa = sqlite3_column_int(stmt, 13) == 1;

    sqlite3_finalize(stmt);
    return 1;
}

static int camiseta_necesita_completar_info(const CamisetaInfoDetalle *info)
{
    if (!info)
    {
        return 0;
    }

    return info->color_principal[0] == '\0' && info->color_secundario[0] == '\0' &&
           info->marca[0] == '\0' && info->modelo[0] == '\0' && info->temporada[0] == '\0' &&
           info->estado_fisico[0] == '\0' && info->fecha_compra[0] == '\0' &&
           info->costo_centavos == 0 && info->observaciones[0] == '\0' &&
           info->proveedor[0] == '\0' && info->fue_regalo == 0 && info->regalo_de[0] == '\0';
}

static int completar_informacion_camiseta(int id)
{
    char color_principal[50];
    char color_secundario[50];
    char marca[60];
    char modelo[80];
    char temporada[40];
    char estado_fisico[40];
    char fecha_compra[20];
    int costo_centavos;
    char observaciones[240];
    char proveedor[120];
    int fue_regalo;
    char regalo_de[100] = {0};

    solicitar_campo_no_vacio("Color principal: ", color_principal, sizeof(color_principal));
    solicitar_campo_no_vacio("Color secundario: ", color_secundario, sizeof(color_secundario));
    solicitar_campo_no_vacio("Marca: ", marca, sizeof(marca));
    solicitar_campo_no_vacio("Modelo: ", modelo, sizeof(modelo));
    solicitar_campo_no_vacio("Temporada (ej: 2026): ", temporada, sizeof(temporada));
    solicitar_estado_fisico(estado_fisico, sizeof(estado_fisico));
    solicitar_campo_no_vacio("Fecha de compra (ej: 2026-04-22): ", fecha_compra,
                             sizeof(fecha_compra));
    costo_centavos = solicitar_costo_centavos("Costo de compra: ");
    input_string_extended("Observaciones: ", observaciones, sizeof(observaciones));
    trim_whitespace(observaciones);
    solicitar_campo_no_vacio("Proveedor: ", proveedor, sizeof(proveedor));
    fue_regalo = solicitar_si_no("Fue un regalo?");
    if (fue_regalo)
    {
        solicitar_campo_no_vacio("Regalo de (ej: Ivan): ", regalo_de, sizeof(regalo_de));
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET color_principal = ?, color_secundario = "
                       "?, marca = ?, modelo = ?, "
                       "temporada = ?, estado_fisico = ?, fecha_compra = ?, "
                       "costo_centavos = ?, observaciones = ?, "
                       "proveedor = ?, fue_regalo = ?, regalo_de = ? WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, color_principal, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_text(stmt, 2, color_secundario, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_text(stmt, 3, marca, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_text(stmt, 4, modelo, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_text(stmt, 5, temporada, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_text(stmt, 6, estado_fisico, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_text(stmt, 7, fecha_compra, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_int(stmt, 8, costo_centavos);
    sqlite3_bind_text(stmt, 9, observaciones, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_text(stmt, 10, proveedor, -1, // NOLINTNEXTLINE(performance-no-int-to-ptr)
                      DB_TRANSIENT);
    sqlite3_bind_int(stmt, 11, fue_regalo ? 1 : 0); // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 12, regalo_de, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 13, id);

    int flag = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return flag;
}

static void imprimir_info_camiseta_detalle(int id, const CamisetaInfoDetalle *info)
{
    int uso_partidos = contar_partidos_por_camiseta(id);
    if (uso_partidos < 0)
    {
        uso_partidos = 0;
    }

    printf("========================================\n");
    printf("ID                 : %d\n", id);
    printf("Nombre             : %s\n", texto_o_defecto(info->nombre, "(sin dato)"));
    printf("Color Principal    : %s\n", texto_o_defecto(info->color_principal, "(sin dato)"));
    printf("Color Secundario   : %s\n", texto_o_defecto(info->color_secundario, "(sin dato)"));
    printf("Marca              : %s\n", texto_o_defecto(info->marca, "(sin dato)"));
    printf("Modelo             : %s\n", texto_o_defecto(info->modelo, "(sin dato)"));
    printf("Temporada          : %s\n", texto_o_defecto(info->temporada, "(sin dato)"));
    printf("Estado Fisico      : %s\n", texto_o_defecto(info->estado_fisico, "(sin dato)"));
    printf("Fecha Compra       : %s\n", texto_o_defecto(info->fecha_compra, "(sin dato)"));
    printf("Costo Compra       : %.2f\n", (double)info->costo_centavos / 100.0);
    printf("Uso Acumulado      : %d partido(s)\n", uso_partidos);
    printf("Observaciones      : %s\n", texto_o_defecto(info->observaciones, "(sin dato)"));
    printf("Proveedor          : %s\n", texto_o_defecto(info->proveedor, "(sin dato)"));
    printf("Regalo             : %s\n", info->fue_regalo ? "SI" : "NO");
    if (info->fue_regalo)
    {
        printf("Regalo De          : %s\n", texto_o_defecto(info->regalo_de, "(sin dato)"));
    }
    printf("Estado             : %s\n", info->activa ? "ACTIVA" : "INACTIVA");
    printf("========================================\n");
}

static int listar_camisetas_con_info_pendiente(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM camiseta ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return -1;
    }

    int pendientes = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);

        CamisetaInfoDetalle info;
        if (!cargar_info_camiseta_detalle(id, &info))
        {
            continue;
        }

        if (camiseta_necesita_completar_info(&info))
        {
            ui_printf_centered_line("%d - %s", id, nombre);
            pendientes++;
        }
    }

    sqlite3_finalize(stmt);
    return pendientes;
}

static void listar_camisetas_simple(void)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt, "SELECT id, nombre, IFNULL(activa, 1) FROM camiseta ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *estado = sqlite3_column_int(stmt, 2) == 1 ? "ACTIVA" : "INACTIVA";
        ui_printf_centered_line("%d - %s [%s]", sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1), estado);
        hay = 1;
    }

    if (!hay)
    {
        mostrar_no_hay_registros("camisetas cargadas");
    }
    sqlite3_finalize(stmt);
}

static void crear_camiseta_simple(void)
{
    char nombre[50];
    solicitar_nombre_camiseta("Nombre y Numero: ", nombre, sizeof(nombre));

    long long id = obtener_siguiente_id("camiseta");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "INSERT INTO camiseta(id, nombre) VALUES(?, ?)"))
    {
        printf("Error al crear la camiseta.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 2, nombre, -1, DB_TRANSIENT);
    int flag = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (flag == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada camiseta id=%lld nombre=%.180s (simple)", id,
                 nombre);
        app_log_event("CAMISETA", log_msg);

        if (confirmar("Desea cargar imagen para esta camiseta ahora?") && !cargar_imagen_para_camiseta_id((int)id))
        {
            printf("No se pudo cargar la imagen en este momento.\n");
        }

        mostrar_alerta_operacion("Camiseta", "Creada (Simple)", nombre);
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear camiseta nombre=%.180s (simple)",
                 nombre);
        app_log_event("CAMISETA", log_msg);
        printf("\nError al crear la camiseta en la base de datos.\n");
        pause_console();
    }
}

static void crear_camiseta_avanzada(void)
{
    CamisetaInfoDetalle info;

    char nombre[50];
    solicitar_nombre_camiseta("Nombre y Numero: ", nombre, sizeof(nombre));
    snprintf(info.nombre, sizeof(info.nombre), "%s", nombre);

    solicitar_campo_no_vacio("Color principal: ", info.color_principal,
                             sizeof(info.color_principal));
    solicitar_campo_no_vacio("Color secundario: ", info.color_secundario,
                             sizeof(info.color_secundario));
    solicitar_campo_no_vacio("Marca: ", info.marca, sizeof(info.marca));
    solicitar_campo_no_vacio("Modelo: ", info.modelo, sizeof(info.modelo));
    solicitar_campo_no_vacio("Temporada (ej: 2026): ", info.temporada, sizeof(info.temporada));
    solicitar_estado_fisico(info.estado_fisico, sizeof(info.estado_fisico));
    solicitar_campo_no_vacio("Fecha de compra (ej: 2026-04-22): ", info.fecha_compra,
                             sizeof(info.fecha_compra));
    info.costo_centavos = solicitar_costo_centavos("Costo de compra: ");
    input_string_extended("Observaciones: ", info.observaciones, sizeof(info.observaciones));
    trim_whitespace(info.observaciones);
    solicitar_campo_no_vacio("Proveedor: ", info.proveedor, sizeof(info.proveedor));
    info.fue_regalo = solicitar_si_no("Fue un regalo?");
    info.regalo_de[0] = '\0';
    if (info.fue_regalo)
    {
        solicitar_campo_no_vacio("Regalo de (ej: Ivan): ", info.regalo_de, sizeof(info.regalo_de));
    }

    long long id = obtener_siguiente_id("camiseta");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "INSERT INTO camiseta(id, nombre, color_principal, "
                       "color_secundario, marca, modelo, temporada, "
                       "estado_fisico, fecha_compra, costo_centavos, "
                       "observaciones, proveedor, fue_regalo, regalo_de) "
                       "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"))
    {
        printf("Error al crear la camiseta.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 2, info.nombre, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 3, info.color_principal, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 4, info.color_secundario, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 5, info.marca, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 6, info.modelo, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 7, info.temporada, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 8, info.estado_fisico, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 9, info.fecha_compra, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 10, info.costo_centavos);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 11, info.observaciones, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 12, info.proveedor, -1, DB_TRANSIENT);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_int(stmt, 13, info.fue_regalo ? 1 : 0);
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    sqlite3_bind_text(stmt, 14, info.regalo_de, -1, DB_TRANSIENT);
    int flag = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (flag == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada camiseta id=%lld nombre=%.180s (avanzada)", id,
                 nombre);
        app_log_event("CAMISETA", log_msg);

        int desea_cargar_imagen = confirmar("Desea cargar imagen para esta camiseta ahora?");
        if (desea_cargar_imagen)
        {
            if (!cargar_imagen_para_camiseta_id((int)id))
            {
                printf("No se pudo cargar la imagen en este momento.\n");
                snprintf(log_msg, sizeof(log_msg),
                         "Camiseta id=%lld creada, pero fallo carga de imagen inicial", id);
                app_log_event("CAMISETA", log_msg);
            }
            else
            {
                snprintf(log_msg, sizeof(log_msg), "Camiseta id=%lld creada con imagen inicial",
                         id);
                app_log_event("CAMISETA", log_msg);
            }
        }
        else
        {
            snprintf(log_msg, sizeof(log_msg),
                     "Camiseta id=%lld creada sin imagen inicial (opcional)", id);
            app_log_event("CAMISETA", log_msg);
        }

        mostrar_alerta_operacion("Camiseta", "Creada", nombre);
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear camiseta nombre=%.180s", nombre);
        app_log_event("CAMISETA", log_msg);
        printf("\nError al crear la camiseta en la base de datos.\n");
        pause_console();
    }
}

void crear_camiseta(void)
{
    clear_screen();
    print_header("CREAR CAMISETA");

    printf("Seleccione modo de carga:\n");
    printf("1) Carga Simple (Solo nombre y numero)\n");
    printf("2) Carga Avanzada (Nombre y todos los datos)\n");
    printf("0) Cancelar\n");
    int opcion = input_int("Opcion: ");
    if (opcion == 0)
    {
        return;
    }
    if (opcion == 1)
    {
        crear_camiseta_simple();
    }
    else if (opcion == 2)
    {
        crear_camiseta_avanzada();
    }
    else
    {
        printf("Opcion invalida.\n");
    }
}

static int cargar_imagen_para_camiseta_id(int id)
{
    return app_cargar_imagen_entidad(id, "camiseta", "mifutbol_imagen_sel.txt");
}

static void listar_camisetas_con_stats(void)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT c.id, c.nombre, IFNULL(c.activa, 1), "
                      "COUNT(p.id), "
                      "IFNULL(SUM(p.goles), 0), "
                      "IFNULL(SUM(p.asistencias), 0), "
                      "IFNULL(SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END), 0), "
                      "IFNULL(SUM(CASE WHEN p.resultado = 2 THEN 1 ELSE 0 END), 0), "
                      "IFNULL(SUM(CASE WHEN p.resultado = 3 THEN 1 ELSE 0 END), 0) "
                      "FROM camiseta c "
                      "LEFT JOIN partido p ON c.id = p.camiseta_id "
                      "WHERE IFNULL(c.activa, 1) = 1 "
                      "GROUP BY c.id, c.nombre "
                      "ORDER BY c.id;";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int usar_unicode = consola_soporta_unicode();
    const char *sep = usar_unicode ? " \u2502 " : " | ";

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        hay |= ui_print_stats_row_from_stmt(stmt, sep);
    }

    if (!hay)
    {
        mostrar_no_hay_registros("camisetas cargadas");
    }
    sqlite3_finalize(stmt);
}

void listar_camisetas(void)
{
    clear_screen();
    print_header("LISTADO DE CAMISETAS");

    app_log_event("CAMISETA", "Listado de camisetas consultado");

    listar_camisetas_con_stats();
    pause_console();
}

void editar_camiseta(void)
{
    clear_screen();
    print_header("EDITAR CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas para editar");
        pause_console();
        return;
    }

    ui_printf_centered_line("Camisetas disponibles:");
    ui_printf("\n");
    listar_camisetas_simple();

    int id = input_int("\nID a editar (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }
    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    char nombre[100];
    solicitar_nombre_camiseta("Nuevo nombre: ", nombre, sizeof(nombre));

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET nombre=? WHERE id=?"))
    {
        printf("Error al actualizar la camiseta.\n");
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Editada camiseta id=%d nuevo_nombre=%.180s", id, nombre);
    app_log_event("CAMISETA", log_msg);

    mostrar_alerta_operacion("Camiseta", "Modificada", nombre);
}

void cargar_imagen_camiseta(void)
{
    clear_screen();
    print_header("CARGAR IMAGEN DE CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        pause_console();
        return;
    }

    listar_camisetas_simple();
    int id = input_int("\nID de camiseta (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    if (!cargar_imagen_para_camiseta_id(id))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    pause_console();
}

void ver_imagen_camiseta(void)
{
    clear_screen();
    print_header("VER IMAGEN DE CAMISETA");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_camiseta_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
        pause_console();
        return;
    }

    if (!abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen en el sistema.\n");
        pause_console();
        return;
    }

    printf("Abriendo imagen...\n");
    pause_console();
}

static void procesar_eliminacion_reasignando_partidos(int id, int partidos_asociados)
{
    int total_camisetas = contar_total_camisetas_activas();
    if (total_camisetas <= 1)
    {
        printf("No hay otra camiseta activa disponible para reasignar partidos.\n");
        pause_console();
        return;
    }

    printf("\nCamisetas activas disponibles para reasignar:\n");
    listar_camisetas_excluyendo(id);
    printf("\n");

    int camiseta_destino = input_int("ID camiseta destino (0 para cancelar): ");
    if (camiseta_destino == 0)
    {
        return;
    }

    if (camiseta_destino == id || !existe_id("camiseta", camiseta_destino) ||
            !camiseta_esta_activa(camiseta_destino))
    {
        printf("Camiseta destino invalida.\n");
        pause_console();
        return;
    }

    char detalle[180] = {0};
    snprintf(detalle, sizeof(detalle),
             "Se reasignaran %d partido(s) a la camiseta %d y se eliminara la "
             "camiseta %d.",
             partidos_asociados, camiseta_destino, id);
    if (!confirmar_borrado_irreversible_camiseta(id, detalle))
    {
        return;
    }

    if (!reasignar_partidos_y_eliminar_camiseta(id, camiseta_destino))
    {
        printf("No se pudo completar la reasignacion y eliminacion.\n");
        pause_console();
        return;
    }

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Eliminada camiseta id=%d con reasignacion a id=%d", id,
             camiseta_destino);
    app_log_event("CAMISETA", log_msg);

    mostrar_alerta_operacion("Camiseta", "Reasignada y Eliminada", NULL);
}

static void procesar_eliminacion_con_partidos_asociados(int id, int partidos_asociados)
{
    char detalle[180] = {0};
    snprintf(detalle, sizeof(detalle),
             "Se eliminara la camiseta %d y TODOS sus %d partido(s) asociados.", id,
             partidos_asociados);
    if (!confirmar_borrado_irreversible_camiseta(id, detalle))
    {
        return;
    }

    if (!eliminar_camiseta_y_partidos_asociados(id))
    {
        printf("No se pudo eliminar la camiseta y sus partidos asociados.\n");
        pause_console();
        return;
    }

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Eliminada camiseta id=%d junto a partidos asociados", id);
    app_log_event("CAMISETA", log_msg);

    mostrar_alerta_operacion("Camiseta", "Eliminada con Partidos Asociados", NULL);
}

static void procesar_retiro_camiseta(int id)
{
    if (!confirmar("Se retirara la camiseta: no aparecera para partidos nuevos, "
                   "pero conservara historial. Continuar?"))
    {
        return;
    }

    if (!actualizar_estado_camiseta(id, 0))
    {
        printf("No se pudo marcar la camiseta como inactiva.\n");
        pause_console();
        return;
    }

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Retirada camiseta id=%d (inactiva)", id);
    app_log_event("CAMISETA", log_msg);

    mostrar_alerta_operacion("Camiseta", "Retirada (Inactiva)", NULL);
}

static void procesar_camiseta_con_partidos(int id, int partidos_asociados)
{
    printf("La camiseta esta asociada a %d partido(s).\n", partidos_asociados);
    printf("Elija una opcion:\n");
    printf("1) Reasignar esos partidos a otra camiseta y eliminar esta camiseta\n");
    printf("2) Eliminar esta camiseta y TODOS los partidos asociados\n");
    printf("3) Retirar camiseta (marcar INACTIVA y conservar historial)\n");
    printf("0) Cancelar\n");

    int opcion = input_int("Opcion: ");
    if (opcion == 0)
    {
        return;
    }

    switch (opcion)
    {
    case 1:
        procesar_eliminacion_reasignando_partidos(id, partidos_asociados);
        return;
    case 2:
        procesar_eliminacion_con_partidos_asociados(id, partidos_asociados);
        return;
    case 3:
        procesar_retiro_camiseta(id);
        return;
    default:
        printf("Opcion invalida.\n");
        pause_console();
        return;
    }
}

static void eliminar_camiseta_sin_partidos(int id)
{
    if (!confirmar_borrado_irreversible_camiseta(id, "Se eliminara la camiseta seleccionada."))
    {
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "DELETE FROM camiseta WHERE id=?"))
    {
        printf("Error al eliminar la camiseta.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Eliminada camiseta id=%d", id);
    app_log_event("CAMISETA", log_msg);

    mostrar_alerta_operacion("Camiseta", "Eliminada", NULL);
}

void eliminar_camiseta(void)
{
    clear_screen();
    print_header("ELIMINAR CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas para eliminar");
        pause_console();
        return;
    }

    ui_printf_centered_line("Camisetas disponibles:");
    ui_printf("\n");
    listar_camisetas_simple();

    int id = input_int("\nID a eliminar (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }
    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    if (!camiseta_esta_activa(id))
    {
        printf("La camiseta seleccionada ya esta inactiva.\n");
        pause_console();
        return;
    }

    int partidos_asociados = contar_partidos_por_camiseta(id);
    if (partidos_asociados < 0)
    {
        printf("No se pudo verificar si la camiseta esta asociada a partidos.\n");
        pause_console();
        return;
    }

    if (partidos_asociados > 0)
    {
        procesar_camiseta_con_partidos(id, partidos_asociados);
        return;
    }

    eliminar_camiseta_sin_partidos(id);
}

static void reactivar_camiseta(void)
{
    clear_screen();
    print_header("REACTIVAR / DESACTIVAR CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        pause_console();
        return;
    }

    listar_camisetas_simple();
    printf("\n");

    int id = input_int("ID de camiseta (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }
    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    int esta_activa = camiseta_esta_activa(id);
    int nuevo_estado = esta_activa ? 0 : 1;

    if (esta_activa)
    {
        if (!confirmar("Desea desactivar esta camiseta?"))
        {
            return;
        }
    }
    else
    {
        if (!confirmar("Desea reactivar esta camiseta?"))
        {
            return;
        }
    }

    if (!actualizar_estado_camiseta(id, nuevo_estado))
    {
        printf("No se pudo actualizar el estado de la camiseta.\n");
        pause_console();
        return;
    }

    char log_msg[256];
    if (nuevo_estado == 1)
    {
        snprintf(log_msg, sizeof(log_msg), "Reactivada camiseta id=%d", id);
        app_log_event("CAMISETA", log_msg);
        mostrar_alerta_operacion("Camiseta", "Reactivada", NULL);
    }
    else
    {
        snprintf(log_msg, sizeof(log_msg), "Desactivada camiseta id=%d", id);
        app_log_event("CAMISETA", log_msg);
        mostrar_alerta_operacion("Camiseta", "Desactivada (Inactiva)", NULL);
    }
}

static void ver_informacion_camiseta(void)
{
    clear_screen();
    print_header("INFORMACION DE CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        pause_console();
        return;
    }

    listar_camisetas_simple();
    printf("\n");

    int id = input_int("ID Camiseta para ver informacion (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    CamisetaInfoDetalle info;
    if (!cargar_info_camiseta_detalle(id, &info))
    {
        printf("No se pudo recuperar la informacion de la camiseta.\n");
        pause_console();
        return;
    }

    imprimir_info_camiseta_detalle(id, &info);
    pause_console();
}

static void cargar_informacion_camiseta(void)
{
    clear_screen();
    print_header("CARGAR INFORMACION DE CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        pause_console();
        return;
    }

    printf("Camisetas con informacion pendiente:\n");
    int pendientes = listar_camisetas_con_info_pendiente();
    if (pendientes < 0)
    {
        pause_console();
        return;
    }

    if (pendientes == 0)
    {
        printf("No hay camisetas con informacion pendiente.\n");
        pause_console();
        return;
    }

    printf("\n");
    int id = input_int("ID Camiseta para cargar informacion (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    CamisetaInfoDetalle info;
    if (!cargar_info_camiseta_detalle(id, &info))
    {
        printf("No se pudo recuperar la informacion actual de la camiseta.\n");
        pause_console();
        return;
    }

    if (!camiseta_necesita_completar_info(&info))
    {
        printf("La camiseta seleccionada no tiene informacion pendiente.\n");
        pause_console();
        return;
    }

    if (!completar_informacion_camiseta(id))
    {
        printf("No se pudo completar la informacion de la camiseta.\n");
        pause_console();
        return;
    }

    mostrar_alerta_operacion("Camiseta", "Informacion Cargada", info.nombre);
}

static void reiniciar_sorteo(void)
{
    sqlite3_exec(db, "UPDATE camiseta SET sorteada = 0 WHERE IFNULL(activa, 1) = 1", 0, 0, 0);
    printf("Todas las camisetas han sido sorteadas. Reiniciando sorteo...\n\n");
}

static void marcar_camiseta_sorteada(int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET sorteada = 1 WHERE id = ?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static char *obtener_nombre_camiseta(int id)
{
    char nombre_buffer[256];

    if (obtener_nombre_entidad("camiseta", id, nombre_buffer, sizeof(nombre_buffer)))
    {
        return strdup(nombre_buffer);
    }

    return strdup("Desconocida");
}

void sortear_camiseta(void)
{
    clear_screen();
    print_header("SORTEO DE CAMISETAS");

    int disponibles = obtener_total("SELECT COUNT(*) FROM camiseta WHERE "
                                    "sorteada = 0 AND IFNULL(activa, 1) = 1");

    if (disponibles == 0)
    {
        reiniciar_sorteo();
        disponibles = obtener_total("SELECT COUNT(*) FROM camiseta WHERE IFNULL(activa, 1) = 1");
    }

    if (disponibles == 0)
    {
        printf("No hay camisetas para sortear.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt_sel;
    int offset = secure_rand_range(disponibles);
    if (!preparar_stmt(&stmt_sel, "SELECT id FROM camiseta WHERE sorteada = 0 "
                       "AND IFNULL(activa, 1) = 1 LIMIT 1 OFFSET ?"))
    {
        printf("Error al seleccionar camiseta aleatoria.\n");
        pause_console();
        return;
    }
    int seleccionado = -1;
    sqlite3_bind_int(stmt_sel, 1, offset);
    if (sqlite3_step(stmt_sel) == SQLITE_ROW)
    {
        seleccionado = sqlite3_column_int(stmt_sel, 0);
    }
    sqlite3_finalize(stmt_sel);

    if (seleccionado == -1)
    {
        printf("Error al seleccionar camiseta aleatoria.\n");
        pause_console();
        return;
    }

    marcar_camiseta_sorteada(seleccionado);
    char *nombre = obtener_nombre_camiseta(seleccionado);

    printf("CAMISETA SORTEADA!\n\n");
    printf("La camiseta seleccionada es: %s\n", nombre);
    printf("Quedan %d camisetas por sortear.\n", disponibles - 1);

    free(nombre);
    pause_console();
}

void menu_camisetas(void)
{
    MenuItem items[] = {{1, "Crear", &crear_camiseta},
        {2, "Listar", &listar_camisetas},
        {3, "Modificar", &editar_camiseta},
        {4, "Eliminar", &eliminar_camiseta},
        {5, "Sortear", &sortear_camiseta},
        {6, "Cargar Imagen", &cargar_imagen_camiseta},
        {7, "Ver Camiseta", &ver_imagen_camiseta},
        {8, "Ajustes Imagen", &menu_ajustes_imagen_camiseta},
        {9, "Ver Informacion", &ver_informacion_camiseta},
        {10, "Cargar Informacion", &cargar_informacion_camiseta},
        {11, "Reactivar/Desactivar", &reactivar_camiseta},
        {0, "Volver", NULL}
    };
    ejecutar_menu("CAMISETAS", items, 12);
}
