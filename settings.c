
#include "settings.h"
#include "atajos.h"
#include "backup.h"
#include "busqueda.h"
#include "cJSON.h"
#include "lang.h"
#include "db.h"
#include "db_integridad.h"
#include "export_all.h"
#include "export_ods.h"
#include "import.h"
#include "menu.h"
#include "notificaciones.h"
#include "undo.h"
#include "utils.h"
#ifdef _WIN32
#include <windows.h>
#else
#include "compat_windows.h"
#endif
#ifdef _WIN32
#include <shellapi.h>
#endif
#include <stdlib.h>
#include <string.h>

#define SETTINGS_MUSIC_VOLUME_DEFAULT 0.8f
#define SETTINGS_MUSIC_REPEAT_DEFAULT 0
#define SETTINGS_MUSIC_EQ_ENABLED_DEFAULT 0
#define SETTINGS_MUSIC_EQ_DB_DEFAULT 0.0f
#define SETTINGS_MUSIC_EQ_DB_MIN (-12.0f)
#define SETTINGS_MUSIC_EQ_DB_MAX (12.0f)
#define SETTINGS_MUSIC_VOLUME_STEP_DEFAULT 0.1f

#ifdef _WIN32
typedef HRESULT(WINAPI *URLDownloadToFileAFunc)(LPUNKNOWN, LPCSTR, LPCSTR,
        DWORD, LPVOID);
#endif

void menu_exportar(void);
void menu_update(void);

// Repositorio usado para buscar la release. Formato: owner/repo
// Si deseas cambiar, modifica esta constante.
#define UPDATE_REPO "thomashamer3/MiFutbolC"

// Version actual de la aplicacion. Debe mantenerse en sincronia con el
// instalador (MiFutbolC.iss)
#define APP_VERSION "4.1"

// Configuracion global
static AppSettings current_settings = {THEME_LIGHT,
                                       LANGUAGE_SPANISH,
                                       MODE_SIMPLE,
                                       TEXT_SIZE_MEDIUM,
                                       1,
                                       SETTINGS_MUSIC_VOLUME_DEFAULT,
                                       SETTINGS_MUSIC_REPEAT_DEFAULT,
                                       SETTINGS_MUSIC_EQ_ENABLED_DEFAULT,
                                       SETTINGS_MUSIC_EQ_DB_DEFAULT,
                                       SETTINGS_MUSIC_EQ_DB_DEFAULT,
                                       SETTINGS_MUSIC_EQ_DB_DEFAULT,
                                       SETTINGS_MUSIC_VOLUME_STEP_DEFAULT,
                                       1
                                      };

static void settings_apply_text_size(void);
static void habilitar_menus_basicos_custom(void);

static char label_music_autoplay[96];
static char label_dashboard_enabled[96];

static void settings_actualizar_label_toggle(const char *clave_base,
        int habilitado, char *destino,
        size_t destino_size)
{
    const char *base = get_text(clave_base);
    const char *estado =
        habilitado ? get_text("state_on") : get_text("state_off");
    snprintf(destino, destino_size, "%s: %s", base, estado);
}

static void set_theme_int(int value)
{
    settings_set_theme((ThemeType)value);
}

static void set_language_int(int value)
{
    settings_set_language((LanguageType)value);
}

static void set_mode_int(int value)
{
    settings_set_mode((ModeType)value);
}

static void set_text_size_int(int value)
{
    settings_set_text_size((TextSizeType)value);
}

static void set_music_autoplay_int(int value)
{
    settings_set_music_autoplay(value);
}

static void set_dashboard_enabled_int(int value)
{
    settings_set_dashboard_enabled(value);
}

static void aplicar_config_y_pausar(void (*setter)(int), int value)
{
    setter(value);
    printf("%s\n", get_text("settings_saved"));
    pause_console();
}

static void aplicar_tema_texto_y_pausar(ThemeType theme,
                                        TextSizeType text_size)
{
    settings_set_theme(theme);
    settings_set_text_size(text_size);
    printf("%s\n", get_text("settings_saved"));
    pause_console();
}

#define DEFINE_SETTING_ACTION(name, setter, value)                             \
  static void name(void) { aplicar_config_y_pausar(setter, value); }

#define DEFINE_THEME_TEXT_ACTION(name, theme, text_size)                       \
  static void name(void) { aplicar_tema_texto_y_pausar(theme, text_size); }

/* Wrappers para usar como acciones en MenuItem (sin argumentos) */
DEFINE_SETTING_ACTION(theme_set_light, set_theme_int, THEME_LIGHT)
DEFINE_SETTING_ACTION(theme_set_dark, set_theme_int, THEME_DARK)
DEFINE_SETTING_ACTION(theme_set_blue, set_theme_int, THEME_BLUE)
DEFINE_SETTING_ACTION(theme_set_green, set_theme_int, THEME_GREEN)
DEFINE_SETTING_ACTION(theme_set_red, set_theme_int, THEME_RED)
DEFINE_SETTING_ACTION(theme_set_purple, set_theme_int, THEME_PURPLE)
DEFINE_SETTING_ACTION(theme_set_classic, set_theme_int, THEME_CLASSIC)
DEFINE_SETTING_ACTION(theme_set_high_contrast, set_theme_int,
                      THEME_HIGH_CONTRAST)
DEFINE_SETTING_ACTION(lang_set_spanish, set_language_int, LANGUAGE_SPANISH)
DEFINE_SETTING_ACTION(lang_set_english, set_language_int, LANGUAGE_ENGLISH)
DEFINE_SETTING_ACTION(text_size_small, set_text_size_int, TEXT_SIZE_SMALL)
DEFINE_SETTING_ACTION(text_size_medium, set_text_size_int, TEXT_SIZE_MEDIUM)
DEFINE_SETTING_ACTION(text_size_large, set_text_size_int, TEXT_SIZE_LARGE)
DEFINE_SETTING_ACTION(accessibility_high_contrast, set_theme_int,
                      THEME_HIGH_CONTRAST)
DEFINE_THEME_TEXT_ACTION(accessibility_normal_theme_text, THEME_LIGHT,
                         TEXT_SIZE_MEDIUM)
DEFINE_SETTING_ACTION(mode_set_simple, set_mode_int, MODE_SIMPLE)
DEFINE_SETTING_ACTION(mode_set_advanced, set_mode_int, MODE_ADVANCED)
static void mode_set_custom(void)
{
    settings_set_mode(MODE_CUSTOM);
    menu_custom_menus();
    pause_console();
}

