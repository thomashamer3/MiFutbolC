#include "estadisticas_canchas.h"
#include "cancha.h"
#include "db.h"
#include "menu.h"
#include "sqlite3.h"
#include "utils.h"
#include <stdio.h>

static const char *cancha_stats_texto(sqlite3_stmt *stmt, int col)
{
    const unsigned char *value = sqlite3_column_text(stmt, col);
    return value ? (const char *)value : "";
}

static int cancha_stats_abrir(sqlite3_stmt **stmt, const char *sql)
{
    if (!db_prepare_stmt(stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        return 0;
    }
    return 1;
}

void mostrar_resumen_canchas(void)
{
    clear_screen();
    print_header("RESUMEN DE CANCHAS");

    sqlite3_stmt *stmt = NULL;
    if (cancha_stats_abrir(&stmt,
                           "SELECT COUNT(*), "
                           "IFNULL(SUM(CASE WHEN IFNULL(activa, 1) = 1 THEN 1 ELSE 0 END), 0), "
                           "IFNULL(SUM(CASE WHEN IFNULL(activa, 1) = 0 THEN 1 ELSE 0 END), 0), "
                           "IFNULL(SUM(CASE WHEN IFNULL(cantidad_canchas, 1) > 0 "
                           "THEN cantidad_canchas ELSE 1 END), 0) "
                           "FROM cancha"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("Canchas registradas : %d\n", sqlite3_column_int(stmt, 0));
            printf("Canchas activas     : %d\n", sqlite3_column_int(stmt, 1));
            printf("Canchas inactivas   : %d\n", sqlite3_column_int(stmt, 2));
            printf("Canchas fisicas     : %d\n", sqlite3_column_int(stmt, 3));
        }
        db_stmt_release(stmt);
    }

    stmt = NULL;
    if (cancha_stats_abrir(&stmt, "SELECT COUNT(*) FROM partido"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("Partidos registrados: %d\n", sqlite3_column_int(stmt, 0));
        }
        db_stmt_release(stmt);
    }

    stmt = NULL;
    if (cancha_stats_abrir(&stmt,
                           "SELECT "
                           "IFNULL(SUM(CASE WHEN EXISTS (SELECT 1 FROM partido p "
                           "WHERE p.cancha_id = c.id) THEN 1 ELSE 0 END), 0), "
                           "IFNULL(SUM(CASE WHEN NOT EXISTS (SELECT 1 FROM partido p "
                           "WHERE p.cancha_id = c.id) THEN 1 ELSE 0 END), 0) "
                           "FROM cancha c"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("Canchas con uso     : %d\n", sqlite3_column_int(stmt, 0));
            printf("Canchas sin uso     : %d\n", sqlite3_column_int(stmt, 1));
        }
        db_stmt_release(stmt);
    }

    pause_console();
}

void mostrar_ranking_canchas_mas_usadas(void)
{
    clear_screen();
    print_header("CANCHAS MAS USADAS");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT c.id, c.nombre, COUNT(p.id) AS partidos, "
                            "IFNULL(SUM(p.goles), 0), IFNULL(SUM(p.asistencias), 0), "
                            "ROUND(IFNULL(AVG(p.rendimiento_general), 0), 2) "
                            "FROM cancha c JOIN partido p ON p.cancha_id = c.id "
                            "GROUP BY c.id, c.nombre "
                            "ORDER BY partidos DESC, c.nombre ASC LIMIT 10"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%2d - %-22s Partidos: %3d | Goles: %3d | Asist: %3d | Rend: %.2f\n",
               sqlite3_column_int(stmt, 0), cancha_stats_texto(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4), sqlite3_column_double(stmt, 5));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("partidos con cancha asignada");
    }

    pause_console();
}

void mostrar_canchas_menos_usadas(void)
{
    clear_screen();
    print_header("CANCHAS MENOS USADAS");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT c.id, c.nombre, COUNT(p.id) AS partidos, "
                            "IFNULL(SUM(p.goles), 0) "
                            "FROM cancha c LEFT JOIN partido p ON p.cancha_id = c.id "
                            "GROUP BY c.id, c.nombre "
                            "ORDER BY partidos ASC, c.nombre ASC LIMIT 10"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%2d - %-22s Partidos: %3d | Goles: %3d\n", sqlite3_column_int(stmt, 0),
               cancha_stats_texto(stmt, 1), sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("canchas");
    }

    pause_console();
}

void mostrar_rendimiento_por_cancha(void)
{
    clear_screen();
    print_header("RENDIMIENTO PROMEDIO POR CANCHA");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT c.id, c.nombre, COUNT(p.id), "
                            "ROUND(AVG(p.rendimiento_general), 2) "
                            "FROM cancha c JOIN partido p ON p.cancha_id = c.id "
                            "WHERE p.rendimiento_general IS NOT NULL "
                            "GROUP BY c.id, c.nombre "
                            "ORDER BY AVG(p.rendimiento_general) DESC, c.nombre ASC"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%2d - %-22s Partidos: %3d | Rendimiento promedio: %.2f\n",
               sqlite3_column_int(stmt, 0), cancha_stats_texto(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 3));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("partidos con rendimiento cargado");
    }

    pause_console();
}

