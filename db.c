/**
 * @file db.c
 * @brief Módulo para la gestión de la base de datos SQLite
 *
 * Este archivo contiene las funciones necesarias para inicializar, configurar y cerrar
 * la conexión a la base de datos SQLite utilizada por la aplicación MiFutbolC.
 */
#include "export.h"
#include "db.h"
#include "utils.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <ShlObj.h>
#else
#include "direct.h"
#include "compat_windows.h"
#include "ShlObj.h"
#endif

#ifdef _WIN32
#define MKDIR(path) _mkdir(path)
#define STRDUP _strdup
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#define STRDUP strdup

static int directorio_existe(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

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

static void app_log_write(const char *level, const char *component, const char *message)
{
    if (!level || !component || !message || LOG_PATH[0] == '\0')
    {
        return;
    }

    FILE *log_file = NULL;
#ifdef _WIN32
    if (fopen_s(&log_file, LOG_PATH, "a") != 0 || !log_file)
    {
        return;
    }
#else
    log_file = fopen(LOG_PATH, "a");
    if (!log_file)
    {
        return;
    }
#endif

    char timestamp[32] = {0};
    build_timestamp(timestamp, sizeof(timestamp));
    fprintf(log_file, "[%s] [%s] [%s] %s\n", timestamp, level, component, message);
    fclose(log_file);
}

void app_log_event(const char *component, const char *message)
{
    app_log_write("INFO", component, message);
}

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
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "No se pudo crear directorio %.120s (%.420s): %.420s", nombre, path, error_buf);
        app_log_write("ERROR", "FS", log_msg);
#else
        printf("Error creando directorio %s: %s\n", nombre, strerror(errno));
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "No se pudo crear directorio %.120s (%.420s): %.420s", nombre, path, strerror(errno));
        app_log_write("ERROR", "FS", log_msg);
#endif
        return 0;
    }
    if (!already_exists)
    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Directorio disponible: %.120s (%.860s)", nombre, path);
        app_log_write("INFO", "FS", log_msg);
    }
    return 1;
}

#ifdef _WIN32
static int configurar_directorio_documentos(const char *subdir, char *out_dir, size_t out_size,
        const char *nombre_principal, const char *nombre_subdir)
{
    char documents_path[MAX_PATH];
    /*
     * Se utiliza SHGetFolderPathA con CSIDL_PERSONAL para obtener la ruta a "Mis Documentos".
     * Esta ubicación se reserva para archivos que el usuario debe manipular manualmente,
     * como las exportaciones e importaciones de datos.
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

static CopyResult copiar_archivo(const char *source_path, const char *dest_path)
{
    FILE *src = NULL;
    FILE *dst = NULL;
#ifdef _WIN32
    if (fopen_s(&src, source_path, "rb") != 0 || !src)
    {
        return COPY_SRC_ERROR;
    }

    if (fopen_s(&dst, dest_path, "wb") != 0 || !dst)
    {
        fclose(src);
        return COPY_DST_ERROR;
    }
#else
    src = fopen(source_path, "rb");
    if (!src)
    {
        return COPY_SRC_ERROR;
    }

    dst = fopen(dest_path, "wb");
    if (!dst)
    {
        fclose(src);
        return COPY_DST_ERROR;
    }
#endif

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

/**
 * @brief Configura rutas y directorios para almacenamiento de datos
 *
 * Establece ubicaciones específicas del sistema operativo para
 * base de datos interna y directorios de usuario accesibles.
 *
 * @return 1 si configuración exitosa, 0 en caso de error
 */
static int setup_database_paths()
{
#ifdef _WIN32
    // Usar AppData\Local para la base de datos (oculta, interna)
    char appdata_path[MAX_PATH];
    /*
     * La base de datos se almacena en CSIDL_LOCAL_APPDATA para garantizar que
     * no sea borrada accidentalmente por el usuario y que el sistema tenga
     * permisos de escritura consistentes sin requerir privilegios de administrador.
     */
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata_path) != S_OK)
    {
        printf("Error obteniendo AppData path\n");
        return 0;
    }

    memset(DB_DIR, 0, sizeof(DB_DIR));
    strcpy_s(DB_DIR, sizeof(DB_DIR), appdata_path);
    strcat_s(DB_DIR, sizeof(DB_DIR), "\\MiFutbolC\\data");

    memset(DB_PATH, 0, sizeof(DB_PATH));
    strcpy_s(DB_PATH, sizeof(DB_PATH), appdata_path);
    strcat_s(DB_PATH, sizeof(DB_PATH), "\\MiFutbolC\\data\\mifutbol.db");

    memset(LOG_PATH, 0, sizeof(LOG_PATH));
    strcpy_s(LOG_PATH, sizeof(LOG_PATH), appdata_path);
    strcat_s(LOG_PATH, sizeof(LOG_PATH), "\\MiFutbolC\\data\\mifutbol.log");

    // Crear directorios si no existen
    char temp_path[MAX_PATH];
    strcpy_s(temp_path, sizeof(temp_path), appdata_path);
    strcat_s(temp_path, sizeof(temp_path), "\\MiFutbolC");
    if (!asegurar_directorio(temp_path, "MiFutbolC"))
    {
        return 0;
    }

    if (!asegurar_directorio(DB_DIR, "data"))
    {
        return 0;
    }

    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Ruta de datos: %.1000s", DB_DIR);
        app_log_write("INFO", "PATHS", log_msg);
        snprintf(log_msg, sizeof(log_msg), "Ruta de DB: %.1003s", DB_PATH);
        app_log_write("INFO", "PATHS", log_msg);
        snprintf(log_msg, sizeof(log_msg), "Ruta de log: %.1002s", LOG_PATH);
        app_log_write("INFO", "PATHS", log_msg);
    }
