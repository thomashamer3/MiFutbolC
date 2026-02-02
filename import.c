/**
 * @file import.c
 * @brief Módulo para importar datos desde archivos JSON a la base de datos.
 */

#include "import.h"
#include "cJSON.h"
#include "db.h"
#include "menu.h"
#include "sqlite3.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration - Estructura para agrupar datos de un partido
typedef struct
{
    int partido_id;
    sqlite3_int64 cancha_id;
    const char *fecha;
    int goles;
    int asistencias;
    int camiseta_id;
    int resultado;
    int clima;
    int dia;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    const char *comentario_personal;
} PartidoData;

// Estructura para entrada de datos de partido (para reducir parámetros)
typedef struct
{
    const char *cancha;
    const char *fecha;
    int goles;
    int asistencias;
    const char *camiseta;
    int resultado;
    int clima;
    int dia;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    const char *comentario;
} PartidoInput;

// Macro para obtener camiseta ID (debe definirse antes de las declaraciones forward)
#define obtener_camiseta_id(nombre) obtener_id_por_nombre("camiseta", nombre)

// Forward declaration
static char *read_file_content(const char *filename);
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt);
static int id_existe_en_tabla(const char *tabla, int id);
static void insertar_camiseta(int id, const char *nombre);
static void insertar_lesion(int id, const char *jugador, const char *tipo,
                            const char *descripcion, const char *fecha);
static void importar_con_pausa(const char *inicio, const char *fin,
                               void (*func)());
static bool crear_tabla_estadisticas();
static int obtener_camiseta_id_estadistica(const char *camiseta_nombre);
static bool estadistica_existe(int camiseta_id);
static void insertar_estadistica(int camiseta_id, int goles, int asistencias,
                                 int partidos, int victorias, int empates,
                                 int derrotas);
static sqlite3_int64 obtener_o_crear_cancha_id(const char *cancha_nombre);
static int partido_existe(sqlite3_int64 cancha_id, const char *fecha, int camiseta_id);
static int obtener_siguiente_partido_id(void);
static void insertar_partido(PartidoData data);
static int procesar_e_insertar_partido(const PartidoInput *input);

// trim_trailing_spaces() fue movido a utils.c como funci\u00f3n gen\u00e9rica
// Se puede usar directamente desde utils.h

/**
 * @brief Estructura para almacenar datos de una camiseta.
 */
typedef struct
{
    int id;
    char nombre[256];
} CamisetaData;

/**
 * @brief Estructura para almacenar datos de una lesión.
 */
typedef struct
{
    int id;
    char jugador[256];
    char tipo[256];
    char descripcion[512];
    char fecha[256];
} LesionData;

/**
 * @brief Estructura para almacenar datos de estadísticas.
 */
typedef struct
{
    char camiseta[256];
    int goles;
    int asistencias;
    int partidos;
    int victorias;
    int empates;
    int derrotas;
} EstadisticaData;

/**
 * @brief Construye el nombre del archivo completo.
 *
 * @param extension Extensión del archivo (e.g., "json").
 * @param filename Buffer para almacenar el nombre completo.
 * @param size Tamaño del buffer.
 */
static void build_filename(const char *extension, char *filename, size_t size)
{
    strcpy_s(filename, size, get_import_dir());
    size_t filename_len = safe_strnlen(filename, size);
    strncat_s(filename, size, "\\", size - filename_len - 1);
    strncat_s(filename, size, extension, size - filename_len - 1);
}

/**
 * @brief Lee el contenido completo de un archivo de texto.
 *
 * Para permitir el análisis eficiente del contenido sin múltiples lecturas de
 * disco.
 *
 * @param filename Ruta del archivo a leer.
 * @return Puntero al contenido del archivo o NULL si hay error.
 */
static char *read_file_content(const char *filename)
{
    FILE *file = NULL;
    errno_t err = fopen_s(&file, filename, "r");
    if (err != 0 || !file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length <= 0 ||
            length > 10485760) // Limit to 10MB to prevent excessive memory usage
    {
        fclose(file);
        return NULL;
    }

    char *content = (char *)malloc(length + 1);
    if (!content)
    {
        printf("Error: No se pudo asignar memoria\n");
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);
    return content;
}

/**
 * @brief Preparar statement y reportar errores.
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

/**
 * @brief Función genérica para cargar y validar un archivo JSON
 * @param json_filename Nombre del archivo JSON (ej: "partidos.json")
 * @param entity_name Nombre de la entidad para mensajes (ej: "partidos")
 * @return puntero a cJSON si es válido y es un array, NULL en caso contrario
 */
static cJSON *cargar_json_array(const char *json_filename, const char *entity_name, int *out_count)
{
    char filename[1024];
    build_filename(json_filename, filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return NULL;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de %s invalido\n", entity_name);
        return NULL;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON de %s debe ser un array\n", entity_name);
        cJSON_Delete(json);
        return NULL;
    }

    int count = cJSON_GetArraySize(json);
    printf("Importando %d %s...\n", count, entity_name);

    if (out_count)
        *out_count = count;

    return json;
}

/**
 * @brief Función genérica para abrir archivos de texto (CSV/TXT) para importación
 * @param txt_filename Nombre del archivo (ej: "lesiones.csv")
 * @param skip_header Si es true, salta la primera línea
 * @return FILE* abierto o NULL si hay error
 */
static FILE *abrir_archivo_texto_importacion(const char *txt_filename, bool skip_header)
{
    char filename[1024];
    build_filename(txt_filename, filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    FILE *file = NULL;
    errno_t err = fopen_s(&file, filename, "r");
    if (err != 0 || !file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return NULL;
    }

    if (skip_header)
    {
        char line[1024];
        if (fgets(line, sizeof(line), file) == NULL)
        {
            printf("Error: Archivo vacío o formato incorrecto\n");
            fclose(file);
            return NULL;
        }
    }

    return file;
}

/**
 * @brief Procesa e inserta un partido validando cancha, camiseta y duplicados
 *
 * Esta función centraliza la lógica común de validación e inserción de partidos
 * que se usa en todas las funciones de importación (TXT, CSV, HTML).
 *
 * @param input Puntero a estructura con los datos del partido a procesar
 * @return 1 si el partido fue insertado, 0 si hubo error o ya existe
 */
static int procesar_e_insertar_partido(const PartidoInput *input)
{
    // Obtener ID de cancha
    sqlite3_int64 cancha_id = obtener_o_crear_cancha_id(input->cancha);
    if (cancha_id == -1)
    {
        printf("Error al obtener ID de cancha para '%s', omitiendo partido...\n",
               input->cancha);
        return 0;
    }

    // Obtener ID de camiseta
    int camiseta_id = obtener_camiseta_id(input->camiseta);

    if (camiseta_id == -1)
    {
        printf("Camiseta '%s' no encontrada, omitiendo partido...\n", input->camiseta);
        return 0;
    }

    // Verificar si ya existe un partido con los mismos datos
    if (partido_existe(cancha_id, input->fecha, camiseta_id))
    {
        printf("Partido ya existe, omitiendo...\n");
        return 0;
    }

    // Obtener siguiente ID para partido
    int partido_id = obtener_siguiente_partido_id();

    // Insertar partido
    PartidoData partido_data =
    {
        partido_id, cancha_id, input->fecha, input->goles, input->asistencias,
        camiseta_id, input->resultado, input->clima, input->dia, input->rendimiento_general,
        input->cansancio, input->estado_animo, input->comentario
    };
    insertar_partido(partido_data);

    printf("Partido en '%s' importado correctamente\n", input->cancha);
    return 1;
}

/**
 * @brief Verifica si existe un registro por ID en la tabla dada.
 */
static int id_existe_en_tabla(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s WHERE id = ?", tabla);
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, id);
    int exists = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        exists = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return exists;
}

