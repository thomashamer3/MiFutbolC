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
#include "export.h"
#include "export_all.h"
#include "import.h"
#include "ascii_art.h"
#include <Windows.h>
#include <stdlib.h>
#include <string.h>

void menu_exportar();

// Configuracion global
static AppSettings current_settings = {THEME_LIGHT, LANGUAGE_SPANISH, MODE_SIMPLE, TEXT_SIZE_MEDIUM};

// Flag para rastrear cambios en menú personalizado
static int custom_menu_changed = 0;

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

static void settings_apply_text_size();

static void habilitar_menus_basicos_custom()
{
    set_custom_menu_enabled("camisetas", 1);
    set_custom_menu_enabled("canchas", 1);
    set_custom_menu_enabled("partidos", 1);
    set_custom_menu_enabled("lesiones", 1);
    set_custom_menu_enabled("equipos", 1);
    set_custom_menu_enabled("estadisticas", 1);
    custom_menu_changed = 0; // Reset flag after setting defaults
}

struct MenuOption
{
    int opcion;
    const char* name;
    const char* display_name;
};

static int confirmar_guardado_configuracion(int default_on_fail)
{
    printf("Guardar configuracion? (S/N): ");
    char confirm;
    if (scanf_s(" %c", &confirm, 1) != 1)
    {
        confirm = default_on_fail ? 'S' : 'N';
    }

    if (confirm == '\n' || confirm == 's' || confirm == 'S')
    {
        settings_save();
        printf("%s\n", get_text("settings_saved"));
        return 1;
    }

    printf("Configuracion no guardada.\n");
    return 0;
}

static void confirmar_guardado_si_cambios()
{
    if (!custom_menu_changed)
    {
        return;
    }

    confirmar_guardado_configuracion(0);
}

