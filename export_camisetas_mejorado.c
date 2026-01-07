/**
 * @file export_camisetas_mejorado.c
 * @brief Funciones mejoradas para exportar datos de camisetas con análisis avanzado
 *
 * Este archivo contiene funciones mejoradas para exportar datos de camisetas
 * con estadísticas avanzadas, eficiencia y análisis de rendimiento.
 */

#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>

/**
 * @brief Estructura para almacenar datos de camiseta con análisis avanzado
 *
 * Esta estructura centraliza los datos de camiseta con análisis avanzado que se utilizan
 * en todas las funciones de exportación, evitando la duplicación de código y facilitando
 * el mantenimiento.
 */
typedef struct
{
    int id;
    const char *nombre;
    int total_goles;
    int total_asistencias;
    int total_partidos;
    int victorias;
    int empates;
    int derrotas;
    int total_lesiones;
    double rendimiento_promedio;
    double cansancio_promedio;
    double estado_animo_promedio;
    double eficiencia_goles_por_partido;
    double eficiencia_asistencias_por_partido;
    double relacion_goles_asistencias;
    double porcentaje_victorias;
    double porcentaje_lesiones_por_partido;
} CamisetaDataMejorado;

/**
 * @brief Obtiene los datos de camisetas con análisis avanzado de la base de datos
 *
 * Función estática que encapsula la consulta SQL común utilizada por todas
 * las funciones de exportación mejorada. Esto evita la duplicación de código y centraliza
 * la lógica de acceso a datos con análisis avanzado.
 *
 * @param[out] camisetas Array de estructuras CamisetaDataMejorado para almacenar los resultados
 * @param[out] count Puntero a entero para almacenar el número de camisetas encontradas
 * @return sqlite3_stmt* Statement preparado para iterar sobre los resultados
 */
static sqlite3_stmt* obtener_datos_camisetas_mejorado(CamisetaDataMejorado *camisetas, int *count)
{
    sqlite3_stmt *check_stmt;
    *count = 0;

    // Verificar si hay camisetas registradas
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        *count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);

    if (*count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return NULL;
    }

    // Preparar la consulta principal con análisis avanzado
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.id, c.nombre, "
                       "COALESCE(SUM(p.goles), 0) as total_goles, "
                       "COALESCE(SUM(p.asistencias), 0) as total_asistencias, "
                       "COUNT(p.id) as total_partidos, "
                       "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                       "COUNT(CASE WHEN p.resultado = 0 THEN 1 END) as empates, "
                       "COUNT(CASE WHEN p.resultado = -1 THEN 1 END) as derrotas, "
                       "COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) as total_lesiones, "
                       "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                       "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                       "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio, "
                       "CASE WHEN COUNT(p.id) > 0 THEN COALESCE(SUM(p.goles), 0) * 1.0 / COUNT(p.id) ELSE 0 END as eficiencia_goles_por_partido, "
                       "CASE WHEN COUNT(p.id) > 0 THEN COALESCE(SUM(p.asistencias), 0) * 1.0 / COUNT(p.id) ELSE 0 END as eficiencia_asistencias_por_partido, "
                       "CASE WHEN COALESCE(SUM(p.asistencias), 0) > 0 THEN COALESCE(SUM(p.goles), 0) * 1.0 / COALESCE(SUM(p.asistencias), 0) ELSE 0 END as relacion_goles_asistencias, "
                       "CASE WHEN COUNT(p.id) > 0 THEN COUNT(CASE WHEN p.resultado = 1 THEN 1 END) * 100.0 / COUNT(p.id) ELSE 0 END as porcentaje_victorias, "
                       "CASE WHEN COUNT(p.id) > 0 THEN COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) * 100.0 / COUNT(p.id) ELSE 0 END as porcentaje_lesiones_por_partido "
                       "FROM camiseta c "
                       "LEFT JOIN partido p ON c.id = p.camiseta_id "
                       "GROUP BY c.id, c.nombre "
                       "ORDER BY c.id", -1, &stmt, NULL);

    return stmt;
}

/**
 * @brief Exporta las camisetas con análisis avanzado a un archivo CSV mejorado
 *
 * Utiliza la función común de obtención de datos para evitar duplicación de código.
 * El formato CSV mejorado es ideal para análisis avanzado en herramientas como Excel,
 * proporcionando métricas de eficiencia y rendimiento que no están disponibles en la exportación estándar.
 */
void exportar_camisetas_csv_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(NULL, &count);
    if (!stmt) return;

    FILE *f = fopen(get_export_path("camisetas_mejorado.csv"), "w");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Escribir encabezado CSV con métricas avanzadas
    fprintf(f, "id,nombre,total_goles,total_asistencias,total_partidos,victorias,empates,derrotas,total_lesiones,rendimiento_promedio,cansancio_promedio,estado_animo_promedio,eficiencia_goles_por_partido,eficiencia_asistencias_por_partido,relacion_goles_asistencias,porcentaje_victorias,porcentaje_lesiones_por_partido\n");

    // Procesar cada fila de resultados con análisis avanzado
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%d,%s,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_double(stmt, 9),
                sqlite3_column_double(stmt, 10),
                sqlite3_column_double(stmt, 11),
                sqlite3_column_double(stmt, 12),
                sqlite3_column_double(stmt, 13),
                sqlite3_column_double(stmt, 14),
                sqlite3_column_double(stmt, 15),
                sqlite3_column_double(stmt, 16));

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.csv"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas con análisis avanzado a un archivo TXT mejorado
 *
 * Utiliza la función común de obtención de datos para evitar duplicación de código.
 * El formato TXT mejorado es ideal para documentación legible con análisis avanzado,
 * proporcionando información detallada sobre eficiencia y rendimiento para informes técnicos.
 */