static void toggle_music_autoplay_setting(void)
{
    aplicar_config_y_pausar(set_music_autoplay_int,
                            current_settings.music_autoplay ? 0 : 1);
    settings_actualizar_label_toggle(
        "settings_music_autoplay", current_settings.music_autoplay,
        label_music_autoplay, sizeof(label_music_autoplay));
    settings_actualizar_label_toggle(
        "settings_dashboard_enabled", current_settings.dashboard_enabled,
        label_dashboard_enabled, sizeof(label_dashboard_enabled));
}

static void toggle_dashboard_enabled_setting(void)
{
    aplicar_config_y_pausar(set_dashboard_enabled_int,
                            current_settings.dashboard_enabled ? 0 : 1);
    settings_actualizar_label_toggle(
        "settings_music_autoplay", current_settings.music_autoplay,
        label_music_autoplay, sizeof(label_music_autoplay));
    settings_actualizar_label_toggle(
        "settings_dashboard_enabled", current_settings.dashboard_enabled,
        label_dashboard_enabled, sizeof(label_dashboard_enabled));
}

#define DEFINE_ABRIR_DESDE_SETTINGS(name, event, display, func_call) \
    static void abrir_##name##_desde_settings(void) { \
        app_log_event(event, "Ingreso a " display " desde Ajustes"); \
        func_call; \
    }

DEFINE_ABRIR_DESDE_SETTINGS(busqueda_global, "BUSQUEDA", "Busqueda Global", menu_busqueda_global())
DEFINE_ABRIR_DESDE_SETTINGS(backup, "BACKUP", "Backup & Restore", menu_backup_restore())
DEFINE_ABRIR_DESDE_SETTINGS(integridad, "INTEGRIDAD", "Integridad BD", menu_integridad_bd())
DEFINE_ABRIR_DESDE_SETTINGS(notificaciones, "NOTIFICACIONES", "Notificaciones", menu_notificaciones())
DEFINE_ABRIR_DESDE_SETTINGS(export_ods, "EXPORT_ODS", "Exportacion ODS", menu_exportar_ods())

static void abrir_undo_desde_settings(void)
{
    app_log_event("UNDO", "Ingreso a Deshacer desde Ajustes");
    undo_mostrar_historial();
    pause_console();
}

static void habilitar_menus_basicos_custom(void)
{
    set_custom_menu_enabled("camisetas", 1);
    set_custom_menu_enabled("canchas", 1);
    set_custom_menu_enabled("partidos", 1);
    set_custom_menu_enabled("lesiones", 1);
    set_custom_menu_enabled("equipos", 1);
    set_custom_menu_enabled("estadisticas", 1);
}

struct MenuOption
{
    int opcion;
    const char *name;
    const char *display_name;
};

static int extraer_primer_caracter(const char *input, char *out)
{
    if (!input || !out)
    {
        return 0;
    }

    for (size_t i = 0; input[i] != '\0'; ++i)
    {
        char c = input[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            continue;
        }
        *out = c;
        return 1;
    }

    return 0;
}

static const struct MenuOption *
buscar_opcion_menu(const struct MenuOption *options, int opcion)
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



static void settings_exec_ignore_error(const char *sql)
{
    char *err = NULL;
    sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err)
    {
        sqlite3_free(err);
    }
}

static void ensure_settings_schema(void)
{
#define ALTER_SETTINGS_COLUMN(column_def)                                      \
  "ALTER TABLE settings ADD COLUMN " column_def ";"
    const char *alter_statements[] =
    {
        ALTER_SETTINGS_COLUMN("mode INTEGER DEFAULT 0"),
        ALTER_SETTINGS_COLUMN("text_size INTEGER DEFAULT 1"),
        ALTER_SETTINGS_COLUMN("image_viewer TEXT DEFAULT ''"),
        ALTER_SETTINGS_COLUMN("music_autoplay INTEGER DEFAULT 1"),
        ALTER_SETTINGS_COLUMN("music_volume REAL DEFAULT 0.8"),
        ALTER_SETTINGS_COLUMN("music_repeat_mode INTEGER DEFAULT 0"),
        ALTER_SETTINGS_COLUMN("music_eq_enabled INTEGER DEFAULT 0"),
        ALTER_SETTINGS_COLUMN("music_eq_bass_db REAL DEFAULT 0"),
        ALTER_SETTINGS_COLUMN("music_eq_mid_db REAL DEFAULT 0"),
        ALTER_SETTINGS_COLUMN("music_eq_treble_db REAL DEFAULT 0"),
        ALTER_SETTINGS_COLUMN("music_volume_step REAL DEFAULT 0.1"),
        ALTER_SETTINGS_COLUMN("dashboard_enabled INTEGER DEFAULT 1"),
        NULL
    };

#undef ALTER_SETTINGS_COLUMN

    for (int i = 0; alter_statements[i] != NULL; i++)
    {
        settings_exec_ignore_error(alter_statements[i]);
    }
}

static void settings_prompt_mode_selection(void)
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
        settings_set_mode(MODE_CUSTOM);
        menu_custom_menus();
        break;
    case 0:
        finalizar_atajos();
        db_close();
        exit(0);
    default:
        settings_set_mode(MODE_SIMPLE);
        settings_save();
    }
}

void settings_init(void)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT theme, language, mode, text_size, music_autoplay, "
                      "music_volume, music_repeat_mode, music_eq_enabled, "
                      "music_eq_bass_db, music_eq_mid_db, music_eq_treble_db, "
                      "music_volume_step, dashboard_enabled "
                      "FROM settings WHERE id = 1;";
    int has_settings = 0;

    ensure_settings_schema();

    if (db_prepare_stmt_with_error(&stmt, sql, "Error al preparar la consulta"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            current_settings.theme = sqlite3_column_int(stmt, 0);
            current_settings.language = sqlite3_column_int(stmt, 1);
            current_settings.mode = sqlite3_column_int(stmt, 2);
            current_settings.text_size = sqlite3_column_int(stmt, 3);
            current_settings.music_autoplay = sqlite3_column_int(stmt, 4) ? 1 : 0;
            current_settings.music_volume =
                utils_clamp_float((float)sqlite3_column_double(stmt, 5), 0.0f, 1.0f);
            current_settings.music_repeat_mode =
                utils_clamp_int(sqlite3_column_int(stmt, 6), 0, 3);
            current_settings.music_eq_enabled = sqlite3_column_int(stmt, 7) ? 1 : 0;
            current_settings.music_eq_bass_db =
                utils_clamp_float((float)sqlite3_column_double(stmt, 8),
                                  SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX);
            current_settings.music_eq_mid_db =
                utils_clamp_float((float)sqlite3_column_double(stmt, 9),
                                  SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX);
            current_settings.music_eq_treble_db =
                utils_clamp_float((float)sqlite3_column_double(stmt, 10),
                                  SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX);
            current_settings.music_volume_step = utils_clamp_float(
                    (float)sqlite3_column_double(stmt, 11), 0.01f, 0.20f);
            current_settings.dashboard_enabled = sqlite3_column_int(stmt, 12) ? 1 : 0;
            has_settings = 1;
        }
        db_stmt_release(stmt);
    }

    if (!has_settings)
    {
        settings_prompt_mode_selection();
    }

    settings_apply_theme();
    settings_apply_text_size();
}

