/**
 * @file analisis.c
 * @brief Módulo para el análisis de rendimiento en partidos de fútbol.
 *
 * Este archivo contiene funciones para analizar el rendimiento comparando
 * los últimos 5 partidos con promedios generales, y calculando rachas.
 */

#include "analisis.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Convierte el número de resultado a texto
 *
 * @param resultado Número del resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA)
 * @return Cadena de texto correspondiente al resultado
 */
static const char *resultado_to_text(int resultado)
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
 * @brief Muestra los últimos 5 partidos
 */
static void mostrar_ultimos5_partidos()
{
    printf("\nULTIMOS 5 PARTIDOS:\n");
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT fecha_hora, goles, asistencias, rendimiento_general, resultado "
                       "FROM partido ORDER BY fecha_hora DESC LIMIT 5",
                       -1, &stmt, NULL);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        int goles = sqlite3_column_int(stmt, 1);
        int asistencias = sqlite3_column_int(stmt, 2);
        int rendimiento = sqlite3_column_int(stmt, 3);
        int resultado = sqlite3_column_int(stmt, 4);

        printf("%s | G:%d A:%d | Rend:%d | %s\n",
               fecha, goles, asistencias, rendimiento, resultado_to_text(resultado));
        count++;
    }

    if (count == 0)
    {
        printf("No hay partidos registrados.\n");
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Genera un mensaje motivacional basado en el rendimiento
 *
 * @param ultimos Puntero a estadísticas de últimos 5 partidos
 * @param generales Puntero a estadísticas generales
 */
static void mensaje_motivacional(const Estadisticas *ultimos, const Estadisticas *generales)
{
    printf("\nANALISIS MOTIVACIONAL:\n");
    printf("----------------------------------------\n");

    double diff_goles = ultimos->avg_goles - generales->avg_goles;
    double diff_rendimiento = ultimos->avg_rendimiento - generales->avg_rendimiento;

    if (diff_goles > 0.5 && diff_rendimiento > 0.5)
    {
        printf("Excelente Estas en racha ascendente. Sigue asi, tu esfuerzo está dando frutos.\n");
        printf("Manten la consistencia y continua trabajando duro en los entrenamientos.\n");
    }
    else if (diff_goles < -0.5 || diff_rendimiento < -0.5)
    {
        printf("No te desanimes. Todos tenemos dias dificiles. Analiza que puedes mejorar:\n");
        printf("- Revisa tu preparación física y tecnica.\n");
        printf("- Habla con tu entrenador sobre estrategias.\n");
        printf("- Recuerda: el fútbol es un deporte de perseverancia.\n");
    }
    else
    {
        printf("Buen trabajo manteniendo el nivel. La consistencia es clave en el futbol.\n");
        printf("Sigue entrenando y manten la motivacion alta. Cada partido es una oportunidad!\n");
    }
}

/**
 * @brief Muestra el análisis completo de rendimiento
 */
void mostrar_analisis()
{
    clear_screen();
    print_header("ANALISIS DE RENDIMIENTO");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    if (generales.total_partidos == 0)
    {
        printf("No hay suficientes datos para realizar el analisis.\n");
        printf("Registra al menos algunos partidos para ver estadisticas.\n");
        pause_console();
        return;
    }

    mostrar_ultimos5_partidos();

    printf("\nCOMPARACION ULTIMOS 5 VS PROMEDIO GENERAL:\n");
    printf("----------------------------------------\n");
    printf("Goles:        %.1f vs %.1f\n", ultimos5.avg_goles, generales.avg_goles);
    printf("Asistencias:  %.1f vs %.1f\n", ultimos5.avg_asistencias, generales.avg_asistencias);
    printf("Rendimiento:  %.1f vs %.1f\n", ultimos5.avg_rendimiento, generales.avg_rendimiento);
    printf("Cansancio:    %.1f vs %.1f\n", ultimos5.avg_cansancio, generales.avg_cansancio);
    printf("Estado Animo: %.1f vs %.1f\n", ultimos5.avg_animo, generales.avg_animo);

    printf("\nRACHAS:\n");
    printf("----------------------------------------\n");
    printf("Mejor racha de victorias: %d partidos\n", mejor_racha_v);
    printf("Peor racha de derrotas: %d partidos\n", peor_racha_d);

    mensaje_motivacional(&ultimos5, &generales);

    pause_console();
}
