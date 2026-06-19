
#include "estadisticas_mes.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#define STATS_MES_BUF_LEN 65536
static char s_mes_cache[STATS_MES_BUF_LEN];
static int  s_mes_valid   = 0;
static int  s_mes_changes = -1;

static void preparar_consulta(sqlite3_stmt **stmt)
{
    preparar_stmt_export(stmt,
                         "SELECT substr(fecha_hora, 7, 4) || '-' || substr(fecha_hora, 4, 2) AS mes_anio, COALESCE(c.nombre, 'Sin Camiseta'), COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias "
                         "FROM partido p "
                         "LEFT JOIN camiseta c ON p.camiseta_id = c.id "
                         "GROUP BY mes_anio, c.id "
                         "ORDER BY mes_anio DESC, total_goles DESC");
}

static void procesar_resultados_a_buf(sqlite3_stmt *stmt, char *buf, size_t buf_size, size_t *pos)
{
    char current_mes[8] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes_anio = (const char *)sqlite3_column_text(stmt, 0);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        int partidos         = sqlite3_column_int(stmt, 2);
        int total_goles      = sqlite3_column_int(stmt, 3);
        int total_asistencias = sqlite3_column_int(stmt, 4);
        double avg_goles     = sqlite3_column_double(stmt, 5);
        double avg_asistencias = sqlite3_column_double(stmt, 6);

        if (strcmp(current_mes, mes_anio) != 0)
        {
            if (hay)
            {
                snprintf(buf + *pos, buf_size - *pos, "\n");
                *pos += strnlen_s(buf + *pos, buf_size - *pos);
            }
            snprintf(buf + *pos, buf_size - *pos, "Mes: %s\n", mes_anio);
            *pos += strnlen_s(buf + *pos, buf_size - *pos);
            snprintf(buf + *pos, buf_size - *pos, "----------------------------------------\n");
            *pos += strnlen_s(buf + *pos, buf_size - *pos);
            snprintf(current_mes, sizeof(current_mes), "%s", mes_anio);
        }

        snprintf(buf + *pos, buf_size - *pos,
                 "%-30s | PJ: %d | G: %d | A: %d | G/P: %.2f | A/P: %.2f\n",
                 camiseta, partidos, total_goles, total_asistencias, avg_goles, avg_asistencias);
        *pos += strnlen_s(buf + *pos, buf_size - *pos);
        hay = 1;
    }

    if (!hay)
    {
        snprintf(buf + *pos, buf_size - *pos, "No hay estadisticas registradas.\n");
        *pos += strnlen_s(buf + *pos, buf_size - *pos);
    }
}

/**
 * Muestra estadisticas historicas agrupadas por mes.
 * Permite analizar tendencias temporales en el rendimiento deportivo, facilitando la identificacion de patrones y mejoras.
 */
void mostrar_estadisticas_por_mes(void)
{
    clear_screen();
    print_header("ESTADISTICAS POR MES");

    int current_changes = sqlite3_total_changes(db);
    if (s_mes_valid && current_changes == s_mes_changes)
    {
        printf("%s", s_mes_cache);
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    preparar_consulta(&stmt);
    if (!stmt)
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    size_t pos = 0;
    s_mes_cache[0] = '\0';
    procesar_resultados_a_buf(stmt, s_mes_cache, sizeof(s_mes_cache), &pos);
    sqlite3_finalize(stmt);

    s_mes_valid   = 1;
    s_mes_changes = current_changes;

    printf("%s", s_mes_cache);
    pause_console();
}