void settings_save(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT OR REPLACE INTO settings ("
        "id, theme, language, mode, text_size, music_autoplay, music_volume, "
        "music_repeat_mode, music_eq_enabled, music_eq_bass_db, music_eq_mid_db, "
        "music_eq_treble_db, music_volume_step, dashboard_enabled"
        ") VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    if (db_prepare_stmt_with_error(&stmt, sql, "Error al preparar la consulta"))
    {
        sqlite3_bind_int(stmt, 1, current_settings.theme);
        sqlite3_bind_int(stmt, 2, current_settings.language);
        sqlite3_bind_int(stmt, 3, current_settings.mode);
        sqlite3_bind_int(stmt, 4, current_settings.text_size);
        sqlite3_bind_int(stmt, 5, current_settings.music_autoplay ? 1 : 0);
        sqlite3_bind_double(
            stmt, 6,
            (double)utils_clamp_float(current_settings.music_volume, 0.0f, 1.0f));
        sqlite3_bind_int(stmt, 7,
                         utils_clamp_int(current_settings.music_repeat_mode, 0, 3));
        sqlite3_bind_int(stmt, 8, current_settings.music_eq_enabled ? 1 : 0);
        sqlite3_bind_double(
            stmt, 9,
            (double)utils_clamp_float(current_settings.music_eq_bass_db,
                                      SETTINGS_MUSIC_EQ_DB_MIN,
                                      SETTINGS_MUSIC_EQ_DB_MAX));
        sqlite3_bind_double(
            stmt, 10,
            (double)utils_clamp_float(current_settings.music_eq_mid_db,
                                      SETTINGS_MUSIC_EQ_DB_MIN,
                                      SETTINGS_MUSIC_EQ_DB_MAX));
        sqlite3_bind_double(
            stmt, 11,
            (double)utils_clamp_float(current_settings.music_eq_treble_db,
                                      SETTINGS_MUSIC_EQ_DB_MIN,
                                      SETTINGS_MUSIC_EQ_DB_MAX));
        sqlite3_bind_double(stmt, 12,
                            (double)utils_clamp_float(
                                current_settings.music_volume_step, 0.01f, 0.20f));
        sqlite3_bind_int(stmt, 13, current_settings.dashboard_enabled ? 1 : 0);
        int result = sqlite3_step(stmt);
        if (result != SQLITE_DONE)
        {
            printf("Error guardando configuracion: %s\n", sqlite3_errmsg(db));
        }
        db_stmt_release(stmt);
    }
}

AppSettings *settings_get(void)
{
    return &current_settings;
}

void settings_set_theme(ThemeType theme)
{
    current_settings.theme = theme;
    settings_apply_theme();
    settings_save();
}

void settings_set_language(LanguageType language)
{
    current_settings.language = language;
    lang_set(language == LANGUAGE_SPANISH ? "es" : "en");
    settings_save();
}

void settings_set_text_size(TextSizeType text_size)
{
    current_settings.text_size = text_size;
    settings_apply_text_size();
    settings_save();
}

void settings_set_mode(ModeType mode)
{
    current_settings.mode = mode;

    // Si se cambia a modo personalizado, habilitar menus basicos por defecto
    if (mode == MODE_CUSTOM)
    {
        // Habilitar menus basicos por defecto en modo personalizado
        habilitar_menus_basicos_custom();
    }

    settings_save();
}

ModeType settings_get_mode(void)
{
    return current_settings.mode;
}

void settings_set_music_autoplay(int enabled)
{
    current_settings.music_autoplay = enabled ? 1 : 0;
    settings_save();
}

int settings_get_music_autoplay(void)
{
    return current_settings.music_autoplay;
}

void settings_set_music_volume(float volume)
{
    current_settings.music_volume = utils_clamp_float(volume, 0.0f, 1.0f);
    settings_save();
}

float settings_get_music_volume(void)
{
    return utils_clamp_float(current_settings.music_volume, 0.0f, 1.0f);
}

void settings_set_music_repeat_mode(int mode)
{
    current_settings.music_repeat_mode = utils_clamp_int(mode, 0, 3);
    settings_save();
}

int settings_get_music_repeat_mode(void)
{
    return utils_clamp_int(current_settings.music_repeat_mode, 0, 3);
}

void settings_set_music_eq_enabled(int enabled)
{
    current_settings.music_eq_enabled = enabled ? 1 : 0;
    settings_save();
}

int settings_get_music_eq_enabled(void)
{
    return current_settings.music_eq_enabled ? 1 : 0;
}

#define DEFINE_EQ_BAND(name, field)                                 \
void settings_set_music_eq_##name##_db(float gain_value)            \
{                                                                    \
    current_settings.field = utils_clamp_float(                      \
        gain_value, SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX); \
    settings_save();                                                \
}                                                                    \
float settings_get_music_eq_##name##_db(void)                       \
{                                                                    \
    return utils_clamp_float(current_settings.field,                 \
                             SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX); \
}

DEFINE_EQ_BAND(bass, music_eq_bass_db)
DEFINE_EQ_BAND(mid, music_eq_mid_db)
DEFINE_EQ_BAND(treble, music_eq_treble_db)

void settings_set_music_eq_profile(int enabled, float bass_db, float mid_db,
                                   float treble_db)
{
    current_settings.music_eq_enabled = enabled ? 1 : 0;
    current_settings.music_eq_bass_db = utils_clamp_float(
                                            bass_db, SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX);
    current_settings.music_eq_mid_db = utils_clamp_float(
                                           mid_db, SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX);
    current_settings.music_eq_treble_db = utils_clamp_float(
            treble_db, SETTINGS_MUSIC_EQ_DB_MIN, SETTINGS_MUSIC_EQ_DB_MAX);
    settings_save();
}

void settings_set_music_volume_step(float step)
{
    current_settings.music_volume_step = utils_clamp_float(step, 0.01f, 0.20f);
    settings_save();
}

float settings_get_music_volume_step(void)
{
    return utils_clamp_float(current_settings.music_volume_step, 0.01f, 0.20f);
}

void settings_set_dashboard_enabled(int enabled)
{
    current_settings.dashboard_enabled = enabled ? 1 : 0;
    settings_save();
}

int settings_get_dashboard_enabled(void)
{
    return current_settings.dashboard_enabled ? 1 : 0;
}

static WORD get_theme_color(ThemeType theme)
{
    switch (theme)
    {
    case THEME_LIGHT:
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case THEME_DARK:
        return BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE |
               FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case THEME_BLUE:
        return BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN |
               FOREGROUND_BLUE;
    case THEME_GREEN:
        return BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN |
               FOREGROUND_BLUE;
    case THEME_RED:
        return BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case THEME_PURPLE:
        return BACKGROUND_RED | BACKGROUND_BLUE | FOREGROUND_RED |
               FOREGROUND_GREEN | FOREGROUND_BLUE;
    case THEME_CLASSIC:
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case THEME_HIGH_CONTRAST:
        return BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE |
               FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    default:
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
}

void settings_apply_theme(void)
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);

    WORD color = get_theme_color(current_settings.theme);

    SetConsoleTextAttribute(hConsole, color);
#endif
}

