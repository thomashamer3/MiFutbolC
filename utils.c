#include "utils.h"
#include "ascii_art.h"
#include "atajos.h"
#include "cJSON.h"
#include "db.h"
#include "export.h"
#include "export_partidos_helpers.h"
#include "menu.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <conio.h>
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#ifndef _WIN32
extern char **environ;
#endif

#include "random_utils.h"

#ifdef _WIN32
/* Funcion deshabilitada: evitar maximizar la consola automaticamente
 * Se deja como no-op para que el usuario pueda cerrar/reducir la ventana.
 */
void ensure_console_maximized_windows(void) { /* Intencionalmente vacio */ }
#else
void ensure_console_maximized_windows(void) { /* No-op en otros sistemas */ }
#endif

#define STMT_CACHE_SIZE 128

typedef struct stmt_cache_entry
{
    char *sql;
    sqlite3_stmt *stmt;
    int in_use;
} stmt_cache_entry;

static stmt_cache_entry g_stmt_cache[STMT_CACHE_SIZE];
static int g_stmt_cache_initialized = 0;

static uint64_t cache_hash_sql(const char *sql)
{
    uint64_t h = 14695981039346656037ULL;
    if (!sql)
        return h;
    while (*sql)
    {
        h ^= (unsigned char)*sql++;
        h *= 1099511628211ULL;
    }
    return h;
}

int db_prepare_stmt(sqlite3_stmt **stmt, const char *sql)
{
    if (!stmt || !sql)
    {
        return 0;
    }
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    return 1;
}

sqlite3_stmt *db_prepare_cached(const char *sql)
{
    if (!sql)
        return NULL;

    if (!g_stmt_cache_initialized)
    {
        for (int i = 0; i < STMT_CACHE_SIZE; i++)
        {
            g_stmt_cache[i].sql = NULL;
            g_stmt_cache[i].stmt = NULL;
            g_stmt_cache[i].in_use = 0;
        }
        g_stmt_cache_initialized = 1;
    }

    uint64_t h = cache_hash_sql(sql);
    int slot = (int)(h % (uint64_t)STMT_CACHE_SIZE);
    int start = slot;

    do
    {
        if (g_stmt_cache[slot].sql == NULL)
        {
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
            {
                return NULL;
            }
            g_stmt_cache[slot].sql = sqlite3_mprintf("%s", sql);
            g_stmt_cache[slot].stmt = stmt;
            g_stmt_cache[slot].in_use = 1;
            return stmt;
        }
        if (strcmp(g_stmt_cache[slot].sql, sql) == 0)
        {
            sqlite3_reset(g_stmt_cache[slot].stmt);
            sqlite3_clear_bindings(g_stmt_cache[slot].stmt);
            g_stmt_cache[slot].in_use = 1;
            return g_stmt_cache[slot].stmt;
        }
        slot = (slot + 1) % STMT_CACHE_SIZE;
    }
    while (slot != start);

    return NULL;
}

void db_stmt_release(sqlite3_stmt *stmt)
{
    if (!stmt)
        return;
    for (int i = 0; i < STMT_CACHE_SIZE; i++)
    {
        if (g_stmt_cache[i].stmt == stmt && g_stmt_cache[i].sql != NULL)
        {
            g_stmt_cache[i].in_use = 0;
            return;
        }
    }
    sqlite3_finalize(stmt);
}

void db_clear_stmt_cache(void)
{
    for (int i = 0; i < STMT_CACHE_SIZE; i++)
    {
        if (g_stmt_cache[i].stmt)
        {
            sqlite3_finalize(g_stmt_cache[i].stmt);
            g_stmt_cache[i].stmt = NULL;
        }
        if (g_stmt_cache[i].sql)
        {
            sqlite3_free(g_stmt_cache[i].sql);
            g_stmt_cache[i].sql = NULL;
        }
        g_stmt_cache[i].in_use = 0;
    }
    g_stmt_cache_initialized = 0;
}

int db_prepare_stmt_with_error(sqlite3_stmt **stmt, const char *sql,
                               const char *error_message)
{
    if (db_prepare_stmt(stmt, sql))
    {
        return 1;
    }

    if (error_message)
    {
        printf("%s: %s\n", error_message, sqlite3_errmsg(db));
    }
    return 0;
}

float utils_clamp_float(float value, float minv, float maxv)
{
    if (value < minv)
    {
        return minv;
    }
    if (value > maxv)
    {
        return maxv;
    }
    return value;
}

int utils_clamp_int(int value, int minv, int maxv)
{
    if (value < minv)
    {
        return minv;
    }
    if (value > maxv)
    {
        return maxv;
    }
    return value;
}

static uint64_t auth_fnv1a64_update(uint64_t hash, const unsigned char *data,
                                    size_t len)
{
    const uint64_t prime = 1099511628211ULL;
    for (size_t i = 0; i < len; i++)
    {
        hash ^= (uint64_t)data[i];
        hash *= prime;
    }
    return hash;
}

static uint64_t auth_fnv1a64_string(const char *text)
{
    const uint64_t offset_basis = 14695981039346656037ULL;
    if (!text)
    {
        return offset_basis;
    }
    return auth_fnv1a64_update(offset_basis, (const unsigned char *)text,
                               strlen_s(text, SIZE_MAX));
}

void auth_generate_salt_hex(char *salt_out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char salt[16];

    if (!salt_out || out_size < 33)
    {
        return;
    }

    if (secure_random_bytes(salt, 16) != 0)
    {
        for (int i = 0; i < 16; i++)
        {
            salt[i] = (unsigned char)((clock() ^ ((time(NULL)) + i * 12345)) & 0xFF);
        }
    }

    for (int i = 0; i < 16; i++)
    {
        salt_out[i * 2] = hex[(salt[i] >> 4) & 0x0F];
        salt_out[i * 2 + 1] = hex[salt[i] & 0x0F];
    }
    salt_out[32] = '\0';
}

void auth_build_password_hash(const char *plain_password, const char *salt_hex,
                              char *hash_out, size_t out_size)
{
    char round_input[512];
    uint64_t h1;
    uint64_t h2;

    if (!plain_password || !salt_hex || !hash_out || out_size < 17)
    {
        if (hash_out && out_size > 0)
        {
            hash_out[0] = '\0';
        }
        return;
    }

    snprintf(round_input, sizeof(round_input), "%s:%s", salt_hex, plain_password);
    h1 = auth_fnv1a64_string(round_input);
    snprintf(round_input, sizeof(round_input), "%s:%016" PRIx64 ":%s",
             plain_password, h1, salt_hex);
    h2 = auth_fnv1a64_string(round_input);
    snprintf(hash_out, out_size, "%016" PRIx64, h2);
}

static int auth_username_exists(sqlite3 *auth_db, const char *username);
static int auth_upsert_user(sqlite3 *auth_db, const char *username,
                            const char *plain_password);

int utils_get_env_var_copy(const char *name, char *buffer, size_t size)
{
    if (!name || !buffer || size == 0)
    {
        return 0;
    }

#if defined(_WIN32) && defined(_MSC_VER)
    char *value = NULL;
    size_t value_len = 0;
    if (_dupenv_s(&value, &value_len, name) != 0 || !value || value_len == 0)
    {
        if (value)
        {
            free(value);
        }
        buffer[0] = '\0';
        return 0;
    }

    strncpy_s(buffer, size, value, _TRUNCATE);
    free(value);
    return buffer[0] != '\0';
#else
    const char *value = getenv(name);
    if (!value || value[0] == '\0')
    {
        buffer[0] = '\0';
        return 0;
    }

    strncpy_s(buffer, size, value, _TRUNCATE);
    return buffer[0] != '\0';
#endif
}

static int app_build_local_appdata_path(char *dest, size_t size,
                                        const char *suffix)
{
#ifdef _WIN32
    char local_app_data[1024] = {0};
    if (!dest || size == 0 || !suffix)
    {
        return 0;
    }

    if (!utils_get_env_var_copy("LOCALAPPDATA", local_app_data,
                                sizeof(local_app_data)))
    {
        return 0;
    }

    if (strcpy_s(dest, size, local_app_data) != 0)
    {
        return 0;
    }
    if (strcat_s(dest, size, "\\MiFutbolC") != 0)
    {
        return 0;
    }

    if (suffix[0] != '\0')
    {
        if (strcat_s(dest, size, "\\") != 0)
        {
            return 0;
        }
        if (strcat_s(dest, size, suffix) != 0)
        {
            return 0;
        }
    }
    return 1;
#else
    (void)dest;
    (void)size;
    (void)suffix;
    return 0;
#endif
}

static void auth_get_db_path(char *path, size_t size)
{
#ifdef _WIN32
    char local_app_data[1024] = {0};
    if (utils_get_env_var_copy("LOCALAPPDATA", local_app_data,
                               sizeof(local_app_data)) &&
            strcpy_s(path, size, local_app_data) == 0 &&
            strcat_s(path, size, "\\MiFutbolC\\data\\users.db") == 0)
    {
        return;
    }
#endif
    strcpy_s(path, size, "./data/users.db");
}

static void build_user_file_path(char *dest, size_t size, const char *base_dir,
                                 const char *username, const char *ext)
{
    char filename[128];
    snprintf(filename, sizeof(filename), "mifutbol_%s%s", username, ext);
    app_build_path(dest, size, base_dir, filename);
}

static void auth_get_user_data_paths(const char *username, char *db_path,
                                     size_t db_size, char *log_path,
                                     size_t log_size)
{
    const char *safe_username = username ? username : "";
    char data_dir[1024] = {0};
    int found = 0;

#ifdef _WIN32
    found = app_build_local_appdata_path(data_dir, sizeof(data_dir), "data");
#endif
    if (!found)
    {
        strcpy_s(data_dir, sizeof(data_dir), "./data");
        found = 1;
    }

    if (found)
    {
        build_user_file_path(db_path, db_size, data_dir, safe_username, ".db");
        build_user_file_path(log_path, log_size, data_dir, safe_username, ".log");
        return;
    }

    db_path[0] = '\0';
    log_path[0] = '\0';
}

static int auth_ensure_parent_dirs(void)
{
#ifdef _WIN32
    char base_dir[1024];
    char data_dir[1024];

    if (app_build_local_appdata_path(base_dir, sizeof(base_dir), "") &&
            app_build_local_appdata_path(data_dir, sizeof(data_dir), "data"))
    {
        MKDIR(base_dir);
        MKDIR(data_dir);
        return 1;
    }
#endif
    MKDIR("./data");
    return 1;
}

static int auth_username_valido(const char *username)
{
    size_t len;

    if (!username)
    {
        return 0;
    }

    len = safe_strnlen(username, 128);
    if (len < 3 || len > 32)
    {
        return 0;
    }

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)username[i];
        if (!(isalnum(c) || c == '_' || c == '-'))
        {
            return 0;
        }
    }

    return 1;
}

static int auth_open(sqlite3 **out_db)
{
    char path[1024];
    const char *schema = "CREATE TABLE IF NOT EXISTS local_users ("
                         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                         " username TEXT NOT NULL UNIQUE,"
                         " password_salt TEXT DEFAULT '',"
                         " password_hash TEXT DEFAULT '',"
                         " created_at TEXT DEFAULT CURRENT_TIMESTAMP);";

    if (!out_db)
    {
        return 0;
    }

    auth_ensure_parent_dirs();
    auth_get_db_path(path, sizeof(path));

    if (sqlite3_open(path, out_db) != SQLITE_OK)
    {
        return 0;
    }

    if (sqlite3_exec(*out_db, schema, NULL, NULL, NULL) != SQLITE_OK)
    {
        sqlite3_close(*out_db);
        *out_db = NULL;
        return 0;
    }

    return 1;
}

static int auth_user_count(sqlite3 *auth_db)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if (!auth_db)
    {
        return 0;
    }

    if (sqlite3_prepare_v2(auth_db, "SELECT COUNT(*) FROM local_users;", -1,
                           &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        db_stmt_release(stmt);
    }
    return count;
}

static int auth_file_exists(const char *path)
{
    FILE *f = NULL;
#ifdef _WIN32
    if (fopen_s(&f, path, "rb") != 0 || !f)
    {
        return 0;
    }
#else
    f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
#endif
    fclose(f);
    return 1;
}

static void auth_normalizar_username_legado(const char *input, char *output,
        size_t out_size)
{
    size_t j = 0;

    if (!output || out_size == 0)
    {
        return;
    }

    output[0] = '\0';
    if (!input)
    {
        return;
    }

    const char *p = input;
    while (*p != '\0' && j + 1 < out_size)
    {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '_' || c == '-')
        {
            output[j++] = (char)c;
        }
        else
        {
            output[j++] = '_';
        }
        p++;
    }
    output[j] = '\0';

    if (j < 3)
    {
        strncpy_s(output, out_size, "usuario", out_size - 1);
    }
    if (j > 32)
    {
        output[32] = '\0';
    }
}

