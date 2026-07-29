
#include "export.h"
#include "cJSON.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

char *get_export_path(const char *filename)
{
    static char path[1024];
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        // Fallback to current directory if export dir is not available
        strcpy_s(path, sizeof(path), filename);
    }
    else
    {
        strcpy_s(path, sizeof(path), export_dir);
        strcat_s(path, sizeof(path), PATH_SEP);
        strcat_s(path, sizeof(path), filename);
    }
    return path;
}

int exportar_archivo_si_hay_registros(const char *tabla, const char *mensaje_sin_registros,
                                      const char *filename, const char *error_al_abrir,
                                      const char *cabecera_opcional, ExportWriterFn writer)
{
    if (!tabla || !mensaje_sin_registros || !filename || !error_al_abrir || !writer)
    {
        return 0;
    }

    if (!has_records(tabla))
    {
        mostrar_no_hay_registros(mensaje_sin_registros);
        return 0;
    }

    FILE *file = abrir_archivo_exportacion(filename, error_al_abrir);
    if (!file)
    {
        return 0;
    }

    if (cabecera_opcional)
    {
        fprintf(file, "%s", cabecera_opcional);
    }

    writer(file);

    fclose(file);
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    return 1;
}

void export_write_json_footer(FILE *file, void *context)
{
    cJSON *root = (cJSON *)context;
    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

void export_write_html_begin(FILE *f, const char *title)
{
    fprintf(f,
            "<!DOCTYPE html>\n"
            "<html lang=\"es\">\n"
            "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
            "<title>%s - MiFutbolC</title>\n"
            "<style>\n"
            ":root{--bg:#f0f2f5;--surface:#fff;--text:#1a1a2e;--text2:#4a5568;--accent:#2563eb;--accent2:#0f3460;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;--border:#e0e7ef;--radius:12px;--shadow:0 4px 24px rgba(0,0,0,.08)}\n"
            "@media(prefers-color-scheme:dark){:root{--bg:#0f172a;--surface:#1e293b;--text:#e2e8f0;--text2:#94a3b8;--accent:#3b82f6;--accent2:#60a5fa;--border:#334155;--shadow:0 4px 24px rgba(0,0,0,.4)}}\n"
            "*{margin:0;padding:0;box-sizing:border-box}\n"
            "body{font-family:Inter,'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);padding:24px;line-height:1.6}\n"
            ".container{max-width:1200px;margin:0 auto;background:var(--surface);border-radius:var(--radius);box-shadow:var(--shadow);overflow:hidden}\n"
            "h1{background:linear-gradient(135deg,#1a1a2e,#16213e,#0f3460);color:#fff;margin:0;padding:24px 32px;font-size:1.5em;font-weight:700;letter-spacing:.3px;display:flex;align-items:center;gap:8px}\n"
            "h1::before{font-size:1.2em}\n"
            "@media(prefers-color-scheme:dark){h1{background:linear-gradient(135deg,#0f172a,#1e293b,#2563eb)}}\n"
            "h2{color:var(--accent2);border-bottom:2px solid var(--border);padding:20px 24px 12px;margin:0;font-size:1.15em;font-weight:600}\n"
            ".section-card{margin:16px 24px;border:1px solid var(--border);border-radius:8px;overflow:hidden}\n"
            ".section-card table{border:0;border-radius:0}\n"
            ".section-card h2{margin:0;border-bottom:1px solid var(--border);padding:14px 20px;font-size:1.05em}\n"
            "table{width:100%%;border-collapse:collapse;font-size:.9em;border:1px solid var(--border)}\n"
            "th{background:var(--accent2);color:#fff;padding:12px;text-align:left;font-weight:600;white-space:nowrap}\n"
            "th:first-child{padding-left:24px}\n"
            "th:last-child{padding-right:24px}\n"
            "td{padding:11px 12px;border-bottom:1px solid var(--border);vertical-align:middle}\n"
            "td:first-child{padding-left:24px;font-weight:600;color:var(--accent2)}\n"
            "td:last-child{padding-right:24px}\n"
            "tr:hover td{background:rgba(37,99,235,.06)}\n"
            "tr:nth-child(even) td{background:rgba(0,0,0,.02)}\n"
            "tr:nth-child(even):hover td{background:rgba(37,99,235,.08)}\n"
            ".stat-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px;padding:20px 24px}\n"
            ".stat-card{background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:20px;transition:box-shadow .2s}\n"
            ".stat-card:hover{box-shadow:0 2px 12px rgba(0,0,0,.08)}\n"
            ".stat-card h3{font-size:.82em;text-transform:uppercase;letter-spacing:.5px;color:var(--text2);margin-bottom:8px}\n"
            ".stat-card .stat-value{font-size:1.6em;font-weight:700;color:var(--accent)}\n"
            ".stat-card .stat-meta{font-size:.85em;color:var(--text2);margin-top:4px}\n"
            ".stat-card .stat-label{font-weight:600;color:var(--text2)}\n"
            ".badge{display:inline-block;font-size:.78em;padding:2px 10px;border-radius:20px;font-weight:600}\n"
            ".badge-blue{background:rgba(37,99,235,.12);color:#2563eb}\n"
            ".badge-green{background:rgba(34,197,94,.12);color:#16a34a}\n"
            ".badge-red{background:rgba(239,68,68,.12);color:#dc2626}\n"
            ".badge-yellow{background:rgba(245,158,11,.12);color:#d97706}\n"
            ".alert{padding:12px 20px;border-radius:8px;margin:12px 24px;font-size:.9em}\n"
            ".alert-info{background:rgba(37,99,235,.08);border-left:4px solid var(--accent);color:var(--text)}\n"
            ".alert-warn{background:rgba(245,158,11,.1);border-left:4px solid var(--warn);color:var(--text)}\n"
            ".alert-empty{text-align:center;padding:32px 20px;color:var(--text2);font-style:italic}\n"
            ".content{padding:16px 24px}\n"
            ".content p{padding:6px 0}\n"
            ".content strong{color:var(--accent2)}\n"
            "@media(max-width:768px){\n"
            "body{padding:12px}\n"
            ".container{border-radius:8px}\n"
            "h1{font-size:1.15em;padding:16px 20px}\n"
            "h2{font-size:1em;padding:14px 16px 10px}\n"
            "th,td{padding:8px 6px;font-size:.82em}\n"
            "th:first-child,td:first-child{padding-left:12px}\n"
            "th:last-child,td:last-child{padding-right:12px}\n"
            "table{font-size:.78em}\n"
            ".stat-grid{grid-template-columns:1fr;padding:12px 16px}\n"
            ".section-card{margin:12px 16px}\n"
            ".content{padding:12px 16px}\n"
            ".alert{margin:8px 16px}\n"
            "}\n"
            "@media print{\n"
            "body{background:#fff;padding:0;color:#000}\n"
            ".container{box-shadow:none;border:1px solid #ddd;border-radius:4px}\n"
            "h1{background:#1a1a2e!important;-webkit-print-color-adjust:exact;print-color-adjust:exact}\n"
            "th{background:#0f3460!important;-webkit-print-color-adjust:exact;print-color-adjust:exact}\n"
            ".stat-card{break-inside:avoid}\n"
            "}\n"
            "</style>\n"
            "</head>\n"
            "<body>\n"
            "<div class=\"container\">\n"
            "<h1>%s</h1>\n",
            title, title);
}

void export_write_html_footer(FILE *file, void *context)
{
    (void)context;
    fprintf(file,
            "\n<div style=\"text-align:center;padding:16px 24px;border-top:1px solid var(--border);"
            "font-size:.8em;color:var(--text2)\">"
            "Generado por <strong>MiFutbolC</strong> &mdash; &copy; %d</div>\n"
            "</div>\n</body>\n</html>\n",
            2026);
}

void export_write_html_table_footer(FILE *file, void *context)
{
    (void)context;
    fprintf(file,
            "</table>\n"
            "<div style=\"text-align:center;padding:16px 24px;border-top:1px solid var(--border);"
            "font-size:.8em;color:var(--text2)\">"
            "Generado por <strong>MiFutbolC</strong> &mdash; &copy; %d</div>\n"
            "</div>\n</body>\n</html>\n",
            2026);
}

void export_json_add_lesion_base_fields(cJSON *item, sqlite3_stmt *stmt)
{
    if (!item || !stmt)
    {
        return;
    }

    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "jugador", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "tipo", (const char *)sqlite3_column_text(stmt, 2));
    cJSON_AddStringToObject(item, "descripcion", (const char *)sqlite3_column_text(stmt, 3));
    cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 4));
}