/**
 * @brief Inserta una camiseta en la base de datos.
 */
static void insertar_camiseta(int id, const char *nombre)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO camiseta(id, nombre, sorteada) VALUES(?, ?, 0)",
                &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Inserta una lesión en la base de datos.
 */
static void insertar_lesion(int id, const char *jugador, const char *tipo,
                            const char *descripcion, const char *fecha)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO lesion(id, jugador, tipo, descripcion, fecha) "
                "VALUES(?, ?, ?, ?, ?)",
                &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, jugador, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Ejecuta importación con mensajes y pausa.
 */
static void importar_con_pausa(const char *inicio, const char *fin,
                               void (*func)())
{
    printf("%s\n", inicio);
    func();
    printf("%s\n", fin);
    pause_console();
}

/**
 * @brief Parsea e inserta una camiseta desde JSON.
 *
 * @param item Objeto JSON de la camiseta.
 * @return 1 si se insertó correctamente, 0 si no.
 */
static int parse_camiseta_json(cJSON const *item)
{
    if (!cJSON_IsObject(item))
        return 0;

    cJSON const *id_json = cJSON_GetObjectItem(item, "id");
    cJSON const *nombre_json = cJSON_GetObjectItem(item, "nombre");

    if (!cJSON_IsNumber(id_json) || !cJSON_IsString(nombre_json))
        return 0;

    int id = id_json->valueint;
    const char *nombre = nombre_json->valuestring;

    // Verificar si ya existe
    if (id_existe_en_tabla("camiseta", id))
    {
        printf("Camiseta ID %d ya existe, omitiendo...\n", id);
        return 0;
    }

    // Insertar
    insertar_camiseta(id, nombre);

    printf("Camiseta '%s' importada correctamente\n", nombre);
    return 1;
}

/**
 * @brief Función genérica para importar desde JSON.
 *
 * @param extension Extensión del archivo.
 * @param parser Función para parsear un item JSON.
 */
static void import_json_generic(const char *extension,
                                int (*parser)(cJSON const *))
{
    char filename[1024];
    build_filename(extension, filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON invalido\n");
        return;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON debe ser un array\n");
        cJSON_Delete(json);
        return;
    }

    int total = cJSON_GetArraySize(json);
    printf("Importando %d items...\n", total);

    int imported = 0;
    for (int i = 0; i < total; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (parser(item))
            imported++;
    }

    cJSON_Delete(json);
    printf("Importacion completada. %d items importados\n", imported);
}
/**
 * @brief Importa camisetas desde archivo JSON.
 *
 * Lee el archivo JSON de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_json()
{
    import_json_generic("camisetas.json", parse_camiseta_json);
}

/**
 * @brief Obtiene el ID de una cancha por nombre, creando una nueva si no
 * existe.
 *
 * @param cancha_nombre Nombre de la cancha.
 * @return ID de la cancha o -1 si hay error.
 */
static sqlite3_int64 obtener_o_crear_cancha_id(const char *cancha_nombre)
{
    // Buscar cancha existente
    sqlite3_stmt *cancha_stmt;
    if (!preparar_stmt("SELECT id FROM cancha WHERE nombre = ?", &cancha_stmt))
    {
        return -1;
    }
    sqlite3_bind_text(cancha_stmt, 1, cancha_nombre, -1, SQLITE_TRANSIENT);
    sqlite3_int64 cancha_id = -1;
    if (sqlite3_step(cancha_stmt) == SQLITE_ROW)
    {
        cancha_id = sqlite3_column_int64(cancha_stmt, 0);
    }
    sqlite3_finalize(cancha_stmt);

    if (cancha_id == -1)
    {
        printf("Cancha '%s' no encontrada, creando...\n", cancha_nombre);
        // Crear cancha si no existe
        sqlite3_stmt *insert_cancha;
        if (!preparar_stmt("INSERT INTO cancha(nombre) VALUES(?)", &insert_cancha))
        {
            return -1;
        }
        sqlite3_bind_text(insert_cancha, 1, cancha_nombre, -1, SQLITE_TRANSIENT);
        sqlite3_step(insert_cancha);
        cancha_id = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(insert_cancha);
    }

    return cancha_id;
}

/**
 * @brief Verifica si ya existe un partido con los mismos datos.
 *
 * @param cancha_id ID de la cancha.
 * @param fecha Fecha y hora del partido.
 * @param camiseta_id ID de la camiseta.
 * @return 1 si existe, 0 si no existe.
 */