#else
    // Para otros sistemas operativos, usar directorio actual
    memset(DB_DIR, 0, sizeof(DB_DIR));
    strcpy_s(DB_DIR, sizeof(DB_DIR), "./data");

    memset(DB_PATH, 0, sizeof(DB_PATH));
    strcpy_s(DB_PATH, sizeof(DB_PATH), "./data/mifutbol.db");

    memset(LOG_PATH, 0, sizeof(LOG_PATH));
    strcpy_s(LOG_PATH, sizeof(LOG_PATH), "./data/mifutbol.log");

    // Crear directorio si no existe
    if (!asegurar_directorio(DB_DIR, "data"))
    {
        return 0;
    }

    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Ruta de datos: %.1000s", DB_DIR);
        app_log_write("INFO", "PATHS", log_msg);
        snprintf(log_msg, sizeof(log_msg), "Ruta de DB: %.1003s", DB_PATH);
        app_log_write("INFO", "PATHS", log_msg);
        snprintf(log_msg, sizeof(log_msg), "Ruta de log: %.1002s", LOG_PATH);
        app_log_write("INFO", "PATHS", log_msg);
    }
#endif
    return 1;
}

/**
 * @brief Establece conexión activa con base de datos SQLite
 *
 * Abre archivo de base de datos y configura puntero global para
 * operaciones subsiguientes de consulta y modificación.
 *
 * @return 1 si conexión exitosa, 0 en caso de error
 */
static int create_database_connection()
{
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK)
    {
        printf("Error abriendo DB: %s\n", sqlite3_errmsg(db));
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Error abriendo DB %.700s: %.280s", DB_PATH, sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_msg);
        return 0;
    }
    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Conexion SQLite abierta en %.996s", DB_PATH);
        app_log_write("INFO", "DB", log_msg);
    }
    return 1;
}

/**
 * @brief Crea esquema completo de tablas si no existen
 *
 * Define estructura relacional completa incluyendo todas las entidades,
 * restricciones de integridad referencial y valores por defecto.
 *
 * @return 1 si creación exitosa, 0 en caso de error
 */
