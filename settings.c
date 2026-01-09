/**
 * @file settings.c
 * @brief Implementación del sistema de configuracion avanzada
 *
 * Incluye temas de interfaz, internacionalización y persistencia en base de datos.
 */

#include "settings.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "ascii_art.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Configuracion global
static AppSettings current_settings = {THEME_LIGHT, LANG_SPANISH};

// Textos en diferentes idiomas
typedef struct
{
    const char* key;
    const char* spanish;
    const char* english;
} TextEntry;

static const TextEntry text_entries[] =
{
    {"menu_title", "MI FUTBOL C", "MI FUTBOL C"},
    {"menu_camisetas", "Camisetas", "Shirts"},
    {"menu_canchas", "Canchas", "Fields"},
    {"menu_partidos", "Partidos", "Matches"},
    {"menu_equipos", "Equipos", "Teams"},
    {"menu_estadisticas", "Estadisticas", "Statistics"},
    {"menu_logros", "Logros", "Achievements"},
    {"menu_analisis", "Analisis", "Analysis"},
    {"menu_lesiones", "Lesiones", "Injuries"},
    {"menu_financiamiento", "Financiamiento", "Financing"},
    {"menu_exportar", "Exportar", "Export"},
    {"menu_importar", "Importar", "Import"},
    {"menu_usuario", "Usuario", "User"},
    {"menu_torneos", "Torneos", "Tournaments"},
    {"menu_settings", "Ajustes", "Settings"},
    {"menu_exit", "Salir", "Exit"},
    {"settings_theme", "Tema de Interfaz", "Interface Theme"},
    {"settings_language", "Idioma", "Language"},
    {"theme_light", "Claro", "Light"},
    {"theme_dark", "Oscuro", "Dark"},
    {"theme_blue", "Azul", "Blue"},
    {"theme_green", "Verde", "Green"},
    {"theme_red", "Rojo", "Red"},
    {"theme_purple", "Morado", "Purple"},
    {"theme_classic", "Clasico", "Classic"},
    {"theme_high_contrast", "Alto Contraste", "High Contrast"},
    {"lang_spanish", "Espaniol", "Spanish"},
    {"lang_english", "Ingles", "English"},
    {"settings_saved", "Configuracion guardada exitosamente.", "Settings saved successfully."},
    {"invalid_option", "Opcion invalida.", "Invalid option."},
    {"press_enter", "Presione Enter para continuar...", "Press Enter to continue..."},
    {"welcome_back", "Bienvenido De Vuelta", "Welcome Back"},
    {"menu_back", "Volver", "Back"},
    {"current_settings", "Configuracion Actual", "Current Settings"},
    {"reset_settings", "Restablecer Configuracion", "Reset Settings"},
    {"reset_confirm", "Esta seguro de que desea restablecer toda la configuracion a valores por defecto?", "Are you sure you want to reset all settings to default values?"},
    {"reset_cancelled", "Operacion cancelada.", "Operation cancelled."},
    {"reset_success", "Configuracion restablecida a valores por defecto.", "Settings reset to default values."},
    {"show_current", "Ver Configuracion Actual", "Show Current Settings"},
    {"reset_defaults", "Restablecer a Valores por Defecto", "Reset to Default Values"},
    {"welcome_message", "Bienvenido De Vuelta, %s\n", "Welcome Back, %s\n"},
    {NULL, NULL, NULL} // Terminador
};

/**
 * @brief Inicializa el sistema de configuracion cargando desde BD
 */
void settings_init()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT theme, language FROM settings WHERE id = 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            current_settings.theme = sqlite3_column_int(stmt, 0);
            current_settings.language = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // Aplicar tema al iniciar
    settings_apply_theme();
}

/**
 * @brief Guarda la configuracion actual en la base de datos
 */
void settings_save()
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO settings (id, theme, language) VALUES (1, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, current_settings.theme);
        sqlite3_bind_int(stmt, 2, current_settings.language);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Obtiene la configuracion actual
 */
AppSettings* settings_get()
{
    return &current_settings;
}

/**
 * @brief Establece el tema de la interfaz
 */
void settings_set_theme(ThemeType theme)
{
    current_settings.theme = theme;
    settings_apply_theme();
    settings_save();
}