static int auth_legacy_open_if_exists(const char *legacy_db_path,
                                      sqlite3 **legacy_db)
{
    if (!legacy_db || !legacy_db_path || !auth_file_exists(legacy_db_path))
    {
        return 0;
    }

    if (sqlite3_open(legacy_db_path, legacy_db) != SQLITE_OK)
    {
        if (*legacy_db)
        {
            sqlite3_close(*legacy_db);
            *legacy_db = NULL;
        }
        return 0;
    }

    return 1;
}

static int auth_legacy_load_user_with_password(sqlite3 *legacy_db,
        char *username_raw,
        size_t username_size, char *salt,
        size_t salt_size, char *hash,
        size_t hash_size)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if (sqlite3_prepare_v2(legacy_db,
                           "SELECT nombre, password_salt, password_hash FROM "
                           "usuario WHERE id = 1 LIMIT 1;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            const char *salt_db = (const char *)sqlite3_column_text(stmt, 1);
            const char *hash_db = (const char *)sqlite3_column_text(stmt, 2);
            strncpy_s(username_raw, username_size, nombre ? nombre : "",
                      username_size - 1);
            strncpy_s(salt, salt_size, salt_db ? salt_db : "", salt_size - 1);
            strncpy_s(hash, hash_size, hash_db ? hash_db : "", hash_size - 1);
            found = 1;
        }
        db_stmt_release(stmt);
    }

    return found;
}

static int auth_legacy_load_fallback_user(sqlite3 *legacy_db,
        char *username_raw,
        size_t username_size)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if (sqlite3_prepare_v2(legacy_db, "SELECT nombre FROM usuario LIMIT 1;", -1,
                           &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            strncpy_s(username_raw, username_size, nombre ? nombre : "",
                      username_size - 1);
            found = 1;
        }
        db_stmt_release(stmt);
    }

    return found;
}

static int auth_insert_legacy_user(sqlite3 *auth_db, const char *username_norm,
                                   const char *salt, const char *hash)
{
    int inserted = 0;

    if (salt[0] != '\0' && hash[0] != '\0')
    {
        sqlite3_stmt *ins = NULL;
        if (sqlite3_prepare_v2(auth_db,
                               "INSERT INTO local_users(username, password_salt, "
                               "password_hash) VALUES(?, ?, ?);",
                               -1, &ins, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(ins, 1, username_norm, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins, 2, salt, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins, 3, hash, -1, SQLITE_STATIC);
            inserted = (sqlite3_step(ins) == SQLITE_DONE);
            db_stmt_release(ins);
        }
    }
    else
    {
        inserted = auth_upsert_user(auth_db, username_norm, "");
    }

    return inserted;
}

static int auth_import_legacy_from_db(sqlite3 *auth_db,
                                      const char *legacy_db_path)
{
    sqlite3 *legacy_db = NULL;
    char username_raw[128] = "";
    char username_norm[64] = "";
    char salt[64] = "";
    char hash[64] = "";
    int inserted = 0;

    if (!auth_legacy_open_if_exists(legacy_db_path, &legacy_db))
    {
        return 0;
    }

    auth_legacy_load_user_with_password(legacy_db, username_raw,
                                        sizeof(username_raw), salt, sizeof(salt),
                                        hash, sizeof(hash));

    if (username_raw[0] == '\0')
    {
        auth_legacy_load_fallback_user(legacy_db, username_raw,
                                       sizeof(username_raw));
    }

    sqlite3_close(legacy_db);

    if (username_raw[0] == '\0')
    {
        return 0;
    }

    auth_normalizar_username_legado(username_raw, username_norm,
                                    sizeof(username_norm));
    if (!auth_username_valido(username_norm) ||
            auth_username_exists(auth_db, username_norm))
    {
        return 0;
    }

    inserted = auth_insert_legacy_user(auth_db, username_norm, salt, hash);

    if (inserted)
    {
        ui_printf("Usuario legado detectado e importado: %s\n", username_norm);
    }

    return inserted;
}

static int auth_importar_usuario_legado_si_existe(sqlite3 *auth_db)
{
    char legacy1[1024];
    char legacy2[1024];
#ifdef _WIN32
    if (app_build_local_appdata_path(legacy1, sizeof(legacy1),
                                     "data\\mifutbol.db"))
    {
        /* ruta lista */
    }
    else
    {
        legacy1[0] = '\0';
    }
#else
    legacy1[0] = '\0';
#endif
    snprintf(legacy2, sizeof(legacy2), "./data/mifutbol.db");

    if (legacy1[0] != '\0' && auth_import_legacy_from_db(auth_db, legacy1))
    {
        return 1;
    }

    if (auth_import_legacy_from_db(auth_db, legacy2))
    {
        return 1;
    }

    return 0;
}

static void uppercase_ascii(const char *src, char *dst, size_t size)
{
    size_t i = 0;
    if (!dst || size == 0)
    {
        return;
    }
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < size)
    {
        unsigned char ch = (unsigned char)src[i];
        dst[i] = (char)toupper(ch);
        i++;
    }
    dst[i] = '\0';
}

int ui_printf(const char *fmt, ...) // NOSONAR
{
    va_list args;
    va_start(args, fmt);
    char *formatted = sqlite3_vmprintf(fmt, args);
    va_end(args);

    if (!formatted)
    {
        return -1;
    }
    int rc = fprintf(stdout, "%s", formatted);
    sqlite3_free(formatted);
    return rc;
}

int ui_puts(const char *s)
{
    const char *text = s;
    if (!text)
    {
        text = "";
    }
    if (fputs(text, stdout) < 0)
    {
        return EOF;
    }
    if (fputc('\n', stdout) == EOF)
    {
        return EOF;
    }
    return 0;
}

int ui_putchar(int c)
{
    return fputc(c, stdout);
}

int ui_printf_centered_line(const char *fmt, ...) // NOSONAR
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    char *formatted = sqlite3_vmprintf(fmt, args);
    va_end(args);

    if (!formatted)
    {
        return -1;
    }

    snprintf(buffer, sizeof(buffer), "%s", formatted);
    sqlite3_free(formatted);

    return fprintf(stdout, "%s\n", buffer);
}

int ui_print_stats_row_from_stmt(sqlite3_stmt *stmt, const char *sep)
{
    if (!stmt || !sep)
    {
        return 0;
    }

    int id = sqlite3_column_int(stmt, 0);
    const char *nom = (const char *)sqlite3_column_text(stmt, 1);
    int partidos = sqlite3_column_int(stmt, 3);
    int goles = sqlite3_column_int(stmt, 4);
    int asistencias = sqlite3_column_int(stmt, 5);
    int victorias = sqlite3_column_int(stmt, 6);
    int empates = sqlite3_column_int(stmt, 7);
    int derrotas = sqlite3_column_int(stmt, 8);

    ui_printf_centered_line(
        "%2d - %-24s%sPartidos: %2d%sGoles: %2d%sAsistencias: %2d%sVictorias: "
        "%2d%sEmpates: %2d%sDerrotas: %2d",
        id, nom ? nom : "(sin nombre)", sep, partidos, sep, goles, sep,
        asistencias, sep, victorias, sep, empates, sep, derrotas);
    return 1;
}

static int ui_readline(char *buffer, int size)
{
    if (!buffer || size <= 0)
    {
        return 0;
    }

    return fgets(buffer, size, stdin) != NULL;
}

/**
 * Permite la entrada de valores numericos por parte del usuario,
 * facilitando la configuracion de parametros enteros en el sistema.
 */
int input_int(const char *msg)
{
    char buffer[64];
    int v = 0;
    int attempts = 0;
    int eof_count = 0;

    while (attempts < 5)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, sizeof(buffer)))
        {
            eof_count++;
            if (eof_count >= 2)
            {
                ui_printf("Entrada cerrada (EOF).\n");
                return -1;
            }
            attempts++;
            continue;
        }

        size_t len = strlen_s(buffer, sizeof(buffer));
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        {
            buffer[--len] = '\0';
        }

        if (len == 0)
        {
            ui_printf("Entrada vacia. Intente nuevamente.\n");
            continue;
        }

        char extra = '\0';
#if defined(_WIN32) && defined(_MSC_VER)
        if (sscanf_s(buffer, "%d %c", &v, &extra, 1) == 1)
#else
        if (sscanf(buffer, "%d %c", &v, &extra) == 1)
#endif
            return v;

        ui_printf("Entrada invalida. Intente nuevamente.\n");
        attempts++;
    }

    ui_printf("Se alcanzo el maximo de intentos.\n");
    return -1;
}

/* Implementacion portable de safe_strnlen */
size_t safe_strnlen(const char *s, size_t maxlen)
{
    if (s == NULL)
        return 0;
    size_t i;
    for (i = 0; i < maxlen; i++)
    {
        if (s[i] == '\0')
            break;
    }
    return i;
}

/**
 * Determina si un punto en la posicion dada es un separador de miles o decimal.
 * Un punto se considera separador de miles si hay al menos 3 digitos despues de
 * el.
 */
static int is_thousands_separator(const char *buffer, int position)
{
    int remaining_digits = 0;

    // Contar digitos despues del punto actual
    for (int k = position + 1; buffer[k] != '\0'; k++)
    {
        if (isdigit(buffer[k]))
        {
            remaining_digits++;
        }
        else if (buffer[k] == ',' || buffer[k] == '.')
        {
            break;
        }
    }

    // Si hay al menos 3 digitos despues, es separador de miles
    return remaining_digits >= 3;
}

/**
 * Procesa un caracter individual y lo agrega al buffer de salida si es valido.
 * Devuelve 1 si se agrego un caracter, 0 si se omitio, -1 si se alcanzo el
 * limite.
 */
static int process_character(char c, char *output, size_t *j,
                             size_t output_size, int *has_decimal,
                             const char *input, int i)
{
    // Procesar coma como separador decimal
    if (c == ',' && !*has_decimal)
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar punto (puede ser separador de miles o decimal)
    if (c == '.' && !*has_decimal)
    {
        if (is_thousands_separator(input, i))
            return 0; // Omitir separador de miles
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar digitos
    if (isdigit(c))
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = c;
        return 1;
    }

    // Ignorar otros caracteres
    return 0;
}

/**
 * Procesa un buffer de entrada para convertir separadores de miles y decimales
 * a formato estandar. Convierte comas a puntos decimales y elimina separadores
 * de miles.
 */
static void process_numeric_input(const char *input, char *output,
                                  size_t output_size)
{
    size_t j = 0;
    int has_decimal = 0;
    int ok = 1;

    if (input == NULL || output == NULL || output_size == 0)
    {
        if (output && output_size > 0)
            output[0] = '\0';
        return;
    }

    int i = 0;
    while (input[i] != '\0' && j < output_size - 1 && ok)
    {
        char c = input[i];

        ok = (process_character(c, output, &j, output_size, &has_decimal, input,
                                i) != -1);
        i++;
    }

    output[j] = '\0';
}

/**
 * Permite la entrada de valores de punto flotante por parte del usuario,
 * facilitando la configuracion de parametros decimales en el sistema.
 * Acepta tanto punto como coma como separador decimal, y maneja separadores de
 * miles.
 */
double input_double(const char *msg)
{
    char buffer[100];
    char processed[100];
    double v = 0.0;

    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, sizeof(buffer)))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        process_numeric_input(buffer, processed, sizeof(processed));

#if defined(_WIN32) && defined(_MSC_VER)
        if (sscanf_s(processed, "%lf", &v) == 1)
#else
        if (sscanf(processed, "%lf", &v) == 1)
#endif
            return v;
        ui_printf("Entrada invalida. Ingrese un numero valido (ej: 250, 1.500, "
                  "12.500, 250.000): ");
    }
}

/**
 * Valida la entrada de texto para asegurar la integridad de los datos y
 * prevenir errores en el procesamiento posterior, aceptando caracteres
 * alfanumericos, espacios, guiones, puntos y parentesis.
 */
void input_string(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        int valid = 1;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            if (!isalpha((unsigned char)buffer[i]) &&
                    !isspace((unsigned char)buffer[i]) &&
                    !isdigit((unsigned char)buffer[i]) && buffer[i] != '-' &&
                    buffer[i] != '.' && buffer[i] != '(' && buffer[i] != ')')
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            return;
        ui_printf("Entrada invalida. Solo se permiten letras, espacios, numeros, "
                  "guiones, puntos y parentesis.\n");
    }
}

void input_string_extended(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        int valid = 1;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            unsigned char c = (unsigned char)buffer[i];
            if (!isalnum(c) && !isspace(c) && c != '+' && c != '-' && c != '.' &&
                    c != ',' && c != '(' && c != ')' && c != ':')
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            return;
        ui_printf("Entrada invalida. Caracteres no permitidos.\n");
    }
}

void convert_display_date_to_storage(const char *display_date,
                                     char *storage_buffer, int buffer_size);