static int create_database_schema()
{
    const char *sql_create =
        "CREATE TABLE IF NOT EXISTS camiseta ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " sorteada INTEGER DEFAULT 0);"

        "CREATE TABLE IF NOT EXISTS cancha ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS partido ("
        " id INTEGER PRIMARY KEY,"
        " cancha_id INTEGER NOT NULL,"
        " fecha_hora TEXT NOT NULL,"
        " goles INTEGER NOT NULL,"
        " asistencias INTEGER NOT NULL,"
        " camiseta_id INTEGER NOT NULL,"
        " resultado INTEGER DEFAULT 0,"
        " rendimiento_general INTEGER DEFAULT 0,"
        " cansancio INTEGER DEFAULT 0,"
        " estado_animo INTEGER DEFAULT 0,"
        " comentario_personal TEXT DEFAULT '',"
        " clima INTEGER DEFAULT 0,"
        " dia INTEGER DEFAULT 0,"
        " precio INTEGER DEFAULT 0,"
        " FOREIGN KEY(cancha_id) REFERENCES cancha(id),"
        " FOREIGN KEY(camiseta_id) REFERENCES camiseta(id));"

        "CREATE TABLE IF NOT EXISTS lesion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " jugador TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " descripcion TEXT NOT NULL,"
        " fecha TEXT NOT NULL,"
        " camiseta_id INTEGER NOT NULL,"
        " estado TEXT DEFAULT 'Activa',"
        " FOREIGN KEY(camiseta_id) REFERENCES camiseta(id));"

        "CREATE TABLE IF NOT EXISTS usuario ("
        " id INTEGER PRIMARY KEY,"
        " nombre TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS equipo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " tipo INTEGER NOT NULL,"
        " tipo_futbol INTEGER NOT NULL,"
        " num_jugadores INTEGER NOT NULL,"
        " partido_id INTEGER DEFAULT -1);"

        "CREATE TABLE IF NOT EXISTS jugador ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " equipo_id INTEGER NOT NULL,"
        " nombre TEXT NOT NULL,"
        " numero INTEGER NOT NULL,"
        " posicion INTEGER NOT NULL,"
        " es_capitan INTEGER NOT NULL,"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id));"

        "CREATE TABLE IF NOT EXISTS torneo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " tiene_equipo_fijo INTEGER NOT NULL,"
        " equipo_fijo_id INTEGER DEFAULT -1,"
        " cantidad_equipos INTEGER NOT NULL,"
        " tipo_torneo INTEGER NOT NULL,"
        " formato_torneo INTEGER NOT NULL,"
        " fase_actual TEXT DEFAULT 'Fase de Grupos');"

        "CREATE TABLE IF NOT EXISTS equipo_torneo ("
        " torneo_id INTEGER NOT NULL,"
        " equipo_id INTEGER NOT NULL,"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id),"
        " PRIMARY KEY(torneo_id, equipo_id));"

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
        " FOREIGN KEY(equipo2_id) REFERENCES equipo(id));"

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
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id));"

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
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id));"

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
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id));"

        "CREATE TABLE IF NOT EXISTS torneo_fases ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " torneo_id INTEGER NOT NULL,"
        " nombre_fase TEXT NOT NULL,"
        " descripcion TEXT,"
        " orden INTEGER NOT NULL,"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id));"

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
        " FOREIGN KEY(fase_id) REFERENCES torneo_fases(id));"

        "CREATE TABLE IF NOT EXISTS settings ("
        " id INTEGER PRIMARY KEY,"
        " theme INTEGER DEFAULT 0,"
        " language INTEGER DEFAULT 0,"
        " mode INTEGER DEFAULT 0,"
        " text_size INTEGER DEFAULT 1);"

        "CREATE TABLE IF NOT EXISTS financiamiento ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo INTEGER NOT NULL,"
        " categoria INTEGER NOT NULL,"
        " descripcion TEXT NOT NULL,"
        " monto REAL NOT NULL,"
        " item_especifico TEXT);"

        "CREATE TABLE IF NOT EXISTS presupuesto_mensual ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " mes_anio TEXT NOT NULL UNIQUE," // formato YYYY-MM
        " presupuesto_total INTEGER NOT NULL,"
        " limite_gasto INTEGER NOT NULL,"
        " alertas_habilitadas INTEGER DEFAULT 1,"
        " fecha_creacion TEXT NOT NULL,"
        " fecha_modificacion TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS comparacion_historial ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " tipo_comparacion INTEGER NOT NULL,"
        " entidad_a_id INTEGER NOT NULL,"
        " entidad_b_id INTEGER NOT NULL,"
        " score_a REAL,"
        " score_b REAL,"
        " ganador INTEGER,"
        " fecha TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS temporada ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " anio INTEGER NOT NULL,"
        " fecha_inicio TEXT NOT NULL,"
        " fecha_fin TEXT NOT NULL,"
        " estado TEXT DEFAULT 'Planificada',"
        " descripcion TEXT);"

        "CREATE TABLE IF NOT EXISTS temporada_fase ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " temporada_id INTEGER NOT NULL,"
        " nombre TEXT NOT NULL,"
        " tipo_fase TEXT NOT NULL,"
        " fecha_inicio TEXT NOT NULL,"
        " fecha_fin TEXT NOT NULL,"
        " descripcion TEXT,"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));"

        "CREATE TABLE IF NOT EXISTS torneo_temporada ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " torneo_id INTEGER NOT NULL,"
        " temporada_id INTEGER NOT NULL,"
        " fase_id INTEGER,"
        " orden_en_temporada INTEGER,"
        " FOREIGN KEY(torneo_id) REFERENCES torneo(id),"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id),"
        " FOREIGN KEY(fase_id) REFERENCES temporada_fase(id));"

        "CREATE TABLE IF NOT EXISTS equipo_temporada_fatiga ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " equipo_id INTEGER NOT NULL,"
        " temporada_id INTEGER NOT NULL,"
        " fecha TEXT NOT NULL,"
        " fatiga_acumulada REAL DEFAULT 0,"
        " partidos_jugados INTEGER DEFAULT 0,"
        " rendimiento_promedio REAL DEFAULT 0,"
        " FOREIGN KEY(equipo_id) REFERENCES equipo(id),"
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));"

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
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));"

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
        " FOREIGN KEY(temporada_id) REFERENCES temporada(id));"

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
        " FOREIGN KEY(mejor_goleador_jugador_id) REFERENCES jugador(id));"

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
        " FOREIGN KEY(peor_equipo_mes) REFERENCES equipo(id));"

        "CREATE TABLE IF NOT EXISTS bienestar_objetivo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " fecha_inicio TEXT NOT NULL,"
        " fecha_fin TEXT NOT NULL,"
        " estado TEXT NOT NULL DEFAULT 'Activo',"
        " notas TEXT DEFAULT '');"

        "CREATE TABLE IF NOT EXISTS bienestar_plan_entrenamiento ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " objetivo_id INTEGER NOT NULL,"
        " frecuencia_semanal INTEGER NOT NULL,"
        " rutina_semanal TEXT NOT NULL,"
        " notas TEXT DEFAULT '',"
        " FOREIGN KEY(objetivo_id) REFERENCES bienestar_objetivo(id));"

        "CREATE TABLE IF NOT EXISTS bienestar_entrenamiento ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " duracion_min INTEGER NOT NULL,"
        " intensidad INTEGER NOT NULL,"
        " omitido INTEGER DEFAULT 0,"
        " notas TEXT DEFAULT '');"

        "CREATE TABLE IF NOT EXISTS bienestar_ejercicio ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " grupo_muscular TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS bienestar_entrenamiento_ejercicio ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " entrenamiento_id INTEGER NOT NULL,"
        " ejercicio_id INTEGER NOT NULL,"
        " series INTEGER DEFAULT 0,"
        " repeticiones INTEGER DEFAULT 0,"
        " tiempo_min INTEGER DEFAULT 0,"
        " FOREIGN KEY(entrenamiento_id) REFERENCES bienestar_entrenamiento(id),"
        " FOREIGN KEY(ejercicio_id) REFERENCES bienestar_ejercicio(id));"

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
        " tipo_diario TEXT DEFAULT '');"

        "CREATE TABLE IF NOT EXISTS bienestar_comida ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " calidad TEXT NOT NULL,"
        " descripcion TEXT DEFAULT '');"

        "CREATE TABLE IF NOT EXISTS bienestar_dia_nutricional ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL UNIQUE,"
        " hidratacion TEXT NOT NULL,"
        " alcohol INTEGER DEFAULT 0,"
        " peso_corporal REAL DEFAULT NULL,"
        " notas TEXT DEFAULT '');"

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
        " FOREIGN KEY(partido_id) REFERENCES partido(id));"

        "CREATE TABLE IF NOT EXISTS bienestar_salud ("
        " id INTEGER PRIMARY KEY CHECK (id = 1),"
        " altura_cm REAL DEFAULT NULL,"
        " peso_kg REAL DEFAULT NULL,"
        " tipo_sangre TEXT DEFAULT '',"
        " ultima_revision TEXT DEFAULT '',"
        " medidas TEXT DEFAULT '',"
        " notas TEXT DEFAULT '');"

        "CREATE TABLE IF NOT EXISTS bienestar_control_medico ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " tipo TEXT NOT NULL,"
        " profesional TEXT DEFAULT '',"
        " resultado TEXT DEFAULT '',"
        " notas TEXT DEFAULT '');"

        "CREATE TABLE IF NOT EXISTS bienestar_recomendacion ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " fecha TEXT NOT NULL,"
        " score_preparacion INTEGER DEFAULT 0,"
        " riesgo_lesion INTEGER DEFAULT 0,"
        " resumen TEXT DEFAULT '',"
        " rutina TEXT DEFAULT '');"

        "CREATE TABLE IF NOT EXISTS tactica_diagrama ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " partido_id INTEGER NOT NULL,"
        " nombre TEXT NOT NULL,"
        " fecha TEXT NOT NULL,"
        " grid TEXT NOT NULL,"
        " FOREIGN KEY(partido_id) REFERENCES partido(id));";

    if (sqlite3_exec(db, sql_create, 0, 0, 0) != SQLITE_OK)
    {
        printf("Error creando tablas\n");
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Error creando esquema: %s", sqlite3_errmsg(db));
        app_log_write("ERROR", "DB", log_msg);
        return 0;
    }
    app_log_write("INFO", "DB", "Esquema validado/creado");
    return 1;
}

