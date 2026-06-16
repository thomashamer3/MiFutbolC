#include "db.h"
#include "export.h"
#include "settings.h"
#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <direct.h>
#include <shlobj.h>
#else
#include "compat_windows.h"
#include "direct.h"
#include "shlobj.h"
#endif

#ifdef _WIN32
#define MKDIR(path) _mkdir(path)
#define STRDUP _strdup
#define DB_PATH_SEP "\\"

static int directorio_existe(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return 0;
    }

    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#define STRDUP strdup
#define DB_PATH_SEP "/"

static int directorio_existe(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

static void sanitize_filename_token(char *token)
{
    if (!token)
    {
        return;
    }

    for (size_t i = 0; token[i] != '\0'; i++)
    {
        unsigned char ch = (unsigned char)token[i];
        if (!(isalnum(ch) || ch == '_' || ch == '-'))
        {
            token[i] = '_';
        }
    }
}

static void generate_salt_hex(char *salt_out, size_t out_size)
{
    enum
    {
        SALT_BYTES = 16
    };
    char hex_buf[33];
    auth_generate_salt_hex(hex_buf, sizeof(hex_buf));
    if (!salt_out || out_size == 0)
    {
        return;
    }
    strncpy_s(salt_out, out_size, hex_buf, _TRUNCATE);
}

static void build_password_hash(const char *plain_password, const char *salt_hex, char *hash_out,
                                size_t out_size)
{
    auth_build_password_hash(plain_password, salt_hex, hash_out, out_size);
}

#ifdef _WIN32
static char error_buf[256];
#endif

/** Puntero global a la base de datos SQLite */
sqlite3 *db = NULL;

/** Directorio donde se almacena la base de datos */
static char DB_DIR[1024];

/** Ruta completa al archivo de la base de datos */
static char DB_PATH[1024];

/** Ruta completa al archivo de log */
static char LOG_PATH[1024];

/** Directorio de exportaciones */
static char EXPORT_DIR[1024];

/** Directorio de importaciones */
static char IMPORT_DIR[1024];

/** Directorio de imagenes */
static char IMAGES_DIR[1024];

/** Directorio de musica */
static char MUSIC_DIR[1024];

/** Usuario local activo para enrutar la base por perfil */
static char ACTIVE_USER[128];

typedef enum
{
    COPY_OK = 0,
    COPY_SRC_ERROR,
    COPY_DST_ERROR
} CopyResult;

static void build_timestamp(char *buffer, size_t size)
{
    if (!buffer || size == 0)
    {
        return;
    }

    time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        snprintf(buffer, size, "%s", "1970-01-01 00:00:00");
        return;
    }

    struct tm local_tm;
#ifdef _WIN32
    if (localtime_s(&local_tm, &now) != 0)
    {
        snprintf(buffer, size, "%s", "1970-01-01 00:00:00");
        return;
    }
#else
    if (!localtime_r(&now, &local_tm))
    {
        snprintf(buffer, size, "%s", "1970-01-01 00:00:00");
        return;
    }
#endif

    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &local_tm);
}

static FILE *app_fopen(const char *path, const char *mode);
static int execute_sql_statements(const char *const *statements, int *failed_index);

static FILE *g_log_fp = NULL;
static int g_log_initialized = 0;

void app_log_init(void)
{
    if (g_log_initialized)
    {
        return;
    }
    g_log_initialized = 1;
    if (LOG_PATH[0] == '\0')
    {
        return;
    }
    g_log_fp = app_fopen(LOG_PATH, "a");
    if (g_log_fp)
    {
        setvbuf(g_log_fp, NULL, _IOLBF, 256);
    }
}

void app_log_close(void)
{
    if (g_log_fp)
    {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
    g_log_initialized = 0;
}

static void app_log_write(const char *level, const char *component, const char *message)
{
    if (!level || !component || !message)
    {
        return;
    }

    if (!g_log_initialized)
    {
        app_log_init();
    }

    if (!g_log_fp)
    {
        return;
    }

    char timestamp[32] = {0};
    build_timestamp(timestamp, sizeof(timestamp));
    fprintf(g_log_fp, "[%s] [%s] [%s] %s\n", timestamp, level, component, message);
}

void app_log_event(const char *component, const char *message)
{
    app_log_write("INFO", component, message);
}

static char log_buf_[1024];

#define LOG_ERROR_FMT(component, fmt, ...)                                                         \
    do                                                                                             \
    {                                                                                              \
        printf(fmt "\n", __VA_ARGS__);                                                             \
        snprintf(log_buf_, sizeof(log_buf_), fmt, __VA_ARGS__);                                    \
        app_log_write("ERROR", component, log_buf_);                                               \
    } while (0)

#define LOG_ERROR_CONSOLE_LOG_FMT(component, console_fmt, log_fmt, ...)                            \
    do                                                                                             \
    {                                                                                              \
        printf(console_fmt "\n", __VA_ARGS__);                                                     \
        snprintf(log_buf_, sizeof(log_buf_), log_fmt, __VA_ARGS__);                                \
        app_log_write("ERROR", component, log_buf_);                                               \
    } while (0)

#define LOG_ERROR_MSG(component, console_msg, log_msg)                                             \
    do                                                                                             \
    {                                                                                              \
        printf("%s\n", console_msg);                                                               \
        snprintf(log_buf_, sizeof(log_buf_), "%s", log_msg);                                       \
        app_log_write("ERROR", component, log_buf_);                                               \
    } while (0)

#define LOG_ERROR_CONSOLE_MSG_LOG_FMT(component, console_msg, log_fmt, ...)                        \
    do                                                                                             \
    {                                                                                              \
        printf("%s\n", console_msg);                                                               \
        snprintf(log_buf_, sizeof(log_buf_), log_fmt, __VA_ARGS__);                                \
        app_log_write("ERROR", component, log_buf_);                                               \
    } while (0)

static int asegurar_directorio(const char *path, const char *nombre)
{
    errno = 0;
    int mkdir_rc = MKDIR(path);
    int already_exists = (mkdir_rc != 0 && errno == EEXIST);

    if (mkdir_rc != 0 && !already_exists)
    {
#ifdef _WIN32
        strerror_s(error_buf, sizeof(error_buf), errno);
        printf("Error creando directorio %s: %s\n", nombre, error_buf);
        snprintf(log_buf_, sizeof(log_buf_), "No se pudo crear directorio %.120s (%.420s): %.420s",
                 nombre, path, error_buf);
        app_log_write("ERROR", "FS", log_buf_);
#else
        printf("Error creando directorio %s: %s\n", nombre, strerror(errno));
        snprintf(log_buf_, sizeof(log_buf_), "No se pudo crear directorio %.120s (%.420s): %.420s",
                 nombre, path, strerror(errno));
        app_log_write("ERROR", "FS", log_buf_);
#endif
        return 0;
    }
    if (!already_exists)
    {
        snprintf(log_buf_, sizeof(log_buf_), "Directorio disponible: %.120s (%.860s)", nombre,
                 path);
        app_log_write("INFO", "FS", log_buf_);
    }
    return 1;
}

static FILE *app_fopen(const char *path, const char *mode)
{
    if (!path || !mode)
    {
        return NULL;
    }

#ifdef _WIN32
    FILE *f = NULL;
    if (fopen_s(&f, path, mode) != 0)
    {
        return NULL;
    }
    return f;
#else
    return fopen(path, mode);
#endif
}

static void ejecutar_alter_table_group(const char *const *statements, const char *component)
{
    const char *dup_col_msg = "duplicate column name";
    const char *no_table_msg = "no such table";
    for (int i = 0; statements[i] != NULL; i++)
    {
        char *errmsg = NULL;
        int rc = sqlite3_exec(db, statements[i], NULL, NULL, &errmsg);
        int error_esperado = (errmsg != NULL) && ((strstr(errmsg, dup_col_msg) != NULL) ||
                             (strstr(errmsg, no_table_msg) != NULL));
        if (rc != SQLITE_OK && errmsg != NULL && !error_esperado)
        {
            snprintf(log_buf_, sizeof(log_buf_), "Migracion %s con error: %.320s | %.520s",
                     component, statements[i], errmsg);
            app_log_write("WARN", "DB", log_buf_);
        }
        sqlite3_free(errmsg);
    }
}

#ifdef _WIN32
static int configurar_directorio_documentos(const char *subdir, char *out_dir, size_t out_size,
        const char *nombre_principal, const char *nombre_subdir)
{
    char documents_path[MAX_PATH];
    /*
     * Se utiliza SHGetFolderPathA con CSIDL_PERSONAL para obtener la ruta a "Mis
     * Documentos". Esta ubicacion se reserva para archivos que el usuario debe
     * manipular manualmente, como las exportaciones e importaciones de datos.
     */
    if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, documents_path) != S_OK)
    {
        printf("Error obteniendo Documents path\n");
        app_log_write("ERROR", "PATHS", "No se pudo resolver Documents path");
        return 0;
    }

    char base_path[MAX_PATH];
    strcpy_s(base_path, sizeof(base_path), documents_path);
    strcat_s(base_path, sizeof(base_path), "\\MiFutbolC");
    if (!asegurar_directorio(base_path, nombre_principal))
    {
        return 0;
    }

    memset(out_dir, 0, out_size);
    strcpy_s(out_dir, out_size, documents_path);
    strcat_s(out_dir, out_size, "\\MiFutbolC\\");
    strcat_s(out_dir, out_size, subdir);

    if (!asegurar_directorio(out_dir, nombre_subdir))
    {
        return 0;
    }

    return 1;
}
#endif

static int configurar_directorio_usuario(const char *dir_preferido_local,
        const char *dir_legado_local,
        const char *subdir_documentos, char *out_dir,
        size_t out_size, const char *nombre_subdir)
{
    if (!out_dir || out_size == 0)
    {
        return 0;
    }

    if (out_dir[0] != '\0')
    {
        return 1;
    }

    if (dir_preferido_local && dir_preferido_local[0] != '\0')
    {
        strcpy_s(out_dir, out_size, dir_preferido_local);
        if (asegurar_directorio(out_dir, nombre_subdir))
        {
            return 1;
        }
        out_dir[0] = '\0';
    }

    if (dir_legado_local && dir_legado_local[0] != '\0' && directorio_existe(dir_legado_local))
    {
        strcpy_s(out_dir, out_size, dir_legado_local);
        if (asegurar_directorio(out_dir, nombre_subdir))
        {
            return 1;
        }
        out_dir[0] = '\0';
    }

#ifdef _WIN32
    if (subdir_documentos && subdir_documentos[0] != '\0')
    {
        return configurar_directorio_documentos(subdir_documentos, out_dir, out_size,
                                                "MiFutbolC en Documents", nombre_subdir);
    }
    return 0;
#else
    return 0;
#endif
}