static const struct MenuOption* buscar_opcion_menu(const struct MenuOption *options, int opcion)
{
    for (int j = 0; options[j].name != NULL; j++)
    {
        if (options[j].opcion == opcion)
        {
            return &options[j];
        }
    }
    return NULL;
}

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
    {"menu_bienestar", "Bienestar", "Wellness"},
    {"menu_lesiones", "Lesiones", "Injuries"},
    {"menu_financiamiento", "Financiamiento", "Financing"},
    {"menu_exportar", "Exportar", "Export"},
    {"menu_importar", "Importar", "Import"},
    {"menu_usuario", "Usuario", "User"},
    {"menu_torneos", "Torneos", "Tournaments"},
    {"menu_temporadas", "Temporadas", "Seasons"},
    {"menu_temporada", "Temporada", "Season"},
    {"menu_qr", "Codigos QR", "QR Codes"},
    {"menu_entrenador_ia", "Entrenador Virtual (IA)", "Virtual Coach (AI)"},
    {"menu_comparador", "Comparador", "Comparator"},
    {"menu_comparaciones", "Comparaciones", "Comparisons"},
    {"menu_settings", "Ajustes", "Settings"},
    {"menu_exit", "Salir", "Exit"},
    {"settings_theme", "Tema de Interfaz", "Interface Theme"},
    {"settings_language", "Idioma", "Language"},
    {"settings_mode", "Modo", "Mode"},
    {"settings_accessibility", "Accesibilidad", "Accessibility"},
    {"settings_text_size", "Tamanio de texto", "Text size"},
    {"text_size_small", "Pequenio", "Small"},
    {"text_size_medium", "Mediano", "Medium"},
    {"text_size_large", "Grande", "Large"},
    {"settings_high_contrast", "Alto Contraste", "High Contrast"},
    {"settings_accessibility_normal", "Configuracion normal", "Normal settings"},
    {"mode_simple", "Sencillo", "Simple"},
    {"mode_advanced", "Avanzado", "Advanced"},
    {"mode_custom", "Personalizado", "Custom"},
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
    {"export_menu_title", "EXPORTAR DATOS", "EXPORT DATA"},
    {"export_partidos_menu_title", "EXPORTAR PARTIDOS", "EXPORT MATCHES"},
    {"export_estadisticas_generales_menu_title", "EXPORTAR ESTADISTICAS GENERALES", "EXPORT GENERAL STATISTICS"},
    {"export_camisetas", "Camisetas", "Shirts"},
    {"export_partidos", "Partidos", "Matches"},
    {"export_lesiones", "Lesiones", "Injuries"},
    {"export_estadisticas", "Estadisticas", "Statistics"},
    {"export_analisis", "Analisis", "Analysis"},
    {"export_estadisticas_generales", "Estadisticas Generales", "General Statistics"},
    {"export_analisis_avanzado", "Analisis Avanzado", "Advanced Analysis"},
    {"export_base_datos", "Base de Datos", "Database"},
    {"export_todo", "Todo", "All"},
    {"export_informe_total_pdf", "Informe Total en PDF", "Full Report (PDF)"},
    {"export_todo_json", "Todo (JSON)", "All (JSON)"},
    {"export_todo_csv", "Todo (CSV)", "All (CSV)"},
    {"export_todos_partidos", "Todos los Partidos", "All Matches"},
    {"export_partido_mas_goles", "Partido con Mas Goles", "Match with Most Goals"},
    {"export_partido_mas_asistencias", "Partido con Mas Asistencias", "Match with Most Assists"},
    {"export_partido_menos_goles_reciente", "Partido Menos Goles Reciente", "Most Recent Match with Fewest Goals"},
    {"export_partido_menos_asistencias_reciente", "Partido Menos Asistencias Reciente", "Most Recent Match with Fewest Assists"},
    {"export_estadisticas_generales_item", "Estadisticas Generales", "General Statistics"},
    {"export_estadisticas_por_mes", "Estadisticas Por Mes", "Monthly Statistics"},
    {"export_estadisticas_por_anio", "Estadisticas Por Anio", "Yearly Statistics"},
    {"export_records_rankings", "Records & Rankings", "Records & Rankings"},
    {"import_menu_title", "IMPORTAR DATOS", "IMPORT DATA"},
    {"import_menu_json_title", "IMPORTAR DATOS DESDE JSON", "IMPORT DATA FROM JSON"},
    {"import_menu_txt_title", "IMPORTAR DATOS DESDE TXT", "IMPORT DATA FROM TXT"},
    {"import_menu_csv_title", "IMPORTAR DATOS DESDE CSV", "IMPORT DATA FROM CSV"},
    {"import_menu_html_title", "IMPORTAR DATOS DESDE HTML", "IMPORT DATA FROM HTML"},
    {"import_from_json", "Importar desde JSON", "Import from JSON"},
    {"import_from_txt", "Importar desde TXT", "Import from TXT"},
    {"import_from_csv", "Importar desde CSV", "Import from CSV"},
    {"import_from_html", "Importar desde HTML", "Import from HTML"},
    {"import_from_db", "Importar desde Base de Datos", "Import from Database"},
    {"import_camisetas", "Camisetas", "Shirts"},
    {"import_partidos", "Partidos", "Matches"},
    {"import_lesiones", "Lesiones", "Injuries"},
    {"import_estadisticas", "Estadisticas", "Statistics"},
    {"import_todo", "Todo", "All"},
    {"import_todo_json_rapido", "Importar TODO desde JSON", "Import ALL from JSON"},
    {"import_todo_csv_rapido", "Importar TODO desde CSV", "Import ALL from CSV"},
    {"backup_created", "Backup automatico creado:", "Automatic backup created:"},
    {"backup_failed", "Error creando backup automatico.", "Failed to create automatic backup."},
    {"current_settings", "Configuracion Actual", "Current Settings"},
    {"reset_settings", "Restablecer Configuracion", "Reset Settings"},
    {"reset_confirm", "Esta seguro de que desea restablecer toda la configuracion a valores por defecto?", "Are you sure you want to reset all settings to default values?"},
    {"reset_cancelled", "Operacion cancelada.", "Operation cancelled."},
    {"reset_success", "Configuracion restablecida a valores por defecto.", "Settings reset to default values."},
    {"show_current", "Ver Configuracion Actual", "Show Current Settings"},
    {"reset_defaults", "Restablecer a Valores por Defecto", "Reset to Default Values"},
    {"welcome_message", "Bienvenido De Vuelta", "Welcome Back"},
    {NULL, NULL, NULL} // Terminador
};