void mostrar_goles_asistencias_por_cancha(void)
{
    clear_screen();
    print_header("GOLES Y ASISTENCIAS POR CANCHA");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT c.id, c.nombre, COUNT(p.id), IFNULL(SUM(p.goles), 0), "
                            "IFNULL(SUM(p.asistencias), 0), "
                            "ROUND(CASE WHEN COUNT(p.id) > 0 "
                            "THEN (IFNULL(SUM(p.goles), 0) + IFNULL(SUM(p.asistencias), 0)) * 1.0 "
                            "/ COUNT(p.id) ELSE 0 END, 2) "
                            "FROM cancha c LEFT JOIN partido p ON p.cancha_id = c.id "
                            "GROUP BY c.id, c.nombre "
                            "ORDER BY (IFNULL(SUM(p.goles), 0) + IFNULL(SUM(p.asistencias), 0)) DESC"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%2d - %-22s Partidos: %3d | Goles: %3d | Asist: %3d | Prom: %.2f\n",
               sqlite3_column_int(stmt, 0), cancha_stats_texto(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4), sqlite3_column_double(stmt, 5));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("canchas");
    }

    pause_console();
}

void mostrar_rendimiento_por_tipo_cancha(void)
{
    clear_screen();
    print_header("RENDIMIENTO POR TIPO DE CANCHA");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT c.tipo_cancha_codigo, COUNT(DISTINCT c.id), COUNT(p.id), "
                            "ROUND(IFNULL(AVG(p.rendimiento_general), 0), 2) "
                            "FROM cancha c LEFT JOIN partido p ON p.cancha_id = c.id "
                            "GROUP BY c.tipo_cancha_codigo "
                            "ORDER BY AVG(p.rendimiento_general) DESC"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int codigo = sqlite3_column_int(stmt, 0);
        printf("%-22s Canchas: %3d | Partidos: %3d | Rend: %.2f\n",
               cancha_texto_tipo(codigo), sqlite3_column_int(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 3));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("canchas");
    }

    pause_console();
}

void mostrar_rendimiento_por_superficie(void)
{
    clear_screen();
    print_header("RENDIMIENTO POR SUPERFICIE");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT c.superficie_codigo, COUNT(DISTINCT c.id), COUNT(p.id), "
                            "ROUND(IFNULL(AVG(p.rendimiento_general), 0), 2) "
                            "FROM cancha c LEFT JOIN partido p ON p.cancha_id = c.id "
                            "GROUP BY c.superficie_codigo "
                            "ORDER BY AVG(p.rendimiento_general) DESC"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int codigo = sqlite3_column_int(stmt, 0);
        printf("%-22s Canchas: %3d | Partidos: %3d | Rend: %.2f\n",
               cancha_texto_superficie(codigo), sqlite3_column_int(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 3));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("canchas");
    }

    pause_console();
}

void mostrar_canchas_techadas_vs_descubiertas(void)
{
    clear_screen();
    print_header("TECHADAS VS DESCUBIERTAS");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT c.techada_estado_codigo, COUNT(DISTINCT c.id), COUNT(p.id), "
                            "ROUND(IFNULL(AVG(p.rendimiento_general), 0), 2) "
                            "FROM cancha c LEFT JOIN partido p ON p.cancha_id = c.id "
                            "GROUP BY c.techada_estado_codigo "
                            "ORDER BY COUNT(p.id) DESC"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int codigo = sqlite3_column_int(stmt, 0);
        printf("Techada: %-8s Canchas: %3d | Partidos: %3d | Rend: %.2f\n",
               cancha_texto_techada(codigo), sqlite3_column_int(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 3));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("canchas");
    }

    pause_console();
}

void mostrar_partidos_por_localidad(void)
{
    clear_screen();
    print_header("PARTIDOS POR LOCALIDAD");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT IFNULL(NULLIF(c.localidad, ''), '(sin localidad)') AS zona, "
                            "COUNT(DISTINCT c.id), COUNT(p.id) "
                            "FROM cancha c LEFT JOIN partido p ON p.cancha_id = c.id "
                            "GROUP BY zona "
                            "ORDER BY COUNT(p.id) DESC"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%-24s Canchas: %3d | Partidos: %3d\n", cancha_stats_texto(stmt, 0),
               sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("canchas");
    }

    pause_console();
}

void mostrar_partidos_por_franja_horaria(void)
{
    clear_screen();
    print_header("PARTIDOS POR FRANJA HORARIA");

    sqlite3_stmt *stmt = NULL;
    if (!cancha_stats_abrir(&stmt,
                            "SELECT substr(fecha_hora, 12, 2) AS hora, COUNT(*) "
                            "FROM partido "
                            "WHERE length(fecha_hora) >= 13 AND substr(fecha_hora, 12, 2) != '' "
                            "GROUP BY hora ORDER BY hora ASC"))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *hora = cancha_stats_texto(stmt, 0);
        printf("%s:00 h | Partidos: %d\n", hora, sqlite3_column_int(stmt, 1));
        hay = 1;
    }
    db_stmt_release(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("partidos con fecha valida");
    }

    pause_console();
}

void menu_estadisticas_canchas(void)
{
    MenuItem items[] = {{1, "Resumen General", &mostrar_resumen_canchas},
        {2, "Canchas Mas Usadas", &mostrar_ranking_canchas_mas_usadas},
        {3, "Canchas Menos Usadas", &mostrar_canchas_menos_usadas},
        {4, "Rendimiento por Cancha", &mostrar_rendimiento_por_cancha},
        {5, "Goles y Asistencias por Cancha", &mostrar_goles_asistencias_por_cancha},
        {6, "Rendimiento por Tipo", &mostrar_rendimiento_por_tipo_cancha},
        {7, "Rendimiento por Superficie", &mostrar_rendimiento_por_superficie},
        {8, "Techadas vs Descubiertas", &mostrar_canchas_techadas_vs_descubiertas},
        {9, "Partidos por Localidad", &mostrar_partidos_por_localidad},
        {10, "Partidos por Franja Horaria", &mostrar_partidos_por_franja_horaria},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ESTADISTICAS DE CANCHAS", items, 11);
}