static CopyResult copiar_archivo(const char *source_path, const char *dest_path)
{
    FILE *src = app_fopen(source_path, "rb");
    if (!src)
    {
        return COPY_SRC_ERROR;
    }

    FILE *dst = app_fopen(dest_path, "wb");
    if (!dst)
    {
        fclose(src);
        return COPY_DST_ERROR;
    }

    char buffer[8192];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);

    return COPY_OK;
}

static int append_str(char *dest, size_t *used, size_t cap, const char *str)
{
    size_t len = strlen_s(str, cap - *used);
    if (*used + len >= cap)
    {
        return 0;
    }

    memcpy(dest + *used, str, len);
    *used += len;
    dest[*used] = '\0';
    return 1;
}

static int ejecutar_stmt_texto(const char *sql, const char *const *params, size_t params_count,
                               int *rows_changed)
{
    sqlite3_stmt *stmt = NULL;

    if (!sql || !db)
    {
        return 0;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        return 0;
    }

    for (size_t i = 0; i < params_count; i++)
    {
        const char *valor = (params && params[i]) ? params[i] : "";
        if (sqlite3_bind_text(stmt, (int)i + 1, valor, -1, SQLITE_STATIC) != SQLITE_OK)
        {
            sqlite3_finalize(stmt);
            return 0;
        }
    }

    int ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (ok && rows_changed)
    {
        *rows_changed = sqlite3_changes(db);
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int db_query_single_text(const char *sql, char *out, size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (!sql || !db)
    {
        return 0;
    }

    if (out && out_size > 0)
    {
        out[0] = '\0';
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *valor = (const char *)sqlite3_column_text(stmt, 0);
        if (valor)
        {
            if (out && out_size > 0)
            {
                ok = (strncpy_s(out, out_size, valor, _TRUNCATE) == 0);
            }
            else
            {
                ok = 1;
            }
        }
    }

    sqlite3_finalize(stmt);
    return ok;
}

int db_set_active_user(const char *username)
{
    size_t len;

    if (!username)
    {
        return 0;
    }

    len = strlen_s(username, sizeof(ACTIVE_USER));
    if (len == 0 || len >= sizeof(ACTIVE_USER))
    {
        return 0;
    }

    memset(ACTIVE_USER, 0, sizeof(ACTIVE_USER));
    strcpy_s(ACTIVE_USER, sizeof(ACTIVE_USER), username);
    sanitize_filename_token(ACTIVE_USER);
    return ACTIVE_USER[0] != '\0';
}

const char *db_get_active_user(void)
{
    return ACTIVE_USER;
}

static int setup_database_paths()
{
    char db_filename[256];
    char log_filename[256];

    if (ACTIVE_USER[0] != '\0')
    {
        snprintf(db_filename, sizeof(db_filename), "mifutbol_%s.db", ACTIVE_USER);
        snprintf(log_filename, sizeof(log_filename), "mifutbol_%s.log", ACTIVE_USER);
    }
    else
    {
        strcpy_s(db_filename, sizeof(db_filename), "mifutbol.db");
        strcpy_s(log_filename, sizeof(log_filename), "mifutbol.log");
    }

#ifdef _WIN32
    // Usar AppData\Local para la base de datos (oculta, interna)
    char appdata_path[MAX_PATH];
    /*
     * La base de datos se almacena en CSIDL_LOCAL_APPDATA para garantizar que
     * no sea borrada accidentalmente por el usuario y que el sistema tenga
     * permisos de escritura consistentes sin requerir privilegios de
     * administrador.
     */
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata_path) != S_OK)
    {
        printf("Error obteniendo AppData path\n");
        return 0;
    }
    snprintf(DB_DIR, sizeof(DB_DIR), "%s\\MiFutbolC\\data", appdata_path);
    snprintf(DB_PATH, sizeof(DB_PATH), "%s\\MiFutbolC\\data\\%s", appdata_path, db_filename);
    snprintf(LOG_PATH, sizeof(LOG_PATH), "%s\\MiFutbolC\\data\\%s", appdata_path, log_filename);

    // Crear directorios si no existen
    char temp_path[1024];
    snprintf(temp_path, sizeof(temp_path), "%s\\MiFutbolC", appdata_path);
    if (!asegurar_directorio(temp_path, "MiFutbolC"))
    {
        return 0;
    }

    if (!asegurar_directorio(DB_DIR, "data"))
    {
        return 0;
    }
#else
    // Para otros sistemas operativos, usar directorio actual
    snprintf(DB_DIR, sizeof(DB_DIR), "./data");
    snprintf(DB_PATH, sizeof(DB_PATH), "./data/%s", db_filename);
    snprintf(LOG_PATH, sizeof(LOG_PATH), "./data/%s", log_filename);

    // Crear directorio si no existe
    if (!asegurar_directorio(DB_DIR, "data"))
    {
        return 0;
    }
#endif

    snprintf(log_buf_, sizeof(log_buf_), "Ruta de datos: %.1000s", DB_DIR);
    app_log_write("INFO", "PATHS", log_buf_);
    snprintf(log_buf_, sizeof(log_buf_), "Ruta de DB: %.1003s", DB_PATH);
    app_log_write("INFO", "PATHS", log_buf_);
    snprintf(log_buf_, sizeof(log_buf_), "Ruta de log: %.1002s", LOG_PATH);
    app_log_write("INFO", "PATHS", log_buf_);
    return 1;
}

static int create_database_connection()
{
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK)
    {
        printf("Error abriendo DB: %s\n", sqlite3_errmsg(db));
        snprintf(log_buf_, sizeof(log_buf_), "Error abriendo DB %.700s: %.280s", DB_PATH,
                 sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_buf_);
        return 0;
    }
    snprintf(log_buf_, sizeof(log_buf_), "Conexion SQLite abierta en %.996s", DB_PATH);
    app_log_write("INFO", "DB", log_buf_);
    return 1;
}

static int apply_database_tuning()
{
    if (sqlite3_busy_timeout(db, 5000) != SQLITE_OK)
    {
        snprintf(log_buf_, sizeof(log_buf_), "No se pudo configurar busy timeout: %s",
                 sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_buf_);
        return 0;
    }

    const char *pragma_statements[] = {"PRAGMA journal_mode = WAL;",
                                       "PRAGMA synchronous = NORMAL;",
                                       "PRAGMA temp_store = MEMORY;",
                                       "PRAGMA cache_size = -32768;",
                                       "PRAGMA mmap_size = 268435456;",
                                       "PRAGMA journal_size_limit = 4194304;",
                                       "PRAGMA cache_spill = OFF;",
                                       "PRAGMA automatic_index = ON;",
                                       NULL
                                      };

    int failed_index = -1;
    if (!execute_sql_statements(pragma_statements, &failed_index))
    {
        snprintf(log_buf_, sizeof(log_buf_), "Fallo PRAGMA '%s': %s",
                 pragma_statements[failed_index], sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_buf_);
        return 0;
    }

    return 1;
}

static int execute_sql_statements(const char *const *statements, int *failed_index)
{
    if (failed_index)
    {
        *failed_index = -1;
    }

    if (!statements)
    {
        return 1;
    }

    for (int i = 0; statements[i] != NULL; i++)
    {
        if (sqlite3_exec(db, statements[i], NULL, NULL, NULL) != SQLITE_OK)
        {
            if (failed_index)
            {
                *failed_index = i;
            }
            return 0;
        }
    }

    return 1;
}

enum
{
    DB_VERSION_SCHEMA = 1,
    DB_VERSION_CAMISETA_COLS = 2,
    DB_VERSION_CANCHA_COLS = 3,
    DB_VERSION_PARTIDO_COLS = 4,
    DB_VERSION_INDEXES = 5,
    DB_VERSION_CANCHA_GRABACION = 6,
    DB_VERSION_ADDITIONAL_INDEXES = 7,
    DB_VERSION_PERFORMANCE_INDEXES = 8,
    DB_VERSION_PARTIDO_ATAJASTE = 9,
    DB_VERSION_PARTIDO_ATAJASTE_FIX = 10,
    DB_VERSION_CURRENT = 10
};

static int get_user_version(int *out_version)
{
    sqlite3_stmt *stmt = NULL;
    if (!out_version)
    {
        return 0;
    }

    *out_version = 0;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *out_version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return 1;
}