/**
 * @brief Agrega columnas faltantes por evolución del esquema
 *
 * Ejecuta sentencias ALTER TABLE para añadir campos nuevos sin
 * afectar datos existentes, ignorando errores de columnas duplicadas.
 */
static void add_missing_columns()
{
    const char *alter_statements[] =
    {
        "ALTER TABLE camiseta ADD COLUMN sorteada INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN resultado INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN rendimiento_general INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN cansancio INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN estado_animo INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN comentario_personal TEXT DEFAULT '';",
        "ALTER TABLE partido ADD COLUMN clima INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN dia INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN precio INTEGER DEFAULT 0;",
        "ALTER TABLE lesion ADD COLUMN partido_id INTEGER DEFAULT NULL;",
        NULL
    };

    for (int i = 0; alter_statements[i] != NULL; i++)
    {
        sqlite3_exec(db, alter_statements[i], 0, 0, 0); // Ignore errors if column already exists
    }
}

/**
 * @brief Inicializa el entorno completo de persistencia de datos
 *
 * Orquesta configuración de rutas, conexión a base de datos,
 * creación de esquema y preparación de directorios auxiliares.
 *
 * @return 1 si inicialización completa exitosa, 0 en caso de error
 */
int db_init()
{
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

    if (!create_database_schema())
    {
        app_log_write("ERROR", "APP", "Fallo create_database_schema");
        return 0;
    }

    add_missing_columns();
    app_log_write("INFO", "DB", "Migraciones de columnas aplicadas");

    // Crear directorios de importación y exportación al iniciar
    get_import_dir();
    get_export_dir();

    app_log_write("INFO", "APP", "Inicializacion de base de datos completada");

    return 1;
}

