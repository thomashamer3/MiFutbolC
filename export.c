/**
 * @file export.c
 * @brief Funciones para exportar datos de la base de datos a diferentes formatos
 *
 * Este archivo contiene funciones para exportar datos de lesiones, partidos,
 * estadísticas y análisis a formatos CSV, TXT, JSON y HTML.
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
 * @brief Elimina espacios en blanco al final de una cadena.
 *
 * @param str Cadena a recortar.
 * @return Puntero a la cadena recortada.
 */
char *trim_trailing_spaces(char *str)
{
    if (!str)
        return NULL;
    char *end = str + strlen(str) - 1;
    while (end > str && *end == ' ')
    {
        *end = '\0';
        end--;
    }
    return str;
}

/**
 * @brief Estructura para almacenar estadísticas de partidos
 */
typedef struct
{
    double avg_goles;
    double avg_asistencias;
    double avg_rendimiento;
    double avg_cansancio;
    double avg_animo;
    int total_partidos;
} Estadisticas;

/**
 * @brief Construye la ruta completa para un archivo de exportación
 *
 * Combina el directorio de datos con el nombre del archivo proporcionado
 * para crear una ruta completa.
 *
 * @param filename Nombre del archivo a exportar
 * @return Cadena de caracteres con la ruta completa del archivo
 */
char *get_export_path(const char *filename)
{
    static char path[1024];
    const char *export_dir = get_export_dir();
    if (!export_dir)
        return NULL;
    strcpy(path, export_dir);
    strcat(path, "\\");
    strcat(path, filename);
    return path;
}

/**
 * @brief Convierte el número de resultado a texto
 *
 * @param resultado Número del resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA)
 * @return Cadena de texto correspondiente al resultado
 */
const char *resultado_to_text(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "VICTORIA";
    case 2:
        return "EMPATE";
    case 3:
        return "DERROTA";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Convierte el número de clima a texto
 *
 * @param clima Número del clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio)
 * @return Cadena de texto correspondiente al clima
 */
const char *clima_to_text(int clima)
{
    switch (clima)
    {
    case 1:
        return "Despejado";
    case 2:
        return "Nublado";
    case 3:
        return "Lluvia";
    case 4:
        return "Ventoso";
    case 5:
        return "Mucho Calor";
    case 6:
        return "Mucho Frio";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Convierte el número de dia a texto
 *
 * @param dia Número del dia (1=Dia, 2=Tarde, 3=Noche)
 * @return Cadena de texto correspondiente al dia
 */
const char *dia_to_text(int dia)
{
    switch (dia)
    {
    case 1:
        return "Dia";
    case 2:
        return "Tarde";
    case 3:
        return "Noche";
    default:
        return "DESCONOCIDO";
    }
}

/* ===================== ANALISIS ===================== */

/**
 * @brief Calcula estadísticas generales de todos los partidos
 *
 * @param stats Puntero a la estructura donde almacenar las estadísticas
 */
static void calcular_estadisticas_generales(Estadisticas *stats)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                       "FROM partido",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->avg_goles = sqlite3_column_double(stmt, 1);
        stats->avg_asistencias = sqlite3_column_double(stmt, 2);
        stats->avg_rendimiento = sqlite3_column_double(stmt, 3);
        stats->avg_cansancio = sqlite3_column_double(stmt, 4);
        stats->avg_animo = sqlite3_column_double(stmt, 5);
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Calcula estadísticas de los últimos 5 partidos
 *
 * @param stats Puntero a la estructura donde almacenar las estadísticas
 */
static void calcular_estadisticas_ultimos5(Estadisticas *stats)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                       "FROM (SELECT * FROM partido ORDER BY fecha_hora DESC LIMIT 5)",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->avg_goles = sqlite3_column_double(stmt, 1);
        stats->avg_asistencias = sqlite3_column_double(stmt, 2);
        stats->avg_rendimiento = sqlite3_column_double(stmt, 3);
        stats->avg_cansancio = sqlite3_column_double(stmt, 4);
        stats->avg_animo = sqlite3_column_double(stmt, 5);
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Calcula la racha más larga de victorias y derrotas
 *
 * @param mejor_racha_victorias Puntero donde almacenar la mejor racha de victorias
 * @param peor_racha_derrotas Puntero donde almacenar la peor racha de derrotas
 */
static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT resultado FROM partido ORDER BY fecha_hora",
                       -1, &stmt, NULL);

    int racha_actual_v = 0, max_racha_v = 0;
    int racha_actual_d = 0, max_racha_d = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        if (resultado == 1)
        {
            // VICTORIA
            racha_actual_v++;
            if (racha_actual_v > max_racha_v)
                max_racha_v = racha_actual_v;
            racha_actual_d = 0;
        }
        else if (resultado == 3)
        {
            // DERROTA
            racha_actual_d++;
            if (racha_actual_d > max_racha_d)
                max_racha_d = racha_actual_d;
            racha_actual_v = 0;
        }
        else
        {
            // EMPATE
            racha_actual_v = 0;
            racha_actual_d = 0;
        }
    }

    *mejor_racha_victorias = max_racha_v;
    *peor_racha_derrotas = max_racha_d;
    sqlite3_finalize(stmt);
}

