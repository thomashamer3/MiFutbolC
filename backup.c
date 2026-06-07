#include "backup.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define BACKUP_PATH_SEP "\\"
#define MKDIR_BACKUP(path) _mkdir(path)
#else
#define BACKUP_PATH_SEP "/"
#define MKDIR_BACKUP(path) mkdir(path, 0755)
#endif

#define BACKUP_SUBDIR "backups"
#define MANIFEST_NAME "backups.json"
#define MAX_BUFFER 1024

static int asegurar_backup_dir(const char *path)
{
    errno = 0;
    int rc = MKDIR_BACKUP(path);
    if (rc != 0 && errno != EEXIST)
    {
#ifdef _WIN32
        char err_buf[128];
        strerror_s(err_buf, sizeof(err_buf), errno);
        printf("Error creando directorio de backups: %s\n", err_buf);
#else
        printf("Error creando directorio de backups: %s\n", strerror(errno));
#endif
        return 0;
    }
    return 1;
}

static int get_backup_dir(char *buffer, size_t size)
{
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        return 0;
    }
    snprintf(buffer, size, "%s%s%s", export_dir, BACKUP_PATH_SEP, BACKUP_SUBDIR);
    return 1;
}

static void get_timestamp_seconds(char *buffer, size_t size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%Y%m%d_%H%M%S", &tm_struct);
}

static void sanitizar_descripcion(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0)
    {
        return;
    }
    size_t i = 0;
    while (*src && i < dst_size - 1)
    {
        char c = *src;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-')
        {
            dst[i++] = c;
        }
        else if (c == ' ')
        {
            dst[i++] = '_';
        }
        src++;
    }
    dst[i] = '\0';
}

static int construir_ruta_db(char *buffer, size_t size)
{
    const char *data_dir = get_data_dir();
    if (!data_dir)
    {
        return 0;
    }

    const char *active_user = db_get_active_user();
    char db_filename[256];

    if (active_user && active_user[0] != '\0')
    {
        snprintf(db_filename, sizeof(db_filename), "mifutbol_%s.db", active_user);
    }
    else
    {
        strcpy_s(db_filename, sizeof(db_filename), "mifutbol.db");
    }

    snprintf(buffer, size, "%s%s%s", data_dir, BACKUP_PATH_SEP, db_filename);
    return 1;
}

static long long obtener_tamano_archivo(const char *path)
{
    FILE *f = NULL;
    if (fopen_s(&f, path, "rb") != 0 || !f)
    {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return -1;
    }
    long size = ftell(f);
    fclose(f);
    return (size < 0) ? -1 : (long long)size;
}

static cJSON *leer_manifest(const char *backup_dir)
{
    char manifest_path[MAX_BUFFER];
    snprintf(manifest_path, sizeof(manifest_path), "%.*s%s%.*s",
             (int)((sizeof(manifest_path) - 2) / 2), backup_dir,
             BACKUP_PATH_SEP,
             (int)((sizeof(manifest_path) - 2) / 2), MANIFEST_NAME);

    FILE *f = NULL;
    if (fopen_s(&f, manifest_path, "rb") != 0 || !f)
    {
        return cJSON_CreateArray();
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0)
    {
        fclose(f);
        return cJSON_CreateArray();
    }

    char *content = (char *)malloc((size_t)len + 1);
    if (!content)
    {
        fclose(f);
        return cJSON_CreateArray();
    }

    size_t read_len = fread(content, 1, (size_t)len, f);
    fclose(f);
    content[read_len] = '\0';

    cJSON *manifest = cJSON_Parse(content);
    free(content);

    if (!manifest)
    {
        return cJSON_CreateArray();
    }

    if (!cJSON_IsArray(manifest))
    {
        cJSON_Delete(manifest);
        return cJSON_CreateArray();
    }

    return manifest;
}