static int partido_existe(sqlite3_int64 cancha_id, const char *fecha,
                          int camiseta_id)
{
    sqlite3_stmt *dup_stmt;
    if (!preparar_stmt("SELECT COUNT(*) FROM partido WHERE cancha_id = ? AND "
                       "fecha_hora = ? AND camiseta_id = ?",
                       &dup_stmt))
    {
        return 0;
    }
    sqlite3_bind_int64(dup_stmt, 1, cancha_id);
    sqlite3_bind_text(dup_stmt, 2, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(dup_stmt, 3, camiseta_id);
    sqlite3_step(dup_stmt);
    int exists = sqlite3_column_int(dup_stmt, 0);
    sqlite3_finalize(dup_stmt);

    return exists;
}

/**
 * @brief Obtiene el siguiente ID disponible para un partido.
 *
 * @return ID del partido.
 */
static int obtener_siguiente_partido_id()
{
    int partido_id = 1;
    sqlite3_stmt *max_stmt;
    if (!preparar_stmt("SELECT COALESCE(MAX(id), 0) + 1 FROM partido", &max_stmt))
    {
        return partido_id;
    }
    if (sqlite3_step(max_stmt) == SQLITE_ROW)
    {
        partido_id = sqlite3_column_int(max_stmt, 0);
    }
    sqlite3_finalize(max_stmt);

    return partido_id;
}

// Estructura PartidoData ya definida en forward declarations

/**
 * @brief Inserta un partido en la base de datos.
 *
 * @param data Estructura con los datos del partido.
 */
static void insertar_partido(PartidoData data)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, "
                "camiseta_id, resultado, clima, dia, rendimiento_general, cansancio, "
                "estado_animo, comentario_personal) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                "?, ?, ?)",
                &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, data.partido_id);
    sqlite3_bind_int64(stmt, 2, data.cancha_id);
    sqlite3_bind_text(stmt, 3, data.fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, data.goles);
    sqlite3_bind_int(stmt, 5, data.asistencias);
    sqlite3_bind_int(stmt, 6, data.camiseta_id);
    sqlite3_bind_int(stmt, 7, data.resultado);
    sqlite3_bind_int(stmt, 8, data.clima);
    sqlite3_bind_int(stmt, 9, data.dia);
    sqlite3_bind_int(stmt, 10, data.rendimiento_general);
    sqlite3_bind_int(stmt, 11, data.cansancio);
    sqlite3_bind_int(stmt, 12, data.estado_animo);
    sqlite3_bind_text(stmt, 13, data.comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Procesa un item de partido desde JSON y lo inserta en la base de
 * datos.
 *
 * @param item El objeto JSON del partido.
 * @return 1 si se importó correctamente, 0 si no.
 */
static int procesar_partido_json_item(cJSON const *item)
{
    if (!cJSON_IsObject(item))
        return 0;

    cJSON const *cancha_json = cJSON_GetObjectItem(item, "cancha");
    cJSON const *fecha_json = cJSON_GetObjectItem(item, "fecha");
    cJSON const *goles_json = cJSON_GetObjectItem(item, "goles");
    cJSON const *asistencias_json = cJSON_GetObjectItem(item, "asistencias");
    cJSON const *camiseta_json = cJSON_GetObjectItem(item, "camiseta");
    cJSON const *resultado_json = cJSON_GetObjectItem(item, "resultado");
    cJSON const *clima_json = cJSON_GetObjectItem(item, "clima");
    cJSON const *dia_json = cJSON_GetObjectItem(item, "dia");
    cJSON const *rendimiento_general_json =
        cJSON_GetObjectItem(item, "rendimiento_general");
    cJSON const *cansancio_json = cJSON_GetObjectItem(item, "cansancio");
    cJSON const *estado_animo_json = cJSON_GetObjectItem(item, "estado_animo");
    cJSON const *comentario_personal_json =
        cJSON_GetObjectItem(item, "comentario_personal");

    if (!cJSON_IsString(cancha_json) || !cJSON_IsString(fecha_json) ||
            !cJSON_IsNumber(goles_json) || !cJSON_IsNumber(asistencias_json) ||
            !cJSON_IsString(camiseta_json))
        return 0;

    const char *cancha_nombre = cancha_json->valuestring;
    const char *fecha = fecha_json->valuestring;
    int goles = goles_json->valueint;
    int asistencias = asistencias_json->valueint;
    const char *camiseta_nombre = camiseta_json->valuestring;
    int resultado = resultado_json ? resultado_json->valueint : 0;
    int clima = clima_json ? clima_json->valueint : 0;
    int dia = dia_json ? dia_json->valueint : 0;
    int rendimiento_general =
        rendimiento_general_json ? rendimiento_general_json->valueint : 0;
    int cansancio = cansancio_json ? cansancio_json->valueint : 0;
    int estado_animo = estado_animo_json ? estado_animo_json->valueint : 0;
    const char *comentario_personal =
        comentario_personal_json ? comentario_personal_json->valuestring : "";

    // Obtener IDs necesarios
    sqlite3_int64 cancha_id = obtener_o_crear_cancha_id(cancha_nombre);
    if (cancha_id == -1)
    {
        printf("Error al obtener ID de cancha para '%s', omitiendo partido...\n",
               cancha_nombre);
        return 0;
    }

    int camiseta_id = obtener_camiseta_id(camiseta_nombre);
    if (camiseta_id == -1)
    {
        printf("Camiseta '%s' no encontrada, omitiendo partido...\n",
               camiseta_nombre);
        return 0;
    }

    // Verificar duplicados
    if (partido_existe(cancha_id, fecha, camiseta_id))
    {
        printf("Partido ya existe, omitiendo...\n");
        return 0;
    }

    // Insertar partido
    int partido_id = obtener_siguiente_partido_id();
    PartidoData partido_data = {partido_id,
                                cancha_id,
                                fecha,
                                goles,
                                asistencias,
                                camiseta_id,
                                resultado,
                                clima,
                                dia,
                                rendimiento_general,
                                cansancio,
                                estado_animo,
                                comentario_personal
                               };
    insertar_partido(partido_data);

    printf("Partido en '%s' importado correctamente\n", cancha_nombre);
    return 1;
}

/**
 * @brief Importa partidos desde archivo JSON.
 *
 * Lee el archivo JSON de partidos y los inserta en la base de datos.
 */
void importar_partidos_json()
{
    int count;
    cJSON *json = cargar_json_array("partidos.json", "partidos", &count);
    if (!json)
        return;

    int imported = 0;
    for (int i = 0; i < count; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (procesar_partido_json_item(item))
            imported++;
    }

    cJSON_Delete(json);
    printf("Importacion de partidos completada. %d partidos importados\n",
           imported);
}

/**
 * @brief Importa lesiones desde archivo JSON.
 *
 * Lee el archivo JSON de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_json()
{
    int count;
    cJSON *json = cargar_json_array("lesiones.json", "lesiones", &count);
    if (!json)
        return;

    for (int i = 0; i < count; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON const *id_json = cJSON_GetObjectItem(item, "id");
        cJSON const *jugador_json = cJSON_GetObjectItem(item, "jugador");
        cJSON const *tipo_json = cJSON_GetObjectItem(item, "tipo");
        cJSON const *descripcion_json = cJSON_GetObjectItem(item, "descripcion");
        cJSON const *fecha_json = cJSON_GetObjectItem(item, "fecha");
        if (!cJSON_IsNumber(id_json) || !cJSON_IsString(jugador_json) ||
                !cJSON_IsString(tipo_json) || !cJSON_IsString(descripcion_json) ||
                !cJSON_IsString(fecha_json))
            continue;

        int id = id_json->valueint;
        const char *jugador = jugador_json->valuestring;
        const char *tipo = tipo_json->valuestring;
        const char *descripcion = descripcion_json->valuestring;
        const char *fecha = fecha_json->valuestring;

        // Verificar si ya existe
        if (id_existe_en_tabla("lesion", id))
        {
            printf("Lesion ID %d ya existe, omitiendo...\n", id);
            continue;
        }

        // Insertar lesión
        insertar_lesion(id, jugador, tipo, descripcion, fecha);

        printf("Lesion de '%s' importada correctamente\n", jugador);
    }

    cJSON_Delete(json);
    printf("Importacion de lesiones completada\n");
}

/**
 * @brief Importa estadisticas desde archivo JSON.
 *
 * Lee el archivo JSON de estadisticas y las inserta en la base de datos.
 */
void importar_estadisticas_json()
{
    if (!crear_tabla_estadisticas())
        return;

    int count;
    cJSON *json = cargar_json_array("estadisticas.json", "estadisticas", &count);
    if (!json)
        return;

    for (int i = 0; i < count; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON const *camiseta_json = cJSON_GetObjectItem(item, "camiseta");
        cJSON const *goles_json = cJSON_GetObjectItem(item, "goles");
        cJSON const *asistencias_json = cJSON_GetObjectItem(item, "asistencias");
        cJSON const *partidos_json = cJSON_GetObjectItem(item, "partidos");
        cJSON const *victorias_json = cJSON_GetObjectItem(item, "victorias");
        cJSON const *empates_json = cJSON_GetObjectItem(item, "empates");
        cJSON const *derrotas_json = cJSON_GetObjectItem(item, "derrotas");

        if (!cJSON_IsString(camiseta_json) || !cJSON_IsNumber(goles_json) ||
                !cJSON_IsNumber(asistencias_json) || !cJSON_IsNumber(partidos_json))
            continue;

        const char *camiseta = camiseta_json->valuestring;
        int goles = goles_json->valueint;
        int asistencias = asistencias_json->valueint;
        int partidos = partidos_json->valueint;
        int victorias = victorias_json ? victorias_json->valueint : 0;
        int empates = empates_json ? empates_json->valueint : 0;
        int derrotas = derrotas_json ? derrotas_json->valueint : 0;

        // Obtener ID de camiseta
        int camiseta_id = obtener_camiseta_id_estadistica(camiseta);

        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo estadística...\n",
                   camiseta);
            continue;
        }

        // Verificar si ya existe estadística para esta camiseta
        if (estadistica_existe(camiseta_id))
        {
            printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n",
                   camiseta);
            continue;
        }

        // Insertar estadística
        insertar_estadistica(camiseta_id, goles, asistencias, partidos, victorias,
                             empates, derrotas);

        printf("Estadistica de '%s' importada correctamente\n", camiseta);
    }

    cJSON_Delete(json);
    printf("Importacion de estadisticas completada\n");
}

