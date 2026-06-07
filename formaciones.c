#include "formaciones.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "ascii_charts.h"
#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RES_VICTORIA 1
#define RES_EMPATE   2
#define RES_DERROTA  3

static int preparar_stmt_formaciones(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static const char *formato_a_nombre_corto(const char *formato)
{
    if (!formato || formato[0] == '\0') return "N/A";
    if (strcmp(formato, "Futbol 5") == 0)  return "F5";
    if (strcmp(formato, "Futbol 7") == 0)  return "F7";
    if (strcmp(formato, "Futbol 8") == 0)  return "F8";
    if (strcmp(formato, "Futbol 11") == 0) return "F11";
    return formato;
}

static int formaciones_tienen_datos()
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_formaciones(&stmt,
                                   "SELECT COUNT(*) FROM partido WHERE formato_partido IS NOT NULL AND formato_partido != ''"))
        return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
}

void mostrar_efectividad_por_formacion()
{
    mostrar_pantalla("EFECTIVIDAD POR FORMACION");

    if (!formaciones_tienen_datos())
    {
        mostrar_no_hay_registros("partidos con formacion");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_formaciones(&stmt,
                                   "SELECT p.formato_partido, "
                                   "  COUNT(*), "
                                   "  SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END), "
                                   "  SUM(CASE WHEN p.resultado = 2 THEN 1 ELSE 0 END), "
                                   "  SUM(CASE WHEN p.resultado = 3 THEN 1 ELSE 0 END), "
                                   "  AVG(CAST(p.goles AS REAL)), "
                                   "  AVG(CAST(p.rendimiento_general AS REAL)) "
                                   "FROM partido p "
                                   "WHERE p.formato_partido IS NOT NULL AND p.formato_partido != '' "
                                   "GROUP BY p.formato_partido "
                                   "ORDER BY p.formato_partido"))
    {
        mostrar_error_operacion("Formaciones", "consultar");
        pause_console();
        return;
    }

    ui_printf("%-12s %6s %6s %6s %6s %8s %10s %10s\n",
              "Formacion", "PJ", "V", "E", "D", "V%", "Goles", "Rend.");

    double valores[4];
    const char *etiquetas[4];
    int idx = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *formato = (const char *)sqlite3_column_text(stmt, 0);
        int total    = sqlite3_column_int(stmt, 1);
        int v        = sqlite3_column_int(stmt, 2);
        int e        = sqlite3_column_int(stmt, 3);
        int d        = sqlite3_column_int(stmt, 4);
        double goles = sqlite3_column_double(stmt, 5);
        double rend  = sqlite3_column_double(stmt, 6);
        double pct_v = total > 0 ? (v * 100.0 / total) : 0.0;

        ui_printf("%-12s %6d %6d %6d %6d %7.1f%% %8.2f %10.2f\n",
                  formato, total, v, e, d, pct_v, goles, rend);

        if (idx < 4)
        {
            valores[idx] = pct_v;
            etiquetas[idx] = formato_a_nombre_corto(formato);
            idx++;
        }
    }
    sqlite3_finalize(stmt);

    if (idx > 0)
        dibujar_grafico_barras(valores, etiquetas, idx,
                               "% Victorias por Formacion", 20);

    pause_console();
}

