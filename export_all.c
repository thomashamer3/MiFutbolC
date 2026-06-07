
#include "export_all.h"
#include "export_all_mejorado.h"
#include "export.h"
#include "export_camisetas.h"
#include "export_partidos.h"
#include "export_lesiones.h"
#include "export_estadisticas.h"
#include "export_estadisticas_generales.h"
#include "export_records_rankings.h"
#include "export_pdf.h"
#include "export_equipo.h"
#include "export_temporada.h"
#include "export_torneo.h"
#include "export_bienestar.h"
#include "export_carrera.h"
#include "export_colecciones.h"
#include "export_recordatorios.h"
#include "export_dashboard.h"
#include "export_calendario.h"
#include "export_ods.h"
#include "utils.h"
#include "menu.h"
#include "ascii_art.h"
#include "db.h"
#include "settings.h"
#include <stdio.h>
#include <sqlite3.h>

static void exportar_camisetas_all()
{
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();
}

static void exportar_partidos_all()
{
    exportar_partidos_csv();
    exportar_partidos_txt();
    exportar_partidos_json();
    exportar_partidos_html();
}

static void exportar_lesiones_all()
{
    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();
}

static void exportar_estadisticas_all()
{
    exportar_estadisticas_csv();
    exportar_estadisticas_txt();
    exportar_estadisticas_json();
    exportar_estadisticas_html();
    exportar_estadisticas_md();
}

static void exportar_analisis_all()
{
    exportar_analisis_csv();
    exportar_analisis_txt();
    exportar_analisis_json();
    exportar_analisis_html();
}

static void exportar_estadisticas_generales_all()
{
    exportar_estadisticas_generales_csv();
    exportar_estadisticas_generales_txt();
    exportar_estadisticas_generales_json();
    exportar_estadisticas_generales_html();
    exportar_estadisticas_generales_md();
}

static void exportar_estadisticas_por_mes_all()
{
    exportar_estadisticas_por_mes_csv();
    exportar_estadisticas_por_mes_txt();
    exportar_estadisticas_por_mes_json();
    exportar_estadisticas_por_mes_html();
}

static void exportar_estadisticas_por_anio_all()
{
    exportar_estadisticas_por_anio_csv();
    exportar_estadisticas_por_anio_txt();
    exportar_estadisticas_por_anio_json();
    exportar_estadisticas_por_anio_html();
}

static void exportar_records_rankings_all()
{
    exportar_record_goles_partido_csv();
    exportar_record_asistencias_partido_csv();
    exportar_mejor_combinacion_cancha_camiseta_csv();
    exportar_peor_combinacion_cancha_camiseta_csv();
    exportar_mejor_temporada_csv();
    exportar_peor_temporada_csv();
    exportar_records_rankings_txt();
    exportar_records_rankings_json();
    exportar_records_rankings_html();
}

static void exportar_equipos_all()
{
    exportar_equipos_csv();
    exportar_equipos_txt();
    exportar_equipos_json();
    exportar_equipos_html();
}

static void exportar_temporadas_all()
{
    exportar_temporadas_csv();
    exportar_temporadas_txt();
    exportar_temporadas_json();
    exportar_temporadas_html();
}

static void exportar_torneos_all()
{
    exportar_torneos_csv();
    exportar_torneos_txt();
    exportar_torneos_json();
    exportar_torneos_html();
}

static void exportar_bienestar_all()
{
    exportar_bienestar_csv();
    exportar_bienestar_txt();
    exportar_bienestar_json();
    exportar_bienestar_html();
}

static void exportar_carrera_all()
{
    exportar_carrera_csv();
    exportar_carrera_txt();
    exportar_carrera_json();
    exportar_carrera_html();
    exportar_carrera_pdf();
}

static void exportar_colecciones_all()
{
    exportar_colecciones_csv();
    exportar_colecciones_txt();
    exportar_colecciones_json();
    exportar_colecciones_html();
}

static void exportar_recordatorios_all()
{
    exportar_recordatorios_csv();
    exportar_recordatorios_txt();
    exportar_recordatorios_json();
    exportar_recordatorios_html();
}

static void exportar_dashboard_all()
{
    exportar_dashboard_csv();
    exportar_dashboard_txt();
    exportar_dashboard_json();
    exportar_dashboard_html();
}