static void settings_apply_text_size(void)
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

const char *get_text(const char *key)
{
    return tr(key);
}

// Funciones wrapper para menu dinamico
static const char *get_menu_text_by_mode(const char *text_key,
        const char *custom_menu_name,
        int visible_simple,
        int visible_advanced,
        int visible_custom)
{
    ModeType mode = settings_get_mode();

    if ((visible_simple && mode == MODE_SIMPLE) ||
            (visible_advanced && mode == MODE_ADVANCED))
    {
        return get_text(text_key);
    }

    if (visible_custom && mode == MODE_CUSTOM && custom_menu_name &&
            is_custom_menu_enabled(custom_menu_name))
    {
        return get_text(text_key);
    }

    return NULL;
}

#define DEFINE_GET_MENU(name, text_key, custom_menu, vis_simple, vis_adv, vis_custom) \
const char *get_menu_##name(void)                                                      \
{                                                                                      \
    return get_menu_text_by_mode(text_key, custom_menu, vis_simple, vis_adv, vis_custom); \
}

#define DEFINE_GET_MENU_TEXT(name, text_key) \
const char *get_##name(void)                \
{                                           \
    return get_text(text_key);             \
}

DEFINE_GET_MENU(camisetas, "menu_camisetas", "camisetas", 1, 1, 1)
DEFINE_GET_MENU(canchas, "menu_canchas", "canchas", 1, 1, 1)
DEFINE_GET_MENU(partidos, "menu_partidos", "partidos", 1, 1, 1)
DEFINE_GET_MENU(equipos, "menu_equipos", "equipos", 0, 1, 1)
DEFINE_GET_MENU(estadisticas, "menu_estadisticas", "estadisticas", 0, 1, 1)
DEFINE_GET_MENU(logros, "menu_logros", "logros", 0, 1, 1)
DEFINE_GET_MENU(analisis, "menu_analisis", "analisis", 0, 1, 1)
DEFINE_GET_MENU(bienestar, "menu_bienestar", "bienestar", 0, 1, 1)
DEFINE_GET_MENU(lesiones, "menu_lesiones", "lesiones", 1, 1, 1)
DEFINE_GET_MENU(financiamiento, "menu_financiamiento", "financiamiento", 0, 1, 1)
DEFINE_GET_MENU(exportar, "menu_exportar", "exportar", 0, 1, 1)
DEFINE_GET_MENU(importar, "menu_importar", "importar", 0, 1, 1)
DEFINE_GET_MENU(torneos, "menu_torneos", NULL, 0, 1, 0)
DEFINE_GET_MENU(temporada, "menu_temporada", "temporada", 0, 1, 1)
DEFINE_GET_MENU(entrenador_ia, "menu_entrenador_ia", "entrenador_ia", 0, 1, 1)
DEFINE_GET_MENU(records_rankings, "menu_records_rankings", "records_rankings", 0, 1, 1)
DEFINE_GET_MENU_TEXT(menu_settings, "menu_settings")
DEFINE_GET_MENU_TEXT(menu_exit, "menu_exit")
DEFINE_GET_MENU_TEXT(menu_title, "menu_title")
DEFINE_GET_MENU_TEXT(menu_usuario, "menu_usuario")
DEFINE_GET_MENU_TEXT(menu_dashboard, "menu_dashboard")
DEFINE_GET_MENU_TEXT(menu_calendario, "menu_calendario")
DEFINE_GET_MENU_TEXT(menu_carrera, "menu_carrera")
DEFINE_GET_MENU_TEXT(menu_recordatorios, "menu_recordatorios")
DEFINE_GET_MENU_TEXT(menu_colecciones, "menu_colecciones")
DEFINE_GET_MENU_TEXT(menu_musica, "menu_musica")
DEFINE_GET_MENU_TEXT(settings_theme, "settings_theme")
DEFINE_GET_MENU_TEXT(settings_language, "settings_language")
DEFINE_GET_MENU_TEXT(show_current, "show_current")
DEFINE_GET_MENU_TEXT(reset_defaults, "reset_defaults")
DEFINE_GET_MENU_TEXT(menu_back, "menu_back")
DEFINE_GET_MENU_TEXT(menu_tiendas, "menu_tiendas")
DEFINE_GET_MENU_TEXT(menu_reclutamiento, "menu_reclutamiento")
DEFINE_GET_MENU_TEXT(menu_media, "menu_media")

#ifdef _WIN32
static void obtener_nombre_repo(const char *owner_repo, char *repo_name,
                                size_t repo_name_size)
{
    const char *slash = strrchr(owner_repo, '/');
    if (slash && *(slash + 1) != '\0')
    {
        snprintf(repo_name, repo_name_size, "%s", slash + 1);
        return;
    }

    snprintf(repo_name, repo_name_size, "%s", owner_repo);
}

static int cargar_descargador(URLDownloadToFileAFunc *out_downloader,
                              HMODULE *out_module)
{
    *out_downloader = NULL;
    *out_module = LoadLibraryA("urlmon.dll");
    if (*out_module)
    {
        /* Evitar warning de cast entre tipos de funcion distintos */
        union
        {
            FARPROC fp;
            URLDownloadToFileAFunc fn;
        } downloader_union;

        downloader_union.fp = GetProcAddress(*out_module, "URLDownloadToFileA");
        *out_downloader = downloader_union.fn;
    }

    return *out_downloader != NULL;
}

static int leer_archivo_completo(const char *path, char **out_data);
static const char *obtener_release_label(const cJSON *tag, const cJSON *name);
static int descargar_archivo(URLDownloadToFileAFunc downloader, const char *url,
                             const char *dest);

