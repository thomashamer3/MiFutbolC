#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "db.h"
#include "camiseta.h"
#include "cancha.h"
#include "partido.h"
#include "lesion.h"
#include "estadisticas.h"
#include "analisis.h"
#include "logros.h"
#include "export.h"
#include "export_all.h"
#include "import.h"
#include "equipo.h"
#include "torneo.h"
#include "temporada.h"
#include "ascii_art.h"
#include "settings.h"
#include "financiamiento.h"
#include "entrenador_ia.h"
#include "bienestar.h"

// Definir items del menú principal directamente con inicialización static
struct MenuItemDefinition
{
    int opcion;
    const char* texto;
    void (*accion)(void);
};

static void abrir_menu_equipos(void)
{
    app_log_event("EQUIPOS", "Ingreso al modulo Equipos");
    menu_equipos();
}

static void abrir_menu_partidos(void)
{
    app_log_event("PARTIDOS", "Ingreso al modulo Partidos");
    menu_partidos();
}

static void abrir_menu_lesiones(void)
{
    app_log_event("LESIONES", "Ingreso al modulo Lesiones");
    menu_lesiones();
}

static void abrir_menu_estadisticas(void)
{
    app_log_event("ESTADISTICAS", "Ingreso al modulo Estadisticas");
    menu_estadisticas();
}

static void abrir_menu_logros(void)
{
    app_log_event("LOGROS", "Ingreso al modulo Logros");
    menu_logros();
}

static void abrir_menu_financiamiento(void)
{
    app_log_event("FINANCIAMIENTO", "Ingreso al modulo Financiamiento");
    menu_financiamiento();
}

static void abrir_menu_torneos(void)
{
    app_log_event("TORNEOS", "Ingreso al modulo Torneos");
    menu_torneos();
}

static void abrir_menu_temporadas(void)
{
    app_log_event("TEMPORADA", "Ingreso al modulo Temporada");
    menu_temporadas();
}

static void abrir_menu_analisis(void)
{
    app_log_event("ANALISIS", "Ingreso al modulo Analisis");
    mostrar_analisis();
}

static void abrir_menu_bienestar(void)
{
    app_log_event("BIENESTAR", "Ingreso al modulo Bienestar");
    menu_bienestar();
}

static void abrir_menu_settings(void)
{
    app_log_event("SETTINGS", "Ingreso al modulo Ajustes");
    menu_settings();
}

static const struct MenuItemDefinition MENU_ITEMS[] =
{
    {1, "Camisetas", &menu_camisetas},
    {2, "Canchas", &menu_canchas},
    {3, "Equipos", &abrir_menu_equipos},
    {4, "Partidos", &abrir_menu_partidos},
    {5, "Lesiones", &abrir_menu_lesiones},
    {6, "Estadisticas", &abrir_menu_estadisticas},
    {7, "Logros", &abrir_menu_logros},
    {9, "Financiamiento", &abrir_menu_financiamiento},
    {10, "Torneos", &abrir_menu_torneos},
    {11, "Temporada", &abrir_menu_temporadas},
    {12, "Analisis", &abrir_menu_analisis},
    {13, "Bienestar", &abrir_menu_bienestar},
    {14, "Ajustes", &abrir_menu_settings},
    {0, "Salir", NULL}
};

// Número de items en el menú principal
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
static const size_t MENU_ITEM_COUNT = ARRAY_SIZE(MENU_ITEMS);

static const MenuItem *buscar_item(const MenuItem *items, int cantidad, int opcion)
{
    for (int i = 0; i < cantidad; i++)
    {
        if (items[i].opcion == opcion)
        {
            return &items[i];
        }
    }
    return NULL;
}


#ifdef UNIT_TEST
static MenuTestCapture *g_menu_test_capture = NULL;

int menu_get_item_count(void)
{
    return (int)MENU_ITEM_COUNT;
}

const MenuItem *menu_get_items(void)
{
    return (const MenuItem *)MENU_ITEMS;
}

const MenuItem *menu_buscar_item(const MenuItem *items, int cantidad, int opcion)
{
    return buscar_item(items, cantidad, opcion);
}

void menu_test_set_capture(MenuTestCapture *capture)
{
    g_menu_test_capture = capture;
}
#endif

/**
 * @brief Inicializa la aplicación: consola, locale, base de datos y configuración
 */