static void exportar_calendario_all()
{
    exportar_calendario_csv();
    exportar_calendario_txt();
    exportar_calendario_json();
    exportar_calendario_html();
}

static void exportar_equipos_todo()
{
    printf("Exportando equipos...\n");
    exportar_equipos_all();
    printf("Exportacion de equipos completada.\n");
    pause_console();
}

static void exportar_temporadas_todo()
{
    printf("Exportando temporadas...\n");
    exportar_temporadas_all();
    printf("Exportacion de temporadas completada.\n");
    pause_console();
}

static void exportar_torneos_todo()
{
    printf("Exportando torneos...\n");
    exportar_torneos_all();
    printf("Exportacion de torneos completada.\n");
    pause_console();
}

static void exportar_bienestar_todo()
{
    printf("Exportando bienestar...\n");
    exportar_bienestar_all();
    printf("Exportacion de bienestar completada.\n");
    pause_console();
}

static void exportar_carrera_todo()
{
    printf("Exportando carrera...\n");
    exportar_carrera_all();
    printf("Exportacion de carrera completada.\n");
    pause_console();
}

static void exportar_colecciones_todo()
{
    printf("Exportando colecciones...\n");
    exportar_colecciones_all();
    printf("Exportacion de colecciones completada.\n");
    pause_console();
}

static void exportar_recordatorios_todo()
{
    printf("Exportando recordatorios...\n");
    exportar_recordatorios_all();
    printf("Exportacion de recordatorios completada.\n");
    pause_console();
}

static void exportar_dashboard_todo()
{
    printf("Exportando dashboard...\n");
    exportar_dashboard_all();
    printf("Exportacion de dashboard completada.\n");
    pause_console();
}

static void exportar_calendario_todo()
{
    printf("Exportando calendario...\n");
    exportar_calendario_all();
    printf("Exportacion de calendario completada.\n");
    pause_console();
}

static void exportar_todo_json()
{
    printf("Exportando todo (JSON)...\n");

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    exportar_camisetas_json();
    exportar_partidos_json();
    exportar_lesiones_json();
    exportar_estadisticas_json();
    exportar_analisis_json();
    exportar_estadisticas_generales_json();
    exportar_estadisticas_por_mes_json();
    exportar_estadisticas_por_anio_json();
    exportar_records_rankings_json();

    exportar_partido_mas_goles_json();
    exportar_partido_mas_asistencias_json();
    exportar_partido_menos_goles_reciente_json();
    exportar_partido_menos_asistencias_reciente_json();

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    printf("Exportacion JSON completada.\n");
    pause_console();
}

static void exportar_todo_csv()
{
    printf("Exportando todo (CSV)...\n");

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    exportar_camisetas_csv();
    exportar_partidos_csv();
    exportar_lesiones_csv();
    exportar_estadisticas_csv();
    exportar_analisis_csv();
    exportar_estadisticas_generales_csv();
    exportar_estadisticas_por_mes_csv();
    exportar_estadisticas_por_anio_csv();
    exportar_record_goles_partido_csv();
    exportar_record_asistencias_partido_csv();
    exportar_mejor_combinacion_cancha_camiseta_csv();
    exportar_peor_combinacion_cancha_camiseta_csv();
    exportar_mejor_temporada_csv();
    exportar_peor_temporada_csv();

    exportar_partido_mas_goles_csv();
    exportar_partido_mas_asistencias_csv();
    exportar_partido_menos_goles_reciente_csv();
    exportar_partido_menos_asistencias_reciente_csv();

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    printf("Exportacion CSV completada.\n");
    pause_console();
}

static void exportar_camisetas_todo()
{
    printf("Exportando camisetas...\n");
    exportar_camisetas_all();
    printf("Exportacion de camisetas completada.\n");
    pause_console();
}

static void exportar_partidos_todo()
{
    printf("Exportando partidos...\n");
    exportar_partidos_all();
    printf("Exportacion de partidos completada.\n");
    pause_console();
}

static void exportar_lesiones_todo()
{
    printf("Exportando lesiones...\n");
    exportar_lesiones_all();
    printf("Exportacion de lesiones completada.\n");
    pause_console();
}

static void exportar_estadisticas_todo()
{
    printf("Exportando estadisticas...\n");
    exportar_estadisticas_all();
    printf("Exportacion de estadisticas completada.\n");
    pause_console();
}