void mostrar_mejor_formacion_por_cancha(int cancha_id)
{
    char titulo[128];
    snprintf(titulo, sizeof(titulo), "MEJOR FORMACION - CANCHA ID %d", cancha_id);
    mostrar_pantalla(titulo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt_formaciones(&stmt,
                                   "SELECT p.formato_partido, "
                                   "  COUNT(*), "
                                   "  SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END), "
                                   "  SUM(CASE WHEN p.resultado = 2 THEN 1 ELSE 0 END), "
                                   "  SUM(CASE WHEN p.resultado = 3 THEN 1 ELSE 0 END), "
                                   "  AVG(CAST(p.goles AS REAL)), "
                                   "  AVG(CAST(p.rendimiento_general AS REAL)) "
                                   "FROM partido p "
                                   "WHERE p.cancha_id = ? "
                                   "  AND p.formato_partido IS NOT NULL AND p.formato_partido != '' "
                                   "GROUP BY p.formato_partido "
                                   "ORDER BY SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END) * 1.0 / COUNT(*) DESC"))
    {
        mostrar_error_operacion("Formaciones", "consultar");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, cancha_id);

    int filas = 0;
    double mejor_pct = -1.0;
    const char *mejor_formacion = NULL;

    ui_printf("%-12s %6s %6s %6s %6s %8s %10s %10s\n",
              "Formacion", "PJ", "V", "E", "D", "V%", "Goles", "Rend.");

    double valores[4];
    const char *etiquetas[4];
    int idx = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *formato = (const char *)sqlite3_column_text(stmt, 0);
        int total    = sqlite3_column_int(stmt, 1);
        int v        = sqlite3_column_int(stmt, 2);
        int e        = sqlite3_column_int(stmt, 3);
        int d        = sqlite3_column_int(stmt, 4);
        double goles = sqlite3_column_double(stmt, 5);
        double rend  = sqlite3_column_double(stmt, 6);
        double pct_v = total > 0 ? (v * 100.0 / total) : 0.0;

        ui_printf("%-12s %6d %6d %6d %6d %7.1f%% %8.2f %10.2f\n",
                  formato, total, v, e, d, pct_v, goles, rend);

        if (total > 0 && pct_v > mejor_pct)
        {
            mejor_pct = pct_v;
            mejor_formacion = formato;
        }

        if (idx < 4)
        {
            valores[idx] = pct_v;
            etiquetas[idx] = formato_a_nombre_corto(formato);
            idx++;
        }
        filas++;
    }
    sqlite3_finalize(stmt);

    if (filas == 0)
    {
        mostrar_no_hay_registros("partidos en esta cancha con formacion");
        pause_console();
        return;
    }

    if (mejor_formacion)
    {
        ui_printf("\nMejor formacion para esta cancha: %s (%.1f%% victorias)\n",
                  mejor_formacion, mejor_pct);
    }

    if (idx > 0)
        dibujar_grafico_barras(valores, etiquetas, idx,
                               "% Victorias por Formacion", 20);

    pause_console();
}

void mostrar_tendencia_formaciones()
{
    mostrar_pantalla("TENDENCIA DE FORMACIONES");

    if (!formaciones_tienen_datos())
    {
        mostrar_no_hay_registros("partidos con formacion");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_formaciones(&stmt,
                                   "SELECT SUBSTR(p.fecha_hora, 1, 7) AS mes, "
                                   "  p.formato_partido, "
                                   "  COUNT(*), "
                                   "  AVG(CAST(p.rendimiento_general AS REAL)), "
                                   "  AVG(CAST(p.goles AS REAL)) "
                                   "FROM partido p "
                                   "WHERE p.formato_partido IS NOT NULL AND p.formato_partido != '' "
                                   "  AND p.fecha_hora IS NOT NULL "
                                   "GROUP BY mes, p.formato_partido "
                                   "ORDER BY mes ASC, p.formato_partido"))
    {
        mostrar_error_operacion("Formaciones", "consultar tendencia");
        pause_console();
        return;
    }

    ui_printf("%-10s %-12s %6s %10s %10s\n",
              "Mes", "Formacion", "PJ", "Rend.", "Gol Prom");

    double valores_trend[24];
    char *labels_trend[24];
    int trend_count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes     = (const char *)sqlite3_column_text(stmt, 0);
        const char *formato = (const char *)sqlite3_column_text(stmt, 1);
        int total    = sqlite3_column_int(stmt, 2);
        double rend  = sqlite3_column_double(stmt, 3);
        double goles = sqlite3_column_double(stmt, 4);

        ui_printf("%-10s %-12s %6d %10.2f %10.2f\n",
                  mes, formato, total, rend, goles);

        if (trend_count < 24)
        {
            char *label = malloc(32);
            if (label)
            {
                snprintf(label, 32, "%s-%s", mes, formato_a_nombre_corto(formato));
                valores_trend[trend_count] = rend;
                labels_trend[trend_count] = label;
                trend_count++;
            }
        }
    }
    sqlite3_finalize(stmt);

    if (trend_count > 0)
    {
        dibujar_grafico_barras(valores_trend, (const char **)labels_trend, trend_count > 8 ? 8 : trend_count,
                               "Rendimiento por Mes-Formacion (max 8)", 20);
    }

    for (int i = 0; i < trend_count; i++)
    {
        free(labels_trend[i]);
    }

    pause_console();
}

