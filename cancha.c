#include "cancha.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif


static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static int abrir_imagen_en_sistema(const char *ruta)
{
    if (!ruta || ruta[0] == '\0')
    {
        return 0;
    }

    char cmd[1400];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", ruta);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" >/dev/null 2>&1", ruta);
#endif
    return system(cmd) == 0;
}

static int obtener_ruta_imagen_cancha_db(int id, char *ruta, size_t size)
{
    if (!ruta || size == 0)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT imagen_ruta FROM cancha WHERE id=?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *valor = sqlite3_column_text(stmt, 0);
        if (valor && valor[0] != '\0' && strncpy_s(ruta, size, (const char *)valor, _TRUNCATE) == 0)
        {
            ok = 1;
        }
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int construir_ruta_absoluta_imagen_cancha_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!obtener_ruta_imagen_cancha_db(id, ruta_db, sizeof(ruta_db)))
    {
        return 0;
    }

    char nombre_archivo[260] = {0};
    if (!app_get_file_name_from_path(ruta_db, nombre_archivo, sizeof(nombre_archivo)))
    {
        return 0;
    }

    const char *images_dir = get_images_dir();
    if (!images_dir)
    {
        return 0;
    }

    app_build_path(ruta_absoluta, size, images_dir, nombre_archivo);

    FILE *f = NULL;
    if (fopen_s(&f, ruta_absoluta, "rb") != 0 || !f)
    {
        return 0;
    }
    fclose(f);

    return 1;
}

static void solicitar_nombre_cancha(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string(prompt, buffer, size);
        trim_whitespace(buffer);

        if (buffer[0] != '\0')
            return;

        printf("El nombre no puede estar vacio.\n");
    }
}

static int cargar_imagen_para_cancha_id(int id)
{
    if (id <= 0)
    {
        return 0;
    }

    char ruta_origen[1024] = {0};
    printf("\nSe abrira el selector de archivos en Descargas.\n");
    if (!app_select_image_from_user(ruta_origen, sizeof(ruta_origen), "mifutbol_imagen_sel_cancha.txt"))
    {
        printf("No se selecciono ninguna imagen.\n");
        return 0;
    }

    const char *ext = app_get_file_extension(ruta_origen);
    if (!app_is_supported_image_extension(ext))
    {
        printf("Formato no soportado. Usa: JPG, JPEG, PNG, BMP o WEBP.\n");
        return 0;
    }

    const char *images_dir = get_images_dir();
    if (!images_dir)
    {
        printf("No se pudo preparar la carpeta Imagenes.\n");
        return 0;
    }

    char ts[32] = {0};
    get_timestamp(ts, (int)sizeof(ts));

    char base_destino[220] = {0};
    snprintf(base_destino, sizeof(base_destino), "cancha_%d_%s", id, ts);

    char nombre_destino_opt[256] = {0};
    snprintf(nombre_destino_opt, sizeof(nombre_destino_opt), "%s.jpg", base_destino);

    char nombre_destino_original[256] = {0};
    snprintf(nombre_destino_original, sizeof(nombre_destino_original), "%s%s", base_destino, ext);

    char ruta_destino_opt[1200] = {0};
    char ruta_destino_original[1200] = {0};
    app_build_path(ruta_destino_opt, sizeof(ruta_destino_opt), images_dir, nombre_destino_opt);
    app_build_path(ruta_destino_original, sizeof(ruta_destino_original), images_dir, nombre_destino_original);

    int optimizada = app_optimize_image_file(ruta_origen, ruta_destino_opt);
    const char *nombre_final = NULL;

    if (optimizada)
    {
        nombre_final = nombre_destino_opt;
    }
    else
    {
        if (!app_copy_binary_file(ruta_origen, ruta_destino_original))
        {
            printf("No se pudo mover/copiar la imagen a la carpeta Imagenes.\n");
            return 0;
        }
        nombre_final = nombre_destino_original;
    }

    char ruta_relativa_db[300] = {0};
    snprintf(ruta_relativa_db, sizeof(ruta_relativa_db), "Imagenes/%s", nombre_final);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE cancha SET imagen_ruta=? WHERE id=?"))
    {
        printf("Error al guardar ruta de imagen en DB.\n");
        return 0;
    }

    sqlite3_bind_text(stmt, 1, ruta_relativa_db, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        printf("Error al guardar ruta de imagen en DB.\n");
        return 0;
    }

    printf("\nImagen cargada correctamente.\n");
    return 1;
}