static void exportar_analisis_todo()
{
    printf("Exportando analisis...\n");
    exportar_analisis_all();
    printf("Exportacion de analisis completada.\n");
    pause_console();
}

static void exportar_estadisticas_generales_todo()
{
    printf("Exportando estadisticas generales...\n");
    exportar_estadisticas_generales_all();
    printf("Exportacion de estadisticas generales completada.\n");
    pause_console();
}

static void exportar_estadisticas_por_mes_todo()
{
    printf("Exportando estadisticas por mes...\n");
    exportar_estadisticas_por_mes_all();
    printf("Exportacion de estadisticas por mes completada.\n");
    pause_console();
}

static void exportar_estadisticas_por_anio_todo()
{
    printf("Exportando estadisticas por anio...\n");
    exportar_estadisticas_por_anio_all();
    printf("Exportacion de estadisticas por anio completada.\n");
    pause_console();
}

static void exportar_records_rankings_todo()
{
    printf("Exportando records & rankings...\n");
    exportar_records_rankings_all();
    printf("Exportacion de records & rankings completada.\n");
    pause_console();
}

static void exportar_partido_mas_goles_todo()
{
    printf("Exportando partido con mas goles...\n");
    exportar_partido_mas_goles_csv();
    exportar_partido_mas_goles_txt();
    exportar_partido_mas_goles_json();
    exportar_partido_mas_goles_html();
    printf("Exportacion completada.\n");
    pause_console();
}

static void exportar_partido_mas_asistencias_todo()
{
    printf("Exportando partido con mas asistencias...\n");
    exportar_partido_mas_asistencias_csv();
    exportar_partido_mas_asistencias_txt();
    exportar_partido_mas_asistencias_json();
    exportar_partido_mas_asistencias_html();
    printf("Exportacion completada.\n");
    pause_console();
}

static void exportar_partido_menos_goles_reciente_todo()
{
    printf("Exportando partido menos goles reciente...\n");
    exportar_partido_menos_goles_reciente_csv();
    exportar_partido_menos_goles_reciente_txt();
    exportar_partido_menos_goles_reciente_json();
    exportar_partido_menos_goles_reciente_html();
    printf("Exportacion completada.\n");
    pause_console();
}

static void exportar_partido_menos_asistencias_reciente_todo()
{
    printf("Exportando partido menos asistencias reciente...\n");
    exportar_partido_menos_asistencias_reciente_csv();
    exportar_partido_menos_asistencias_reciente_txt();
    exportar_partido_menos_asistencias_reciente_json();
    exportar_partido_menos_asistencias_reciente_html();
    printf("Exportacion completada.\n");
    pause_console();
}

static void exportar_todo()
{
    printf("Exportando todo...\n");

    // Exportar camisetas en todos los formatos
    exportar_camisetas_all();

    // Exportar partidos en todos los formatos
    exportar_partidos_all();

    // Exportar partidos especificos
    exportar_partido_mas_goles_csv();
    exportar_partido_mas_goles_txt();
    exportar_partido_mas_goles_json();
    exportar_partido_mas_goles_html();

    exportar_partido_mas_asistencias_csv();
    exportar_partido_mas_asistencias_txt();
    exportar_partido_mas_asistencias_json();
    exportar_partido_mas_asistencias_html();

    exportar_partido_menos_goles_reciente_csv();
    exportar_partido_menos_goles_reciente_txt();
    exportar_partido_menos_goles_reciente_json();
    exportar_partido_menos_goles_reciente_html();

    exportar_partido_menos_asistencias_reciente_csv();
    exportar_partido_menos_asistencias_reciente_txt();
    exportar_partido_menos_asistencias_reciente_json();
    exportar_partido_menos_asistencias_reciente_html();

    // Exportar lesiones en todos los formatos
    exportar_lesiones_all();

    // Exportar estadisticas basicas
    exportar_estadisticas_all();

    // Exportar analisis
    exportar_analisis_all();

    // Exportar estadisticas generales
    exportar_estadisticas_generales_all();

    // Exportar estadisticas por mes
    exportar_estadisticas_por_mes_all();

    // Exportar estadisticas por ano
    exportar_estadisticas_por_anio_all();

    // Exportar records y rankings
    exportar_records_rankings_all();

    // Exportar nuevos modulos
    exportar_equipos_all();
    exportar_temporadas_all();
    exportar_torneos_all();
    exportar_bienestar_all();
    exportar_carrera_all();
    exportar_colecciones_all();
    exportar_recordatorios_all();
    exportar_dashboard_all();
    exportar_calendario_all();

    printf("Exportacion de todo completada.\n");
    pause_console();
}