static int set_user_version(int version)
{
    char sql[64];
    snprintf(sql, sizeof(sql), "PRAGMA user_version = %d;", version);
    return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

static void backfill_mes_anio_once(void)
{
    int current_version = 0;
    if (!get_user_version(&current_version))
    {
        app_log_write("WARN", "DB",
                      "No se pudo leer PRAGMA user_version; se omite backfill de mes_anio");
        return;
    }

    if (current_version >= DB_VERSION_SCHEMA)
    {
        return;
    }

    if (sqlite3_exec(db,
                     "UPDATE partido SET mes_anio = "
                     "CASE "
                     "WHEN length(fecha_hora) >= 10 AND substr(fecha_hora, 5, 1) = '-' "
                     "THEN substr(fecha_hora, 1, 7) "
                     "ELSE substr(fecha_hora, 7, 4) || '-' || substr(fecha_hora, 4, 2) "
                     "END "
                     "WHERE (mes_anio = '' OR mes_anio IS NULL) AND length(fecha_hora) >= "
                     "7;",
                     NULL, NULL, NULL) != SQLITE_OK)
    {
        snprintf(log_buf_, sizeof(log_buf_), "Fallo backfill mes_anio: %s", sqlite3_errmsg(db));
        app_log_write("WARN", "DB", log_buf_);
        return;
    }

    if (!set_user_version(DB_VERSION_SCHEMA))
    {
        snprintf(log_buf_, sizeof(log_buf_),
                 "No se pudo actualizar user_version tras backfill mes_anio: %s",
                 sqlite3_errmsg(db));
        app_log_write("WARN", "DB", log_buf_);
        return;
    }

    app_log_write("INFO", "DB", "Backfill mes_anio aplicado una sola vez");
}

/* Definiciones compartidas para evitar duplicar columnas entre CREATE TABLE y
 * ALTER TABLE. */
#define COL_CAMISETA_COLOR_PRINCIPAL "color_principal TEXT DEFAULT ''"
#define COL_CAMISETA_COLOR_SECUNDARIO "color_secundario TEXT DEFAULT ''"
#define COL_CAMISETA_MARCA "marca TEXT DEFAULT ''"
#define COL_CAMISETA_MODELO "modelo TEXT DEFAULT ''"
#define COL_CAMISETA_TEMPORADA "temporada TEXT DEFAULT ''"
#define COL_CAMISETA_ESTADO_FISICO "estado_fisico TEXT DEFAULT ''"
#define COL_CAMISETA_FECHA_COMPRA "fecha_compra TEXT DEFAULT ''"
#define COL_CAMISETA_COSTO_CENTAVOS "costo_centavos INTEGER DEFAULT 0"
#define COL_CAMISETA_OBSERVACIONES "observaciones TEXT DEFAULT ''"
#define COL_CAMISETA_PROVEEDOR "proveedor TEXT DEFAULT ''"
#define COL_CAMISETA_FUE_REGALO "fue_regalo INTEGER DEFAULT 0"
#define COL_CAMISETA_REGALO_DE "regalo_de TEXT DEFAULT ''"
#define COL_CAMISETA_IMAGEN_RUTA "imagen_ruta TEXT DEFAULT ''"
#define COL_CAMISETA_SORTEADA "sorteada INTEGER DEFAULT 0"
#define COL_CAMISETA_ACTIVA "activa INTEGER DEFAULT 1"

#define COL_CANCHA_TELEFONO "telefono TEXT DEFAULT ''"
#define COL_CANCHA_DIRECCION "direccion TEXT DEFAULT ''"
#define COL_CANCHA_LOCALIDAD "localidad TEXT DEFAULT ''"
#define COL_CANCHA_TIPO_CANCHA_CODIGO "tipo_cancha_codigo INTEGER DEFAULT 0"
#define COL_CANCHA_SUPERFICIE_CODIGO "superficie_codigo INTEGER DEFAULT 0"
#define COL_CANCHA_TECHADA_ESTADO_CODIGO "techada_estado_codigo INTEGER DEFAULT 2"
#define COL_CANCHA_TIENE_ILUMINACION "tiene_iluminacion INTEGER DEFAULT 0"
#define COL_CANCHA_HORARIO_APERTURA_MIN "horario_apertura_min INTEGER DEFAULT -1"
#define COL_CANCHA_HORARIO_CIERRE_MIN "horario_cierre_min INTEGER DEFAULT -1"
#define COL_CANCHA_PRECIO_HORA_DIA_CENTAVOS "precio_hora_dia_centavos INTEGER DEFAULT 0"
#define COL_CANCHA_PRECIO_HORA_NOCHE_CENTAVOS "precio_hora_noche_centavos INTEGER DEFAULT 0"
#define COL_CANCHA_TIENE_VESTUARIOS "tiene_vestuarios INTEGER DEFAULT 0"
#define COL_CANCHA_TIENE_DUCHAS "tiene_duchas INTEGER DEFAULT 0"
#define COL_CANCHA_TIENE_BUFFET "tiene_buffet INTEGER DEFAULT 0"
#define COL_CANCHA_TIENE_ESTACIONAMIENTO "tiene_estacionamiento INTEGER DEFAULT 0"
#define COL_CANCHA_CANTIDAD_CANCHAS "cantidad_canchas INTEGER DEFAULT 1"
#define COL_CANCHA_ESTADO "estado TEXT DEFAULT ''"
#define COL_CANCHA_DESCRIPCION "descripcion TEXT DEFAULT ''"
#define COL_CANCHA_DIRECCION_CALLE "direccion_calle TEXT DEFAULT ''"
#define COL_CANCHA_DIRECCION_ZONA "direccion_zona TEXT DEFAULT ''"
#define COL_CANCHA_TIPO_CANCHA "tipo_cancha TEXT DEFAULT ''"
#define COL_CANCHA_PRECIO_HORA "precio_hora REAL DEFAULT 0"
#define COL_CANCHA_SUPERFICIE "superficie TEXT DEFAULT ''"
#define COL_CANCHA_TECHADA_ESTADO "techada_estado TEXT DEFAULT 'NO SE'"
#define COL_CANCHA_HORARIO "horario TEXT DEFAULT ''"
#define COL_CANCHA_CONTACTO_ALT "contacto_alt TEXT DEFAULT ''"
#define COL_CANCHA_IMAGEN_RUTA "imagen_ruta TEXT DEFAULT ''"
#define COL_CANCHA_ACTIVA "activa INTEGER DEFAULT 1"
#define COL_CANCHA_TIENE_GRABACION "tiene_grabacion INTEGER DEFAULT 0"

#define COL_PARTIDO_RESULTADO "resultado INTEGER DEFAULT 0"
#define COL_PARTIDO_RENDIMIENTO_GENERAL "rendimiento_general INTEGER DEFAULT 0"
#define COL_PARTIDO_CANSANCIO "cansancio INTEGER DEFAULT 0"
#define COL_PARTIDO_ESTADO_ANIMO "estado_animo INTEGER DEFAULT 0"
#define COL_PARTIDO_COMENTARIO_PERSONAL "comentario_personal TEXT DEFAULT ''"
#define COL_PARTIDO_CLIMA "clima INTEGER DEFAULT 0"
#define COL_PARTIDO_DIA "dia INTEGER DEFAULT 0"
#define COL_PARTIDO_PRECIO "precio INTEGER DEFAULT 0"
#define COL_PARTIDO_TIPO_PARTIDO "tipo_partido INTEGER DEFAULT 1"
#define COL_PARTIDO_RIVAL_NOMBRE "rival_nombre TEXT DEFAULT ''"
#define COL_PARTIDO_TIPO_RIVAL "tipo_rival TEXT DEFAULT ''"
#define COL_PARTIDO_POSICION_JUGADA "posicion_jugada TEXT DEFAULT ''"
#define COL_PARTIDO_MINUTOS_JUGADOS "minutos_jugados INTEGER DEFAULT 0"
#define COL_PARTIDO_INTENSIDAD "intensidad INTEGER DEFAULT 0"
#define COL_PARTIDO_ESFUERZO_PERCIBIDO "esfuerzo_percibido INTEGER DEFAULT 0"
#define COL_PARTIDO_CONDICION_CANCHA "condicion_cancha TEXT DEFAULT ''"
#define COL_PARTIDO_ARBITRAJE "arbitraje TEXT DEFAULT ''"
#define COL_PARTIDO_EVENTOS_CLAVE "eventos_clave TEXT DEFAULT ''"
#define COL_PARTIDO_RATING_TECNICO "rating_tecnico INTEGER DEFAULT 0"
#define COL_PARTIDO_RATING_FISICO "rating_fisico INTEGER DEFAULT 0"
#define COL_PARTIDO_RATING_MENTAL "rating_mental INTEGER DEFAULT 0"
#define COL_PARTIDO_ESTADO_CANCHA "estado_cancha INTEGER DEFAULT 0"
#define COL_PARTIDO_GOLES_EQUIPO "goles_equipo INTEGER DEFAULT -1"
#define COL_PARTIDO_GOLES_RIVAL "goles_rival INTEGER DEFAULT -1"
#define COL_PARTIDO_FORMATO "formato_partido TEXT DEFAULT ''"
#define COL_PARTIDO_TARJETA "tarjeta INTEGER DEFAULT 1"
#define COL_PARTIDO_GOLES_EN_CONTRA "goles_en_contra INTEGER DEFAULT 0"
#define COL_PARTIDO_DOLOR_FISICO "dolor_fisico INTEGER DEFAULT 0"
#define COL_PARTIDO_TEMPERATURA_C "temperatura_c REAL DEFAULT NULL"
#define COL_PARTIDO_ARBITRAJE_SCORE "arbitraje_score INTEGER DEFAULT 0"
#define COL_PARTIDO_LO_MEJOR "lo_mejor TEXT DEFAULT ''"
#define COL_PARTIDO_QUE_MEJORAR "que_mejorar TEXT DEFAULT ''"
#define COL_PARTIDO_TAGS "tags TEXT DEFAULT ''"
#define COL_PARTIDO_GOLES_DETALLE "goles_detalle TEXT DEFAULT ''"
#define COL_PARTIDO_ASISTENCIAS_DETALLE "asistencias_detalle TEXT DEFAULT ''"
#define COL_PARTIDO_ATAJASTE_TODO "atajaste_todo_el_partido INTEGER DEFAULT 1"

static int create_database_schema()
{
    const char *schema_statements[] =
    {
        "CREATE TABLE IF NOT EXISTS camiseta ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " " COL_CAMISETA_COLOR_PRINCIPAL ","
        " " COL_CAMISETA_COLOR_SECUNDARIO ","
        " " COL_CAMISETA_MARCA ","
        " " COL_CAMISETA_MODELO ","
        " " COL_CAMISETA_TEMPORADA ","
        " " COL_CAMISETA_ESTADO_FISICO ","
        " " COL_CAMISETA_FECHA_COMPRA ","
        " " COL_CAMISETA_COSTO_CENTAVOS ","
        " " COL_CAMISETA_OBSERVACIONES ","
        " " COL_CAMISETA_PROVEEDOR ","
        " " COL_CAMISETA_FUE_REGALO ","
        " " COL_CAMISETA_REGALO_DE ","
        " " COL_CAMISETA_IMAGEN_RUTA ","
        " " COL_CAMISETA_SORTEADA ","
        " " COL_CAMISETA_ACTIVA ");",

        "CREATE TABLE IF NOT EXISTS coleccion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL UNIQUE,"
        " descripcion TEXT DEFAULT '',"
        " fecha_creacion TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);",

        "CREATE TABLE IF NOT EXISTS inventario_item ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " tipo INTEGER NOT NULL,"
        " nombre TEXT DEFAULT '',"
        " estado INTEGER NOT NULL DEFAULT 0,"
        " valor REAL DEFAULT 0,"
        " fecha_compra TEXT DEFAULT '',"
        " camiseta_id INTEGER UNIQUE,"
        " CHECK(tipo IN (1, 2, 3)),"
        " CHECK(estado IN (0, 1)),"
        " FOREIGN KEY(camiseta_id) REFERENCES camiseta(id) ON DELETE SET NULL);",

        "CREATE TABLE IF NOT EXISTS coleccion_inventario ("
        " coleccion_id INTEGER NOT NULL,"
        " inventario_id INTEGER NOT NULL,"
        " fecha_asociacion TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " PRIMARY KEY(coleccion_id, inventario_id),"
        " FOREIGN KEY(coleccion_id) REFERENCES coleccion(id) ON DELETE CASCADE,"
        " FOREIGN KEY(inventario_id) REFERENCES inventario_item(id) ON DELETE "
        "CASCADE);",

        "CREATE INDEX IF NOT EXISTS idx_inventario_tipo ON "
        "inventario_item(tipo);",
        "CREATE INDEX IF NOT EXISTS idx_coleccion_inventario_item ON "
        "coleccion_inventario(inventario_id);",

        "INSERT OR IGNORE INTO coleccion(nombre, descripcion) VALUES"
        " ('Equipamiento actual', 'Items que usas actualmente.'),"
        " ('Historico', 'Coleccion de items de temporadas anteriores.'),"
        " ('Vendidos', 'Items vendidos o fuera del inventario activo.');",

        "CREATE TABLE IF NOT EXISTS cancha ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " " COL_CANCHA_TELEFONO ","
        " " COL_CANCHA_DIRECCION ","
        " " COL_CANCHA_LOCALIDAD ","
        " " COL_CANCHA_TIPO_CANCHA_CODIGO ","
        " " COL_CANCHA_SUPERFICIE_CODIGO ","
        " " COL_CANCHA_TECHADA_ESTADO_CODIGO ","
        " " COL_CANCHA_TIENE_ILUMINACION ","
        " " COL_CANCHA_HORARIO_APERTURA_MIN ","
        " " COL_CANCHA_HORARIO_CIERRE_MIN ","
        " " COL_CANCHA_PRECIO_HORA_DIA_CENTAVOS ","
        " " COL_CANCHA_PRECIO_HORA_NOCHE_CENTAVOS ","
        " " COL_CANCHA_TIENE_VESTUARIOS ","
        " " COL_CANCHA_TIENE_DUCHAS ","
        " " COL_CANCHA_TIENE_BUFFET ","
        " " COL_CANCHA_TIENE_ESTACIONAMIENTO ","
        " " COL_CANCHA_CANTIDAD_CANCHAS ","
        " " COL_CANCHA_ESTADO ","
        " " COL_CANCHA_DESCRIPCION ","
        " " COL_CANCHA_DIRECCION_CALLE ","
        " " COL_CANCHA_DIRECCION_ZONA ","
        " " COL_CANCHA_TIPO_CANCHA ","
        " " COL_CANCHA_PRECIO_HORA ","
        " " COL_CANCHA_SUPERFICIE ","
        " " COL_CANCHA_TECHADA_ESTADO ","
        " " COL_CANCHA_HORARIO ","
        " " COL_CANCHA_CONTACTO_ALT ","
        " " COL_CANCHA_IMAGEN_RUTA ","
        " " COL_CANCHA_TIENE_GRABACION ","
        " " COL_CANCHA_ACTIVA ");",

        "CREATE TABLE IF NOT EXISTS partido ("
        " id INTEGER PRIMARY KEY,"
        " cancha_id INTEGER NOT NULL,"
        " fecha_hora TEXT NOT NULL,"
        " goles INTEGER NOT NULL,"
        " asistencias INTEGER NOT NULL,"
        " camiseta_id INTEGER NOT NULL,"
        " " COL_PARTIDO_RESULTADO ","
        " " COL_PARTIDO_RENDIMIENTO_GENERAL ","
        " " COL_PARTIDO_CANSANCIO ","
        " " COL_PARTIDO_ESTADO_ANIMO ","
        " " COL_PARTIDO_COMENTARIO_PERSONAL ","
        " " COL_PARTIDO_CLIMA ","
        " " COL_PARTIDO_DIA ","
        " " COL_PARTIDO_PRECIO ","
        " " COL_PARTIDO_TIPO_PARTIDO ","
        " " COL_PARTIDO_RIVAL_NOMBRE ","
        " " COL_PARTIDO_TIPO_RIVAL ","
        " " COL_PARTIDO_POSICION_JUGADA ","
        " " COL_PARTIDO_MINUTOS_JUGADOS ","
        " " COL_PARTIDO_INTENSIDAD ","
        " " COL_PARTIDO_ESFUERZO_PERCIBIDO ","
        " " COL_PARTIDO_CONDICION_CANCHA ","
        " " COL_PARTIDO_ARBITRAJE ","
        " " COL_PARTIDO_EVENTOS_CLAVE ","
        " " COL_PARTIDO_RATING_TECNICO ","
        " " COL_PARTIDO_RATING_FISICO ","
        " " COL_PARTIDO_RATING_MENTAL ","
        " " COL_PARTIDO_ESTADO_CANCHA ","
        " " COL_PARTIDO_GOLES_EQUIPO ","
        " " COL_PARTIDO_GOLES_RIVAL ","
        " " COL_PARTIDO_FORMATO ","
        " " COL_PARTIDO_TARJETA ","
        " " COL_PARTIDO_GOLES_EN_CONTRA ","
        " " COL_PARTIDO_DOLOR_FISICO ","
        " " COL_PARTIDO_TEMPERATURA_C ","
        " " COL_PARTIDO_ARBITRAJE_SCORE ","
        " " COL_PARTIDO_LO_MEJOR ","
        " " COL_PARTIDO_QUE_MEJORAR ","
        " " COL_PARTIDO_TAGS ","
        " " COL_PARTIDO_GOLES_DETALLE ","
        " " COL_PARTIDO_ASISTENCIAS_DETALLE ","
        " " COL_PARTIDO_ATAJASTE_TODO ","
        " FOREIGN KEY(cancha_id) REFERENCES cancha(id),"
        " FOREIGN KEY(camiseta_id) REFERENCES camiseta(id));",

        "CREATE TABLE IF NOT EXISTS lesion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " jugador TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " descripcion TEXT NOT NULL,"
        " fecha TEXT NOT NULL,"
        " camiseta_id INTEGER NOT NULL,"
        " estado TEXT DEFAULT 'Activa',"
        " FOREIGN KEY(camiseta_id) REFERENCES camiseta(id));",

        "CREATE TABLE IF NOT EXISTS usuario ("
        " id INTEGER PRIMARY KEY,"
        " nombre TEXT NOT NULL,"
        " password_salt TEXT DEFAULT '',"
        " password_hash TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS equipo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " tipo INTEGER NOT NULL,"
        " tipo_futbol INTEGER NOT NULL,"
        " num_jugadores INTEGER NOT NULL,"
        " partido_id INTEGER DEFAULT -1,"
        " activa INTEGER DEFAULT 1,"
        " imagen_ruta TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS jugador ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " equipo_id INTEGER NOT NULL,"
        " nombre TEXT NOT NULL,"
        " numero INTEGER NOT NULL,"
        " posicion INTEGER NOT NULL,"
        " es_capitan INTEGER NOT NULL,"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id));",

        "CREATE TABLE IF NOT EXISTS torneo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " tiene_equipo_fijo INTEGER NOT NULL,"
        " equipo_fijo_id INTEGER DEFAULT -1,"
        " cantidad_equipos INTEGER NOT NULL,"
        " tipo_torneo INTEGER NOT NULL,"
        " formato_torneo INTEGER NOT NULL,"
        " fase_actual TEXT DEFAULT 'Fase de Grupos');",

        "CREATE TABLE IF NOT EXISTS equipo_torneo ("
        " torneo_id INTEGER NOT NULL,"
        " equipo_id INTEGER NOT NULL,"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id),"
        " PRIMARY KEY(torneo_id, equipo_id));",

        "CREATE TABLE IF NOT EXISTS equipo_torneo_nombre ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " torneo_id INTEGER NOT NULL,"
        " nombre TEXT NOT NULL,"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id));",

        "CREATE TABLE IF NOT EXISTS partido_torneo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " torneo_id INTEGER NOT NULL,"
        " equipo1_id INTEGER NOT NULL,"
        " equipo2_id INTEGER NOT NULL,"
        " fecha TEXT,"
        " goles_equipo1 INTEGER DEFAULT 0,"
        " goles_equipo2 INTEGER DEFAULT 0,"
        " estado TEXT,"
        " fase TEXT DEFAULT 'Fase de Grupos',"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(equipo1_id) REFERENCES equipo(id),"
        " FOREIGN KEY(equipo2_id) REFERENCES equipo(id));",

        "CREATE TABLE IF NOT EXISTS equipo_torneo_estadisticas ("
        " torneo_id INTEGER NOT NULL,"
        " equipo_id INTEGER NOT NULL,"
        " partidos_jugados INTEGER DEFAULT 0,"
        " partidos_ganados INTEGER DEFAULT 0,"
        " partidos_empatados INTEGER DEFAULT 0,"
        " partidos_perdidos INTEGER DEFAULT 0,"
        " goles_favor INTEGER DEFAULT 0,"
        " goles_contra INTEGER DEFAULT 0,"
        " puntos INTEGER DEFAULT 0,"
        " estado TEXT DEFAULT 'Activo',"
        " PRIMARY KEY(torneo_id, equipo_id),"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id));",

        "CREATE TABLE IF NOT EXISTS jugador_estadisticas ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " jugador_id INTEGER NOT NULL,"
        " torneo_id INTEGER NOT NULL,"
        " equipo_id INTEGER NOT NULL,"
        " goles INTEGER DEFAULT 0,"
        " asistencias INTEGER DEFAULT 0,"
        " tarjetas_amarillas INTEGER DEFAULT 0,"
        " tarjetas_rojas INTEGER DEFAULT 0,"
        " minutos_jugados INTEGER DEFAULT 0,"
        " FOREIGN KEY(jugador_id) REFERENCES jugador(id),"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id));",

        "CREATE TABLE IF NOT EXISTS equipo_historial ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " equipo_id INTEGER NOT NULL,"
        " torneo_id INTEGER NOT NULL,"
        " posicion_final INTEGER,"
        " partidos_jugados INTEGER DEFAULT 0,"
        " partidos_ganados INTEGER DEFAULT 0,"
        " partidos_empatados INTEGER DEFAULT 0,"
        " partidos_perdidos INTEGER DEFAULT 0,"
        " goles_favor INTEGER DEFAULT 0,"
        " goles_contra INTEGER DEFAULT 0,"
        " mejor_goleador TEXT,"
        " goles_mejor_goleador INTEGER DEFAULT 0,"
        " fecha_inicio TEXT,"
        " fecha_fin TEXT,"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id),"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id));",

        "CREATE TABLE IF NOT EXISTS torneo_fases ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " torneo_id INTEGER NOT NULL,"
        " nombre_fase TEXT NOT NULL,"
        " descripcion TEXT,"
        " orden INTEGER NOT NULL,"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id));",

        "CREATE TABLE IF NOT EXISTS equipo_fase ("
        " torneo_id INTEGER NOT NULL,"
        " equipo_id INTEGER NOT NULL,"
        " fase_id INTEGER NOT NULL,"
        " grupo TEXT,"
        " posicion_en_grupo INTEGER DEFAULT 0,"
        " clasificado INTEGER DEFAULT 0,"
        " eliminado INTEGER DEFAULT 0,"
        " PRIMARY KEY(torneo_id, equipo_id, fase_id),"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id),"
        " FOREIGN KEY(fase_id) REFERENCES torneo_fases(id));",

        "CREATE TABLE IF NOT EXISTS settings ("
        " id INTEGER PRIMARY KEY,"
        " theme INTEGER DEFAULT 0,"
        " language INTEGER DEFAULT 0,"
        " mode INTEGER DEFAULT 0,"
        " text_size INTEGER DEFAULT 1,"
        " image_viewer TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS financiamiento ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo INTEGER NOT NULL,"
        " categoria INTEGER NOT NULL,"
        " descripcion TEXT NOT NULL,"
        " monto REAL NOT NULL,"
        " item_especifico TEXT);",

        "CREATE TABLE IF NOT EXISTS presupuesto_mensual ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " mes_anio TEXT NOT NULL UNIQUE,"
        " presupuesto_total INTEGER NOT NULL,"
        " limite_gasto INTEGER NOT NULL,"
        " alertas_habilitadas INTEGER DEFAULT 1,"
        " fecha_creacion TEXT NOT NULL,"
        " fecha_modificacion TEXT NOT NULL);",

        "CREATE TABLE IF NOT EXISTS comparacion_historial ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " tipo_comparacion INTEGER NOT NULL,"
        " entidad_a_id INTEGER NOT NULL,"
        " entidad_b_id INTEGER NOT NULL,"
        " score_a REAL,"
        " score_b REAL,"
        " ganador INTEGER,"
        " fecha TEXT NOT NULL);",

        "CREATE TABLE IF NOT EXISTS temporada ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " anio INTEGER NOT NULL,"
        " fecha_inicio TEXT NOT NULL,"
        " fecha_fin TEXT NOT NULL,"
        " estado TEXT DEFAULT 'Planificada',"
        " descripcion TEXT);",

        "CREATE TABLE IF NOT EXISTS temporada_fase ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " temporada_id INTEGER NOT NULL,"
        " nombre TEXT NOT NULL,"
        " tipo_fase TEXT NOT NULL,"
        " fecha_inicio TEXT NOT NULL,"
        " fecha_fin TEXT NOT NULL,"
        " descripcion TEXT,"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));",

        "CREATE TABLE IF NOT EXISTS torneo_temporada ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " torneo_id INTEGER NOT NULL,"
        " temporada_id INTEGER NOT NULL,"
        " fase_id INTEGER,"
        " orden_en_temporada INTEGER,"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id),"
        " FOREIGN KEY(fase_id) REFERENCES temporada_fase(id));",

        "CREATE TABLE IF NOT EXISTS equipo_temporada_fatiga ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " equipo_id INTEGER NOT NULL,"
        " temporada_id INTEGER NOT NULL,"
        " fecha TEXT NOT NULL,"
        " fatiga_acumulada REAL DEFAULT 0,"
        " partidos_jugados INTEGER DEFAULT 0,"
        " rendimiento_promedio REAL DEFAULT 0,"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id),"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));",

        "CREATE TABLE IF NOT EXISTS jugador_temporada_fatiga ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " jugador_id INTEGER NOT NULL,"
        " temporada_id INTEGER NOT NULL,"
        " fecha TEXT NOT NULL,"
        " fatiga_acumulada REAL DEFAULT 0,"
        " minutos_jugados_total INTEGER DEFAULT 0,"
        " rendimiento_promedio REAL DEFAULT 0,"
        " lesiones_acumuladas INTEGER DEFAULT 0,"
        " FOREIGN KEY(jugador_id) REFERENCES jugador(id),"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));",

        "CREATE TABLE IF NOT EXISTS equipo_temporada_evolucion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " equipo_id INTEGER NOT NULL,"
        " temporada_id INTEGER NOT NULL,"
        " fecha_medicion TEXT NOT NULL,"
        " puntuacion_rendimiento REAL,"
        " tendencia TEXT,"
        " partidos_ganados INTEGER DEFAULT 0,"
        " partidos_totales INTEGER DEFAULT 0,"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id),"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));",

        "CREATE TABLE IF NOT EXISTS temporada_resumen ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " temporada_id INTEGER NOT NULL UNIQUE,"
        " total_partidos INTEGER DEFAULT 0,"
        " total_goles INTEGER DEFAULT 0,"
        " promedio_goles_partido REAL DEFAULT 0,"
        " equipo_campeon_id INTEGER,"
        " mejor_goleador_jugador_id INTEGER,"
        " mejor_goleador_goles INTEGER DEFAULT 0,"
        " total_lesiones INTEGER DEFAULT 0,"
        " fecha_generacion TEXT NOT NULL,"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id),"
        " FOREIGN KEY(equipo_campeon_id) REFERENCES equipo(id),"
        " FOREIGN KEY(mejor_goleador_jugador_id) REFERENCES jugador(id));",

        "CREATE TABLE IF NOT EXISTS mensual_resumen ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " temporada_id INTEGER NOT NULL,"
        " mes_anio TEXT NOT NULL,"
        " total_partidos INTEGER DEFAULT 0,"
        " total_goles INTEGER DEFAULT 0,"
        " promedio_goles_partido REAL DEFAULT 0,"
        " partidos_ganados INTEGER DEFAULT 0,"
        " partidos_empatados INTEGER DEFAULT 0,"
        " partidos_perdidos INTEGER DEFAULT 0,"
        " total_lesiones INTEGER DEFAULT 0,"
        " total_gastos REAL DEFAULT 0,"
        " total_ingresos REAL DEFAULT 0,"
        " mejor_equipo_mes INTEGER,"
        " peor_equipo_mes INTEGER,"
        " fecha_generacion TEXT NOT NULL,"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id),"
        " FOREIGN KEY(mejor_equipo_mes) REFERENCES equipo(id),"
        " FOREIGN KEY(peor_equipo_mes) REFERENCES equipo(id));",

        "CREATE TABLE IF NOT EXISTS bienestar_objetivo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " fecha_inicio TEXT NOT NULL,"
        " fecha_fin TEXT NOT NULL,"
        " estado TEXT NOT NULL DEFAULT 'Activo',"
        " notas TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_plan_entrenamiento ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " objetivo_id INTEGER NOT NULL,"
        " frecuencia_semanal INTEGER NOT NULL,"
        " rutina_semanal TEXT NOT NULL,"
        " notas TEXT DEFAULT '',"
        " FOREIGN KEY(objetivo_id) REFERENCES bienestar_objetivo(id));",

        "CREATE TABLE IF NOT EXISTS bienestar_entrenamiento ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " duracion_min INTEGER NOT NULL,"
        " intensidad INTEGER NOT NULL,"
        " omitido INTEGER DEFAULT 0,"
        " notas TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_ejercicio ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " grupo_muscular TEXT NOT NULL);",

        "CREATE TABLE IF NOT EXISTS bienestar_entrenamiento_ejercicio ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " entrenamiento_id INTEGER NOT NULL,"
        " ejercicio_id INTEGER NOT NULL,"
        " series INTEGER DEFAULT 0,"
        " repeticiones INTEGER DEFAULT 0,"
        " tiempo_min INTEGER DEFAULT 0,"
        " FOREIGN KEY(entrenamiento_id) REFERENCES bienestar_entrenamiento(id),"
        " FOREIGN KEY(ejercicio_id) REFERENCES bienestar_ejercicio(id));",

        "CREATE TABLE IF NOT EXISTS bienestar_habito ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " dormi_bien INTEGER DEFAULT 0,"
        " hidratacion INTEGER DEFAULT 0,"
        " alcohol INTEGER DEFAULT 0,"
        " estado_animico TEXT DEFAULT '',"
        " nervios INTEGER DEFAULT 0,"
        " confianza INTEGER DEFAULT 0,"
        " motivacion INTEGER DEFAULT 0,"
        " notas TEXT DEFAULT '',"
        " tipo_diario TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_comida ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " calidad TEXT NOT NULL,"
        " descripcion TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_dia_nutricional ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL UNIQUE,"
        " hidratacion TEXT NOT NULL,"
        " alcohol INTEGER DEFAULT 0,"
        " peso_corporal REAL DEFAULT NULL,"
        " notas TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_sesion_mental ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " momento TEXT NOT NULL DEFAULT 'N/A',"
        " partido_id INTEGER DEFAULT NULL,"
        " confianza INTEGER DEFAULT 0,"
        " estres INTEGER DEFAULT 0,"
        " motivacion INTEGER DEFAULT 0,"
        " miedos TEXT DEFAULT '',"
        " presion INTEGER DEFAULT 0,"
        " concentracion INTEGER DEFAULT 0,"
        " pensamientos_clave TEXT DEFAULT '',"
        " texto_libre TEXT DEFAULT '',"
        " FOREIGN KEY(partido_id) REFERENCES partido(id));",

        "CREATE TABLE IF NOT EXISTS bienestar_salud ("
        " id INTEGER PRIMARY KEY CHECK (id = 1),"
        " altura_cm REAL DEFAULT NULL,"
        " peso_kg REAL DEFAULT NULL,"
        " tipo_sangre TEXT DEFAULT '',"
        " ultima_revision TEXT DEFAULT '',"
        " medidas TEXT DEFAULT '',"
        " notas TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_control_medico ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " profesional TEXT DEFAULT '',"
        " resultado TEXT DEFAULT '',"
        " notas TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_estudio_archivo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " control_id INTEGER NOT NULL,"
        " nombre_original TEXT NOT NULL,"
        " ruta_archivo TEXT NOT NULL,"
        " tipo_archivo TEXT DEFAULT '',"
        " fecha_subida TEXT NOT NULL,"
        " FOREIGN KEY(control_id) REFERENCES bienestar_control_medico(id));",

        "CREATE TABLE IF NOT EXISTS bienestar_recomendacion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " score_preparacion INTEGER DEFAULT 0,"
        " riesgo_lesion INTEGER DEFAULT 0,"
        " resumen TEXT DEFAULT '',"
        " rutina TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS bienestar_menu_imagen ("
        " menu_key TEXT PRIMARY KEY,"
        " imagen_ruta TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS carrera_identidad ("
        " id INTEGER PRIMARY KEY CHECK (id = 1),"
        " nombre TEXT DEFAULT '',"
        " edad INTEGER DEFAULT 0,"
        " pie_habil TEXT DEFAULT '',"
        " posiciones TEXT DEFAULT '',"
        " altura_cm REAL DEFAULT NULL,"
        " peso_kg REAL DEFAULT NULL,"
        " estilo TEXT DEFAULT '',"
        " dorsal_favorito INTEGER DEFAULT 0,"
        " objetivos TEXT DEFAULT '',"
        " historia TEXT DEFAULT '',"
        " updated_at TEXT DEFAULT '');",

        "CREATE TABLE IF NOT EXISTS carrera_partido_hito ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " partido_id INTEGER NOT NULL UNIQUE,"
        " tipo_hito TEXT NOT NULL,"
        " nota TEXT DEFAULT '',"
        " created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        " FOREIGN KEY(partido_id) REFERENCES partido(id));",

        "CREATE TABLE IF NOT EXISTS carrera_resumen_narrativo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " periodo_inicio TEXT DEFAULT '',"
        " periodo_fin TEXT DEFAULT '',"
        " perfil_dinamico TEXT DEFAULT '',"
        " resumen TEXT NOT NULL);",

        "CREATE TABLE IF NOT EXISTS tactica_diagrama ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " partido_id INTEGER NOT NULL,"
        " nombre TEXT NOT NULL,"
        " fecha TEXT NOT NULL,"
        " grid TEXT NOT NULL,"
        " FOREIGN KEY(partido_id) REFERENCES partido(id));",

        "CREATE TABLE IF NOT EXISTS entrenamiento_plan ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " descripcion TEXT NOT NULL,"
        " duracion_semanas INTEGER DEFAULT 4,"
        " sesiones_por_semana INTEGER DEFAULT 3);",

        "CREATE TABLE IF NOT EXISTS progresion_jugador ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " jugador_id INTEGER NOT NULL,"
        " plan_id INTEGER,"
        " semana INTEGER NOT NULL,"
        " ataque INTEGER DEFAULT 0,"
        " defensa INTEGER DEFAULT 0,"
        " resistencia INTEGER DEFAULT 0,"
        " velocidad INTEGER DEFAULT 0,"
        " tecnica INTEGER DEFAULT 0,"
        " FOREIGN KEY(jugador_id) REFERENCES jugador(id),"
        " FOREIGN KEY(plan_id) REFERENCES entrenamiento_plan(id));",

        "CREATE TABLE IF NOT EXISTS reporte_config ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " descripcion TEXT NOT NULL,"
        " periodicidad TEXT NOT NULL DEFAULT 'semanal',"
        " habilitado INTEGER DEFAULT 1);",

        "CREATE TABLE IF NOT EXISTS reporte_generado ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " archivo TEXT NOT NULL,"
        " fecha_generacion TEXT DEFAULT (datetime('now','localtime')));",

        "CREATE TABLE IF NOT EXISTS notificacion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " tipo TEXT NOT NULL,"
        " mensaje TEXT NOT NULL,"
        " leida INTEGER DEFAULT 0,"
        " fecha TEXT DEFAULT (datetime('now','localtime')));",

        "CREATE TABLE IF NOT EXISTS backup_config ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " intervalo_horas INTEGER NOT NULL DEFAULT 24,"
        " proximo_backup TEXT,"
        " activo INTEGER DEFAULT 0);",

        NULL
    };

    int failed_index = -1;
    if (!execute_sql_statements(schema_statements, &failed_index))
    {
        printf("Error creando tablas\n");
        snprintf(log_buf_, sizeof(log_buf_), "Error creando esquema (stmt %d): %s", failed_index,
                 sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_buf_);
        return 0;
    }

    app_log_write("INFO", "DB", "Esquema validado/creado");
    return 1;
}