void cargar_imagen_cancha()
{
    mostrar_pantalla("CARGAR IMAGEN DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    int id = input_int("\nID de cancha (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    if (!cargar_imagen_para_cancha_id(id))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    pause_console();
}

void ver_imagen_cancha()
{
    mostrar_pantalla("VER IMAGEN DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    int id = input_int("\nID de cancha (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    char ruta_absoluta[1200] = {0};
    if (!construir_ruta_absoluta_imagen_cancha_por_id(id, ruta_absoluta, sizeof(ruta_absoluta)))
    {
        printf("No se encontro imagen cargada para esa cancha.\n");
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



/**
 * @brief Crea una nueva cancha en la base de datos
 *
 * Permite a los usuarios agregar canchas para asignacion en partidos,
 * reutilizando IDs eliminados para mantener la secuencia.
 */
void crear_cancha()
{
    char nombre[100];
    solicitar_nombre_cancha("Nombre de la cancha: ", nombre, sizeof(nombre));

    long long id = obtener_siguiente_id("cancha");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "INSERT INTO cancha(id, nombre) VALUES(?, ?)") )
    {
        printf("Error al crear la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, (int)id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada cancha id=%lld nombre=%.180s", id, nombre);
        app_log_event("CANCHA", log_msg);
        if (confirmar("Desea cargar imagen para esta cancha ahora?") &&
            !cargar_imagen_para_cancha_id((int)id))
        {
            printf("No se pudo cargar la imagen en este momento.\n");
        }
        printf("Cancha creada correctamente\n");
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear cancha nombre=%.180s", nombre);
        app_log_event("CANCHA", log_msg);
        printf("Error al crear la cancha.\n");
    }

    pause_console();
}

/**
 * @brief Muestra un listado de todas las canchas registradas
 *
 * Proporciona visibilidad de canchas disponibles para seleccion
 * en partidos y operaciones de gestion.
 */
void listar_canchas()
{
    app_log_event("CANCHA", "Listado de canchas consultado");
    listar_entidades("cancha", "LISTADO DE CANCHAS", "No hay canchas cargadas.");
}

/**
 * @brief Elimina una cancha de la base de datos
 *
 * Permite remover canchas obsoletas mientras mantiene integridad
 * de datos con validaciones y confirmaciones de usuario.
 */
void eliminar_cancha()
{
    mostrar_pantalla("ELIMINAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    printf("\n");

    int id = input_int("ID Cancha a Eliminar (0 para cancelar): ");

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    if (!confirmar("Seguro que desea eliminar esta cancha?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "DELETE FROM cancha WHERE id = ?"))
    {
        printf("Error al eliminar la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Cancha Eliminada Correctamente\n");
    pause_console();
}

/**
 * @brief Permite modificar el nombre de una cancha existente
 *
 * Permite correcciones en nombres de canchas sin necesidad
 * de eliminar y recrear registros, mejorando usabilidad.
 */
void modificar_cancha()
{
    mostrar_pantalla("MODIFICAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    printf("\n");

    int id = input_int("ID Cancha a Modificar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    char nombre[100];
    solicitar_nombre_cancha("Nuevo nombre de la cancha: ", nombre, sizeof(nombre));

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE cancha SET nombre = ? WHERE id = ?"))
    {
        printf("Error al modificar la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Cancha Modificada Correctamente\n");
    pause_console();
}

/**
 * @brief Muestra el menu principal de gestion de canchas
 *
 * Proporciona interfaz centralizada para operaciones CRUD de canchas,
 * facilitando la navegacion y delegacion de tareas especificas.
 */
void menu_canchas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_cancha},
        {2, "Listar", listar_canchas},
        {3, "Modificar", modificar_cancha},
        {4, "Eliminar", eliminar_cancha},
        {5, "Cargar Imagen", cargar_imagen_cancha},
        {6, "Ver Imagen", ver_imagen_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu("CANCHAS", items, 7);
}
