#include "media.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static const char *tipo_to_text(int tipo)
{
    switch (tipo)
    {
    case 1:
        return "Video";
    case 2:
        return "Foto";
    case 3:
        return "Articulo";
    case 4:
        return "Audio";
    default:
        return "Desconocido";
    }
}

void media_crear(void)
{
    clear_screen();
    print_header("NUEVA REFERENCIA MULTIMEDIA");

    printf("Tipo:\n");
    printf("  1 - Video\n  2 - Foto\n  3 - Articulo\n  4 - Audio\n");
    int tipo = input_int_rango("Seleccione tipo", 1, 4);

    char titulo[256] = {0};
    char url[1024] = {0};
    char descripcion[1024] = {0};

    input_string("Titulo: ", titulo, (int)sizeof(titulo));
    if (titulo[0] == '\0')
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    input_string("URL (YouTube, Drive, etc.): ", url, (int)sizeof(url));
    input_string("Descripcion: ", descripcion, (int)sizeof(descripcion));

    int partido_id = 0;
    if (hay_registros("partido"))
    {
        partido_id = input_int("ID del partido asociado (0=ninguno): ");
        if (partido_id < 0) partido_id = 0;
    }

    long long id = obtener_siguiente_id("media");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "INSERT INTO media (id, partido_id, tipo, titulo, url, "
                       "descripcion) VALUES (?,?,?,?,?,?)"))
    {
        mostrar_error_operacion("Media", "crear");
        return;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, partido_id);
    sqlite3_bind_int(stmt, 3, tipo);
    sqlite3_bind_text(stmt, 4, titulo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, url[0] ? url : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, descripcion[0] ? descripcion : "", -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Referencia", "Agregada", titulo);
    }
    else
    {
        mostrar_error_operacion("Media", "guardar");
    }
    pause_console();
}

static void mostrar_media_item(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    int partido_id = sqlite3_column_int(stmt, 1);
    int tipo = sqlite3_column_int(stmt, 2);
    const char *titulo = (const char *)sqlite3_column_text(stmt, 3);
    const char *url = (const char *)sqlite3_column_text(stmt, 4);
    const char *desc = (const char *)sqlite3_column_text(stmt, 5);
    const char *fecha = (const char *)sqlite3_column_text(stmt, 6);

    printf("  %d. [%s] %s\n", id, tipo_to_text(tipo), titulo ? titulo : "");
    if (url && url[0]) printf("     URL: %s\n", url);
    if (desc && desc[0]) printf("     Desc: %s\n", desc);
    if (partido_id > 0) printf("     Partido ID: %d\n", partido_id);
    if (fecha && fecha[0]) printf("     Fecha: %s\n", fecha);
    printf("\n");
}

void media_listar(void)
{
    if (!hay_registros("media"))
    {
        mostrar_no_hay_registros("referencias multimedia");
        pause_console();
        return;
    }

    clear_screen();
    print_header("REFERENCIAS MULTIMEDIA");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT id, partido_id, tipo, titulo, url, descripcion, "
                       "fecha FROM media ORDER BY fecha DESC"))
    {
        return;
    }

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total++;
        mostrar_media_item(stmt);
    }
    sqlite3_finalize(stmt);

    printf("Total: %d referencia(s)\n", total);
    pause_console();
}

