/**
 * @file resumen_compartible.c
 * @brief Generador de resumenes compartibles del perfil futbolistico
 *
 * Genera resumenes visuales del perfil del jugador en formato HTML y Markdown,
 * incluyendo estadisticas, rendimiento, rachas, logros y mejores temporadas.
 * Optimizados para compartir en redes sociales y mensajeria.
 */

#include "resumen_compartible.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "export.h"
#include "logros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

/* ─── Helpers estaticos ───────────────────────────────────────────────────── */

/**
 * @brief Prepara una sentencia SQL usando la conexion global
 * @param sql Consulta SQL a preparar
 * @param stmt Puntero donde almacenar la sentencia preparada
 * @return 1 si la preparacion fue exitosa, 0 en caso contrario
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

/**
 * @brief Obtiene el nombre del jugador activo desde la base de datos
 *
 * Utiliza get_user_name() para obtener el nombre configurado.
 * Si no hay nombre asignado, retorna "Jugador".
 *
 * @param buffer Buffer de salida para el nombre
 * @param size Tamano del buffer
 */
static void obtener_nombre_jugador(char *buffer, size_t size)
{
    char *nombre = get_user_name();
    if (nombre && nombre[0] != '\0')
    {
        snprintf(buffer, size, "%s", nombre);
    }
    else
    {
        snprintf(buffer, size, "Jugador");
    }
    free(nombre);
}

/**
 * @brief Cuenta el total de partidos registrados
 * @return Numero total de partidos, 0 si no hay registros
 */
static int contar_total_partidos(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COUNT(*) FROM partido;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/**
 * @brief Obtiene la suma total de goles
 * @return Total de goles, 0 si no hay registros
 */
static int contar_total_goles(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COALESCE(SUM(goles), 0) FROM partido;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/**
 * @brief Obtiene la suma total de asistencias
 * @return Total de asistencias, 0 si no hay registros
 */
static int contar_total_asistencias(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COALESCE(SUM(asistencias), 0) FROM partido;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/**
 * @brief Calcula la racha actual basada en todos los partidos
 *
 * Analiza los goles y asistencias desde el partido mas reciente hacia atras
 * para determinar la racha actual.
 *
 * @param tipo_racha Char de salida: 'P' positiva, 'N' negativa, 'X' neutral
 * @return Longitud de la racha actual
 */
static int calcular_racha_actual(char *tipo_racha)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT goles, asistencias, fecha_hora FROM partido "
        "ORDER BY fecha_hora DESC, id DESC;";

    int racha = 0;
    char tipo_inicial = '\0';
    *tipo_racha = 'X';

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int goles = sqlite3_column_int(stmt, 0);
        int asistencias = sqlite3_column_int(stmt, 1);

        char tipo_actual;
        if (goles >= 2)
        {
            tipo_actual = 'P';
        }
        else if (goles == 0 && asistencias == 0)
        {
            tipo_actual = 'N';
        }
        else
        {
            tipo_actual = 'X';
        }

        if (tipo_inicial == '\0')
        {
            tipo_inicial = tipo_actual;
            *tipo_racha = tipo_actual;
            racha = 1;
        }
        else if (tipo_actual == tipo_inicial)
        {
            racha++;
        }
        else
        {
            break;
        }
    }

    sqlite3_finalize(stmt);
    return racha;
}

/**
 * @brief Muestra en consola el detalle de los partidos que forman la racha
 *
 * Lista los partidos mas recientes con su fecha, goles y asistencias
 * para que el usuario verifique visualmente la racha.
 */
static void mostrar_detalle_racha(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT fecha_hora, goles, asistencias FROM partido "
        "ORDER BY fecha_hora DESC, id DESC LIMIT 10;";

    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }

    ui_printf("\nUltimos 10 partidos:\n");
    ui_printf("  %-20s %s %s\n", "Fecha", "G", "A");
    ui_printf("  %-20s %s %s\n", "--------------------", "--", "--");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        int goles = sqlite3_column_int(stmt, 1);
        int asistencias = sqlite3_column_int(stmt, 2);
        ui_printf("  %-20s %d %d\n", fecha ? fecha : "?", goles, asistencias);
    }

    sqlite3_finalize(stmt);
    ui_printf("\n");
}

/**
 * @brief Obtiene el progreso de logros como porcentaje
 *
 * Retorna 0 ya que la tabla logro_oculto fue eliminada.
 *
 * @param completados Puntero donde almacenar la cantidad completada
 * @param total Puntero donde almacenar el total de logros
 * @return Porcentaje de progreso (0-100)
 */
static int obtener_progreso_logros(int *completados, int *total)
{
    *total = logros_get_total();
    *completados = 0;
    return 0;
}

/**
 * @brief Obtiene el nombre de la camiseta mas usada
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_camiseta_favorita(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT c.nombre FROM partido p "
        "JOIN camiseta c ON p.camiseta_id = c.id "
        "GROUP BY p.camiseta_id ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", nombre ? nombre : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene la cancha mas jugada
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_cancha_favorita(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT ca.nombre FROM partido p "
        "JOIN cancha ca ON p.cancha_id = ca.id "
        "GROUP BY p.cancha_id ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", nombre ? nombre : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene el rival mas enfrentado
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_rival_favorito(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT rival_nombre FROM partido "
        "WHERE rival_nombre IS NOT NULL AND rival_nombre != '' "
        "GROUP BY rival_nombre ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", nombre ? nombre : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Cuenta victorias, empates y derrotas
 * @param v Puntero para victorias
 * @param e Puntero para empates
 * @param d Puntero para derrotas
 */
