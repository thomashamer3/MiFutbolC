
#include "export_all.h"
#include "ascii_art.h"
#include "db.h"
#include "export.h"
#include "export_all_mejorado.h"
#include "export_bienestar.h"
#include "export_calendario.h"
#include "export_camisetas.h"
#include "export_carrera.h"
#include "export_colecciones.h"
#include "export_dashboard.h"
#include "export_equipo.h"
#include "export_estadisticas.h"
#include "export_estadisticas_generales.h"
#include "export_lesiones.h"
#include "export_ods.h"
#include "export_partidos.h"
#include "export_pdf.h"
#include "export_recordatorios.h"
#include "export_records_rankings.h"
#include "export_temporada.h"
#include "export_torneo.h"
#include "menu.h"
#include "settings.h"
#include "utils.h"
#include <sqlite3.h>
#include <stdio.h>

/* Macros para reducir duplicacion de codigo en exportacion */

#define DEFINE_EXPORT_ALL_4(name)                                              \
  static void exportar_##name##_all(void) {                                    \
    exportar_##name##_csv();                                                   \
    exportar_##name##_txt();                                                   \
    exportar_##name##_json();                                                  \
    exportar_##name##_html();                                                  \
  }

#define DEFINE_EXPORT_TODO(name, label)                                        \
  static void exportar_##name##_todo(void) {                                   \
    printf("Exportando " label "...\n");                                       \
    exportar_##name##_all();                                                   \
    printf("Exportacion de " label " completada.\n");                          \
    pause_console();                                                           \
  }

/* partidos/lesiones ahora usan batch en su modulo */

/* estadisticas_generales/por_mes ahora usan batch en su modulo */
/* estadisticas_por_anio ahora usa batch en su modulo */

static void exportar_records_rankings_all(void)
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

/* equipos/temporadas/torneos/bienestar ahora usan batch en su modulo */
/* carrera ahora usa batch en su modulo */
/* colecciones ahora usa batch en su modulo */
DEFINE_EXPORT_ALL_4(recordatorios)
DEFINE_EXPORT_ALL_4(calendario)

DEFINE_EXPORT_TODO(equipos, "equipos")
DEFINE_EXPORT_TODO(temporadas, "temporadas")
DEFINE_EXPORT_TODO(torneos, "torneos")
DEFINE_EXPORT_TODO(bienestar, "bienestar")
DEFINE_EXPORT_TODO(carrera, "carrera")
DEFINE_EXPORT_TODO(colecciones, "colecciones")
DEFINE_EXPORT_TODO(recordatorios, "recordatorios")
DEFINE_EXPORT_TODO(dashboard, "dashboard")
DEFINE_EXPORT_TODO(calendario, "calendario")

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

