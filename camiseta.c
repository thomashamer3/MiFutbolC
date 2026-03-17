#include "camiseta.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif
#include <ctype.h>
#include <limits.h>


#define MAX_CAMISETAS_SORTEO 150

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql);
static int obtener_ruta_imagen_camiseta_db(int id, char *ruta, size_t size);
static void listar_camisetas_simple(void);
static int cargar_imagen_para_camiseta_id(int id);

static void asegurar_fila_settings()
{
    sqlite3_exec(db,
                 "INSERT OR IGNORE INTO settings(id, theme, language, mode, text_size, image_viewer) "
                 "VALUES(1, 0, 0, 0, 1, '');",
                 NULL,
                 NULL,
                 NULL);
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

    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *v = sqlite3_column_text(stmt, 0);
        if (v && strncpy_s(buffer, size, (const char *)v, _TRUNCATE) == 0)
        {
            trim_whitespace(buffer);
            ok = 1;
        }
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int guardar_visor_preferido(const char *viewer)
{
    asegurar_fila_settings();

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE settings SET image_viewer = ? WHERE id = 1"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, viewer ? viewer : "", -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int instalar_paquete_linux(const char *package_name)
{
#ifdef _WIN32
    (void)package_name;
    return 0;
#else
    if (!package_name || package_name[0] == '\0')
    {
        return 0;
    }

    char install_cmd[512] = {0};
    if (app_command_exists("apt-get"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo apt-get update && sudo apt-get install -y %s", package_name);
    }
    else if (app_command_exists("dnf"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo dnf install -y %s", package_name);
    }
    else if (app_command_exists("pacman"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo pacman -Sy --noconfirm %s", package_name);
    }
    else if (app_command_exists("zypper"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo zypper --non-interactive install %s", package_name);
    }
    else
    {
        return 0;
    }

    return system(install_cmd) == 0;
#endif
}

static int construir_ruta_absoluta_imagen_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!obtener_ruta_imagen_camiseta_db(id, ruta_db, sizeof(ruta_db)))
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


static int abrir_imagen_en_sistema(const char *ruta)
{
    if (!ruta || ruta[0] == '\0')
    {
        return 0;
    }

#ifdef _WIN32
    char viewer[64] = {0};
    obtener_visor_preferido(viewer, sizeof(viewer));

    char cmd[1200];
    if (viewer[0] != '\0' && _stricmp(viewer, "auto") != 0)
    {
        if (_stricmp(viewer, "mspaint") == 0)
        {
            snprintf(cmd, sizeof(cmd), "mspaint \"%s\"", ruta);
            if (system(cmd) == 0)
            {
                return 1;
            }
        }
        else
        {
            printf("Visor preferido no soportado en Windows: %s\n", viewer);
        }
    }

    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", ruta);
    if (system(cmd) == 0)
    {
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "mspaint \"%s\"", ruta);
    return system(cmd) == 0;
#else
    char viewer[64] = {0};
    obtener_visor_preferido(viewer, sizeof(viewer));

    char cmd_open[1400];

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
            printf("El visor preferido '%s' no esta instalado.\n", viewer);
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
        if (!confirmar("No se detecto visor de imagen. Desea instalar 'feh' automaticamente?"))
        {
            return 0;
        }

        printf("Instalando visor liviano 'feh'...\n");
        if (!instalar_paquete_linux("feh"))
        {
            printf("No se pudo instalar 'feh'.\n");
            return 0;
        }

        visor = "feh";
        if (viewer[0] == '\0' || strcmp(viewer, "auto") == 0)
        {
            guardar_visor_preferido("feh");
        }
    }

    if (strcmp(visor, "gio") == 0)
    {
        snprintf(cmd_open, sizeof(cmd_open), "gio open \"%s\" >/dev/null 2>&1", ruta);
    }
    else
    {
        snprintf(cmd_open, sizeof(cmd_open), "%s \"%s\" >/dev/null 2>&1", visor, ruta);
    }

    return system(cmd_open) == 0;
#endif
}

static void configurar_visor_preferido_imagen()
{
    clear_screen();
    print_header("CONFIGURAR VISOR DE IMAGEN");

    char actual[64] = {0};
    obtener_visor_preferido(actual, sizeof(actual));
    printf("Visor actual: %s\n\n", actual[0] ? actual : "auto");

    printf("Escribe el visor a usar o 'auto'.\n");
#ifdef _WIN32
    printf("Opciones recomendadas: auto, mspaint\n");
#else
    printf("Opciones recomendadas: auto, xdg-open, gio, feh, eog, gwenview\n");
#endif

    char nuevo[64] = {0};
    input_string("Visor: ", nuevo, (int)sizeof(nuevo));
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

static void previsualizar_imagen_camiseta_consola()
{
    clear_screen();
    print_header("PREVISUALIZAR IMAGEN (CONSOLA)");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_camiseta_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
        pause_console();
        return;
    }

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

    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "chafa --size 80x40 \"%s\"", ruta_absoluta);

    if (system(cmd) != 0)
    {
        printf("No se pudo previsualizar con chafa.\n");
        pause_console();
        return;
    }

    pause_console();
}

static void probar_visor_imagen_actual()
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

static void menu_ajustes_imagen_camiseta()
{
    MenuItem items[] =
    {
        {1, "Configurar visor", configurar_visor_preferido_imagen},
        {2, "Probar visor", probar_visor_imagen_actual},
        {3, "Previsualizar en consola", previsualizar_imagen_camiseta_consola},
        {0, "Volver", NULL}
    };
    ejecutar_menu("AJUSTES IMAGEN", items, 4);
}

static int obtener_ruta_imagen_camiseta_db(int id, char *ruta, size_t size)
{
    if (!ruta || size == 0)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT imagen_ruta FROM camiseta WHERE id=?"))
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

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
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

static void solicitar_nombre_camiseta(const char *prompt, char *buffer, int size)
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

static void listar_camisetas_simple()
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM camiseta"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d - %s",
                                sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("camisetas cargadas");

    sqlite3_finalize(stmt);
}

/**
 * @brief Crea una nueva camiseta en la base de datos
 *
 * Permite a los usuarios agregar camisetas para gestion y sorteos,
 * reutilizando IDs eliminados para mantener la secuencia.
 */
void crear_camiseta()
{
    clear_screen();
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
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada camiseta id=%lld nombre=%.180s", id, nombre);
        app_log_event("CAMISETA", log_msg);

        int desea_cargar_imagen = confirmar("Desea cargar imagen para esta camiseta ahora?");
        if (desea_cargar_imagen)
        {
            if (!cargar_imagen_para_camiseta_id((int)id))
            {
                printf("No se pudo cargar la imagen en este momento.\n");
                snprintf(log_msg, sizeof(log_msg), "Camiseta id=%lld creada, pero fallo carga de imagen inicial", id);
                app_log_event("CAMISETA", log_msg);
            }
            else
            {
                snprintf(log_msg, sizeof(log_msg), "Camiseta id=%lld creada con imagen inicial", id);
                app_log_event("CAMISETA", log_msg);
            }
        }
        else
        {
            snprintf(log_msg, sizeof(log_msg), "Camiseta id=%lld creada sin imagen inicial (opcional)", id);
            app_log_event("CAMISETA", log_msg);
        }
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear camiseta nombre=%.180s", nombre);
        app_log_event("CAMISETA", log_msg);
    }

    pause_console();
}

/**
 * @brief Muestra un listado de todas las camisetas registradas
 *
 * Proporciona visibilidad a los usuarios de las camisetas disponibles
 * para facilitar la toma de decisiones en otras operaciones.
 */
static int cargar_imagen_para_camiseta_id(int id)
{
    if (id <= 0)
    {
        return 0;
    }

    char ruta_origen[1024] = {0};
    printf("\nSe abrira el selector de archivos en Descargas.\n");
    if (!app_select_image_from_user(ruta_origen, sizeof(ruta_origen), "mifutbol_imagen_sel.txt"))
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
    snprintf(base_destino, sizeof(base_destino), "camiseta_%d_%s", id, ts);

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
    const char *ruta_final = NULL;

    if (optimizada)
    {
        nombre_final = nombre_destino_opt;
        ruta_final = ruta_destino_opt;
    }
    else
    {
        if (!app_copy_binary_file(ruta_origen, ruta_destino_original))
        {
            printf("No se pudo mover/copiar la imagen a la carpeta Imagenes.\n");
            return 0;
        }
        nombre_final = nombre_destino_original;
        ruta_final = ruta_destino_original;
    }

    if (!nombre_final || !ruta_final)
    {
        printf("No se pudo mover/copiar la imagen a la carpeta Imagenes.\n");
        return 0;
    }

    char ruta_relativa_db[300] = {0};
    snprintf(ruta_relativa_db, sizeof(ruta_relativa_db), "Imagenes/%s", nombre_final);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET imagen_ruta=? WHERE id=?"))
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
    if (optimizada)
    {
        printf("Optimizada (max 1280px, calidad alta) y guardada en: %s\n", ruta_final);
    }
    else
    {
        printf("Guardada sin optimizacion (no habia optimizador disponible) en: %s\n", ruta_final);
    }
    return 1;
}

void listar_camisetas()
{
    clear_screen();
    print_header("LISTADO DE CAMISETAS");

    app_log_event("CAMISETA", "Listado de camisetas consultado");

    listar_camisetas_simple();
    pause_console();
}

/**
 * @brief Permite editar el nombre de una camiseta existente
 *
 * Permite correcciones en la informacion de camisetas sin necesidad
 * de eliminar y recrear registros, mejorando la usabilidad.
 */
void editar_camiseta()
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
        return;

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

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("\nCamiseta actualizada correctamente\n");
    pause_console();
}

void cargar_imagen_camiseta()
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

void ver_imagen_camiseta()
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

/**
 * @brief Elimina una camiseta de la base de datos
 *
 * Permite remover camisetas que ya no son necesarias mientras
 * mantiene la integridad de los datos con validaciones y confirmaciones.
 */
void eliminar_camiseta()
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
        return;

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    if (!confirmar("Seguro que desea eliminar esta camiseta?"))
        return;

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

    printf("\nCamiseta eliminada correctamente\n");
    pause_console();
}

