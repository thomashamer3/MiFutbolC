/**
 * @file dashboard.c
 * @brief Implementación del dashboard interactivo inicial
 */

#include "dashboard.h"
#include "db.h"
#include "utils.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, 0) == SQLITE_OK;
}

static int obtener_tiempo_local(time_t instante, struct tm *out_tm)
{
#ifdef _WIN32
    return localtime_s(out_tm, &instante) == 0;
#else
    return localtime_r(&instante, out_tm) != NULL;
#endif
}

/**
 * @brief Obtiene la racha actual analizando últimos partidos
 */
int obtener_racha_actual(char *tipo_racha)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT goles, asistencias FROM partido "
        "ORDER BY fecha DESC, hora DESC LIMIT 20;";

    int racha = 0;
    char tipo_inicial = '\0';
    *tipo_racha = 'N'; // Sin racha por defecto

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int goles = sqlite3_column_int(stmt, 0);
        int asistencias = sqlite3_column_int(stmt, 1);

        // Determinar resultado (simplificado: victoria si goles > 0)
        char tipo_actual;
        if (goles >= 3)
        {
            tipo_actual = 'V'; // Victoria
        }
        else if (goles == 0 && asistencias == 0)
        {
            tipo_actual = 'D'; // Derrota
        }
        else
        {
            tipo_actual = 'E'; // Empate/Normal
        }

        // Primera iteración: establecer tipo de racha
        if (tipo_inicial == '\0')
        {
            tipo_inicial = tipo_actual;
            *tipo_racha = tipo_actual;
            racha = 1;
        }
        // Si la racha continúa
        else if (tipo_actual == tipo_inicial)
        {
            racha++;
        }
        // Si se rompe la racha, detenerse
        else
        {
            break;
        }
    }

    sqlite3_finalize(stmt);
    return racha;
}

/**
 * @brief Cuenta recordatorios de hoy
 */
int contar_recordatorios_hoy()
{
    sqlite3_stmt *stmt;
    time_t ahora = time(NULL);
    struct tm tm_info = {0};
    if (!obtener_tiempo_local(ahora, &tm_info))
    {
        return 0;
    }
    char fecha_hoy[11];

    strftime(fecha_hoy, sizeof(fecha_hoy), "%Y-%m-%d", &tm_info);

    const char *sql =
        "SELECT COUNT(*) FROM recordatorios "
        "WHERE fecha = ?;";

    int count = 0;

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, fecha_hoy, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    return count;
}

/**
 * @brief Cuenta próximos partidos (simplificado: últimos 5 registrados)
 */
int contar_proximos_partidos()
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT COUNT(*) FROM partido "
        "WHERE fecha >= date('now') LIMIT 5;";

    int count = 0;

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    // Si no hay partidos futuros, mostrar "próximos" como últimos registrados
    if (count == 0)
    {
        const char *sql2 = "SELECT COUNT(*) FROM partido ORDER BY fecha DESC, hora DESC LIMIT 3;";
        if (preparar_stmt(sql2, &stmt))
        {
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }

    return count;
}

/**
 * @brief Obtiene balance del mes actual
 */
int obtener_balance_mes_actual()
{
    sqlite3_stmt *stmt;
    time_t ahora = time(NULL);
    struct tm tm_info = {0};
    if (!obtener_tiempo_local(ahora, &tm_info))
    {
        return 0;
    }
    char mes_actual[8];

    // Formato: YYYY-MM
    strftime(mes_actual, sizeof(mes_actual), "%Y-%m", &tm_info);

    const char *sql =
        "SELECT "
        "  SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) - "
        "  SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) "
        "FROM finanzas "
        "WHERE substr(fecha, 1, 7) = ?;";

    int balance = 0;

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, mes_actual, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            balance = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    return balance;
}

/**
 * @brief Obtiene progreso de logros
 */
int obtener_progreso_logros(int *total_logros)
{
    // Simulación: calcular basado en estadísticas reales
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM partido;";

    int partidos = 0;

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            partidos = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Total de logros definidos (simplificado)
    *total_logros = 50;

    // Logros completados estimados (basado en partidos jugados)
    int completados = 0;
    if (partidos >= 1) completados += 1;   // Primer partido
    if (partidos >= 10) completados += 1;  // 10 partidos
    if (partidos >= 25) completados += 1;  // 25 partidos
    if (partidos >= 50) completados += 1;  // 50 partidos
    if (partidos >= 100) completados += 1; // 100 partidos

    // Verificar goles
    const char *sql_goles = "SELECT SUM(goles) FROM partido;";
    if (preparar_stmt(sql_goles, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int goles = sqlite3_column_int(stmt, 0);
            if (goles >= 10) completados += 1;
            if (goles >= 50) completados += 1;
            if (goles >= 100) completados += 1;
            if (goles >= 200) completados += 1;
        }
        sqlite3_finalize(stmt);
    }

    return completados;
}

/**
 * @brief Muestra el dashboard principal
 */
void mostrar_dashboard()
{
    clear_screen();

    // Obtener datos
    char tipo_racha = 'N';
    int racha = obtener_racha_actual(&tipo_racha);
    int recordatorios_hoy = contar_recordatorios_hoy();
    int proximos = contar_proximos_partidos();
    int total_logros = 0;
    int logros_completados = obtener_progreso_logros(&total_logros);
    int balance = obtener_balance_mes_actual();

    // Calcular porcentaje de logros
    int porcentaje_logros = (total_logros > 0) ?
                            (logros_completados * 100 / total_logros) : 0;

    // Símbolo de racha
    const char *simbolo_racha;
    const char *texto_racha;

    if (racha >= 3)
    {
        if (tipo_racha == 'V')
        {
            simbolo_racha = "🔥";
            texto_racha = "victorias";
        }
        else if (tipo_racha == 'D')
        {
            simbolo_racha = "⛈️";
            texto_racha = "derrotas";
        }
        else
        {
            simbolo_racha = "📊";
            texto_racha = "partidos";
        }
    }
    else
    {
        simbolo_racha = "⚽";
        texto_racha = "sin racha";
        racha = 0;
    }

    // Mostrar dashboard
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              DASHBOARD - MiFutbolC 4.1                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");

    // Próximos partidos
    printf("║  📅 Proximos Partidos: %-2d                                 ║\n", proximos);

    // Recordatorios
    printf("║  ⏰ Recordatorios Hoy: %-2d                                 ║\n", recordatorios_hoy);

    // Racha
    if (racha > 0)
    {
        printf("║  %s Racha Actual: %d %s %-25s║\n",
               simbolo_racha, racha, texto_racha, "");
    }
    else
    {
        printf("║  %s Sin racha destacada                                     ║\n", simbolo_racha);
    }

    // Logros
    printf("║  🏆 Logros Desbloqueados: %d/%d (%d%%)%-19s║\n",
           logros_completados, total_logros, porcentaje_logros, "");

    // Balance
    if (balance >= 0)
    {
        printf("║  💰 Balance del Mes: +$%d%-31s║\n", balance, "");
    }
    else
    {
        printf("║  💰 Balance del Mes: -$%d%-31s║\n", -balance, "");
    }

    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    pause_console();
}