void initialize_application()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    setlocale(LC_ALL, "");

    if (!db_init())
    {
        printf("Error inicializando base de datos\n");
        exit(1);
    }

    settings_init();
    app_log_event("APP", "Aplicacion iniciada");

}

/**
 * @brief Maneja la verificación y creación del nombre de usuario
 */
void handle_user_name()
{
    char* nombre_usuario;
    char buffer[256];

    nombre_usuario = get_user_name();

    if (!nombre_usuario)
    {
        pedir_nombre_usuario();
        nombre_usuario = get_user_name();
    }

    if (nombre_usuario)
    {
        snprintf(buffer, sizeof(buffer), "%s, %s\n", get_text("welcome_message"), nombre_usuario);
        snprintf(buffer, sizeof(buffer), "Sesion iniciada por usuario: %.180s", nombre_usuario);
        app_log_event("APP", buffer);
        snprintf(buffer, sizeof(buffer), "%s, %s\n", get_text("welcome_message"), nombre_usuario);
        fputs(buffer, stdout);
        pause_console();
        free(nombre_usuario);
    }
}

/**
 * @brief Crea el menú filtrado dinámicamente
 */
MenuItem* create_filtered_menu(int* count)
{
    *count = (int)MENU_ITEM_COUNT;

    MenuItem* filtered_items = (MenuItem*)malloc((size_t)(*count) * sizeof(MenuItem));
    if (!filtered_items)
    {
        printf("Error de memoria\n");
        db_close();
        exit(1);
    }

    for (int i = 0; i < *count; i++)
    {
        filtered_items[i].opcion = (MENU_ITEMS[i].opcion == 0) ? 0 : (i + 1);
        filtered_items[i].texto = MENU_ITEMS[i].texto;
        filtered_items[i].accion = MENU_ITEMS[i].accion;
    }

    return filtered_items;
}

/**
 * @brief Ejecuta el menú principal y libera recursos
 */
void run_menu(MenuItem* filtered_items, int count)
{
    ejecutar_menu(get_text("menu_title"), filtered_items, count);
    free(filtered_items);
    db_close();
}

/**
 * @brief Ejecuta un menú interactivo en la consola
 *
 * Esta función muestra un menú con el título proporcionado y una lista de opciones.
 * Permite al usuario seleccionar una opción y ejecuta la acción correspondiente.
 * Si la acción es NULL, sale del menú.
 */
void ejecutar_menu(const char *titulo, const MenuItem *items, int cantidad)
{
#ifdef UNIT_TEST
    if (g_menu_test_capture)
    {
        g_menu_test_capture->titulo = titulo;
        g_menu_test_capture->cantidad = cantidad;
        if (items && cantidad > 0)
        {
            g_menu_test_capture->last_item = items[cantidad - 1];
        }
        else
        {
            g_menu_test_capture->last_item.opcion = 0;
            g_menu_test_capture->last_item.texto = NULL;
            g_menu_test_capture->last_item.accion = NULL;
        }
        return;
    }
#endif
    int opcion;
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Ingreso al menu: %.180s", titulo ? titulo : "(sin titulo)");
    app_log_event("MENU", log_msg);

    while (1)
    {
        clear_screen();
        print_header(titulo);

        for (int i = 0; i < cantidad; i++)
        {
            printf("%d.%s\n", items[i].opcion, items[i].texto);
        }

        opcion = input_int(">");

        const MenuItem *selected = buscar_item(items, cantidad, opcion);

        if (selected)
        {
            snprintf(log_msg, sizeof(log_msg), "Menu %.120s -> opcion %d (%.120s)",
                     titulo ? titulo : "(sin titulo)",
                     selected->opcion,
                     selected->texto ? selected->texto : "(sin texto)");
            app_log_event("MENU", log_msg);

            if (!selected->accion)
            {
                snprintf(log_msg, sizeof(log_msg), "Salida del menu: %.180s", titulo ? titulo : "(sin titulo)");
                app_log_event("MENU", log_msg);
                return;
            }

            selected->accion();
        }
        else
        {
            snprintf(log_msg, sizeof(log_msg), "Menu %.120s -> opcion invalida: %d",
                     titulo ? titulo : "(sin titulo)", opcion);
            app_log_event("MENU", log_msg);
        }
    }
}
