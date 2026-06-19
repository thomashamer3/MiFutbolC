
#include "estadisticas_generales.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define STATS_GEN_COUNT 11
#define STATS_GEN_OUTPUT_LEN 512

typedef struct
{
    char buf[STATS_GEN_OUTPUT_LEN];
} StatsGenItem;

static StatsGenItem s_stats_cache[STATS_GEN_COUNT];
static int s_stats_valid = 0;
static int s_stats_changes = -1;

static size_t estadisticas_generales_strnlen_seguro(const char *texto,
        size_t max_len)
{
    if (!texto)
    {
        return 0;
    }

#if defined(__STDC_LIB_EXT1__)
    return strnlen_s(texto, max_len);
#elif defined(_MSC_VER)
    return strnlen_s(texto, max_len);
#else
    size_t i = 0;
    while (i < max_len && texto[i] != '\0')
    {
        i++;
    }
    return i;
#endif
}

static void mostrar_por_dia_semana(const char *titulo, const char *columna,
                                   const char *order_by, int limit)
{
    clear_screen();
    print_header(titulo);

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes(titulo));
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    char sql[1024];
    const char *limit_clause = (limit > 0) ? " LIMIT 1" : "";

    int order_by_columna =
        (strcmp(order_by, "DESC") != 0 && strcmp(order_by, "ASC") != 0);

    snprintf(sql, sizeof(sql),
             "WITH dias_semana AS ("
             "SELECT 0 AS dia_num, 'Domingo' AS dia_nombre UNION ALL "
             "SELECT 1, 'Lunes' UNION ALL "
             "SELECT 2, 'Martes' UNION ALL "
             "SELECT 3, 'Miercoles' UNION ALL "
             "SELECT 4, 'Jueves' UNION ALL "
             "SELECT 5, 'Viernes' UNION ALL "
             "SELECT 6, 'Sabado'"
             ") "
             "SELECT ds.dia_nombre, "
             "ROUND(COALESCE(AVG(p.%s), 0), 2) AS promedio "
             "FROM dias_semana ds "
             "LEFT JOIN partido p ON CAST(strftime('%%w', substr(p.fecha_hora, "
             "7, 4) || '-' || substr(p.fecha_hora, 4, 2) || '-' || "
             "substr(p.fecha_hora, 1, 2)) AS INTEGER) = ds.dia_num "
             "AND p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
             "GROUP BY ds.dia_num, ds.dia_nombre "
             "ORDER BY %s%s%s",
             columna, order_by_columna ? "" : "promedio ", order_by,
             limit_clause);

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *dia = (const char *)sqlite3_column_text(stmt, 0);
        double promedio = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f\n", remover_tildes(dia), promedio);
    }

    sqlite3_finalize(stmt);
    pause_console();
}

// Array de dias de la semana en espanol
const char *dias[] = {"Domingo", "Lunes",   "Martes", "Miercoles",
                      "Jueves",  "Viernes", "Sabado"
                     };

static void query(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;
    char nombre[200];
    int num_cols;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }
    num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols == 1)
        {
            if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
            {
                printf("%d\n", sqlite3_column_int(stmt, 0));
            }
            else if (sqlite3_column_type(stmt, 0) == SQLITE_FLOAT)
            {
                printf("%.2f\n", sqlite3_column_double(stmt, 0));
            }
            else
            {
                snprintf(nombre, sizeof(nombre), "%s", sqlite3_column_text(stmt, 0));
                printf("%s\n", nombre);
            }
        }
        else
        {
            snprintf(nombre, sizeof(nombre), "%s", sqlite3_column_text(stmt, 0));

            // Check if the second column is integer or real
            if (sqlite3_column_type(stmt, 1) == SQLITE_INTEGER)
            {
                printf("%-30s : %d\n", nombre, sqlite3_column_int(stmt, 1));
            }
            else if (sqlite3_column_type(stmt, 1) == SQLITE_FLOAT)
            {
                printf("%-30s : %.2f\n", nombre, sqlite3_column_double(stmt, 1));
            }
            else
            {
                // Fallback to int
                printf("%-30s : %d\n", nombre, sqlite3_column_int(stmt, 1));
            }
        }
    }

    sqlite3_finalize(stmt);
}