void exportar_analisis_formaciones_csv()
{
    if (!formaciones_tienen_datos())
    {
        mostrar_no_hay_registros("partidos con formacion");
        pause_console();
        return;
    }

    const char *filename = "analisis_formaciones.csv";
    FILE *f = NULL;
#ifdef _WIN32
    fopen_s(&f, get_export_path(filename), "w");
#else
    f = fopen(get_export_path(filename), "w");
#endif

    if (!f)
    {
        mostrar_error_operacion("Archivo CSV", "crear");
        pause_console();
        return;
    }

    fprintf(f, "Formacion,Total,Victorias,Empates,Derrotas,PorcentajeVictorias,PromedioGoles,PromedioRendimiento\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt_formaciones(&stmt,
                                   "SELECT p.formato_partido, "
                                   "  COUNT(*), "
                                   "  SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END), "
                                   "  SUM(CASE WHEN p.resultado = 2 THEN 1 ELSE 0 END), "
                                   "  SUM(CASE WHEN p.resultado = 3 THEN 1 ELSE 0 END), "
                                   "  AVG(CAST(p.goles AS REAL)), "
                                   "  AVG(CAST(p.rendimiento_general AS REAL)) "
                                   "FROM partido p "
                                   "WHERE p.formato_partido IS NOT NULL AND p.formato_partido != '' "
                                   "GROUP BY p.formato_partido "
                                   "ORDER BY p.formato_partido"))
    {
        fclose(f);
        mostrar_error_operacion("Formaciones", "consultar");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *formato = (const char *)sqlite3_column_text(stmt, 0);
        int total    = sqlite3_column_int(stmt, 1);
        int v        = sqlite3_column_int(stmt, 2);
        int e        = sqlite3_column_int(stmt, 3);
        int d        = sqlite3_column_int(stmt, 4);
        double goles = sqlite3_column_double(stmt, 5);
        double rend  = sqlite3_column_double(stmt, 6);
        double pct_v = total > 0 ? (v * 100.0 / total) : 0.0;

        fprintf(f, "%s,%d,%d,%d,%d,%.1f,%.2f,%.2f\n",
                formato ? formato : "", total, v, e, d, pct_v, goles, rend);
    }
    sqlite3_finalize(stmt);
    fclose(f);

    ui_printf("Analisis exportado a: %s\n", get_export_path(filename));
    pause_console();
}

void menu_analisis_formaciones()
{
    int opcion;
    do
    {
        mostrar_pantalla("ANALISIS DE FORMACIONES");
        ui_printf("1. Efectividad por Formacion\n");
        ui_printf("2. Mejor Formacion por Cancha\n");
        ui_printf("3. Tendencia de Formaciones\n");
        ui_printf("4. Exportar Analisis a CSV\n");
        ui_printf("0. Volver\n");
        ui_printf("Seleccione una opcion: ");

        opcion = input_int("");

        switch (opcion)
        {
        case 1:
            mostrar_efectividad_por_formacion();
            break;
        case 2:
        {
            if (!hay_registros("cancha"))
            {
                mostrar_no_hay_registros("canchas");
                pause_console();
                break;
            }
            printf("\nCanchas disponibles:\n");
            listar_entidades("cancha", "ID - Nombre", "ninguna");
            printf("\n");
            int cancha_id = input_int("Ingrese el ID de la cancha: ");
            if (existe_id("cancha", cancha_id))
                mostrar_mejor_formacion_por_cancha(cancha_id);
            else
                mostrar_no_existe("Cancha");
            pause_console();
            break;
        }
        case 3:
            mostrar_tendencia_formaciones();
            break;
        case 4:
            exportar_analisis_formaciones_csv();
            break;
        case 0:
            break;
        default:
            ui_printf("Opcion invalida.\n");
            pause_console();
            break;
        }
    }
    while (opcion != 0);
}