static cJSON *
descargar_y_parsear_release_json(URLDownloadToFileAFunc downloader,
                                 const char *api_url, const char *json_path)
{
    if (!descargar_archivo(downloader, api_url, json_path))
    {
        return NULL;
    }

    char *json_data = NULL;
    if (!leer_archivo_completo(json_path, &json_data))
    {
        return NULL;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root || !cJSON_IsObject(root))
    {
        if (root)
            cJSON_Delete(root);
        return NULL;
    }

    return root;
}

static void liberar_descargador(HMODULE module)
{
    if (module)
    {
        FreeLibrary(module);
    }
}

static void liberar_releases(char *release_names[], char *asset_urls[],
                             int count)
{
    for (int i = 0; i < count; i++)
    {
        free(release_names[i]);
        free(asset_urls[i]);
    }
}

static int descargar_archivo(URLDownloadToFileAFunc downloader, const char *url,
                             const char *dest)
{
    HRESULT hr = E_FAIL;
    if (downloader)
    {
        hr = downloader(NULL, url, dest, 0, NULL);
    }

    return hr == S_OK;
}

static int ejecutar_instalador(const char *dest)
{
    HINSTANCE h = ShellExecuteA(NULL, "open", dest, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        // Intentar elevar privilegios si la ejecucion fallo la primera vez.
        HINSTANCE h2 =
            ShellExecuteA(NULL, "runas", dest, NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)h2 > 32)
        {
            return 1;
        }

        // Si falla, es probable que el ejecutable este bloqueado (porque MiFutbolC
        // esta en ejecucion).
        printf("Error al ejecutar el instalador. Asegurate de cerrar MiFutbolC y "
               "vuelve a intentarlo.\n");
        return 0;
    }
    return 1;
}

// Compara versiones semanticas simples (mayor.minor.patch).
// Retorna -1 si a < b, 0 si son iguales, 1 si a > b.
static void parsear_version_tripleta(const char *version, int *major,
                                     int *minor, int *patch)
{
    const char *p = version ? version : "";
    char *endptr = NULL;

    *major = 0;
    *minor = 0;
    *patch = 0;

    long value = strtol(p, &endptr, 10);
    if (endptr == p)
    {
        return;
    }
    *major = (int)value;

    if (*endptr != '.')
    {
        return;
    }

    p = endptr + 1;
    value = strtol(p, &endptr, 10);
    if (endptr == p)
    {
        return;
    }
    *minor = (int)value;

    if (*endptr != '.')
    {
        return;
    }

    p = endptr + 1;
    value = strtol(p, &endptr, 10);
    if (endptr == p)
    {
        return;
    }
    *patch = (int)value;
}

static int comparar_versiones(const char *a, const char *b)
{
    int am = 0;
    int an = 0;
    int ap = 0;
    int bm = 0;
    int bn = 0;
    int bp = 0;

    // Permitir cadenas que comiencen con "v" o "V".
    if (a && (*a == 'v' || *a == 'V'))
    {
        a++;
    }
    if (b && (*b == 'v' || *b == 'V'))
    {
        b++;
    }

    parsear_version_tripleta(a, &am, &an, &ap);
    parsear_version_tripleta(b, &bm, &bn, &bp);

    if (am != bm)
        return am < bm ? -1 : 1;
    if (an != bn)
        return an < bn ? -1 : 1;
    if (ap != bp)
        return ap < bp ? -1 : 1;
    return 0;
}

// Descarga la informacion de la ultima release desde GitHub y devuelve su tag
// (tag_name o name). El valor devuelto debe liberarse con free() por el
// llamador.
static char *obtener_latest_release_tag(const char *owner_repo,
                                        const char *repo_name,
                                        const char *temp_path)
{
    URLDownloadToFileAFunc downloader;
    HMODULE module;
    if (!cargar_descargador(&downloader, &module))
    {
        return NULL;
    }

    char api_url[1024];
    char json_path[1024];
    snprintf(api_url, sizeof(api_url),
             "https://api.github.com/repos/%s/releases/latest", owner_repo);
    snprintf(json_path, sizeof(json_path), "%s%s_latest_release.json", temp_path,
             repo_name);

    if (!descargar_archivo(downloader, api_url, json_path))
    {
        liberar_descargador(module);
        return NULL;
    }

    char *json_data = NULL;
    if (!leer_archivo_completo(json_path, &json_data))
    {
        liberar_descargador(module);
        return NULL;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root || !cJSON_IsObject(root))
    {
        if (root)
        {
            cJSON_Delete(root);
        }
        liberar_descargador(module);
        return NULL;
    }

    const cJSON *tag = cJSON_GetObjectItem(root, "tag_name");
    const cJSON *name = cJSON_GetObjectItem(root, "name");
    const char *label = obtener_release_label(tag, name);

    char *result = _strdup(label);
    cJSON_Delete(root);
    liberar_descargador(module);
    return result;
}

static int leer_archivo_completo(const char *path, char **out_data)
{
    FILE *f = NULL;
    errno_t open_err = fopen_s(&f, path, "rb");
    if (open_err != 0 || !f)
    {
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }

    long fsize = ftell(f);
    if (fsize <= 0)
    {
        fclose(f);
        return 0;
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return 0;
    }

    size_t bytes = (size_t)fsize;
    char *data = (char *)malloc(bytes + 1);
    if (!data)
    {
        fclose(f);
        return 0;
    }

    size_t read_bytes = fread(data, 1, bytes, f);
    fclose(f);

    if (read_bytes != bytes)
    {
        free(data);
        return 0;
    }

    data[bytes] = '\0';
    *out_data = data;
    return 1;
}

static const char *obtener_release_label(const cJSON *tag, const cJSON *name)
{
    if (tag && cJSON_IsString(tag) && tag->valuestring)
    {
        return tag->valuestring;
    }
    if (name && cJSON_IsString(name) && name->valuestring)
    {
        return name->valuestring;
    }
    return "(sin nombre)";
}

static int extraer_asset_exe(const cJSON *assets, const char **asset_url)
{
    if (!assets || !cJSON_IsArray(assets))
    {
        return 0;
    }

    const cJSON *asset = NULL;
    cJSON_ArrayForEach(asset, assets)
    {
        const cJSON *asset_name_item = cJSON_GetObjectItem(asset, "name");
        const cJSON *url_item = cJSON_GetObjectItem(asset, "browser_download_url");
        if (!asset_name_item || !url_item || !cJSON_IsString(asset_name_item) ||
                !cJSON_IsString(url_item))
        {
            continue;
        }

        if (strstr(asset_name_item->valuestring, ".exe") != NULL)
        {
            *asset_url = url_item->valuestring;
            return 1;
        }
    }

    return 0;
}

