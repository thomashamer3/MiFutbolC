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
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/**
 * @brief Estructura para almacenar estadísticas mensuales
 */
typedef struct
{
    int mes;
    int anio;
    double avg_goles;
    double avg_asistencias;
    double avg_rendimiento;
    int total_partidos;
} EstadisticasMensuales;

/**
 * @brief Convierte número de mes a nombre
 *
 * @param mes Número del mes (1-12)
 * @return Nombre del mes en español
 */
static const char *mes_to_text(int mes)
{
    switch (mes)
    {
    case 1:
        return "Enero";
    case 2:
        return "Febrero";
    case 3:
        return "Marzo";
    case 4:
        return "Abril";
    case 5:
        return "Mayo";
    case 6:
        return "Junio";
    case 7:
        return "Julio";
    case 8:
        return "Agosto";
    case 9:
        return "Septiembre";
    case 10:
        return "Octubre";
    case 11:
        return "Noviembre";
    case 12:
        return "Diciembre";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Calcula estadísticas mensuales para una métrica específica
 *
 * @param stats Array donde almacenar las estadísticas mensuales
 * @param max_stats Tamaño máximo del array
 * @param columna Nombre de la columna a promediar (goles, asistencias, rendimiento_general)
 * @return Número de meses con datos
 */
static int calcular_estadisticas_mensuales(EstadisticasMensuales *stats, int max_stats, const char *columna)
{
    sqlite3_stmt *stmt;
    char sql[512];

    sprintf(sql,
            "SELECT strftime('%%m', fecha_hora) as mes, strftime('%%Y', fecha_hora) as anio, "
            "AVG(%s), COUNT(*) "
            "FROM partido "
            "GROUP BY strftime('%%Y', fecha_hora), strftime('%%m', fecha_hora) "
            "ORDER BY anio DESC, mes DESC",
            columna);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_stats)
    {
        stats[count].mes = atoi((const char *)sqlite3_column_text(stmt, 0));
        stats[count].anio = atoi((const char *)sqlite3_column_text(stmt, 1));
        stats[count].avg_goles = sqlite3_column_double(stmt, 2);
        stats[count].total_partidos = sqlite3_column_int(stmt, 3);
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/**
 * @brief Muestra la evolución mensual de una métrica
 *
 * @param titulo Título a mostrar
 * @param columna Nombre de la columna de la base de datos
 */
static void mostrar_evolucion_mensual(const char *titulo, const char *columna)
{
    clear_screen();
    print_header(titulo);

    EstadisticasMensuales stats[120]; // Máximo 10 años de datos
    int num_meses = calcular_estadisticas_mensuales(stats, 120, columna);

    if (num_meses == 0)
    {
        printf("No hay suficientes datos para mostrar la evolución mensual.\n");
        printf("Registra al menos algunos partidos para ver estadísticas.\n");
        pause_console();
        return;
    }

    printf("EVOLUCION MENSUAL:\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < num_meses; i++)
    {
        printf("%s %d: %.2f (%d partidos)\n",
               mes_to_text(stats[i].mes), stats[i].anio,
               stats[i].avg_goles, stats[i].total_partidos);
    }

    pause_console();
}

/**
 * @brief Encuentra el mejor o peor mes histórico
 *
 * @param mejor 1 para mejor mes, 0 para peor mes
 */
static void encontrar_mes_historico(int mejor)
{
    clear_screen();
    print_header(mejor ? "MEJOR MES HISTORICO" : "PEOR MES HISTORICO");

    sqlite3_stmt *stmt;
    const char *sql = mejor ?
                      "SELECT strftime('%m', fecha_hora) as mes, strftime('%Y', fecha_hora) as anio, "
                      "AVG(rendimiento_general), COUNT(*) "
                      "FROM partido "
                      "GROUP BY strftime('%Y', fecha_hora), strftime('%m', fecha_hora) "
                      "ORDER BY AVG(rendimiento_general) DESC LIMIT 1" :
                      "SELECT strftime('%m', fecha_hora) as mes, strftime('%Y', fecha_hora) as anio, "
                      "AVG(rendimiento_general), COUNT(*) "
                      "FROM partido "
                      "GROUP BY strftime('%Y', fecha_hora), strftime('%m', fecha_hora) "
                      "ORDER BY AVG(rendimiento_general) ASC LIMIT 1";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int mes = atoi((const char *)sqlite3_column_text(stmt, 0));
        int anio = atoi((const char *)sqlite3_column_text(stmt, 1));
        double avg_rendimiento = sqlite3_column_double(stmt, 2);
        int partidos = sqlite3_column_int(stmt, 3);

        printf("%s MES HISTORICO:\n", mejor ? "MEJOR" : "PEOR");
        printf("----------------------------------------\n");
        printf("Mes: %s %d\n", mes_to_text(mes), anio);
        printf("Rendimiento promedio: %.2f\n", avg_rendimiento);
        printf("Partidos jugados: %d\n", partidos);
    }
    else
    {
        printf("No hay suficientes datos para determinar el %s mes histórico.\n",
               mejor ? "mejor" : "peor");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Compara rendimiento al inicio vs fin de año
 */
static void comparar_inicio_fin_anio()
{
    clear_screen();
    print_header("INICIO VS FIN DE ANIO");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "CASE WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) <= 6 THEN 'Inicio' ELSE 'Fin' END as periodo, "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), COUNT(*) "
        "FROM partido "
        "GROUP BY CASE WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) <= 6 THEN 'Inicio' ELSE 'Fin' END";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    printf("COMPARACION INICIO VS FIN DE ANIO:\n");
    printf("----------------------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
        double avg_goles = sqlite3_column_double(stmt, 1);
        double avg_asistencias = sqlite3_column_double(stmt, 2);
        double avg_rendimiento = sqlite3_column_double(stmt, 3);
        int partidos = sqlite3_column_int(stmt, 4);

        printf("%s de año (Ene-Jun):\n", strcmp(periodo, "Inicio") == 0 ? "Inicio" : "Fin");
        printf("  Goles: %.2f\n", avg_goles);
        printf("  Asistencias: %.2f\n", avg_asistencias);
        printf("  Rendimiento: %.2f\n", avg_rendimiento);
        printf("  Partidos: %d\n\n", partidos);
        count++;
    }

    if (count == 0)
    {
        printf("No hay suficientes datos para comparar inicio vs fin de año.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Compara rendimiento en meses fríos vs cálidos
 */
static void comparar_meses_frios_calidos()
{
    clear_screen();
    print_header("MESES FRIOS VS CALIDOS");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "CASE "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) BETWEEN 6 AND 9 THEN 'Frios' "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) IN (12,1,2,3,4) THEN 'Calidos' "
        "  ELSE 'Otros' "
        "END as temporada, "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), COUNT(*) "
        "FROM partido "
        "GROUP BY CASE "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) BETWEEN 6 AND 9 THEN 'Frios' "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) IN (12,1,2,3,4) THEN 'Calidos' "
        "  ELSE 'Otros' "
        "END";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    printf("COMPARACION MESES FRIOS VS CALIDOS:\n");
    printf("----------------------------------------\n");
    printf("Meses frios: Junio, Julio, Agosto, Septiembre\n");
    printf("Meses calidos: Diciembre, Enero, Febrero, Marzo, Abril\n\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *temporada = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(temporada, "Otros") == 0)
            continue;

        double avg_goles = sqlite3_column_double(stmt, 1);
        double avg_asistencias = sqlite3_column_double(stmt, 2);
        double avg_rendimiento = sqlite3_column_double(stmt, 3);
        int partidos = sqlite3_column_int(stmt, 4);

        printf("Meses %s:\n", temporada);
        printf("  Goles: %.2f\n", avg_goles);
        printf("  Asistencias: %.2f\n", avg_asistencias);
        printf("  Rendimiento: %.2f\n", avg_rendimiento);
        printf("  Partidos: %d\n\n", partidos);
        count++;
    }

    if (count == 0)
    {
        printf("No hay suficientes datos en meses frios o calidos para comparar.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Calcula y muestra el progreso total del jugador
 */
static void calcular_progreso_total()
{
    clear_screen();
    print_header("PROGRESO TOTAL DEL JUGADOR");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "COUNT(*), "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), "
        "MIN(fecha_hora), MAX(fecha_hora) "
        "FROM partido";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int total_partidos = sqlite3_column_int(stmt, 0);
        double avg_goles = sqlite3_column_double(stmt, 1);
        double avg_asistencias = sqlite3_column_double(stmt, 2);
        double avg_rendimiento = sqlite3_column_double(stmt, 3);
        const char *fecha_inicio = (const char *)sqlite3_column_text(stmt, 4);
        const char *fecha_fin = (const char *)sqlite3_column_text(stmt, 5);

        printf("PROGRESO TOTAL DEL JUGADOR:\n");
        printf("----------------------------------------\n");
        printf("Periodo: %s - %s\n", fecha_inicio ? fecha_inicio : "N/A", fecha_fin ? fecha_fin : "N/A");
        printf("Total de partidos: %d\n", total_partidos);
        printf("Promedio de goles: %.2f\n", avg_goles);
        printf("Promedio de asistencias: %.2f\n", avg_asistencias);
        printf("Promedio de rendimiento: %.2f\n", avg_rendimiento);

        // Calcular tendencia (comparar primeros vs últimos partidos)
        if (total_partidos >= 10)
        {
            sqlite3_stmt *tend_stmt;
            const char *tend_sql =
                "SELECT AVG(rendimiento_general) FROM "
                "(SELECT rendimiento_general FROM partido ORDER BY fecha_hora ASC LIMIT 5) "
                "UNION ALL "
                "SELECT AVG(rendimiento_general) FROM "
                "(SELECT rendimiento_general FROM partido ORDER BY fecha_hora DESC LIMIT 5)";

            if (sqlite3_prepare_v2(db, tend_sql, -1, &tend_stmt, NULL) == SQLITE_OK)
            {
                double avg_primeros = 0, avg_ultimos = 0;
                if (sqlite3_step(tend_stmt) == SQLITE_ROW)
                    avg_primeros = sqlite3_column_double(tend_stmt, 0);
                if (sqlite3_step(tend_stmt) == SQLITE_ROW)
                    avg_ultimos = sqlite3_column_double(tend_stmt, 0);

                double tendencia = avg_ultimos - avg_primeros;
                printf("\nTENDENCIA:\n");
                printf("Primeros 5 partidos: %.2f\n", avg_primeros);
                printf("Últimos 5 partidos: %.2f\n", avg_ultimos);
                printf("Tendencia: %s (%.2f)\n",
                       tendencia > 0.5 ? "ASCENDENTE" : (tendencia < -0.5 ? "DESCENDENTE" : "ESTABLE"),
                       tendencia);

                sqlite3_finalize(tend_stmt);
            }
        }
    }
    else
    {
        printf("No hay datos suficientes para calcular el progreso total.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Muestra el menú de evolución temporal
 */
void mostrar_evolucion_temporal()
{
    clear_screen();
    print_header("EVOLUCION TEMPORAL");

    MenuItem items[] =
    {
        {1, "Evolucion Mensual de Goles", evolucion_mensual_goles},
        {2, "Evolucion Mensual de Asistencias", evolucion_mensual_asistencias},
        {3, "Evolucion Mensual de Rendimiento", evolucion_mensual_rendimiento},
        {4, "Mejor Mes Historico", mejor_mes_historico},
        {5, "Peor Mes Historico", peor_mes_historico},
        {6, "Inicio vs Fin de Anio", inicio_vs_fin_anio},
        {7, "Meses Frios vs Calidos", meses_frios_vs_calidos},
        {8, "Progreso Total del Jugador", progreso_total_jugador},
        {0, "Volver", NULL}
    };

    ejecutar_menu("EVOLUCION TEMPORAL", items, 9);
}

/**
 * @brief Muestra la evolución mensual de goles
 */
void evolucion_mensual_goles()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE GOLES", "goles");
}

/**
 * @brief Muestra la evolución mensual de asistencias
 */
void evolucion_mensual_asistencias()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE ASISTENCIAS", "asistencias");
}

/**
 * @brief Muestra la evolución mensual de rendimiento
 */
void evolucion_mensual_rendimiento()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE RENDIMIENTO", "rendimiento_general");
}

/**
 * @brief Muestra el mejor mes histórico
 */
void mejor_mes_historico()
{
    encontrar_mes_historico(1);
}

/**
 * @brief Muestra el peor mes histórico
 */
void peor_mes_historico()
{
    encontrar_mes_historico(0);
}

/**
 * @brief Compara el rendimiento al inicio vs fin de año
 */
void inicio_vs_fin_anio()
{
    comparar_inicio_fin_anio();
}

/**
 * @brief Compara el rendimiento en meses fríos vs cálidos
 */
void meses_frios_vs_calidos()
{
    comparar_meses_frios_calidos();
}

/**
 * @brief Muestra el progreso total del jugador
 */
void progreso_total_jugador()
{
    calcular_progreso_total();
}
