#include "botin.h"
#include "db.h"
#include "menu.h"
#include "random_utils.h"
#include "settings.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static int obtener_botin_predeterminado(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT IFNULL(botin_predeterminado, 0) FROM settings WHERE id = 1"))
    {
        return 0;
    }

    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

int botin_obtener_predeterminado(void)
{
    return obtener_botin_predeterminado();
}

static void listar_botines_simple(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id, nombre, IFNULL(activa, 1) FROM botin ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int predet = obtener_botin_predeterminado();

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        const char *estado = sqlite3_column_int(stmt, 2) == 1 ? "ACTIVO" : "INACTIVO";
        if (id == predet)
        {
            ui_printf_centered_line("%d - %s [%s] (PREDETERMINADO)", id, nombre, estado);
        }
        else
        {
            ui_printf_centered_line("%d - %s [%s]", id, nombre, estado);
        }
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("botines cargados");

    sqlite3_finalize(stmt);
}

static int contar_partidos_por_botin(int botin_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM partido WHERE botin_id = ?"))
    {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, botin_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

void crear_botin(void)
{
    clear_screen();
    print_header("CREAR BOTIN");

    char nombre[100];
    input_string("Nombre del botin: ", nombre, sizeof(nombre));
    trim_whitespace(nombre);
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return;
    }

    long long id = obtener_siguiente_id("botin");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "INSERT INTO botin(id, nombre) VALUES(?, ?)"))
    {
        printf("Error al crear el botin.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creado botin id=%lld nombre=%.180s", id, nombre);
        app_log_event("BOTIN", log_msg);
        mostrar_alerta_operacion("Botin", "Creado", nombre);
    }
    else
    {
        printf("\nError al crear el botin en la base de datos.\n");
        pause_console();
    }
}

void listar_botines(void)
{
    clear_screen();
    print_header("LISTADO DE BOTINES");

    app_log_event("BOTIN", "Listado de botines consultado");

    listar_botines_simple();
    pause_console();
}

