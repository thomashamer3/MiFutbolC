/**
 * @file export.h
 * @brief Declaraciones de funciones para exportar datos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones de exportación
 * que permiten guardar los datos del sistema en diferentes formatos:
 * CSV, TXT, JSON y HTML para camisetas, partidos, estadísticas y lesiones.
 */

#ifndef EXPORT_H
#define EXPORT_H

#include <stdio.h>

struct cJSON;
typedef struct cJSON cJSON;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef void (*ExportWriterFn)(FILE *file);

/**
 * @brief Configuracion reutilizable para el flujo generico de exportacion.
 *
 * El llamador prepara los callbacks de cabecera/fila/pie, el nombre del archivo
 * de salida y un contexto opcional (por ejemplo, un cJSON root para JSON).
 */
typedef struct
{
    const char *filename;
    void *context;
    void (*write_header)(FILE *f, void *context);
    void (*write_row)(FILE *f, sqlite3_stmt *stmt, void *context);
    void (*write_footer)(FILE *f, void *context);
} ExportConfig;

/**
 * @name Macros para reducir el boilerplate de exportacion
 *
 * Cada modulo define sus callbacks write_*_header/row/footer y una funcion
 * obtener_datos_ENTIDAD(), y luego invoca estos macros para las 4 funciones
 * publicas exportar_ENTIDAD_{csv,txt,json,html}.
 */
/** @{ */

/**
 * @brief Macro para exportar en formato de varias filas.
 * @param name   Nombre completo de la funcion (ej. exportar_camisetas_csv)
 * @param data_fn Funcion que prepara el stmt (ej. obtener_datos_camisetas)
 * @param fn     Nombre del archivo de salida
 * @param ctx    Contexto (NULL para CSV/TXT/HTML, cJSON_CreateArray() para JSON)
 * @param hdr    Callback de cabecera
 * @param row    Callback de fila
 * @param ftr    Callback de pie (NULL para CSV/TXT/HTML)
 */
#define EXPORT_FORMAT_ROWS(name, data_fn, fn, ctx, hdr, row, ftr) \
    void name() \
    { \
        ExportConfig config = { .filename = fn, .context = ctx, \
            .write_header = hdr, .write_row = row, .write_footer = ftr }; \
        int count; \
        sqlite3_stmt *stmt = data_fn(&count); \
        if (!stmt) return; \
        export_generic_rows(&config, stmt); \
    }

/**
 * @brief Macro para exportar en formato de una sola fila (dashboard).
 */
#define EXPORT_FORMAT_SINGLE(name, data_fn, fn, ctx, hdr, row, ftr) \
    void name() \
    { \
        ExportConfig config = { .filename = fn, .context = ctx, \
            .write_header = hdr, .write_row = row, .write_footer = ftr }; \
        int count; \
        sqlite3_stmt *stmt = data_fn(&count); \
        if (!stmt) return; \
        export_generic_single(&config, stmt); \
    }

/** @} */

/**
 * @brief Ejecuta el flujo generico de exportacion iterando filas del @p stmt.
 *
 * Escribe la cabecera (si esta definida), recorre todas las filas invocando
 * el callback de fila, y finalmente escribe el pie (si esta definido).
 *
 * @param config Configuracion con callbacks y nombre de archivo.
 * @param stmt Sentencia preparada que se finaliza antes de retornar.
 */
void export_generic_rows(ExportConfig *config, sqlite3_stmt *stmt);

typedef sqlite3_stmt* (*ExportDataFn)(int *count);

/**
 * @brief Exporta la misma consulta a multiples formatos (CSV/TXT/JSON/HTML)
 *        ejecutando el SQL una sola vez y re-stepeando entre formatos.
 *
 * Elimina 3 de 4 ejecuciones de la consulta SQL cuando se exportan los 4
 * formatos, ahorrando agregaciones/joins repetidos.
 *
 * @param data_fn Funcion que prepara y retorna el stmt (ej. obtener_datos_camisetas)
 * @param configs Arreglo de ExportConfig, uno por formato
 * @param num_formats Cantidad de formatos en configs
 */
void export_all_formats(ExportDataFn data_fn, ExportConfig configs[], int num_formats);

/**
 * @brief Ejecuta el flujo generico de exportacion para una unica fila.
 *
 * Variante de export_generic_rows() para consultas que devuelven un solo
 * registro (por ejemplo, agregaciones de dashboard).
 *
 * @param config Configuracion con callbacks y nombre de archivo.
 * @param stmt Sentencia preparada que se finaliza antes de retornar.
 */
void export_generic_single(ExportConfig *config, sqlite3_stmt *stmt);

/** @name Funciones utilitarias */
/** @{ */

/**
 * @brief Elimina espacios en blanco al final de una cadena.
 *
 * @param str Cadena a recortar.
 * @return Puntero a la cadena recortada.
 */
char *trim_trailing_spaces(char *str);

/**
 * @brief Convierte el número de resultado a texto
 *
 * @param resultado Número del resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA)
 * @return Cadena de texto correspondiente al resultado
 */
const char *resultado_to_text(int resultado);

