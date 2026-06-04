#include "reportes.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "ascii_charts.h"
#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static void configurar_reporte_automatico(void)
{
    char nombre[256], desc[1024], periodicidad[64];
    int habilitado;

    input_string("Nombre del reporte: ", nombre, sizeof(nombre));
    input_string("Descripcion: ", desc, sizeof(desc));
    input_string("Periodicidad (diario/semanal/mensual): ", periodicidad, sizeof(periodicidad));
    habilitado = input_int("Habilitado? (1=Si, 0=No): ");

    sqlite3_stmt *stmt;
    if (!preparar_stmt("INSERT INTO reporte_config (nombre, descripcion, periodicidad, habilitado) VALUES (?,?,?,?)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, periodicidad, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, habilitado);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Reporte configurado.\n");
    else
        printf("Error al configurar reporte: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
}

static void listar_reportes_config(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, nombre, descripcion, periodicidad, habilitado FROM reporte_config ORDER BY id", &stmt))
    {
        mostrar_no_hay_registros("reportes configurados");
        return;
    }

    mostrar_pantalla("REPORTES CONFIGURADOS");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. %s [%s] %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 3),
               sqlite3_column_int(stmt, 4) ? "ACTIVO" : "INACTIVO");
        printf("     %s\n", sqlite3_column_text(stmt, 2));
        printf("     ------------------------------\n");
    }
    sqlite3_finalize(stmt);
    if (count == 0) mostrar_no_hay_registros("reportes configurados");
    pause_console();
}

static void toggle_reporte(int id, int habilitado)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE reporte_config SET habilitado = ? WHERE id = ?", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, habilitado);
    sqlite3_bind_int(stmt, 2, id);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf(habilitado ? "Reporte activado.\n" : "Reporte desactivado.\n");
    else
        printf("Error al actualizar reporte: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
}

static void habilitar_reporte(void)
{
    int id = input_int("ID del reporte: ");
    toggle_reporte(id, 1);
}

static void deshabilitar_reporte(void)
{
    int id = input_int("ID del reporte: ");
    toggle_reporte(id, 0);
}