/**
 * @brief Libera recursos de conexión a base de datos
 *
 * Evita fugas de memoria cerrando conexiones SQLite activas
 * de manera ordenada durante el cierre de la aplicación.
 */
void db_close()
{
    if (db)
    {
        app_log_write("INFO", "DB", "Cerrando conexion SQLite");
        sqlite3_close(db);
        app_log_write("INFO", "DB", "Conexion SQLite cerrada");
    }
}

/**
 * @brief Recupera identidad del usuario para personalización
 *
 * Permite adaptar la interfaz y mensajes según el usuario identificado,
 * mejorando la experiencia personalizada de la aplicación.
 *
 * @return Puntero a cadena con el nombre del usuario, o NULL si no existe
 */
char *get_user_name()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM usuario LIMIT 1;";
    char *nombre = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *temp = (const char *)sqlite3_column_text(stmt, 0);
            if (temp)
            {
                nombre = STRDUP(temp);
            }
        }
        sqlite3_finalize(stmt);
    }

    return nombre;
}

/**
 * @brief Persiste identidad del usuario para sesiones futuras
 *
 * Almacena nombre de usuario en tabla dedicada para mantener
 * consistencia en configuraciones personales entre ejecuciones.
 *
 * @param nombre El nombre del usuario a guardar
 * @return 1 si la operación fue exitosa, 0 en caso de error
 */