static void add_camiseta_columns(void)
{
    const char *alter_statements[] =
    {
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_SORTEADA ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_IMAGEN_RUTA ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_ACTIVA ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_COLOR_PRINCIPAL ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_COLOR_SECUNDARIO ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_MARCA ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_MODELO ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_TEMPORADA ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_ESTADO_FISICO ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_FECHA_COMPRA ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_COSTO_CENTAVOS ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_OBSERVACIONES ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_PROVEEDOR ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_FUE_REGALO ";",
        "ALTER TABLE camiseta ADD COLUMN " COL_CAMISETA_REGALO_DE ";",
        NULL
    };
    ejecutar_alter_table_group(alter_statements, "camiseta");
}

static void add_cancha_columns(void)
{
    const char *alter_statements[] =
    {
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_IMAGEN_RUTA ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_ACTIVA ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TELEFONO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_DIRECCION ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_LOCALIDAD ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIPO_CANCHA_CODIGO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_SUPERFICIE_CODIGO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TECHADA_ESTADO_CODIGO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIENE_ILUMINACION ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_HORARIO_APERTURA_MIN ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_HORARIO_CIERRE_MIN ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_PRECIO_HORA_DIA_CENTAVOS ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_PRECIO_HORA_NOCHE_CENTAVOS ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIENE_VESTUARIOS ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIENE_DUCHAS ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIENE_BUFFET ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIENE_ESTACIONAMIENTO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_CANTIDAD_CANCHAS ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_ESTADO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_DESCRIPCION ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_DIRECCION_CALLE ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_DIRECCION_ZONA ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIPO_CANCHA ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_PRECIO_HORA ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_SUPERFICIE ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TECHADA_ESTADO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_HORARIO ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_CONTACTO_ALT ";",
        "ALTER TABLE cancha ADD COLUMN " COL_CANCHA_TIENE_GRABACION ";",
        "ALTER TABLE equipo ADD COLUMN imagen_ruta TEXT DEFAULT '';",
        "ALTER TABLE equipo ADD COLUMN activa INTEGER DEFAULT 1;",
        NULL
    };
    ejecutar_alter_table_group(alter_statements, "cancha");
}