/**
 * @brief Importa camisetas desde archivo JSON con pausa.
 */
static void importar_camisetas_json_con_pausa()
{
    importar_con_pausa("Importando camisetas desde JSON...",
                       "Importacion de camisetas completada.",
                       importar_camisetas_json);
}

/**
 * @brief Importa partidos desde archivo JSON con pausa.
 */
static void importar_partidos_json_con_pausa()
{
    importar_con_pausa("Importando partidos desde JSON...",
                       "Importacion de partidos completada.",
                       importar_partidos_json);
}

/**
 * @brief Importa lesiones desde archivo JSON con pausa.
 */
static void importar_lesiones_json_con_pausa()
{
    importar_con_pausa("Importando lesiones desde JSON...",
                       "Importacion de lesiones completada.",
                       importar_lesiones_json);
}

/**
 * @brief Importa estadisticas desde archivo JSON con pausa.
 */
static void importar_estadisticas_json_con_pausa()
{
    importar_con_pausa("Importando estadisticas desde JSON...",
                       "Importacion de estadisticas completada.",
                       importar_estadisticas_json);
}

/**
 * @brief Importa camisetas desde archivo TXT con pausa.
 */
static void importar_camisetas_txt_con_pausa()
{
    importar_con_pausa("Importando camisetas desde TXT...",
                       "Importacion de camisetas completada.",
                       importar_camisetas_txt);
}

/**
 * @brief Importa partidos desde archivo TXT con pausa.
 */
static void importar_partidos_txt_con_pausa()
{
    importar_con_pausa("Importando partidos desde TXT...",
                       "Importacion de partidos completada.",
                       importar_partidos_txt);
}

/**
 * @brief Importa lesiones desde archivo TXT con pausa.
 */
static void importar_lesiones_txt_con_pausa()
{
    importar_con_pausa("Importando lesiones desde TXT...",
                       "Importacion de lesiones completada.",
                       importar_lesiones_txt);
}

/**
 * @brief Importa estadisticas desde archivo TXT con pausa.
 */
static void importar_estadisticas_txt_con_pausa()
{
    importar_con_pausa("Importando estadisticas desde TXT...",
                       "Importacion de estadisticas completada.",
                       importar_estadisticas_txt);
}

/**
 * @brief Importa camisetas desde archivo CSV con pausa.
 */
static void importar_camisetas_csv_con_pausa()
{
    importar_con_pausa("Importando camisetas desde CSV...",
                       "Importacion de camisetas completada.",
                       importar_camisetas_csv);
}

/**
 * @brief Importa partidos desde archivo CSV con pausa.
 */
static void importar_partidos_csv_con_pausa()
{
    importar_con_pausa("Importando partidos desde CSV...",
                       "Importacion de partidos completada.",
                       importar_partidos_csv);
}

/**
 * @brief Importa lesiones desde archivo CSV con pausa.
 */
static void importar_lesiones_csv_con_pausa()
{
    importar_con_pausa("Importando lesiones desde CSV...",
                       "Importacion de lesiones completada.",
                       importar_lesiones_csv);
}

/**
 * @brief Importa estadisticas desde archivo CSV con pausa.
 */
static void importar_estadisticas_csv_con_pausa()
{
    importar_con_pausa("Importando estadisticas desde CSV...",
                       "Importacion de estadisticas completada.",
                       importar_estadisticas_csv);
}

/**
 * @brief Importa todos los datos desde archivos CSV con pausa.
 */
static void importar_todo_csv_con_pausa()
{
    printf("Importando todo desde CSV...\n");
    importar_camisetas_csv();
    importar_partidos_csv();
    importar_lesiones_csv();
    importar_estadisticas_csv();
    printf("Importacion de todo desde CSV completada.\n");
    pause_console();
}

/**
 * @brief Importa camisetas desde archivo HTML con pausa.
 */
static void importar_camisetas_html_con_pausa()
{
    importar_con_pausa("Importando camisetas desde HTML...",
                       "Importacion de camisetas completada.",
                       importar_camisetas_html);
}

/**
 * @brief Importa partidos desde archivo HTML con pausa.
 */
static void importar_partidos_html_con_pausa()
{
    importar_con_pausa("Importando partidos desde HTML...",
                       "Importacion de partidos completada.",
                       importar_partidos_html);
}

/**
 * @brief Importa lesiones desde archivo HTML con pausa.
 */
static void importar_lesiones_html_con_pausa()
{
    importar_con_pausa("Importando lesiones desde HTML...",
                       "Importacion de lesiones completada.",
                       importar_lesiones_html);
}

/**
 * @brief Importa estadisticas desde archivo HTML con pausa.
 */
static void importar_estadisticas_html_con_pausa()
{
    importar_con_pausa("Importando estadisticas desde HTML...",
                       "Importacion de estadisticas completada.",
                       importar_estadisticas_html);
}

/**
 * @brief Importa todos los datos desde archivos HTML con pausa.
 */
static void importar_todo_html_con_pausa()
{
    printf("Importando todo desde HTML...\n");
    importar_camisetas_html();
    importar_partidos_html();
    importar_lesiones_html();
    importar_estadisticas_html();
    printf("Importacion de todo desde HTML completada.\n");
    pause_console();
}

/**
 * @brief Importa todos los datos desde archivos TXT con pausa.
 */
static void importar_todo_txt_con_pausa()
{
    printf("Importando todo desde TXT...\n");
    importar_camisetas_txt();
    importar_partidos_txt();
    importar_lesiones_txt();
    importar_estadisticas_txt();
    printf("Importacion de todo desde TXT completada.\n");
    pause_console();
}

/* ===================== IMPORTACIÓN DESDE TXT ===================== */

/**
 * @brief Importa camisetas desde archivo TXT.
 *
 * Lee el archivo TXT de camisetas y las inserta en la base de datos.
 * El formato esperado es: ID - NOMBRE
 */
