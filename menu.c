#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifdef _WIN32
#include <Windows.h>
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
#include "qr.h"

// Definir items del menú principal directamente con inicialización static
struct MenuItemDefinition
{
    int opcion;
    const char* texto;
    void (*accion)(void);
};

static const struct MenuItemDefinition MENU_ITEMS[] =
{
    {1, "Camisetas", &menu_camisetas},
    {2, "Canchas", &menu_canchas},
    {3, "Equipos", &menu_equipos},
    {4, "Partidos", &menu_partidos},
    {5, "Lesiones", &menu_lesiones},
    {6, "Estadisticas", &menu_estadisticas},
    {7, "QR", &menu_qr},
    {8, "Logros", &menu_logros},
    {9, "Financiamiento", &menu_financiamiento},
    {10, "Torneos", &menu_torneos},
    {11, "Temporada", &menu_temporadas},
    {12, "Analisis", &mostrar_analisis},
    {13, "Entrenador IA", &menu_entrenador_ia},
    {14, "Exportar", &menu_exportar},
    {15, "Importar", &menu_importar},
    {16, "Ajustes", &menu_settings},
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
        fputs(buffer, stdout);
        free(nombre_usuario);
        pause_console();
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
            if (!selected->accion)
            {
                return;
            }

            selected->accion();
        }
    }
}