static void get_current_date(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y", &tm_struct);
}

static void get_current_time(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%H:%M", &tm_struct);
}

static void get_current_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

static int es_hora_sola(const char *buffer)
{
    return strchr(buffer, ':') != NULL && strchr(buffer, '/') == NULL &&
           strchr(buffer, '-') == NULL;
}

static int msg_pide_datetime(const char *msg)
{
    if (!msg)
        return 0;
    return (strstr(msg, "HH:MM") || strstr(msg, "hh:mm")) &&
           (strstr(msg, "DD") || strstr(msg, "dd") || strstr(msg, "YYYY") ||
            strstr(msg, "AAAA") || strstr(msg, "/"));
}

static int msg_pide_hora(const char *msg)
{
    if (!msg)
        return 0;
    return (strstr(msg, "HH:MM") || strstr(msg, "hh:mm")) &&
           !msg_pide_datetime(msg);
}

static void completar_fecha_por_defecto(const char *msg, char *buffer,
                                        int size)
{
    if (msg_pide_datetime(msg))
    {
        get_current_datetime(buffer, size);
        return;
    }

    if (msg_pide_hora(msg))
    {
        get_current_time(buffer, size);
        return;
    }

    get_current_date(buffer, size);
}

static int validar_fecha_chars(const char *buffer)
{
    for (int i = 0; buffer[i] != '\0'; i++)
    {
        if (!isdigit(buffer[i]) && buffer[i] != '/' && buffer[i] != ':' &&
                buffer[i] != ' ' && buffer[i] != '-')
        {
            return 0;
        }
    }
    return 1;
}

static int procesar_input_date(const char *msg, char *buffer, int size)
{
    if (safe_strnlen(buffer, (size_t)size) == 0)
    {
        completar_fecha_por_defecto(msg, buffer, size);
    }

    if (!validar_fecha_chars(buffer))
    {
        ui_printf(
            "Entrada invalida. Solo se permiten digitos, barras diagonales (/), "
            "guiones (-) y dos puntos (:).\n");
        return 0;
    }

    if (!es_hora_sola(buffer) && strchr(buffer, '/') != NULL)
    {
        char storage[64];
        convert_display_date_to_storage(buffer, storage, sizeof(storage));
        strncpy_s(buffer, (size_t)size, storage, (size_t)size - 1);
    }

    return 1;
}

/**
 * Valida la entrada de fecha para asegurar el formato correcto,
 * aceptando solo digitos, barras diagonales (/), guiones (-) y dos puntos (:).
 */
void input_date(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        if (procesar_input_date(msg, buffer, size))
        {
            return;
        }
    }
}

/**
 * Proporciona una representacion legible de la fecha y hora actual para mostrar
 * en interfaces de usuario, mejorando la experiencia al contextualizar acciones
 * con el tiempo.
 */
void get_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

/**
 * Genera un identificador temporal compacto para usar en nombres de archivos,
 * asegurando unicidad y orden cronologico en exportaciones y backups.
 */
void get_timestamp(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%Y%m%d_%H%M", &tm_struct);
}

/**
 * Verifica la existencia de registros en la base de datos para mantener la
 * integridad referencial y evitar operaciones invalidas que puedan corromper
 * los datos.
 */
int existe_id(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];

    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s WHERE id=?", tabla);
    if (!db_prepare_stmt(&stmt, sql))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, id);

    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    db_stmt_release(stmt);
    return existe;
}

/**
 * Limpia la pantalla de la consola para proporcionar una interfaz limpia y
 * organizada, mejorando la legibilidad de la informacion mostrada.
 */
static void clear_screen_with_ansi(void)
{
#ifdef _WIN32
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h_out != INVALID_HANDLE_VALUE && GetConsoleMode(h_out, &mode))
    {
        SetConsoleMode(h_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    printf("\033[2J\033[3J\033[H");
    fflush(stdout);
}

#ifdef _WIN32
static int clear_windows_console_buffer(HANDLE h_out)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD origin = {0, 0};
    DWORD written = 0;

    if (h_out == INVALID_HANDLE_VALUE ||
            !GetConsoleScreenBufferInfo(h_out, &csbi))
    {
        return 0;
    }

    SHORT window_width = (SHORT)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    SHORT window_height = (SHORT)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    if (window_width <= 0 || window_height <= 0)
    {
        return 0;
    }

    SMALL_RECT reset_window = {0, 0, (SHORT)(window_width - 1),
                               (SHORT)(window_height - 1)
                              };
    COORD resized_buffer = csbi.dwSize;

    if (resized_buffer.X < window_width)
    {
        resized_buffer.X = window_width;
    }
    resized_buffer.Y = window_height;

    SetConsoleCursorPosition(h_out, origin);
    SetConsoleWindowInfo(h_out, TRUE, &reset_window);
    SetConsoleScreenBufferSize(h_out, resized_buffer);

    if (!GetConsoleScreenBufferInfo(h_out, &csbi))
    {
        return 0;
    }

    DWORD total_cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    if (!FillConsoleOutputCharacterA(h_out, ' ', total_cells, origin, &written))
    {
        return 0;
    }
    if (!FillConsoleOutputAttribute(h_out, csbi.wAttributes, total_cells, origin,
                                    &written))
    {
        return 0;
    }

    return SetConsoleCursorPosition(h_out, origin) != 0;
}
#endif

void clear_screen(void)
{
#ifdef _WIN32
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (clear_windows_console_buffer(h_out))
    {
        clear_screen_with_ansi();
        return;
    }
#endif
    clear_screen_with_ansi();
}

typedef struct
{
    const char *ascii;
    const char *keyword_primary;
    const char *keyword_secondary;
} HeaderAsciiRule;

static int titulo_coincide_regla_ascii(const char *titulo,
                                       const HeaderAsciiRule *rule)
{
    if (!titulo || !rule || !rule->keyword_primary)
    {
        return 0;
    }

    if (strstr(titulo, rule->keyword_primary))
    {
        return 1;
    }

    return rule->keyword_secondary && strstr(titulo, rule->keyword_secondary);
}

/**
 * Muestra informacion contextual del usuario y fecha para personalizar la
 * experiencia y registrar el momento de las operaciones, incluyendo arte ASCII
 * contextual.
 */
static const char *obtener_ascii_por_titulo(const char *titulo)
{
    static const HeaderAsciiRule rules[] =
    {
        {ASCII_BIENVENIDA, "MI FUTBOL C", NULL},
        {ASCII_CAMISETA, "CAMISETA", "CAMISETAS"},
        {ASCII_CANCHA, "CANCHAS", NULL},
        {ASCII_FUTBOL, "PARTIDO", "PARTIDOS"},
        {ASCII_EQUIPO, "EQUIPOS", NULL},
        {ASCII_ESTADISTICAS, "ESTADISTICA", "ESTADISTICAS"},
        {ASCII_LOGROS, "LOGROS", NULL},
        {ASCII_ANALISIS, "ANALISIS", "EVOLUCION TEMPORAL"},
        {ASCII_BIENESTAR, "BIENESTAR", "WELLNESS"},
        {ASCII_LESIONES, "LESIONES", NULL},
        {ASCII_FINANCIAMIENTO, "FINANCIAMIENTO", NULL},
        {ASCII_EXPORTAR, "EXPORTAR", NULL},
        {ASCII_IMPORTAR, "IMPORTAR", NULL},
        {ASCII_TORNEOS, "TORNEOS", NULL},
        {ASCII_AJUSTES, "AJUSTES", "SETTINGS"},
        {ASCII_TEMPORADA, "TEMPORADA", "SEASON"},
        {ASCII_TIENDAS, "TIENDAS", NULL},
        {ASCII_RECORDATORIOS, "RECORDATORIOS", NULL},
        {ASCII_COLECCIONES, "COLECCIONES", NULL},
        {ASCII_ENTRENADOR_IA, "ENTRENADOR IA", NULL},
        {ASCII_CARRERA, "CARRERA", NULL},
        {ASCII_DASHBOARD, "DASHBOARD", NULL},
        {ASCII_CALENDARIO, "CALENDARIO", NULL},
        {ASCII_MUSICA, "MUSICA", NULL}
    };

    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++)
    {
        if (titulo_coincide_regla_ascii(titulo, &rules[i]))
        {
            return rules[i].ascii;
        }
    }

    return NULL;
}

static void free_nombre_usuario_if_needed(char *nombre_usuario)
{
    if (nombre_usuario && strcmp(nombre_usuario, "Usuario Desconocido") != 0)
    {
        free(nombre_usuario);
    }
}

static void print_header_stdout(const char *ascii, const char *titulo_display,
                                const char *nombre_usuario, const char *fecha,
                                int mostrar_datos)
{
    if (ascii)
    {
        printf("%s\n", ascii);
    }

    printf("%s\n", titulo_display);
    if (mostrar_datos)
    {
        printf(" Usuario: %s\n", nombre_usuario);
        printf(" Fecha  : %s\n", fecha);
    }
    printf("========================================\n\n");
}

void print_header(const char *titulo)
{
    char fecha[20];
    get_datetime(fecha, sizeof(fecha));
    char titulo_buf[128];
    const char *titulo_display;

    char *nombre_usuario = get_user_name();
    if (!nombre_usuario)
    {
        nombre_usuario = "Usuario Desconocido";
    }

    uppercase_ascii(titulo, titulo_buf, sizeof(titulo_buf));
    titulo_display = titulo ? titulo_buf : "";
    const char *ascii = obtener_ascii_por_titulo(titulo_display);
    int mostrar_datos = 1;
    if (titulo && strstr(titulo, "LISTADO") != NULL)
    {
        mostrar_datos = 0;
    }

    print_header_stdout(ascii, titulo_display, nombre_usuario, fecha,
                        mostrar_datos);
    free_nombre_usuario_if_needed(nombre_usuario);
}

int consola_soporta_unicode(void)
{
#ifdef _WIN32
    return 0;
#else
    return 1;
#endif
}

/**
 * Pausa la ejecucion para permitir al usuario revisar informacion antes de
 * continuar, mejorando la interaccion controlada.
 */
void pause_console(void)
{
    ui_printf("\nPresione ENTER para continuar...");
    getchar();
}

/**
 * Solicita confirmacion binaria del usuario para operaciones criticas,
 * previniendo acciones accidentales que puedan afectar datos.
 */
int confirmar(const char *msg)
{
    char linea[16];
    ui_printf("%s (S/N): ", msg);
    if (!fgets(linea, sizeof(linea), stdin))
        return 0;
    size_t len = strlen_s(linea, sizeof(linea));
    if (len > 0 && linea[len - 1] == '\n')
        linea[len - 1] = '\0';
    if (linea[0] == '\0')
        return 0;
    return (linea[0] == 's' || linea[0] == 'S');
}

static void leer_nombre_no_vacio(const char *prompt, const char *prompt_vacio,
                                 char *nombre, int size)
{
    ui_printf("%s", prompt);

    ui_readline(nombre, size);
    nombre[strcspn(nombre, "\n")] = 0;

    while (safe_strnlen(nombre, (size_t)size) == 0)
    {
        ui_printf("%s", prompt_vacio);
        ui_readline(nombre, size);
        nombre[strcspn(nombre, "\n")] = 0;
    }
}

static int leer_contrasena_no_vacia(const char *prompt, char *buffer,
                                    int size)
{
    if (!buffer || size <= 1)
    {
        return 0;
    }

    while (1)
    {
        ui_printf("%s", prompt);
        int pos = 0;
        int done = 0;

        memset(buffer, 0, (size_t)size);

        while (!done)
        {
#ifdef _WIN32
            int ch = _getch();
#else
            struct termios oldt;
            struct termios newt;
            int ch;

            if (tcgetattr(STDIN_FILENO, &oldt) != 0)
            {
                return 0;
            }
            newt = oldt;
            newt.c_lflag &= (unsigned int)~(ECHO | ICANON);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            ch = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

            if (ch == '\r' || ch == '\n')
            {
                done = 1;
                ui_printf("\n");
            }
            else if ((ch == 8 || ch == 127) && pos > 0)
            {
                pos--;
                buffer[pos] = '\0';
                ui_printf("\b \b");
            }
            else if (ch >= 32 && ch <= 126 && pos < (size - 1))
            {
                buffer[pos++] = (char)ch;
                buffer[pos] = '\0';
                ui_printf("*");
            }
        }

        if (safe_strnlen(buffer, (size_t)size) >= 4)
        {
            return 1;
        }

        ui_printf("La contrasena debe tener al menos 4 caracteres.\n");
    }
}

static int password_symbols_enabled(void)
{
    return 1;
}

static int es_simbolo_password_permitido(unsigned char c)
{
    const char *allowed = "!@#$%^&*()-_=+[]{};:,.?";
    for (int i = 0; allowed[i] != '\0'; i++)
    {
        if ((unsigned char)allowed[i] == c)
        {
            return 1;
        }
    }
    return 0;
}