static void add_partido_columns(void)
{
    const char *alter_statements[] =
    {
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_RESULTADO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_RENDIMIENTO_GENERAL ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_CANSANCIO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_ESTADO_ANIMO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_COMENTARIO_PERSONAL ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_CLIMA ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_DIA ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_PRECIO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_TIPO_PARTIDO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_RIVAL_NOMBRE ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_TIPO_RIVAL ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_POSICION_JUGADA ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_MINUTOS_JUGADOS ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_INTENSIDAD ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_ESFUERZO_PERCIBIDO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_CONDICION_CANCHA ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_ARBITRAJE ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_EVENTOS_CLAVE ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_RATING_TECNICO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_RATING_FISICO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_RATING_MENTAL ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_ESTADO_CANCHA ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_GOLES_EQUIPO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_GOLES_RIVAL ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_FORMATO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_TARJETA ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_GOLES_EN_CONTRA ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_DOLOR_FISICO ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_TEMPERATURA_C ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_ARBITRAJE_SCORE ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_LO_MEJOR ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_QUE_MEJORAR ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_TAGS ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_GOLES_DETALLE ";",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_ASISTENCIAS_DETALLE ";",
        "ALTER TABLE lesion ADD COLUMN partido_id INTEGER DEFAULT NULL;",
        "ALTER TABLE settings ADD COLUMN image_viewer TEXT DEFAULT '';",
        "ALTER TABLE usuario ADD COLUMN password_salt TEXT DEFAULT '';",
        "ALTER TABLE usuario ADD COLUMN password_hash TEXT DEFAULT '';",
        "ALTER TABLE partido ADD COLUMN mes_anio TEXT DEFAULT '';",
        "ALTER TABLE partido ADD COLUMN " COL_PARTIDO_ATAJASTE_TODO ";",
        NULL
    };
    ejecutar_alter_table_group(alter_statements, "partido");
}

