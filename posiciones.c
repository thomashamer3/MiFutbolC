#include "posiciones.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>

void mostrar_tabla_posiciones_global()
{
    clear_screen();
    print_header("TABLA DE POSICIONES GLOBAL");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT "
                      " COUNT(*) AS total,"
                      " SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS pg,"
                      " SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS pe,"
                      " SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS pp,"
                      " SUM(goles) AS gf,"
                      " SUM(COALESCE(goles_en_contra, 0)) AS gc,"
                      " SUM(goles) - SUM(COALESCE(goles_en_contra, 0)) AS dg,"
                      " SUM(CASE WHEN resultado = 1 THEN 3 WHEN resultado = 2 "
                      "THEN 1 ELSE 0 END) AS pts"
                      " FROM partido WHERE resultado > 0";

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int total = sqlite3_column_int(stmt, 0);
        int pg = sqlite3_column_int(stmt, 1);
        int pe = sqlite3_column_int(stmt, 2);
        int pp = sqlite3_column_int(stmt, 3);
        int gf = sqlite3_column_int(stmt, 4);
        int gc = sqlite3_column_int(stmt, 5);
        int dg = sqlite3_column_int(stmt, 6);
        int pts = sqlite3_column_int(stmt, 7);
        float v_ratio = total > 0 ? (100.0f * pg / total) : 0.0f;

        printf("\n");
        printf("  %-25s %s\n", "Metrica", "Valor");
        printf("  %s\n", "----------------------------------------");
        printf("  %-25s %3d\n", "Partidos Totales", total);
        printf("  %-25s %3d\n", "Victorias (PG)", pg);
        printf("  %-25s %3d\n", "Empates (PE)", pe);
        printf("  %-25s %3d\n", "Derrotas (PP)", pp);
        printf("  %-25s %3d\n", "Goles a Favor (GF)", gf);
        printf("  %-25s %3d\n", "Goles en Contra (GC)", gc);
        printf("  %-25s %+3d\n", "Diferencia (DG)", dg);
        printf("  %-25s %3d\n", "Puntos (Pts)", pts);
        printf("  %-25s %5.1f%%\n", "Tasa de Victoria", v_ratio);
        printf("  %-25s %.2f\n", "Promedio Goles/Partido",
               total > 0 ? (double)gf / total : 0.0);
        printf("  %-25s %.2f\n", "Promedio Goles en Contra",
               total > 0 ? (double)gc / total : 0.0);
        hay = 1;
    }
    sqlite3_finalize(stmt);

    if (!hay)
    {
        printf("\n  No hay partidos registrados.\n");
        pause_console();
        return;
    }

    // Desglose por anio
    printf("\n\n  %s\n", "DESGLOSE POR ANIO");
    printf("  %s\n", "----------------------------------------");
    printf("  %4s | %3s | %3s | %3s | %3s | %3s | %3s | %4s | %s\n", "Anio", "PJ",
           "PG", "PE", "PP", "GF", "GC", "DG", "V%");
    printf("  %s\n", "-----+-----+-----+-----+-----+-----+-----+------+------");

    const char *sql_anio =
        "SELECT CAST(SUBSTR(fecha_hora, 7, 4) AS INTEGER) AS anio,"
        " COUNT(*) AS total,"
        " SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS pg,"
        " SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS pe,"
        " SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS pp,"
        " SUM(goles) AS gf,"
        " SUM(COALESCE(goles_en_contra, 0)) AS gc,"
        " SUM(goles) - SUM(COALESCE(goles_en_contra, 0)) AS dg"
        " FROM partido WHERE resultado > 0"
        " GROUP BY anio ORDER BY anio DESC";

    if (!preparar_stmt_export(&stmt, sql_anio))
    {
        printf("Error al consultar desglose.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int anio = sqlite3_column_int(stmt, 0);
        int t = sqlite3_column_int(stmt, 1);
        int pg2 = sqlite3_column_int(stmt, 2);
        int pe2 = sqlite3_column_int(stmt, 3);
        int pp2 = sqlite3_column_int(stmt, 4);
        int gf2 = sqlite3_column_int(stmt, 5);
        int gc2 = sqlite3_column_int(stmt, 6);
        int dg2 = sqlite3_column_int(stmt, 7);
        float vp = t > 0 ? (100.0f * pg2 / t) : 0.0f;
        printf("  %4d | %3d | %3d | %3d | %3d | %3d | %3d | %+4d | %4.0f%%\n", anio,
               t, pg2, pe2, pp2, gf2, gc2, dg2, vp);
    }
    sqlite3_finalize(stmt);

    pause_console();
}