static int password_es_alfanumerica_con_reglas(const char *password)
{
    int has_upper = 0;
    int has_lower = 0;
    int has_digit = 0;

    if (!password)
    {
        return 0;
    }

    for (int i = 0; password[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)password[i];
        if (!isalnum(c) &&
                !(password_symbols_enabled() && es_simbolo_password_permitido(c)))
        {
            return 0;
        }
        if (isupper(c))
        {
            has_upper = 1;
        }
        else if (islower(c))
        {
            has_lower = 1;
        }
        else if (isdigit(c))
        {
            has_digit = 1;
        }
    }

    return has_upper && has_lower && has_digit;
}

static const char *fortaleza_password(const char *password)
{
    int has_upper = 0;
    int has_lower = 0;
    int has_digit = 0;
    int score = 0;
    size_t len = safe_strnlen(password, 256);

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)password[i];
        if (isupper(c))
        {
            has_upper = 1;
        }
        else if (islower(c))
        {
            has_lower = 1;
        }
        else if (isdigit(c))
        {
            has_digit = 1;
        }
    }

    if (len >= 8)
    {
        score++;
    }
    if (len >= 12)
    {
        score++;
    }
    score += has_upper + has_lower + has_digit;

    if (score >= 5)
    {
        return "ALTA";
    }
    if (score >= 3)
    {
        return "MEDIA";
    }
    return "BAJA";
}

static int flujo_configurar_password(int requiere_actual)
{
    char nueva[128];
    char confirmar_pwd[128];

    (void)requiere_actual;

    if (!leer_contrasena_no_vacia("Ingresa tu nueva contrasena: ", nueva,
                                  (int)sizeof(nueva)))
    {
        return 0;
    }

    if (!password_es_alfanumerica_con_reglas(nueva))
    {
        if (password_symbols_enabled())
        {
            ui_printf("La contrasena debe incluir mayuscula, minuscula y numero. "
                      "Puede usar letras, numeros y simbolos permitidos "
                      "(!@#$%^&*()-_=+[]{};:,.?).\n");
        }
        else
        {
            ui_printf("La contrasena debe usar solo letras y numeros, e incluir "
                      "mayuscula, minuscula y numero.\n");
        }
        return 0;
    }

    ui_printf("Fortaleza estimada: %s\n", fortaleza_password(nueva));

    if (!leer_contrasena_no_vacia("Confirma tu nueva contrasena: ", confirmar_pwd,
                                  (int)sizeof(confirmar_pwd)))
    {
        return 0;
    }

    if (strcmp(nueva, confirmar_pwd) != 0)
    {
        ui_printf("Las contrasenas no coinciden.\n");
        return 0;
    }

    if (!set_user_password(nueva))
    {
        ui_printf("No se pudo guardar la contrasena.\n");
        return 0;
    }

    ui_printf("Contrasena guardada correctamente.\n");
    return 1;
}

