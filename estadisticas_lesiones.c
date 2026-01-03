#include "estadisticas_lesiones.h"
#include "db.h"
#include "utils.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Muestra el total de lesiones
 */
static void mostrar_total_lesiones()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Total de lesiones: %d\n", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra lesiones por tipo
 */
static void mostrar_lesiones_por_tipo()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT tipo, COUNT(*) FROM lesion GROUP BY tipo ORDER BY COUNT(*) DESC", -1, &stmt, NULL);
    printf("Lesiones por tipo:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("  %s: %d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra lesiones por camiseta
 */
static void mostrar_lesiones_por_camiseta()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.numero, c.nombre, COUNT(l.id) FROM camiseta c LEFT JOIN lesion l ON c.id = l.camiseta_id GROUP BY c.id ORDER BY COUNT(l.id) DESC",
                       -1, &stmt, NULL);
    printf("Lesiones por camiseta:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("  Camiseta %d (%s): %d\n", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra lesiones por mes
 */
static void mostrar_lesiones_por_mes()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT strftime('%m', substr(fecha,7,4)||'-'||substr(fecha,4,2)||'-'||substr(fecha,1,2)) as mes, COUNT(*) FROM lesion GROUP BY mes ORDER BY COUNT(*) DESC", -1, &stmt, NULL);
    printf("Lesiones por mes:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* mes_str = (const char*)sqlite3_column_text(stmt, 0);
        int mes = mes_str ? atoi(mes_str) : 0;
        const char* nombre_mes[] = {"", "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
        const char* nombre = (mes >= 1 && mes <= 12) ? nombre_mes[mes] : "Desconocido";
        printf("%s (%d)\n", nombre, sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra el mes con más lesiones
 */
static void mostrar_mes_con_mas_lesiones()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT strftime('%m', substr(fecha,7,4)||'-'||substr(fecha,4,2)||'-'||substr(fecha,1,2)) as mes, COUNT(*) FROM lesion GROUP BY mes ORDER BY COUNT(*) DESC LIMIT 1", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int mes = atoi((const char*)sqlite3_column_text(stmt, 0));
        const char* nombre_mes[] = {"", "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
        printf("Mes Con mas Lesiones:%s\n", nombre_mes[mes]);
    }
    else
    {
        printf("Ninguno\n");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra el tiempo promedio entre lesiones
 */
static void mostrar_tiempo_promedio_entre_lesiones()
{
    // Obtener todas las fechas de lesiones ordenadas por camiseta y fecha
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT camiseta_id, fecha FROM lesion ORDER BY camiseta_id, fecha", -1, &stmt, NULL);

    double total_dias = 0;
    int count = 0;
    int prev_camiseta = -1;
    double prev_jd = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int camiseta = sqlite3_column_int(stmt, 0);
        const char* fecha = (const char*)sqlite3_column_text(stmt, 1);
        double jd = 0;
        // Calcular día juliano
        sqlite3_stmt *stmt_jd;
        sqlite3_prepare_v2(db, "SELECT julianday(?)", -1, &stmt_jd, NULL);
        sqlite3_bind_text(stmt_jd, 1, fecha, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt_jd) == SQLITE_ROW)
        {
            jd = sqlite3_column_double(stmt_jd, 0);
        }
        sqlite3_finalize(stmt_jd);

        if (camiseta == prev_camiseta && prev_jd > 0)
        {
            total_dias += jd - prev_jd;
            count++;
        }
        prev_camiseta = camiseta;
        prev_jd = jd;
    }
    sqlite3_finalize(stmt);

    if (count > 0)
    {
        printf("Tiempo promedio entre lesiones: %.1f dias\n", total_dias / count);
    }
    else
    {
        printf("Tiempo promedio entre lesiones: N/A (menos de 2 lesiones)\n");
    }
}

/**
 * @brief Calcula rendimiento promedio antes de lesiones
 */
static double calcular_rendimiento_promedio_antes()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT AVG(p.goles + p.asistencias) FROM partido p JOIN lesion l ON p.camiseta_id = l.camiseta_id WHERE p.fecha < l.fecha",
                       -1, &stmt, NULL);
    double avg = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        avg = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return avg;
}

/**
 * @brief Calcula rendimiento promedio después de lesiones
 */
static double calcular_rendimiento_promedio_despues()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT AVG(p.goles + p.asistencias) FROM partido p JOIN lesion l ON p.camiseta_id = l.camiseta_id WHERE p.fecha > l.fecha",
                       -1, &stmt, NULL);
    double avg = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        avg = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return avg;
}

/**
 * @brief Muestra rendimiento promedio antes y después de lesiones
 */
static void mostrar_rendimiento_promedio()
{
    double antes = calcular_rendimiento_promedio_antes();
    double despues = calcular_rendimiento_promedio_despues();
    printf("Rendimiento promedio antes de una lesion: %.2f\n", antes);
    printf("Rendimiento promedio despues de una lesion: %.2f\n", despues);
    printf("Baja el rendimiento previo a una lesion? %s\n", (antes < despues) ? "Sí" : "No");
}

/**
 * @brief Muestra todas las estadísticas de lesiones
 */
void mostrar_estadisticas_lesiones()
{
    clear_screen();
    print_header("ESTADISTICAS DE LESIONES");

    mostrar_total_lesiones();
    printf("\n");
    mostrar_lesiones_por_tipo();
    printf("\n");
    mostrar_lesiones_por_camiseta();
    printf("\n");
    mostrar_lesiones_por_mes();
    printf("\n");
    mostrar_mes_con_mas_lesiones();
    printf("\n");
    mostrar_tiempo_promedio_entre_lesiones();
    printf("\n");
    mostrar_rendimiento_promedio();

    pause_console();
}