void importar_camisetas_txt()
{
    FILE *file = abrir_archivo_texto_importacion("camisetas.txt", true);
    if (!file)
        return;

    printf("Importando camisetas desde TXT...\n");
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "ID - NOMBRE"
        int id;
        char nombre[256];

        if (sscanf_s(line, "%d - %s", &id, nombre, (unsigned)_countof(nombre)) ==
                2)
        {
            trim_trailing_spaces(nombre);
            // Verificar si ya existe
            if (id_existe_en_tabla("camiseta", id))
            {
                printf("Camiseta ID %d ya existe, omitiendo...\n", id);
                continue;
            }

            // Insertar
            insertar_camiseta(id, nombre);

            printf("Camiseta '%s' importada correctamente\n", nombre);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de camisetas desde TXT completada. %d camisetas "
           "importadas\n",
           count);
}

/**
 * @brief Convierte una cadena de resultado a número.
 *
 * @param resultado_str Cadena del resultado.
 * @return Número correspondiente al resultado.
 */
static int convertir_resultado(const char *resultado_str)
{
    if (strcmp(resultado_str, "VICTORIA") == 0)
        return 1;
    else if (strcmp(resultado_str, "EMPATE") == 0)
        return 2;
    else if (strcmp(resultado_str, "DERROTA") == 0)
        return 3;
    return 0;
}

/**
 * @brief Convierte una cadena de clima a número.
 *
 * @param clima_str Cadena del clima.
 * @return Número correspondiente al clima.
 */
static int convertir_clima(const char *clima_str)
{
    if (strcmp(clima_str, "Despejado") == 0)
        return 1;
    else if (strcmp(clima_str, "Nublado") == 0)
        return 2;
    else if (strcmp(clima_str, "Lluvia") == 0)
        return 3;
    else if (strcmp(clima_str, "Ventoso") == 0)
        return 4;
    else if (strcmp(clima_str, "Mucho") == 0)
        return 5; // Mucho Calor o Mucho Frio
    else if (strcmp(clima_str, "Frio") == 0)
        return 6;
    return 0;
}

/**
 * @brief Convierte una cadena de día a número.
 *
 * @param dia_str Cadena del día.
 * @return Número correspondiente al día.
 */
static int convertir_dia(const char *dia_str)
{
    if (strcmp(dia_str, "Dia") == 0)
        return 1;
    else if (strcmp(dia_str, "Tarde") == 0)
        return 2;
    else if (strcmp(dia_str, "Noche") == 0)
        return 3;
    return 0;
}

/**
 * @brief Procesa una línea de partido desde TXT y la inserta en la base de
 * datos.
 *
 * @param line Línea a procesar.
 * @return 1 si se procesó correctamente, 0 si no.
 */
static int procesar_partido_txt_line(const char *line)
{
    char cancha[256];
    char fecha[256];
    char camiseta[256];
    char resultado_str[32];
    char clima_str[32];
    char dia_str[32];
    char comentario[512];
    int goles;
    int asistencias;
    int rendimiento_general;
    int cansancio;
    int estado_animo;

    // Formato: CANCHA | FECHA | G:Goles A:Asistencias | CAMISETA | Res:Resultado
    // Cli:Clima Dia:Dia RG:Rendimiento Can:Cansancio EA:EstadoAnimo | Comentario
    if (sscanf_s(line,
                 "%[^|] | %[^|] | G:%d A:%d | %[^|] | Res:%[^ ] Cli:%[^ ] "
                 "Dia:%[^ ] RG:%d Can:%d EA:%d | %[^\n]",
                 cancha, sizeof(cancha), fecha, sizeof(fecha), &goles,
                 &asistencias, camiseta, sizeof(camiseta), resultado_str,
                 sizeof(resultado_str), clima_str, sizeof(clima_str), dia_str,
                 sizeof(dia_str), &rendimiento_general, &cansancio, &estado_animo,
                 comentario, sizeof(comentario)) != 12)
        return 0;

    int resultado = convertir_resultado(resultado_str);
    int clima = convertir_clima(clima_str);
    int dia = convertir_dia(dia_str);

    // Crear estructura de entrada
    PartidoInput input =
    {
        cancha, fecha, goles, asistencias, camiseta,
        resultado, clima, dia, rendimiento_general,
        cansancio, estado_animo, comentario
    };

    return procesar_e_insertar_partido(&input);
}

/**
 * @brief Importa partidos desde archivo TXT.
 *
 * Lee el archivo TXT de partidos y los inserta en la base de datos.
 * El formato esperado es complejo con múltiples campos separados por |
 */
void importar_partidos_txt()
{
    FILE *file = abrir_archivo_texto_importacion("partidos.txt", true);
    if (!file)
        return;

    printf("Importando partidos desde TXT...\n");
    char line[2048];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        if (procesar_partido_txt_line(line))
            count++;
    }

    fclose(file);
    printf(
        "Importacion de partidos desde TXT completada. %d partidos importados\n",
        count);
}

/**
 * @brief Importa lesiones desde archivo TXT.
 *
 * Lee el archivo TXT de lesiones y las inserta en la base de datos.
 * El formato esperado es: ID - JUGADOR | TIPO | DESCRIPCION | FECHA
 */
void importar_lesiones_txt()
{
    FILE *file = abrir_archivo_texto_importacion("lesiones.txt", true);
    if (!file)
        return;

    printf("Importando lesiones desde TXT...\n");
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "ID - JUGADOR | TIPO | DESCRIPCION | FECHA"
        int id;
        char jugador[256];
        char tipo[256];
        char descripcion[512];
        char fecha[256];

        if (sscanf_s(line, "%d - %[^|] | %[^|] | %[^|] | %[^\n]", &id, jugador,
                     (unsigned)_countof(jugador), tipo, (unsigned)_countof(tipo),
                     descripcion, (unsigned)_countof(descripcion), fecha,
                     (unsigned)_countof(fecha)) == 5)
        {
            // Verificar si ya existe
            if (id_existe_en_tabla("lesion", id))
            {
                printf("Lesion ID %d ya existe, omitiendo...\n", id);
                continue;
            }

            // Insertar lesión
            insertar_lesion(id, jugador, tipo, descripcion, fecha);

            printf("Lesion de '%s' importada correctamente\n", jugador);
            count++;
        }
    }

    fclose(file);
    printf(
        "Importacion de lesiones desde TXT completada. %d lesiones importadas\n",
        count);
}

/**
 * @brief Importa estadisticas desde archivo TXT.
 *
 * Lee el archivo TXT de estadisticas y las inserta en la base de datos.
 * El formato esperado es: CAMISETA | G:Goles A:Asistencias P:Partidos
 * V:Victorias E:Empates D:Derrotas
 */