static int guardar_manifest(const char *backup_dir, cJSON const *manifest)
{
    char manifest_path[MAX_BUFFER];
    snprintf(manifest_path, sizeof(manifest_path), "%.*s%s%.*s",
             (int)((sizeof(manifest_path) - 2) / 2), backup_dir,
             BACKUP_PATH_SEP,
             (int)((sizeof(manifest_path) - 2) / 2), MANIFEST_NAME);

    char *json_str = cJSON_Print(manifest);
    if (!json_str)
    {
        return 0;
    }

    FILE *f = NULL;
    if (fopen_s(&f, manifest_path, "w") != 0 || !f)
    {
        cJSON_free(json_str);
        return 0;
    }

    fprintf(f, "%s\n", json_str);
    fclose(f);
    cJSON_free(json_str);
    return 1;
}

static int backup_existe_en_manifest(cJSON const *manifest, const char *filename)
{
    if (!manifest || !filename)
    {
        return 0;
    }

    int count = cJSON_GetArraySize(manifest);
    for (int i = 0; i < count; i++)
    {
        cJSON const *entry = cJSON_GetArrayItem(manifest, i);
        cJSON const *fname = cJSON_GetObjectItem(entry, "filename");
        if (fname && cJSON_IsString(fname) && strcmp(fname->valuestring, filename) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static char *obtener_fecha_actual(void)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    char *buffer = (char *)malloc(32);
    if (!buffer)
    {
        return NULL;
    }
    strftime(buffer, 32, "%d/%m/%Y %H:%M", &tm_struct);
    return buffer;
}

int crear_backup(const char *descripcion)
{
    app_log_event("BACKUP", "Inicio de creacion de backup manual");

    char backup_dir[MAX_BUFFER];
    if (!get_backup_dir(backup_dir, sizeof(backup_dir)))
    {
        printf("Error: No se pudo obtener el directorio de backups.\n");
        app_log_event("BACKUP", "No se pudo obtener directorio de backups");
        return 0;
    }

    if (!asegurar_backup_dir(backup_dir))
    {
        printf("Error: No se pudo crear el directorio de backups.\n");
        app_log_event("BACKUP", "No se pudo crear directorio de backups");
        return 0;
    }

    char timestamp[32];
    get_timestamp_seconds(timestamp, sizeof(timestamp));

    char desc_safe[128] = {0};
    if (descripcion && descripcion[0] != '\0')
    {
        sanitizar_descripcion(descripcion, desc_safe, sizeof(desc_safe));
    }

    if (desc_safe[0] == '\0')
    {
        strcpy_s(desc_safe, sizeof(desc_safe), "sin_descripcion");
    }

    char filename[MAX_BUFFER];
    snprintf(filename, sizeof(filename), "%s_%s.db", timestamp, desc_safe);

    char dest_path[MAX_BUFFER];
    snprintf(dest_path, sizeof(dest_path), "%.*s%s%.*s",
             (int)((sizeof(dest_path) - 2) / 2), backup_dir,
             BACKUP_PATH_SEP,
             (int)((sizeof(dest_path) - 2) / 2), filename);

    char db_path[MAX_BUFFER];
    if (!construir_ruta_db(db_path, sizeof(db_path)))
    {
        printf("Error: No se pudo determinar la ruta de la base de datos.\n");
        app_log_event("BACKUP", "No se pudo determinar ruta de DB");
        return 0;
    }

    if (db)
    {
        sqlite3 *dest_db = NULL;
        if (sqlite3_open(dest_path, &dest_db) != SQLITE_OK)
        {
            printf("Error: No se pudo crear el archivo de backup.\n");
            snprintf(backup_dir, sizeof(backup_dir), "Error creando backup: %s", sqlite3_errmsg(dest_db));
            app_log_event("BACKUP", backup_dir);
            sqlite3_close(dest_db);
            return 0;
        }

        sqlite3_backup *backup = sqlite3_backup_init(dest_db, "main", db, "main");
        if (!backup)
        {
            printf("Error: No se pudo iniciar el backup SQLite.\n");
            sqlite3_close(dest_db);
            return 0;
        }

        int step_rc = sqlite3_backup_step(backup, -1);
        int finish_rc = sqlite3_backup_finish(backup);
        sqlite3_close(dest_db);

        if (step_rc != SQLITE_DONE || finish_rc != SQLITE_OK)
        {
            printf("Error: Fallo la copia de seguridad SQLite.\n");
            app_log_event("BACKUP", "Fallo backup via SQLite backup API");
            return 0;
        }
    }
    else
    {
        if (!app_copy_binary_file(db_path, dest_path))
        {
            printf("Error: No se pudo copiar la base de datos.\n");
            app_log_event("BACKUP", "Fallo copia directa del archivo DB");
            return 0;
        }
    }

    long long file_size = obtener_tamano_archivo(dest_path);
    if (file_size < 0)
    {
        file_size = 0;
    }

    cJSON *manifest = leer_manifest(backup_dir);
    if (!manifest)
    {
        printf("Error: No se pudo leer el manifiesto de backups.\n");
        return 0;
    }

    char *fecha_str = obtener_fecha_actual();
    if (!fecha_str)
    {
        cJSON_Delete(manifest);
        return 0;
    }

    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "filename", filename);
    cJSON_AddStringToObject(entry, "descripcion", (descripcion && descripcion[0] != '\0') ? descripcion : "sin_descripcion");
    cJSON_AddStringToObject(entry, "fecha", fecha_str);
    cJSON_AddNumberToObject(entry, "size_bytes", (double)file_size);
    cJSON_AddItemToArray(manifest, entry);

    int ok = guardar_manifest(backup_dir, manifest);
    cJSON_Delete(manifest);
    free(fecha_str);

    if (!ok)
    {
        printf("Error: No se pudo actualizar el manifiesto de backups.\n");
        app_log_event("BACKUP", "No se pudo guardar el manifiesto");
        return 0;
    }

    printf("Backup creado exitosamente:\n");
    printf("  Archivo: %s\n", filename);
    printf("  Ruta: %s\n", dest_path);
    printf("  Tamano: %lld bytes\n", file_size);

    snprintf(backup_dir, sizeof(backup_dir), "Backup manual completado: %.990s", dest_path);
    app_log_event("BACKUP", backup_dir);
    return 1;
}

int listar_backups(void)
{
    char backup_dir[MAX_BUFFER];
    if (!get_backup_dir(backup_dir, sizeof(backup_dir)))
    {
        mostrar_no_hay_registros("backups");
        return 0;
    }

    cJSON *manifest = leer_manifest(backup_dir);
    if (!manifest)
    {
        mostrar_no_hay_registros("backups");
        return 0;
    }

    int count = cJSON_GetArraySize(manifest);
    if (count == 0)
    {
        cJSON_Delete(manifest);
        mostrar_no_hay_registros("backups");
        return 0;
    }

    mostrar_pantalla("BACKUPS DISPONIBLES");

    int validos = 0;
    for (int i = 0; i < count; i++)
    {
        cJSON const *entry = cJSON_GetArrayItem(manifest, i);
        cJSON const *fname = cJSON_GetObjectItem(entry, "filename");
        cJSON const *desc = cJSON_GetObjectItem(entry, "descripcion");
        cJSON const *fecha = cJSON_GetObjectItem(entry, "fecha");
        cJSON const *size = cJSON_GetObjectItem(entry, "size_bytes");

        const char *fn = (fname && cJSON_IsString(fname)) ? fname->valuestring : "?";
        const char *dc = (desc && cJSON_IsString(desc)) ? desc->valuestring : "?";
        const char *fe = (fecha && cJSON_IsString(fecha)) ? fecha->valuestring : "?";
        long long sz = (size && cJSON_IsNumber(size)) ? (long long)size->valuedouble : 0;

        char filepath[MAX_BUFFER];
        snprintf(filepath, sizeof(filepath), "%.*s%s%.*s",
                 (int)((sizeof(filepath) - 2) / 2), backup_dir,
                 BACKUP_PATH_SEP,
                 (int)((sizeof(filepath) - 2) / 2), fn);

        int existe = 0;
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(filepath);
        existe = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
        struct stat st;
        existe = (stat(filepath, &st) == 0 && S_ISREG(st.st_mode));
#endif

        if (!existe)
        {
            continue;
        }

        validos++;
        printf("  %d. %s\n", validos, fn);
        printf("     Descripcion: %s\n", dc);
        printf("     Fecha: %s\n", fe);
        printf("     Tamano: %lld bytes\n", sz);
        printf("     ----------------------------------------\n");
    }

    cJSON_Delete(manifest);

    if (validos == 0)
    {
        mostrar_no_hay_registros("backups");
        return 0;
    }

    printf("\nTotal: %d backup(s) disponible(s)\n", validos);
    printf("\nNota: Para restaurar, anote el nombre exacto del archivo.\n");
    pause_console();
    return 1;
}

int restaurar_backup(const char *filename)
{
    if (!filename || filename[0] == '\0')
    {
        printf("Error: Nombre de archivo invalido.\n");
        return 0;
    }

    app_log_event("BACKUP", "Inicio de restauracion de backup");

    char backup_dir[MAX_BUFFER];
    if (!get_backup_dir(backup_dir, sizeof(backup_dir)))
    {
        printf("Error: No se pudo obtener el directorio de backups.\n");
        return 0;
    }

    char source_path[MAX_BUFFER];
    snprintf(source_path, sizeof(source_path), "%.*s%s%.*s",
             (int)((sizeof(source_path) - 2) / 2), backup_dir,
             BACKUP_PATH_SEP,
             (int)((sizeof(source_path) - 2) / 2), filename);

    int archivo_valido = 0;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(source_path);
    archivo_valido = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    archivo_valido = (stat(source_path, &st) == 0 && S_ISREG(st.st_mode));
#endif

    if (!archivo_valido)
    {
        printf("Error: El archivo de backup no existe:\n%s\n", source_path);
        app_log_event("BACKUP", "Archivo de backup no encontrado para restaurar");
        return 0;
    }

    cJSON *manifest = leer_manifest(backup_dir);
    int encontrado = backup_existe_en_manifest(manifest, filename);
    cJSON_Delete(manifest);

    if (!encontrado)
    {
        printf("Error: El backup no esta registrado en el manifiesto.\n");
        return 0;
    }

    printf("ADVERTENCIA: Restaurar este backup reemplazara la base de datos actual.\n");
    printf("  Backup: %s\n", filename);
    printf("  Esta accion no se puede deshacer.\n");

    if (!confirmar("  Desea continuar"))
    {
        printf("Restauracion cancelada.\n");
        app_log_event("BACKUP", "Restauracion cancelada por el usuario");
        return 0;
    }

    char db_path[MAX_BUFFER];
    if (!construir_ruta_db(db_path, sizeof(db_path)))
    {
        printf("Error: No se pudo determinar la ruta de la base de datos.\n");
        return 0;
    }

    if (!app_copy_binary_file(source_path, db_path))
    {
        printf("Error: No se pudo restaurar la base de datos desde el backup.\n");
        snprintf(backup_dir, sizeof(backup_dir), "Fallo restauracion desde: %.970s", source_path);
        app_log_event("BACKUP", backup_dir);
        return 0;
    }

    printf("Base de datos restaurada exitosamente desde:\n");
    printf("  %s\n", source_path);
    printf("  -> %s\n", db_path);
    printf("\nIMPORTANTE: Reinicie la aplicacion para usar la base de datos restaurada.\n");

    snprintf(backup_dir, sizeof(backup_dir), "Restauracion completada desde: %.970s", source_path);
    app_log_event("BACKUP", backup_dir);
    pause_console();
    return 1;
}

int eliminar_backup(const char *filename)
{
    if (!filename || filename[0] == '\0')
    {
        printf("Error: Nombre de archivo invalido.\n");
        return 0;
    }

    app_log_event("BACKUP", "Inicio de eliminacion de backup");

    char backup_dir[MAX_BUFFER];
    if (!get_backup_dir(backup_dir, sizeof(backup_dir)))
    {
        printf("Error: No se pudo obtener el directorio de backups.\n");
        return 0;
    }

    char filepath[MAX_BUFFER];
    snprintf(filepath, sizeof(filepath), "%.*s%s%.*s",
             (int)((sizeof(filepath) - 2) / 2), backup_dir,
             BACKUP_PATH_SEP,
             (int)((sizeof(filepath) - 2) / 2), filename);

    int archivo_valido = 0;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(filepath);
    archivo_valido = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    archivo_valido = (stat(filepath, &st) == 0 && S_ISREG(st.st_mode));
#endif

    if (!archivo_valido)
    {
        printf("Error: El archivo de backup no existe:\n%s\n", filepath);
        app_log_event("BACKUP", "Archivo de backup no encontrado para eliminar");
        return 0;
    }

    cJSON *manifest = leer_manifest(backup_dir);
    int encontrado = backup_existe_en_manifest(manifest, filename);

    if (!encontrado)
    {
        cJSON_Delete(manifest);
        printf("Error: El backup no esta registrado en el manifiesto.\n");
        return 0;
    }

    printf("Desea eliminar el siguiente backup?\n");
    printf("  Archivo: %s\n", filename);

    if (!confirmar("  Esta accion es irreversible"))
    {
        cJSON_Delete(manifest);
        printf("Eliminacion cancelada.\n");
        app_log_event("BACKUP", "Eliminacion cancelada por el usuario");
        return 0;
    }

    if (remove(filepath) != 0)
    {
        cJSON_Delete(manifest);
        printf("Error: No se pudo eliminar el archivo de backup.\n");
        app_log_event("BACKUP", "Fallo eliminacion de archivo de backup");
        return 0;
    }

    int old_count = cJSON_GetArraySize(manifest);
    for (int i = old_count - 1; i >= 0; i--)
    {
        cJSON const *entry = cJSON_GetArrayItem(manifest, i);
        cJSON const *fname = cJSON_GetObjectItem(entry, "filename");
        if (fname && cJSON_IsString(fname) && strcmp(fname->valuestring, filename) == 0)
        {
            cJSON_DeleteItemFromArray(manifest, i);
            break;
        }
    }

    int ok = guardar_manifest(backup_dir, manifest);
    cJSON_Delete(manifest);

    if (!ok)
    {
        printf("Error: No se pudo actualizar el manifiesto de backups.\n");
        app_log_event("BACKUP", "No se pudo guardar el manifiesto tras eliminacion");
        return 0;
    }

    printf("Backup eliminado exitosamente:\n");
    printf("  %s\n", filename);

    snprintf(backup_dir, sizeof(backup_dir), "Backup eliminado: %.970s", filename);
    app_log_event("BACKUP", backup_dir);
    return 1;
}

static void mostrar_lista_backups(void)
{
    listar_backups();
}

static void pedir_y_crear_backup(void)
{
    char descripcion[256];
    input_string("Ingrese una descripcion para el backup (opcional): ", descripcion, (int)sizeof(descripcion));

    if (descripcion[0] == '\0')
    {
        crear_backup(NULL);
    }
    else
    {
        crear_backup(descripcion);
    }
    pause_console();
}

static void pedir_y_restaurar_backup(void)
{
    char backup_dir[MAX_BUFFER];
    if (!get_backup_dir(backup_dir, sizeof(backup_dir)))
    {
        mostrar_no_hay_registros("backups");
        pause_console();
        return;
    }

    cJSON *manifest = leer_manifest(backup_dir);
    if (!manifest || cJSON_GetArraySize(manifest) == 0)
    {
        if (manifest)
        {
            cJSON_Delete(manifest);
        }
        mostrar_no_hay_registros("backups");
        pause_console();
        return;
    }

    mostrar_pantalla("SELECCIONAR BACKUP A RESTAURAR");

    int count = cJSON_GetArraySize(manifest);
    int validos = 0;
    char *nombres[MAX_BUFFER];

    for (int i = 0; i < count; i++)
    {
        if (validos >= MAX_BUFFER) break;

        cJSON const *entry = cJSON_GetArrayItem(manifest, i);
        cJSON *fname = cJSON_GetObjectItem(entry, "filename");
        cJSON const *desc = cJSON_GetObjectItem(entry, "descripcion");
        cJSON const *fecha = cJSON_GetObjectItem(entry, "fecha");

        if (!fname || !cJSON_IsString(fname))
        {
            continue;
        }

        char filepath[MAX_BUFFER];
        snprintf(filepath, sizeof(filepath), "%.*s%s%.*s",
                 (int)((sizeof(filepath) - 2) / 2), backup_dir,
                 BACKUP_PATH_SEP,
                 (int)((sizeof(filepath) - 2) / 2), fname->valuestring);

        int existe = 0;
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(filepath);
        existe = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
        struct stat st;
        existe = (stat(filepath, &st) == 0 && S_ISREG(st.st_mode));
#endif

        if (!existe)
        {
            continue;
        }


        nombres[validos] = fname->valuestring;

        const char *dc = (desc && cJSON_IsString(desc)) ? desc->valuestring : "?";
        const char *fe = (fecha && cJSON_IsString(fecha)) ? fecha->valuestring : "?";

        printf("  %d. %s\n", validos + 1, nombres[validos]);
        printf("     Descripcion: %s\n", dc);
        printf("     Fecha: %s\n", fe);
        printf("     ----------------------------------------\n");
        validos++;
    }

    if (validos == 0)
    {
        cJSON_Delete(manifest);
        mostrar_no_hay_registros("backups");
        pause_console();
        return;
    }

    int seleccion = input_int_rango("Seleccione el numero de backup a restaurar (0 para cancelar): ", 0, validos);
    if (seleccion <= 0)
    {
        cJSON_Delete(manifest);
        printf("Restauracion cancelada.\n");
        pause_console();
        return;
    }

    const char *filename = nombres[seleccion - 1];
    cJSON_Delete(manifest);

    restaurar_backup(filename);
}

static void pedir_y_eliminar_backup(void)
{
    char backup_dir[MAX_BUFFER];
    if (!get_backup_dir(backup_dir, sizeof(backup_dir)))
    {
        mostrar_no_hay_registros("backups");
        pause_console();
        return;
    }

    cJSON *manifest = leer_manifest(backup_dir);
    if (!manifest || cJSON_GetArraySize(manifest) == 0)
    {
        if (manifest)
        {
            cJSON_Delete(manifest);
        }
        mostrar_no_hay_registros("backups");
        pause_console();
        return;
    }

    mostrar_pantalla("SELECCIONAR BACKUP A ELIMINAR");

    int count = cJSON_GetArraySize(manifest);
    int validos = 0;
    char *nombres[MAX_BUFFER];

    for (int i = 0; i < count; i++)
    {
        if (validos >= MAX_BUFFER) break;

        cJSON const *entry = cJSON_GetArrayItem(manifest, i);
        cJSON *fname = cJSON_GetObjectItem(entry, "filename");
        cJSON const *desc = cJSON_GetObjectItem(entry, "descripcion");
        cJSON const *fecha = cJSON_GetObjectItem(entry, "fecha");

        if (!fname || !cJSON_IsString(fname))
        {
            continue;
        }

        char filepath[MAX_BUFFER];
        snprintf(filepath, sizeof(filepath), "%.*s%s%.*s",
                 (int)((sizeof(filepath) - 2) / 2), backup_dir,
                 BACKUP_PATH_SEP,
                 (int)((sizeof(filepath) - 2) / 2), fname->valuestring);

        int existe = 0;
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(filepath);
        existe = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
        struct stat st;
        existe = (stat(filepath, &st) == 0 && S_ISREG(st.st_mode));
#endif

        if (!existe)
        {
            continue;
        }


        nombres[validos] = fname->valuestring;

        const char *dc = (desc && cJSON_IsString(desc)) ? desc->valuestring : "?";
        const char *fe = (fecha && cJSON_IsString(fecha)) ? fecha->valuestring : "?";

        printf("  %d. %s\n", validos + 1, nombres[validos]);
        printf("     Descripcion: %s\n", dc);
        printf("     Fecha: %s\n", fe);
        printf("     ----------------------------------------\n");
        validos++;
    }

    if (validos == 0)
    {
        cJSON_Delete(manifest);
        mostrar_no_hay_registros("backups");
        pause_console();
        return;
    }

    int seleccion = input_int_rango("Seleccione el numero de backup a eliminar (0 para cancelar): ", 0, validos);
    if (seleccion <= 0)
    {
        cJSON_Delete(manifest);
        printf("Eliminacion cancelada.\n");
        pause_console();
        return;
    }

    const char *filename = nombres[seleccion - 1];
    cJSON_Delete(manifest);

    eliminar_backup(filename);
    pause_console();
}

static void configurar_auto_backup(void)
{
    int intervalo = input_int("Intervalo en horas entre backups (ej: 24): ");
    if (intervalo < 1) intervalo = 24;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db,
                           "INSERT OR REPLACE INTO backup_config (id, intervalo_horas, proximo_backup, activo) "
                           "VALUES (1, ?, datetime('now','localtime'), 1)", -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, intervalo);
        if (sqlite3_step(stmt) == SQLITE_DONE)
            printf("Auto-backup configurado (cada %d horas).\n", intervalo);
        else
            printf("Error al configurar auto-backup: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
    }
    app_log_event("BACKUP", "Auto-backup configurado");
    pause_console();
}

