#include "lang.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

#define LANG_DIR "langs"
#define LANG_MAX_VAL 512

typedef struct
{
    char key[128];
    char val[LANG_MAX_VAL];
} LangPair;

static LangPair s_pairs[LANG_MAX_KEYS];
static int s_count = 0;
static char s_current[LANG_CODE_SIZE] = "en";

static int load_file(const char *path)
{
    FILE *f = NULL;
#ifdef _WIN32
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return 0;
#else
    f = fopen(path, "rb");
    if (!f)
        return 0;
#endif

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0)
    {
        fclose(f);
        return 0;
    }

    char *data = (char *)malloc((size_t)(len + 1));
    if (!data)
    {
        fclose(f);
        return 0;
    }

    size_t nread = fread(data, 1, (size_t)len, f);
    fclose(f);
    if ((long)nread != len)
    {
        free(data);
        return 0;
    }
    data[len] = '\0';

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) return 0;

    cJSON *strings = cJSON_GetObjectItem(root, "strings");
    if (!strings || !cJSON_IsObject(strings))
    {
        cJSON_Delete(root);
        return 0;
    }

    s_count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, strings)
    {
        if (s_count >= LANG_MAX_KEYS)
            break;
        if (cJSON_IsString(item) && item->valuestring && item->string)
        {
            strncpy_s(s_pairs[s_count].key, sizeof(s_pairs[s_count].key),
                      item->string, _TRUNCATE);
            strncpy_s(s_pairs[s_count].val, sizeof(s_pairs[s_count].val),
                      item->valuestring, _TRUNCATE);
            s_count++;
        }
    }

    cJSON_Delete(root);
    return 1;
}

static int try_load(const char *code)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.json", LANG_DIR, code);
    if (load_file(path))
        return 1;

#ifdef _WIN32
    char mod_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, mod_path, sizeof(mod_path)))
    {
        char *p = strrchr(mod_path, '\\');
        if (p) *p = '\0';
        snprintf(path, sizeof(path), "%s\\%s\\%s.json", mod_path, LANG_DIR, code);
        if (load_file(path))
            return 1;
    }
#else
    char self[4096] = {0};
    ssize_t r = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (r > 0)
    {
        self[r] = '\0';
        char *p = strrchr(self, '/');
        if (p) *p = '\0';
        snprintf(path, sizeof(path), "%s/%s/%s.json", self, LANG_DIR, code);
        if (load_file(path))
            return 1;
    }
#endif

    return 0;
}

static void load_fallback(void)
{
    static const struct
    {
        const char *key;
        const char *val;
    } fb_pairs[] =
    {
        {"menu_title", "MI FUTBOL C"},
        {"menu_dashboard", "Dashboard"},
        {"menu_calendario", "Calendar"},
        {"menu_camisetas", "Shirts"},
        {"menu_canchas", "Fields"},
        {"menu_partidos", "Matches"},
        {"menu_equipos", "Teams"},
        {"menu_estadisticas", "Statistics"},
        {"menu_logros", "Achievements"},
        {"menu_analisis", "Analysis"},
        {"menu_bienestar", "Wellness"},
        {"menu_lesiones", "Injuries"},
        {"menu_financiamiento", "Financing"},
        {"menu_exportar", "Export"},
        {"menu_importar", "Import"},
        {"menu_usuario", "User"},
        {"menu_torneos", "Tournaments"},
        {"menu_temporada", "Season"},
        {"menu_settings", "Settings"},
        {"menu_exit", "Exit"},
        {"menu_carrera", "Career"},
        {"menu_recordatorios", "Reminders"},
        {"menu_colecciones", "Collections"},
        {"menu_musica", "Music"},
        {"menu_records_rankings", "Records & Rankings"},
        {"menu_tiendas", "Tiendas"},
        {"menu_back", "Back"},
        {"settings_theme", "Interface Theme"},
        {"settings_language", "Language"},
        {"settings_saved", "Settings saved successfully."},
        {"invalid_option", "Invalid option."},
        {"press_enter", "Press Enter to continue..."},
        {"welcome_back", "Welcome Back"},
        {"lang_spanish", "Spanish"},
        {"lang_english", "English"},
        {"settings_mode", "Mode"},
        {"mode_simple", "Simple"},
        {"mode_advanced", "Advanced"},
        {"mode_custom", "Custom"},
        {"theme_light", "Light"},
        {"theme_dark", "Dark"},
        {"theme_blue", "Blue"},
        {"theme_green", "Green"},
        {"theme_red", "Red"},
        {"theme_purple", "Purple"},
        {"theme_classic", "Classic"},
        {"theme_high_contrast", "High Contrast"},
        {"text_size_small", "Small"},
        {"text_size_medium", "Medium"},
        {"text_size_large", "Large"},
        {"settings_music_autoplay", "Music on startup"},
        {"settings_dashboard_enabled", "Dashboard on startup"},
        {"state_enabled", "Enabled"},
        {"state_disabled", "Disabled"},
        {"state_on", "On"},
        {"state_off", "Off"},
        {"show_current", "Show Current Settings"},
        {"reset_defaults", "Reset to Default Values"},
        {"current_settings", "Current Settings"},
        {"reset_settings", "Reset Settings"},
        {NULL, NULL}
    };

    for (int i = 0; fb_pairs[i].key != NULL && s_count < LANG_MAX_KEYS; i++)
    {
        strncpy_s(s_pairs[s_count].key, sizeof(s_pairs[s_count].key),
                  fb_pairs[i].key, _TRUNCATE);
        strncpy_s(s_pairs[s_count].val, sizeof(s_pairs[s_count].val),
                  fb_pairs[i].val, _TRUNCATE);
        s_count++;
    }
}

int lang_init(void)
{
    s_count = 0;
    strncpy_s(s_current, sizeof(s_current), "en", _TRUNCATE);

    if (try_load("es"))
    {
        strncpy_s(s_current, sizeof(s_current), "es", _TRUNCATE);
        return 1;
    }
    if (try_load("en"))
    {
        strncpy_s(s_current, sizeof(s_current), "en", _TRUNCATE);
        return 1;
    }

    load_fallback();
    return 0;
}

void lang_set(const char *lang_code)
{
    if (!lang_code || lang_code[0] == '\0')
        return;

    LangPair backup[LANG_MAX_KEYS];
    int backup_count = s_count;
    memcpy(backup, s_pairs, (size_t)s_count * sizeof(LangPair));

    s_count = 0;
    if (!try_load(lang_code))
    {
        memcpy(s_pairs, backup, (size_t)backup_count * sizeof(LangPair));
        s_count = backup_count;
        return;
    }

    strncpy_s(s_current, sizeof(s_current), lang_code, _TRUNCATE);
}

const char *lang_get_current(void)
{
    return s_current;
}

const char *tr(const char *key)
{
    if (!key)
        return "";

    for (int i = 0; i < s_count; i++)
    {
        if (strcmp(s_pairs[i].key, key) == 0)
            return s_pairs[i].val;
    }
    return key;
}

void lang_cleanup(void)
{
    s_count = 0;
}
