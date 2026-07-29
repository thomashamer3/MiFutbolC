#include "menu.h"
#include "settings.h"
#include "utils.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include "compat_windows.h"
#endif
#include "analisis.h"
#include "bienestar.h"
#include "botin.h"
#include "calendario.h"
#include "camiseta.h"
#include "cancha.h"
#include "carrera.h"
#include "colecciones.h"
#include "dashboard.h"
#include "db.h"
#include "equipo.h"
#include "estadisticas.h"
#include "financiamiento.h"
#include "lang.h"
#include "lesion.h"
#include "logros.h"
#include "media.h"
#include "musica.h"
#include "partido.h"
#include "reclutamiento.h"
#include "recordatorios.h"
#include "records_rankings.h"
#include "temporada.h"
#include "tienda.h"
#include "torneo.h"
#include "tutorial.h"
#include "atajos_config.h"
#include "resumen_compartible.h"
#include "ranking_amigos.h"

// Definir items del menu principal directamente con inicializacion static
struct MenuItemDefinition
{
    int opcion;
    const char *texto;
    void (*accion)(void);
};

static void abrir_menu_equipos(void)
{
    app_log_event("EQUIPOS", "Ingreso al modulo Equipos");
    menu_equipos();
}

static void abrir_menu_camisetas(void)
{
    app_log_event("CAMISETAS", "Ingreso al modulo Camisetas");
    menu_camisetas();
}

static void abrir_menu_canchas(void)
{
    app_log_event("CANCHAS", "Ingreso al modulo Canchas");
    menu_canchas();
}