static void ensure_settings_schema()
{
    char *err = NULL;
    sqlite3_exec(db, "ALTER TABLE settings ADD COLUMN mode INTEGER DEFAULT 0;", NULL, NULL, &err);
    if (err)
    {
        sqlite3_free(err);
        err = NULL;
    }

    sqlite3_exec(db, "ALTER TABLE settings ADD COLUMN text_size INTEGER DEFAULT 1;", NULL, NULL, &err);
    if (err)
    {
        sqlite3_free(err);
    }
}

/**
 * @brief Inicializa el sistema de configuracion cargando desde BD
 */
void settings_init()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT theme, language, mode, text_size FROM settings WHERE id = 1;";
    int has_settings = 0;

    ensure_settings_schema();

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            current_settings.theme = sqlite3_column_int(stmt, 0);
            current_settings.language = sqlite3_column_int(stmt, 1);
            current_settings.mode = sqlite3_column_int(stmt, 2);
            current_settings.text_size = sqlite3_column_int(stmt, 3);
            has_settings = 1;
        }
        sqlite3_finalize(stmt);
    }

    // Si no hay configuracion guardada, pedir al usuario que elija el modo
    if (!has_settings)
    {
        clear_screen();
        print_header(get_text("settings_mode"));

        printf("1. %s\n", get_text("mode_simple"));
        printf("2. %s\n", get_text("mode_advanced"));
        printf("3. %s\n", get_text("mode_custom"));
        printf("0. %s\n", get_text("menu_exit"));

        int opcion = input_int("> ");

        switch (opcion)
        {
        case 1:
            settings_set_mode(MODE_SIMPLE);
            printf("%s\n", get_text("settings_saved"));
            break;
        case 2:
            settings_set_mode(MODE_ADVANCED);
            printf("%s\n", get_text("settings_saved"));
            break;
        case 3:
            current_settings.mode = MODE_CUSTOM;
            // Habilitar menús básicos por defecto en modo personalizado
            habilitar_menus_basicos_custom();
            // Mostrar menú para configurar menús
            menu_custom_menus();
            // Ask for confirmation only if changes were made
            confirmar_guardado_si_cambios();
            break;
        case 0:
            // Exit the program
            db_close();
            exit(0);
        default:
            current_settings.mode = MODE_SIMPLE;
            settings_save();
            break;
        }
    }

    // Aplicar tema al iniciar
    settings_apply_theme();
    settings_apply_text_size();
}

/**
 * @brief Guarda la configuracion actual en la base de datos
 */
void settings_save()
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO settings (id, theme, language, mode, text_size) VALUES (1, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, current_settings.theme);
        sqlite3_bind_int(stmt, 2, current_settings.language);
        sqlite3_bind_int(stmt, 3, current_settings.mode);
        sqlite3_bind_int(stmt, 4, current_settings.text_size);
        int result = sqlite3_step(stmt);
        if (result != SQLITE_DONE)
        {
            printf("Error guardando configuracion: %s\n", sqlite3_errmsg(db));
        }
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
 * @brief Establece el tamaño de texto de la aplicación
 */
void settings_set_text_size(TextSizeType text_size)
{
    current_settings.text_size = text_size;
    settings_apply_text_size();
    settings_save();
}

/**
 * @brief Establece el modo de la aplicación
 */