int set_user_name(const char *nombre)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO usuario (id, nombre) VALUES (1, ?);";
    int result = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            result = 1;
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

/**
 * @brief Proporciona acceso al directorio de almacenamiento interno
 *
 * Facilita operaciones de archivo que requieren ubicación de datos
 * persistentes, manteniendo separación entre datos de aplicación y usuario.
 *
 * @return Puntero a cadena con la ruta del directorio de datos
 */
const char *get_data_dir()
{
    return DB_DIR;
}

/**
 * @brief Establece ubicación accesible para archivos exportados
 *
 * Configura directorio visible al usuario para almacenamiento de
 * datos exportados, permitiendo fácil acceso y backup de información.
 *
 * @return Puntero a cadena con la ruta del directorio de exportaciones
 */
const char *get_export_dir()
{
#ifdef _WIN32
    // Usar Documents para exportaciones (visible para el usuario)
    if (EXPORT_DIR[0] == '\0' &&
            !configurar_directorio_documentos("Exportaciones", EXPORT_DIR, sizeof(EXPORT_DIR),
                    "MiFutbolC en Documents", "Exportaciones"))
    {
        return NULL;
    }
#else
    if (EXPORT_DIR[0] == '\0')
    {
        // Para otros sistemas operativos
        strcpy_s(EXPORT_DIR, sizeof(EXPORT_DIR), "./exportaciones");
        if (!asegurar_directorio(EXPORT_DIR, "exportaciones"))
        {
            return NULL;
        }
    }
#endif
    return EXPORT_DIR;
}

/**
 * @brief Establece ubicación accesible para archivos a importar
 *
 * Configura directorio visible al usuario para colocación de
 * archivos de importación, facilitando carga masiva de datos.
 *
 * @return Puntero a cadena con la ruta del directorio de importaciones
 */
const char *get_import_dir()
{
#ifdef _WIN32
    // Usar Documents para importaciones (visible para el usuario)
    if (IMPORT_DIR[0] == '\0' &&
            !configurar_directorio_documentos("Importaciones", IMPORT_DIR, sizeof(IMPORT_DIR),
                    "MiFutbolC en Documents", "Importaciones"))
    {
        return NULL;
    }
#else
    if (IMPORT_DIR[0] == '\0')
    {
        // En Linux/macOS priorizar "Importaciones" y mantener compatibilidad
        // con instalaciones anteriores que usaban "importaciones".
        const char *import_dir_preferido = "./Importaciones";
        const char *import_dir_legado = "./importaciones";

        if (directorio_existe(import_dir_preferido))
        {
            strcpy_s(IMPORT_DIR, sizeof(IMPORT_DIR), import_dir_preferido);
        }
        else if (directorio_existe(import_dir_legado))
        {
            strcpy_s(IMPORT_DIR, sizeof(IMPORT_DIR), import_dir_legado);
        }
        else
        {
            strcpy_s(IMPORT_DIR, sizeof(IMPORT_DIR), import_dir_preferido);
        }

        if (!asegurar_directorio(IMPORT_DIR, "Importaciones"))
        {
            return NULL;
        }
    }
#endif
    return IMPORT_DIR;
}