static const char *buscar_nombre_asset(const cJSON *assets,
                                       const char *target_url)
{
    if (!assets || !cJSON_IsArray(assets) || !target_url)
    {
        return NULL;
    }

    const cJSON *asset = NULL;
    cJSON_ArrayForEach(asset, assets)
    {
        const cJSON *name_item = cJSON_GetObjectItem(asset, "name");
        const cJSON *url_item = cJSON_GetObjectItem(asset, "browser_download_url");
        if (name_item && cJSON_IsString(name_item) && url_item &&
                cJSON_IsString(url_item) &&
                strcmp(url_item->valuestring, target_url) == 0)
        {
            return name_item->valuestring;
        }
    }

    return NULL;
}

static int cargar_releases_ejecutables(cJSON *root, char *release_names[],
                                       char *asset_urls[], int max_releases)
{
    int count = 0;
    cJSON *rel = NULL;

    cJSON_ArrayForEach(rel, root)
    {
        if (count >= max_releases)
        {
            break;
        }

        const cJSON *tag = cJSON_GetObjectItem(rel, "tag_name");
        const cJSON *name = cJSON_GetObjectItem(rel, "name");
        const char *release_label = obtener_release_label(tag, name);
        const cJSON *assets = cJSON_GetObjectItem(rel, "assets");
        const char *asset_url = NULL;

        if (!extraer_asset_exe(assets, &asset_url))
        {
            continue;
        }

        release_names[count] = _strdup(release_label);
        asset_urls[count] = _strdup(asset_url);
        if (!release_names[count] || !asset_urls[count])
        {
            free(release_names[count]);
            free(asset_urls[count]);
            break;
        }
        count++;
    }

    return count;
}

static int descargar_y_ejecutar_latest(const char *owner_repo,
                                       const char *repo_name,
                                       const char *temp_path)
{
    URLDownloadToFileAFunc downloader;
    HMODULE module = NULL;
    cJSON *root = NULL;
    char dest[1024];
    char api_url[1024];
    char json_path[1024];
    int success = 0;

    if (!cargar_descargador(&downloader, &module))
    {
        printf("Error cargando modulo de descarga (urlmon.dll).\n");
        return 0;
    }

    snprintf(api_url, sizeof(api_url),
             "https://api.github.com/repos/%s/releases/latest", owner_repo);
    snprintf(json_path, sizeof(json_path), "%s%s_latest_release.json", temp_path,
             repo_name);

    printf("Obteniendo informacion de la ultima release...\n");
    root = descargar_y_parsear_release_json(downloader, api_url, json_path);
    if (!root)
    {
        printf("Error obteniendo informacion de la release.\n");
        goto cleanup;
    }

    const cJSON *assets = cJSON_GetObjectItem(root, "assets");
    const char *asset_url = NULL;
    if (!extraer_asset_exe(assets, &asset_url))
    {
        printf("No se encontro ningun asset .exe en la ultima release.\n");
        goto cleanup;
    }

    const char *asset_name = buscar_nombre_asset(assets, asset_url);
    if (asset_name && asset_name[0] != '\0')
    {
        snprintf(dest, sizeof(dest), "%s%s", temp_path, asset_name);
    }
    else
    {
        snprintf(dest, sizeof(dest), "%s%s_latest.exe", temp_path, repo_name);
    }

    printf("Descargando %s -> %s\n", asset_url, dest);
    if (!descargar_archivo(downloader, asset_url, dest))
    {
        printf("Error descargando la release.\n");
        goto cleanup;
    }

    printf("Descarga completada: %s\n", dest);
    ejecutar_instalador(dest);
    success = 1;

cleanup:
    if (root)
        cJSON_Delete(root);
    liberar_descargador(module);
    return success;
}

static int descargar_y_ejecutar_release_seleccionada(const char *owner_repo,
        const char *repo_name,
        const char *temp_path)
{
    URLDownloadToFileAFunc downloader;
    HMODULE module;
    if (!cargar_descargador(&downloader, &module))
    {
        printf("Error cargando modulo de descarga (urlmon.dll).\n");
        return 0;
    }

    char api_url[1024];
    char json_path[1024];
    snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/releases",
             owner_repo);
    snprintf(json_path, sizeof(json_path), "%s%s_releases.json", temp_path,
             repo_name);

    printf("Obteniendo lista de releases...\n");
    if (!descargar_archivo(downloader, api_url, json_path))
    {
        printf("Error descargando lista de releases.\n");
        liberar_descargador(module);
        return 0;
    }

    char *json_data = NULL;
    if (!leer_archivo_completo(json_path, &json_data))
    {
        printf("Error abriendo o leyendo archivo de releases descargado.\n");
        liberar_descargador(module);
        return 0;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root || !cJSON_IsArray(root))
    {
        printf("Respuesta invalida de GitHub API.\n");
        if (root)
        {
            cJSON_Delete(root);
        }
        liberar_descargador(module);
        return 0;
    }

    enum { SETTINGS_MAX_RELEASES = 64 };
    char *asset_urls[SETTINGS_MAX_RELEASES] = {0};
    char *release_names[SETTINGS_MAX_RELEASES] = {0};
    int count = cargar_releases_ejecutables(root, release_names, asset_urls,
                                            SETTINGS_MAX_RELEASES);
    cJSON_Delete(root);

    if (count == 0)
    {
        printf("No se encontraron assets .exe en las releases.\n");
        liberar_descargador(module);
        return 0;
    }

    printf("Selecciona la version a descargar:\n");
    for (int i = 0; i < count; i++)
    {
        printf("%d. %s\n", i + 1, release_names[i]);
    }
    printf("0. %s\n", get_text("menu_back"));

    int choice = input_int("> ");
    if (choice <= 0 || choice > count)
    {
        liberar_releases(release_names, asset_urls, count);
        liberar_descargador(module);
        return 0;
    }

    int selected_index = choice - 1;
    const char *chosen_url = asset_urls[selected_index];
    char dest[1024];
    snprintf(dest, sizeof(dest), "%s%s_selected.exe", temp_path, repo_name);

    printf("Descargando %s -> %s\n", chosen_url, dest);
    int ok = descargar_archivo(downloader, chosen_url, dest);

    liberar_releases(release_names, asset_urls, count);
    liberar_descargador(module);

    if (!ok)
    {
        printf("Error descargando el asset.\n");
        return 0;
    }

    printf("Descarga completada: %s\n", dest);
    ejecutar_instalador(dest);
    return 1;
}
#endif

static void menu_theme_settings(void)
{
    MenuItem items[] =
    {
        {1, get_text("theme_light"), &theme_set_light},
        {2, get_text("theme_dark"), &theme_set_dark},
        {3, get_text("theme_blue"), &theme_set_blue},
        {4, get_text("theme_green"), &theme_set_green},
        {5, get_text("theme_red"), &theme_set_red},
        {6, get_text("theme_purple"), &theme_set_purple},
        {7, get_text("theme_classic"), &theme_set_classic},
        {8, get_text("theme_high_contrast"), &theme_set_high_contrast},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_theme"), items,
                  (int)(sizeof(items) / sizeof(items[0])));
}

