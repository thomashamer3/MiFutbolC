/**
 * @file db.c
 * @brief Módulo para la gestión de la base de datos SQLite
 *
 * Este archivo contiene las funciones necesarias para inicializar, configurar y cerrar
 * la conexión a la base de datos SQLite utilizada por la aplicación MiFutbolC.
 */

#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <direct.h>
#include <Windows.h>
#include <ShlObj.h>

static char error_buf[256];

#ifdef _WIN32
#define MKDIR(path) _mkdir(path)
#define STRDUP _strdup
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#define STRDUP strdup
#endif

/** Puntero global a la base de datos SQLite */
sqlite3 *db = NULL;

/** Directorio donde se almacena la base de datos */
static char DB_DIR[1024];

/** Ruta completa al archivo de la base de datos */
static char DB_PATH[1024];

/** Directorio de exportaciones */
static char EXPORT_DIR[1024];

/** Directorio de importaciones */
static char IMPORT_DIR[1024];

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

    // Crear directorios si no existen
    char temp_path[MAX_PATH];
    strcpy_s(temp_path, sizeof(temp_path), appdata_path);
    strcat_s(temp_path, sizeof(temp_path), "\\MiFutbolC");
    if (MKDIR(temp_path) != 0 && errno != EEXIST)
    {
        strerror_s(error_buf, sizeof(error_buf), errno);
        printf("Error creando directorio MiFutbolC: %s\n", error_buf);
        return 0;
    }

    if (MKDIR(DB_DIR) != 0 && errno != EEXIST)
    {
        strerror_s(errno, error_buf, sizeof(error_buf));
        printf("Error creando directorio data: %s\n", error_buf);
        return 0;
    }
#else
    // Para otros sistemas operativos, usar directorio actual
    memset(DB_DIR, 0, sizeof(DB_DIR));
    strcpy_s(DB_DIR, sizeof(DB_DIR), "./data");

    memset(DB_PATH, 0, sizeof(DB_PATH));
    strcpy_s(DB_PATH, sizeof(DB_PATH), "./data/mifutbol.db");

    // Crear directorio si no existe
    if (MKDIR(DB_DIR) != 0 && errno != EEXIST)
    {
        printf("Error creando directorio: %s\n", strerror(errno));
        return 0;
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
        return 0;
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
        " language INTEGER DEFAULT 0);"

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
        " mes_anio TEXT NOT NULL UNIQUE,"  // formato YYYY-MM
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
        " FOREIGN KEY(peor_equipo_mes) REFERENCES equipo(id));";

    if (sqlite3_exec(db, sql_create, 0, 0, 0) != SQLITE_OK)
    {
        printf("Error creando tablas\n");
        return 0;
    }
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
        "ALTER TABLE partido ADD COLUMN clima INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN dia INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN rendimiento_general INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN cansancio INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN estado_animo INTEGER DEFAULT 0;",
        "ALTER TABLE partido ADD COLUMN comentario_personal TEXT DEFAULT '';",
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
    if (!setup_database_paths()) return 0;
    if (!create_database_connection()) return 0;
    if (!create_database_schema()) return 0;
    add_missing_columns();

    // Crear directorios de importación y exportación al iniciar
    get_import_dir();
    get_export_dir();

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
        sqlite3_close(db);
}

/**
 * @brief Recupera identidad del usuario para personalización
 *
 * Permite adaptar la interfaz y mensajes según el usuario identificado,
 * mejorando la experiencia personalizada de la aplicación.
 *
 * @return Puntero a cadena con el nombre del usuario, o NULL si no existe
 */
char* get_user_name()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM usuario LIMIT 1;";
    char *nombre = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *temp = (const char*)sqlite3_column_text(stmt, 0);
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
int set_user_name(const char* nombre)
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
const char* get_data_dir()
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
const char* get_export_dir()
{
    if (EXPORT_DIR[0] == '\0')
    {
#ifdef _WIN32
        // Usar Documents para exportaciones (visible para el usuario)
        char documents_path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, documents_path) != S_OK)
        {
            printf("Error obteniendo Documents path\n");
            return NULL;
        }

        memset(EXPORT_DIR, 0, sizeof(EXPORT_DIR));
        strcpy_s(EXPORT_DIR, sizeof(EXPORT_DIR), documents_path);
        strcat_s(EXPORT_DIR, sizeof(EXPORT_DIR), "\\MiFutbolC\\Exportaciones");

        // Crear directorios si no existen
        char temp_path[MAX_PATH];
        strcpy_s(temp_path, sizeof(temp_path), documents_path);
        strcat_s(temp_path, sizeof(temp_path), "\\MiFutbolC");
        if (MKDIR(temp_path) != 0 && errno != EEXIST)
        {
            strerror_s(errno, error_buf, sizeof(error_buf));
            printf("Error creando directorio MiFutbolC en Documents: %s\n", error_buf);
            return NULL;
        }

        if (MKDIR(EXPORT_DIR) != 0 && errno != EEXIST)
        {
            strerror_s(errno, error_buf, sizeof(error_buf));
            printf("Error creando directorio Exportaciones: %s\n", error_buf);
            return NULL;
        }
#else
        // Para otros sistemas operativos
        strcpy_s(EXPORT_DIR, sizeof(EXPORT_DIR), "./exportaciones");
        if (MKDIR(EXPORT_DIR) != 0 && errno != EEXIST)
        {
            printf("Error creando directorio exportaciones: %s\n", strerror(errno));
            return NULL;
        }
#endif
    }
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
const char* get_import_dir()
{
    if (IMPORT_DIR[0] == '\0')
    {
#ifdef _WIN32
        // Usar Documents para importaciones (visible para el usuario)
        char documents_path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, documents_path) != S_OK)
        {
            printf("Error obteniendo Documents path\n");
            return NULL;
        }

        memset(IMPORT_DIR, 0, sizeof(IMPORT_DIR));
        strcpy_s(IMPORT_DIR, sizeof(IMPORT_DIR), documents_path);
        strcat_s(IMPORT_DIR, sizeof(IMPORT_DIR), "\\MiFutbolC\\Importaciones");

        // Crear directorios si no existen
        char temp_path[MAX_PATH];
        strcpy_s(temp_path, sizeof(temp_path), documents_path);
        strcat_s(temp_path, sizeof(temp_path), "\\MiFutbolC");
        if (MKDIR(temp_path) != 0 && errno != EEXIST)
        {
            strerror_s(errno, error_buf, sizeof(error_buf));
            printf("Error creando directorio MiFutbolC en Documents: %s\n", error_buf);
            return NULL;
        }

        if (MKDIR(IMPORT_DIR) != 0 && errno != EEXIST)
        {
            strerror_s(errno, error_buf, sizeof(error_buf));
            printf("Error creando directorio Importaciones: %s\n", error_buf);
            return NULL;
        }
#else
        // Para otros sistemas operativos
        strcpy_s(IMPORT_DIR, sizeof(IMPORT_DIR), "./importaciones");
        if (MKDIR(IMPORT_DIR) != 0 && errno != EEXIST)
        {
            printf("Error creando directorio importaciones: %s\n", strerror(errno));
            return NULL;
        }
#endif
    }
    return IMPORT_DIR;
}