static void add_missing_columns_legacy(void)
{
    add_camiseta_columns();
    add_cancha_columns();
    add_partido_columns();
}

static void add_missing_columns()
{
    int current_version = 0;
    if (!get_user_version(&current_version))
    {
        add_missing_columns_legacy();
        return;
    }

    if (current_version >= DB_VERSION_CURRENT)
    {
        add_camiseta_columns();
        add_cancha_columns();
        add_partido_columns();
        return;
    }

    if (current_version < DB_VERSION_CAMISETA_COLS)
        add_camiseta_columns();
    if (current_version < DB_VERSION_CANCHA_COLS)
        add_cancha_columns();
    if (current_version < DB_VERSION_PARTIDO_COLS)
        add_partido_columns();
    if (current_version < DB_VERSION_CANCHA_GRABACION)
        add_cancha_columns();
    if (current_version < DB_VERSION_PARTIDO_ATAJASTE)
        add_partido_columns();
}

static int drop_legacy_mes_anio_triggers(void)
{
    const char *drop_statements[] = {"DROP TRIGGER IF EXISTS trg_partido_mes_anio_insert;",
                                     "DROP TRIGGER IF EXISTS trg_partido_mes_anio_update;", NULL
                                    };
    int failed = -1;
    if (!execute_sql_statements(drop_statements, &failed))
    {
        snprintf(log_buf_, sizeof(log_buf_), "Fallo eliminando trigger obsoleto: %s",
                 sqlite3_errmsg(db));
        app_log_write("WARN", "DB", log_buf_);
        return 0;
    }
    return 1;
}

static int run_index_migration(const char *const *statements, int version, const char *label)
{
    int failed = -1;
    if (!execute_sql_statements(statements, &failed))
    {
        snprintf(log_buf_, sizeof(log_buf_), "Fallo indice %s '%s': %s", label, statements[failed],
                 sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_buf_);
        return 0;
    }
    if (!set_user_version(version))
    {
        snprintf(log_buf_, sizeof(log_buf_),
                 "No se pudo actualizar user_version tras indices %s: %s", label,
                 sqlite3_errmsg(db));
        app_log_write("WARN", "DB", log_buf_);
        return 0;
    }
    return 1;
}