static void query_to_buf(const char *sql, char *out, size_t out_size)
{
    sqlite3_stmt *stmt;
    char nombre[200];
    int num_cols;
    size_t pos = 0;
    out[0] = '\0';

    if (!preparar_stmt_export(&stmt, sql))
    {
        snprintf(out, out_size, "Error al consultar la base de datos.\n");
        return;
    }
    num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char line[256];
        if (num_cols == 1)
        {
            if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
                snprintf(line, sizeof(line), "%d\n", sqlite3_column_int(stmt, 0));
            else if (sqlite3_column_type(stmt, 0) == SQLITE_FLOAT)
                snprintf(line, sizeof(line), "%.2f\n", sqlite3_column_double(stmt, 0));
            else
                snprintf(line, sizeof(line), "%s\n",
                         (const char *)sqlite3_column_text(stmt, 0));
        }
        else
        {
            snprintf(nombre, sizeof(nombre), "%s",
                     (const char *)sqlite3_column_text(stmt, 0));
            if (sqlite3_column_type(stmt, 1) == SQLITE_INTEGER)
                snprintf(line, sizeof(line), "%-30s : %d\n", nombre,
                         sqlite3_column_int(stmt, 1));
            else if (sqlite3_column_type(stmt, 1) == SQLITE_FLOAT)
                snprintf(line, sizeof(line), "%-30s : %.2f\n", nombre,
                         sqlite3_column_double(stmt, 1));
            else
                snprintf(line, sizeof(line), "%-30s : %d\n", nombre,
                         sqlite3_column_int(stmt, 1));
        }
        size_t line_len = estadisticas_generales_strnlen_seguro(line, sizeof(line));
        if (pos + line_len + 1 < out_size)
        {
            memcpy(out + pos, line, line_len);
            pos += line_len;
            out[pos] = '\0';
        }
    }

    sqlite3_finalize(stmt);
}

static void mostrar_query_simple(const char *header, const char *titulo,
                                 const char *sql)
{
    clear_screen();
    print_header(header);
    query(titulo, sql);
    pause_console();
}

#define SQL_CAMISETA_AGREGADA(expr, orden)                                     \
  "SELECT c.nombre, " expr " "                                                 \
  "FROM partido p "                                                            \
  "JOIN camiseta c ON p.camiseta_id=c.id "                                     \
  "GROUP BY c.id "                                                             \
  "ORDER BY 2 " orden " LIMIT 1"

#define SQL_CAMISETA_POR_RESULTADO(resultado)                                  \
  "SELECT c.nombre, COUNT(*) "                                                 \
  "FROM partido p "                                                            \
  "JOIN camiseta c ON p.camiseta_id=c.id "                                     \
  "WHERE p.resultado = " resultado " "                                         \
  "GROUP BY c.id "                                                             \
  "ORDER BY 2 DESC LIMIT 1"