static void generar_reporte_general_html(void)
{
    char filename[256];
    char path[1024];
    sqlite3_stmt *stmt;

    snprintf(filename, sizeof(filename), "reporte_general_%ld.html", (long)time(NULL));
    char *base_path = get_export_path(filename);
    strcpy_s(path, sizeof(path), base_path);

    FILE *f = NULL;
    if (fopen_s(&f, path, "w") != 0 || !f)
    {
        printf("Error al crear archivo: %s\n", path);
        return;
    }

    fprintf(f, "<html><head><meta charset='UTF-8'>"
            "<title>Reporte General MiFutbolC</title>"
            "<style>body{font-family:Arial;margin:20px}"
            "table{border-collapse:collapse;width:100%%;margin:10px 0}"
            "th,td{border:1px solid #ccc;padding:8px;text-align:left}"
            "th{background:#4CAF50;color:white}"
            "h2{color:#333}</style></head><body>");
    fprintf(f, "<h1>Reporte General MiFutbolC</h1>");

    time_t t = time(NULL);
    struct tm tm_struct;
    localtime_s(&tm_struct, &t);
    char fecha_str[64];
    strftime(fecha_str, sizeof(fecha_str), "%d/%m/%Y %H:%M", &tm_struct);
    fprintf(f, "<p>Generado: %s</p>", fecha_str);

    if (preparar_stmt("SELECT COUNT(*) FROM partido", &stmt))
    {
        int total = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;
        sqlite3_finalize(stmt);
        fprintf(f, "<h2>Partidos</h2><p>Total: %d</p>", total);
    }

    if (preparar_stmt("SELECT resultado, COUNT(*) FROM partido WHERE resultado > 0 GROUP BY resultado", &stmt))
    {
        fprintf(f, "<h2>Resultados</h2><table><tr><th>Resultado</th><th>Cantidad</th></tr>");
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *r = resultado_to_text(sqlite3_column_int(stmt, 0));
            fprintf(f, "<tr><td>%s</td><td>%d</td></tr>", r, sqlite3_column_int(stmt, 1));
        }
        sqlite3_finalize(stmt);
        fprintf(f, "</table>");
    }

    if (preparar_stmt("SELECT COALESCE(SUM(goles),0), COALESCE(SUM(asistencias),0) FROM partido", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(f, "<h2>Goles y Asistencias</h2>");
            fprintf(f, "<p>Goles: %d | Asistencias: %d</p>",
                    sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }

    if (preparar_stmt("SELECT COUNT(*) FROM torneo", &stmt))
    {
        int total = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;
        sqlite3_finalize(stmt);
        fprintf(f, "<h2>Torneos</h2><p>Total: %d</p>", total);
    }

    if (preparar_stmt("SELECT COUNT(*) FROM lesion WHERE fecha >= date('now', '-30 days')", &stmt))
    {
        int recientes = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;
        sqlite3_finalize(stmt);
        fprintf(f, "<h2>Lesiones (ultimos 30 dias)</h2><p>Total: %d</p>", recientes);
    }

    fprintf(f, "</body></html>");
    fclose(f);
    printf("Reporte generado: %s\n", path);

    if (preparar_stmt("INSERT INTO reporte_generado (nombre, archivo) VALUES (?,?)", &stmt))
    {
        sqlite3_bind_text(stmt, 1, "Reporte General HTML", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    app_log_event("REPORTES", "Reporte general generado");
    pause_console();
}

static void generar_reporte_rendimiento_txt(void)
{
    char filename[256];
    char path[1024];
    sqlite3_stmt *stmt;

    snprintf(filename, sizeof(filename), "rendimiento_%ld.txt", (long)time(NULL));
    char *base_path = get_export_path(filename);
    strcpy_s(path, sizeof(path), base_path);

    FILE *f = NULL;
    if (fopen_s(&f, path, "w") != 0 || !f)
    {
        printf("Error al crear archivo: %s\n", path);
        return;
    }

    fprintf(f, "============================================\n");
    fprintf(f, "  REPORTE DE RENDIMIENTO\n");

    time_t t = time(NULL);
    struct tm tm_struct;
    localtime_s(&tm_struct, &t);
    char fecha_str[64];
    strftime(fecha_str, sizeof(fecha_str), "%d/%m/%Y %H:%M", &tm_struct);
    fprintf(f, "  %s\n", fecha_str);
    fprintf(f, "============================================\n\n");

    if (preparar_stmt("SELECT substr(fecha_hora,1,7) as mes, "
                      "COUNT(*), SUM(CASE WHEN resultado=1 THEN 1 ELSE 0 END), "
                      "SUM(CASE WHEN resultado=2 THEN 1 ELSE 0 END), "
                      "SUM(CASE WHEN resultado=3 THEN 1 ELSE 0 END) "
                      "FROM partido WHERE resultado > 0 "
                      "GROUP BY mes ORDER BY mes DESC LIMIT 12", &stmt))
    {
        fprintf(f, "Rendimiento por Mes:\n");
        fprintf(f, "%-10s %5s %5s %5s %5s\n", "Mes", "PJ", "V", "E", "D");
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(f, "%-10s %5d %5d %5d %5d\n",
                    sqlite3_column_text(stmt, 0),
                    sqlite3_column_int(stmt, 1),
                    sqlite3_column_int(stmt, 2),
                    sqlite3_column_int(stmt, 3),
                    sqlite3_column_int(stmt, 4));
        }
        sqlite3_finalize(stmt);
    }

    fprintf(f, "\n");
    fclose(f);
    printf("Reporte de rendimiento generado: %s\n", path);

    if (preparar_stmt("INSERT INTO reporte_generado (nombre, archivo) VALUES (?,?)", &stmt))
    {
        sqlite3_bind_text(stmt, 1, "Reporte Rendimiento TXT", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    app_log_event("REPORTES", "Reporte rendimiento generado");
    pause_console();
}

static void listar_reportes_generados(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, nombre, archivo, fecha_generacion FROM reporte_generado ORDER BY id DESC LIMIT 20", &stmt))
    {
        mostrar_no_hay_registros("reportes generados");
        return;
    }

    mostrar_pantalla("REPORTES GENERADOS");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. %s\n", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1));
        printf("     Archivo: %s\n", sqlite3_column_text(stmt, 2));
        printf("     Fecha: %s\n", sqlite3_column_text(stmt, 3));
        printf("     ------------------------------\n");
    }
    sqlite3_finalize(stmt);
    if (count == 0) mostrar_no_hay_registros("reportes generados");
    pause_console();
}

static void menu_config_reportes(void)
{
    MenuItem items[] =
    {
        {1, "Configurar Reporte Automatico", configurar_reporte_automatico},
        {2, "Listar Reportes Configurados", listar_reportes_config},
        {3, "Habilitar Reporte", habilitar_reporte},
        {4, "Deshabilitar Reporte", deshabilitar_reporte},
        {0, "Volver", NULL}
    };
    ejecutar_menu("CONFIGURACION DE REPORTES", items, 5);
}

static void menu_generar_reportes(void)
{
    MenuItem items[] =
    {
        {1, "Reporte General HTML", generar_reporte_general_html},
        {2, "Reporte de Rendimiento TXT", generar_reporte_rendimiento_txt},
        {0, "Volver", NULL}
    };
    ejecutar_menu("GENERAR REPORTES", items, 3);
}

void menu_reportes(void)
{
    MenuItem items[] =
    {
        {1, "Configurar Reportes", menu_config_reportes},
        {2, "Generar Reportes", menu_generar_reportes},
        {3, "Reportes Generados", listar_reportes_generados},
        {0, "Volver", NULL}
    };
    ejecutar_menu("REPORTES AUTOMATICOS", items, 4);
}