static int auth_username_exists(sqlite3 *auth_db, const char *username)
{
    sqlite3_stmt *stmt = NULL;
    int exists = 0;

    if (sqlite3_prepare_v2(
                auth_db, "SELECT 1 FROM local_users WHERE username = ? LIMIT 1;", -1,
                &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        exists = (sqlite3_step(stmt) == SQLITE_ROW);
        db_stmt_release(stmt);
    }
    return exists;
}

static int auth_upsert_user(sqlite3 *auth_db, const char *username,
                            const char *plain_password)
{
    sqlite3_stmt *stmt = NULL;
    char salt[33] = "";
    char hash[17] = "";
    const char *sql = "INSERT INTO local_users(username, password_salt, "
                      "password_hash) VALUES(?, ?, ?);";

    if (plain_password && plain_password[0] != '\0')
    {
        auth_generate_salt_hex(salt, sizeof(salt));
        auth_build_password_hash(plain_password, salt, hash, sizeof(hash));
    }

    if (sqlite3_prepare_v2(auth_db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, salt, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, hash, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        db_stmt_release(stmt);
        return 0;
    }

    db_stmt_release(stmt);
    return 1;
}

static int auth_get_password_fields(sqlite3 *auth_db, const char *username,
                                    char *salt_out, size_t salt_size,
                                    char *hash_out, size_t hash_size)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(auth_db,
                           "SELECT password_salt, password_hash FROM local_users "
                           "WHERE username = ? LIMIT 1;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *salt = (const char *)sqlite3_column_text(stmt, 0);
        const char *hash = (const char *)sqlite3_column_text(stmt, 1);
        if (salt_out && salt_size > 0)
        {
            strncpy_s(salt_out, salt_size, salt ? salt : "", salt_size - 1);
        }
        if (hash_out && hash_size > 0)
        {
            strncpy_s(hash_out, hash_size, hash ? hash : "", hash_size - 1);
        }
        ok = 1;
    }

    db_stmt_release(stmt);
    return ok;
}

static int auth_verify_user_password(sqlite3 *auth_db, const char *username,
                                     const char *plain_password)
{
    char salt[64];
    char hash[64];
    char computed[32];

    if (!auth_get_password_fields(auth_db, username, salt, sizeof(salt), hash,
                                  sizeof(hash)))
    {
        return 0;
    }

    if (hash[0] == '\0')
    {
        return 1;
    }

    auth_build_password_hash(plain_password, salt, computed, sizeof(computed));
    return strcmp(hash, computed) == 0;
}

static int auth_user_requires_password(sqlite3 *auth_db, const char *username)
{
    char salt[64];
    char hash[64];
    if (!auth_get_password_fields(auth_db, username, salt, sizeof(salt), hash,
                                  sizeof(hash)))
    {
        return 0;
    }
    return hash[0] != '\0';
}

static int auth_prompt_password_login(sqlite3 *auth_db, const char *username)
{
    char intento[128];

    if (!auth_user_requires_password(auth_db, username))
    {
        return 1;
    }

    for (int i = 0; i < 3; i++)
    {
        if (!leer_contrasena_no_vacia("Contrasena: ", intento,
                                      (int)sizeof(intento)))
        {
            continue;
        }

        if (auth_verify_user_password(auth_db, username, intento))
        {
            return 1;
        }

        ui_printf("Contrasena incorrecta. Intentos restantes: %d\n", 2 - i);
    }

    return 0;
}

static int auth_open_for_active_user(sqlite3 **auth_db,
                                     const char **username_out)
{
    const char *username = db_get_active_user();

    if (!username || username[0] == '\0')
    {
        ui_printf("No hay sesion activa.\n");
        pause_console();
        return 0;
    }

    if (!auth_open(auth_db))
    {
        ui_printf("No se pudo abrir el registro de usuarios.\n");
        pause_console();
        return 0;
    }

    *username_out = username;
    return 1;
}

static int auth_confirm_current_password(sqlite3 *auth_db, const char *username,
        const char *prompt,
        const char *error_message)
{
    char actual[128];

    if (!auth_user_requires_password(auth_db, username))
    {
        return 1;
    }

    if (!leer_contrasena_no_vacia(prompt, actual, (int)sizeof(actual)) ||
            !auth_verify_user_password(auth_db, username, actual))
    {
        ui_printf("%s\n", error_message);
        sqlite3_close(auth_db);
        pause_console();
        return 0;
    }

    return 1;
}

static int auth_registrar_usuario_interactivo(sqlite3 *auth_db)
{
    char username[64];
    char respuesta[16];
    char nueva[128] = "";
    char confirmar_pwd[128] = "";

    leer_nombre_no_vacio("Nuevo usuario (3-32, letras/numeros/_/-): ",
                         "Usuario obligatorio: ", username,
                         (int)sizeof(username));

    if (!auth_username_valido(username))
    {
        ui_printf(
            "Usuario invalido. Usa 3-32 caracteres: letras, numeros, '_' o '-'.\n");
        return 0;
    }

    if (auth_username_exists(auth_db, username))
    {
        ui_printf("Ese usuario ya existe.\n");
        return 0;
    }

    ui_printf("Deseas poner contrasena para este usuario? (S/N): ");
    if (!fgets(respuesta, sizeof(respuesta), stdin))
    {
        return 0;
    }

    if (respuesta[0] == 's' || respuesta[0] == 'S')
    {
        if (!leer_contrasena_no_vacia("Ingresa contrasena: ", nueva,
                                      (int)sizeof(nueva)))
        {
            return 0;
        }
        if (!password_es_alfanumerica_con_reglas(nueva))
        {
            ui_printf("La contrasena debe incluir mayuscula, minuscula y numero.\n");
            return 0;
        }
        ui_printf("Fortaleza estimada: %s\n", fortaleza_password(nueva));
        if (!leer_contrasena_no_vacia("Confirma contrasena: ", confirmar_pwd,
                                      (int)sizeof(confirmar_pwd)))
        {
            return 0;
        }
        if (strcmp(nueva, confirmar_pwd) != 0)
        {
            ui_printf("Las contrasenas no coinciden.\n");
            return 0;
        }
    }

    if (!auth_upsert_user(auth_db, username, nueva))
    {
        ui_printf("No se pudo crear el usuario.\n");
        return 0;
    }

    ui_printf("Usuario '%s' creado correctamente.\n", username);
    return 1;
}

static int auth_seleccionar_usuario(sqlite3 *auth_db, char *username_out,
                                    size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    char users[64][64];
    int count = 0;

    if (!username_out || out_size == 0)
    {
        return 0;
    }

    if (sqlite3_prepare_v2(auth_db,
                           "SELECT username FROM local_users ORDER BY username;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && count < 64)
    {
        const char *u = (const char *)sqlite3_column_text(stmt, 0);
        strncpy_s(users[count], sizeof(users[count]), u ? u : "",
                  sizeof(users[count]) - 1);
        count++;
    }
    db_stmt_release(stmt);

    if (count == 0)
    {
        return 0;
    }

    ui_printf("\nUsuarios locales:\n");
    for (int i = 0; i < count; i++)
    {
        ui_printf("%d. %s\n", i + 1, users[i]);
    }
    ui_printf("0. Volver\n");

    int op = input_int("> ");
    if (op <= 0 || op > count)
    {
        return 0;
    }

    strncpy_s(username_out, out_size, users[op - 1], out_size - 1);
    return 1;
}

static int auth_get_single_username(sqlite3 *auth_db, char *username_out,
                                    size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (!username_out || out_size == 0)
    {
        return 0;
    }

    if (sqlite3_prepare_v2(
                auth_db,
                "SELECT username FROM local_users ORDER BY username LIMIT 1;", -1,
                &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *u = (const char *)sqlite3_column_text(stmt, 0);
        strncpy_s(username_out, out_size, u ? u : "", out_size - 1);
        ok = (username_out[0] != '\0');
    }

    db_stmt_release(stmt);
    return ok;
}

static int auth_activar_usuario_y_cerrar(sqlite3 *auth_db,
        const char *username)
{
    db_set_active_user(username);
    sqlite3_close(auth_db);
    return 1;
}

static int auth_flujo_sin_usuarios(sqlite3 *auth_db, char *selected_user,
                                   size_t selected_user_size)
{
    if (auth_importar_usuario_legado_si_existe(auth_db))
    {
        return 0;
    }

    ui_printf(
        "\nNo hay usuarios locales. Crea tu primer usuario para continuar.\n");
    ui_printf(
        "Con un solo usuario es suficiente; agregar mas usuarios es opcional.\n");

    if (!auth_registrar_usuario_interactivo(auth_db))
    {
        return 0;
    }

    if (!auth_seleccionar_usuario(auth_db, selected_user, selected_user_size))
    {
        return 0;
    }

    return 1;
}

static int auth_flujo_un_usuario(sqlite3 *auth_db, char *selected_user,
                                 size_t selected_user_size)
{
    if (!auth_get_single_username(auth_db, selected_user, selected_user_size))
    {
        ui_printf("No se pudo cargar el usuario local.\n");
        return 0;
    }

    if (!auth_prompt_password_login(auth_db, selected_user))
    {
        ui_printf("Acceso denegado.\n");
        return 0;
    }

    return 1;
}

static int auth_menu_multiusuario(sqlite3 *auth_db, char *selected_user,
                                  size_t selected_user_size)
{
    ui_printf("\n=== MULTIUSUARIO LOCAL ===\n");
    ui_printf("1. Iniciar sesion\n");
    ui_printf("2. Agregar usuario local (opcional)\n");
    ui_printf("0. Salir\n");

    int op = input_int("> ");
    if (op == 0)
    {
        return 0;
    }

    if (op == 2)
    {
        auth_registrar_usuario_interactivo(auth_db);
        return -1;
    }

    if (op != 1)
    {
        ui_printf("Opcion invalida.\n");
        return -1;
    }

    if (!auth_seleccionar_usuario(auth_db, selected_user, selected_user_size))
    {
        return -1;
    }

    if (!auth_prompt_password_login(auth_db, selected_user))
    {
        ui_printf("Acceso denegado.\n");
        return -1;
    }

    return 1;
}

int iniciar_sesion_multiusuario_local(void)
{
    sqlite3 *auth_db = NULL;
    char selected_user[64];

    if (!auth_open(&auth_db))
    {
        ui_printf("No se pudo abrir el registro local de usuarios.\n");
        return 0;
    }

    while (1)
    {
        int total = auth_user_count(auth_db);
        if (total == 0)
        {
            if (auth_flujo_sin_usuarios(auth_db, selected_user,
                                        sizeof(selected_user)))
            {
                return auth_activar_usuario_y_cerrar(auth_db, selected_user);
            }
            continue;
        }

        if (total == 1)
        {
            if (auth_flujo_un_usuario(auth_db, selected_user,
                                      sizeof(selected_user)))
            {
                return auth_activar_usuario_y_cerrar(auth_db, selected_user);
            }
            continue;
        }

        int action =
            auth_menu_multiusuario(auth_db, selected_user, sizeof(selected_user));
        if (action == 0)
        {
            sqlite3_close(auth_db);
            return 0;
        }
        if (action < 0)
        {
            continue;
        }

        return auth_activar_usuario_y_cerrar(auth_db, selected_user);
    }
}

void configurar_password_inicial_opcional(void)
{
    char respuesta[16];

    if (user_has_password())
    {
        return;
    }

    ui_printf("\nDeseas configurar una contrasena para tu usuario? (S/N): ");
    if (!fgets(respuesta, sizeof(respuesta), stdin))
    {
        return;
    }

    if (respuesta[0] == 's' || respuesta[0] == 'S')
    {
        flujo_configurar_password(0);
    }
    else
    {
        ui_printf("Contrasena omitida. Puedes configurarla luego en Ajustes -> "
                  "Usuario.\n");
    }
}

int autenticar_usuario_si_tiene_password(void)
{
    char intento[128];

    if (!user_has_password())
    {
        return 1;
    }

    ui_printf("\nEste perfil tiene contrasena.\n");
    for (int i = 0; i < 3; i++)
    {
        if (!leer_contrasena_no_vacia("Ingresa tu contrasena: ", intento,
                                      (int)sizeof(intento)))
        {
            continue;
        }

        if (verify_user_password(intento))
        {
            return 1;
        }

        ui_printf("Contrasena incorrecta. Intentos restantes: %d\n", 2 - i);
    }

    return 0;
}

static void configurar_o_cambiar_password_usuario(void)
{
    sqlite3 *auth_db = NULL;
    char nueva[128];
    char confirmar_pwd[128];
    sqlite3_stmt *stmt = NULL;
    const char *username = NULL;
    char salt[33];
    char hash[17];

    if (!auth_open_for_active_user(&auth_db, &username) ||
            !auth_confirm_current_password(
                auth_db, username,
                "Ingresa tu contrasena actual: ", "Contrasena actual incorrecta."))
    {
        return;
    }

    if (!leer_contrasena_no_vacia("Ingresa tu nueva contrasena: ", nueva,
                                  (int)sizeof(nueva)))
    {
        sqlite3_close(auth_db);
        pause_console();
        return;
    }
    if (!password_es_alfanumerica_con_reglas(nueva))
    {
        ui_printf("La contrasena debe incluir mayuscula, minuscula y numero.\n");
        sqlite3_close(auth_db);
        pause_console();
        return;
    }
    ui_printf("Fortaleza estimada: %s\n", fortaleza_password(nueva));
    if (!leer_contrasena_no_vacia("Confirma tu nueva contrasena: ", confirmar_pwd,
                                  (int)sizeof(confirmar_pwd)) ||
            strcmp(nueva, confirmar_pwd) != 0)
    {
        ui_printf("Las contrasenas no coinciden.\n");
        sqlite3_close(auth_db);
        pause_console();
        return;
    }

    auth_generate_salt_hex(salt, sizeof(salt));
    auth_build_password_hash(nueva, salt, hash, sizeof(hash));

    if (sqlite3_prepare_v2(auth_db,
                           "UPDATE local_users SET password_salt = ?, "
                           "password_hash = ? WHERE username = ?;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, salt, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            ui_printf("Contrasena actualizada correctamente.\n");
        }
        else
        {
            ui_printf("No se pudo actualizar la contrasena.\n");
        }
        db_stmt_release(stmt);
    }

    sqlite3_close(auth_db);
    pause_console();
}

static void quitar_password_usuario(void)
{
    sqlite3 *auth_db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *username = NULL;

    if (!auth_open_for_active_user(&auth_db, &username) ||
            !auth_confirm_current_password(
                auth_db, username, "Para quitarla, ingresa tu contrasena actual: ",
                "Contrasena incorrecta."))
    {
        return;
    }

    if (sqlite3_prepare_v2(auth_db,
                           "UPDATE local_users SET password_salt = '', "
                           "password_hash = '' WHERE username = ?;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            ui_printf("Contrasena eliminada correctamente.\n");
        }
        else
        {
            ui_printf("No se pudo eliminar la contrasena.\n");
        }
        db_stmt_release(stmt);
    }

    sqlite3_close(auth_db);
    pause_console();
}

/**
 * Recopila la identidad del usuario en el inicio para personalizar la
 * aplicacion y mantener un registro de uso.
 */
void pedir_nombre_usuario(void)
{
    char nombre[100];
    clear_screen();
    ui_printf("%s\n", ASCII_BIENVENIDA);
    leer_nombre_no_vacio("Por favor, ingresa tu Nombre: ",
                         "El nombre no puede estar vacio. Ingresa tu nombre: ",
                         nombre, (int)sizeof(nombre));

    if (set_user_name(nombre))
    {
        ui_printf("!Bienvenido, %s!\n", nombre);
    }
    else
    {
        ui_printf("Error al guardar el nombre. Intenta nuevamente.\n");
    }
    pause_console();
}

/**
 * Permite al usuario verificar su identidad actual almacenada,
 * facilitando la gestion de su perfil.
 */
void mostrar_nombre_usuario(void)
{
    char *nombre = get_user_name();
    if (nombre)
    {
        ui_printf("Tu nombre actual es: %s\n", nombre);
        free(nombre);
    }
    else
    {
        ui_printf("No se pudo obtener el nombre del usuario.\n");
    }
    pause_console();
}

/**
 * Habilita la actualizacion de la identidad del usuario para mantener la
 * informacion actualizada y personalizada.
 */
void editar_nombre_usuario(void)
{
    char nombre[100];
    leer_nombre_no_vacio(
        "Ingresa tu nuevo nombre: ",
        "El nombre no puede estar vacio. Ingresa tu nuevo nombre: ", nombre,
        (int)sizeof(nombre));

    if (set_user_name(nombre))
    {
        ui_printf("Nombre actualizado exitosamente a: %s\n", nombre);
    }
    else
    {
        ui_printf("Error al actualizar el nombre.\n");
    }
    pause_console();
}

static void agregar_usuario_local(void)
{
    sqlite3 *auth_db = NULL;
    if (!auth_open(&auth_db))
    {
        ui_printf("No se pudo abrir el registro de usuarios.\n");
        pause_console();
        return;
    }

    auth_registrar_usuario_interactivo(auth_db);
    sqlite3_close(auth_db);
    pause_console();
}

static void eliminar_mi_cuenta_local(void)
{
    sqlite3 *auth_db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *username = NULL;
    char user_db_path[1024];
    char user_log_path[1024];

    if (!db_get_active_user() || db_get_active_user()[0] == '\0')
    {
        ui_printf("No hay sesion activa.\n");
        pause_console();
        return;
    }

    ui_printf(
        "ATENCION: Esta accion es IRREVERSIBLE y eliminara tu cuenta local.\n");
    if (!confirmar("Estas seguro de continuar"))
    {
        ui_printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    if (!auth_open_for_active_user(&auth_db, &username) ||
            !auth_confirm_current_password(
                auth_db, username,
                "Confirma tu contrasena actual: ", "Contrasena incorrecta."))
    {
        return;
    }

    if (sqlite3_prepare_v2(auth_db, "DELETE FROM local_users WHERE username = ?;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            ui_printf("Cuenta eliminada. Cerrando aplicacion...\n");
        }
        else
        {
            ui_printf("No se pudo eliminar la cuenta.\n");
            db_stmt_release(stmt);
            sqlite3_close(auth_db);
            pause_console();
            return;
        }
        db_stmt_release(stmt);
    }

    sqlite3_close(auth_db);

    auth_get_user_data_paths(username, user_db_path, sizeof(user_db_path),
                             user_log_path, sizeof(user_log_path));
    finalizar_atajos();
    db_close();
    remove(user_db_path);
    remove(user_log_path);
    exit(0);
}

/**
 * Proporciona una interfaz estructurada para gestionar opciones relacionadas
 * con el perfil del usuario.
 */
void menu_usuario(void)
{
    MenuItem items[] =
    {
        {1, "Mostrar Nombre", mostrar_nombre_usuario},
        {2, "Editar Nombre Visible", editar_nombre_usuario},
        {3, "Agregar Usuario Local", agregar_usuario_local},
        {4, "Modificar Mi Contrasena", configurar_o_cambiar_password_usuario},
        {5, "Quitar Mi Contrasena", quitar_password_usuario},
        {6, "Eliminar Mi Cuenta (Irreversible)", eliminar_mi_cuenta_local},
        {0, "Volver", NULL}
    };

    ejecutar_menu("USUARIO", items, 7);
}

/**
 * Adapta fechas del almacenamiento interno a un formato amigable para la
 * visualizacion, permitiendo flexibilidad en formatos futuros.
 */
void format_date_for_display(const char *input_date, char *output_buffer,
                             int buffer_size)
{
    if (!input_date || buffer_size <= 0)
        return;

    if (safe_strnlen(input_date, (size_t)buffer_size) >= 10 &&
            input_date[4] == '-' && input_date[7] == '-')
    {
        char fecha[16];
        fecha[0] = input_date[8];
        fecha[1] = input_date[9];
        fecha[2] = '/';
        fecha[3] = input_date[5];
        fecha[4] = input_date[6];
        fecha[5] = '/';
        fecha[6] = input_date[0];
        fecha[7] = input_date[1];
        fecha[8] = input_date[2];
        fecha[9] = input_date[3];
        fecha[10] = '\0';

        if (input_date[10] == ' ')
        {
            snprintf(output_buffer, (size_t)buffer_size, "%s%s", fecha,
                     input_date + 10);
        }
        else
        {
            strncpy_s(output_buffer, buffer_size, fecha, buffer_size - 1);
        }
        return;
    }

    strncpy_s(output_buffer, buffer_size, input_date, buffer_size - 1);
}

void format_date_with_weekday_for_display(const char *input_date,
        char *output_buffer,
        int buffer_size)
{
    static const char *dias_semana[] =
    {
        "Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"
    };

    if (!output_buffer || buffer_size <= 0)
    {
        return;
    }

    output_buffer[0] = '\0';
    if (!input_date)
    {
        return;
    }

    char fecha_formateada[32] = {0};
    format_date_for_display(input_date, fecha_formateada,
                            sizeof(fecha_formateada));
    if (fecha_formateada[0] == '\0')
    {
        return;
    }

    if (safe_strnlen(fecha_formateada, sizeof(fecha_formateada)) >= 10)
    {
        int dia = 0;
        int mes = 0;
        int anio = 0;
#if defined(_WIN32) && defined(_MSC_VER)
        if (sscanf_s(fecha_formateada, "%2d/%2d/%4d", &dia, &mes, &anio) == 3)
#else
        if (sscanf(fecha_formateada, "%2d/%2d/%4d", &dia, &mes, &anio) == 3)
#endif
        {
            struct tm tm_fecha = {0};
            tm_fecha.tm_mday = dia;
            tm_fecha.tm_mon = mes - 1;
            tm_fecha.tm_year = anio - 1900;

            if (mktime(&tm_fecha) != (time_t)-1 && tm_fecha.tm_wday >= 0 &&
                    tm_fecha.tm_wday <= 6)
            {
                snprintf(output_buffer, (size_t)buffer_size, "%s %s",
                         dias_semana[tm_fecha.tm_wday], fecha_formateada);
                return;
            }
        }
    }

    strncpy_s(output_buffer, buffer_size, fecha_formateada, _TRUNCATE);
}

/**
 * Convierte fechas ingresadas por el usuario a un formato interno consistente,
 * facilitando el almacenamiento y procesamiento uniforme.
 */
void convert_display_date_to_storage(const char *display_date,
                                     char *storage_buffer, int buffer_size)
{
    if (!display_date || buffer_size <= 0)
        return;

    if (strchr(display_date, '/') != NULL &&
            safe_strnlen(display_date, (size_t)buffer_size) >= 10)
    {
        char yyyy[5] = {display_date[6], display_date[7], display_date[8],
                        display_date[9], '\0'
                       };
        char mm[3] = {display_date[3], display_date[4], '\0'};
        char dd[3] = {display_date[0], display_date[1], '\0'};

        if (display_date[10] == ' ')
        {
            snprintf(storage_buffer, (size_t)buffer_size, "%s-%s-%s%s", yyyy, mm, dd,
                     display_date + 10);
        }
        else
        {
            snprintf(storage_buffer, (size_t)buffer_size, "%s-%s-%s", yyyy, mm, dd);
        }
        return;
    }

    strncpy_s(storage_buffer, buffer_size, display_date, buffer_size - 1);
}

/**
 * Normaliza cadenas de texto removiendo caracteres acentuados para asegurar
 * compatibilidad con sistemas que no los soportan y mejorar la consistencia en
 * busquedas.
 */
char *remover_tildes(const char *str)
{
    static char buffer[256];
    size_t j = 0;

    if (!str)
    {
        buffer[0] = '\0';
        return buffer;
    }

    const unsigned char acentos[][3] = {{0xE1, 0xC1, 'a'}, {0xE9, 0xC9, 'e'},
        {0xED, 0xCD, 'i'}, {0xF3, 0xD3, 'o'},
        {0xFA, 0xDA, 'u'}, {0xF1, 0xD1, 'n'},
        {0xFC, 0xDC, 'u'}
    };

    const char *p = str;
    while (*p != '\0' && j < sizeof(buffer) - 1)
    {
        unsigned char c = (unsigned char)*p++;
        char out = (char)c;

        for (size_t i = 0; i < sizeof(acentos) / sizeof(acentos[0]); i++)
        {
            if (c == acentos[i][0] || c == acentos[i][1])
            {
                out = (char)acentos[i][2];
                break;
            }
        }

        buffer[j++] = out;
    }
    buffer[j] = '\0';
    return buffer;
}

void sanitizar_ascii_basico(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (!src)
    {
        return;
    }

    size_t limit = safe_strnlen(src, dst_size - 1);
    for (size_t i = 0; i < limit; i++)
    {
        unsigned char c = (unsigned char)src[i];

        if (c >= 32 && c <= 126)
        {
            dst[i] = (char)c;
        }
        else if (c == '\n' || c == '\r' || c == '\t')
        {
            dst[i] = ' ';
        }
        else
        {
            dst[i] = '?';
        }
    }

    dst[limit] = '\0';
}

/**
 * Convierte un valor de resultado a texto
 *
 * @param resultado El valor numerico del resultado
 * @return La representacion textual del resultado
 */
const char *resultado_to_text(int resultado)
{
    static const char *lookup[] =
    {
        [1] = "VICTORIA",
        [2] = "EMPATE",
        [3] = "DERROTA"
    };
    if (resultado >= 1 && resultado <= 3)
        return lookup[resultado];
    return "DESCONOCIDO";
}

/**
 * Convierte un valor de clima a texto
 *
 * @param clima El valor numerico del clima
 * @return La representacion textual del clima
 */
const char *clima_to_text(int clima)
{
    static const char *clima_names[] =
    {
        [1] = "Despejado",
        [2] = "Nublado",
        [3] = "Lluvia",
        [4] = "Ventoso",
        [5] = "Mucho Calor",
        [6] = "Mucho Frio",
        [7] = "Frio",
        [8] = "Calor",
        [9] = "Llovizna leve",
        [10] = "Lluvia Moderada",
        [11] = "Lluvia fuerte",
        [12] = "Cancha inundada"
    };
    if (clima >= 1 && clima <= 12)
        return clima_names[clima];
    return "DESCONOCIDO";
}

/**
 * Convierte un valor de dia a texto
 *
 * @param dia El valor numerico del dia
 * @return La representacion textual del dia
 */
const char *dia_to_text(int dia)
{
    static const char *lookup[] =
    {
        [1] = "Madrugada",
        [2] = "Manana",
        [3] = "Mediodia",
        [4] = "Tarde",
        [5] = "Atardecer",
        [6] = "Noche"
    };
    if (dia >= 1 && dia <= 6)
        return lookup[dia];
    return "DESCONOCIDO";
}

const char *get_clima_case_sql(void)
{
    return "CASE WHEN clima = 1 THEN 'Despejado' WHEN clima = 2 THEN 'Nublado' "
           "WHEN clima = 3 THEN 'Lluvia' WHEN clima = 4 THEN 'Ventoso' WHEN "
           "clima = 5 THEN 'Mucho Calor' WHEN clima = 6 THEN 'Mucho Frio' WHEN "
           "clima = 7 THEN 'Frio' WHEN clima = 8 THEN 'Calor' WHEN clima = 9 "
           "THEN 'Llovizna leve' WHEN clima = 10 THEN 'Lluvia Moderada' WHEN "
           "clima = 11 THEN 'Lluvia fuerte' WHEN clima = 12 THEN 'Cancha "
           "inundada' END";
}

const char *get_nivel_case_sql(const char *columna)
{
    static char sql[256];
    snprintf(sql, sizeof(sql),
             "CASE WHEN %s <= 3 THEN 'Bajo (1-3)' WHEN %s <= 7 THEN 'Medio "
             "(4-7)' ELSE 'Alto (8-10)' END",
             columna, columna);
    return sql;
}

const char *get_dolor_fisico_case_sql(void)
{
    return "CASE dolor_fisico WHEN 0 THEN '0 Ninguna' WHEN 1 THEN '1 Leve' "
           "WHEN 2 THEN '2 Moderada' WHEN 3 THEN '3 Fuerte' ELSE 'Sin dato' END";
}

const char *get_arbitraje_case_sql(void)
{
    return "CASE arbitraje_score WHEN 1 THEN '1 Muy malo' WHEN 2 THEN '2 Regular' "
           "WHEN 3 THEN '3 Normal' WHEN 4 THEN '4 Bueno' WHEN 5 THEN '5 Excelente' "
           "ELSE 'Sin dato' END";
}

/**
 * Obtiene el nombre de una entidad por su ID desde la base de datos.
 * Funcion generica para evitar duplicacion de codigo en consultas SQL comunes.
 *
 * @param tabla Nombre de la tabla (ej: "camiseta", "torneo", "cancha")
 * @param id ID de la entidad a buscar
 * @param buffer Buffer donde se almacenara el nombre encontrado
 * @param size Tamano maximo del buffer
 * @return 1 si se encontro la entidad, 0 si no se encontro
 */
int obtener_nombre_entidad(const char *tabla, int id, char *buffer,
                           size_t size)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !buffer || size == 0)
    {
        return 0;
    }

    snprintf(sql, sizeof(sql), "SELECT nombre FROM %s WHERE id = ?", tabla);

    if (!db_prepare_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        if (nombre)
        {
            snprintf(buffer, size, "%s", nombre);
            found = 1;
        }
    }

    db_stmt_release(stmt);
    return found;
}

long long obtener_siguiente_id(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[512];

    if (!tabla)
        return 1;

    for (size_t i = 0; tabla[i] != '\0'; i++)
    {
        unsigned char ch = (unsigned char)tabla[i];
        if (!(isalnum(ch) || ch == '_'))
        {
            return 1;
        }
    }

    snprintf(sql, sizeof(sql),
             "SELECT COALESCE(MIN(t1.id + 1), 1) "
             "FROM (SELECT 0 AS id UNION ALL SELECT id FROM %s) t1 "
             "LEFT JOIN (SELECT id FROM %s) t2 ON t1.id + 1 = t2.id "
             "WHERE t2.id IS NULL "
             "ORDER BY t1.id LIMIT 1",
             tabla, tabla);

    if (!db_prepare_stmt(&stmt, sql))
    {
        return 1;
    }

    long long id = 1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int64(stmt, 0);
        if (id <= 0)
            id = 1;
    }
    db_stmt_release(stmt);
    return id;
}

int hay_registros(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla)
        return 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", tabla);
    if (!db_prepare_stmt(&stmt, sql))
    {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    db_stmt_release(stmt);

    return count > 0;
}

int obtener_id_por_nombre(const char *tabla, const char *nombre)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !nombre)
        return -1;

    snprintf(sql, sizeof(sql), "SELECT id FROM %s WHERE nombre = ?", tabla);
    if (!db_prepare_stmt(&stmt, sql))
    {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);
    db_stmt_release(stmt);

    return id;
}