void importar_estadisticas_txt()
{
    if (!crear_tabla_estadisticas())
        return;

    FILE *file = abrir_archivo_texto_importacion("estadisticas.txt", true);
    if (!file)
        return;

    printf("Importando estadisticas desde TXT...\n");
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "CAMISETA | G:Goles A:Asistencias P:Partidos V:Victorias
        // E:Empates D:Derrotas"
        char camiseta[256];
        int goles;
        int asistencias;
        int partidos;
        int victorias;
        int empates;
        int derrotas;

        if (sscanf_s(line, "%[^|] | G:%d A:%d P:%d V:%d E:%d D:%d", camiseta,
                     (unsigned)_countof(camiseta), &goles, &asistencias, &partidos,
                     &victorias, &empates, &derrotas) == 7)
        {
            // Obtener ID de camiseta
            int camiseta_id = obtener_camiseta_id_estadistica(camiseta);

            if (camiseta_id == -1)
            {
                printf("Camiseta '%s' no encontrada, omitiendo estadística...\n",
                       camiseta);
                continue;
            }

            // Verificar si ya existe estadística para esta camiseta
            if (estadistica_existe(camiseta_id))
            {
                printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n",
                       camiseta);
                continue;
            }

            // Insertar estadística
            insertar_estadistica(camiseta_id, goles, asistencias, partidos, victorias,
                                 empates, derrotas);

            printf("Estadistica de '%s' importada correctamente\n", camiseta);
            count++;
        }
        else
        {
            printf("Error parsing line: %s", line);
        }
    }

    fclose(file);
    printf("Importacion de estadisticas desde TXT completada. %d estadisticas "
           "importadas\n",
           count);
}

/* ===================== IMPORTACIÓN DESDE CSV ===================== */

/**
 * @brief Importa camisetas desde archivo CSV.
 *
 * Lee el archivo CSV de camisetas y las inserta en la base de datos.
 * El formato esperado es: id,nombre
 */
void importar_camisetas_csv()
{
    FILE *file = abrir_archivo_texto_importacion("camisetas.csv", true);
    if (!file)
        return;

    printf("Importando camisetas desde CSV...\n");
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "id,nombre"
        int id;
        char nombre[256];

        if (sscanf_s(line, "%d,%s", &id, nombre, (unsigned)_countof(nombre)) == 2)
        {
            // Verificar si ya existe
            if (id_existe_en_tabla("camiseta", id))
            {
                printf("Camiseta ID %d ya existe, omitiendo...\n", id);
                continue;
            }

            // Insertar
            insertar_camiseta(id, nombre);

            printf("Camiseta '%s' importada correctamente\n", nombre);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de camisetas desde CSV completada. %d camisetas "
           "importadas\n",
           count);
}

/**
 * @brief Procesa una línea de partido desde CSV y la inserta en la base de
 * datos.
 *
 * @param line Línea a procesar.
 * @return 1 si se procesó correctamente, 0 si no.
 */
static int procesar_partido_csv_line(const char *line)
{
    char cancha[256];
    char fecha[256];
    char camiseta[256];
    char resultado_str[32];
    char clima_str[32];
    char dia_str[32];
    char comentario[512];
    int goles;
    int asistencias;
    int rendimiento_general;
    int cansancio;
    int estado_animo;

    // Formato:
    // cancha,fecha,goles,asistencias,camiseta,resultado,clima,dia,rendimiento_general,cansancio,estado_animo,comentario
    if (sscanf_s(line,
                 "%[^,],%[^,],%d,%d,%[^,],%[^,],%[^,],%[^,],%d,%d,%d,%[^\n]",
                 cancha, sizeof(cancha), fecha, sizeof(fecha), &goles,
                 &asistencias, camiseta, sizeof(camiseta), resultado_str,
                 sizeof(resultado_str), clima_str, sizeof(clima_str), dia_str,
                 sizeof(dia_str), &rendimiento_general, &cansancio, &estado_animo,
                 comentario, sizeof(comentario)) != 12)
        return 0;

    int resultado = convertir_resultado(resultado_str);
    int clima = convertir_clima(clima_str);
    int dia = convertir_dia(dia_str);

    // Crear estructura de entrada
    PartidoInput input =
    {
        cancha, fecha, goles, asistencias, camiseta,
        resultado, clima, dia, rendimiento_general,
        cansancio, estado_animo, comentario
    };

    return procesar_e_insertar_partido(&input);
}

/**
 * @brief Importa partidos desde archivo CSV.
 *
 * Lee el archivo CSV de partidos y los inserta en la base de datos.
 * El formato esperado es complejo con múltiples campos separados por coma.
 */
void importar_partidos_csv()
{
    FILE *file = abrir_archivo_texto_importacion("partidos.csv", true);
    if (!file)
        return;

    printf("Importando partidos desde CSV...\n");
    char line[2048];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        if (procesar_partido_csv_line(line))
            count++;
    }

    fclose(file);
    printf(
        "Importacion de partidos desde CSV completada. %d partidos importados\n",
        count);
}

/**
 * @brief Importa lesiones desde archivo CSV.
 *
 * Lee el archivo CSV de lesiones y las inserta en la base de datos.
 * El formato esperado es: id,jugador,tipo,descripcion,fecha
 */
void importar_lesiones_csv()
{
    FILE *file = abrir_archivo_texto_importacion("lesiones.csv", true);
    if (!file)
        return;

    printf("Importando lesiones desde CSV...\n");
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea: "id,jugador,tipo,descripcion,fecha"
        int id;
        char jugador[256];
        char tipo[256];
        char descripcion[512];
        char fecha[256];

        if (sscanf_s(line, "%d,%[^,],%[^,],%[^,],%s", &id, jugador,
                     (unsigned)_countof(jugador), tipo, (unsigned)_countof(tipo),
                     descripcion, (unsigned)_countof(descripcion), fecha,
                     (unsigned)_countof(fecha)) == 5)
        {
            // Verificar si ya existe
            if (id_existe_en_tabla("lesion", id))
            {
                printf("Lesion ID %d ya existe, omitiendo...\n", id);
                continue;
            }

            // Insertar lesión
            insertar_lesion(id, jugador, tipo, descripcion, fecha);

            printf("Lesion de '%s' importada correctamente\n", jugador);
            count++;
        }
    }

    fclose(file);
    printf(
        "Importacion de lesiones desde CSV completada. %d lesiones importadas\n",
        count);
}

/**
 * @brief Importa estadisticas desde archivo CSV.
 *
 * Lee el archivo CSV de estadisticas y las inserta en la base de datos.
 * El formato esperado es:
 * camiseta,goles,asistencias,partidos,victorias,empates,derrotas
 */
void importar_estadisticas_csv()
{
    if (!crear_tabla_estadisticas())
        return;

    FILE *file = abrir_archivo_texto_importacion("estadisticas.csv", true);
    if (!file)
        return;

    printf("Importando estadisticas desde CSV...\n");
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Parsear línea:
        // "camiseta,goles,asistencias,partidos,victorias,empates,derrotas"
        char camiseta[256];
        int goles;
        int asistencias;
        int partidos;
        int victorias;
        int empates;
        int derrotas;

        if (sscanf_s(line, "%[^,],%d,%d,%d,%d,%d,%d", camiseta,
                     (unsigned)_countof(camiseta), &goles, &asistencias, &partidos,
                     &victorias, &empates, &derrotas) == 7)
        {
            // Obtener ID de camiseta
            int camiseta_id = obtener_camiseta_id_estadistica(camiseta);

            if (camiseta_id == -1)
            {
                printf("Camiseta '%s' no encontrada, omitiendo estadística...\n",
                       camiseta);
                continue;
            }

            // Verificar si ya existe estadística para esta camiseta
            if (estadistica_existe(camiseta_id))
            {
                printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n",
                       camiseta);
                continue;
            }

            // Insertar estadística
            insertar_estadistica(camiseta_id, goles, asistencias, partidos, victorias,
                                 empates, derrotas);

            printf("Estadistica de '%s' importada correctamente\n", camiseta);
            count++;
        }
    }

    fclose(file);
    printf("Importacion de estadisticas desde CSV completada. %d estadisticas "
           "importadas\n",
           count);
}