/**
 * @brief Establece el idioma de la aplicación
 */
void settings_set_language(LanguageType language)
{
    current_settings.language = language;
    settings_save();
}

/**
 * @brief Aplica el tema actual a la consola
 */
void settings_apply_theme()
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);

    WORD color = 0;

    switch (current_settings.theme)
    {
    case THEME_LIGHT:
        // Tema claro: fondo blanco, texto negro
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto negro
        break;

    case THEME_DARK:
        // Tema oscuro: fondo negro, texto blanco
        color = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE; // Fondo negro
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_BLUE:
        // Tema azul: fondo azul oscuro, texto blanco
        color = BACKGROUND_BLUE; // Fondo azul
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_GREEN:
        // Tema verde: fondo verde oscuro, texto negro
        color = BACKGROUND_GREEN; // Fondo verde
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto negro
        break;

    case THEME_RED:
        // Tema rojo: fondo rojo oscuro, texto blanco
        color = BACKGROUND_RED; // Fondo rojo
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_PURPLE:
        // Tema morado: fondo magenta, texto blanco
        color = BACKGROUND_RED | BACKGROUND_BLUE; // Fondo magenta
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_CLASSIC:
        // Tema clásico: colores por defecto de Windows
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Gris por defecto
        break;

    case THEME_HIGH_CONTRAST:
        // Alto contraste: fondo negro, texto amarillo brillante
        color = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE; // Fondo negro
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Texto amarillo brillante
        break;

    default:
        // Por defecto, tema claro
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    }

    SetConsoleTextAttribute(hConsole, color);
#endif
}

/**
 * @brief Obtiene el texto correspondiente al idioma actual
 */
const char* get_text(const char* key)
{
    for (int i = 0; text_entries[i].key != NULL; i++)
    {
        if (strcmp(text_entries[i].key, key) == 0)
        {
            return (current_settings.language == LANG_SPANISH) ?
                   text_entries[i].spanish : text_entries[i].english;
        }
    }
    return key; // Retornar la clave si no se encuentra
}

// Funciones wrapper para menu dinámico
const char* get_menu_camisetas()
{
    return get_text("menu_camisetas");
}
const char* get_menu_canchas()
{
    return get_text("menu_canchas");
}
const char* get_menu_partidos()
{
    return get_text("menu_partidos");
}
const char* get_menu_equipos()
{
    return get_text("menu_equipos");
}
const char* get_menu_estadisticas()
{
    return get_text("menu_estadisticas");
}
const char* get_menu_logros()
{
    return get_text("menu_logros");
}
const char* get_menu_analisis()
{
    return get_text("menu_analisis");
}
const char* get_menu_lesiones()
{
    return get_text("menu_lesiones");
}
const char* get_menu_financiamiento()
{
    return get_text("menu_financiamiento");
}
const char* get_menu_exportar()
{
    return get_text("menu_exportar");
}
const char* get_menu_importar()
{
    return get_text("menu_importar");
}
const char* get_menu_torneos()
{
    return get_text("menu_torneos");
}
const char* get_menu_settings()
{
    return get_text("menu_settings");
}
const char* get_menu_exit()
{
    return get_text("menu_exit");
}
const char* get_menu_title()
{
    return get_text("menu_title");
}
const char* get_settings_theme()
{
    return get_text("settings_theme");
}
const char* get_settings_language()
{
    return get_text("settings_language");
}
const char* get_menu_usuario()
{
    return get_text("menu_usuario");
}
const char* get_show_current()
{
    return get_text("show_current");
}
const char* get_reset_defaults()
{
    return get_text("reset_defaults");
}
const char* get_menu_back()
{
    return get_text("menu_back");
}

/**
 * @brief Submenú para configuracion de temas
 */