void listar_entidades(const char *tabla, const char *titulo,
                      const char *mensaje_vacio)
{
    if (!tabla || !titulo || !mensaje_vacio)
        return;

    clear_screen();
    print_header(titulo);

    sqlite3_stmt *stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, nombre FROM %s ORDER BY id", tabla);
    if (!db_prepare_stmt(&stmt, sql))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d - %s", sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        ui_printf_centered_line("%s", mensaje_vacio);

    db_stmt_release(stmt);
    pause_console();
}

int input_int_rango(const char *msg, int min, int max)
{
    int valor;
    do
    {
        valor = input_int(msg);
        if (valor < min || valor > max)
            printf("Ingrese un valor entre %d y %d.\n", min, max);
    }
    while (valor < min || valor > max);
    return valor;
}

void mostrar_no_hay_registros(const char *entidad)
{
    if (!entidad)
        return;
    size_t len = safe_strnlen(entidad, SIZE_MAX);
    printf("No hay %s registrad%s.\n", entidad,
           (entidad[len - 1] == 'a' || entidad[len - 1] == 'o') ? "o" : "os");
}

void mostrar_no_existe(const char *entidad)
{
    if (!entidad)
        return;
    printf("El %s no existe.\n", entidad);
}

void mostrar_error_operacion(const char *entidad, const char *operacion)
{
    if (!entidad || !operacion)
        return;
    printf("Error al %s el %s.\n", operacion, entidad);
}

void mostrar_pantalla(const char *titulo)
{
    clear_screen();
    print_header(titulo);
}

char *trim_trailing_spaces(char *str)
{
    if (!str)
        return str;

    size_t len = safe_strnlen(str, SIZE_MAX);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' ||
                       str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[--len] = '\0';
    }
    return str;
}

sqlite3_stmt *execute_query(const char *sql)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, sql))
    {
        return NULL;
    }
    return stmt;
}

int list_available_teams(const char *no_records_msg, int pause_on_empty)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (db_prepare_stmt(&stmt, sql))
    {
        ui_printf_centered_line("=== EQUIPOS DISPONIBLES ===");
        ui_printf("\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
            ui_printf_centered_line("%d. %s", id, nombre);
        }

        if (!found)
        {
            mostrar_no_hay_registros(no_records_msg);
            db_stmt_release(stmt);
            if (pause_on_empty)
            {
                pause_console();
            }
            return 0;
        }
        db_stmt_release(stmt);
        return 1;
    }

    if (pause_on_empty)
    {
        pause_console();
    }
    return 0;
}

int select_team_id(const char *prompt, const char *no_records_msg,
                   int pause_on_error)
{
    if (!list_available_teams(no_records_msg, pause_on_error))
    {
        return 0;
    }
    int equipo_id = input_int(prompt);
    if (equipo_id == 0)
    {
        return 0;
    }
    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        if (pause_on_error)
        {
            pause_console();
        }
        return 0;
    }
    return equipo_id;
}

void write_csv_header(FILE *f, const char *header)
{
    fprintf(f, "%s\n", header);
}

static char *get_trimmed_cancha_from_stmt(sqlite3_stmt *stmt)
{
    char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
    trim_trailing_spaces(cancha_trimmed);
    return cancha_trimmed;
}

void write_partido_csv_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    int atajaste_raw = sqlite3_column_int(stmt, 12);
    if (sqlite3_column_type(stmt, 12) == SQLITE_NULL || atajaste_raw == 0)
    {
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s,%s\n", cancha_trimmed,
                sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)), sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9), sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11), "-");
    }
    else
    {
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s,%d\n", cancha_trimmed,
                sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3), sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)), sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9), sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11), atajaste_raw);
    }
    free(cancha_trimmed);
}

void write_partido_txt_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    int atajaste_val = sqlite3_column_int(stmt, 12);
    const char *atajaste_txt;
    if (sqlite3_column_type(stmt, 12) == SQLITE_NULL || atajaste_val == 0)
        atajaste_txt = "-";
    else
        atajaste_txt = (atajaste_val == 1) ? "SI" : "NO";
    fprintf(f,
            "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d "
            "| Atajaste:%s | %s\n",
            cancha_trimmed, sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)), sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9), sqlite3_column_int(stmt, 10),
            atajaste_txt,
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