/**
 * @brief Copia la base de datos SQLite a la carpeta de documentos
 *
 * Esta función realiza una copia exacta del archivo de base de datos
 * desde la ubicación de datos internos (AppData) a la carpeta de exportación
 * (Documentos del usuario) para respaldo y portabilidad.
 */
void exportar_base_datos()
{
    app_log_write("INFO", "EXPORT", "Inicio de exportacion de base de datos");

    const char *dest_path = get_export_path("mifutbol.db");
    if (!dest_path)
    {
        printf("Error: No se pudo obtener la ruta de destino para exportar la base de datos.\n");
        app_log_write("ERROR", "EXPORT", "No se pudo obtener ruta de destino para exportacion");
        pause_console();
        return;
    }

    if (!db)
    {
        // Fallback cuando no hay conexión abierta.
        const char *source_path = DB_PATH;
        CopyResult result = copiar_archivo(source_path, dest_path);
        if (result == COPY_SRC_ERROR)
        {
            printf("Error: No se encontro la base de datos en:\n%s\n", source_path);
            char log_msg[1024] = {0};
            snprintf(log_msg, sizeof(log_msg), "No se encontro DB origen para exportar: %.980s", source_path);
            app_log_write("ERROR", "EXPORT", log_msg);
            pause_console();
            return;
        }

        if (result == COPY_DST_ERROR)
        {
            printf("Error creando archivo destino:\n%s\n", dest_path);
            char log_msg[1024] = {0};
            snprintf(log_msg, sizeof(log_msg), "No se pudo abrir DB destino para exportar: %.977s", dest_path);
            app_log_write("ERROR", "EXPORT", log_msg);
            pause_console();
            return;
        }

        printf("Base de datos exportada a:\n%s\n", dest_path);
        {
            char log_msg[1024] = {0};
            snprintf(log_msg, sizeof(log_msg), "Exportacion finalizada (copia directa): %.983s", dest_path);
            app_log_write("INFO", "EXPORT", log_msg);
        }
        pause_console();
        return;
    }

    // Exportación segura con SQLite backup API (evita errores por archivo en uso).
    sqlite3 *dest_db = NULL;
    if (sqlite3_open(dest_path, &dest_db) != SQLITE_OK)
    {
        printf("Error abriendo base de datos destino:\n%s\n", dest_path);
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Error abriendo DB destino (%.700s): %.260s", dest_path, sqlite3_errmsg(dest_db));
        app_log_write("ERROR", "EXPORT", log_msg);
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
        printf("Error iniciando backup SQLite: %s\n", sqlite3_errmsg(dest_db));
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Error iniciando backup SQLite: %s", sqlite3_errmsg(dest_db));
        app_log_write("ERROR", "EXPORT", log_msg);
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
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Error SQLite backup final_rc=%d (%s)", final_rc, sqlite3_errstr(final_rc));
        app_log_write("ERROR", "EXPORT", log_msg);
        pause_console();
        return;
    }

    printf("Base de datos exportada a:\n%s\n", dest_path);
    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Exportacion finalizada (SQLite backup): %.982s", dest_path);
        app_log_write("INFO", "EXPORT", log_msg);
    }
    pause_console();
}