static void contar_resultados(int *v, int *e, int *d)
{
    sqlite3_stmt *stmt;
    *v = 0;
    *e = 0;
    *d = 0;

    if (preparar_stmt(
                "SELECT "
                "COUNT(CASE WHEN resultado = 1 THEN 1 END), "
                "COUNT(CASE WHEN resultado = 2 THEN 1 END), "
                "COUNT(CASE WHEN resultado = 3 THEN 1 END) "
                "FROM partido;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            *v = sqlite3_column_int(stmt, 0);
            *e = sqlite3_column_int(stmt, 1);
            *d = sqlite3_column_int(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Obtiene la posicion donde se rindio mejor
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_mejor_posicion(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT posicion_jugada FROM partido "
        "WHERE posicion_jugada IS NOT NULL AND posicion_jugada != '' "
        "GROUP BY posicion_jugada "
        "ORDER BY AVG(rendimiento_general) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *pos = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", pos ? pos : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene datos del ultimo partido jugado
 * @param fecha Buffer de salida para fecha
 * @param fecha_size Tamano del buffer de fecha
 * @param rival Buffer de salida para rival
 * @param rival_size Tamano del buffer de rival
 * @param goles Puntero para goles del ultimo partido
 * @param asistencias Puntero para asistencias del ultimo partido
 * @param resultado Puntero para resultado (1=V, 2=E, 3=D)
 */
static void obtener_ultimo_partido(char *fecha, size_t fecha_size,
                                   char *rival, size_t rival_size,
                                   int *goles, int *asistencias, int *resultado)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT fecha_hora, rival_nombre, goles, asistencias, resultado "
        "FROM partido ORDER BY id DESC LIMIT 1;";

    snprintf(fecha, fecha_size, "N/A");
    snprintf(rival, rival_size, "N/A");
    *goles = 0;
    *asistencias = 0;
    *resultado = 0;

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *f = (const char *)sqlite3_column_text(stmt, 0);
            const char *r = (const char *)sqlite3_column_text(stmt, 1);
            if (f) snprintf(fecha, fecha_size, "%s", f);
            if (r) snprintf(rival, rival_size, "%s", r);
            *goles = sqlite3_column_int(stmt, 2);
            *asistencias = sqlite3_column_int(stmt, 3);
            *resultado = sqlite3_column_int(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Obtiene el clima mas frecuente
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_clima_favorito(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT clima FROM partido "
        "WHERE clima > 0 "
        "GROUP BY clima ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int clima_val = sqlite3_column_int(stmt, 0);
            snprintf(buffer, size, "%s", clima_to_text(clima_val));
        }
        else
        {
            snprintf(buffer, size, "Sin datos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Sin datos");
    }
}

/**
 * @brief Obtiene el torneo mas participado
 * @param buffer Buffer de salida
 * @param size Tamano del buffer
 */
static void obtener_torneo_favorito(char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT t.nombre FROM partido_torneo pt "
        "JOIN torneo t ON pt.torneo_id = t.id "
        "GROUP BY pt.torneo_id ORDER BY COUNT(*) DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(buffer, size, "%s", nombre ? nombre : "Sin datos");
        }
        else
        {
            snprintf(buffer, size, "Ningun partido vinculado a torneos");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(buffer, size, "Ningun partido vinculado a torneos");
    }
}

/**
 * @brief Cuenta el total de lesiones registradas
 * @return Numero total de lesiones, 0 si no hay registros
 */
static int contar_lesiones(void)
{
    sqlite3_stmt *stmt;
    int total = 0;

    if (preparar_stmt("SELECT COUNT(*) FROM lesion;", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return total;
}

/* ─── Funciones publicas ──────────────────────────────────────────────────── */

void menu_resumen_compartible(void)
{
    app_log_event("RESUMEN_COMPARTIBLE", "Ingreso al modulo Resumen Compartible");

    MenuItem items[] =
    {
        {1, "Generar Resumen HTML", generar_resumen_html},
        {2, "Generar Resumen Markdown", generar_resumen_markdown},
        {3, "Estadisticas Destacadas", generar_resumen_estadisticas},
        {4, "Mejor Temporada", generar_resumen_mejor_temporada},
        {0, "Volver", NULL}
    };
    ejecutar_menu("RESUMEN COMPARTIBLE", items, 5);
}

void generar_resumen_html(void)
{
    clear_screen();
    print_header("RESUMEN HTML COMPARTIBLE");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    char jugador[128];
    obtener_nombre_jugador(jugador, sizeof(jugador));

    int total_partidos = contar_total_partidos();
    int total_goles = contar_total_goles();
    int total_asistencias = contar_total_asistencias();

    sqlite3_stmt *stmt;
    int mejor_rendimiento = 0;
    char mejor_fecha[64] = "N/A";

    if (preparar_stmt(
                "SELECT rendimiento_general, fecha_hora FROM partido "
                "ORDER BY rendimiento_general DESC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            mejor_rendimiento = sqlite3_column_int(stmt, 0);
            const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
            if (fecha_raw)
            {
                snprintf(mejor_fecha, sizeof(mejor_fecha), "%s", fecha_raw);
            }
        }
        sqlite3_finalize(stmt);
    }

    char tipo_racha;
    int racha_longitud = calcular_racha_actual(&tipo_racha);
    const char *racha_texto;
    const char *racha_desc;
    const char *racha_class;
    switch (tipo_racha)
    {
    case 'P':
        racha_texto = "Positiva";
        racha_desc = "Rendimiento destacado en los ultimos partidos";
        racha_class = "positiva";
        break;
    case 'N':
        racha_texto = "Negativa";
        racha_desc = "Momento dificil, pero siempre se puede dar la vuelta";
        racha_class = "negativa";
        break;
    default:
        racha_texto = "Neutral";
        racha_desc = "Resultados mixtos en los ultimos partidos";
        racha_class = "neutral";
        break;
    }

    int logros_total = 0;
    int logros_completados = 0;
    obtener_progreso_logros(&logros_completados, &logros_total);

    char fecha_gen[64];
    get_datetime(fecha_gen, sizeof(fecha_gen));

    char camiseta_fav[128];
    obtener_camiseta_favorita(camiseta_fav, sizeof(camiseta_fav));

    char cancha_fav[128];
    obtener_cancha_favorita(cancha_fav, sizeof(cancha_fav));

    double promedio_goles = total_partidos > 0 ? (double)total_goles / total_partidos : 0.0;
    double promedio_asistencias =
        total_partidos > 0 ? (double)total_asistencias / total_partidos : 0.0;

    char rival_fav[128];
    obtener_rival_favorito(rival_fav, sizeof(rival_fav));

    int victorias = 0;
    int empates = 0;
    int derrotas = 0;
    contar_resultados(&victorias, &empates, &derrotas);

    char mejor_posicion[128];
    obtener_mejor_posicion(mejor_posicion, sizeof(mejor_posicion));

    char ultimo_fecha[64];
    char ultimo_rival[128];
    int ult_goles = 0;
    int ult_asistencias = 0;
    int ult_resultado = 0;
    obtener_ultimo_partido(ultimo_fecha, sizeof(ultimo_fecha),
                           ultimo_rival, sizeof(ultimo_rival),
                           &ult_goles, &ult_asistencias, &ult_resultado);

    char clima_fav[128];
    obtener_clima_favorito(clima_fav, sizeof(clima_fav));

    char torneo_fav[128];
    obtener_torneo_favorito(torneo_fav, sizeof(torneo_fav));

    int total_lesiones = contar_lesiones();
    char lesiones_str[32];
    if (total_lesiones == 0)
    {
        snprintf(lesiones_str, sizeof(lesiones_str), "Ninguna lesion registrada");
    }
    else
    {
        snprintf(lesiones_str, sizeof(lesiones_str), "%d lesion%s", total_lesiones, total_lesiones == 1 ? "" : "es");
    }

    const char *ult_res_texto;
    const char *ult_res_color;
    switch (ult_resultado)
    {
    case 1:
        ult_res_texto = "Victoria";
        ult_res_color = "#2ecc71";
        break;
    case 2:
        ult_res_texto = "Empate";
        ult_res_color = "#f1c40f";
        break;
    case 3:
        ult_res_texto = "Derrota";
        ult_res_color = "#e74c3c";
        break;
    default:
        ult_res_texto = "Sin datos";
        ult_res_color = "#999";
        break;
    }

    char *filepath = get_export_path("resumen_compartible.html");
    FILE *f = NULL;
    fopen_s(&f, filepath, "w");
    if (!f)
    {
        printf("Error al crear el archivo HTML: %s\n", filepath);
        pause_console();
        return;
    }

    fprintf(f,
            "<!DOCTYPE html>\n"
            "<html lang=\"es\">\n"
            "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
            "<title>Resumen de %s - MiFutbolC</title>\n"
            "<meta property=\"og:title\" content=\"Perfil de %s - MiFutbolC\">\n"
            "<meta property=\"og:description\" content=\"%d partidos, %d goles, %d asistencias\">\n"
            "<meta property=\"og:type\" content=\"profile\">\n"
            "<style>\n"
            "  * { margin: 0; padding: 0; box-sizing: border-box; }\n"
            "  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n"
            "         background: linear-gradient(135deg, #1a1a2e 0%%, #16213e 50%%, #0f3460 100%%);\n"
            "         color: #e0e0e0; min-height: 100vh; padding: 20px; }\n"
            "  .container { max-width: 700px; margin: 0 auto; }\n"
            "  @keyframes fadeIn { from { opacity: 0; transform: translateY(15px); }\n"
            "                      to { opacity: 1; transform: translateY(0); } }\n"
            "  .animate { animation: fadeIn 0.5s ease-out forwards; opacity: 0; }\n"
            "  .d1 { animation-delay: 0.05s; } .d2 { animation-delay: 0.10s; }\n"
            "  .d3 { animation-delay: 0.15s; } .d4 { animation-delay: 0.20s; }\n"
            "  .d5 { animation-delay: 0.25s; } .d6 { animation-delay: 0.30s; }\n"
            "  .d7 { animation-delay: 0.35s; } .d8 { animation-delay: 0.40s; }\n"
            "  .d9 { animation-delay: 0.45s; } .d10 { animation-delay: 0.50s; }\n"
            "  .header { text-align: center; padding: 30px 20px; margin-bottom: 25px;\n"
            "            background: rgba(255,255,255,0.05); border-radius: 16px;\n"
            "            border: 1px solid rgba(255,255,255,0.1); }\n"
            "  .header h1 { font-size: 1.8em; color: #e94560; margin-bottom: 5px; }\n"
            "  .header .subtitle { color: #aaa; font-size: 0.95em; }\n"
            "  .header .fecha { color: #777; font-size: 0.8em; margin-top: 8px; }\n"
            "  .stats-grid { display: grid; grid-template-columns: repeat(3, 1fr);\n"
            "                gap: 15px; margin-bottom: 25px; }\n"
            "  .stat-card { background: rgba(255,255,255,0.07); border-radius: 12px;\n"
            "               padding: 20px 10px; text-align: center;\n"
            "               border: 1px solid rgba(255,255,255,0.08);\n"
            "               transition: transform 0.2s, box-shadow 0.2s; cursor: default; }\n"
            "  .stat-card:hover { transform: translateY(-3px);\n"
            "                     box-shadow: 0 6px 20px rgba(233,69,96,0.2); }\n"
            "  .stat-card .icon { font-size: 1.6em; margin-bottom: 4px; }\n"
            "  .stat-card .value { font-size: 2.2em; font-weight: bold; color: #e94560;\n"
            "                      line-height: 1.2; }\n"
            "  .stat-card .label { font-size: 0.8em; color: #999; margin-top: 6px;\n"
            "                      text-transform: uppercase; letter-spacing: 0.5px; }\n"
            "  .section { background: rgba(255,255,255,0.05); border-radius: 12px;\n"
            "             padding: 20px; margin-bottom: 20px;\n"
            "             border: 1px solid rgba(255,255,255,0.08);\n"
            "             transition: transform 0.2s; }\n"
            "  .section:hover { transform: translateY(-2px); }\n"
            "  .section h2 { font-size: 1.1em; color: #e94560; margin-bottom: 15px;\n"
            "                border-bottom: 1px solid rgba(233,69,96,0.3); padding-bottom: 8px; }\n"
            "  .detail-row { display: flex; justify-content: space-between;\n"
            "                padding: 8px 0; border-bottom: 1px solid rgba(255,255,255,0.05); }\n"
            "  .detail-row:last-child { border-bottom: none; }\n"
            "  .detail-label { color: #aaa; }\n"
            "  .detail-value { color: #fff; font-weight: 500; }\n"
            "  .racha-box { text-align: center; padding: 10px 0; }\n"
            "  .racha-badge { display: inline-block; padding: 8px 20px; border-radius: 20px;\n"
            "                 font-weight: bold; font-size: 1.1em; }\n"
            "  .racha-positiva { background: rgba(46,204,113,0.2); color: #2ecc71;\n"
            "                    border: 1px solid rgba(46,204,113,0.4); }\n"
            "  .racha-negativa { background: rgba(231,76,60,0.2); color: #e74c3c;\n"
            "                    border: 1px solid rgba(231,76,60,0.4); }\n"
            "  .racha-neutral  { background: rgba(241,196,15,0.2); color: #f1c40f;\n"
            "                    border: 1px solid rgba(241,196,15,0.4); }\n"
            "  .racha-desc { color: #999; font-size: 0.85em; margin-top: 8px; font-style: italic; }\n"
            "  .ratio-bar { display: flex; border-radius: 8px; overflow: hidden;\n"
            "               height: 32px; margin: 10px 0; }\n"
            "  .ratio-v { background: #2ecc71; display: flex; align-items: center;\n"
            "             justify-content: center; font-size: 0.8em; font-weight: bold; color: #fff; }\n"
            "  .ratio-e { background: #f1c40f; display: flex; align-items: center;\n"
            "             justify-content: center; font-size: 0.8em; font-weight: bold; color: #333; }\n"
            "  .ratio-d { background: #e74c3c; display: flex; align-items: center;\n"
            "             justify-content: center; font-size: 0.8em; font-weight: bold; color: #fff; }\n"
            "  .ratio-legend { display: flex; justify-content: center; gap: 15px;\n"
            "                  font-size: 0.8em; color: #999; margin-top: 5px; }\n"
            "  .ratio-legend span { display: flex; align-items: center; gap: 4px; }\n"
            "  .dot { width: 8px; height: 8px; border-radius: 50%%; display: inline-block; }\n"
            "  .dot-v { background: #2ecc71; } .dot-e { background: #f1c40f; }\n"
            "  .dot-d { background: #e74c3c; }\n"
            "  .ultimo-resultado { text-align: center; padding: 12px 0; }\n"
            "  .ultimo-badge { display: inline-block; padding: 6px 18px; border-radius: 16px;\n"
            "                  font-weight: bold; font-size: 0.95em; }\n"
            "  .logros-info { text-align: center; padding: 10px 0; }\n"
            "  .logros-count { font-size: 2em; font-weight: bold; color: #e94560; }\n"
            "  .logros-label { color: #999; font-size: 0.85em; margin-top: 4px; }\n"
            "  .share-btn { display: inline-block; padding: 10px 24px; border-radius: 8px;\n"
            "               background: linear-gradient(135deg, #e94560, #c23152);\n"
            "               color: #fff; text-decoration: none; font-weight: bold;\n"
            "               font-size: 0.9em; transition: opacity 0.2s; margin-top: 10px; }\n"
            "  .share-btn:hover { opacity: 0.85; }\n"
            "  .footer { text-align: center; margin-top: 25px; padding: 15px;\n"
            "            color: #555; font-size: 0.75em; }\n"
            "  .footer span { color: #e94560; }\n"
            "</style>\n"
            "</head>\n",
            jugador, jugador, total_partidos, total_goles, total_asistencias);

    fprintf(f,
            "<body>\n"
            "<div class=\"container\">\n"
            "\n"
            "<div class=\"header animate d1\">\n"
            "  <h1>&#9917; %s</h1>\n"
            "  <div class=\"subtitle\">Mi perfil futbolistico en MiFutbolC</div>\n"
            "  <div class=\"fecha\">Generado: %s</div>\n"
            "</div>\n"
            "\n"
            "<div class=\"stats-grid\">\n"
            "  <div class=\"stat-card animate d2\">\n"
            "    <div class=\"icon\">&#9917;</div>\n"
            "    <div class=\"value\">%d</div>\n"
            "    <div class=\"label\">Partidos</div>\n"
            "  </div>\n"
            "  <div class=\"stat-card animate d3\">\n"
            "    <div class=\"icon\">&#9918;</div>\n"
            "    <div class=\"value\">%d</div>\n"
            "    <div class=\"label\">Goles</div>\n"
            "  </div>\n"
            "  <div class=\"stat-card animate d4\">\n"
            "    <div class=\"icon\">&#127941;</div>\n"
            "    <div class=\"value\">%d</div>\n"
            "    <div class=\"label\">Asistencias</div>\n"
            "  </div>\n"
            "</div>\n",
            jugador, fecha_gen,
            total_partidos, total_goles, total_asistencias);

    fprintf(f,
            "<div class=\"section animate d3\">\n"
            "  <h2>Rendimiento Destacado</h2>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Mejor rendimiento</span>\n"
            "    <span class=\"detail-value\">%d/10</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Fecha del mejor partido</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Promedio goles/partido</span>\n"
            "    <span class=\"detail-value\">%.2f</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Promedio asistencias/partido</span>\n"
            "    <span class=\"detail-value\">%.2f</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Camiseta favorita</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Cancha mas jugada</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Rival mas enfrentado</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Mejor posicion jugada</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "</div>\n",
            mejor_rendimiento, mejor_fecha,
            promedio_goles, promedio_asistencias,
            camiseta_fav, cancha_fav,
            rival_fav, mejor_posicion);

    fprintf(f,
            "<div class=\"section animate d4\">\n"
            "  <h2>Resultados</h2>\n"
            "  <div class=\"ratio-bar\">\n"
            "    <div class=\"ratio-v\" style=\"width: %.1f%%;\">%dV</div>\n"
            "    <div class=\"ratio-e\" style=\"width: %.1f%%;\">%dE</div>\n"
            "    <div class=\"ratio-d\" style=\"width: %.1f%%;\">%dD</div>\n"
            "  </div>\n"
            "  <div class=\"ratio-legend\">\n"
            "    <span><span class=\"dot dot-v\"></span> Victorias</span>\n"
            "    <span><span class=\"dot dot-e\"></span> Empates</span>\n"
            "    <span><span class=\"dot dot-d\"></span> Derrotas</span>\n"
            "  </div>\n"
            "</div>\n",
            total_partidos > 0 ? (double)victorias / total_partidos * 100.0 : 0.0, victorias,
            total_partidos > 0 ? (double)empates / total_partidos * 100.0 : 0.0, empates,
            total_partidos > 0 ? (double)derrotas / total_partidos * 100.0 : 0.0, derrotas);

    fprintf(f,
            "<div class=\"section animate d5\">\n"
            "  <h2>&#127942; Ultimo Partido</h2>\n"
            "  <div class=\"ultimo-resultado\">\n"
            "    <span class=\"ultimo-badge\" style=\"background: %s22; color: %s; border: 1px solid %s55;\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Fecha</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Rival</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Goles</span>\n"
            "    <span class=\"detail-value\">%d</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Asistencias</span>\n"
            "    <span class=\"detail-value\">%d</span>\n"
            "  </div>\n"
            "</div>\n",
            ult_res_color, ult_res_color, ult_res_color, ult_res_texto,
            ultimo_fecha, ultimo_rival, ult_goles, ult_asistencias);

    fprintf(f,
            "<div class=\"section animate d6\">\n"
            "  <h2>Racha Actual</h2>\n"
            "  <div class=\"racha-box\">\n"
            "    <span class=\"racha-badge racha-%s\">%s: %d partido%s consecutivo%s</span>\n"
            "    <div class=\"racha-desc\">%s</div>\n"
            "  </div>\n"
            "</div>\n",
            racha_class,
            racha_texto, racha_longitud,
            (racha_longitud == 1) ? "" : "s",
            (racha_longitud == 1) ? "" : "s",
            racha_desc);

    fprintf(f,
            "<div class=\"section animate d7\">\n"
            "  <h2>&#127941; Perfil de Juego</h2>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Clima frecuente</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Torneo mas participado</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"detail-row\">\n"
            "    <span class=\"detail-label\">Lesiones registradas</span>\n"
            "    <span class=\"detail-value\">%s</span>\n"
            "  </div>\n"
            "</div>\n",
            clima_fav, torneo_fav, lesiones_str);

    fprintf(f,
            "<div class=\"section animate d8\">\n"
            "  <h2>Logros Desbloqueados</h2>\n"
            "  <div class=\"logros-info\">\n"
            "    <div class=\"logros-count\">%d</div>\n"
            "    <div class=\"logros-label\">logros disponibles para desbloquear</div>\n"
            "  </div>\n"
            "</div>\n"
            "\n"
            "<div class=\"section animate d9\" style=\"text-align:center;\">\n"
            "  <a class=\"share-btn\" href=\"mailto:?subject=Mi%%20Perfil%%20Futbolistico&body=Mira%%20mi%%20perfil%%20en%%20MiFutbolC:%%20%d%%20partidos,%%20%d%%20goles,%%20%d%%20asistencias\">&#128231; Compartir por Email</a>\n"
            "</div>\n"
            "\n"
            "<div class=\"footer animate d10\">\n"
            "  Generado por <span>MiFutbolC</span> v4.3 &mdash; %s\n"
            "</div>\n"
            "\n"
            "</div>\n"
            "</body>\n"
            "</html>\n",
            logros_total,
            total_partidos, total_goles, total_asistencias,
            fecha_gen);

    fclose(f);

    ui_printf("\nResumen HTML generado correctamente.\n");
    ui_printf("Archivo: %s\n\n", filepath);
    mostrar_detalle_racha();
    app_log_event("RESUMEN_COMPARTIBLE", "Resumen HTML generado");
    pause_console();
}

void generar_resumen_markdown(void)
{
    clear_screen();
    print_header("RESUMEN MARKDOWN COMPARTIBLE");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    char jugador[128];
    obtener_nombre_jugador(jugador, sizeof(jugador));

    int total_partidos = contar_total_partidos();
    int total_goles = contar_total_goles();
    int total_asistencias = contar_total_asistencias();

    sqlite3_stmt *stmt;
    int mejor_rendimiento = 0;
    char mejor_fecha[64] = "N/A";

    if (preparar_stmt(
                "SELECT rendimiento_general, fecha_hora FROM partido "
                "ORDER BY rendimiento_general DESC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            mejor_rendimiento = sqlite3_column_int(stmt, 0);
            const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
            if (fecha_raw)
            {
                snprintf(mejor_fecha, sizeof(mejor_fecha), "%s", fecha_raw);
            }
        }
        sqlite3_finalize(stmt);
    }

    char tipo_racha;
    int racha_longitud = calcular_racha_actual(&tipo_racha);
    const char *racha_texto;
    const char *racha_desc;
    switch (tipo_racha)
    {
    case 'P':
        racha_texto = "Positiva";
        racha_desc = "Rendimiento destacado en los ultimos partidos";
        break;
    case 'N':
        racha_texto = "Negativa";
        racha_desc = "Momento dificil, pero siempre se puede dar la vuelta";
        break;
    default:
        racha_texto = "Neutral";
        racha_desc = "Resultados mixtos en los ultimos partidos";
        break;
    }

    int logros_total = 0;
    int logros_completados = 0;
    obtener_progreso_logros(&logros_completados, &logros_total);

    char fecha_gen[64];
    get_datetime(fecha_gen, sizeof(fecha_gen));

    char camiseta_fav[128];
    obtener_camiseta_favorita(camiseta_fav, sizeof(camiseta_fav));

    char cancha_fav[128];
    obtener_cancha_favorita(cancha_fav, sizeof(cancha_fav));

    double promedio_goles = total_partidos > 0 ? (double)total_goles / total_partidos : 0.0;
    double promedio_asistencias =
        total_partidos > 0 ? (double)total_asistencias / total_partidos : 0.0;

    char rival_fav[128];
    obtener_rival_favorito(rival_fav, sizeof(rival_fav));

    int victorias = 0;
    int empates = 0;
    int derrotas = 0;
    contar_resultados(&victorias, &empates, &derrotas);

    char mejor_posicion[128];
    obtener_mejor_posicion(mejor_posicion, sizeof(mejor_posicion));

    char ultimo_fecha[64];
    char ultimo_rival[128];
    int ult_goles = 0;
    int ult_asistencias = 0;
    int ult_resultado = 0;
    obtener_ultimo_partido(ultimo_fecha, sizeof(ultimo_fecha),
                           ultimo_rival, sizeof(ultimo_rival),
                           &ult_goles, &ult_asistencias, &ult_resultado);

    char clima_fav[128];
    obtener_clima_favorito(clima_fav, sizeof(clima_fav));

    char torneo_fav[128];
    obtener_torneo_favorito(torneo_fav, sizeof(torneo_fav));

    int total_lesiones = contar_lesiones();
    char lesiones_str[32];
    if (total_lesiones == 0)
    {
        snprintf(lesiones_str, sizeof(lesiones_str), "Ninguna lesion registrada");
    }
    else
    {
        snprintf(lesiones_str, sizeof(lesiones_str), "%d lesion%s", total_lesiones, total_lesiones == 1 ? "" : "es");
    }

    const char *ult_res_texto;
    switch (ult_resultado)
    {
    case 1:
        ult_res_texto = "Victoria";
        break;
    case 2:
        ult_res_texto = "Empate";
        break;
    case 3:
        ult_res_texto = "Derrota";
        break;
    default:
        ult_res_texto = "Sin datos";
        break;
    }

    char *filepath = get_export_path("resumen_compartible.md");
    FILE *f = NULL;
    fopen_s(&f, filepath, "w");
    if (!f)
    {
        printf("Error al crear el archivo Markdown: %s\n", filepath);
        pause_console();
        return;
    }

    fprintf(f,
            "# &#9917; Perfil Futbolistico de %s\n\n"
            "> Generado por **MiFutbolC** v4.3 &mdash; %s\n\n"
            "---\n\n"
            "## &#128202; Estadisticas Generales\n\n"
            "| Metrica | Valor |\n"
            "|---------|-------|\n"
            "| Partidos jugados | **%d** |\n"
            "| Goles totales | **%d** |\n"
            "| Asistencias totales | **%d** |\n"
            "| Promedio goles/partido | **%.2f** |\n"
            "| Promedio asistencias/partido | **%.2f** |\n\n"
            "---\n\n"
            "## &#127942; Rendimiento Destacado\n\n"
            "| Detalle | Valor |\n"
            "|---------|-------|\n"
            "| Mejor rendimiento | **%d/10** |\n"
            "| Fecha del mejor partido | %s |\n"
            "| Camiseta favorita | %s |\n"
            "| Cancha mas jugada | %s |\n"
            "| Rival mas enfrentado | %s |\n"
            "| Mejor posicion jugada | %s |\n\n"
            "---\n\n"
            "## &#128200; Resultados\n\n"
            "| Resultado | Cantidad | Porcentaje |\n"
            "|-----------|----------|------------|\n"
            "| Victorias | **%d** | %.1f%% |\n"
            "| Empates | **%d** | %.1f%% |\n"
            "| Derrotas | **%d** | %.1f%% |\n\n"
            "---\n\n"
            "## &#127942; Ultimo Partido\n\n"
            "| Detalle | Valor |\n"
            "|---------|-------|\n"
            "| Resultado | **%s** |\n"
            "| Fecha | %s |\n"
            "| Rival | %s |\n"
            "| Goles | %d |\n"
            "| Asistencias | %d |\n\n"
            "---\n\n"
            "## &#128200; Racha Actual\n\n"
            "**%s**: %d partido%s consecutivo%s\n\n"
            "_%s_\n\n"
            "---\n\n"
            "## &#127941; Perfil de Juego\n\n"
            "| Detalle | Valor |\n"
            "|---------|-------|\n"
            "| Clima frecuente | %s |\n"
            "| Torneo mas participado | %s |\n"
            "| Lesiones registradas | **%s** |\n\n"
            "---\n\n"
            "## &#127919; Logros Desbloqueados\n\n"
            "**%d** logros disponibles para desbloquear\n\n"
            "---\n\n"
            "*Generado el %s*\n",
            jugador,
            fecha_gen,
            total_partidos, total_goles, total_asistencias,
            promedio_goles, promedio_asistencias,
            mejor_rendimiento,
            mejor_fecha,
            camiseta_fav, cancha_fav,
            rival_fav, mejor_posicion,
            victorias, total_partidos > 0 ? (double)victorias / total_partidos * 100.0 : 0.0,
            empates, total_partidos > 0 ? (double)empates / total_partidos * 100.0 : 0.0,
            derrotas, total_partidos > 0 ? (double)derrotas / total_partidos * 100.0 : 0.0,
            ult_res_texto, ultimo_fecha, ultimo_rival, ult_goles, ult_asistencias,
            racha_texto, racha_longitud,
            (racha_longitud == 1) ? "" : "s",
            (racha_longitud == 1) ? "" : "s",
            racha_desc,
            clima_fav, torneo_fav, lesiones_str,
            logros_total,
            fecha_gen);

    fclose(f);

    ui_printf("\nResumen Markdown generado correctamente.\n");
    ui_printf("Archivo: %s\n\n", filepath);
    mostrar_detalle_racha();
    app_log_event("RESUMEN_COMPARTIBLE", "Resumen Markdown generado");
    pause_console();
}

void generar_resumen_estadisticas(void)
{
    clear_screen();
    print_header("ESTADISTICAS DESTACADAS");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;

    /* Mejor partido */
    ui_printf("\n=== MEJOR PARTIDO ===\n");
    if (preparar_stmt(
                "SELECT p.fecha_hora, c.nombre, p.goles, p.asistencias, p.rendimiento_general "
                "FROM partido p "
                "JOIN camiseta c ON p.camiseta_id = c.id "
                "ORDER BY p.rendimiento_general DESC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ui_printf("Fecha: %s\n", sqlite3_column_text(stmt, 0));
            ui_printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
            ui_printf("Goles: %d | Asistencias: %d\n",
                      sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3));
            ui_printf("Rendimiento: %d/10\n", sqlite3_column_int(stmt, 4));
        }
        else
        {
            mostrar_no_hay_registros("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }

    /* Peor partido */
    ui_printf("\n=== PEOR PARTIDO ===\n");
    if (preparar_stmt(
                "SELECT p.fecha_hora, c.nombre, p.goles, p.asistencias, p.rendimiento_general "
                "FROM partido p "
                "JOIN camiseta c ON p.camiseta_id = c.id "
                "ORDER BY p.rendimiento_general ASC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ui_printf("Fecha: %s\n", sqlite3_column_text(stmt, 0));
            ui_printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
            ui_printf("Goles: %d | Asistencias: %d\n",
                      sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3));
            ui_printf("Rendimiento: %d/10\n", sqlite3_column_int(stmt, 4));
        }
        else
        {
            mostrar_no_hay_registros("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }

    /* Promedios generales */
    ui_printf("\n=== PROMEDIOS GENERALES ===\n");
    if (preparar_stmt(
                "SELECT "
                "ROUND(AVG(goles), 2), "
                "ROUND(AVG(asistencias), 2), "
                "ROUND(AVG(rendimiento_general), 2), "
                "ROUND(AVG(cansancio), 2), "
                "ROUND(AVG(estado_animo), 2), "
                "COUNT(*) "
                "FROM partido;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ui_printf("Goles promedio: %.2f\n", sqlite3_column_double(stmt, 0));
            ui_printf("Asistencias promedio: %.2f\n", sqlite3_column_double(stmt, 1));
            ui_printf("Rendimiento promedio: %.2f/10\n", sqlite3_column_double(stmt, 2));
            ui_printf("Cansancio promedio: %.2f/10\n", sqlite3_column_double(stmt, 3));
            ui_printf("Estado de animo promedio: %.2f/10\n", sqlite3_column_double(stmt, 4));
            ui_printf("Total de partidos: %d\n", sqlite3_column_int(stmt, 5));
        }
        sqlite3_finalize(stmt);
    }

    /* Goles y asistencias totales */
    ui_printf("\n=== TOTALES ===\n");
    ui_printf("Goles totales: %d\n", contar_total_goles());
    ui_printf("Asistencias totales: %d\n", contar_total_asistencias());
    ui_printf("Partidos totales: %d\n", contar_total_partidos());

    /* Camiseta favorita */
    char camiseta_fav[128];
    obtener_camiseta_favorita(camiseta_fav, sizeof(camiseta_fav));
    ui_printf("Camiseta mas usada: %s\n", camiseta_fav);

    /* Cancha favorita */
    char cancha_fav[128];
    obtener_cancha_favorita(cancha_fav, sizeof(cancha_fav));
    ui_printf("Cancha mas jugada: %s\n", cancha_fav);

    ui_printf("\n");
    app_log_event("RESUMEN_COMPARTIBLE", "Estadisticas destacadas mostradas");
    pause_console();
}

static void mostrar_mejor_temporada_fallback(void)
{
    sqlite3_stmt *stmt;

    ui_printf("\n=== MEJOR TEMPORADA POR RENDIMIENTO ===\n\n");

    if (!preparar_stmt(
                "SELECT substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) as anio, "
                "ROUND(AVG(p.rendimiento_general), 2) as rendimiento, "
                "COUNT(*) as partidos, "
                "SUM(p.goles) as goles, "
                "SUM(p.asistencias) as asistencias "
                "FROM partido p "
                "WHERE p.fecha_hora IS NOT NULL "
                "GROUP BY anio "
                "ORDER BY rendimiento DESC LIMIT 1;",
                &stmt))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *anio = (const char *)sqlite3_column_text(stmt, 0);
        double rendimiento = sqlite3_column_double(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int goles = sqlite3_column_int(stmt, 3);
        int asistencias = sqlite3_column_int(stmt, 4);

        ui_printf("Anio: %s\n", anio ? anio : "Desconocido");
        ui_printf("Rendimiento promedio: %.2f/10\n", rendimiento);
        ui_printf("Partidos jugados: %d\n", partidos);
        ui_printf("Goles totales: %d\n", goles);
        ui_printf("Asistencias totales: %d\n", asistencias);
    }
    else
    {
        mostrar_no_hay_registros("temporadas con datos");
    }
    sqlite3_finalize(stmt);
}

static void mostrar_mejor_temporada_victorias(void)
{
    sqlite3_stmt *stmt;

    ui_printf("\n=== MEJOR TEMPORADA POR VICTORIAS ===\n\n");

    if (!preparar_stmt(
                "SELECT t.nombre, "
                "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                "COUNT(*) as total "
                "FROM partido p "
                "JOIN temporada t ON substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) = t.nombre "
                "WHERE p.fecha_hora IS NOT NULL "
                "GROUP BY t.nombre "
                "ORDER BY victorias DESC LIMIT 1;",
                &stmt))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre_temp = (const char *)sqlite3_column_text(stmt, 0);
        int victorias = sqlite3_column_int(stmt, 1);
        int total = sqlite3_column_int(stmt, 2);

        ui_printf("Temporada: %s\n", nombre_temp ? nombre_temp : "Sin nombre");
        ui_printf("Victorias: %d de %d partidos\n", victorias, total);
        double porcentaje = total > 0 ? ((double)victorias / total) * 100.0 : 0.0;
        ui_printf("Porcentaje de victorias: %.1f%%\n", porcentaje);
    }
    else
    {
        mostrar_no_hay_registros("temporadas con victorias");
    }
    sqlite3_finalize(stmt);
}

void generar_resumen_mejor_temporada(void)
{
    clear_screen();
    print_header("MEJOR TEMPORADA");

    sqlite3_stmt *stmt;

    if (preparar_stmt(
                "SELECT t.nombre, tr.total_partidos, tr.total_goles, "
                "tr.promedio_goles_partido, "
                "e.nombre as campeon, "
                "tr.total_lesiones "
                "FROM temporada_resumen tr "
                "JOIN temporada t ON tr.temporada_id = t.id "
                "LEFT JOIN equipo e ON tr.equipo_campeon_id = e.id "
                "ORDER BY tr.total_goles DESC LIMIT 1;",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre_temp = (const char *)sqlite3_column_text(stmt, 0);
            int partidos = sqlite3_column_int(stmt, 1);
            int goles = sqlite3_column_int(stmt, 2);
            double promedio = sqlite3_column_double(stmt, 3);
            const char *campeon = (const char *)sqlite3_column_text(stmt, 4);
            int lesiones = sqlite3_column_int(stmt, 5);

            ui_printf("\n=== MEJOR TEMPORADA POR GOLES ===\n\n");
            ui_printf("Temporada: %s\n", nombre_temp ? nombre_temp : "Sin nombre");
            ui_printf("Partidos jugados: %d\n", partidos);
            ui_printf("Total de goles: %d\n", goles);
            ui_printf("Promedio goles/partido: %.2f\n", promedio);
            ui_printf("Equipo campeon: %s\n", campeon ? campeon : "No determinado");
            ui_printf("Lesiones: %d\n", lesiones);
            sqlite3_finalize(stmt);
        }
        else
        {
            sqlite3_finalize(stmt);
            mostrar_mejor_temporada_fallback();
        }
    }

    mostrar_mejor_temporada_victorias();

    ui_printf("\n");
    app_log_event("RESUMEN_COMPARTIBLE", "Mejor temporada mostrada");
    pause_console();
}