static void exportar_informe_total_pdf()
{
    printf("Generando informe total en PDF...\n");
    generar_informe_total_pdf();
    pause_console();
}

static void md_exportar_tabla(FILE *f, const char *titulo, const char *sql,
                              const char *encabezados[], int num_cols)
{
    fprintf(f, "\n## %s\n\n", titulo);
    fprintf(f, "|");
    for (int i = 0; i < num_cols; i++)
        fprintf(f, " %s |", encabezados[i]);
    fprintf(f, "\n|");
    for (int i = 0; i < num_cols; i++)
        fprintf(f, "---|");
    fprintf(f, "\n");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(f, "_Sin datos._\n\n");
        return;
    }

    int rows = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(f, "|");
        for (int i = 0; i < num_cols; i++)
        {
            if (sqlite3_column_type(stmt, i) == SQLITE_NULL)
                fprintf(f, " |");
            else if (sqlite3_column_type(stmt, i) == SQLITE_INTEGER)
                fprintf(f, " %d |", sqlite3_column_int(stmt, i));
            else if (sqlite3_column_type(stmt, i) == SQLITE_FLOAT)
                fprintf(f, " %.2f |", sqlite3_column_double(stmt, i));
            else
            {
                const char *txt = (const char *)sqlite3_column_text(stmt, i);
                fprintf(f, " %s |", txt ? txt : "");
            }
        }
        fprintf(f, "\n");
        rows++;
    }
    sqlite3_finalize(stmt);
    if (rows == 0)
        fprintf(f, "_Sin datos._\n");
    fprintf(f, "\n");
}

static void exportar_todo_md()
{
    printf("Exportando todo a Markdown...\n");

    FILE *f = abrir_archivo_exportacion("exportacion_completa.md",
                                        "Error al crear el archivo Markdown");
    if (!f) return;

    fprintf(f, "# Exportacion Completa - MiFutbolC\n\n");
    fprintf(f, "*Generado el %s*\n\n", __DATE__);

    md_exportar_tabla(f, "Partidos",
                      "SELECT p.id, p.fecha_hora, p.goles, p.asistencias, p.resultado, "
                      "COALESCE(ca.nombre,''), COALESCE(cam.nombre,'') "
                      "FROM partido p LEFT JOIN cancha ca ON p.cancha_id=ca.id "
                      "LEFT JOIN camiseta cam ON p.camiseta_id=cam.id ORDER BY p.fecha_hora DESC LIMIT 100",
                      (const char *[])
    {"ID", "Fecha", "Goles", "Asist", "Result", "Cancha", "Camiseta"
    }, 7);

    md_exportar_tabla(f, "Equipos",
                      "SELECT id, nombre, tipo, num_jugadores FROM equipo ORDER BY nombre",
                      (const char *[])
    {"ID", "Nombre", "Tipo", "Jugadores"
    }, 4);

    md_exportar_tabla(f, "Camisetas",
                      "SELECT id, nombre, club, temporada FROM camisetas ORDER BY nombre",
                      (const char *[])
    {"ID", "Nombre", "Club", "Temp"
    }, 4);

    md_exportar_tabla(f, "Canchas",
                      "SELECT id, nombre, ciudad, capacidad FROM cancha ORDER BY nombre",
                      (const char *[])
    {"ID", "Nombre", "Ciudad", "Capacidad"
    }, 4);

    md_exportar_tabla(f, "Jugadores",
                      "SELECT id, nombre, numero, posicion FROM jugador ORDER BY nombre",
                      (const char *[])
    {"ID", "Nombre", "#", "Pos"
    }, 4);

    md_exportar_tabla(f, "Lesiones",
                      "SELECT id, jugador, tipo, fecha FROM lesion ORDER BY fecha DESC LIMIT 50",
                      (const char *[])
    {"ID", "Jugador", "Tipo", "Fecha"
    }, 4);

    md_exportar_tabla(f, "Torneos",
                      "SELECT id, nombre, tipo_torneo, formato_torneo FROM torneo ORDER BY nombre",
                      (const char *[])
    {"ID", "Nombre", "Tipo", "Formato"
    }, 4);

    md_exportar_tabla(f, "Temporadas",
                      "SELECT id, nombre, anio, estado FROM temporada ORDER BY anio DESC",
                      (const char *[])
    {"ID", "Nombre", "Anio", "Estado"
    }, 4);

    md_exportar_tabla(f, "Financiamiento",
                      "SELECT id, descripcion, monto, fecha FROM financiamiento ORDER BY fecha DESC LIMIT 50",
                      (const char *[])
    {"ID", "Descripcion", "Monto", "Fecha"
    }, 4);

    md_exportar_tabla(f, "Carrera - Identidad",
                      "SELECT id, nombre, posiciones, club_inicios FROM carrera_identidad",
                      (const char *[])
    {"ID", "Nombre", "Posicion", "Club"
    }, 4);

    md_exportar_tabla(f, "Carrera - Hitos",
                      "SELECT id, tipo_hito, nota FROM carrera_partido_hito ORDER BY id LIMIT 50",
                      (const char *[])
    {"ID", "Tipo", "Nota"
    }, 3);

    md_exportar_tabla(f, "Bienestar - Objetivos",
                      "SELECT id, descripcion FROM bienestar_objetivo ORDER BY id",
                      (const char *[])
    {"ID", "Descripcion"
    }, 2);

    fclose(f);
    printf("Exportado a: %s\n", get_export_path("exportacion_completa.md"));
    pause_console();
}