void write_partido_json_object(cJSON *item, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);

    cJSON_AddStringToObject(item, "cancha", cancha_trimmed);
    cJSON_AddStringToObject(item, "fecha",
                            (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 3));
    cJSON_AddStringToObject(item, "camiseta",
                            (const char *)sqlite3_column_text(stmt, 4));
    cJSON_AddStringToObject(item, "resultado",
                            resultado_to_text(sqlite3_column_int(stmt, 5)));
    cJSON_AddStringToObject(item, "clima",
                            clima_to_text(sqlite3_column_int(stmt, 6)));
    cJSON_AddStringToObject(item, "dia",
                            dia_to_text(sqlite3_column_int(stmt, 7)));
    cJSON_AddNumberToObject(item, "rendimiento_general",
                            sqlite3_column_int(stmt, 8));
    cJSON_AddNumberToObject(item, "cansancio", sqlite3_column_int(stmt, 9));
    cJSON_AddNumberToObject(item, "estado_animo", sqlite3_column_int(stmt, 10));
    cJSON_AddStringToObject(item, "comentario_personal",
                            (const char *)sqlite3_column_text(stmt, 11));
    int atajaste_j = sqlite3_column_int(stmt, 12);
    if (sqlite3_column_type(stmt, 12) == SQLITE_NULL || atajaste_j == 0)
    {
        cJSON_AddNumberToObject(item, "atajaste_todo_el_partido", -1);
    }
    else
    {
        cJSON_AddNumberToObject(item, "atajaste_todo_el_partido", atajaste_j);
    }

    free(cancha_trimmed);
}

void write_partido_html_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    int atajaste_h = sqlite3_column_int(stmt, 12);
    const char *atajaste_html;
    if (sqlite3_column_type(stmt, 12) == SQLITE_NULL || atajaste_h == 0)
        atajaste_html = "-";
    else
        atajaste_html = (atajaste_h == 1) ? "SI" : "NO";
    fprintf(f,
            "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</"
            "td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</"
            "td><td>%s</td></tr>",
            cancha_trimmed, sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)), sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9), sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11),
            atajaste_html);
    free(cancha_trimmed);
}

void mostrar_record_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt)
        return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Valor: %d\n", sqlite3_column_int(stmt, 0));
        if (sqlite3_column_count(stmt) > 1)
        {
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        }
        if (sqlite3_column_count(stmt) > 2)
        {
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 2));
        }
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    db_stmt_release(stmt);
}

void mostrar_combinacion_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt)
        return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Cancha: %s\n", sqlite3_column_text(stmt, 0));
        printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 2));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 3));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    db_stmt_release(stmt);
}

void mostrar_temporada_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt)
        return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *year = (const char *)sqlite3_column_text(stmt, 0);
        if (year)
        {
            printf("Ano: %s\n", year);
        }
        else
        {
            printf("Ano: Desconocido\n");
        }
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 1));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 2));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    db_stmt_release(stmt);
}

void exportar_record_simple_csv(const char *titulo, const char *sql,
                                const char *filename)
{
    FILE *file = NULL;
    errno_t err = fopen_s(&file, get_export_path(filename), "w");
    if (err != 0 || !file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Valor,Camiseta,Fecha\n");

    sqlite3_stmt *stmt = execute_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s\n", sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
    }

    if (stmt)
        db_stmt_release(stmt);
    printf("Exportado a %s\n", get_export_path(filename));
    fclose(file);
}

void exportar_partido_especifico_csv(const char *order_by,
                                     const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }
    FILE *f = NULL;
    if (fopen_s(&f, get_export_path(filename), "w") != 0 || !f)
        return;
    write_csv_header(
        f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,"
        "Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal");
    sqlite3_stmt *stmt = prepare_partido_query(order_by);
    if (stmt)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            write_partido_csv_row(f, stmt);
        db_stmt_release(stmt);
    }
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

void exportar_partido_especifico_txt(const char *order_by, const char *filename,
                                     const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }
    FILE *f = NULL;
    if (fopen_s(&f, get_export_path(filename), "w") != 0 || !f)
        return;
    fprintf(f, "%s\n\n", title);
    sqlite3_stmt *stmt = prepare_partido_query(order_by);
    if (stmt)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            write_partido_txt_row(f, stmt);
        db_stmt_release(stmt);
    }
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

void exportar_partido_especifico_json(const char *order_by,
                                      const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }
    FILE *f = NULL;
    if (fopen_s(&f, get_export_path(filename), "w") != 0 || !f)
        return;
    cJSON *root = cJSON_CreateObject();
    sqlite3_stmt *stmt = prepare_partido_query(order_by);
    if (stmt)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            write_partido_json_object(root, stmt);
        db_stmt_release(stmt);
    }
    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

void exportar_partido_especifico_html(const char *order_by,
                                      const char *filename, const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }
    FILE *f = NULL;
    if (fopen_s(&f, get_export_path(filename), "w") != 0 || !f)
        return;
    fprintf(f,
            "<html><body><h1>%s</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</"
            "th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</"
            "th><th>Rendimiento General</th><th>Cansancio</th><th>Estado "
            "Animo</th><th>Comentario Personal</th></tr>",
            title);
    sqlite3_stmt *stmt = prepare_partido_query(order_by);
    if (stmt)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            write_partido_html_row(f, stmt);
        db_stmt_release(stmt);
    }
    fprintf(f, "</table></body></html>");
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/* ===================== FUNCIONES DE ESTADiSTICAS COMPARTIDAS
 * ===================== */

void reset_estadisticas(Estadisticas *stats)
{
    memset(stats, 0, sizeof(*stats));
}

void calcular_estadisticas(Estadisticas *stats, const char *sql)
{
    sqlite3_stmt *stmt;
    reset_estadisticas(stats);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->avg_goles = sqlite3_column_double(stmt, 1);
        stats->avg_asistencias = sqlite3_column_double(stmt, 2);
        stats->avg_rendimiento = sqlite3_column_double(stmt, 3);
        stats->avg_cansancio = sqlite3_column_double(stmt, 4);
        stats->avg_animo = sqlite3_column_double(stmt, 5);
    }
    db_stmt_release(stmt);
}

void actualizar_rachas(int resultado, int *racha_actual_v, int *max_racha_v,
                       int *racha_actual_d, int *max_racha_d)
{
    if (resultado == 1)
    {
        (*racha_actual_v)++;
        if (*racha_actual_v > *max_racha_v)
            *max_racha_v = *racha_actual_v;
        *racha_actual_d = 0;
        return;
    }

    if (resultado == 3)
    {
        (*racha_actual_d)++;
        if (*racha_actual_d > *max_racha_d)
            *max_racha_d = *racha_actual_d;
        *racha_actual_v = 0;
        return;
    }

    *racha_actual_v = 0;
    *racha_actual_d = 0;
}

int preparar_stmt_export(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

int preparar_consulta_con_verificacion(sqlite3_stmt **stmt, const char *tabla,
                                       const char *mensaje, const char *sql,
                                       int *count)
{
    sqlite3_stmt *check_stmt;
    *count = 0;

    // Construir consulta de conteo
    char count_sql[256];
    snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) FROM %s", tabla);

    // Verificar si hay registros
    if (!preparar_stmt_export(&check_stmt, count_sql))
    {
        return 0;
    }

    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        *count = sqlite3_column_int(check_stmt, 0);
    }
    db_stmt_release(check_stmt);

    if (*count == 0)
    {
        mostrar_no_hay_registros(mensaje);
        return 0;
    }

    // Preparar la consulta principal
    if (!preparar_stmt_export(stmt, sql))
    {
        return 0;
    }

    return 1;
}

FILE *abrir_archivo_exportacion(const char *filename, const char *error_msg)
{
    FILE *file;
    const char *path = get_export_path(filename);
    errno_t err = fopen_s(&file, path, "w");
    if (err != 0 || file == NULL)
    {
        printf("%s\n", error_msg);
        return NULL;
    }
    setvbuf(file, NULL, _IOFBF, 65536);
    return file;
}

int has_records(const char *table_name)
{
    sqlite3_stmt *stmt;
    char sql[256];
    int count = 0;
    int result = 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table_name);

    if (preparar_stmt_export(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        db_stmt_release(stmt);
        result = count > 0;
    }

    return result;
}

void trim_whitespace(char *str)
{
    if (!str)
        return;

    size_t total = strlen_s(str, SIZE_MAX);
    if (total == 0)
        return;

    char const *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    char const *end = start + strlen_s(start, SIZE_MAX);
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;

    size_t len = (size_t)(end - start);
    if (start != str)
        memmove(str, start, len + 1);
    else
        str[len] = '\0';
}

void extraer_estadistica_anio(sqlite3_stmt *stmt, EstadisticaAnio *stats)
{
    stats->anio = (const char *)sqlite3_column_text(stmt, 0);
    stats->camiseta = (const char *)sqlite3_column_text(stmt, 1);
    stats->partidos = sqlite3_column_int(stmt, 2);
    stats->total_goles = sqlite3_column_int(stmt, 3);
    stats->total_asistencias = sqlite3_column_int(stmt, 4);
    stats->avg_goles = sqlite3_column_double(stmt, 5);
    stats->avg_asistencias = sqlite3_column_double(stmt, 6);
}

#if defined(_WIN32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void app_escape_single_quotes_ps(const char *src, char *dst,
                                        size_t dst_size)
{
    if (!src || !dst || dst_size == 0)
    {
        return;
    }

    size_t j = 0;
    const char *p = src;
    while (*p != '\0' && j + 1 < dst_size)
    {
        if (*p == '\'')
        {
            if (j + 2 >= dst_size)
            {
                break;
            }
            dst[j++] = '\'';
            dst[j++] = '\'';
        }
        else
        {
            dst[j++] = *p;
        }
        p++;
    }
    dst[j] = '\0';
}

#pragma GCC diagnostic pop
#endif

static int app_command_has_safe_chars(const char *cmd)
{
    if (!cmd || cmd[0] == '\0')
    {
        return 0;
    }

    const unsigned char *p = (const unsigned char *)cmd;
    while (*p != '\0')
    {
        if (!(isalnum(*p) || *p == '_' || *p == '-' || *p == '.' || *p == '/' ||
                *p == '\\' || *p == ':'))
        {
            return 0;
        }
        p++;
    }
    return 1;
}

static int app_command_has_path_separator(const char *cmd)
{
    if (!cmd)
    {
        return 0;
    }

    while (*cmd != '\0')
    {
        if (*cmd == '/' || *cmd == '\\')
        {
            return 1;
        }
        cmd++;
    }
    return 0;
}

static int app_path_is_executable_file(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return 0;
    }

#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return 0;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    return access(path, X_OK) == 0;
#endif
}

#ifdef _WIN32
static int app_search_in_windows_path(const char *name)
{
    char resolved[MAX_PATH];
    DWORD found =
        SearchPathA(NULL, name, NULL, (DWORD)sizeof(resolved), resolved, NULL);
    return found > 0 && found < (DWORD)sizeof(resolved);
}

static int app_command_exists_windows(const char *cmd)
{
    if (app_command_has_path_separator(cmd))
    {
        return app_path_is_executable_file(cmd);
    }

    if (app_search_in_windows_path(cmd))
    {
        return 1;
    }

    if (strchr(cmd, '.') != NULL)
    {
        return 0;
    }

    const char *extensions[] = {".exe", ".cmd", ".bat", ".com", NULL};
    for (int i = 0; extensions[i] != NULL; i++)
    {
        char candidate[MAX_PATH];
        int n = snprintf(candidate, sizeof(candidate), "%s%s", cmd, extensions[i]);
        if (n <= 0 || (size_t)n >= sizeof(candidate))
        {
            continue;
        }

        if (app_search_in_windows_path(candidate))
        {
            return 1;
        }
    }

    return 0;
}
#else
static int app_resolve_command_path_posix(const char *cmd, char *resolved_path,
        size_t resolved_size)
{
    if (!cmd || !resolved_path || resolved_size == 0)
    {
        return 0;
    }

    resolved_path[0] = '\0';

    if (!app_command_has_safe_chars(cmd))
    {
        return 0;
    }

    if (app_command_has_path_separator(cmd))
    {
        if (!app_path_is_executable_file(cmd))
        {
            return 0;
        }

        int n = snprintf(resolved_path, resolved_size, "%s", cmd);
        return n > 0 && (size_t)n < resolved_size;
    }

    const char *path_env = getenv("PATH");
    if (!path_env || path_env[0] == '\0')
    {
        return 0;
    }

    size_t path_len = strlen_s(path_env, SIZE_MAX);
    char *path_copy = (char *)malloc(path_len + 1);
    if (!path_copy)
    {
        return 0;
    }

    memcpy(path_copy, path_env, path_len + 1);

    char *segment = path_copy;
    while (segment)
    {
        char *sep = strchr(segment, ':');
        if (sep)
        {
            *sep = '\0';
        }

        const char *dir = (segment[0] == '\0') ? "." : segment;
        size_t needed = strlen_s(dir, SIZE_MAX) + 1 + strlen_s(cmd, SIZE_MAX) + 1;
        char *candidate = (char *)malloc(needed);
        if (candidate)
        {
            snprintf(candidate, needed, "%s/%s", dir, cmd);
            if (app_path_is_executable_file(candidate))
            {
                int n = snprintf(resolved_path, resolved_size, "%s", candidate);
                free(candidate);
                free(path_copy);
                return n > 0 && (size_t)n < resolved_size;
            }
            free(candidate);
        }

        if (!sep)
        {
            break;
        }
        segment = sep + 1;
    }

    free(path_copy);
    return 0;
}