void exportar_camisetas_txt_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(NULL, &count);
    if (!stmt) return;

    FILE *f = fopen(get_export_path("camisetas_mejorado.txt"), "w");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Escribir encabezado del archivo de texto con análisis avanzado
    fprintf(f, "LISTADO DE CAMISETAS CON ESTADISTICAS AVANZADAS\n\n");

    // Procesar cada fila de resultados con formato legible y métricas avanzadas
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "ID: %d - Nombre: %s\n"
                "  Goles Totales: %d\n"
                "  Asistencias Totales: %d\n"
                "  Partidos Totales: %d\n"
                "  Victorias: %d (%.2f%%)\n"
                "  Empates: %d\n"
                "  Derrotas: %d\n"
                "  Lesiones Totales: %d (%.2f%% por partido)\n"
                "  Rendimiento Promedio: %.2f\n"
                "  Cansancio Promedio: %.2f\n"
                "  Estado de Animo Promedio: %.2f\n"
                "  Eficiencia: %.2f goles/partido, %.2f asistencias/partido\n"
                "  Relacion Goles/Asistencias: %.2f\n\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_double(stmt, 15),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_double(stmt, 16),
                sqlite3_column_double(stmt, 9),
                sqlite3_column_double(stmt, 10),
                sqlite3_column_double(stmt, 11),
                sqlite3_column_double(stmt, 12),
                sqlite3_column_double(stmt, 13),
                sqlite3_column_double(stmt, 14));

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.txt"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas con análisis avanzado a un archivo JSON mejorado
 *
 * Utiliza la función común de obtención de datos para evitar duplicación de código.
 * El formato JSON mejorado es ideal para integración con aplicaciones y APIs,
 * proporcionando datos estructurados con análisis avanzado para procesamiento automatizado.
 */
void exportar_camisetas_json_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(NULL, &count);
    if (!stmt) return;

    FILE *f = fopen(get_export_path("camisetas_mejorado.json"), "w");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Crear estructura JSON y procesar resultados con métricas avanzadas
    cJSON *root = cJSON_CreateArray();

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(item, "nombre", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(item, "total_goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(item, "total_asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddNumberToObject(item, "total_partidos", sqlite3_column_int(stmt, 4));
        cJSON_AddNumberToObject(item, "victorias", sqlite3_column_int(stmt, 5));
        cJSON_AddNumberToObject(item, "empates", sqlite3_column_int(stmt, 6));
        cJSON_AddNumberToObject(item, "derrotas", sqlite3_column_int(stmt, 7));
        cJSON_AddNumberToObject(item, "total_lesiones", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(item, "rendimiento_promedio", sqlite3_column_double(stmt, 9));
        cJSON_AddNumberToObject(item, "cansancio_promedio", sqlite3_column_double(stmt, 10));
        cJSON_AddNumberToObject(item, "estado_animo_promedio", sqlite3_column_double(stmt, 11));
        cJSON_AddNumberToObject(item, "eficiencia_goles_por_partido", sqlite3_column_double(stmt, 12));
        cJSON_AddNumberToObject(item, "eficiencia_asistencias_por_partido", sqlite3_column_double(stmt, 13));
        cJSON_AddNumberToObject(item, "relacion_goles_asistencias", sqlite3_column_double(stmt, 14));
        cJSON_AddNumberToObject(item, "porcentaje_victorias", sqlite3_column_double(stmt, 15));
        cJSON_AddNumberToObject(item, "porcentaje_lesiones_por_partido", sqlite3_column_double(stmt, 16));
        cJSON_AddItemToArray(root, item);
    }

    // Escribir JSON al archivo y liberar recursos
    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.json"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas con análisis avanzado a un archivo HTML mejorado
 *
 * Utiliza la función común de obtención de datos para evitar duplicación de código.
 * El formato HTML mejorado es ideal para visualización interactiva en navegadores web,
 * proporcionando una interfaz de usuario amigable con análisis avanzado para presentación
 * de datos y reportes visuales.
 */
void exportar_camisetas_html_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(NULL, &count);
    if (!stmt) return;

    FILE *f = fopen(get_export_path("camisetas_mejorado.html"), "w");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Escribir encabezado HTML y estructura de tabla con métricas avanzadas
    fprintf(f,
            "<html><body><h1>Camisetas con Estadisticas Avanzadas</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Goles Totales</th><th>Asistencias Totales</th><th>Partidos Totales</th><th>Victorias</th><th>%% Victorias</th><th>Empates</th><th>Derrotas</th><th>Lesiones Totales</th><th>%% Lesiones</th><th>Rendimiento Promedio</th><th>Cansancio Promedio</th><th>Estado de Animo Promedio</th><th>Eficiencia Goles/P</th><th>Eficiencia Asist/P</th><th>Relacion G/A</th></tr>");

    // Procesar cada fila de resultados y generar filas HTML con métricas avanzadas
    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f%%</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f%%</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%.2f</td></tr>",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_double(stmt, 15),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_double(stmt, 16),
                sqlite3_column_double(stmt, 9),
                sqlite3_column_double(stmt, 10),
                sqlite3_column_double(stmt, 11),
                sqlite3_column_double(stmt, 12),
                sqlite3_column_double(stmt, 13),
                sqlite3_column_double(stmt, 14));

    // Cerrar estructura HTML
    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.html"));
    fclose(f);
}