/* ===================== IMPORTACIÓN DESDE HTML ===================== */

/**
 * @brief Importa camisetas desde archivo HTML.
 *
 * Lee el archivo HTML de camisetas y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML con <td> para id y nombre.
 */
void importar_camisetas_html()
{
    char filename[1024];
    build_filename("camisetas.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando camisetas desde HTML...\n");
    int count = 0;
    char const *ptr = content;

    // Buscar <td> tags
    int continue_parsing = 1;
    while (continue_parsing)
    {
        ptr = strstr(ptr, "<td>");
        if (ptr == NULL)
        {
            continue_parsing = 0;
            continue;
        }
        ptr += 4; // Saltar <td>
        char *end = strstr(ptr, "</td>");
        if (!end)
        {
            continue_parsing = 0;
            continue;
        }

        *end = '\0';
        int id = atoi(ptr);

        // Siguiente <td> para nombre
        ptr = strstr(end + 5, "<td>");
        if (!ptr)
        {
            continue_parsing = 0;
            continue;
        }
        ptr += 4;
        end = strstr(ptr, "</td>");
        if (!end)
        {
            continue_parsing = 0;
            continue;
        }
        *end = '\0';
        char nombre[256];
        strcpy_s(nombre, sizeof(nombre), ptr);

        // Verificar si ya existe
        if (id_existe_en_tabla("camiseta", id))
        {
            printf("Camiseta ID %d ya existe, omitiendo...\n", id);
            ptr = end + 5;
            continue;
        }

        // Insertar
        insertar_camiseta(id, nombre);

        printf("Camiseta '%s' importada correctamente\n", nombre);
        count++;
        ptr = end + 5;
    }

    free(content);
    printf("Importacion de camisetas desde HTML completada. %d camisetas "
           "importadas\n",
           count);
}

/**
 * @brief Procesa una fila de partido desde HTML y la inserta en la base de
 * datos.
 *
 * @param ptr Puntero a la fila <tr> en el contenido HTML.
 * @return 1 si se procesó correctamente, 0 si no.
 */
static int procesar_partido_html_row(char **ptr)
{
    char cancha[256];
    char fecha[256];
    char camiseta[256];
    char resultado_str[32];
    char clima_str[32];
    char dia_str[32];
    char comentario[512];
    int goles;
    int asistencias;
    int rendimiento_general;
    int cansancio;
    int estado_animo;

    // Extraer celdas
    for (int i = 0; i < 12; i++)
    {
        char const *td = strstr(*ptr, "<td>");
        if (!td)
            return 0;
        td += 4;
        char *end = strstr(td, "</td>");
        if (!end)
            return 0;
        *end = '\0';

        if (i == 0)
            strcpy_s(cancha, sizeof(cancha), td);
        else if (i == 1)
            strcpy_s(fecha, sizeof(fecha), td);
        else if (i == 2)
            goles = atoi(td);
        else if (i == 3)
            asistencias = atoi(td);
        else if (i == 4)
            strcpy_s(camiseta, sizeof(camiseta), td);
        else if (i == 5)
            strcpy_s(resultado_str, sizeof(resultado_str), td);
        else if (i == 6)
            strcpy_s(clima_str, sizeof(clima_str), td);
        else if (i == 7)
            strcpy_s(dia_str, sizeof(dia_str), td);
        else if (i == 8)
            rendimiento_general = atoi(td);
        else if (i == 9)
            cansancio = atoi(td);
        else if (i == 10)
            estado_animo = atoi(td);
        else if (i == 11)
            strcpy_s(comentario, sizeof(comentario), td);
        *ptr = end + 5;
    }

    int resultado = convertir_resultado(resultado_str);
    int clima = convertir_clima(clima_str);
    int dia = convertir_dia(dia_str);

    // Crear estructura de entrada
    PartidoInput input =
    {
        cancha, fecha, goles, asistencias, camiseta,
        resultado, clima, dia, rendimiento_general,
        cansancio, estado_animo, comentario
    };

    return procesar_e_insertar_partido(&input);
}

/**
 * @brief Importa partidos desde archivo HTML.
 *
 * Lee el archivo HTML de partidos y los inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_partidos_html()
{
    char filename[1024];
    build_filename("partidos.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando partidos desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>
        if (procesar_partido_html_row(&ptr))
            count++;
    }

    free(content);
    printf(
        "Importacion de partidos desde HTML completada. %d partidos importados\n",
        count);
}

/**
 * @brief Importa lesiones desde archivo HTML.
 *
 * Lee el archivo HTML de lesiones y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_lesiones_html()
{
    char filename[1024];
    build_filename("lesiones.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando lesiones desde HTML...\n");
    int count = 0;
    char const *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>
        int id;
        char jugador[256];
        char tipo[256];
        char descripcion[512];
        char fecha[256];

        // Extraer celdas
        char const *td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        char *end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        id = atoi(td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(jugador, sizeof(jugador), td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(tipo, sizeof(tipo), td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(descripcion, sizeof(descripcion), td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(fecha, sizeof(fecha), td);
        ptr = end + 5;

        // Verificar si ya existe
        if (id_existe_en_tabla("lesion", id))
        {
            printf("Lesion ID %d ya existe, omitiendo...\n", id);
            continue;
        }

        // Insertar lesión
        insertar_lesion(id, jugador, tipo, descripcion, fecha);

        printf("Lesion de '%s' importada correctamente\n", jugador);
        count++;
    }

    free(content);
    printf(
        "Importacion de lesiones desde HTML completada. %d lesiones importadas\n",
        count);
}

/**
 * @brief Crea la tabla de estadisticas si no existe.
 *
 * @return true si la tabla se creó o ya existe, false si hubo error.
 */