/* ===================== HELPER FUNCTIONS (STATIC) ===================== */

/* Forward declarations of static functions */
static void calcular_estadisticas_generales(Estadisticas *stats);
static void calcular_estadisticas_ultimos5(Estadisticas *stats);
static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas);
static int has_partido_records(void);

/**
 * Verifica si hay registros de partidos en la base de datos.
 * Centraliza la logica de verificacion para evitar duplicacion de codigo.
 * Retorna 1 si hay registros, 0 si no hay.
 */
static int has_partido_records(void)
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    if (!preparar_stmt_export(&check_stmt, "SELECT COUNT(*) FROM partido"))
    {
        return 0;
    }
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    return count > 0;
}

/**
 * Calcula todas las estadisticas necesarias para el analisis.
 * Centraliza la logica de calculo para evitar duplicacion de codigo.
 */
static void calcular_todas_estadisticas(Estadisticas *generales, Estadisticas *ultimos5,
                                        int *mejor_racha_v, int *peor_racha_d)
{
    calcular_estadisticas_generales(generales);
    calcular_estadisticas_ultimos5(ultimos5);
    calcular_rachas(mejor_racha_v, peor_racha_d);
}

/* ===================== ANALISIS ===================== */

/**
 * Calcula estadisticas generales de todos los partidos.
 * Esta funcion es utilizada por el analisis de rendimiento para obtener metricas globales.
 */