static int create_performance_indexes()
{
    int current_version = 0;
    if (!get_user_version(&current_version))
    {
        current_version = 0;
    }

    if (current_version >= DB_VERSION_INDEXES)
    {
        if (current_version < DB_VERSION_ADDITIONAL_INDEXES)
        {
            const char *extra_indexes[] =
            {
                "CREATE INDEX IF NOT EXISTS idx_camiseta_nombre ON camiseta(nombre);",
                "CREATE INDEX IF NOT EXISTS idx_cancha_nombre ON cancha(nombre);",
                "CREATE INDEX IF NOT EXISTS idx_equipo_nombre ON equipo(nombre);",
                "CREATE INDEX IF NOT EXISTS idx_temporada_nombre ON "
                "temporada(nombre);",
                "CREATE INDEX IF NOT EXISTS idx_notificacion_leida ON "
                "notificacion(leida, fecha);",
                NULL
            };
            run_index_migration(extra_indexes, DB_VERSION_ADDITIONAL_INDEXES, "extra");
        }

        if (current_version < DB_VERSION_PERFORMANCE_INDEXES)
        {
            const char *perf_indexes[] =
            {
                "CREATE INDEX IF NOT EXISTS idx_camiseta_activa ON camiseta(activa);",
                "CREATE INDEX IF NOT EXISTS idx_cancha_activa ON cancha(activa);",
                "CREATE INDEX IF NOT EXISTS idx_temporada_estado_anio ON "
                "temporada(estado, anio);",
                "CREATE INDEX IF NOT EXISTS idx_mensual_resumen_temporada_mes ON "
                "mensual_resumen(temporada_id, mes_anio);",
                "CREATE INDEX IF NOT EXISTS idx_inventario_item_tipo ON "
                "inventario_item(tipo);",
                NULL
            };
            run_index_migration(perf_indexes, DB_VERSION_PERFORMANCE_INDEXES, "rendimiento");
        }

        if (current_version < DB_VERSION_PARTIDO_ATAJASTE)
        {
            run_index_migration(NULL, DB_VERSION_PARTIDO_ATAJASTE, "atajaste");
        }

        if (current_version < DB_VERSION_PARTIDO_ATAJASTE_FIX)
        {
            const char *fix_atajaste[] =
            {
                "UPDATE partido SET atajaste_todo_el_partido = 0 WHERE id != 19;", NULL
            };
            run_index_migration(fix_atajaste, DB_VERSION_PARTIDO_ATAJASTE_FIX, "atajaste_fix");
        }

        if (sqlite3_exec(db, "PRAGMA optimize;", NULL, NULL, NULL) != SQLITE_OK)
        {
            snprintf(log_buf_, sizeof(log_buf_), "Fallo PRAGMA optimize: %s", sqlite3_errmsg(db));
            app_log_write("ERROR", "DB", log_buf_);
            return 0;
        }
        return 1;
    }

    drop_legacy_mes_anio_triggers();

    const char *index_statements[] =
    {
        "CREATE INDEX IF NOT EXISTS idx_partido_fecha_hora ON "
        "partido(fecha_hora);",
        "CREATE INDEX IF NOT EXISTS idx_partido_cancha_fecha ON "
        "partido(cancha_id, fecha_hora);",
        "CREATE INDEX IF NOT EXISTS idx_partido_camiseta_fecha ON "
        "partido(camiseta_id, fecha_hora);",
        "CREATE INDEX IF NOT EXISTS idx_lesion_camiseta_fecha ON "
        "lesion(camiseta_id, fecha);",
        "CREATE INDEX IF NOT EXISTS idx_lesion_partido_id ON lesion(partido_id);",
        "CREATE INDEX IF NOT EXISTS idx_jugador_equipo_numero ON "
        "jugador(equipo_id, numero);",
        "CREATE INDEX IF NOT EXISTS idx_equipo_torneo_equipo_id ON "
        "equipo_torneo(equipo_id);",
        "CREATE INDEX IF NOT EXISTS idx_partido_torneo_torneo_fase ON "
        "partido_torneo(torneo_id, fase);",
        "CREATE INDEX IF NOT EXISTS idx_jugador_estadisticas_jugador_torneo ON "
        "jugador_estadisticas(jugador_id, torneo_id);",
        "CREATE INDEX IF NOT EXISTS idx_torneo_temporada_temporada_orden ON "
        "torneo_temporada(temporada_id, orden_en_temporada);",
        "CREATE INDEX IF NOT EXISTS idx_carrera_hito_partido ON "
        "carrera_partido_hito(partido_id);",
        "CREATE INDEX IF NOT EXISTS idx_carrera_hito_tipo ON "
        "carrera_partido_hito(tipo_hito);",
        "CREATE INDEX IF NOT EXISTS idx_carrera_resumen_fecha ON "
        "carrera_resumen_narrativo(fecha);",
        "CREATE INDEX IF NOT EXISTS idx_partido_resultado ON partido(resultado);",
        "CREATE INDEX IF NOT EXISTS idx_partido_clima ON partido(clima);",
        "CREATE INDEX IF NOT EXISTS idx_partido_camiseta ON "
        "partido(camiseta_id);",
        "CREATE INDEX IF NOT EXISTS idx_partido_mes_anio ON partido(mes_anio);",
        "CREATE INDEX IF NOT EXISTS idx_equipo_partido_id ON equipo(partido_id);",
        "CREATE INDEX IF NOT EXISTS idx_coleccion_inventario_coleccion ON "
        "coleccion_inventario(coleccion_id);",
        "CREATE INDEX IF NOT EXISTS idx_financiamiento_fecha ON "
        "financiamiento(fecha);",
        "CREATE INDEX IF NOT EXISTS idx_financiamiento_tipo_fecha ON "
        "financiamiento(tipo, fecha);",
        "CREATE INDEX IF NOT EXISTS idx_financiamiento_substr_fecha ON "
        "financiamiento(substr(fecha, 1, 7));",
        "CREATE INDEX IF NOT EXISTS idx_bienestar_sesion_fecha ON "
        "bienestar_sesion_mental(fecha);",
        "CREATE INDEX IF NOT EXISTS idx_bienestar_entrenamiento_fecha ON "
        "bienestar_entrenamiento(fecha);",
        "CREATE INDEX IF NOT EXISTS idx_bienestar_comida_fecha_calidad ON "
        "bienestar_comida(fecha, calidad);",
        "CREATE INDEX IF NOT EXISTS idx_camiseta_nombre ON camiseta(nombre);",
        "CREATE INDEX IF NOT EXISTS idx_cancha_nombre ON cancha(nombre);",
        "CREATE INDEX IF NOT EXISTS idx_equipo_nombre ON equipo(nombre);",
        "CREATE INDEX IF NOT EXISTS idx_temporada_nombre ON temporada(nombre);",
        "CREATE INDEX IF NOT EXISTS idx_notificacion_leida ON "
        "notificacion(leida, fecha);",
        "CREATE INDEX IF NOT EXISTS idx_camiseta_activa ON camiseta(activa);",
        "CREATE INDEX IF NOT EXISTS idx_cancha_activa ON cancha(activa);",
        "CREATE INDEX IF NOT EXISTS idx_temporada_estado_anio ON "
        "temporada(estado, anio);",
        "CREATE INDEX IF NOT EXISTS idx_mensual_resumen_temporada_mes ON "
        "mensual_resumen(temporada_id, mes_anio);",
        "CREATE INDEX IF NOT EXISTS idx_inventario_item_tipo ON "
        "inventario_item(tipo);",
        NULL
    };

    if (!run_index_migration(index_statements, DB_VERSION_CURRENT, "inicial"))
    {
        return 0;
    }

    if (sqlite3_exec(db, "PRAGMA optimize;", NULL, NULL, NULL) != SQLITE_OK)
    {
        snprintf(log_buf_, sizeof(log_buf_), "Fallo PRAGMA optimize: %s", sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_buf_);
        return 0;
    }

    app_log_write("INFO", "DB", "Indices de rendimiento validados/creados");
    return 1;
}

int db_init()
{
    app_log_init();
    app_log_write("INFO", "APP", "Inicio de inicializacion de base de datos");

    if (!setup_database_paths())
    {
        app_log_write("ERROR", "APP", "Fallo setup_database_paths");
        return 0;
    }

    if (!create_database_connection())
    {
        app_log_write("ERROR", "APP", "Fallo create_database_connection");
        return 0;
    }

    if (!apply_database_tuning())
    {
        app_log_write("ERROR", "APP", "Fallo apply_database_tuning");
        return 0;
    }

    if (!create_database_schema())
    {
        app_log_write("ERROR", "APP", "Fallo create_database_schema");
        return 0;
    }

    add_missing_columns();
    app_log_write("INFO", "DB", "Migraciones de columnas aplicadas");

    backfill_mes_anio_once();

    if (!create_performance_indexes())
    {
        app_log_write("ERROR", "APP", "Fallo create_performance_indexes");
        return 0;
    }

    // Crear directorios de importacion y exportacion al iniciar
    get_import_dir();
    get_export_dir();
    get_images_dir();
    get_music_dir();

    app_log_write("INFO", "APP", "Inicializacion de base de datos completada");

    return 1;
}

void db_close()
{
    if (db)
    {
        app_log_write("INFO", "DB", "Cerrando conexion SQLite");
        db_clear_stmt_cache();
        sqlite3_close(db);
        app_log_write("INFO", "DB", "Conexion SQLite cerrada");
        app_log_close();
    }
}

char *get_user_name()
{
    const char *sql = "SELECT nombre FROM usuario LIMIT 1;";
    char nombre_tmp[256] = {0};
    char *nombre = NULL;

    if (db_query_single_text(sql, nombre_tmp, sizeof(nombre_tmp)))
    {
        nombre = STRDUP(nombre_tmp);
    }

    return nombre;
}

int set_user_name(const char *nombre)
{
    const char *sql_update = "UPDATE usuario SET nombre = ? WHERE id = 1;";
    const char *sql_insert = "INSERT INTO usuario (id, nombre, password_salt, "
                             "password_hash) VALUES (1, ?, '', '');";
    int updated_rows = 0;
    const char *update_params[] = {nombre};
    const char *insert_params[] = {nombre};

    if (ejecutar_stmt_texto(sql_update, update_params, 1, &updated_rows) && updated_rows > 0)
    {
        return 1;
    }

    return ejecutar_stmt_texto(sql_insert, insert_params, 1, NULL);
}

int user_has_password(void)
{
    const char *sql = "SELECT password_hash FROM usuario WHERE id = 1;";
    char hash_tmp[128] = {0};

    return db_query_single_text(sql, hash_tmp, sizeof(hash_tmp)) && hash_tmp[0] != '\0';
}

int set_user_password(const char *plain_password)
{
    char salt_hex[33];
    char hash_hex[17];
    int updated_rows = 0;
    char *nombre = NULL;
    const char *sql_update =
        "UPDATE usuario SET password_salt = ?, password_hash = ? WHERE id = 1;";
    const char *sql_insert = "INSERT INTO usuario (id, nombre, password_salt, "
                             "password_hash) VALUES (1, ?, ?, ?);";

    if (!plain_password || plain_password[0] == '\0')
    {
        return 0;
    }

    generate_salt_hex(salt_hex, sizeof(salt_hex));
    build_password_hash(plain_password, salt_hex, hash_hex, sizeof(hash_hex));

    const char *update_params[] = {salt_hex, hash_hex};
    if (ejecutar_stmt_texto(sql_update, update_params, 2, &updated_rows) && updated_rows > 0)
    {
        return 1;
    }

    nombre = get_user_name();
    if (!nombre)
    {
        nombre = STRDUP("Usuario");
    }

    if (!nombre)
    {
        return 0;
    }

    const char *insert_params[] = {nombre, salt_hex, hash_hex};
    int result = ejecutar_stmt_texto(sql_insert, insert_params, 3, NULL);

    free(nombre);
    return result;
}

int verify_user_password(const char *plain_password)
{
    const char *sql_salt = "SELECT password_salt FROM usuario WHERE id = 1;";
    const char *sql_hash = "SELECT password_hash FROM usuario WHERE id = 1;";
    char salt[128] = {0};
    char stored_hash[128] = {0};

    if (!plain_password || plain_password[0] == '\0')
    {
        return 0;
    }

    if (!db_query_single_text(sql_salt, salt, sizeof(salt)) ||
            !db_query_single_text(sql_hash, stored_hash, sizeof(stored_hash)))
    {
        return 0;
    }

    if (salt[0] == '\0' || stored_hash[0] == '\0')
    {
        return 0;
    }

    char computed_hash[17];
    build_password_hash(plain_password, salt, computed_hash, sizeof(computed_hash));
    return strcmp(computed_hash, stored_hash) == 0;
}

int clear_user_password(void)
{
    const char *sql = "UPDATE usuario SET password_salt = '', password_hash = '' WHERE id = 1;";
    return ejecutar_stmt_texto(sql, NULL, 0, NULL);
}

const char *get_data_dir()
{
    return DB_DIR;
}

static const char *obtener_directorio_publico(const char *dir_preferido_local,
        const char *dir_legado_local,
        const char *subdir_documentos, char *out_dir,
        size_t out_size, const char *nombre_subdir)
{
    if (!configurar_directorio_usuario(dir_preferido_local, dir_legado_local, subdir_documentos,
                                       out_dir, out_size, nombre_subdir))
    {
        return NULL;
    }
    return out_dir;
}

typedef struct
{
    const char *preferred_local;
    const char *legacy_local;
    const char *preferred_linux;
    const char *legacy_linux;
    const char *subdir_documentos;
    const char *nombre_subdir;
    char *out_dir;
    size_t out_size;
} DirCfg;

static const char *resolver_directorio_config(const DirCfg *cfg)
{
    if (!cfg)
    {
        return NULL;
    }

    const char *dir_preferido_local = cfg->preferred_local;
    const char *dir_legado_local = cfg->legacy_local;

#ifndef _WIN32
    if (cfg->preferred_linux)
    {
        dir_preferido_local = cfg->preferred_linux;
    }
    if (cfg->legacy_linux)
    {
        dir_legado_local = cfg->legacy_linux;
    }
#endif

    return obtener_directorio_publico(dir_preferido_local, dir_legado_local, cfg->subdir_documentos,
                                      cfg->out_dir, cfg->out_size, cfg->nombre_subdir);
}