void settings_set_mode(ModeType mode)
{
    current_settings.mode = mode;

    // Si se cambia a modo personalizado, habilitar menús básicos por defecto
    if (mode == MODE_CUSTOM)
    {
        // Habilitar menús básicos por defecto en modo personalizado
        habilitar_menus_basicos_custom();
    }

    settings_save();
}

/**
 * @brief Obtiene el modo actual de la aplicación
 */
ModeType settings_get_mode()
{
    return current_settings.mode;
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

static void settings_apply_text_size()
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX cfi;
    memset(&cfi, 0, sizeof(cfi));
    cfi.cbSize = sizeof(cfi);

    if (!GetCurrentConsoleFontEx(hConsole, FALSE, &cfi))
    {
        return;
    }

    switch (current_settings.text_size)
    {
    case TEXT_SIZE_SMALL:
        cfi.dwFontSize.Y = 16;
        break;
    case TEXT_SIZE_LARGE:
        cfi.dwFontSize.Y = 24;
        break;
    case TEXT_SIZE_MEDIUM:
    default:
        cfi.dwFontSize.Y = 20;
        break;
    }

    cfi.dwFontSize.X = 0;
    SetCurrentConsoleFontEx(hConsole, FALSE, &cfi);
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
            return (current_settings.language == LANGUAGE_SPANISH) ?
                   text_entries[i].spanish : text_entries[i].english;
        }
    }
    return key; // Retornar la clave si no se encuentra
}

// Funciones wrapper para menu dinámico
const char* get_menu_camisetas()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_camisetas");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("camisetas"))
    {
        return get_text("menu_camisetas");
    }
    return NULL; // No mostrar en modo personalizado si no está habilitado
}

const char* get_menu_canchas()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_canchas");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("canchas"))
    {
        return get_text("menu_canchas");
    }
    return NULL; // No mostrar en modo personalizado si no está habilitado
}

const char* get_menu_partidos()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_partidos");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("partidos"))
    {
        return get_text("menu_partidos");
    }
    return NULL; // No mostrar en modo personalizado si no está habilitado
}