static void calcular_estadisticas_generales(Estadisticas *stats)
{
    calcular_estadisticas(stats, "SELECT COUNT(*), AVG(goles), AVG(asistencias), "
                          "AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM partido");
}

static void calcular_estadisticas_ultimos5(Estadisticas *stats)
{
    calcular_estadisticas(stats, "SELECT COUNT(*), AVG(goles), AVG(asistencias), "
                          "AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM (SELECT * FROM partido ORDER BY fecha_hora DESC LIMIT 5)");
}

static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, "SELECT resultado FROM partido ORDER BY fecha_hora"))
    {
        *mejor_racha_victorias = 0;
        *peor_racha_derrotas = 0;
        return;
    }

    int racha_actual_v = 0;
    int max_racha_v = 0;
    int racha_actual_d = 0;
    int max_racha_d = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        actualizar_rachas(resultado, &racha_actual_v, &max_racha_v, &racha_actual_d, &max_racha_d);
    }

    *mejor_racha_victorias = max_racha_v;
    *peor_racha_derrotas = max_racha_d;
    sqlite3_finalize(stmt);
}

/* ===================== EXPORTACIONES TXT ADICIONALES ===================== */

void exportar_finanzas_resumen_txt(void)
{
    FILE *file = abrir_archivo_exportacion("finanzas_resumen.txt",
                                           "Error al crear archivo de finanzas resumen TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "RESUMEN FINANCIERO\n\n");

    if (!has_records("financiamiento"))
    {
        fprintf(file, "No hay transacciones financieras registradas.\n");
        fclose(file);
        return;
    }

    sqlite3_stmt *stmt;

    fprintf(file, "RESUMEN POR MES\n");
    fprintf(file, "----------------\n");
    if (preparar_stmt_export(&stmt, "SELECT strftime('%Y-%m', fecha) as periodo, "
                             "SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) ingresos, "
                             "SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) gastos "
                             "FROM financiamiento GROUP BY periodo ORDER BY periodo"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
            double ingresos = sqlite3_column_double(stmt, 1);
            double gastos = sqlite3_column_double(stmt, 2);
            fprintf(file, "Mes: %s | Ingresos: %.2f | Gastos: %.2f | Balance: %.2f\n",
                    periodo ? periodo : "-", ingresos, gastos, ingresos - gastos);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(file, "\nRESUMEN POR ANIO\n");
    fprintf(file, "----------------\n");
    if (preparar_stmt_export(&stmt, "SELECT strftime('%Y', fecha) as anio, "
                             "SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) ingresos, "
                             "SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) gastos "
                             "FROM financiamiento GROUP BY anio ORDER BY anio"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *anio = (const char *)sqlite3_column_text(stmt, 0);
            double ingresos = sqlite3_column_double(stmt, 1);
            double gastos = sqlite3_column_double(stmt, 2);
            fprintf(file, "Anio: %s | Ingresos: %.2f | Gastos: %.2f | Balance: %.2f\n",
                    anio ? anio : "-", ingresos, gastos, ingresos - gastos);
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
}

void exportar_ranking_canchas_txt(void)
{
    FILE *file = abrir_archivo_exportacion("ranking_canchas.txt",
                                           "Error al crear archivo de ranking de canchas TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "RANKING DE CANCHAS (RENDIMIENTO Y LESIONES)\n\n");

    if (!has_records("cancha"))
    {
        fprintf(file, "No hay canchas registradas.\n");
        fclose(file);
        return;
    }

    sqlite3_stmt *stmt;
    if (preparar_stmt_export(
                &stmt, "SELECT can.nombre, "
                "COUNT(DISTINCT p.id) as partidos, "
                "COALESCE(AVG(p.rendimiento_general), 0), "
                "COUNT(l.id) as lesiones "
                "FROM cancha can "
                "LEFT JOIN partido p ON p.cancha_id = can.id "
                "LEFT JOIN lesion l ON l.partido_id = p.id "
                "GROUP BY can.id "
                "ORDER BY COALESCE(AVG(p.rendimiento_general), 0) DESC, COUNT(l.id) ASC"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            int partidos = sqlite3_column_int(stmt, 1);
            double rendimiento = sqlite3_column_double(stmt, 2);
            int lesiones = sqlite3_column_int(stmt, 3);
            fprintf(file, "Cancha: %s | Partidos: %d | Rendimiento Promedio: %.2f | Lesiones: %d\n",
                    nombre ? nombre : "-", partidos, rendimiento, lesiones);
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
}

void exportar_partidos_por_clima_txt(void)
{
    FILE *file = abrir_archivo_exportacion("partidos_por_clima.txt",
                                           "Error al crear archivo de partidos por clima TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "PARTIDOS POR CLIMA\n\n");

    if (!has_records("partido"))
    {
        fprintf(file, "No hay partidos registrados.\n");
        fclose(file);
        return;
    }

    sqlite3_stmt *stmt;
    if (preparar_stmt_export(&stmt, "SELECT clima, COUNT(*), AVG(goles), AVG(asistencias) "
                             "FROM partido GROUP BY clima ORDER BY clima"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int clima = sqlite3_column_int(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);
            double avg_goles = sqlite3_column_double(stmt, 2);
            double avg_asist = sqlite3_column_double(stmt, 3);
            fprintf(file,
                    "Clima: %s | Partidos: %d | Prom. Goles: %.2f | Prom. Asistencias: %.2f\n",
                    clima_to_text(clima), count, avg_goles, avg_asist);
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
}

void exportar_lesiones_por_tipo_estado_txt(void)
{
    FILE *file = abrir_archivo_exportacion(
                     "lesiones_por_tipo_estado.txt", "Error al crear archivo de lesiones por tipo y estado TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "DISTRIBUCION DE LESIONES POR TIPO Y ESTADO\n\n");

    if (!has_records("lesion"))
    {
        fprintf(file, "No hay lesiones registradas.\n");
        fclose(file);
        return;
    }

    sqlite3_stmt *stmt;
    if (preparar_stmt_export(&stmt, "SELECT tipo, estado, COUNT(*) FROM lesion GROUP BY tipo, "
                             "estado ORDER BY tipo, estado"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *tipo = (const char *)sqlite3_column_text(stmt, 0);
            const char *estado = (const char *)sqlite3_column_text(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);
            fprintf(file, "Tipo: %s | Estado: %s | Cantidad: %d\n", tipo ? tipo : "-",
                    estado ? estado : "-", count);
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
}

void exportar_rachas_historial_txt(void)
{
    FILE *file =
        abrir_archivo_exportacion("rachas_historial.txt", "Error al crear archivo de rachas TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "HISTORIAL DE RACHAS\n\n");

    if (!has_records("partido"))
    {
        fprintf(file, "No hay partidos registrados.\n");
        fclose(file);
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt,
                              "SELECT resultado, fecha_hora FROM partido ORDER BY fecha_hora"))
    {
        fclose(file);
        return;
    }

    int racha_resultado = -1;
    int racha_count = 0;
    char fecha_inicio[32] = "";
    char fecha_fin[32] = "";

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
        char fecha_formateada[32];
        if (fecha_raw)
        {
            format_date_for_display(fecha_raw, fecha_formateada, sizeof(fecha_formateada));
        }
        else
        {
            strcpy_s(fecha_formateada, sizeof(fecha_formateada), "-");
        }

        if (racha_resultado == -1)
        {
            racha_resultado = resultado;
            racha_count = 1;
            strcpy_s(fecha_inicio, sizeof(fecha_inicio), fecha_formateada);
            strcpy_s(fecha_fin, sizeof(fecha_fin), fecha_formateada);
            continue;
        }

        if (resultado == racha_resultado)
        {
            racha_count++;
            strcpy_s(fecha_fin, sizeof(fecha_fin), fecha_formateada);
        }
        else
        {
            fprintf(file, "Racha %s: %d partido(s) | Desde %s hasta %s\n",
                    resultado_to_text(racha_resultado), racha_count, fecha_inicio, fecha_fin);
            racha_resultado = resultado;
            racha_count = 1;
            strcpy_s(fecha_inicio, sizeof(fecha_inicio), fecha_formateada);
            strcpy_s(fecha_fin, sizeof(fecha_fin), fecha_formateada);
        }
    }

    if (racha_resultado != -1)
    {
        fprintf(file, "Racha %s: %d partido(s) | Desde %s hasta %s\n",
                resultado_to_text(racha_resultado), racha_count, fecha_inicio, fecha_fin);
    }

    sqlite3_finalize(stmt);
    fclose(file);
}

void exportar_estado_animo_cansancio_txt(void)
{
    FILE *file = abrir_archivo_exportacion(
                     "estado_animo_cansancio.txt", "Error al crear archivo de estado de animo y cansancio TXT");
    if (!file)
    {
        return;
    }

    fprintf(file, "DISTRIBUCION DE ESTADO DE ANIMO Y CANSANCIO\n\n");

    if (!has_records("partido"))
    {
        fprintf(file, "No hay partidos registrados.\n");
        fclose(file);
        return;
    }

    sqlite3_stmt *stmt;

    fprintf(file, "ESTADO DE ANIMO\n");
    fprintf(file, "---------------\n");
    if (preparar_stmt_export(&stmt, "SELECT estado_animo, COUNT(*) FROM partido GROUP BY "
                             "estado_animo ORDER BY estado_animo"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int valor = sqlite3_column_int(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);
            fprintf(file, "Estado de animo %d: %d partido(s)\n", valor, count);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(file, "\nCANSANCIO\n");
    fprintf(file, "---------\n");
    if (preparar_stmt_export(
                &stmt, "SELECT cansancio, COUNT(*) FROM partido GROUP BY cansancio ORDER BY cansancio"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int valor = sqlite3_column_int(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);
            fprintf(file, "Cansancio %d: %d partido(s)\n", valor, count);
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
}

static const char *mensaje_motivacional(const Estadisticas *ultimos, const Estadisticas *generales)
{
    double diff_goles = ultimos->avg_goles - generales->avg_goles;
    double diff_rendimiento = ultimos->avg_rendimiento - generales->avg_rendimiento;

    if (diff_goles > 0.5 && diff_rendimiento > 0.5)
    {
        return "Excelente. Estas en racha ascendente. Sigue asi, tu esfuerzo esta dando frutos. "
               "Mantien la consistencia y continua trabajando duro en los entrenamientos.";
    }
    else if (diff_goles < -0.5 || diff_rendimiento < -0.5)
    {
        return "No te desanimes. Todos tenemos dias dificiles. Analiza que puedes mejorar: Revisa "
               "tu preparacion fisica y tecnica. Habla con tu entrenador sobre estrategias. "
               "Recuerda: el futbol es un deporte de perseverancia.";
    }
    else
    {
        return "Buen trabajo manteniendo el nivel. La consistencia es clave en el futbol. Sigue "
               "entrenando y manten la motivacion alta. Cada partido es una oportunidad!";
    }
}

/**
 * @name Escritores de analisis (reutilizan stats precalculados)
 */
/** @{ */

static void write_analisis_csv(FILE *file, const Estadisticas *generales,
                               const Estadisticas *ultimos5, int mejor_racha_v, int peor_racha_d,
                               const char *msg)
{
    fprintf(file, "Tipo,Promedio_Goles,Promedio_Asistencias,Promedio_Rendimiento,Promedio_"
            "Cansancio,Promedio_Animo,Total_Partidos\n");
    fprintf(file, "Generales,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n", generales->avg_goles,
            generales->avg_asistencias, generales->avg_rendimiento, generales->avg_cansancio,
            generales->avg_animo, generales->total_partidos);
    fprintf(file, "Ultimos5,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n", ultimos5->avg_goles,
            ultimos5->avg_asistencias, ultimos5->avg_rendimiento, ultimos5->avg_cansancio,
            ultimos5->avg_animo, ultimos5->total_partidos);
    fprintf(file, "Rachas,%d,%d\n", mejor_racha_v, peor_racha_d);
    fprintf(file, "Mensaje,%s\n", msg);
}

static void write_analisis_txt(FILE *file, const Estadisticas *generales,
                               const Estadisticas *ultimos5, int mejor_racha_v, int peor_racha_d,
                               const char *msg)
{
    fprintf(file, "ANALISIS DE RENDIMIENTO\n\n");
    fprintf(file, "ESTADISTICAS GENERALES:\n");
    fprintf(file, "Total Partidos: %d\n", generales->total_partidos);
    fprintf(file, "Promedio Goles: %.2f\n", generales->avg_goles);
    fprintf(file, "Promedio Asistencias: %.2f\n", generales->avg_asistencias);
    fprintf(file, "Promedio Rendimiento: %.2f\n", generales->avg_rendimiento);
    fprintf(file, "Promedio Cansancio: %.2f\n", generales->avg_cansancio);
    fprintf(file, "Promedio Estado Animo: %.2f\n\n", generales->avg_animo);
    fprintf(file, "ULTIMOS 5 PARTIDOS:\n");
    fprintf(file, "Total Partidos: %d\n", ultimos5->total_partidos);
    fprintf(file, "Promedio Goles: %.2f\n", ultimos5->avg_goles);
    fprintf(file, "Promedio Asistencias: %.2f\n", ultimos5->avg_asistencias);
    fprintf(file, "Promedio Rendimiento: %.2f\n", ultimos5->avg_rendimiento);
    fprintf(file, "Promedio Cansancio: %.2f\n", ultimos5->avg_cansancio);
    fprintf(file, "Promedio Estado Animo: %.2f\n\n", ultimos5->avg_animo);
    fprintf(file, "RACHAS:\n");
    fprintf(file, "Mejor racha de victorias: %d partidos\n", mejor_racha_v);
    fprintf(file, "Peor racha de derrotas: %d partidos\n\n", peor_racha_d);
    fprintf(file, "ANALISIS MOTIVACIONAL:\n%s\n", msg);
}

static void write_analisis_json(FILE *file, const Estadisticas *generales,
                                const Estadisticas *ultimos5, int mejor_racha_v, int peor_racha_d,
                                const char *msg)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *generales_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(generales_obj, "total_partidos", generales->total_partidos);
    cJSON_AddNumberToObject(generales_obj, "avg_goles", generales->avg_goles);
    cJSON_AddNumberToObject(generales_obj, "avg_asistencias", generales->avg_asistencias);
    cJSON_AddNumberToObject(generales_obj, "avg_rendimiento", generales->avg_rendimiento);
    cJSON_AddNumberToObject(generales_obj, "avg_cansancio", generales->avg_cansancio);
    cJSON_AddNumberToObject(generales_obj, "avg_animo", generales->avg_animo);
    cJSON_AddItemToObject(root, "generales", generales_obj);
    cJSON *ultimos5_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(ultimos5_obj, "total_partidos", ultimos5->total_partidos);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_goles", ultimos5->avg_goles);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_asistencias", ultimos5->avg_asistencias);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_rendimiento", ultimos5->avg_rendimiento);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_cansancio", ultimos5->avg_cansancio);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_animo", ultimos5->avg_animo);
    cJSON_AddItemToObject(root, "ultimos5", ultimos5_obj);
    cJSON *rachas_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rachas_obj, "mejor_racha_victorias", mejor_racha_v);
    cJSON_AddNumberToObject(rachas_obj, "peor_racha_derrotas", peor_racha_d);
    cJSON_AddItemToObject(root, "rachas", rachas_obj);
    cJSON_AddStringToObject(root, "mensaje_motivacional", msg);
    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

static void write_analisis_table(FILE *file, const char *titulo, const Estadisticas *stats)
{
    fprintf(file, "<div class=\"section-card\"><h2>%s</h2><table>", titulo);
    fprintf(file, "<tr><th>Total Partidos</th><td>%d</td></tr>", stats->total_partidos);
    fprintf(file, "<tr><th>Promedio Goles</th><td>%.2f</td></tr>", stats->avg_goles);
    fprintf(file, "<tr><th>Promedio Asistencias</th><td>%.2f</td></tr>", stats->avg_asistencias);
    fprintf(file, "<tr><th>Promedio Rendimiento</th><td>%.2f</td></tr>", stats->avg_rendimiento);
    fprintf(file, "<tr><th>Promedio Cansancio</th><td>%.2f</td></tr>", stats->avg_cansancio);
    fprintf(file, "<tr><th>Promedio Estado Animo</th><td>%.2f</td></tr>", stats->avg_animo);
    fprintf(file, "</table></div>\n");
}

static void write_analisis_html(FILE *file, const Estadisticas *generales,
                                const Estadisticas *ultimos5, int mejor_racha_v, int peor_racha_d,
                                const char *msg)
{
    export_write_html_begin(file, "Analisis de Rendimiento");
    write_analisis_table(file, "Estadisticas Generales", generales);
    write_analisis_table(file, "Ultimos 5 Partidos", ultimos5);
    fprintf(file, "<div class=\"section-card\"><h2>Rachas</h2><table>");
    fprintf(file, "<tr><th>Mejor Racha Victorias</th><td>%d partidos</td></tr>", mejor_racha_v);
    fprintf(file, "<tr><th>Peor Racha Derrotas</th><td>%d partidos</td></tr>", peor_racha_d);
    fprintf(file, "</table></div>\n");
    fprintf(file, "<div class=\"section-card\"><h2>Analisis Motivacional</h2><div class=\"content\"><p>%s</p></div></div>\n", msg);
    export_write_html_footer(file, NULL);
}

/** @} */

/**
 * Exporta el analisis de rendimiento a un archivo CSV.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_csv(void)
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *file =
        abrir_archivo_exportacion("analisis.csv", "Error al crear archivo de analisis CSV");
    if (!file)
    {
        return;
    }

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;
    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);
    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    write_analisis_csv(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.csv"));
    fclose(file);
}

/**
 * Exporta el analisis de rendimiento a un archivo de texto plano.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_txt(void)
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *file =
        abrir_archivo_exportacion("analisis.txt", "Error al crear archivo de analisis TXT");
    if (!file)
    {
        return;
    }

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;
    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);
    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    write_analisis_txt(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.txt"));
    fclose(file);
}

/**
 * Exporta el analisis de rendimiento a un archivo JSON.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_json(void)
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *file =
        abrir_archivo_exportacion("analisis.json", "Error al crear archivo de analisis JSON");
    if (!file)
    {
        return;
    }

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;
    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);
    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    write_analisis_json(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.json"));
    fclose(file);
}

/**
 * Exporta el analisis de rendimiento a un archivo HTML.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_html(void)
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *file =
        abrir_archivo_exportacion("analisis.html", "Error al crear archivo de analisis HTML");
    if (!file)
    {
        return;
    }

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;
    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);
    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    write_analisis_html(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.html"));
    fclose(file);
}

/**
 * Exporta el analisis de rendimiento a los 4 formatos con una sola
 * ejecucion de calcular_todas_estadisticas().
 */
void exportar_analisis_all(void)
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;
    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);
    const char *msg = mensaje_motivacional(&ultimos5, &generales);

    FILE *file;

    file = abrir_archivo_exportacion("analisis.csv", "Error al crear CSV");
    if (file)
    {
        write_analisis_csv(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);
        printf("Archivo exportado a: %s\n", get_export_path("analisis.csv"));
        fclose(file);
    }

    file = abrir_archivo_exportacion("analisis.txt", "Error al crear TXT");
    if (file)
    {
        write_analisis_txt(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);
        printf("Archivo exportado a: %s\n", get_export_path("analisis.txt"));
        fclose(file);
    }

    file = abrir_archivo_exportacion("analisis.json", "Error al crear JSON");
    if (file)
    {
        write_analisis_json(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);
        printf("Archivo exportado a: %s\n", get_export_path("analisis.json"));
        fclose(file);
    }

    file = abrir_archivo_exportacion("analisis.html", "Error al crear HTML");
    if (file)
    {
        write_analisis_html(file, &generales, &ultimos5, mejor_racha_v, peor_racha_d, msg);
        printf("Archivo exportado a: %s\n", get_export_path("analisis.html"));
        fclose(file);
    }
}