void editar_botin(void)
{
    clear_screen();
    print_header("EDITAR BOTIN");

    if (!hay_registros("botin"))
    {
        mostrar_no_hay_registros("botines para editar");
        pause_console();
        return;
    }

    ui_printf_centered_line("Botines disponibles:");
    ui_printf("\n");
    listar_botines_simple();

    int id = input_int("\nID a editar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("botin", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    char nombre[100];
    input_string("Nuevo nombre: ", nombre, sizeof(nombre));
    trim_whitespace(nombre);
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE botin SET nombre=? WHERE id=?"))
    {
        printf("Error al actualizar el botin.\n");
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Editado botin id=%d nuevo_nombre=%.180s", id, nombre);
    app_log_event("BOTIN", log_msg);

    mostrar_alerta_operacion("Botin", "Modificado", nombre);
}

void eliminar_botin(void)
{
    clear_screen();
    print_header("ELIMINAR BOTIN");

    if (!hay_registros("botin"))
    {
        mostrar_no_hay_registros("botines para eliminar");
        pause_console();
        return;
    }

    ui_printf_centered_line("Botines disponibles:");
    ui_printf("\n");
    listar_botines_simple();

    int id = input_int("\nID a eliminar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("botin", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    int partidos_asociados = contar_partidos_por_botin(id);

    if (partidos_asociados > 0)
    {
        printf("El botin esta asociado a %d partido(s). No se puede eliminar.\n",
               partidos_asociados);
        pause_console();
        return;
    }

    if (!confirmar("Esta seguro de eliminar este botin?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "DELETE FROM botin WHERE id=?"))
    {
        printf("Error al eliminar el botin.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    int predet = obtener_botin_predeterminado();
    if (id == predet)
    {
        sqlite3_stmt *s;
        if (preparar_stmt(&s, "UPDATE settings SET botin_predeterminado = 0 WHERE id = 1"))
        {
            sqlite3_step(s);
            sqlite3_finalize(s);
        }
    }

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Eliminado botin id=%d", id);
    app_log_event("BOTIN", log_msg);

    mostrar_alerta_operacion("Botin", "Eliminado", NULL);
}

void fijar_botin_predeterminado(void)
{
    clear_screen();
    print_header("FIJAR BOTIN PREDETERMINADO");

    if (!hay_registros("botin"))
    {
        mostrar_no_hay_registros("botines");
        pause_console();
        return;
    }

    listar_botines_simple();
    printf("\n");

    int id = input_int("ID del botin a usar como predeterminado (0 para quitar): ");
    if (id == 0)
    {
        sqlite3_stmt *stmt;
        if (preparar_stmt(&stmt, "UPDATE settings SET botin_predeterminado = 0 WHERE id = 1"))
        {
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        printf("Botin predeterminado eliminado.\n");
        pause_console();
        return;
    }

    if (!existe_id("botin", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE settings SET botin_predeterminado = ? WHERE id = 1"))
    {
        printf("Error al guardar la configuracion.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Fijado botin predeterminado id=%d", id);
    app_log_event("BOTIN", log_msg);

    mostrar_alerta_operacion("Botin", "Fijado como Predeterminado", NULL);
}

static int construir_ruta_absoluta_imagen_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
        return 0;

    char ruta_db[300] = {0};
    if (!db_get_image_path_by_id("botin", id, ruta_db, sizeof(ruta_db)))
        return 0;

    return db_resolve_image_absolute_path(ruta_db, ruta_absoluta, size);
}

static int pedir_imagen_botin_y_resolver_ruta(char *ruta_absoluta, size_t size)
{
    if (!hay_registros("botin"))
    {
        mostrar_no_hay_registros("botines");
        return 0;
    }

    listar_botines_simple();
    int id = input_int("\nID de botin (0 para cancelar): ");
    if (id == 0)
        return 0;

    if (!existe_id("botin", id))
    {
        printf("ID inexistente.\n");
        return 0;
    }

    if (!construir_ruta_absoluta_imagen_por_id(id, ruta_absoluta, size))
    {
        printf("No se encontro imagen cargada en disco para ese botin.\n");
        return 0;
    }

    return 1;
}

static int contar_total_botines_activos(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM botin WHERE IFNULL(activa, 1) = 1"))
        return 0;
    sqlite3_step(stmt);
    int total = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

static int botin_esta_activo(int botin_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT IFNULL(activa, 1) FROM botin WHERE id = ?"))
        return 0;
    sqlite3_bind_int(stmt, 1, botin_id);
    int activo = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        activo = sqlite3_column_int(stmt, 0) == 1;
    sqlite3_finalize(stmt);
    return activo;
}

static int actualizar_estado_botin(int botin_id, int activo)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE botin SET activa = ? WHERE id = ?"))
        return 0;
    sqlite3_bind_int(stmt, 1, activo ? 1 : 0);
    sqlite3_bind_int(stmt, 2, botin_id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

void sortear_botin(void)
{
    clear_screen();
    print_header("SORTEO DE BOTINES");

    int disponibles = contar_total_botines_activos();
    if (disponibles == 0)
    {
        mostrar_no_hay_registros("botines para sortear");
        pause_console();
        return;
    }

    int offset = secure_rand_range(disponibles);
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM botin WHERE IFNULL(activa, 1) = 1 LIMIT 1 OFFSET ?"))
    {
        printf("Error al seleccionar botin aleatorio.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, offset);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        printf("BOTIN SORTEADO!\n\n");
        printf("El botin seleccionado es: %s (ID %d)\n", nombre, id);

        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Sorteado botin id=%d nombre=%.180s", id, nombre);
        app_log_event("BOTIN", log_msg);
    }
    else
    {
        printf("Error al seleccionar botin aleatorio.\n");
    }
    sqlite3_finalize(stmt);
    pause_console();
}

void cargar_imagen_botin(void)
{
    clear_screen();
    print_header("CARGAR IMAGEN DE BOTIN");

    if (!hay_registros("botin"))
    {
        mostrar_no_hay_registros("botines");
        pause_console();
        return;
    }

    listar_botines_simple();
    int id = input_int("\nID de botin (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("botin", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    if (!app_cargar_imagen_entidad(id, "botin", "mifutbol_imagen_sel.txt"))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Cargada imagen para botin id=%d", id);
    app_log_event("BOTIN", log_msg);
    pause_console();
}

void ver_imagen_botin(void)
{
    clear_screen();
    print_header("VER IMAGEN DE BOTIN");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_botin_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
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

    if (!app_open_with_default_app(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen en el sistema.\n");
        pause_console();
        return;
    }

    printf("Abriendo imagen...\n");
    pause_console();
}

static void ver_informacion_botin(void)
{
    clear_screen();
    print_header("INFORMACION DE BOTIN");

    if (!hay_registros("botin"))
    {
        mostrar_no_hay_registros("botines");
        pause_console();
        return;
    }

    listar_botines_simple();
    printf("\n");

    int id = input_int("ID de botin para ver informacion (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("botin", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT nombre, IFNULL(activa, 1) FROM botin WHERE id = ?"))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        int activo = sqlite3_column_int(stmt, 1) == 1;
        int partidos = contar_partidos_por_botin(id);
        int predet = obtener_botin_predeterminado();

        printf("========================================\n");
        printf("ID              : %d\n", id);
        printf("Nombre          : %s\n", nombre);
        printf("Estado          : %s\n", activo ? "ACTIVO" : "INACTIVO");
        printf("Usado en        : %d partido(s)\n", partidos);
        printf("Predeterminado  : %s\n", id == predet ? "SI" : "NO");
        printf("========================================\n");
    }
    else
    {
        printf("No se pudo recuperar la informacion del botin.\n");
    }
    sqlite3_finalize(stmt);
    pause_console();
}

static void cargar_informacion_botin(void)
{
    clear_screen();
    print_header("CARGAR INFORMACION DE BOTIN");
    printf("No hay informacion adicional disponible para botines.\n");
    pause_console();
}

static void reactivar_botin(void)
{
    clear_screen();
    print_header("REACTIVAR / DESACTIVAR BOTIN");

    if (!hay_registros("botin"))
    {
        mostrar_no_hay_registros("botines");
        pause_console();
        return;
    }

    listar_botines_simple();
    printf("\n");

    int id = input_int("ID de botin (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("botin", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    int esta_activo = botin_esta_activo(id);
    int nuevo_estado = esta_activo ? 0 : 1;

    if (!confirmar(esta_activo ? "Desea desactivar este botin?" : "Desea reactivar este botin?"))
        return;

    if (!actualizar_estado_botin(id, nuevo_estado))
    {
        printf("No se pudo actualizar el estado del botin.\n");
        pause_console();
        return;
    }

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "%s botin id=%d",
             nuevo_estado == 1 ? "Reactivado" : "Desactivado", id);
    app_log_event("BOTIN", log_msg);
    mostrar_alerta_operacion("Botin", nuevo_estado == 1 ? "Reactivado" : "Desactivado", NULL);
}

static void configurar_visor_preferido_imagen(void)
{
    char actual[64] = {0};
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, "SELECT IFNULL(image_viewer, '') FROM settings WHERE id = 1"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *v = sqlite3_column_text(stmt, 0);
            if (v)
                snprintf(actual, sizeof(actual), "%s", (const char *)v);
        }
        sqlite3_finalize(stmt);
    }

    printf("Visor actual: %s\n", actual[0] ? actual : "auto");
    printf("Opciones: auto, mspaint (Windows)\n");
    printf("Escriba el nombre del visor: ");

    char nuevo[64] = {0};
    input_string("Visor: ", nuevo, (int)sizeof(nuevo));
    trim_whitespace(nuevo);
    if (nuevo[0] == '\0')
    {
        printf("No se realizaron cambios.\n");
        pause_console();
        return;
    }

    if (preparar_stmt(&stmt, "INSERT OR IGNORE INTO settings(id, theme, language, mode, text_size, image_viewer) VALUES(1, 0, 0, 0, 1, '')"))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    if (preparar_stmt(&stmt, "UPDATE settings SET image_viewer = ? WHERE id = 1"))
    {
        sqlite3_bind_text(stmt, 1, nuevo, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        printf("Visor guardado: %s\n", nuevo);
    }
    pause_console();
}

static void probar_visor_imagen_actual(void)
{
    printf("Se abrira la imagen del botin seleccionado con el visor actual.\n");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_botin_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
        pause_console();
        return;
    }

    if (!app_open_with_default_app(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen con el visor actual.\n");
    }
    else
    {
        printf("Visor ejecutado correctamente.\n");
    }
    pause_console();
}

static void menu_ajustes_imagen_botin(void)
{
    MenuItem items[] =
    {
        {1, "Configurar visor", configurar_visor_preferido_imagen},
        {2, "Probar visor", probar_visor_imagen_actual},
        {0, "Volver", NULL}
    };
    ejecutar_menu("AJUSTES IMAGEN BOTIN", items, 3);
}

void menu_botines(void)
{
    MenuItem items[] = {{1, "Crear", crear_botin},
        {2, "Listar", listar_botines},
        {3, "Modificar", editar_botin},
        {4, "Eliminar", eliminar_botin},
        {5, "Sortear", sortear_botin},
        {6, "Cargar Imagen", cargar_imagen_botin},
        {7, "Ver Botin", ver_imagen_botin},
        {8, "Ajustes Imagen", menu_ajustes_imagen_botin},
        {9, "Ver Informacion", ver_informacion_botin},
        {10, "Cargar Informacion", cargar_informacion_botin},
        {11, "Reactivar/Desactivar", reactivar_botin},
        {12, "Fijar Predeterminado", fijar_botin_predeterminado},
        {0, "Volver", NULL}
    };
    ejecutar_menu("BOTINES", items, 13);
}