const char* get_menu_equipos()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_equipos");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("equipos"))
    {
        return get_text("menu_equipos");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_estadisticas()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_estadisticas");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("estadisticas"))
    {
        return get_text("menu_estadisticas");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_logros()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_logros");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("logros"))
    {
        return get_text("menu_logros");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_analisis()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_analisis");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("analisis"))
    {
        return get_text("menu_analisis");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_bienestar()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_bienestar");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("bienestar"))
    {
        return get_text("menu_bienestar");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_lesiones()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_lesiones");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("lesiones"))
    {
        return get_text("menu_lesiones");
    }
    return NULL; // No mostrar en modo personalizado si no está habilitado
}

const char* get_menu_financiamiento()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_financiamiento");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("financiamiento"))
    {
        return get_text("menu_financiamiento");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_exportar()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_exportar");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("exportar"))
    {
        return get_text("menu_exportar");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_importar()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_importar");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("importar"))
    {
        return get_text("menu_importar");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_torneos()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_torneos");
    }
    return NULL; // No mostrar en modos simple y personalizado
}

const char* get_menu_qr()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_qr");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("qr"))
    {
        return get_text("menu_qr");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_temporada()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_temporada");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("temporada"))
    {
        return get_text("menu_temporada");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
}

const char* get_menu_entrenador_ia()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_entrenador_ia");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("entrenador_ia"))
    {
        return get_text("menu_entrenador_ia");
    }
    return NULL; // No mostrar en modo simple o personalizado si no está habilitado
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

static const MenuItem* buscar_item_settings(const MenuItem *items, int cantidad, int opcion)
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
            settings_set_language(LANGUAGE_SPANISH);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 2:
            settings_set_language(LANGUAGE_ENGLISH);
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

static const char* get_current_text_size_name()
{
    switch (current_settings.text_size)
    {
    case TEXT_SIZE_SMALL:
        return get_text("text_size_small");
    case TEXT_SIZE_LARGE:
        return get_text("text_size_large");
    case TEXT_SIZE_MEDIUM:
    default:
        return get_text("text_size_medium");
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
    printf("Idioma: %s\n", current_settings.language == LANGUAGE_SPANISH ? get_text("lang_spanish") : get_text("lang_english"));
    printf("Tamanio de texto: %s\n", get_current_text_size_name());

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
 * @brief Submenú para tamaño de texto
 */
static void menu_text_size_settings()
{
    int opcion;
    do
    {
        clear_screen();
        print_header(get_text("settings_text_size"));

        printf("1. %s\n", get_text("text_size_small"));
        printf("2. %s\n", get_text("text_size_medium"));
        printf("3. %s\n", get_text("text_size_large"));
        printf("0. %s\n", get_text("menu_back"));

        opcion = input_int("> ");

        switch (opcion)
        {
        case 1:
            settings_set_text_size(TEXT_SIZE_SMALL);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 2:
            settings_set_text_size(TEXT_SIZE_MEDIUM);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 3:
            settings_set_text_size(TEXT_SIZE_LARGE);
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
 * @brief Submenú de accesibilidad
 */
static void menu_accessibility_settings()
{
    int opcion;
    do
    {
        clear_screen();
        print_header(get_text("settings_accessibility"));

        printf("1. %s: %s\n", get_text("settings_text_size"), get_current_text_size_name());
        printf("2. %s\n", get_text("settings_high_contrast"));
        printf("3. %s\n", get_text("settings_accessibility_normal"));
        printf("0. %s\n", get_text("menu_back"));

        opcion = input_int("> ");

        switch (opcion)
        {
        case 1:
            menu_text_size_settings();
            break;
        case 2:
            settings_set_theme(THEME_HIGH_CONTRAST);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 3:
            settings_set_theme(THEME_LIGHT);
            settings_set_text_size(TEXT_SIZE_MEDIUM);
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
 * @brief Restablece la configuracion a valores por defecto
 */
static void reset_settings_to_defaults()
{
    clear_screen();
    print_header(get_text("reset_settings"));

    printf("%s\n", get_text("reset_confirm"));
    printf("(S/N): ");

    char confirm;
    scanf_s(" %c", &confirm, 1);

    if (confirm == 's' || confirm == 'S')
    {
        current_settings.theme = THEME_LIGHT;
        current_settings.language = LANGUAGE_SPANISH;
        settings_apply_theme();
        settings_save();

        // Limpiar nombre de usuario también
        sqlite3_stmt *stmt;
        const char *sql = "DELETE FROM usuario;";
        if (preparar_stmt(sql, &stmt))
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
 * @brief Verifica si un menú está habilitado en modo Custom
 */
int is_custom_menu_enabled(const char* menu_name)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT enabled FROM custom_menus WHERE menu_name = ?;";
    int enabled = 0;

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, menu_name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            enabled = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return enabled;
}

/**
 * @brief Establece el estado de un menú en modo Custom
 */
void set_custom_menu_enabled(const char* menu_name, int enabled)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO custom_menus (menu_name, enabled) VALUES (?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, menu_name, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, enabled);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        custom_menu_changed = 1; // Marcar que se hizo un cambio
    }
}

/**
 * @brief Submenú para configuracion de modo
 */
static void menu_mode_settings()
{
    int opcion;
    do
    {
        clear_screen();
        print_header(get_text("settings_mode"));

        printf("1. %s\n", get_text("mode_simple"));
        printf("2. %s\n", get_text("mode_advanced"));
        printf("3. %s\n", get_text("mode_custom"));
        printf("0. %s\n", get_text("menu_back"));

        opcion = input_int("> ");

        switch (opcion)
        {
        case 1:
            settings_set_mode(MODE_SIMPLE);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 2:
            settings_set_mode(MODE_ADVANCED);
            printf("%s\n", get_text("settings_saved"));
            pause_console();
            break;
        case 3:
            current_settings.mode = MODE_CUSTOM;
            // Habilitar menús básicos por defecto en modo personalizado
            habilitar_menus_basicos_custom();
            // Mostrar menú para configurar menús
            menu_custom_menus();
            // Ask for confirmation only if changes were made
            confirmar_guardado_si_cambios();
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
 * @brief Menú para configurar menús personalizados en modo Custom
 */
void menu_custom_menus()
{
#ifdef UNIT_TEST
    return;
#endif
    custom_menu_changed = 0; // Reset flag at the beginning
    clear_screen();
    print_header(get_text("menu_settings"));

    printf("Selecciona los menus que deseas habilitar/deshabilitar:\n\n");

    // Lista de menús disponibles (excluyendo exit que siempre está)
    struct MenuOption options[] =
    {
        {1, "camisetas", get_text("menu_camisetas")},
        {2, "canchas", get_text("menu_canchas")},
        {3, "partidos", get_text("menu_partidos")},
        {4, "equipos", get_text("menu_equipos")},
        {5, "estadisticas", get_text("menu_estadisticas")},
        {6, "qr", get_text("menu_qr")},
        {7, "logros", get_text("menu_logros")},
        {8, "analisis", get_text("menu_analisis")},
        {9, "bienestar", get_text("menu_bienestar")},
        {10, "lesiones", get_text("menu_lesiones")},
        {11, "financiamiento", get_text("menu_financiamiento")},
        {12, "torneos", get_text("menu_torneos")},
        {13, "temporada", get_text("menu_temporada")},
        {14, "settings", get_text("menu_settings")},
        {0, NULL, NULL}
    };

    for (int j = 0; options[j].name != NULL; j++)
    {
        int enabled = is_custom_menu_enabled(options[j].name);
        printf("%d. [%s] %s\n", options[j].opcion, enabled ? "X" : " ", options[j].display_name);
    }

    printf("0. %s\n", get_text("menu_back"));
    printf("\nIngresa el numero del menu para alternar su estado:\n");

    int opcion = input_int("> ");

    if (opcion == 0)
    {
        return;
    }

    const struct MenuOption *option = buscar_opcion_menu(options, opcion);
    if (!option)
    {
        printf("%s\n", get_text("invalid_option"));
        pause_console();
        menu_custom_menus(); // Recargar menú
        return;
    }

    int current_state = is_custom_menu_enabled(option->name);
    set_custom_menu_enabled(option->name, !current_state);
    printf("Menu %s %s.\n", option->display_name, !current_state ? "habilitado" : "deshabilitado");
    confirmar_guardado_configuracion(1);
    pause_console();
    menu_custom_menus(); // Recargar menú
}

/**
 * @brief Menú principal de configuracion
 */
void menu_settings()
{
    MenuItem items[] =
    {
        {1, get_text("settings_theme"), menu_theme_settings},
        {2, get_text("settings_language"), menu_language_settings},
        {3, get_text("settings_accessibility"), menu_accessibility_settings},
        {4, get_text("menu_usuario"), menu_usuario},
        {5, get_text("show_current"), show_current_settings},
        {6, get_text("reset_defaults"), reset_settings_to_defaults},
        {7, get_text("settings_mode"), menu_mode_settings},
        {8, get_text("menu_exportar"), menu_exportar},
        {9, get_text("menu_importar"), menu_importar},
        {0, get_text("menu_back"), NULL}
    };

    const int item_count = (int)(sizeof(items) / sizeof(items[0]));

    while (1)
    {
        clear_screen();
        print_header(get_text("menu_settings"));

        for (int i = 0; i < item_count; i++)
        {
            printf("%d.%s\n", items[i].opcion, items[i].texto);
        }

        int opcion = input_int("> ");
        const MenuItem *selected = buscar_item_settings(items, item_count, opcion);

        if (!selected)
        {
            printf("%s\n", get_text("invalid_option"));
            pause_console();
            continue;
        }

        if (!selected->accion)
        {
            return;
        }

        selected->accion();
    }
}