void mostrar_estadisticas_generales(void)
{
    clear_screen();
    print_header("ESTADISTICAS");
    typedef struct
    {
        const char *titulo;
        const char *sql;
    } StatQuery;

    StatQuery queries[] =
    {
        {
            "Camiseta con mas Goles",
            SQL_CAMISETA_AGREGADA("IFNULL(SUM(p.goles),0)", "DESC")
        },
        {
            "Camiseta con mas Asistencias",
            SQL_CAMISETA_AGREGADA("IFNULL(SUM(p.asistencias),0)", "DESC")
        },
        {"Camiseta con mas Partidos", SQL_CAMISETA_AGREGADA("COUNT(*)", "DESC")},
        {
            "Camiseta con mas Goles + Asistencias",
            SQL_CAMISETA_AGREGADA("IFNULL(SUM(p.goles+p.asistencias),0)", "DESC")
        },
        {
            "Camiseta con mejor Rendimiento General promedio",
            SQL_CAMISETA_AGREGADA(
                "IFNULL(ROUND(AVG(p.rendimiento_general), 2), 0.00)", "DESC")
        },
        {
            "Camiseta con mejor Estado de Animo promedio",
            SQL_CAMISETA_AGREGADA("IFNULL(ROUND(AVG(p.estado_animo), 2), 0.00)",
                                  "DESC")
        },
        {
            "Camiseta con menos Cansancio promedio",
            SQL_CAMISETA_AGREGADA("IFNULL(ROUND(AVG(p.cansancio), 2), 0.00)",
                                  "ASC")
        },
        {"Camiseta con mas Victorias", SQL_CAMISETA_POR_RESULTADO("1")},
        {"Camiseta con mas Empates", SQL_CAMISETA_POR_RESULTADO("2")},
        {"Camiseta con mas Derrotas", SQL_CAMISETA_POR_RESULTADO("3")},
        {
            "Camiseta mas Sorteada", "SELECT c.nombre, c.sorteada "
            "FROM camiseta c "
            "ORDER BY c.sorteada DESC LIMIT 1"
        }
    };

    size_t total = sizeof(queries) / sizeof(queries[0]);

    int current_changes = sqlite3_total_changes(db);
    int cache_hit = s_stats_valid && (current_changes == s_stats_changes);

    for (size_t i = 0; i < total; i++)
    {
        printf("\n%s\n", queries[i].titulo);
        printf("----------------------------------------\n");
        if (!cache_hit)
            query_to_buf(queries[i].sql, s_stats_cache[i].buf, STATS_GEN_OUTPUT_LEN);
        printf("%s", s_stats_cache[i].buf);
    }

    if (!cache_hit)
    {
        s_stats_valid = 1;
        s_stats_changes = current_changes;
    }

    pause_console();
}

#undef SQL_CAMISETA_POR_RESULTADO
#undef SQL_CAMISETA_AGREGADA

void mostrar_total_partidos_jugados(void)
{
    mostrar_query_simple("TOTAL DE PARTIDOS JUGADOS", "Total de Partidos Jugados",
                         "SELECT COUNT(*) FROM partido");
}

void mostrar_promedio_goles_por_partido(void)
{
    mostrar_query_simple("PROMEDIO DE GOLES POR PARTIDO",
                         "Promedio de Goles por Partido",
                         "SELECT ROUND(AVG(goles), 2) FROM partido");
}

void mostrar_promedio_asistencias_por_partido(void)
{
    mostrar_query_simple("PROMEDIO DE ASISTENCIAS POR PARTIDO",
                         "Promedio de Asistencias por Partido",
                         "SELECT ROUND(AVG(asistencias), 2) FROM partido");
}

void mostrar_promedio_rendimiento_general(void)
{
    mostrar_query_simple(
        "PROMEDIO DE RENDIMIENTO_GENERAL", "Promedio de Rendimiento General",
        "SELECT ROUND(AVG(rendimiento_general), 2) FROM partido");
}

void mostrar_rendimiento_promedio_por_clima(void)
{
    char sql[1024];
    int written =
        snprintf(sql, sizeof(sql),
                 "SELECT %s AS clima_texto, ROUND(AVG(rendimiento_general), 2) "
                 "FROM partido GROUP BY clima ORDER BY clima",
                 get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("RENDIMIENTO PROMEDIO POR CLIMA",
                         "Rendimiento Promedio por Clima", sql);
}

void mostrar_goles_por_clima(void)
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS clima_texto, SUM(goles) FROM partido "
                           "GROUP BY clima ORDER BY clima",
                           get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("GOLES POR CLIMA", "Goles por Clima", sql);
}

void mostrar_asistencias_por_clima(void)
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS clima_texto, SUM(asistencias) FROM "
                           "partido GROUP BY clima ORDER BY clima",
                           get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("ASISTENCIAS POR CLIMA", "Asistencias por Clima", sql);
}