void media_editar(void)
{
    if (!hay_registros("media"))
    {
        mostrar_no_hay_registros("referencias multimedia");
        pause_console();
        return;
    }

    listar_entidades("media", "EDITAR REFERENCIA", "No hay referencias");

    int id = input_int("ID de la referencia a editar (0=cancelar): ");
    if (id <= 0 || !existe_id("media", id))
    {
        if (id > 0) mostrar_no_existe("Referencia");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT partido_id, tipo, titulo, url, descripcion "
                       "FROM media WHERE id=?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    int partido_actual = 0;
    int tipo_actual = 0;
    char titulo_actual[256] = {0};
    char url_actual[1024] = {0};
    char desc_actual[1024] = {0};

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        partido_actual = sqlite3_column_int(stmt, 0);
        tipo_actual = sqlite3_column_int(stmt, 1);
        const char *p = (const char *)sqlite3_column_text(stmt, 2);
        if (p) strncpy_s(titulo_actual, sizeof(titulo_actual), p, _TRUNCATE);
        p = (const char *)sqlite3_column_text(stmt, 3);
        if (p) strncpy_s(url_actual, sizeof(url_actual), p, _TRUNCATE);
        p = (const char *)sqlite3_column_text(stmt, 4);
        if (p) strncpy_s(desc_actual, sizeof(desc_actual), p, _TRUNCATE);
    }
    sqlite3_finalize(stmt);

    char titulo[256] = {0};
    char url[1024] = {0};
    char desc[1024] = {0};

    printf("Editando: %s\n\n", titulo_actual);

    printf("Titulo [%s]: ", titulo_actual);
    input_string("", titulo, (int)sizeof(titulo));
    if (titulo[0] == '\0') strncpy_s(titulo, sizeof(titulo), titulo_actual, _TRUNCATE);

    printf("Tipo (1-4) [%d]: ", tipo_actual);
    int tipo = input_int("");
    if (tipo < 1 || tipo > 4) tipo = tipo_actual;

    printf("URL [%s]: ", url_actual);
    input_string("", url, (int)sizeof(url));
    if (url[0] == '\0') strncpy_s(url, sizeof(url), url_actual, _TRUNCATE);

    printf("Descripcion [%s]: ", desc_actual);
    input_string("", desc, (int)sizeof(desc));
    if (desc[0] == '\0') strncpy_s(desc, sizeof(desc), desc_actual, _TRUNCATE);

    printf("Partido ID [%d]: ", partido_actual);
    int partido_id = input_int("");
    if (partido_id < 0) partido_id = partido_actual;

    if (!preparar_stmt(&stmt, "UPDATE media SET tipo=?, titulo=?, url=?, "
                       "descripcion=?, partido_id=? WHERE id=?"))
    {
        mostrar_error_operacion("Media", "actualizar");
        return;
    }

    sqlite3_bind_int(stmt, 1, tipo);
    sqlite3_bind_text(stmt, 2, titulo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, desc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, partido_id);
    sqlite3_bind_int(stmt, 6, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Referencia", "Editada", titulo);
    }
    else
    {
        mostrar_error_operacion("Media", "editar");
    }
    pause_console();
}

void media_eliminar(void)
{
    if (!hay_registros("media"))
    {
        mostrar_no_hay_registros("referencias multimedia");
        pause_console();
        return;
    }

    listar_entidades("media", "ELIMINAR REFERENCIA", "No hay referencias");

    int id = input_int("ID a eliminar (0=cancelar): ");
    if (id <= 0 || !existe_id("media", id))
    {
        if (id > 0) mostrar_no_existe("Referencia");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT titulo FROM media WHERE id = ?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    char titulo[256] = {0};
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strncpy_s(titulo, sizeof(titulo),
                  (const char *)sqlite3_column_text(stmt, 0), _TRUNCATE);
    }
    sqlite3_finalize(stmt);

    printf("Eliminar '%s'? (ID: %d)\n", titulo, id);
    if (!confirmar("Confirmar eliminacion"))
    {
        printf("Cancelado.\n");
        pause_console();
        return;
    }

    if (!preparar_stmt(&stmt, "DELETE FROM media WHERE id = ?"))
    {
        mostrar_error_operacion("Media", "eliminar");
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Referencia", "Eliminada", titulo);
    }
    else
    {
        mostrar_error_operacion("Media", "eliminar");
    }
    pause_console();
}

void media_filtrar_por_tipo(void)
{
    if (!hay_registros("media"))
    {
        mostrar_no_hay_registros("referencias multimedia");
        pause_console();
        return;
    }

    printf("Filtrar por tipo:\n");
    printf("  1 - Video\n  2 - Foto\n  3 - Articulo\n  4 - Audio\n");
    int tipo = input_int_rango("Seleccione tipo", 1, 4);

    clear_screen();
    print_header("REFERENCIAS - FILTRADAS");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT id, partido_id, tipo, titulo, url, descripcion, "
                       "fecha FROM media WHERE tipo = ? ORDER BY fecha DESC"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, tipo);

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total++;
        mostrar_media_item(stmt);
    }
    sqlite3_finalize(stmt);

    printf("Total: %d %s(s)\n", total, tipo_to_text(tipo));
    pause_console();
}

void menu_media(void)
{
    MenuItem items[] =
    {
        {1, "Nueva referencia", media_crear},
        {2, "Listar todas", media_listar},
        {3, "Filtrar por tipo", media_filtrar_por_tipo},
        {4, "Editar referencia", media_editar},
        {5, "Eliminar referencia", media_eliminar},
        {0, "Volver", NULL}
    };
    ejecutar_menu("REFERENCIAS MULTIMEDIA", items, 6);
}