/**
 * @brief Reinicia el estado de sorteo de todas las camisetas
 *
 * Necesario cuando todas las camisetas han sido sorteadas para permitir
 * nuevos sorteos sin requerir recrear registros.
 */
static void reiniciar_sorteo()
{
    sqlite3_exec(db, "UPDATE camiseta SET sorteada = 0", 0, 0, 0);
    printf("Todas las camisetas han sido sorteadas. Reiniciando sorteo...\n\n");
}

/**
 * @brief Obtiene la lista de IDs de camisetas disponibles para sorteo
 *
 * @param ids Array donde almacenar los IDs
 * @param max Tamano maximo del array
 * @return Numero de IDs obtenidos
 */
static int obtener_ids_disponibles(int ids[], int max)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id FROM camiseta WHERE sorteada = 0"))
    {
        return 0;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < max)
    {
        ids[i++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return i;
}

/**
 * @brief Selecciona aleatoriamente un ID de la lista proporcionada
 *
 * @param ids Array de IDs disponibles
 * @param count Numero de IDs en el array
 * @return ID seleccionado aleatoriamente
 */
static int seleccionar_id_aleatorio(const int ids[], int count)
{
    // Seed the random number generator with current time and process ID for better randomness
    srand((unsigned int)(time(NULL) + _getpid()));

    // Prevent division by zero
    if (count <= 0)
    {
        return -1; // Return error code
    }

    return ids[rand() % count];
}

/**
 * @brief Marca una camiseta como sorteada en la base de datos
 *
 * @param id ID de la camiseta a marcar
 */
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

/**
 * @brief Obtiene el nombre de una camiseta por su ID
 *
 * @param id ID de la camiseta
 * @return Nombre de la camiseta (debe ser liberado con free)
 */
static char* obtener_nombre_camiseta(int id)
{
    char nombre_buffer[256];

    if (obtener_nombre_entidad("camiseta", id, nombre_buffer, sizeof(nombre_buffer)))
    {
        return strdup(nombre_buffer);
    }

    return strdup("Desconocida");
}

/**
 * @brief Realiza un sorteo aleatorio entre las camisetas disponibles
 *
 * Permite sorteos continuos reutilizando camisetas ya sorteadas cuando
 * se agotan las disponibles, manteniendo la funcionalidad del sistema
 * sin necesidad de intervencion manual del usuario.
 */
void sortear_camiseta()
{
    clear_screen();
    print_header("SORTEO DE CAMISETAS");

    int disponibles = obtener_total("SELECT COUNT(*) FROM camiseta WHERE sorteada = 0");

    if (disponibles == 0)
    {
        reiniciar_sorteo();
        disponibles = obtener_total("SELECT COUNT(*) FROM camiseta");
    }

    if (disponibles == 0)
    {
        printf("No hay camisetas para sortear.\n");
        pause_console();
        return;
    }

    int ids[MAX_CAMISETAS_SORTEO];
    int count = obtener_ids_disponibles(ids, MAX_CAMISETAS_SORTEO);
    int seleccionado = seleccionar_id_aleatorio(ids, count);

    // Check if random selection failed
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

/**
 * @brief Muestra el menu principal de gestion de camisetas
 *
 * Proporciona una interfaz estructurada para las operaciones de
 * gestion de camisetas, centralizando el acceso a todas las funcionalidades.
 */
void menu_camisetas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_camiseta},
        {2, "Listar", listar_camisetas},
        {3, "Modificar", editar_camiseta},
        {4, "Eliminar", eliminar_camiseta},
        {5, "Sortear", sortear_camiseta},
        {6, "Cargar Imagen", cargar_imagen_camiseta},
        {7, "Ver Camiseta", ver_imagen_camiseta},
        {8, "Ajustes Imagen", menu_ajustes_imagen_camiseta},
        {0, "Volver", NULL}
    };
    ejecutar_menu("CAMISETAS", items, 9);
}