void mostrar_clima_mejor_rendimiento(void)
{
    char sql[1024];
    int written = snprintf(
                      sql, sizeof(sql),
                      "SELECT %s AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM "
                      "partido GROUP BY clima ORDER BY AVG(rendimiento_general) DESC LIMIT 1",
                      get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("CLIMA DONDE SE RINDE MEJOR",
                         "Clima con Mejor Rendimiento Promedio", sql);
}

void mostrar_clima_peor_rendimiento(void)
{
    char sql[1024];
    int written = snprintf(
                      sql, sizeof(sql),
                      "SELECT %s AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM "
                      "partido GROUP BY clima ORDER BY AVG(rendimiento_general) ASC LIMIT 1",
                      get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("CLIMA DONDE SE RINDE PEOR",
                         "Clima con Peor Rendimiento Promedio", sql);
}

void mostrar_mejor_dia_semana(void)
{
    mostrar_por_dia_semana("MEJOR DIA DE LA SEMANA", "rendimiento_general",
                           "DESC", 1);
}

void mostrar_peor_dia_semana(void)
{
    mostrar_por_dia_semana("PEOR DIA DE LA SEMANA", "rendimiento_general", "ASC",
                           1);
}

void mostrar_goles_promedio_por_dia(void)
{
    mostrar_por_dia_semana("GOLES PROMEDIO POR DIA", "goles", "ds.dia_num", 0);
}

void mostrar_asistencias_promedio_por_dia(void)
{
    mostrar_por_dia_semana("ASISTENCIAS PROMEDIO POR DIA", "asistencias",
                           "ds.dia_num", 0);
}

void mostrar_rendimiento_promedio_por_dia(void)
{
    mostrar_por_dia_semana("RENDIMIENTO PROMEDIO POR DIA", "rendimiento_general",
                           "ds.dia_num", 0);
}

void mostrar_rendimiento_por_nivel_cansancio(void)
{
    char sql[1024];
    int written = snprintf(
                      sql, sizeof(sql),
                      "SELECT %s AS nivel_cansancio, ROUND(AVG(rendimiento_general), 2) AS "
                      "rendimiento_promedio, COUNT(*) AS partidos FROM partido GROUP BY %s "
                      "ORDER BY rendimiento_promedio DESC",
                      get_nivel_case_sql("cansancio"), get_nivel_case_sql("cansancio"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("RENDIMIENTO POR NIVEL DE CANSANCIO",
                         "Rendimiento por Nivel de Cansancio", sql);
}

void mostrar_goles_cansancio_alto_vs_bajo(void)
{
    clear_screen();
    print_header("GOLES CON CANSANCIO ALTO VS BAJO");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Goles con Cansancio Alto vs Bajo"));
    printf("----------------------------------------\n");

    // Query modificada para mostrar el formato esperado: Alto: 1, Bajo: 0
    sqlite3_stmt *stmt;
    int num_cols;

    // Consulta para cansancio alto (>7) vs bajo (<=7)
    const char *sql = "SELECT CASE WHEN cansancio > 7 THEN 'Alto' ELSE 'Bajo' "
                      "END AS nivel_cansancio, "
                      "SUM(goles) AS total_goles, ROUND(AVG(goles), 2) AS "
                      "promedio_goles, COUNT(*) AS partidos "
                      "FROM partido GROUP BY CASE WHEN cansancio > 7 THEN 'Alto' "
                      "ELSE 'Bajo' END";

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }
    num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols >= 2)
        {
            const char *nivel = (const char *)sqlite3_column_text(stmt, 0);
            int total_goles = sqlite3_column_int(stmt, 1);
            double promedio_goles = sqlite3_column_double(stmt, 2);

            // Mostrar en el formato especificado en la tarea
            printf("%-30s : %d", remover_tildes(nivel), total_goles);

            // Agregar nota para cansancio bajo si hay caida de rendimiento
            if (strcmp(nivel, "Bajo") == 0 && promedio_goles < 1.0)
            {
                printf(", Caida de Rendimiento por Cansancio Acumulado");
            }
            printf("\n");
        }
    }

    sqlite3_finalize(stmt);

    pause_console();
}

void mostrar_partidos_cansancio_alto(void)
{
    mostrar_query_simple("PARTIDOS JUGADOS CON CANSANCIO ALTO",
                         "Partidos con Cansancio Alto (>7)",
                         "SELECT COUNT(*) AS partidos_cansancio_alto FROM "
                         "partido WHERE cansancio > 7");
}

void mostrar_caida_rendimiento_cansancio_acumulado(void)
{
    clear_screen();
    print_header("CAIDA DE RENDIMIENTO POR CANSANCIO ACUMULADO");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n",
           remover_tildes("Caida de Rendimiento por Cansancio Acumulado"));
    printf("----------------------------------------\n");

    // Comparar rendimiento en partidos recientes vs antiguos con alto cansancio
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT 'Recientes (ultimos 5)' AS periodo, "
        "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio FROM (SELECT "
        "rendimiento_general FROM partido WHERE cansancio > 7 ORDER BY "
        "fecha_hora DESC LIMIT 5) UNION ALL SELECT 'Antiguos (primeros 5)' AS "
        "periodo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio "
        "FROM (SELECT rendimiento_general FROM partido WHERE cansancio > 7 ORDER "
        "BY fecha_hora ASC LIMIT 5)";

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
        double rendimiento = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f", remover_tildes(periodo), rendimiento);
        printf("\n");
    }

    sqlite3_finalize(stmt);

    pause_console();
}

