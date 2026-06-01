
#include "estadisticas_anio.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#define STATS_ANIO_BUF_LEN 32768
static char s_anio_cache[STATS_ANIO_BUF_LEN];
static int  s_anio_valid   = 0;
static int  s_anio_changes = -1;

void mostrar_estadisticas_por_anio()
{
    clear_screen();
    print_header("ESTADISTICAS POR ANIO");

    int current_changes = sqlite3_total_changes(db);
    if (s_anio_valid && current_changes == s_anio_changes)
    {
        printf("%s", s_anio_cache);
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt,
                              "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias "
                              "FROM partido p "
                              "JOIN camiseta c ON p.camiseta_id = c.id "
                              "GROUP BY anio, c.id "
                              "ORDER BY anio DESC, total_goles DESC"))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    size_t pos = 0;
    char current_anio[5] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EstadisticaAnio stats;
        extraer_estadistica_anio(stmt, &stats);

        if (strcmp(current_anio, stats.anio) != 0)
        {
            if (hay) pos += (size_t)snprintf(s_anio_cache + pos, sizeof(s_anio_cache) - pos, "\n");
            pos += (size_t)snprintf(s_anio_cache + pos, sizeof(s_anio_cache) - pos, "Anio: %s\n", stats.anio);
            pos += (size_t)snprintf(s_anio_cache + pos, sizeof(s_anio_cache) - pos, "----------------------------------------\n");
            strcpy_s(current_anio, sizeof(current_anio), stats.anio);
        }

        pos += (size_t)snprintf(s_anio_cache + pos, sizeof(s_anio_cache) - pos,
                                "%-30s | PJ: %d | G: %d | A: %d | G/P: %.2f | A/P: %.2f\n",
                                stats.camiseta, stats.partidos, stats.total_goles,
                                stats.total_asistencias, stats.avg_goles, stats.avg_asistencias);
        hay = 1;
    }

    if (!hay)
        pos += (size_t)snprintf(s_anio_cache + pos, sizeof(s_anio_cache) - pos, "No hay estadisticas registradas.\n");

    sqlite3_finalize(stmt);

    s_anio_valid   = 1;
    s_anio_changes = current_changes;

    printf("%s", s_anio_cache);
    pause_console();
}