static void menu_language_settings(void)
{
    MenuItem items[] = {{1, get_text("lang_spanish"), &lang_set_spanish},
        {2, get_text("lang_english"), &lang_set_english},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_language"), items,
                  (int)(sizeof(items) / sizeof(items[0])));
}

static const char * get_current_theme_name(void)
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

static const char * get_current_text_size_name(void)
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

static void show_current_settings(void)
{
    clear_screen();
    print_header(get_text("current_settings"));

    printf("Tema: %s\n", get_current_theme_name());
    printf("Idioma: %s\n", current_settings.language == LANGUAGE_SPANISH
           ? get_text("lang_spanish")
           : get_text("lang_english"));
    printf("Tamanio de texto: %s\n", get_current_text_size_name());
    printf("Musica al iniciar: %s\n", current_settings.music_autoplay
           ? get_text("state_enabled")
           : get_text("state_disabled"));
    printf("Dashboard al iniciar: %s\n", current_settings.dashboard_enabled
           ? get_text("state_enabled")
           : get_text("state_disabled"));

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

static void menu_text_size_settings(void)
{
    MenuItem items[] = {{1, get_text("text_size_small"), &text_size_small},
        {2, get_text("text_size_medium"), &text_size_medium},
        {3, get_text("text_size_large"), &text_size_large},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_text_size"), items,
                  (int)(sizeof(items) / sizeof(items[0])));
}

static void menu_accessibility_settings(void)
{
    MenuItem items[] =
    {
        {1, get_text("settings_text_size"), &menu_text_size_settings},
        {2, get_text("settings_high_contrast"), &accessibility_high_contrast},
        {
            3, get_text("settings_accessibility_normal"),
            &accessibility_normal_theme_text
        },
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_accessibility"), items,
                  (int)(sizeof(items) / sizeof(items[0])));
}

static void reset_settings_to_defaults(void)
{
    clear_screen();
    print_header(get_text("reset_settings"));

    printf("%s\n", get_text("reset_confirm"));
    printf("(S/N): ");

    char confirm;
    char input[16];
    if (!fgets(input, sizeof(input), stdin) ||
            !extraer_primer_caracter(input, &confirm))
    {
        confirm = 'N';
    }

    if (confirm == 's' || confirm == 'S')
    {
        current_settings.theme = THEME_LIGHT;
        current_settings.language = LANGUAGE_SPANISH;
        current_settings.mode = MODE_SIMPLE;
        current_settings.text_size = TEXT_SIZE_MEDIUM;
        current_settings.music_autoplay = 1;
        current_settings.music_volume = SETTINGS_MUSIC_VOLUME_DEFAULT;
        current_settings.music_repeat_mode = SETTINGS_MUSIC_REPEAT_DEFAULT;
        current_settings.music_eq_enabled = SETTINGS_MUSIC_EQ_ENABLED_DEFAULT;
        current_settings.music_eq_bass_db = SETTINGS_MUSIC_EQ_DB_DEFAULT;
        current_settings.music_eq_mid_db = SETTINGS_MUSIC_EQ_DB_DEFAULT;
        current_settings.music_eq_treble_db = SETTINGS_MUSIC_EQ_DB_DEFAULT;
        current_settings.music_volume_step = SETTINGS_MUSIC_VOLUME_STEP_DEFAULT;
        current_settings.dashboard_enabled = 1;
        settings_apply_theme();
        settings_apply_text_size();
        settings_save();

        // Limpiar nombre de usuario tambien
        sqlite3_stmt *stmt;
        const char *sql = "DELETE FROM usuario;";
        if (db_prepare_stmt_with_error(&stmt, sql,
                                       "Error al preparar la consulta"))
        {
            sqlite3_step(stmt);
            db_stmt_release(stmt);
        }

        printf("%s\n", get_text("reset_success"));
    }
    else
    {
        printf("%s\n", get_text("reset_cancelled"));
    }

    pause_console();
}

int is_custom_menu_enabled(const char *menu_name)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT enabled FROM custom_menus WHERE menu_name = ?;";
    int enabled = 0;

    if (db_prepare_stmt_with_error(&stmt, sql, "Error al preparar la consulta"))
    {
        sqlite3_bind_text(stmt, 1, menu_name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            enabled = sqlite3_column_int(stmt, 0);
        }
        db_stmt_release(stmt);
    }

    return enabled;
}

void set_custom_menu_enabled(const char *menu_name, int enabled)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT OR REPLACE INTO custom_menus (menu_name, enabled) VALUES (?, ?);";

    if (db_prepare_stmt_with_error(&stmt, sql, "Error al preparar la consulta"))
    {
        sqlite3_bind_text(stmt, 1, menu_name, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, enabled);
        sqlite3_step(stmt);
        db_stmt_release(stmt);
    }
}

static void menu_mode_settings(void)
{
    MenuItem items[] = {{1, get_text("mode_simple"), &mode_set_simple},
        {2, get_text("mode_advanced"), &mode_set_advanced},
        {3, get_text("mode_custom"), &mode_set_custom},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_mode"), items,
                  (int)(sizeof(items) / sizeof(items[0])));
}

void menu_custom_menus(void)
{
#if defined(UNIT_TEST)
    return;
#else
    clear_screen();
    print_header(get_text("menu_settings"));

    printf("Selecciona los menus que deseas habilitar/deshabilitar:\n\n");

    // Lista de menus disponibles (excluyendo exit que siempre esta)
    struct MenuOption options[] =
    {
        {1, "camisetas", get_text("menu_camisetas")},
        {2, "canchas", get_text("menu_canchas")},
        {3, "partidos", get_text("menu_partidos")},
        {4, "equipos", get_text("menu_equipos")},
        {5, "estadisticas", get_text("menu_estadisticas")},
        {6, "logros", get_text("menu_logros")},
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
        printf("%d. [%s] %s\n", options[j].opcion, enabled ? "X" : " ",
               options[j].display_name);
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
        menu_custom_menus(); // Recargar menu
        return;
    }

    int current_state = is_custom_menu_enabled(option->name);
    set_custom_menu_enabled(option->name, !current_state);
    printf("Menu %s %s.\n", option->display_name,
           !current_state ? "habilitado" : "deshabilitado");
    pause_console();
    menu_custom_menus(); // Recargar menu
#endif
}