static void mostrar_estado_auto_backup(void)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db,
                           "SELECT intervalo_horas, proximo_backup, activo FROM backup_config WHERE id = 1",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        mostrar_pantalla("ESTADO AUTO-BACKUP");
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("  Intervalo: %d horas\n", sqlite3_column_int(stmt, 0));
            printf("  Proximo backup: %s\n", sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "N/A");
            printf("  Activo: %s\n", sqlite3_column_int(stmt, 2) ? "SI" : "NO");
        }
        else
        {
            printf("  No configurado.\n");
        }
        sqlite3_finalize(stmt);
    }
    pause_console();
}

static void desactivar_auto_backup(void)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db,
                           "UPDATE backup_config SET activo = 0 WHERE id = 1",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        printf("Auto-backup desactivado.\n");
    }
    app_log_event("BACKUP", "Auto-backup desactivado");
    pause_console();
}

static void menu_auto_backup(void)
{
    MenuItem items[] =
    {
        {1, "Configurar Auto-Backup", &configurar_auto_backup},
        {2, "Estado Auto-Backup", &mostrar_estado_auto_backup},
        {3, "Desactivar Auto-Backup", &desactivar_auto_backup},
        {0, "Volver", NULL}
    };
    ejecutar_menu("AUTO-BACKUP", items, 4);
}