static void abrir_menu_botines(void)
{
    app_log_event("BOTINES", "Ingreso al modulo Botines");
    menu_botines();
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

static void abrir_menu_carrera(void)
{
    app_log_event("CARRERA", "Ingreso al modulo Carrera Futbolistica");
    menu_carrera_futbolistica();
}

static void abrir_menu_colecciones(void)
{
    app_log_event("COLECCIONES", "Ingreso al modulo Colecciones e Inventario");
    menu_colecciones_inventario();
}

static void abrir_menu_recordatorios(void)
{
    app_log_event("RECORDATORIOS", "Ingreso al modulo Recordatorios");
    menu_recordatorios();
}

static void abrir_dashboard(void)
{
    app_log_event("DASHBOARD", "Ingreso al Dashboard");
    mostrar_dashboard();
}

static void abrir_calendario(void)
{
    app_log_event("CALENDARIO", "Ingreso al Calendario");
    menu_calendario();
}

static void abrir_menu_musica(void)
{
    app_log_event("MUSICA", "Ingreso al Reproductor de Musica");
    menu_musica();
}

static void abrir_menu_records_rankings(void)
{
    app_log_event("RECORDS_RANKINGS", "Ingreso al modulo Records & Rankings");
    menu_records_rankings();
}

static void abrir_menu_tiendas(void)
{
    app_log_event("TIENDAS", "Ingreso al modulo Tiendas");
    menu_tiendas();
}

static void abrir_menu_reclutamiento(void)
{
    app_log_event("RECLUTAMIENTO", "Ingreso al modulo Reclutamiento");
    menu_reclutamiento();
}

static void abrir_menu_media(void)
{
    app_log_event("MEDIA", "Ingreso al modulo Referencias Multimedia");
    menu_media();
}

static void abrir_menu_tutorial(void)
{
    app_log_event("TUTORIAL", "Ingreso al modulo Tutorial");
    menu_tutorial();
}

static void abrir_menu_atajos_config(void)
{
    app_log_event("ATAJOS_CONFIG", "Ingreso al modulo Atajos Config");
    menu_atajos_config();
}

static void abrir_menu_resumen_compartible(void)
{
    app_log_event("RESUMEN_COMPARTIBLE", "Ingreso al modulo Resumen Compartible");
    menu_resumen_compartible();
}

static void abrir_menu_ranking_amigos(void)
{
    app_log_event("RANKING_AMIGOS", "Ingreso al modulo Ranking Amigos");
    menu_ranking_amigos();
}

static const struct MenuItemDefinition MENU_ITEMS[] =
{
    {1, "Dashboard", &abrir_dashboard},
    {2, "Calendario", &abrir_calendario},
    {3, "Camisetas", &abrir_menu_camisetas},
    {4, "Canchas", &abrir_menu_canchas},
    {5, "Botines", &abrir_menu_botines},
    {6, "Equipos", &abrir_menu_equipos},
    {7, "Partidos", &abrir_menu_partidos},
    {8, "Lesiones", &abrir_menu_lesiones},
    {9, "Estadisticas", &abrir_menu_estadisticas},
    {10, "Logros", &abrir_menu_logros},
    {11, "Financiamiento", &abrir_menu_financiamiento},
    {12, "Torneos", &abrir_menu_torneos},
    {13, "Temporada", &abrir_menu_temporadas},
    {14, "Analisis", &abrir_menu_analisis},
    {15, "Bienestar", &abrir_menu_bienestar},
    {16, "Carrera Futbolistica", &abrir_menu_carrera},
    {17, "Recordatorios", &abrir_menu_recordatorios},
    {18, "Colecciones", &abrir_menu_colecciones},
    {19, "Musica", &abrir_menu_musica},
    {20, "Records & Rankings", &abrir_menu_records_rankings},
    {21, "Tiendas", &abrir_menu_tiendas},
    {22, "Reclutamiento", &abrir_menu_reclutamiento},
    {23, "Referencias Multimedia", &abrir_menu_media},
    {24, "Tutorial", &abrir_menu_tutorial},
    {25, "Atajos Config", &abrir_menu_atajos_config},
    {26, "Resumen Compartible", &abrir_menu_resumen_compartible},
    {27, "Ranking Amigos", &abrir_menu_ranking_amigos},
    {28, "Ajustes", &abrir_menu_settings},
    {0, "Salir", NULL}
};

// Numero de items en el menu principal
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

void initialize_application(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");

    if (!db_init())
    {
        printf("Error inicializando base de datos\n");
        exit(1);
    }

    if (db_get_active_user()[0] != '\0')
    {
        set_user_name(db_get_active_user());
    }

    lang_init();
    settings_init();
    app_log_event("APP", "Aplicacion iniciada");
}

void handle_user_name(void)
{
    char *nombre_usuario;
    char buffer[256];

    nombre_usuario = get_user_name();

    if (!nombre_usuario && db_get_active_user()[0] != '\0')
    {
        set_user_name(db_get_active_user());
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

MenuItem *create_filtered_menu(int *count)
{
#ifdef UNIT_TEST
    // Durante tests mantenemos el comportamiento original (lista completa)
    *count = (int)MENU_ITEM_COUNT;

    MenuItem *filtered_items = (MenuItem *)malloc((size_t)(*count) * sizeof(MenuItem));
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
#else
    // En ejecución normal, construir menú dinámico según el modo
    // (Simple/Advanced/Custom)
    int max = (int)MENU_ITEM_COUNT;
    MenuItem *filtered_items = (MenuItem *)malloc((size_t)max * sizeof(MenuItem));
    if (!filtered_items)
    {
        printf("Error de memoria\n");
        db_close();
        exit(1);
    }

    // Getters definidos en settings.h — en el mismo orden lógico que MENU_ITEMS
    const char *(*getters[])(void) = {&get_menu_dashboard,      &get_menu_calendario,
                                      &get_menu_camisetas,      &get_menu_canchas,
                                      &get_menu_botines,        &get_menu_equipos,
                                      &get_menu_partidos,       &get_menu_lesiones,
                                      &get_menu_estadisticas,   &get_menu_logros,
                                      &get_menu_financiamiento, &get_menu_torneos,
                                      &get_menu_temporada,      &get_menu_analisis,
                                      &get_menu_bienestar,      &get_menu_carrera,
                                      &get_menu_recordatorios,  &get_menu_colecciones,
                                      &get_menu_musica,         &get_menu_records_rankings,
                                      &get_menu_tiendas,        &get_menu_reclutamiento,
                                      &get_menu_media,
                                      &get_menu_tutorial,
                                      &get_menu_atajos_config,
                                      &get_menu_resumen_compartible,
                                      &get_menu_ranking_amigos, &get_menu_settings
                                     };

    int out = 0;
    for (int i = 0; i < max; i++)
    {
        const char *label = NULL;
        if (i < (int)(sizeof(getters) / sizeof(getters[0])) && getters[i])
        {
            label = getters[i]();
        }
        else
        {
            // No hay getter -- usar el texto estático del arreglo original
            label = MENU_ITEMS[i].texto;
        }

        if (!label)
        {
            // Si el getter devuelve NULL, omitimos este item (modo Custom lo permite)
            continue;
        }

        filtered_items[out].opcion = (MENU_ITEMS[i].opcion == 0) ? 0 : (out + 1);
        filtered_items[out].texto = label;
        filtered_items[out].accion = MENU_ITEMS[i].accion;
        out++;
    }

    *count = out;
    return filtered_items;
#endif
}

void run_menu(MenuItem *filtered_items, int count)
{
    ejecutar_menu(get_text("menu_title"), filtered_items, count);
    free(filtered_items);
}

static const char *menu_safe_title(const char *titulo)
{
    return titulo ? titulo : "(sin titulo)";
}

#ifdef UNIT_TEST
static int manejar_captura_test_menu(const char *titulo, const MenuItem *items, int cantidad)
{
    if (!g_menu_test_capture)
        return 0;

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

    return 1;
}
#endif

static void log_menu_opcion_invalida(const char *titulo, int opcion, char *log_msg, size_t log_size)
{
    snprintf(log_msg, log_size, "Menu %.120s -> opcion invalida: %d", menu_safe_title(titulo),
             opcion);
    app_log_event("MENU", log_msg);
}

static int ejecutar_accion_menu(const char *titulo, const MenuItem *selected, char *log_msg,
                                size_t log_size)
{
    snprintf(log_msg, log_size, "Menu %.120s -> opcion %d (%.120s)", menu_safe_title(titulo),
             selected->opcion, selected->texto ? selected->texto : "(sin texto)");
    app_log_event("MENU", log_msg);

    if (!selected->accion)
    {
        snprintf(log_msg, log_size, "Salida del menu: %.180s", menu_safe_title(titulo));
        app_log_event("MENU", log_msg);
        return 0;
    }

    selected->accion();
    return 1;
}

void ejecutar_menu(const char *titulo, const MenuItem *items, int cantidad)
{
#ifdef UNIT_TEST
    if (manejar_captura_test_menu(titulo, items, cantidad))
        return;
#endif
    int opcion;
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Ingreso al menu: %.180s", menu_safe_title(titulo));
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

        if (opcion == -1)
        {
            snprintf(log_msg, sizeof(log_msg),
                     "Menu %.120s -> entrada no valida repetida/EOF, salida preventiva",
                     menu_safe_title(titulo));
            app_log_event("MENU", log_msg);
            return;
        }

        const MenuItem *selected = buscar_item(items, cantidad, opcion);
        if (!selected)
        {
            log_menu_opcion_invalida(titulo, opcion, log_msg, sizeof(log_msg));
            continue;
        }

        if (!ejecutar_accion_menu(titulo, selected, log_msg, sizeof(log_msg)))
        {
            return;
        }
    }
}