static const DirCfg DIR_CFG_EXPORT = {NULL,
                                      NULL,
                                      "./Exportaciones",
                                      NULL,
                                      "Exportaciones",
                                      "Exportaciones",
                                      EXPORT_DIR,
                                      sizeof(EXPORT_DIR)
                                     };

static const DirCfg DIR_CFG_IMPORT = {NULL,
                                      NULL,
                                      "./Importaciones",
                                      "./importaciones",
                                      "Importaciones",
                                      "Importaciones",
                                      IMPORT_DIR,
                                      sizeof(IMPORT_DIR)
                                     };

static const DirCfg DIR_CFG_IMAGES = {"./Imagenes", NULL,       NULL,       NULL,
                                      "Imagenes",   "Imagenes", IMAGES_DIR, sizeof(IMAGES_DIR)
                                     };

static const DirCfg DIR_CFG_MUSIC = {NULL,     NULL,     "./Musica", NULL,
                                     "Musica", "Musica", MUSIC_DIR,  sizeof(MUSIC_DIR)
                                    };

const char *get_export_dir()
{
    return resolver_directorio_config(&DIR_CFG_EXPORT);
}

const char *get_import_dir()
{
    return resolver_directorio_config(&DIR_CFG_IMPORT);
}

const char *get_images_dir()
{
    return resolver_directorio_config(&DIR_CFG_IMAGES);
}

const char *get_music_dir()
{
    return resolver_directorio_config(&DIR_CFG_MUSIC);
}

int db_get_image_path_by_id(const char *table_name, int id, char *ruta, size_t size)
{
    if (!table_name || table_name[0] == '\0' || !ruta || size == 0 || id <= 0)
    {
        return 0;
    }

    for (size_t i = 0; table_name[i] != '\0'; i++)
    {
        unsigned char ch = (unsigned char)table_name[i];
        if (!(isalnum(ch) || ch == '_'))
        {
            return 0;
        }
    }

    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT imagen_ruta FROM %s WHERE id=?", table_name);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
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

int db_resolve_image_absolute_path(const char *ruta_db, char *ruta_absoluta, size_t size)
{
    if (!ruta_db || ruta_db[0] == '\0' || !ruta_absoluta || size == 0)
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

    FILE *f = app_fopen(ruta_absoluta, "rb");
    if (!f)
    {
        return 0;
    }
    fclose(f);

    return 1;
}

void exportar_base_datos()
{
    app_log_write("INFO", "EXPORT", "Inicio de exportacion de base de datos");

    const char *dest_path = get_export_path("mifutbol.db");
    if (!dest_path)
    {
        LOG_ERROR_MSG("EXPORT",
                      "Error: No se pudo obtener la ruta de destino para exportar "
                      "la base de datos.",
                      "No se pudo obtener ruta de destino para exportacion");
        pause_console();
        return;
    }

    if (!db)
    {
        // Fallback cuando no hay conexion abierta.
        const char *source_path = DB_PATH;
        CopyResult result = copiar_archivo(source_path, dest_path);
        if (result == COPY_SRC_ERROR)
        {
            LOG_ERROR_CONSOLE_LOG_FMT("EXPORT", "Error: No se encontro la base de datos en:\n%s",
                                      "No se encontro DB origen para exportar: %.980s",
                                      source_path);
            pause_console();
            return;
        }

        if (result == COPY_DST_ERROR)
        {
            LOG_ERROR_CONSOLE_LOG_FMT("EXPORT", "Error creando archivo destino:\n%s",
                                      "No se pudo abrir DB destino para exportar: %.977s",
                                      dest_path);
            pause_console();
            return;
        }

        printf("Base de datos exportada a:\n%s\n", dest_path);
        snprintf(log_buf_, sizeof(log_buf_), "Exportacion finalizada (copia directa): %.983s",
                 dest_path);
        app_log_write("INFO", "EXPORT", log_buf_);
        pause_console();
        return;
    }

    // Exportacion segura con SQLite backup API (evita errores por archivo en
    // uso).
    sqlite3 *dest_db = NULL;
    if (sqlite3_open(dest_path, &dest_db) != SQLITE_OK)
    {
        printf("Error abriendo base de datos destino:\n%s\n", dest_path);
        snprintf(log_buf_, sizeof(log_buf_), "Error abriendo DB destino (%.700s): %.260s",
                 dest_path, sqlite3_errmsg(dest_db));
        app_log_write("ERROR", "EXPORT", log_buf_);
        if (dest_db)
        {
            sqlite3_close(dest_db);
        }
        pause_console();
        return;
    }

    sqlite3_backup *backup = sqlite3_backup_init(dest_db, "main", db, "main");
    if (!backup)
    {
        LOG_ERROR_FMT("EXPORT", "Error iniciando backup SQLite: %s", sqlite3_errmsg(dest_db));
        sqlite3_close(dest_db);
        pause_console();
        return;
    }

    int step_rc = sqlite3_backup_step(backup, -1);
    int finish_rc = sqlite3_backup_finish(backup);
    int final_rc = (step_rc == SQLITE_DONE) ? finish_rc : step_rc;

    sqlite3_close(dest_db);

    if (final_rc != SQLITE_OK)
    {
        printf("Error exportando base de datos (SQLite backup): %s\n", sqlite3_errstr(final_rc));
        snprintf(log_buf_, sizeof(log_buf_), "Error SQLite backup final_rc=%d (%s)", final_rc,
                 sqlite3_errstr(final_rc));
        app_log_write("ERROR", "EXPORT", log_buf_);
        pause_console();
        return;
    }

    printf("Base de datos exportada a:\n%s\n", dest_path);
    snprintf(log_buf_, sizeof(log_buf_), "Exportacion finalizada (SQLite backup): %.982s",
             dest_path);
    app_log_write("INFO", "EXPORT", log_buf_);
    pause_console();
}

int backup_base_datos_automatico(const char *motivo)
{
    snprintf(log_buf_, sizeof(log_buf_), "Inicio de backup automatico. Motivo: %s",
             (motivo && motivo[0] != '\0') ? motivo : "sin_motivo");
    app_log_write("INFO", "BACKUP", log_buf_);

    const char *source_path = DB_PATH;
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        LOG_ERROR_MSG("BACKUP", get_text("backup_failed"),
                      "No se pudo resolver directorio de exportacion");
        return 0;
    }

    char backup_dir[1024];
    strcpy_s(backup_dir, sizeof(backup_dir), export_dir);
    strcat_s(backup_dir, sizeof(backup_dir), DB_PATH_SEP "Backups");

    if (!asegurar_directorio(backup_dir, "Backups"))
    {
        LOG_ERROR_CONSOLE_MSG_LOG_FMT("BACKUP", get_text("backup_failed"),
                                      "No se pudo crear/acceder a directorio de backups: %.972s",
                                      backup_dir);
        return 0;
    }

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    char dest_path[1024];
    const char *prefix = "mifutbol_backup_";
    const char *ext = ".db";
    char motivo_safe[64] = {0};

    if (motivo && motivo[0] != '\0')
    {
        strncpy_s(motivo_safe, sizeof(motivo_safe), motivo, sizeof(motivo_safe) - 1);
        sanitize_filename_token(motivo_safe);
    }

    size_t used = 0;
    dest_path[0] = '\0';

    if (!append_str(dest_path, &used, sizeof(dest_path), backup_dir) ||
            !append_str(dest_path, &used, sizeof(dest_path), DB_PATH_SEP) ||
            !append_str(dest_path, &used, sizeof(dest_path), prefix))
    {
        LOG_ERROR_MSG("BACKUP", get_text("backup_failed"),
                      "No se pudo construir ruta base de backup");
        return 0;
    }

    if (motivo_safe[0] != '\0' && (!append_str(dest_path, &used, sizeof(dest_path), motivo_safe) ||
                                   !append_str(dest_path, &used, sizeof(dest_path), "_")))
    {
        LOG_ERROR_MSG("BACKUP", get_text("backup_failed"),
                      "No se pudo agregar motivo a ruta de backup");
        return 0;
    }

    if (!append_str(dest_path, &used, sizeof(dest_path), timestamp) ||
            !append_str(dest_path, &used, sizeof(dest_path), ext))
    {
        LOG_ERROR_MSG("BACKUP", get_text("backup_failed"),
                      "No se pudo completar nombre de archivo backup");
        return 0;
    }

    CopyResult result = copiar_archivo(source_path, dest_path);
    if (result != COPY_OK)
    {
        LOG_ERROR_CONSOLE_MSG_LOG_FMT("BACKUP", get_text("backup_failed"),
                                      "Fallo copia de backup desde %.470s hacia %.470s",
                                      source_path, dest_path);
        return 0;
    }

    printf("%s\n%s\n", get_text("backup_created"), dest_path);
    snprintf(log_buf_, sizeof(log_buf_), "Backup automatico completado: %.992s", dest_path);
    app_log_write("INFO", "BACKUP", log_buf_);
    return 1;
}

void importar_base_datos()
{
    app_log_write("INFO", "IMPORT", "Inicio de importacion de base de datos");

    const char *import_dir = get_import_dir();
    if (!import_dir)
    {
        LOG_ERROR_MSG("IMPORT", "Error obteniendo directorio de importaciones",
                      "No se pudo obtener directorio de importaciones");
        return;
    }

    char source_path[1024];
    strcpy_s(source_path, sizeof(source_path), import_dir);
    strcat_s(source_path, sizeof(source_path), DB_PATH_SEP "mifutbol.db");

    const char *dest_path = DB_PATH;

    // Verificar que exista el archivo a importar
    CopyResult result = copiar_archivo(source_path, dest_path);
    if (result != COPY_OK)
    {
        if (result == COPY_SRC_ERROR)
        {
            LOG_ERROR_CONSOLE_LOG_FMT("IMPORT",
                                      "Error: No se encontro el archivo a importar en:\n%s",
                                      "No se encontro archivo de importacion: %.983s", source_path);
        }
        else
        {
            LOG_ERROR_CONSOLE_LOG_FMT(
                "IMPORT", "Error: No se pudo abrir la base de datos destino:\n%s",
                "No se pudo abrir DB destino para importar: %.977s", dest_path);
        }
        return;
    }

    printf("Base de datos importada correctamente.\n");
    printf("Origen: %s\n", source_path);
    printf("Destino: %s\n", dest_path);
    printf("Reinicia la aplicacion para usar la nueva base.\n");

    snprintf(log_buf_, sizeof(log_buf_), "Importacion completada. Origen: %.470s | Destino: %.470s",
             source_path, dest_path);
    app_log_write("INFO", "IMPORT", log_buf_);

    pause_console();
}