DEFINE_EXPORT_TODO(camisetas, "camisetas")
DEFINE_EXPORT_TODO(partidos, "partidos")
DEFINE_EXPORT_TODO(lesiones, "lesiones")
DEFINE_EXPORT_TODO(estadisticas, "estadisticas")
DEFINE_EXPORT_TODO(analisis, "analisis")
DEFINE_EXPORT_TODO(estadisticas_generales, "estadisticas generales")
DEFINE_EXPORT_TODO(estadisticas_por_mes, "estadisticas por mes")
DEFINE_EXPORT_TODO(estadisticas_por_anio, "estadisticas por anio")

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
    if (!f)
        return;

    fprintf(f, "# Exportacion Completa - MiFutbolC\n\n");
    fprintf(f, "*Generado el %s*\n\n", __DATE__);

    md_exportar_tabla(
        f, "Partidos",
        "SELECT p.id, p.fecha_hora, p.goles, p.asistencias, p.resultado, "
        "COALESCE(ca.nombre,''), COALESCE(cam.nombre,'') "
        "FROM partido p LEFT JOIN cancha ca ON p.cancha_id=ca.id "
        "LEFT JOIN camiseta cam ON p.camiseta_id=cam.id ORDER BY p.fecha_hora "
        "DESC LIMIT 100",
        (const char *[])
    {"ID", "Fecha", "Goles", "Asist", "Result", "Cancha",
        "Camiseta"
    },
    7);

    md_exportar_tabla(
        f, "Equipos",
        "SELECT id, nombre, tipo, num_jugadores FROM equipo ORDER BY nombre",
        (const char *[])
    {"ID", "Nombre", "Tipo", "Jugadores"
    }, 4);

    md_exportar_tabla(
        f, "Camisetas",
        "SELECT id, nombre, club, temporada FROM camisetas ORDER BY nombre",
        (const char *[])
    {"ID", "Nombre", "Club", "Temp"
    }, 4);

    md_exportar_tabla(
        f, "Canchas",
        "SELECT id, nombre, ciudad, capacidad FROM cancha ORDER BY nombre",
        (const char *[])
    {"ID", "Nombre", "Ciudad", "Capacidad"
    }, 4);

    md_exportar_tabla(
        f, "Jugadores",
        "SELECT id, nombre, numero, posicion FROM jugador ORDER BY nombre",
        (const char *[])
    {"ID", "Nombre", "#", "Pos"
    }, 4);

    md_exportar_tabla(f, "Lesiones",
                      "SELECT id, jugador, tipo, fecha FROM lesion ORDER BY "
                      "fecha DESC LIMIT 50",
                      (const char *[])
    {"ID", "Jugador", "Tipo", "Fecha"
    }, 4);

    md_exportar_tabla(f, "Torneos",
                      "SELECT id, nombre, tipo_torneo, formato_torneo FROM "
                      "torneo ORDER BY nombre",
                      (const char *[])
    {"ID", "Nombre", "Tipo", "Formato"
    }, 4);

    md_exportar_tabla(
        f, "Temporadas",
        "SELECT id, nombre, anio, estado FROM temporada ORDER BY anio DESC",
        (const char *[])
    {"ID", "Nombre", "Anio", "Estado"
    }, 4);

    md_exportar_tabla(f, "Financiamiento",
                      "SELECT id, descripcion, monto, fecha FROM financiamiento "
                      "ORDER BY fecha DESC LIMIT 50",
                      (const char *[])
    {"ID", "Descripcion", "Monto", "Fecha"
    }, 4);

    md_exportar_tabla(
        f, "Carrera - Identidad",
        "SELECT id, nombre, posiciones, club_inicios FROM carrera_identidad",
        (const char *[])
    {"ID", "Nombre", "Posicion", "Club"
    }, 4);

    md_exportar_tabla(f, "Carrera - Hitos",
                      "SELECT id, tipo_hito, nota FROM carrera_partido_hito "
                      "ORDER BY id LIMIT 50",
                      (const char *[])
    {"ID", "Tipo", "Nota"
    }, 3);

    md_exportar_tabla(
        f, "Bienestar - Objetivos",
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
        {1, get_text("export_todos_partidos"), &exportar_partidos_todo},
        {
            2, get_text("export_partido_mas_goles"),
            &exportar_partido_mas_goles_todo
        },
        {
            3, get_text("export_partido_mas_asistencias"),
            &exportar_partido_mas_asistencias_todo
        },
        {
            4, get_text("export_partido_menos_goles_reciente"),
            &exportar_partido_menos_goles_reciente_todo
        },
        {
            5, get_text("export_partido_menos_asistencias_reciente"),
            &exportar_partido_menos_asistencias_reciente_todo
        },
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu(get_text("export_partidos_menu_title"), items, 6);
}

static void menu_exportar_estadisticas_generales()
{
    MenuItem items[] =
    {
        {
            1, get_text("export_estadisticas_generales_item"),
            &exportar_estadisticas_generales_todo
        },
        {
            2, get_text("export_estadisticas_por_mes"),
            &exportar_estadisticas_por_mes_todo
        },
        {
            3, get_text("export_estadisticas_por_anio"),
            &exportar_estadisticas_por_anio_todo
        },
        {4, get_text("export_records_rankings"), &exportar_records_rankings_todo},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu(get_text("export_estadisticas_generales_menu_title"), items, 5);
}

void menu_exportar()
{
    MenuItem items[] =
    {
        {1, "Camisetas", &exportar_camisetas_todo},
        {2, "Partidos", &menu_exportar_partidos},
        {3, "Lesiones", &exportar_lesiones_todo},
        {4, "Estadisticas", &exportar_estadisticas_todo},
        {5, "Analisis", &exportar_analisis_todo},
        {6, "Estadisticas Generales", &menu_exportar_estadisticas_generales},
        {7, "Analisis Avanzado", &menu_exportar_mejorado},
        {8, "Equipos", &exportar_equipos_todo},
        {9, "Temporadas", &exportar_temporadas_todo},
        {10, "Torneos", &exportar_torneos_todo},
        {11, "Bienestar", &exportar_bienestar_todo},
        {12, "Carrera", &exportar_carrera_todo},
        {13, "Colecciones", &exportar_colecciones_todo},
        {14, "Recordatorios", &exportar_recordatorios_todo},
        {15, "Dashboard", &exportar_dashboard_todo},
        {16, "Calendario", &exportar_calendario_todo},
        {17, "Base de Datos", &exportar_base_datos},
        {18, "Exportar Todo", &exportar_todo},
        {19, "Exportar Todo (JSON)", &exportar_todo_json},
        {20, "Exportar Todo (CSV)", &exportar_todo_csv},
        {21, "Informe Total PDF", &exportar_informe_total_pdf},
        {22, "Exportar Todo (Markdown)", &exportar_todo_md},
        {23, "Exportar a XLSX", &menu_exportar_xlsx},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR DATOS", items, 24);
}