static bool crear_tabla_estadisticas()
{
    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS estadistica ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "camiseta_id INTEGER,"
        "goles INTEGER,"
        "asistencias INTEGER,"
        "partidos INTEGER,"
        "victorias INTEGER,"
        "empates INTEGER,"
        "derrotas INTEGER,"
        "FOREIGN KEY (camiseta_id) REFERENCES camiseta(id));";
    char *err_msg = NULL;
    if (sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg) != SQLITE_OK)
    {
        printf("Error creando tabla estadistica: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

/**
 * @brief Estructura para almacenar datos de estadisticas.
 */
typedef struct
{
    char camiseta[256];
    int goles;
    int asistencias;
    int partidos;
    int victorias;
    int empates;
    int derrotas;
} EstadisticasData;

/**
 * @brief Extrae datos de estadisticas de una fila HTML.
 *
 * @param ptr Puntero al contenido HTML.
 * @param data Puntero a la estructura EstadisticasData para almacenar los
 * datos.
 * @return true si la extracción fue exitosa, false si no.
 */
static bool extraer_datos_estadisticas_html(char **ptr,
        EstadisticasData *data)
{
    // Extraer celdas
    for (int i = 0; i < 7; i++)
    {
        char const *td = strstr(*ptr, "<td>");
        if (!td)
            return false;
        td += 4;
        char *end = strstr(td, "</td>");
        if (!end)
            return false;
        *end = '\0';

        if (i == 0)
            strcpy_s(data->camiseta, 256, td);
        else if (i == 1)
            data->goles = atoi(td);
        else if (i == 2)
            data->asistencias = atoi(td);
        else if (i == 3)
            data->partidos = atoi(td);
        else if (i == 4)
            data->victorias = atoi(td);
        else if (i == 5)
            data->empates = atoi(td);
        else if (i == 6)
            data->derrotas = atoi(td);
        *ptr = end + 5;
    }
    return true;
}

/**
 * @brief Obtiene el ID de una camiseta por nombre.
 *
 * @param camiseta_nombre Nombre de la camiseta.
 * @return ID de la camiseta o -1 si no existe.
 */
static int obtener_camiseta_id_estadistica(const char *camiseta_nombre)
{
    sqlite3_stmt *camiseta_stmt;
    if (!preparar_stmt("SELECT id FROM camiseta WHERE nombre = ?", &camiseta_stmt))
    {
        return -1;
    }
    sqlite3_bind_text(camiseta_stmt, 1, camiseta_nombre, -1, SQLITE_TRANSIENT);
    int camiseta_id = -1;
    if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
    {
        camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
    }
    sqlite3_finalize(camiseta_stmt);
    return camiseta_id;
}

/**
 * @brief Verifica si ya existe una estadistica para una camiseta.
 *
 * @param camiseta_id ID de la camiseta.
 * @return true si existe, false si no existe.
 */
static bool estadistica_existe(int camiseta_id)
{
    sqlite3_stmt *check_stmt;
    if (!preparar_stmt("SELECT COUNT(*) FROM estadistica WHERE camiseta_id = ?",
                       &check_stmt))
    {
        return false;
    }
    sqlite3_bind_int(check_stmt, 1, camiseta_id);
    sqlite3_step(check_stmt);
    int exists = sqlite3_column_int(check_stmt, 0);
    sqlite3_finalize(check_stmt);
    return exists > 0;
}

/**
 * @brief Inserta una estadistica en la base de datos.
 *
 * @param camiseta_id ID de la camiseta.
 * @param goles Goles.
 * @param asistencias Asistencias.
 * @param partidos Partidos.
 * @param victorias Victorias.
 * @param empates Empates.
 * @param derrotas Derrotas.
 */
static void insertar_estadistica(int camiseta_id, int goles, int asistencias,
                                 int partidos, int victorias, int empates,
                                 int derrotas)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO estadistica(camiseta_id, goles, asistencias, partidos, "
                "victorias, empates, derrotas) VALUES(?, ?, ?, ?, ?, ?, ?)",
                &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, camiseta_id);
    sqlite3_bind_int(stmt, 2, goles);
    sqlite3_bind_int(stmt, 3, asistencias);
    sqlite3_bind_int(stmt, 4, partidos);
    sqlite3_bind_int(stmt, 5, victorias);
    sqlite3_bind_int(stmt, 6, empates);
    sqlite3_bind_int(stmt, 7, derrotas);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Importa estadisticas desde archivo HTML.
 *
 * Lee el archivo HTML de estadisticas y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_estadisticas_html()
{
    if (!crear_tabla_estadisticas())
        return;

    char filename[1024];
    build_filename("estadisticas.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando estadisticas desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>

        EstadisticasData data;

        if (!extraer_datos_estadisticas_html(&ptr, &data))
            continue;

        int camiseta_id = obtener_camiseta_id_estadistica(data.camiseta);
        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo estadística...\n",
                   data.camiseta);
            continue;
        }

        if (estadistica_existe(camiseta_id))
        {
            printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n",
                   data.camiseta);
            continue;
        }

        insertar_estadistica(camiseta_id, data.goles, data.asistencias,
                             data.partidos, data.victorias, data.empates,
                             data.derrotas);
        printf("Estadistica de '%s' importada correctamente\n", data.camiseta);
        count++;
    }

    free(content);
    printf("Importacion de estadisticas desde HTML completada. %d estadisticas "
           "importadas\n",
           count);
}

/**
 * @brief Importa todos los datos desde archivos JSON con pausa.
 */
static void importar_todo_con_pausa()
{
    printf("Importando todo...\n");
    importar_camisetas_json();
    importar_partidos_json();
    importar_lesiones_json();
    importar_estadisticas_json();
    printf("Importacion de todo completada.\n");
    pause_console();
}

/**
 * @brief Submenú para importar datos desde archivos JSON.
 */
static void submenu_importar_json()
{
    MenuItem items[] = {{1, "Camisetas", importar_camisetas_json_con_pausa},
        {2, "Partidos", importar_partidos_json_con_pausa},
        {3, "Lesiones", importar_lesiones_json_con_pausa},
        {4, "Estadisticas", importar_estadisticas_json_con_pausa},
        {5, "Todo", importar_todo_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE JSON", items, 6);
}

/**
 * @brief Submenú para importar datos desde archivos TXT.
 */
static void submenu_importar_txt()
{
    MenuItem items[] = {{1, "Camisetas", importar_camisetas_txt_con_pausa},
        {2, "Partidos", importar_partidos_txt_con_pausa},
        {3, "Lesiones", importar_lesiones_txt_con_pausa},
        {4, "Estadisticas", importar_estadisticas_txt_con_pausa},
        {5, "Todo", importar_todo_txt_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE TXT", items, 6);
}

/**
 * @brief Submenú para importar datos desde archivos CSV.
 */
static void submenu_importar_csv()
{
    MenuItem items[] = {{1, "Camisetas", importar_camisetas_csv_con_pausa},
        {2, "Partidos", importar_partidos_csv_con_pausa},
        {3, "Lesiones", importar_lesiones_csv_con_pausa},
        {4, "Estadisticas", importar_estadisticas_csv_con_pausa},
        {5, "Todo", importar_todo_csv_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE CSV", items, 6);
}

/**
 * @brief Submenú para importar datos desde archivos HTML.
 */
static void submenu_importar_html()
{
    MenuItem items[] = {{1, "Camisetas", importar_camisetas_html_con_pausa},
        {2, "Partidos", importar_partidos_html_con_pausa},
        {3, "Lesiones", importar_lesiones_html_con_pausa},
        {4, "Estadisticas", importar_estadisticas_html_con_pausa},
        {5, "Todo", importar_todo_html_con_pausa},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS DESDE HTML", items, 6);
}

/**
 * @brief Menu principal para importar datos desde archivos según selección del
 * usuario.
 *
 * Esta función muestra un menú principal para que el usuario seleccione el
 * formato de archivo desde el cual importar: JSON, TXT, CSV o HTML. Cada opción
 * lleva a un submenú específico para ese formato.
 */
void menu_importar()
{
    MenuItem items[] = {{1, "Importar desde JSON", submenu_importar_json},
        {2, "Importar desde TXT", submenu_importar_txt},
        {3, "Importar desde CSV", submenu_importar_csv},
        {4, "Importar desde HTML", submenu_importar_html},
        {5, "Importar desde Base de Datos", importar_base_datos},
        {0, "Volver", NULL}
    };
    ejecutar_menu("IMPORTAR DATOS", items, 6);
}
