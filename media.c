#include "media.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
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
        if (partido_id < 0)
        {
            partido_id = 0;
        }
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
    sqlite3_bind_text(stmt, 4, titulo, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 5, url[0] ? url : "", -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 6, descripcion[0] ? descripcion : "", -1, DB_TRANSIENT);

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
    if (url && url[0])
    {
        printf("     URL: %s\n", url);
    }
    if (desc && desc[0])
    {
        printf("     Desc: %s\n", desc);
    }
    if (partido_id > 0)
    {
        printf("     Partido ID: %d\n", partido_id);
    }
    if (fecha && fecha[0])
    {
        printf("     Fecha: %s\n", fecha);
    }
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

typedef struct
{
    int partido_id;
    int tipo;
    char titulo[256];
    char url[1024];
    char desc[1024];
} MediaActual;

static bool leer_media_actual(int id, MediaActual *out)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT partido_id, tipo, titulo, url, descripcion "
                       "FROM media WHERE id=?"))
    {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    memset(out, 0, sizeof(*out));
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        out->partido_id = sqlite3_column_int(stmt, 0);
        out->tipo = sqlite3_column_int(stmt, 1);
        const char *p;
        p = (const char *)sqlite3_column_text(stmt, 2);
        if (p) strncpy_s(out->titulo, sizeof(out->titulo), p, _TRUNCATE);
        p = (const char *)sqlite3_column_text(stmt, 3);
        if (p) strncpy_s(out->url, sizeof(out->url), p, _TRUNCATE);
        p = (const char *)sqlite3_column_text(stmt, 4);
        if (p) strncpy_s(out->desc, sizeof(out->desc), p, _TRUNCATE);
    }
    sqlite3_finalize(stmt);
    return true;
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

    MediaActual actual;
    if (!leer_media_actual(id, &actual))
    {
        mostrar_no_existe("Referencia");
        pause_console();
        return;
    }

    char titulo[256] = {0};
    char url[1024] = {0};
    char desc[1024] = {0};

    printf("Editando: %s\n\n", actual.titulo);

    input_string_default("Titulo", actual.titulo, titulo, (int)sizeof(titulo));
    printf("Tipo (1-4) [%d]: ", actual.tipo);
    int tipo = input_int("");
    if (tipo < 1 || tipo > 4) tipo = actual.tipo;
    input_string_default("URL", actual.url, url, (int)sizeof(url));
    input_string_default("Descripcion", actual.desc, desc, (int)sizeof(desc));

    printf("Partido ID [%d]: ", actual.partido_id);
    int partido_id = input_int("");
    if (partido_id < 0) partido_id = actual.partido_id;

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "UPDATE media SET tipo=?, titulo=?, url=?, "
                       "descripcion=?, partido_id=? WHERE id=?"))
    {
        mostrar_error_operacion("Media", "actualizar");
        return;
    }

    sqlite3_bind_int(stmt, 1, tipo);
    sqlite3_bind_text(stmt, 2, titulo, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 3, url, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 4, desc, -1, DB_TRANSIENT);
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
        if (id > 0)
        {
            mostrar_no_existe("Referencia");
        }
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
        strncpy_s(titulo, sizeof(titulo), (const char *)sqlite3_column_text(stmt, 0), _TRUNCATE);
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
    MenuItem items[] = {{1, "Nueva referencia", &media_crear},
        {2, "Listar todas", &media_listar},
        {3, "Filtrar por tipo", &media_filtrar_por_tipo},
        {4, "Editar referencia", &media_editar},
        {5, "Eliminar referencia", &media_eliminar},
        {0, "Volver", NULL}
    };
    ejecutar_menu("REFERENCIAS MULTIMEDIA", items, 6);
}