static void menu_exportar_partidos()
{
    MenuItem items[] =
    {
        {1, get_text("export_todos_partidos"), exportar_partidos_todo},
        {2, get_text("export_partido_mas_goles"), exportar_partido_mas_goles_todo},
        {3, get_text("export_partido_mas_asistencias"), exportar_partido_mas_asistencias_todo},
        {4, get_text("export_partido_menos_goles_reciente"), exportar_partido_menos_goles_reciente_todo},
        {5, get_text("export_partido_menos_asistencias_reciente"), exportar_partido_menos_asistencias_reciente_todo},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu(get_text("export_partidos_menu_title"), items, 6);
}

static void menu_exportar_estadisticas_generales()
{
    MenuItem items[] =
    {
        {1, get_text("export_estadisticas_generales_item"), exportar_estadisticas_generales_todo},
        {2, get_text("export_estadisticas_por_mes"), exportar_estadisticas_por_mes_todo},
        {3, get_text("export_estadisticas_por_anio"), exportar_estadisticas_por_anio_todo},
        {4, get_text("export_records_rankings"), exportar_records_rankings_todo},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu(get_text("export_estadisticas_generales_menu_title"), items, 5);
}

void menu_exportar()
{
    MenuItem items[] =
    {
        {1, "Camisetas", exportar_camisetas_todo},
        {2, "Partidos", menu_exportar_partidos},
        {3, "Lesiones", exportar_lesiones_todo},
        {4, "Estadisticas", exportar_estadisticas_todo},
        {5, "Analisis", exportar_analisis_todo},
        {6, "Estadisticas Generales", menu_exportar_estadisticas_generales},
        {7, "Analisis Avanzado", menu_exportar_mejorado},
        {8, "Equipos", exportar_equipos_todo},
        {9, "Temporadas", exportar_temporadas_todo},
        {10, "Torneos", exportar_torneos_todo},
        {11, "Bienestar", exportar_bienestar_todo},
        {12, "Carrera", exportar_carrera_todo},
        {13, "Colecciones", exportar_colecciones_todo},
        {14, "Recordatorios", exportar_recordatorios_todo},
        {15, "Dashboard", exportar_dashboard_todo},
        {16, "Calendario", exportar_calendario_todo},
        {17, "Base de Datos", exportar_base_datos},
        {18, "Exportar Todo", exportar_todo},
        {19, "Exportar Todo (JSON)", exportar_todo_json},
        {20, "Exportar Todo (CSV)", exportar_todo_csv},
        {21, "Informe Total PDF", exportar_informe_total_pdf},
        {22, "Exportar Todo (Markdown)", exportar_todo_md},
        {23, "Exportar a XLSX", &menu_exportar_xlsx},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR DATOS", items, 24);
}