void mostrar_rendimiento_por_estado_animo(void)
{
    char sql[1024];
    int written = snprintf(
                      sql, sizeof(sql),
                      "SELECT %s AS nivel_animo, ROUND(AVG(rendimiento_general), 2) AS "
                      "rendimiento_promedio, COUNT(*) AS partidos FROM partido GROUP BY %s "
                      "ORDER BY rendimiento_promedio DESC",
                      get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("RENDIMIENTO POR ESTADO DE ANIMO",
                         "Rendimiento por Estado de Animo", sql);
}

void mostrar_goles_por_estado_animo(void)
{
    char sql[1024];
    int written = snprintf(
                      sql, sizeof(sql),
                      "SELECT %s AS nivel_animo, SUM(goles) AS total_goles, ROUND(AVG(goles), "
                      "2) AS promedio_goles, COUNT(*) AS partidos FROM partido GROUP BY %s "
                      "ORDER BY promedio_goles DESC",
                      get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("GOLES POR ESTADO DE ANIMO", "Goles por Estado de Animo",
                         sql);
}

void mostrar_asistencias_por_estado_animo(void)
{
    char sql[1024];
    int written = snprintf(
                      sql, sizeof(sql),
                      "SELECT %s AS nivel_animo, SUM(asistencias) AS total_asistencias, "
                      "ROUND(AVG(asistencias), 2) AS promedio_asistencias, COUNT(*) AS "
                      "partidos FROM partido GROUP BY %s ORDER BY promedio_asistencias DESC",
                      get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("ASISTENCIAS POR ESTADO DE ANIMO",
                         "Asistencias por Estado de Animo", sql);
}

void mostrar_estado_animo_ideal(void)
{
    char sql[1024];
    int written = snprintf(
                      sql, sizeof(sql),
                      "SELECT %s AS nivel_animo, ROUND(AVG(rendimiento_general), 2) AS "
                      "rendimiento_promedio FROM partido GROUP BY %s ORDER BY "
                      "rendimiento_promedio DESC LIMIT 1",
                      get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("ESTADO DE ANIMO IDEAL PARA JUGAR",
                         "Estado de Animo Ideal", sql);
}

const char *obtener_dia_semana(int dia, int mes, int anio)
{
    struct tm fecha = {0};
    fecha.tm_mday = dia;
    fecha.tm_mon = mes - 1;      // Meses: 0-11
    fecha.tm_year = anio - 2023; // Anos desde 1900

    mktime(&fecha); // Calcula el dia de la semana

    return dias[fecha.tm_wday];
}
