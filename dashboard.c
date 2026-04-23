#include "dashboard.h"
#include "db.h"
#include "utils.h"
#include "settings.h"
#include "logros.h"
#include "ascii_art.h"
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

int obtener_progreso_logros(int *total_logros)
{
    *total_logros = logros_get_total();
    return logros_get_completados_primera_camiseta();
}

typedef struct dashboard_view_data_t
{
    int racha;
    int recordatorios_hoy;
    int proximos;
    int total_logros;
    int logros_completados;
    int balance;
    int porcentaje_logros;
    const char *simbolo_racha;
    const char *texto_racha;
} dashboard_view_data_t;

static void configurar_racha_visual(dashboard_view_data_t *data, int usar_unicode, char tipo_racha)
{
    if (data->racha < 3)
    {
        data->simbolo_racha = usar_unicode ? "" : "o";
        data->texto_racha = "sin racha";
        data->racha = 0;
        return;
    }

    if (tipo_racha == 'V')
    {
        data->simbolo_racha = usar_unicode ? "" : "*";
        data->texto_racha = "victorias";
        return;
    }

    if (tipo_racha == 'D')
    {
        data->simbolo_racha = usar_unicode ? "" : "X";
        data->texto_racha = "derrotas";
        return;
    }

    data->simbolo_racha = usar_unicode ? "" : "=";
    data->texto_racha = "partidos";
}

static void imprimir_racha_unicode(const dashboard_view_data_t *data)
{
    if (data->racha > 0)
    {
        printf("║  %s Racha Actual: %d %s %-25s║\n",
               data->simbolo_racha, data->racha, data->texto_racha, "");
        return;
    }

    printf("║  %s Sin racha destacada                                     ║\n", data->simbolo_racha);
}

static void imprimir_racha_ascii(const dashboard_view_data_t *data)
{
    if (data->racha > 0)
    {
        printf("|  %s Racha Actual: %d %s%-32s|\n",
               data->simbolo_racha, data->racha, data->texto_racha, "");
        return;
    }

    printf("|  Sin racha destacada                                         |\n");
}

static void imprimir_balance_unicode(const dashboard_view_data_t *data)
{
    if (data->balance >= 0)
    {
        printf("║  Balance del Mes: +$%d%-31s║\n", data->balance, "");
        return;
    }

    printf("║  Balance del Mes: -$%d%-31s║\n", -data->balance, "");
}

static void imprimir_balance_ascii(const dashboard_view_data_t *data)
{
    if (data->balance >= 0)
    {
        printf("|  Balance del Mes: +$%d%-37s|\n", data->balance, "");
        return;
    }

    printf("|  Balance del Mes: -$%d%-37s|\n", -data->balance, "");
}

static void imprimir_dashboard_unicode(const dashboard_view_data_t *data)
{
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              DASHBOARD - MiFutbolC 4.2                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");

    printf("║  Proximos Partidos: %-2d                                 ║\n", data->proximos);
    printf("║  Recordatorios Hoy: %-2d                                 ║\n", data->recordatorios_hoy);

    imprimir_racha_unicode(data);

    printf("║  Logros Desbloqueados: %d/%d (%d%%)%-19s║\n",
           data->logros_completados, data->total_logros, data->porcentaje_logros, "");

    imprimir_balance_unicode(data);

    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void imprimir_dashboard_ascii(const dashboard_view_data_t *data)
{
    printf("+--------------------------------------------------------------+\n");
    printf("|              DASHBOARD - MiFutbolC 4.2                      |\n");
    printf("+--------------------------------------------------------------+\n");
    printf("|                                                              |\n");

    printf("|  Proximos Partidos: %-2d                                      |\n", data->proximos);
    printf("|  Recordatorios Hoy: %-2d                                      |\n", data->recordatorios_hoy);

    imprimir_racha_ascii(data);

    printf("|  Logros Desbloqueados: %d/%d (%d%%)%-22s|\n",
           data->logros_completados, data->total_logros, data->porcentaje_logros, "");

    imprimir_balance_ascii(data);

    printf("|                                                              |\n");
    printf("+--------------------------------------------------------------+\n");
}

void mostrar_dashboard()
{
    clear_screen();
    print_header("DASHBOARD - MiFutbolC");

    int usar_unicode = consola_soporta_unicode();

    dashboard_view_data_t data = {0};
    char tipo_racha = 'N';

    data.racha = obtener_racha_actual(&tipo_racha);
    data.recordatorios_hoy = contar_recordatorios_hoy();
    data.proximos = contar_proximos_partidos();
    data.logros_completados = obtener_progreso_logros(&data.total_logros);
    data.balance = obtener_balance_mes_actual();
    data.porcentaje_logros =
        (data.total_logros > 0) ? (data.logros_completados * 100 / data.total_logros) : 0;

    configurar_racha_visual(&data, usar_unicode, tipo_racha);

    printf("\n");

    if (usar_unicode)
    {
        imprimir_dashboard_unicode(&data);
    }
    else
    {
        imprimir_dashboard_ascii(&data);
    }

    printf("\n");

    pause_console();
}