static int app_command_exists_posix(const char *cmd)
{
    char resolved[4096];
    return app_resolve_command_path_posix(cmd, resolved, sizeof(resolved));
}
#endif

int app_command_exists(const char *cmd)
{
    if (!app_command_has_safe_chars(cmd))
    {
        return 0;
    }

#ifdef _WIN32
    return app_command_exists_windows(cmd);
#else
    return app_command_exists_posix(cmd);
#endif
}

int app_command_exists_public(const char *cmd)
{
    return app_command_exists(cmd);
}

#ifndef _WIN32
static int app_spawn_command_posix(const char *command, const char *arg1,
                                   const char *arg2)
{
    if (!command || command[0] == '\0')
    {
        return 0;
    }

    char *argv[4] = {0};
    int argc = 0;
    argv[argc++] = (char *)command;
    if (arg1 && arg1[0] != '\0')
    {
        argv[argc++] = (char *)arg1;
    }
    if (arg2 && arg2[0] != '\0')
    {
        argv[argc++] = (char *)arg2;
    }

    pid_t pid = (pid_t)0;
    if (posix_spawnp(&pid, command, NULL, NULL, argv, environ) != 0)
    {
        return 0;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif

int app_open_with_command(const char *command, const char *path)
{
    if (!path || path[0] == '\0' || !app_validate_file_exists(path))
    {
        return 0;
    }

    if (!command || command[0] == '\0')
    {
        return app_open_with_default_app(path);
    }

    if (!app_command_has_safe_chars(command))
    {
        return 0;
    }

#ifdef _WIN32
    if (!app_command_exists_windows(command))
    {
        return 0;
    }

    char parameters[2048];
    int n = snprintf(parameters, sizeof(parameters), "\"%s\"", path);
    if (n <= 0 || (size_t)n >= sizeof(parameters))
    {
        return 0;
    }

    HINSTANCE h =
        ShellExecuteA(NULL, "open", command, parameters, NULL, SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
#else
    if (!app_command_exists_posix(command))
    {
        return 0;
    }

    if (strcmp(command, "gio") == 0)
    {
        return app_spawn_command_posix(command, "open", path);
    }

    return app_spawn_command_posix(command, path, NULL);
#endif
}

int app_open_with_default_app(const char *path)
{
    if (!path || path[0] == '\0' || !app_validate_file_exists(path))
    {
        return 0;
    }

#ifdef _WIN32
    HINSTANCE h = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
#else
    const char *commands[] = {"xdg-open", "gio", NULL};
    for (int i = 0; commands[i] != NULL; i++)
    {
        if (app_open_with_command(commands[i], path))
        {
            return 1;
        }
    }
    return 0;
#endif
}

void app_build_path(char *dest, size_t size, const char *dir,
                    const char *file_name)
{
    if (!dest || size == 0)
    {
        return;
    }

    if (!dir)
    {
        dir = "";
    }

    if (!file_name)
    {
        file_name = "";
    }

    char full[2048];
#ifdef _WIN32
    snprintf(full, sizeof(full), "%s\\%s", dir, file_name);
#else
    snprintf(full, sizeof(full), "%s/%s", dir, file_name);
#endif
    full[sizeof(full) - 1] = '\0';
    strncpy_s(dest, size, full, size - 1);
}

int app_copy_binary_file(const char *source_path, const char *dest_path)
{
    FILE *src = NULL;
    FILE *dst = NULL;

    if (fopen_s(&src, source_path, "rb") != 0 || !src)
    {
        return 0;
    }

    if (fopen_s(&dst, dest_path, "wb") != 0 || !dst)
    {
        fclose(src);
        return 0;
    }

    char buffer[8192];
    size_t bytes = 0;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, bytes, dst) != bytes)
        {
            fclose(src);
            fclose(dst);
            return 0;
        }
    }

    fclose(src);
    fclose(dst);
    return 1;
}

int app_get_file_name_from_path(const char *path, char *nombre, size_t size)
{
    if (!path || !nombre || size == 0)
    {
        return 0;
    }

    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *base = path;

    if (last_slash && last_backslash)
    {
        base = (last_slash > last_backslash) ? last_slash + 1 : last_backslash + 1;
    }
    else if (last_slash)
    {
        base = last_slash + 1;
    }
    else if (last_backslash)
    {
        base = last_backslash + 1;
    }

    return strncpy_s(nombre, size, base, _TRUNCATE) == 0;
}

void mostrar_alerta_operacion(const char *entidad, const char *operacion,
                              const char *nombre_item)
{
    char log_msg[512];
    const char *entidad_safe = entidad ? entidad : "Entidad";
    const char *operacion_safe = operacion ? operacion : "Procesada";

    printf("\n");
    printf("========================================\n");
    printf("   OPERACION EXITOSA\n");
    printf("========================================\n\n");

    printf("  Entidad  : %s\n", entidad_safe);
    printf("  Operacion: %s\n", operacion_safe);

    if (nombre_item && nombre_item[0] != '\0')
    {
        printf("  Detalle  : %s\n", nombre_item);
    }

    printf("\n========================================\n");

    if (nombre_item && nombre_item[0] != '\0')
    {
        snprintf(log_msg, sizeof(log_msg), "%s %s: %.200s", entidad_safe,
                 operacion_safe, nombre_item);
    }
    else
    {
        snprintf(log_msg, sizeof(log_msg), "%s %s exitosamente", entidad_safe,
                 operacion_safe);
    }

    app_log_event("OPERACION", log_msg);
    pause_console();
}

char *utils_file_read_to_buffer(const char *path, long *out_size)
{
    if (!path)
    {
        if (out_size) *out_size = 0;
        return NULL;
    }
    FILE *f;
    errno_t err = fopen_s(&f, path, "rb");
    if (err != 0 || f == NULL)
    {
        if (out_size) *out_size = 0;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0)
    {
        fclose(f);
        if (out_size) *out_size = 0;
        return NULL;
    }
    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf)
    {
        fclose(f);
        if (out_size) *out_size = 0;
        return NULL;
    }
    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (read > 0 && read <= (size_t)len)
    {
        buf[read] = '\0';
    }
    else
    {
        buf[0] = '\0';
    }
    if (out_size) *out_size = (long)read;
    return buf;
}

int app_is_path_safe_for_shell(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return 0;
    }

    size_t len = safe_strnlen(path, 4096);
    if (len > 4096)
    {
        return 0;
    }

    const char *forbidden = "`$|;&><`";
    size_t forbidden_len = safe_strnlen(forbidden, 16);
    for (size_t i = 0; i < forbidden_len; i++)
    {
        if (strchr(path, forbidden[i]) != NULL)
        {
            return 0;
        }
    }

    if (strstr(path, "..") != NULL)
    {
        return 0;
    }

    return 1;
}

int app_validate_file_exists(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return 0;
    }

#ifdef _WIN32
    return _access_s(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

const char *app_get_file_extension_simple(const char *filename)
{
    if (!filename)
    {
        return NULL;
    }

    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
    {
        return "";
    }

    return dot;
}

int app_get_file_extension(const char *filename, char *ext, size_t size)
{
    if (!filename || !ext || size == 0)
    {
        return 0;
    }

    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
    {
        return 0;
    }

    strncpy_s(ext, size, dot + 1, _TRUNCATE);
    return 1;
}

int app_optimize_image_file(const char *input_path, const char *output_path)
{
    if (!input_path || !output_path)
    {
        return 0;
    }

    return app_copy_binary_file(input_path, output_path);
}

int app_copy_file_binary(const char *source_path, const char *dest_path)
{
    if (!source_path || !dest_path)
    {
        return 0;
    }

    return app_copy_binary_file(source_path, dest_path);
}

int app_select_existing_file(char *ruta, size_t size, const char *prompt,
                             const char *windows_filter,
                             const char *windows_userprofile_subdir)
{
    if (!ruta || size == 0)
    {
        return 0;
    }

#ifdef _WIN32
    (void)prompt;
    char file_buffer[MAX_PATH] = {0};
    char initial_dir[MAX_PATH] = {0};

    if (windows_userprofile_subdir &&
            utils_get_env_var_copy("USERPROFILE", initial_dir, sizeof(initial_dir)))
    {
        strcat_s(initial_dir, sizeof(initial_dir), windows_userprofile_subdir);
    }

    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = (DWORD)sizeof(file_buffer);
    ofn.lpstrFilter = windows_filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrInitialDir = initial_dir[0] ? initial_dir : NULL;

    if (!GetOpenFileNameA(&ofn))
    {
        return 0;
    }

    if (strncpy_s(ruta, size, file_buffer, _TRUNCATE) != 0)
    {
        return 0;
    }

    trim_whitespace(ruta);
    if (!app_validate_file_exists(ruta))
    {
        return 0;
    }
    return ruta[0] != '\0';
#else
    input_string(prompt, ruta, (int)size);
    trim_whitespace(ruta);
    return app_is_path_safe_for_shell(ruta) && app_validate_file_exists(ruta);
#endif
}

static int appSeleccionarArchivoImagen(char *ruta, size_t size)
{
    static const char filter[] =
        "Imagenes "
        "(*.jpg;*.jpeg;*.png;*.bmp;*.webp)\0*.jpg;*.jpeg;*.png;*.bmp;*.webp\0"
        "Todos los archivos (*.*)\0*.*\0";

    return app_select_existing_file(ruta, size, "Ruta de la imagen: ", filter,
                                    "\\Pictures");
}

int app_seleccionar_y_copiar_imagen(const char *config_file,
                                    const char *prefijo, char *ruta_out,
                                    size_t ruta_size)
{
    if (!config_file || !ruta_out || ruta_size == 0)
    {
        return 0;
    }

    char origen[1024] = {0};
    if (!appSeleccionarArchivoImagen(origen, sizeof(origen)))
    {
        return 0;
    }

    char nombre_archivo[256] = {0};
    if (!app_get_file_extension(origen, nombre_archivo, sizeof(nombre_archivo)))
    {
        snprintf(nombre_archivo, sizeof(nombre_archivo), "img");
    }

    char timestamp_str[64];
    get_timestamp(timestamp_str, sizeof(timestamp_str));

    char dest_nombre[350];
    const char *pref = prefijo ? prefijo : "imagen";
    snprintf(dest_nombre, sizeof(dest_nombre), "%s_%s.%s", pref, timestamp_str,
             nombre_archivo);

    char app_dir[512] = {0};
    app_build_path(app_dir, sizeof(app_dir), NULL, "imagenes");
    MKDIR(app_dir);

    char dest_ruta[600];
    app_build_path(dest_ruta, sizeof(dest_ruta), app_dir, dest_nombre);

    if (!app_copy_binary_file(origen, dest_ruta))
    {
        return 0;
    }

    snprintf(ruta_out, ruta_size, "imagenes/%s", dest_nombre);

    FILE *f = NULL;
    if (fopen_s(&f, config_file, "w") == 0 && f)
    {
        fprintf(f, "%s", dest_nombre);
        fclose(f);
    }

    return 1;
}

int app_cargar_imagen_entidad(int id, const char *tabla,
                              const char *config_file)
{
    if (!tabla || !config_file)
    {
        return 0;
    }

    char origen[1024] = {0};
    if (!appSeleccionarArchivoImagen(origen, sizeof(origen)))
    {
        return 0;
    }

    char nombre_archivo[256] = {0};
    if (!app_get_file_extension(origen, nombre_archivo, sizeof(nombre_archivo)))
    {
        snprintf(nombre_archivo, sizeof(nombre_archivo), "png");
    }

    char timestamp_str[64];
    get_timestamp(timestamp_str, sizeof(timestamp_str));

    char dest_nombre[350];
    snprintf(dest_nombre, sizeof(dest_nombre), "%s_%d_%s.%s", tabla, id,
             timestamp_str, nombre_archivo);

    char app_dir[512] = {0};
    app_build_path(app_dir, sizeof(app_dir), NULL, "imagenes");
    MKDIR(app_dir);

    char dest_ruta[600];
    app_build_path(dest_ruta, sizeof(dest_ruta), app_dir, dest_nombre);

    if (!app_copy_binary_file(origen, dest_ruta))
    {
        return 0;
    }

    char rel_path[400];
    snprintf(rel_path, sizeof(rel_path), "imagenes/%s", dest_nombre);

    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE %s SET imagen_ruta = ? WHERE id = ?",
             tabla);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, rel_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int result = sqlite3_step(stmt) == SQLITE_DONE;
    db_stmt_release(stmt);

    return result;
}