int verificar_backup_programado(void)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db,
                           "SELECT activo, proximo_backup, intervalo_horas FROM backup_config WHERE id = 1",
                           -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }
    int activo = sqlite3_column_int(stmt, 0);
    const char *proximo = (const char*)sqlite3_column_text(stmt, 1);
    int intervalo = sqlite3_column_int(stmt, 2);
    sqlite3_finalize(stmt);

    if (!activo || !proximo)
        return 0;

    time_t ahora = time(NULL);
    struct tm tm_prox = {0};

#ifdef _WIN32
    sscanf_s(proximo, "%d-%d-%d %d:%d",
#else
    sscanf(proximo, "%d-%d-%d %d:%d",
#endif
             &tm_prox.tm_year, &tm_prox.tm_mon, &tm_prox.tm_mday,
             &tm_prox.tm_hour, &tm_prox.tm_min);
    tm_prox.tm_year -= 1900;
    tm_prox.tm_mon -= 1;
    tm_prox.tm_isdst = -1;

    time_t t_prox = mktime(&tm_prox);
    if (t_prox == -1)
        return 0;

    if (difftime(ahora, t_prox) >= 0 && crear_backup("Auto-backup programado"))
    {
        /* Actualizar proximo backup usando el intervalo */
        sqlite3_stmt *upd;
        char sql_upd[256];
        snprintf(sql_upd, sizeof(sql_upd),
                 "UPDATE backup_config SET proximo_backup = datetime('now','localtime','+%d hours') WHERE id = 1",
                 intervalo);
        if (sqlite3_prepare_v2(db, sql_upd, -1, &upd, NULL) == SQLITE_OK)
        {
            sqlite3_step(upd);
            sqlite3_finalize(upd);
        }
        app_log_event("BACKUP", "Auto-backup ejecutado");
        printf("[Auto-Backup] Backup creado automaticamente.\n");
        return 1;
    }
    return 0;
}

void menu_backup_restore(void)
{
    MenuItem items[] =
    {
        {1, "Crear backup", &pedir_y_crear_backup},
        {2, "Listar backups", &mostrar_lista_backups},
        {3, "Restaurar backup", &pedir_y_restaurar_backup},
        {4, "Eliminar backup", &pedir_y_eliminar_backup},
        {5, "Auto-Backup", &menu_auto_backup},
        {0, "Volver", NULL}
    };
    ejecutar_menu("BACKUP Y RESTAURACION", items, 6);
}