void menu_update(void)
{
#if defined(UNIT_TEST)
    return;
#else
#ifdef _WIN32
    clear_screen();
    print_header(get_text("menu_update"));
    const char *owner_repo = UPDATE_REPO; // formato owner/repo
    char repo_name[128] = {0};
    obtener_nombre_repo(owner_repo, repo_name, sizeof(repo_name));

    // Verificar version actual contra la ultima version en GitHub.
    char temp_path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_path) == 0)
    {
        printf("Error obteniendo carpeta temporal.\n");
        pause_console();
        return;
    }

    char *latest_tag =
        obtener_latest_release_tag(owner_repo, repo_name, temp_path);
    if (latest_tag)
    {
        int cmp = comparar_versiones(APP_VERSION, latest_tag);
        if (cmp >= 0)
        {
            printf("Ya tienes la ultima version (%s).\n", APP_VERSION);
            free(latest_tag);
            pause_console();
            return;
        }

        printf("Nueva version disponible: %s (actual: %s).\n", latest_tag,
               APP_VERSION);
        free(latest_tag);
    }

    // Opciones al usuario
    printf("1. %s\n", "Ultima version (latest)");
    printf("2. %s\n", "Elegir una version de la lista");
    printf("0. %s\n", get_text("menu_back"));

    int modo = input_int("> ");
    if (modo == 0)
    {
        return;
    }

    if (modo == 1)
    {
        descargar_y_ejecutar_latest(owner_repo, repo_name, temp_path);
        pause_console();
        return;
    }

    descargar_y_ejecutar_release_seleccionada(owner_repo, repo_name, temp_path);
    pause_console();
#else
    printf("Actualizar solo esta soportado en Windows.\n");
    pause_console();
#endif
#endif
}

void menu_settings(void)
{
    char label_tema[96];
    char label_idioma[96];
    char label_accesibilidad[96];
    char label_usuario[96];
    char label_actual[96];
    char label_reset[96];
    char label_modo[96];
    char label_exportar[96];
    char label_importar[96];
    char label_busqueda[96];
    char label_actualizar[96];

    snprintf(label_tema, sizeof(label_tema), "%s", get_text("settings_theme"));
    snprintf(label_idioma, sizeof(label_idioma), "%s",
             get_text("settings_language"));
    snprintf(label_accesibilidad, sizeof(label_accesibilidad), "%s",
             get_text("settings_accessibility"));
    snprintf(label_usuario, sizeof(label_usuario), "%s",
             get_text("menu_usuario"));
    snprintf(label_actual, sizeof(label_actual), "%s", get_text("show_current"));
    snprintf(label_reset, sizeof(label_reset), "%s", get_text("reset_defaults"));
    snprintf(label_modo, sizeof(label_modo), "%s", get_text("settings_mode"));
    snprintf(label_exportar, sizeof(label_exportar), "%s",
             get_text("menu_exportar"));
    snprintf(label_importar, sizeof(label_importar), "%s",
             get_text("menu_importar"));
    snprintf(label_busqueda, sizeof(label_busqueda), "Busqueda Global");
    snprintf(label_actualizar, sizeof(label_actualizar), "%s",
             get_text("menu_update"));

    AppSettings const *cfg = settings_get();
    settings_actualizar_label_toggle("settings_music_autoplay",
                                     cfg->music_autoplay, label_music_autoplay,
                                     sizeof(label_music_autoplay));
    settings_actualizar_label_toggle(
        "settings_dashboard_enabled", cfg->dashboard_enabled,
        label_dashboard_enabled, sizeof(label_dashboard_enabled));

    MenuItem items[] =
    {
        {1, label_tema, menu_theme_settings},
        {2, label_idioma, menu_language_settings},
        {3, label_accesibilidad, menu_accessibility_settings},
        {4, label_usuario, menu_usuario},
        {5, label_actual, show_current_settings},
        {6, label_reset, reset_settings_to_defaults},
        {7, label_modo, menu_mode_settings},
        {8, label_exportar, menu_exportar},
        {9, label_importar, menu_importar},
        {10, "Exportar a ODS", &abrir_export_ods_desde_settings},
        {11, label_busqueda, &abrir_busqueda_global_desde_settings},
        {12, label_actualizar, menu_update},
        {13, label_music_autoplay, toggle_music_autoplay_setting},
        {14, label_dashboard_enabled, toggle_dashboard_enabled_setting},
        {15, "Backup & Restore", &abrir_backup_desde_settings},
        {16, "Integridad BD", &abrir_integridad_desde_settings},
        {17, "Deshacer (Undo)", &abrir_undo_desde_settings},
        {18, "Notificaciones", &abrir_notificaciones_desde_settings},
        {0, get_text("menu_back"), NULL}
    };

    const int item_count = (int)(sizeof(items) / sizeof(items[0]));

    ejecutar_menu(get_text("menu_settings"), items, item_count);
}

void verificar_actualizacion_disponible(int mostrar_mensaje)
{
#if defined(UNIT_TEST)
    return;
#else
#ifdef _WIN32
    const char *owner_repo = UPDATE_REPO;
    char repo_name[128] = {0};
    obtener_nombre_repo(owner_repo, repo_name, sizeof(repo_name));

    char temp_path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_path) == 0)
    {
        return;
    }

    char *latest_tag =
        obtener_latest_release_tag(owner_repo, repo_name, temp_path);
    if (!latest_tag)
    {
        if (mostrar_mensaje)
        {
            printf("No se pudo verificar actualizaciones.\n");
        }
        return;
    }

    int cmp = comparar_versiones(APP_VERSION, latest_tag);

    if (cmp < 0)
    {
        // Hay una nueva versión disponible
        printf("\n");
        printf(
            "╔══════════════════════════════════════════════════════════════╗\n");
        printf(
            "║                                                              ║\n");
        printf("║           ¡NUEVA VERSION DISPONIBLE!                        ║\n");
        printf(
            "║                                                              ║\n");
        printf("║  Version actual:  %-10s                             ║\n",
               APP_VERSION);
        printf("║  Nueva version:   %-10s                             ║\n",
               latest_tag);
        printf(
            "║                                                              ║\n");
        printf("║  Para actualizar, ve a Ajustes > Actualizar                 ║\n");
        printf(
            "║                                                              ║\n");
        printf(
            "╚══════════════════════════════════════════════════════════════╝\n");
        printf("\n");
        pause_console();
    }
    else if (mostrar_mensaje)
    {
        if (cmp > 0)
        {
            printf("Estas usando una version de desarrollo (%s).\n", APP_VERSION);
        }
        else
        {
            printf("✓ Estas usando la ultima version (%s).\n", APP_VERSION);
        }
        pause_console();
    }

    free(latest_tag);
#else
    if (mostrar_mensaje)
    {
        printf("La verificación automatica solo esta disponible en Windows.\n");
        pause_console();
    }
#endif
#endif
}