/**
 * @brief Convierte el número de clima a texto
 *
 * @param clima Número del clima
 * (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio,
 * 7=Frio, 8=Calor, 9=Llovizna leve, 10=Lluvia Moderada, 11=Lluvia fuerte, 12=Cancha inundada)
 * @return Cadena de texto correspondiente al clima
 */
const char *clima_to_text(int clima);

/**
 * @brief Convierte el número de dia a texto
 *
 * @param dia Número del dia (1=Dia, 2=Tarde, 3=Noche)
 * @return Cadena de texto correspondiente al dia
 */
const char *dia_to_text(int dia);

/** @} */

/** @name Funciones de exportación de análisis */
/** @{ */

/**
 * @brief Exporta el análisis de rendimiento a formato CSV
 *
 * Genera un archivo CSV con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional.
 */
void exportar_analisis_csv();

/**
 * @brief Exporta el análisis de rendimiento a formato TXT
 *
 * Genera un archivo de texto con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional.
 */
void exportar_analisis_txt();

/**
 * @brief Exporta el análisis de rendimiento a formato JSON
 *
 * Genera un archivo JSON con un objeto conteniendo todas las estadísticas
 * del análisis de rendimiento.
 */
void exportar_analisis_json();

/**
 * @brief Exporta el análisis de rendimiento a formato HTML
 *
 * Genera una página HTML con las estadísticas presentadas en formato web.
 */
void exportar_analisis_html();

/**
 * @brief Exporta el analisis a los 4 formatos con un solo calculo de estadisticas.
 */
void exportar_analisis_all();

/**
 * @brief Exporta un resumen financiero por mes y año a TXT
 */
void exportar_finanzas_resumen_txt();

/**
 * @brief Exporta ranking de canchas por rendimiento y lesiones a TXT
 */
void exportar_ranking_canchas_txt();

/**
 * @brief Exporta partidos agrupados por clima a TXT
 */
void exportar_partidos_por_clima_txt();

/**
 * @brief Exporta distribución de lesiones por tipo y estado a TXT
 */
void exportar_lesiones_por_tipo_estado_txt();

/**
 * @brief Exporta historial de rachas a TXT
 */
void exportar_rachas_historial_txt();

/**
 * @brief Exporta distribución de estado de ánimo y cansancio a TXT
 */
void exportar_estado_animo_cansancio_txt();

/** @} */

/**
 * @brief Construye la ruta completa para un archivo de exportación
 *
 * Combina el directorio de datos con el nombre del archivo proporcionado
 * para crear una ruta completa.
 *
 * @param filename Nombre del archivo a exportar
 * @return Cadena de caracteres con la ruta completa del archivo
 */
char *get_export_path(const char *filename);

/**
 * @brief Exporta a archivo si la tabla indicada tiene registros.
 *
 * Aplica un flujo comun: validar registros, abrir archivo, escribir cabecera opcional,
 * ejecutar writer y reportar ruta final.
 *
 * @return 1 si se exporto correctamente, 0 en caso contrario.
 */
int exportar_archivo_si_hay_registros(const char *tabla,
                                      const char *mensaje_sin_registros,
                                      const char *filename,
                                      const char *error_al_abrir,
                                      const char *cabecera_opcional,
                                      ExportWriterFn writer);

/**
 * @brief Agrega los campos base de una lesion a un objeto JSON.
 *
 * Campos agregados: id, jugador, tipo, descripcion, fecha.
 */
void export_json_add_lesion_base_fields(cJSON *item, sqlite3_stmt *stmt);

/** @name Funciones de escritura de secciones (multi-seccion) */
/** @{ */

/**
 * @brief Escribe una seccion CSV a partir de una consulta SQL.
 */
int escribir_seccion_csv(FILE *f, const char *sql, const char *cabecera,
                         void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt));

/**
 * @brief Escribe una seccion TXT a partir de una consulta SQL.
 */
void escribir_seccion_txt(FILE *f, const char *titulo, const char *sql,
                          void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt));

/**
 * @brief Escribe una seccion JSON a partir de una consulta SQL.
 */
void escribir_seccion_json(cJSON *arr, const char *sql,
                           void (*escribir_objeto)(cJSON *item, sqlite3_stmt *stmt));

/**
 * @brief Escribe una seccion HTML a partir de una consulta SQL.
 */
void escribir_seccion_html(FILE *f, const char *titulo, const char *sql,
                           const char *cabeceras[],
                           void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt));

/** @} */

/** @name Callbacks de pie de pagina compartidos (evitan duplicacion en modulos de exportacion) */
/** @{ */

/**
 * @brief Callback de pie para exportacion JSON: imprime y libera el cJSON root.
 *
 * @param f Archivo de salida.
 * @param context Puntero al objeto cJSON root (cJSON*).
 */
void export_write_json_footer(FILE *f, void *context);

/**
 * @brief Callback de pie para exportacion HTML: cierra la tabla y el documento.
 *
 * @param f Archivo de salida.
 * @param context No utilizado.
 */
void export_write_html_footer(FILE *f, void *context);

/** @} */

#endif