/**
 * @brief Genera un mensaje motivacional basado en el rendimiento
 *
 * @param ultimos Puntero a estadísticas de últimos 5 partidos
 * @param generales Puntero a estadísticas generales
 * @return Cadena de texto con el mensaje motivacional
 */
static const char *mensaje_motivacional(const Estadisticas *ultimos, const Estadisticas *generales)
{
    double diff_goles = ultimos->avg_goles - generales->avg_goles;
    double diff_rendimiento = ultimos->avg_rendimiento - generales->avg_rendimiento;

    if (diff_goles > 0.5 && diff_rendimiento > 0.5)
    {
        return "Excelente. Estas en racha ascendente. Sigue asi, tu esfuerzo esta dando frutos. Mantien la consistencia y continua trabajando duro en los entrenamientos.";
    }
    else if (diff_goles < -0.5 || diff_rendimiento < -0.5)
    {
        return "No te desanimes. Todos tenemos dias dificiles. Analiza que puedes mejorar: Revisa tu preparacion fisica y tecnica. Habla con tu entrenador sobre estrategias. Recuerda: el futbol es un deporte de perseverancia.";
    }
    else
    {
        return "Buen trabajo manteniendo el nivel. La consistencia es clave en el futbol. Sigue entrenando y manten la motivacion alta. Cada partido es una oportunidad!";
    }
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo CSV
 *
 * Crea un archivo CSV con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Tipo,Promedio_Goles,Promedio_Asistencias,Promedio_Rendimiento,Promedio_Cansancio,Promedio_Animo,Total_Partidos\n");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    fprintf(f, "Generales,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
            generales.avg_goles, generales.avg_asistencias, generales.avg_rendimiento,
            generales.avg_cansancio, generales.avg_animo, generales.total_partidos);

    fprintf(f, "Ultimos5,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
            ultimos5.avg_goles, ultimos5.avg_asistencias, ultimos5.avg_rendimiento,
            ultimos5.avg_cansancio, ultimos5.avg_animo, ultimos5.total_partidos);

    fprintf(f, "Rachas,%d,%d\n", mejor_racha_v, peor_racha_d);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "Mensaje,%s\n", msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.csv"));
    fclose(f);
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo de texto plano
 *
 * Crea un archivo de texto con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.txt"), "w");
    if (!f)
        return;

    fprintf(f, "ANALISIS DE RENDIMIENTO\n\n");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    fprintf(f, "ESTADISTICAS GENERALES:\n");
    fprintf(f, "Total Partidos: %d\n", generales.total_partidos);
    fprintf(f, "Promedio Goles: %.2f\n", generales.avg_goles);
    fprintf(f, "Promedio Asistencias: %.2f\n", generales.avg_asistencias);
    fprintf(f, "Promedio Rendimiento: %.2f\n", generales.avg_rendimiento);
    fprintf(f, "Promedio Cansancio: %.2f\n", generales.avg_cansancio);
    fprintf(f, "Promedio Estado Animo: %.2f\n\n", generales.avg_animo);

    fprintf(f, "ULTIMOS 5 PARTIDOS:\n");
    fprintf(f, "Total Partidos: %d\n", ultimos5.total_partidos);
    fprintf(f, "Promedio Goles: %.2f\n", ultimos5.avg_goles);
    fprintf(f, "Promedio Asistencias: %.2f\n", ultimos5.avg_asistencias);
    fprintf(f, "Promedio Rendimiento: %.2f\n", ultimos5.avg_rendimiento);
    fprintf(f, "Promedio Cansancio: %.2f\n", ultimos5.avg_cansancio);
    fprintf(f, "Promedio Estado Animo: %.2f\n\n", ultimos5.avg_animo);

    fprintf(f, "RACHAS:\n");
    fprintf(f, "Mejor racha de victorias: %d partidos\n", mejor_racha_v);
    fprintf(f, "Peor racha de derrotas: %d partidos\n\n", peor_racha_d);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "ANALISIS MOTIVACIONAL:\n%s\n", msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.txt"));
    fclose(f);
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo JSON
 *
 * Crea un archivo JSON con un objeto conteniendo todas las estadísticas
 * del análisis de rendimiento. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    cJSON *generales_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(generales_obj, "total_partidos", generales.total_partidos);
    cJSON_AddNumberToObject(generales_obj, "avg_goles", generales.avg_goles);
    cJSON_AddNumberToObject(generales_obj, "avg_asistencias", generales.avg_asistencias);
    cJSON_AddNumberToObject(generales_obj, "avg_rendimiento", generales.avg_rendimiento);
    cJSON_AddNumberToObject(generales_obj, "avg_cansancio", generales.avg_cansancio);
    cJSON_AddNumberToObject(generales_obj, "avg_animo", generales.avg_animo);
    cJSON_AddItemToObject(root, "generales", generales_obj);

    cJSON *ultimos5_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(ultimos5_obj, "total_partidos", ultimos5.total_partidos);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_goles", ultimos5.avg_goles);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_asistencias", ultimos5.avg_asistencias);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_rendimiento", ultimos5.avg_rendimiento);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_cansancio", ultimos5.avg_cansancio);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_animo", ultimos5.avg_animo);
    cJSON_AddItemToObject(root, "ultimos5", ultimos5_obj);

    cJSON *rachas_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rachas_obj, "mejor_racha_victorias", mejor_racha_v);
    cJSON_AddNumberToObject(rachas_obj, "peor_racha_derrotas", peor_racha_d);
    cJSON_AddItemToObject(root, "rachas", rachas_obj);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    cJSON_AddStringToObject(root, "mensaje_motivacional", msg);

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    printf("Archivo exportado a: %s\n", get_export_path("analisis.json"));
    fclose(f);
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo HTML
 *
 * Crea una página HTML con las estadísticas presentadas en formato web.
 * El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.html"), "w");
    if (!f)
        return;

    fprintf(f, "<html><body><h1>Analisis de Rendimiento</h1>");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    fprintf(f, "<h2>Estadisticas Generales</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Total Partidos</th><td>%d</td></tr>", generales.total_partidos);
    fprintf(f, "<tr><th>Promedio Goles</th><td>%.2f</td></tr>", generales.avg_goles);
    fprintf(f, "<tr><th>Promedio Asistencias</th><td>%.2f</td></tr>", generales.avg_asistencias);
    fprintf(f, "<tr><th>Promedio Rendimiento</th><td>%.2f</td></tr>", generales.avg_rendimiento);
    fprintf(f, "<tr><th>Promedio Cansancio</th><td>%.2f</td></tr>", generales.avg_cansancio);
    fprintf(f, "<tr><th>Promedio Estado Animo</th><td>%.2f</td></tr>", generales.avg_animo);
    fprintf(f, "</table>");

    fprintf(f, "<h2>Ultimos 5 Partidos</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Total Partidos</th><td>%d</td></tr>", ultimos5.total_partidos);
    fprintf(f, "<tr><th>Promedio Goles</th><td>%.2f</td></tr>", ultimos5.avg_goles);
    fprintf(f, "<tr><th>Promedio Asistencias</th><td>%.2f</td></tr>", ultimos5.avg_asistencias);
    fprintf(f, "<tr><th>Promedio Rendimiento</th><td>%.2f</td></tr>", ultimos5.avg_rendimiento);
    fprintf(f, "<tr><th>Promedio Cansancio</th><td>%.2f</td></tr>", ultimos5.avg_cansancio);
    fprintf(f, "<tr><th>Promedio Estado Animo</th><td>%.2f</td></tr>", ultimos5.avg_animo);
    fprintf(f, "</table>");

    fprintf(f, "<h2>Rachas</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Mejor Racha Victorias</th><td>%d partidos</td></tr>", mejor_racha_v);
    fprintf(f, "<tr><th>Peor Racha Derrotas</th><td>%d partidos</td></tr>", peor_racha_d);
    fprintf(f, "</table>");

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "<h2>Analisis Motivacional</h2>");
    fprintf(f, "<p>%s</p>", msg);

    fprintf(f, "</body></html>");
    printf("Archivo exportado a: %s\n", get_export_path("analisis.html"));
    fclose(f);
}