int backup_base_datos_automatico(const char *motivo)
{
    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Inicio de backup automatico. Motivo: %s", (motivo && motivo[0] != '\0') ? motivo : "sin_motivo");
        app_log_write("INFO", "BACKUP", log_msg);
    }

    const char *source_path = DB_PATH;
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        printf("%s\n", get_text("backup_failed"));
        app_log_write("ERROR", "BACKUP", "No se pudo resolver directorio de exportacion");
        return 0;
    }

    char backup_dir[1024];
    strcpy_s(backup_dir, sizeof(backup_dir), export_dir);
    strcat_s(backup_dir, sizeof(backup_dir), "\\Backups");

    if (!asegurar_directorio(backup_dir, "Backups"))
    {
        printf("%s\n", get_text("backup_failed"));
        {
            char log_msg[1024] = {0};
            snprintf(log_msg, sizeof(log_msg), "No se pudo crear/acceder a directorio de backups: %.972s", backup_dir);
            app_log_write("ERROR", "BACKUP", log_msg);
        }
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
    }

    size_t used = 0;
    dest_path[0] = '\0';

    if (!append_str(dest_path, &used, sizeof(dest_path), backup_dir) ||
            !append_str(dest_path, &used, sizeof(dest_path), "\\") ||
            !append_str(dest_path, &used, sizeof(dest_path), prefix))
    {
        printf("%s\n", get_text("backup_failed"));
        app_log_write("ERROR", "BACKUP", "No se pudo construir ruta base de backup");
        return 0;
    }

    if (motivo_safe[0] != '\0' &&
            (!append_str(dest_path, &used, sizeof(dest_path), motivo_safe) ||
             !append_str(dest_path, &used, sizeof(dest_path), "_")))
    {
        printf("%s\n", get_text("backup_failed"));
        app_log_write("ERROR", "BACKUP", "No se pudo agregar motivo a ruta de backup");
        return 0;
    }

    if (!append_str(dest_path, &used, sizeof(dest_path), timestamp) ||
            !append_str(dest_path, &used, sizeof(dest_path), ext))
    {
        printf("%s\n", get_text("backup_failed"));
        app_log_write("ERROR", "BACKUP", "No se pudo completar nombre de archivo backup");
        return 0;
    }

    CopyResult result = copiar_archivo(source_path, dest_path);
    if (result != COPY_OK)
    {
        printf("%s\n", get_text("backup_failed"));
        {
            char log_msg[1024] = {0};
            snprintf(log_msg, sizeof(log_msg), "Fallo copia de backup desde %.470s hacia %.470s", source_path, dest_path);
            app_log_write("ERROR", "BACKUP", log_msg);
        }
        return 0;
    }

    printf("%s\n%s\n", get_text("backup_created"), dest_path);
    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Backup automatico completado: %.992s", dest_path);
        app_log_write("INFO", "BACKUP", log_msg);
    }
    return 1;
}

void importar_base_datos()
{
    app_log_write("INFO", "IMPORT", "Inicio de importacion de base de datos");

    const char *import_dir = get_import_dir();
    if (!import_dir)
    {
        printf("Error obteniendo directorio de importaciones\n");
        app_log_write("ERROR", "IMPORT", "No se pudo obtener directorio de importaciones");
        return;
    }

    char source_path[1024];
    strcpy_s(source_path, sizeof(source_path), import_dir);
    strcat_s(source_path, sizeof(source_path), "\\mifutbol.db");

    const char *dest_path = DB_PATH;

    // Verificar que exista el archivo a importar
    CopyResult result = copiar_archivo(source_path, dest_path);
    if (result == COPY_SRC_ERROR)
    {
        printf("Error: No se encontró el archivo a importar en:\n%s\n", source_path);
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "No se encontro archivo de importacion: %.983s", source_path);
        app_log_write("ERROR", "IMPORT", log_msg);
        return;
    }

    if (result == COPY_DST_ERROR)
    {
        printf("Error: No se pudo abrir la base de datos destino:\n%s\n", dest_path);
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "No se pudo abrir DB destino para importar: %.977s", dest_path);
        app_log_write("ERROR", "IMPORT", log_msg);
        return;
    }

    printf("Base de datos importada correctamente.\n");
    printf("Origen: %s\n", source_path);
    printf("Destino: %s\n", dest_path);
    printf("Reinicia la aplicación para usar la nueva base.\n");

    {
        char log_msg[1024] = {0};
        snprintf(log_msg, sizeof(log_msg), "Importacion completada. Origen: %.470s | Destino: %.470s", source_path, dest_path);
        app_log_write("INFO", "IMPORT", log_msg);
    }

    pause_console();
}