static void menu_theme_settings()
{
    int opcion;
    do
    {
        clear_screen();
        print_header(get_text("settings_theme"));

        printf("1. %s\n", get_text("theme_light"));
        printf("2. %s\n", get_text("theme_dark"));
        printf("3. %s\n", get_text("theme_blue"));
        printf("4. %s\n", get_text("theme_green"));
        printf("5. %s\n", get_text("theme_red"));
        printf("6. %s\n", get_text("theme_purple"));
        printf("7. %s\n", get_text("theme_classic"));
        printf("8. %s\n", get_text("theme_high_contrast"));
        printf("0. %s\n", get_text("menu_back"));

        opcion = input_int("> ");

        switch (opcion)
        {
        case 1:
            settings_set_theme(THEME_LIGHT);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 2:
            settings_set_theme(THEME_DARK);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 3:
            settings_set_theme(THEME_BLUE);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 4:
            settings_set_theme(THEME_GREEN);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 5:
            settings_set_theme(THEME_RED);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 6:
            settings_set_theme(THEME_PURPLE);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 7:
            settings_set_theme(THEME_CLASSIC);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 8:
            settings_set_theme(THEME_HIGH_CONTRAST);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 0:
            break;
        default:
            printf("%s\n", get_text("invalid_option"));
            pause_console();
        }
    }
    while (opcion != 0);
}

/**
 * @brief Submenú para configuracion de idioma
 */
static void menu_language_settings()
{
    int opcion;
    do
    {
        clear_screen();
        print_header(get_text("settings_language"));

        printf("1. %s\n", get_text("lang_spanish"));
        printf("2. %s\n", get_text("lang_english"));
        printf("0. %s\n", get_text("menu_back"));

        opcion = input_int("> ");

        switch (opcion)
        {
        case 1:
            settings_set_language(LANG_SPANISH);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 2:
            settings_set_language(LANG_ENGLISH);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 0:
            break;
        default:
            printf("%s\n", get_text("invalid_option"));
            pause_console();
        }
    }
    while (opcion != 0);
}

/**
 * @brief Obtiene el nombre del tema actual
 */
static const char* get_current_theme_name()
{
    switch (current_settings.theme)
    {
    case THEME_LIGHT:
        return get_text("theme_light");
    case THEME_DARK:
        return get_text("theme_dark");
    case THEME_BLUE:
        return get_text("theme_blue");
    case THEME_GREEN:
        return get_text("theme_green");
    case THEME_RED:
        return get_text("theme_red");
    case THEME_PURPLE:
        return get_text("theme_purple");
    case THEME_CLASSIC:
        return get_text("theme_classic");
    case THEME_HIGH_CONTRAST:
        return get_text("theme_high_contrast");
    default:
        return get_text("theme_light");
    }
}

/**
 * @brief Muestra la configuracion actual
 */
static void show_current_settings()
{
    clear_screen();
    print_header(get_text("current_settings"));

    printf("Tema: %s\n", get_current_theme_name());
    printf("Idioma: %s\n", current_settings.language == LANG_SPANISH ? get_text("lang_spanish") : get_text("lang_english"));

    char *usuario = get_user_name();
    if (usuario)
    {
        printf("Usuario: %s\n", usuario);
        free(usuario);
    }
    else
    {
        printf("Usuario: No configurado\n");
    }

    printf("\n");
    pause_console();
}

/**
 * @brief Restablece la configuracion a valores por defecto
 */
static void reset_settings_to_defaults()
{
    clear_screen();
    print_header(get_text("reset_settings"));

    printf("%s\n", get_text("reset_confirm"));
    printf("(S/N): ");

    char confirm;
    scanf(" %c", &confirm);

    if (confirm == 's' || confirm == 'S')
    {
        current_settings.theme = THEME_LIGHT;
        current_settings.language = LANG_SPANISH;
        settings_apply_theme();
        settings_save();

        // Limpiar nombre de usuario también
        sqlite3_stmt *stmt;
        const char *sql = "DELETE FROM usuario;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        printf("%s\n", get_text("reset_success"));
    }
    else
    {
        printf("%s\n", get_text("reset_cancelled"));
    }

    pause_console();
}

/**
 * @brief Menú principal de configuracion
 */
void menu_settings()
{
    // Mostrar ASCII art de ajustes
    printf("%s\n\n", ASCII_AJUSTES);

    MenuItem items[] =
    {
        {1, get_text("settings_theme"), menu_theme_settings},
        {2, get_text("settings_language"), menu_language_settings},
        {3, get_text("menu_usuario"), menu_usuario},
        {4, get_text("show_current"), show_current_settings},
        {5, get_text("reset_defaults"), reset_settings_to_defaults},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("menu_settings"), items, 6);
}
