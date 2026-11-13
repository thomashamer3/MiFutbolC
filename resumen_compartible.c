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
#include "pdfgen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static void menu_resumen_anio(void);

void menu_resumen_compartible(void)
{
    app_log_event("RESUMEN_COMPARTIBLE", "Ingreso al modulo Resumen Compartible");

    MenuItem items[] =
    {
        {1, "Generar Resumen HTML", generar_resumen_html},
        {2, "Generar Resumen Markdown", generar_resumen_markdown},
        {3, "Estadisticas Destacadas", generar_resumen_estadisticas},
        {4, "Mejor Temporada", generar_resumen_mejor_temporada},
        {5, "Resumen Anual", menu_resumen_anio},
        {0, "Volver", NULL}
    };
    ejecutar_menu("RESUMEN COMPARTIBLE", items, 6);
}

/*
 * Prepara los datos del perfil una sola vez y escribe el resumen en el formato
 * pedido: Markdown cuando es_markdown es distinto de 0, HTML en caso contrario.
 * Los dos formatos comparten toda la preparacion de datos.
 */
typedef struct
{
    const char *texto;
    const char *desc;
    const char *clase;
    int longitud;
} ResumenRacha;

typedef struct
{
    int victorias;
    int empates;
    int derrotas;
} ResumenResultados;

typedef struct
{
    int rendimiento;
    char fecha[64];
} ResumenMejorPartido;

typedef struct
{
    char camiseta[128];
    char cancha[128];
    char rival[128];
    char posicion[128];
} ResumenFavoritos;

typedef struct
{
    char fecha[64];
    char rival[128];
    int goles;
    int asistencias;
    const char *texto;
    const char *color;
} ResumenUltimoPartido;

typedef struct
{
    char clima[128];
    char torneo[128];
    char lesiones[32];
} ResumenPerfilJuego;

typedef struct
{
    char jugador[128];
    int total_partidos;
    int total_goles;
    int total_asistencias;
    double promedio_goles;
    double promedio_asistencias;
    char fecha_gen[64];
    int logros_total;
    ResumenMejorPartido mejor;
    ResumenRacha racha;
    ResumenFavoritos favoritos;
    ResumenResultados resultados;
    ResumenUltimoPartido ultimo;
    ResumenPerfilJuego perfil;
} DatosResumen;

static void resumen_describir_racha(char tipo, const char **texto, const char **desc,
                                    const char **clase)
{
    switch (tipo)
    {
    case 'P':
        *texto = "Positiva";
        *desc = "Rendimiento destacado en los ultimos partidos";
        *clase = "positiva";
        break;
    case 'N':
        *texto = "Negativa";
        *desc = "Momento dificil, pero siempre se puede dar la vuelta";
        *clase = "negativa";
        break;
    default:
        *texto = "Neutral";
        *desc = "Resultados mixtos en los ultimos partidos";
        *clase = "neutral";
        break;
    }
}

static void resumen_describir_resultado(int resultado, const char **texto, const char **color)
{
    switch (resultado)
    {
    case 1:
        *texto = "Victoria";
        *color = "#2ecc71";
        break;
    case 2:
        *texto = "Empate";
        *color = "#f1c40f";
        break;
    case 3:
        *texto = "Derrota";
        *color = "#e74c3c";
        break;
    default:
        *texto = "Sin datos";
        *color = "#999";
        break;
    }
}

static void resumen_obtener_mejor_rendimiento(int *rendimiento, char *fecha, size_t fecha_size)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt("SELECT rendimiento_general, fecha_hora FROM partido "
                       "ORDER BY rendimiento_general DESC LIMIT 1;", &stmt))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *rendimiento = sqlite3_column_int(stmt, 0);
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
        if (fecha_raw)
        {
            snprintf(fecha, fecha_size, "%s", fecha_raw);
        }
    }
    sqlite3_finalize(stmt);
}

static void resumen_formatear_lesiones(int total, char *buffer, size_t size)
{
    if (total == 0)
    {
        snprintf(buffer, size, "Ninguna lesion registrada");
    }
    else
    {
        snprintf(buffer, size, "%d lesion%s", total, total == 1 ? "" : "es");
    }
}

static void recolectar_datos_resumen(DatosResumen *d)
{
    memset(d, 0, sizeof(*d));
    snprintf(d->mejor.fecha, sizeof(d->mejor.fecha), "N/A");

    obtener_nombre_jugador(d->jugador, sizeof(d->jugador));
    d->total_partidos = contar_total_partidos();
    d->total_goles = contar_total_goles();
    d->total_asistencias = contar_total_asistencias();

    resumen_obtener_mejor_rendimiento(&d->mejor.rendimiento, d->mejor.fecha,
                                      sizeof(d->mejor.fecha));

    char tipo_racha;
    d->racha.longitud = calcular_racha_actual(&tipo_racha);
    resumen_describir_racha(tipo_racha, &d->racha.texto, &d->racha.desc, &d->racha.clase);

    int logros_completados = 0;
    obtener_progreso_logros(&logros_completados, &d->logros_total);

    get_datetime(d->fecha_gen, sizeof(d->fecha_gen));
    obtener_camiseta_favorita(d->favoritos.camiseta, sizeof(d->favoritos.camiseta));
    obtener_cancha_favorita(d->favoritos.cancha, sizeof(d->favoritos.cancha));

    d->promedio_goles = d->total_partidos > 0 ? (double)d->total_goles / d->total_partidos : 0.0;
    d->promedio_asistencias =
        d->total_partidos > 0 ? (double)d->total_asistencias / d->total_partidos : 0.0;

    obtener_rival_favorito(d->favoritos.rival, sizeof(d->favoritos.rival));
    contar_resultados(&d->resultados.victorias, &d->resultados.empates, &d->resultados.derrotas);
    obtener_mejor_posicion(d->favoritos.posicion, sizeof(d->favoritos.posicion));

    int ult_resultado = 0;
    obtener_ultimo_partido(d->ultimo.fecha, sizeof(d->ultimo.fecha),
                           d->ultimo.rival, sizeof(d->ultimo.rival),
                           &d->ultimo.goles, &d->ultimo.asistencias, &ult_resultado);
    resumen_describir_resultado(ult_resultado, &d->ultimo.texto, &d->ultimo.color);

    obtener_clima_favorito(d->perfil.clima, sizeof(d->perfil.clima));
    obtener_torneo_favorito(d->perfil.torneo, sizeof(d->perfil.torneo));
    resumen_formatear_lesiones(contar_lesiones(), d->perfil.lesiones, sizeof(d->perfil.lesiones));
}

static void resumen_escribir_markdown(FILE *f, const DatosResumen *d)
{
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
            d->jugador,
            d->fecha_gen,
            d->total_partidos, d->total_goles, d->total_asistencias,
            d->promedio_goles, d->promedio_asistencias,
            d->mejor.rendimiento,
            d->mejor.fecha,
            d->favoritos.camiseta, d->favoritos.cancha,
            d->favoritos.rival, d->favoritos.posicion,
            d->resultados.victorias,
            d->total_partidos > 0 ? (double)d->resultados.victorias / d->total_partidos * 100.0 : 0.0,
            d->resultados.empates,
            d->total_partidos > 0 ? (double)d->resultados.empates / d->total_partidos * 100.0 : 0.0,
            d->resultados.derrotas,
            d->total_partidos > 0 ? (double)d->resultados.derrotas / d->total_partidos * 100.0 : 0.0,
            d->ultimo.texto, d->ultimo.fecha, d->ultimo.rival, d->ultimo.goles, d->ultimo.asistencias,
            d->racha.texto, d->racha.longitud,
            (d->racha.longitud == 1) ? "" : "s",
            (d->racha.longitud == 1) ? "" : "s",
            d->racha.desc,
            d->perfil.clima, d->perfil.torneo, d->perfil.lesiones,
            d->logros_total,
            d->fecha_gen);
}

static void resumen_escribir_html(FILE *f, const DatosResumen *d)
{
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
            d->jugador, d->jugador, d->total_partidos, d->total_goles, d->total_asistencias);

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
            d->jugador, d->fecha_gen,
            d->total_partidos, d->total_goles, d->total_asistencias);

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
            d->mejor.rendimiento, d->mejor.fecha,
            d->promedio_goles, d->promedio_asistencias,
            d->favoritos.camiseta, d->favoritos.cancha,
            d->favoritos.rival, d->favoritos.posicion);

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
            d->total_partidos > 0 ? (double)d->resultados.victorias / d->total_partidos * 100.0 : 0.0,
            d->resultados.victorias,
            d->total_partidos > 0 ? (double)d->resultados.empates / d->total_partidos * 100.0 : 0.0,
            d->resultados.empates,
            d->total_partidos > 0 ? (double)d->resultados.derrotas / d->total_partidos * 100.0 : 0.0,
            d->resultados.derrotas);

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
            d->ultimo.color, d->ultimo.color, d->ultimo.color, d->ultimo.texto,
            d->ultimo.fecha, d->ultimo.rival, d->ultimo.goles, d->ultimo.asistencias);

    fprintf(f,
            "<div class=\"section animate d6\">\n"
            "  <h2>Racha Actual</h2>\n"
            "  <div class=\"racha-box\">\n"
            "    <span class=\"racha-badge racha-%s\">%s: %d partido%s consecutivo%s</span>\n"
            "    <div class=\"racha-desc\">%s</div>\n"
            "  </div>\n"
            "</div>\n",
            d->racha.clase,
            d->racha.texto, d->racha.longitud,
            (d->racha.longitud == 1) ? "" : "s",
            (d->racha.longitud == 1) ? "" : "s",
            d->racha.desc);

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
            d->perfil.clima, d->perfil.torneo, d->perfil.lesiones);

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
            d->logros_total,
            d->total_partidos, d->total_goles, d->total_asistencias,
            d->fecha_gen);
}

static void generar_resumen_formato(int es_markdown)
{
    clear_screen();
    print_header(es_markdown ? "RESUMEN MARKDOWN COMPARTIBLE" : "RESUMEN HTML COMPARTIBLE");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    DatosResumen d;
    recolectar_datos_resumen(&d);

    const char *nombre_archivo = es_markdown ? "resumen_compartible.md" : "resumen_compartible.html";
    char *filepath = get_export_path(nombre_archivo);
    FILE *f = NULL;
    fopen_s(&f, filepath, "w");
    if (!f)
    {
        printf("Error al crear el archivo %s: %s\n", es_markdown ? "Markdown" : "HTML", filepath);
        pause_console();
        return;
    }

    if (es_markdown)
    {
        resumen_escribir_markdown(f, &d);
        ui_printf("\nResumen Markdown generado correctamente.\n");
    }
    else
    {
        resumen_escribir_html(f, &d);
        ui_printf("\nResumen HTML generado correctamente.\n");
    }

    fclose(f);
    ui_printf("Archivo: %s\n\n", filepath);
    mostrar_detalle_racha();
    app_log_event("RESUMEN_COMPARTIBLE",
                  es_markdown ? "Resumen Markdown generado" : "Resumen HTML generado");
    pause_console();
}

void generar_resumen_html(void)
{
    generar_resumen_formato(0);
}

void generar_resumen_markdown(void)
{
    generar_resumen_formato(1);
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

/* ─── Resumen del año ─────────────────────────────────────────────────────── */

/*
 * Reune las estadisticas del año en curso y las exporta a TXT, HTML y PDF.
 * El filtro por año se hace en C para aceptar tanto fechas ISO (AAAA-MM-DD)
 * como fechas locales (DD/MM/AAAA), que conviven en la tabla partido.
 */

#define RESUMEN_ANIO_MESES 12
#define RESUMEN_ANIO_MAX_GRUPOS 64

/**
 * @brief Datos completos del mejor partido del año.
 *
 * Se cargan bajo demanda con resumen_anio_cargar_partido() para no repetir
 * todas las columnas de partido durante el recorrido anual.
 */
typedef struct
{
    int id;
    char fecha_hora[64];
    char rival[128];
    char tipo_rival[64];
    int resultado;
    int goles;
    int asistencias;
    char goles_detalle[512];
    char asistencias_detalle[512];
    int goles_equipo;
    int goles_rival;
    int diferencia_gol;
    int rendimiento_general;
    int rating_tecnico;
    int rating_fisico;
    int rating_mental;
    int minutos_jugados;
    int intensidad;
    int esfuerzo_percibido;
    int cansancio;
    int estado_animo;
    int dolor_fisico;
    char posicion_jugada[64];
    char formato_partido[64];
    int tipo_partido;
    int tarjeta;
    char cancha[128];
    char camiseta[128];
    int clima;
    int tiene_temperatura;
    double temperatura_c;
    int tiene_precipitacion;
    double precip_mm;
    int tiene_viento;
    double wind_kmh;
    int tiene_sensacion;
    double apparent_temp_c;
    char condicion_cancha[64];
    int estado_cancha;
    char arbitraje[64];
    int arbitraje_score;
    char comentario[512];
    char lo_mejor[512];
    char que_mejorar[512];
    char tags[256];
    char eventos_clave[512];
} ResumenAnioPartido;

typedef struct
{
    int anio;
    char jugador[128];
    char fecha_gen[64];
    int partidos;
    int goles;
    int asistencias;
    int victorias;
    int empates;
    int derrotas;
    int goles_favor;
    int goles_contra;
    int partidos_con_marcador;
    int mejor_id;
    ResumenAnioPartido mejor;
    int mes_partidos[RESUMEN_ANIO_MESES];
    int mes_goles[RESUMEN_ANIO_MESES];
    int mes_asistencias[RESUMEN_ANIO_MESES];
    int mes_victorias[RESUMEN_ANIO_MESES];
    int mes_empates[RESUMEN_ANIO_MESES];
    int mes_derrotas[RESUMEN_ANIO_MESES];
    int camisetas_activas;
    int camisetas_usadas;
    char camiseta_top[128];
    int camiseta_top_partidos;
    int canchas_activas;
    int canchas_usadas;
    char cancha_top[128];
    int cancha_top_partidos;
    int lesiones;
    int lesiones_activas;
} ResumenAnio;

/**
 * @brief Lee un numero de digitos consecutivos dentro de una fecha.
 * @return Valor leido, o -1 si algun caracter no es un digito.
 */
static int resumen_anio_digitos(const char *texto, size_t inicio, int cantidad)
{
    int valor = 0;
    int i;

    for (i = 0; i < cantidad; i++)
    {
        char c = texto[inicio + (size_t)i];
        if (c < '0' || c > '9')
        {
            return -1;
        }
        valor = valor * 10 + (c - '0');
    }

    return valor;
}

/**
 * @brief Extrae el año de una fecha ISO (AAAA-MM-DD) o local (DD/MM/AAAA).
 * @return Año leido, o -1 si la fecha no tiene un formato reconocible.
 */
static int resumen_anio_desde_fecha(const char *fecha)
{
    size_t largo;

    if (!fecha)
    {
        return -1;
    }

    largo = strlen(fecha);
    if (largo >= 10 && fecha[2] == '/' && fecha[5] == '/')
    {
        return resumen_anio_digitos(fecha, 6, 4);
    }
    if (largo >= 4)
    {
        return resumen_anio_digitos(fecha, 0, 4);
    }

    return -1;
}

/**
 * @brief Extrae el mes (1-12) de una fecha ISO o local.
 * @return Mes leido, o -1 si no se puede determinar.
 */
static int resumen_anio_mes_desde_fecha(const char *fecha)
{
    size_t largo;

    if (!fecha)
    {
        return -1;
    }

    largo = strlen(fecha);
    if (largo >= 10 && fecha[2] == '/' && fecha[5] == '/')
    {
        return resumen_anio_digitos(fecha, 3, 2);
    }
    if (largo >= 7 && fecha[4] == '-')
    {
        return resumen_anio_digitos(fecha, 5, 2);
    }

    return -1;
}

static int resumen_anio_anio_actual(void)
{
    time_t ahora = time(NULL);
    struct tm tm_ahora;

#ifdef _WIN32
    localtime_s(&tm_ahora, &ahora);
#else
    localtime_r(&ahora, &tm_ahora);
#endif

    return tm_ahora.tm_year + 1900;
}

static const char *resumen_anio_mes_nombre(int indice)
{
    static const char *const nombres[RESUMEN_ANIO_MESES] =
    {
        "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
        "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
    };

    if (indice < 0 || indice >= RESUMEN_ANIO_MESES)
    {
        return "Sin mes";
    }

    return nombres[indice];
}

static double resumen_anio_promedio(int parte, int total)
{
    return total > 0 ? (double)parte / (double)total : 0.0;
}

/**
 * @brief Efectividad del año en porcentaje de puntos obtenidos.
 *
 * Se usa la convencion futbolistica: 3 puntos por victoria y 1 por empate,
 * sobre el maximo posible (3 por partido jugado).
 *
 * @param r Resumen recolectado.
 * @return Porcentaje entre 0 y 100, o 0 si no hubo partidos.
 */
static double resumen_anio_efectividad(const ResumenAnio *r)
{
    if (!r || r->partidos <= 0)
    {
        return 0.0;
    }

    return ((double)(r->victorias * 3 + r->empates) /
            ((double)r->partidos * 3.0)) * 100.0;
}

/**
 * @brief Ejecuta un COUNT(*) y devuelve el valor, 0 si falla.
 */
static int resumen_anio_contar(const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    int total = 0;

    if (!sql || !preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return total;
}

/**
 * @brief Busca un nombre por id con la consulta indicada.
 */
static void resumen_anio_nombre_por_id(const char *sql, int id, char *buffer, size_t size)
{
    sqlite3_stmt *stmt = NULL;
    size_t largo;

    snprintf(buffer, size, "Sin datos");
    if (!sql || id < 0 || !preparar_stmt(sql, &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        if (nombre && nombre[0] != '\0')
        {
            snprintf(buffer, size, "%s", nombre);
            /* Recorta espacios finales para que el reporte no muestre dobles separaciones. */
            largo = strlen(buffer);
            while (largo > 0 && (buffer[largo - 1] == ' ' || buffer[largo - 1] == '\t'))
            {
                buffer[--largo] = '\0';
            }
        }
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Devuelve el indice del grupo con ese id, agregandolo si es nuevo.
 * @return Indice dentro de los arreglos, o -1 si ya no cabe otro grupo.
 */
static int resumen_anio_indice_grupo(int *ids, int *cuentas, int *total, int id)
{
    int i;

    for (i = 0; i < *total; i++)
    {
        if (ids[i] == id)
        {
            return i;
        }
    }

    if (*total >= RESUMEN_ANIO_MAX_GRUPOS)
    {
        return -1;
    }

    ids[*total] = id;
    cuentas[*total] = 0;
    (*total)++;
    return *total - 1;
}

static void resumen_anio_top_grupo(const int *ids, const int *cuentas, int total,
                                   int *id_top, int *partidos_top)
{
    int i;

    *id_top = -1;
    *partidos_top = 0;
    for (i = 0; i < total; i++)
    {
        if (cuentas[i] > *partidos_top)
        {
            *id_top = ids[i];
            *partidos_top = cuentas[i];
        }
    }
}

/**
 * @brief Copia una columna de texto de SQLite a un buffer, tolerando NULL.
 */
static void resumen_anio_copiar_texto(char *destino, size_t tam, sqlite3_stmt *stmt, int columna)
{
    const char *valor = (const char *)sqlite3_column_text(stmt, columna);
    size_t largo;

    if (!destino || tam == 0)
    {
        return;
    }

    snprintf(destino, tam, "%s", valor ? valor : "");

    /* La base puede guardar nombres con espacios finales: se recortan aqui. */
    largo = strlen(destino);
    while (largo > 0 && (destino[largo - 1] == ' ' || destino[largo - 1] == '\t'))
    {
        destino[--largo] = '\0';
    }
}

/**
 * @brief Agrega un texto a un buffer de descripcion sin desbordarlo.
 */
static void resumen_anio_append(char *buffer, size_t tam, int *usados, const char *texto)
{
    int escrito;

    if (!buffer || tam == 0 || !usados || !texto || texto[0] == '\0' ||
            *usados < 0 || (size_t)*usados >= tam)
    {
        return;
    }

    if (*usados > 0)
    {
        escrito = snprintf(buffer + *usados, tam - (size_t)*usados, " | ");
        if (escrito > 0)
        {
            *usados += escrito;
        }
    }

    if (*usados < 0 || (size_t)*usados >= tam)
    {
        *usados = (int)tam - 1;
        return;
    }

    escrito = snprintf(buffer + *usados, tam - (size_t)*usados, "%s", texto);
    if (escrito > 0)
    {
        *usados += escrito;
    }

    if (*usados < 0 || (size_t)*usados >= tam)
    {
        *usados = (int)tam - 1;
    }
}

static const char *resumen_anio_tipo_partido_texto(int tipo_partido)
{
    switch (tipo_partido)
    {
    case 2:
        return "Torneo";
    case 3:
        return "Entrenamiento";
    case 1:
    default:
        return "Amistoso";
    }
}

static const char *resumen_anio_tarjeta_texto(int tarjeta)
{
    switch (tarjeta)
    {
    case 2:
        return "Amarilla";
    case 3:
        return "Roja";
    default:
        return "No";
    }
}

static const char *resumen_anio_estado_cancha_texto(int estado_cancha)
{
    switch (estado_cancha)
    {
    case 1:
        return "Excelente";
    case 2:
        return "Buena";
    case 3:
        return "Regular";
    case 4:
        return "Mala";
    case 5:
        return "Pesima";
    default:
        return "No definido";
    }
}

static const char *resumen_anio_dolor_texto(int dolor_fisico)
{
    switch (dolor_fisico)
    {
    case 1:
        return "Leve";
    case 2:
        return "Moderada";
    case 3:
        return "Fuerte";
    default:
        return "Ninguna";
    }
}

static const char *resumen_anio_arbitraje_texto(int arbitraje_score)
{
    switch (arbitraje_score)
    {
    case 1:
        return "Muy malo";
    case 2:
        return "Regular";
    case 3:
        return "Normal";
    case 4:
        return "Bueno";
    case 5:
        return "Excelente";
    default:
        return "";
    }
}

/**
 * @brief Carga todas las columnas de un partido para el detalle del mejor del año.
 *
 * @param id Identificador del partido.
 * @param p Estructura de salida; se limpia antes de cargar.
 */
static void resumen_anio_cargar_partido(int id, ResumenAnioPartido *p)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT p.fecha_hora, IFNULL(p.rival_nombre, ''), IFNULL(p.tipo_rival, ''), "
        "p.resultado, p.goles, p.asistencias, IFNULL(p.goles_detalle, ''), "
        "IFNULL(p.asistencias_detalle, ''), p.rendimiento_general, p.rating_tecnico, "
        "p.rating_fisico, p.rating_mental, p.minutos_jugados, p.intensidad, "
        "p.esfuerzo_percibido, p.cansancio, p.estado_animo, p.dolor_fisico, "
        "IFNULL(p.posicion_jugada, ''), IFNULL(p.formato_partido, ''), p.tipo_partido, "
        "p.tarjeta, IFNULL(can.nombre, ''), IFNULL(cam.nombre, ''), p.clima, "
        "p.temperatura_c, p.precip_mm, p.wind_kmh, p.apparent_temp_c, "
        "IFNULL(p.condicion_cancha, ''), p.estado_cancha, IFNULL(p.arbitraje, ''), "
        "p.arbitraje_score, IFNULL(p.comentario_personal, ''), IFNULL(p.lo_mejor, ''), "
        "IFNULL(p.que_mejorar, ''), IFNULL(p.tags, ''), IFNULL(p.eventos_clave, ''), "
        "p.goles_equipo, p.goles_rival "
        "FROM partido p "
        "LEFT JOIN cancha can ON p.cancha_id = can.id "
        "LEFT JOIN camiseta cam ON p.camiseta_id = cam.id "
        "WHERE p.id = ?;";

    if (!p)
    {
        return;
    }

    memset(p, 0, sizeof(*p));
    p->id = id;
    p->goles_equipo = -1;
    p->goles_rival = -1;

    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        resumen_anio_copiar_texto(p->fecha_hora, sizeof(p->fecha_hora), stmt, 0);
        resumen_anio_copiar_texto(p->rival, sizeof(p->rival), stmt, 1);
        resumen_anio_copiar_texto(p->tipo_rival, sizeof(p->tipo_rival), stmt, 2);
        p->resultado = sqlite3_column_int(stmt, 3);
        p->goles = sqlite3_column_int(stmt, 4);
        p->asistencias = sqlite3_column_int(stmt, 5);
        resumen_anio_copiar_texto(p->goles_detalle, sizeof(p->goles_detalle), stmt, 6);
        resumen_anio_copiar_texto(p->asistencias_detalle, sizeof(p->asistencias_detalle), stmt, 7);
        p->rendimiento_general = sqlite3_column_int(stmt, 8);
        p->rating_tecnico = sqlite3_column_int(stmt, 9);
        p->rating_fisico = sqlite3_column_int(stmt, 10);
        p->rating_mental = sqlite3_column_int(stmt, 11);
        p->minutos_jugados = sqlite3_column_int(stmt, 12);
        p->intensidad = sqlite3_column_int(stmt, 13);
        p->esfuerzo_percibido = sqlite3_column_int(stmt, 14);
        p->cansancio = sqlite3_column_int(stmt, 15);
        p->estado_animo = sqlite3_column_int(stmt, 16);
        p->dolor_fisico = sqlite3_column_int(stmt, 17);
        resumen_anio_copiar_texto(p->posicion_jugada, sizeof(p->posicion_jugada), stmt, 18);
        resumen_anio_copiar_texto(p->formato_partido, sizeof(p->formato_partido), stmt, 19);
        p->tipo_partido = sqlite3_column_int(stmt, 20);
        p->tarjeta = sqlite3_column_int(stmt, 21);
        resumen_anio_copiar_texto(p->cancha, sizeof(p->cancha), stmt, 22);
        resumen_anio_copiar_texto(p->camiseta, sizeof(p->camiseta), stmt, 23);
        p->clima = sqlite3_column_int(stmt, 24);
        p->tiene_temperatura = sqlite3_column_type(stmt, 25) != SQLITE_NULL;
        p->temperatura_c = sqlite3_column_double(stmt, 25);
        p->tiene_precipitacion = sqlite3_column_type(stmt, 26) != SQLITE_NULL;
        p->precip_mm = sqlite3_column_double(stmt, 26);
        p->tiene_viento = sqlite3_column_type(stmt, 27) != SQLITE_NULL;
        p->wind_kmh = sqlite3_column_double(stmt, 27);
        p->tiene_sensacion = sqlite3_column_type(stmt, 28) != SQLITE_NULL;
        p->apparent_temp_c = sqlite3_column_double(stmt, 28);
        resumen_anio_copiar_texto(p->condicion_cancha, sizeof(p->condicion_cancha), stmt, 29);
        p->estado_cancha = sqlite3_column_int(stmt, 30);
        resumen_anio_copiar_texto(p->arbitraje, sizeof(p->arbitraje), stmt, 31);
        p->arbitraje_score = sqlite3_column_int(stmt, 32);
        resumen_anio_copiar_texto(p->comentario, sizeof(p->comentario), stmt, 33);
        resumen_anio_copiar_texto(p->lo_mejor, sizeof(p->lo_mejor), stmt, 34);
        resumen_anio_copiar_texto(p->que_mejorar, sizeof(p->que_mejorar), stmt, 35);
        resumen_anio_copiar_texto(p->tags, sizeof(p->tags), stmt, 36);
        resumen_anio_copiar_texto(p->eventos_clave, sizeof(p->eventos_clave), stmt, 37);
        p->goles_equipo = sqlite3_column_int(stmt, 38);
        p->goles_rival = sqlite3_column_int(stmt, 39);

        if (p->goles_equipo >= 0 && p->goles_rival >= 0)
        {
            p->diferencia_gol = p->goles_equipo - p->goles_rival;
        }
    }

    sqlite3_finalize(stmt);
}

static void resumen_anio_texto_rendimiento(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->rating_tecnico > 0 || p->rating_fisico > 0 || p->rating_mental > 0)
    {
        snprintf(temp, sizeof(temp), "%d/10 (tecnico %d, fisico %d, mental %d)",
                 p->rendimiento_general, p->rating_tecnico, p->rating_fisico, p->rating_mental);
    }
    else
    {
        snprintf(temp, sizeof(temp), "%d/10", p->rendimiento_general);
    }
    resumen_anio_append(buffer, tam, &usados, temp);
}

static void resumen_anio_texto_marcador(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;
    int resultado;

    buffer[0] = '\0';
    if (p->goles_equipo < 0 || p->goles_rival < 0)
    {
        return;
    }

    /* El resultado se deduce del marcador: la columna resultado puede no coincidir. */
    if (p->goles_equipo > p->goles_rival)
    {
        resultado = 1;
    }
    else if (p->goles_equipo == p->goles_rival)
    {
        resultado = 2;
    }
    else
    {
        resultado = 3;
    }

    snprintf(temp, sizeof(temp), "%d - %d (%s)", p->goles_equipo, p->goles_rival,
             resultado_to_text(resultado));
    resumen_anio_append(buffer, tam, &usados, temp);
    snprintf(temp, sizeof(temp), "Diferencia: %+d", p->diferencia_gol);
    resumen_anio_append(buffer, tam, &usados, temp);
}

static void resumen_anio_texto_clima(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->clima >= 1 && p->clima <= 12)
    {
        resumen_anio_append(buffer, tam, &usados, clima_to_text(p->clima));
    }

    if (p->tiene_temperatura)
    {
        snprintf(temp, sizeof(temp), "%.1f C", p->temperatura_c);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->tiene_sensacion)
    {
        snprintf(temp, sizeof(temp), "sensacion %.1f C", p->apparent_temp_c);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->tiene_viento)
    {
        snprintf(temp, sizeof(temp), "viento %.1f km/h", p->wind_kmh);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->tiene_precipitacion)
    {
        snprintf(temp, sizeof(temp), "lluvia %.1f mm", p->precip_mm);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

static void resumen_anio_texto_fisico(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->minutos_jugados > 0)
    {
        snprintf(temp, sizeof(temp), "Duracion %d min", p->minutos_jugados);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->intensidad > 0)
    {
        snprintf(temp, sizeof(temp), "Intensidad %d", p->intensidad);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->esfuerzo_percibido > 0)
    {
        snprintf(temp, sizeof(temp), "Esfuerzo %d", p->esfuerzo_percibido);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->cansancio > 0)
    {
        snprintf(temp, sizeof(temp), "Cansancio %d", p->cansancio);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->estado_animo > 0)
    {
        snprintf(temp, sizeof(temp), "Animo %d", p->estado_animo);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->dolor_fisico > 0)
    {
        snprintf(temp, sizeof(temp), "Dolor %s", resumen_anio_dolor_texto(p->dolor_fisico));
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

static void resumen_anio_texto_contexto(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[128];
    int usados = 0;

    buffer[0] = '\0';
    snprintf(temp, sizeof(temp), "%s", resumen_anio_tipo_partido_texto(p->tipo_partido));
    resumen_anio_append(buffer, tam, &usados, temp);

    if (p->formato_partido[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "Formato %s", p->formato_partido);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->tarjeta > 1)
    {
        snprintf(temp, sizeof(temp), "Tarjeta %s", resumen_anio_tarjeta_texto(p->tarjeta));
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    if (p->posicion_jugada[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "Posicion %s", p->posicion_jugada);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

static void resumen_anio_texto_arbitraje(const ResumenAnioPartido *p, char *buffer, size_t tam)
{
    char temp[96];
    int usados = 0;

    buffer[0] = '\0';
    if (p->arbitraje[0] != '\0' && p->arbitraje_score > 0)
    {
        snprintf(temp, sizeof(temp), "%s (%d/5)", p->arbitraje, p->arbitraje_score);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
    else if (p->arbitraje[0] != '\0')
    {
        resumen_anio_append(buffer, tam, &usados, p->arbitraje);
    }
    else if (p->arbitraje_score > 0)
    {
        snprintf(temp, sizeof(temp), "%s (%d/5)", resumen_anio_arbitraje_texto(p->arbitraje_score),
                 p->arbitraje_score);
        resumen_anio_append(buffer, tam, &usados, temp);
    }
}

/**
 * @brief Recorre los partidos del año y acumula totales, meses y favoritos.
 */
static void resumen_anio_leer_partidos(ResumenAnio *r)
{
    sqlite3_stmt *stmt = NULL;
    int ids_cancha[RESUMEN_ANIO_MAX_GRUPOS];
    int cuentas_cancha[RESUMEN_ANIO_MAX_GRUPOS];
    int ids_camiseta[RESUMEN_ANIO_MAX_GRUPOS];
    int cuentas_camiseta[RESUMEN_ANIO_MAX_GRUPOS];
    int total_canchas = 0;
    int total_camisetas = 0;
    int id_top = -1;
    int mejor_rendimiento = -1;
    const char *sql =
        "SELECT fecha_hora, goles, asistencias, resultado, rendimiento_general, "
        "rival_nombre, cancha_id, camiseta_id, goles_equipo, goles_rival, id "
        "FROM partido ORDER BY fecha_hora ASC, id ASC;";

    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        int goles = sqlite3_column_int(stmt, 1);
        int asistencias = sqlite3_column_int(stmt, 2);
        int resultado = sqlite3_column_int(stmt, 3);
        int rendimiento = sqlite3_column_int(stmt, 4);
        int cancha_id = sqlite3_column_int(stmt, 6);
        int camiseta_id = sqlite3_column_int(stmt, 7);
        int goles_equipo = sqlite3_column_int(stmt, 8);
        int goles_rival = sqlite3_column_int(stmt, 9);
        int partido_id = sqlite3_column_int(stmt, 10);
        int mes = resumen_anio_mes_desde_fecha(fecha);
        int indice;

        if (resumen_anio_desde_fecha(fecha) != r->anio)
        {
            continue;
        }

        r->partidos++;
        r->goles += goles;
        r->asistencias += asistencias;

        if (resultado == 1)
        {
            r->victorias++;
        }
        else if (resultado == 2)
        {
            r->empates++;
        }
        else if (resultado == 3)
        {
            r->derrotas++;
        }

        if (goles_equipo >= 0 && goles_rival >= 0)
        {
            r->goles_favor += goles_equipo;
            r->goles_contra += goles_rival;
            r->partidos_con_marcador++;
        }

        if (mes >= 1 && mes <= RESUMEN_ANIO_MESES)
        {
            int idx = mes - 1;

            r->mes_partidos[idx]++;
            r->mes_goles[idx] += goles;
            r->mes_asistencias[idx] += asistencias;
            if (resultado == 1)
            {
                r->mes_victorias[idx]++;
            }
            else if (resultado == 2)
            {
                r->mes_empates[idx]++;
            }
            else if (resultado == 3)
            {
                r->mes_derrotas[idx]++;
            }
        }

        if (r->partidos == 1 || rendimiento > mejor_rendimiento)
        {
            mejor_rendimiento = rendimiento;
            r->mejor_id = partido_id;
        }

        if (cancha_id > 0)
        {
            indice = resumen_anio_indice_grupo(ids_cancha, cuentas_cancha, &total_canchas,
                                               cancha_id);
            if (indice >= 0)
            {
                cuentas_cancha[indice]++;
            }
        }

        if (camiseta_id > 0)
        {
            indice = resumen_anio_indice_grupo(ids_camiseta, cuentas_camiseta, &total_camisetas,
                                               camiseta_id);
            if (indice >= 0)
            {
                cuentas_camiseta[indice]++;
            }
        }
    }
    sqlite3_finalize(stmt);

    r->canchas_usadas = total_canchas;
    resumen_anio_top_grupo(ids_cancha, cuentas_cancha, total_canchas, &id_top,
                           &r->cancha_top_partidos);
    if (id_top >= 0)
    {
        resumen_anio_nombre_por_id("SELECT nombre FROM cancha WHERE id = ?;", id_top,
                                   r->cancha_top, sizeof(r->cancha_top));
    }

    r->camisetas_usadas = total_camisetas;
    resumen_anio_top_grupo(ids_camiseta, cuentas_camiseta, total_camisetas, &id_top,
                           &r->camiseta_top_partidos);
    if (id_top >= 0)
    {
        resumen_anio_nombre_por_id("SELECT nombre FROM camiseta WHERE id = ?;", id_top,
                                   r->camiseta_top, sizeof(r->camiseta_top));
    }

    if (r->mejor_id > 0)
    {
        resumen_anio_cargar_partido(r->mejor_id, &r->mejor);
    }
}

/**
 * @brief Cuenta las lesiones registradas en el año y cuantas siguen activas.
 */
static void resumen_anio_leer_lesiones(ResumenAnio *r)
{
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt("SELECT fecha, estado FROM lesion;", &stmt))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        const char *estado = (const char *)sqlite3_column_text(stmt, 1);

        if (resumen_anio_desde_fecha(fecha) != r->anio)
        {
            continue;
        }

        r->lesiones++;
        if (estado && strcmp(estado, "Activa") == 0)
        {
            r->lesiones_activas++;
        }
    }
    sqlite3_finalize(stmt);
}

static void resumen_anio_recolectar(ResumenAnio *r, int anio)
{
    memset(r, 0, sizeof(*r));
    r->anio = anio;
    r->mejor_id = 0;
    snprintf(r->cancha_top, sizeof(r->cancha_top), "Sin datos");
    snprintf(r->camiseta_top, sizeof(r->camiseta_top), "Sin datos");

    obtener_nombre_jugador(r->jugador, sizeof(r->jugador));
    get_datetime(r->fecha_gen, sizeof(r->fecha_gen));

    resumen_anio_leer_partidos(r);
    resumen_anio_leer_lesiones(r);

    r->camisetas_activas = resumen_anio_contar("SELECT COUNT(*) FROM camiseta WHERE activa = 1;");
    r->canchas_activas = resumen_anio_contar("SELECT COUNT(*) FROM cancha WHERE activa = 1;");
}

/* ── Formato TXT ──────────────────────────────────────────────────────────── */

static void resumen_anio_txt_cabecera(FILE *f, const ResumenAnio *r)
{
    fprintf(f, "========================================\n");
    fprintf(f, " RESUMEN DEL AÑO %d\n", r->anio);
    fprintf(f, "========================================\n");
    fprintf(f, "Jugador : %s\n", r->jugador);
    fprintf(f, "Generado: %s\n\n", r->fecha_gen);

    if (r->partidos == 0)
    {
        fprintf(f, "Sin partidos registrados en %d.\n\n", r->anio);
    }
}

static void resumen_anio_txt_partidos(FILE *f, const ResumenAnio *r)
{
    fprintf(f, "--- PARTIDOS Y RESULTADOS ---\n");
    fprintf(f, "Partidos jugados  : %d\n", r->partidos);
    fprintf(f, "Goles             : %d\n", r->goles);
    fprintf(f, "Asistencias       : %d\n", r->asistencias);
    fprintf(f, "Victorias         : %d\n", r->victorias);
    fprintf(f, "Empates           : %d\n", r->empates);
    fprintf(f, "Derrotas          : %d\n", r->derrotas);
    fprintf(f, "Efectividad       : %.1f%%\n", resumen_anio_efectividad(r));
    fprintf(f, "Promedio goles    : %.2f por partido\n",
            resumen_anio_promedio(r->goles, r->partidos));
    fprintf(f, "Promedio asisten. : %.2f por partido\n",
            resumen_anio_promedio(r->asistencias, r->partidos));

    if (r->partidos_con_marcador > 0)
    {
        fprintf(f, "Goles a favor     : %d\n", r->goles_favor);
        fprintf(f, "Goles en contra   : %d\n", r->goles_contra);
        fprintf(f, "Diferencia        : %+d\n", r->goles_favor - r->goles_contra);
        fprintf(f, "Con marcador      : %d partidos\n", r->partidos_con_marcador);
    }
    else
    {
        fprintf(f, "Marcador detallado: sin datos\n");
    }
    fprintf(f, "\n");
}

static void resumen_anio_txt_meses(FILE *f, const ResumenAnio *r)
{
    int i;

    fprintf(f, "--- MES A MES ---\n");
    fprintf(f, "%-12s %4s %4s %4s %4s %4s %4s\n", "Mes", "PJ", "G", "A", "V", "E", "D");
    for (i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        fprintf(f, "%-12s %4d %4d %4d %4d %4d %4d\n",
                resumen_anio_mes_nombre(i),
                r->mes_partidos[i], r->mes_goles[i], r->mes_asistencias[i],
                r->mes_victorias[i], r->mes_empates[i], r->mes_derrotas[i]);
    }
    fprintf(f, "\n");
}

static void resumen_anio_txt_equipamiento(FILE *f, const ResumenAnio *r)
{
    fprintf(f, "--- EQUIPAMIENTO ---\n");
    fprintf(f, "Camisetas activas  : %d\n", r->camisetas_activas);
    fprintf(f, "Camisetas usadas   : %d\n", r->camisetas_usadas);
    fprintf(f, "Camiseta mas usada : %s (%d partidos)\n",
            r->camiseta_top, r->camiseta_top_partidos);
    fprintf(f, "\n");

    fprintf(f, "--- LESIONES ---\n");
    fprintf(f, "Lesiones en el año : %d\n", r->lesiones);
    fprintf(f, "Lesiones activas   : %d\n", r->lesiones_activas);
    fprintf(f, "\n");

    fprintf(f, "--- CANCHAS ---\n");
    fprintf(f, "Canchas activas    : %d\n", r->canchas_activas);
    fprintf(f, "Canchas usadas     : %d\n", r->canchas_usadas);
    fprintf(f, "Cancha mas usada   : %s (%d partidos)\n",
            r->cancha_top, r->cancha_top_partidos);
    fprintf(f, "\n");
}

static void resumen_anio_txt_mejor(FILE *f, const ResumenAnio *r)
{
    const ResumenAnioPartido *p = &r->mejor;
    char texto[768];
    char temp[160];
    int usados;

    fprintf(f, "--- MEJOR PARTIDO DEL AÑO ---\n");
    if (r->mejor_id <= 0)
    {
        fprintf(f, "Sin partidos destacados en el año.\n\n");
        return;
    }

    resumen_anio_texto_rendimiento(p, texto, sizeof(texto));
    fprintf(f, "Rendimiento  : %s\n", texto);
    fprintf(f, "Fecha        : %s\n", p->fecha_hora[0] != '\0' ? p->fecha_hora : "Sin registrar");
    fprintf(f, "Rival        : %s\n", p->rival[0] != '\0' ? p->rival : "Sin registrar");

    if (p->tipo_rival[0] != '\0')
    {
        fprintf(f, "Tipo rival   : %s\n", p->tipo_rival);
    }

    resumen_anio_texto_marcador(p, texto, sizeof(texto));
    if (texto[0] != '\0')
    {
        fprintf(f, "Marcador     : %s\n", texto);
    }

    if (p->goles_detalle[0] != '\0')
    {
        fprintf(f, "Goles        : %d [%s]\n", p->goles, p->goles_detalle);
    }
    else if (p->goles > 0)
    {
        fprintf(f, "Goles        : %d\n", p->goles);
    }

    if (p->asistencias_detalle[0] != '\0')
    {
        fprintf(f, "Asistencias  : %d [%s]\n", p->asistencias, p->asistencias_detalle);
    }
    else if (p->asistencias > 0)
    {
        fprintf(f, "Asistencias  : %d\n", p->asistencias);
    }

    usados = 0;
    texto[0] = '\0';
    resumen_anio_append(texto, sizeof(texto), &usados, p->cancha);
    if (p->estado_cancha > 0)
    {
        snprintf(temp, sizeof(temp), "estado %s",
                 resumen_anio_estado_cancha_texto(p->estado_cancha));
        resumen_anio_append(texto, sizeof(texto), &usados, temp);
    }
    if (p->condicion_cancha[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "condicion %s", p->condicion_cancha);
        resumen_anio_append(texto, sizeof(texto), &usados, temp);
    }
    if (texto[0] != '\0')
    {
        fprintf(f, "Cancha       : %s\n", texto);
    }

    if (p->camiseta[0] != '\0')
    {
        fprintf(f, "Camiseta     : %s\n", p->camiseta);
    }

    resumen_anio_texto_clima(p, texto, sizeof(texto));
    if (texto[0] != '\0')
    {
        fprintf(f, "Clima        : %s\n", texto);
    }

    resumen_anio_texto_fisico(p, texto, sizeof(texto));
    if (texto[0] != '\0')
    {
        fprintf(f, "Fisico       : %s\n", texto);
    }

    resumen_anio_texto_contexto(p, texto, sizeof(texto));
    if (texto[0] != '\0')
    {
        fprintf(f, "Contexto     : %s\n", texto);
    }

    resumen_anio_texto_arbitraje(p, texto, sizeof(texto));
    if (texto[0] != '\0')
    {
        fprintf(f, "Arbitraje    : %s\n", texto);
    }

    if (p->lo_mejor[0] != '\0')
    {
        fprintf(f, "Lo mejor     : %s\n", p->lo_mejor);
    }
    if (p->que_mejorar[0] != '\0')
    {
        fprintf(f, "A mejorar    : %s\n", p->que_mejorar);
    }
    if (p->comentario[0] != '\0')
    {
        fprintf(f, "Comentario   : %s\n", p->comentario);
    }
    if (p->tags[0] != '\0')
    {
        fprintf(f, "Tags         : %s\n", p->tags);
    }
    if (p->eventos_clave[0] != '\0')
    {
        fprintf(f, "Eventos clave: %s\n", p->eventos_clave);
    }

    fprintf(f, "\n");
}

static void resumen_anio_escribir_txt(FILE *f, const ResumenAnio *r)
{
    resumen_anio_txt_cabecera(f, r);
    resumen_anio_txt_partidos(f, r);
    resumen_anio_txt_meses(f, r);
    resumen_anio_txt_equipamiento(f, r);
    resumen_anio_txt_mejor(f, r);

    fprintf(f, "========================================\n");
    fprintf(f, " Generado por MiFutbolC\n");
    fprintf(f, "========================================\n");
}

/* ── Formato HTML ─────────────────────────────────────────────────────────── */

static void resumen_anio_html_stat(FILE *f, const char *titulo, const char *valor,
                                   const char *meta)
{
    fprintf(f,
            "<div class=\"stat-card\">\n"
            "<h3>%s</h3>\n"
            "<div class=\"stat-value\">%s</div>\n"
            "<div class=\"stat-meta\">%s</div>\n"
            "</div>\n",
            html_texto(titulo), html_texto(valor), html_texto(meta));
}

static void resumen_anio_html_stat_int(FILE *f, const char *titulo, int valor, const char *meta)
{
    char buffer[32];

    snprintf(buffer, sizeof(buffer), "%d", valor);
    resumen_anio_html_stat(f, titulo, buffer, meta);
}

static void resumen_anio_html_stats(FILE *f, const ResumenAnio *r)
{
    char efectividad[32];

    snprintf(efectividad, sizeof(efectividad), "%.1f%%", resumen_anio_efectividad(r));

    fprintf(f, "<div class=\"stat-grid\">\n");
    resumen_anio_html_stat_int(f, "Partidos", r->partidos, "jugados en el año");
    resumen_anio_html_stat_int(f, "Goles", r->goles, "anotados");
    resumen_anio_html_stat_int(f, "Asistencias", r->asistencias, "repartidas");
    resumen_anio_html_stat_int(f, "Victorias", r->victorias, "partidos ganados");
    resumen_anio_html_stat_int(f, "Empates", r->empates, "partidos igualados");
    resumen_anio_html_stat_int(f, "Derrotas", r->derrotas, "partidos perdidos");
    resumen_anio_html_stat(f, "Efectividad", efectividad, "de puntos obtenidos");
    resumen_anio_html_stat_int(f, "Lesiones", r->lesiones, "registradas en el año");
    fprintf(f, "</div>\n");
}

static void resumen_anio_html_fila(FILE *f, const char *etiqueta, const char *valor)
{
    fprintf(f, "<tr><td>%s</td><td>%s</td></tr>\n",
            html_texto(etiqueta), html_texto(valor));
}

static void resumen_anio_html_ofensiva(FILE *f, const ResumenAnio *r)
{
    char buffer[64];

    fprintf(f, "<div class=\"section-card\">\n");
    fprintf(f, "<h2>Rendimiento ofensivo</h2>\n");
    fprintf(f, "<table>\n<thead><tr><th>Metrica</th><th>Valor</th></tr></thead>\n<tbody>\n");

    snprintf(buffer, sizeof(buffer), "%d", r->goles);
    resumen_anio_html_fila(f, "Goles totales", buffer);
    snprintf(buffer, sizeof(buffer), "%d", r->asistencias);
    resumen_anio_html_fila(f, "Asistencias totales", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f por partido", resumen_anio_promedio(r->goles, r->partidos));
    resumen_anio_html_fila(f, "Promedio de goles", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f por partido",
             resumen_anio_promedio(r->asistencias, r->partidos));
    resumen_anio_html_fila(f, "Promedio de asistencias", buffer);

    if (r->partidos_con_marcador > 0)
    {
        snprintf(buffer, sizeof(buffer), "%d", r->goles_favor);
        resumen_anio_html_fila(f, "Goles a favor", buffer);
        snprintf(buffer, sizeof(buffer), "%d", r->goles_contra);
        resumen_anio_html_fila(f, "Goles en contra", buffer);
        snprintf(buffer, sizeof(buffer), "%+d", r->goles_favor - r->goles_contra);
        resumen_anio_html_fila(f, "Diferencia de gol", buffer);
        snprintf(buffer, sizeof(buffer), "%d partidos", r->partidos_con_marcador);
        resumen_anio_html_fila(f, "Partidos con marcador", buffer);
    }
    else
    {
        resumen_anio_html_fila(f, "Marcador detallado", "Sin datos");
    }

    fprintf(f, "</tbody>\n</table>\n</div>\n");
}

static void resumen_anio_html_meses(FILE *f, const ResumenAnio *r)
{
    int i;

    fprintf(f, "<div class=\"section-card\">\n");
    fprintf(f, "<h2>Partidos mes a mes</h2>\n");
    fprintf(f, "<table>\n<thead><tr><th>Mes</th><th>PJ</th><th>Goles</th>"
            "<th>Asist.</th><th>V</th><th>E</th><th>D</th></tr></thead>\n<tbody>\n");
    for (i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        fprintf(f, "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td>"
                "<td>%d</td><td>%d</td></tr>\n",
                html_texto(resumen_anio_mes_nombre(i)),
                r->mes_partidos[i], r->mes_goles[i], r->mes_asistencias[i],
                r->mes_victorias[i], r->mes_empates[i], r->mes_derrotas[i]);
    }
    fprintf(f, "</tbody>\n</table>\n</div>\n");
}

static void resumen_anio_html_equipamiento(FILE *f, const ResumenAnio *r)
{
    char buffer[192];

    fprintf(f, "<div class=\"section-card\">\n");
    fprintf(f, "<h2>Equipamiento, lesiones y canchas</h2>\n");
    fprintf(f, "<table>\n<thead><tr><th>Concepto</th><th>Valor</th></tr></thead>\n<tbody>\n");

    snprintf(buffer, sizeof(buffer), "%d", r->camisetas_activas);
    resumen_anio_html_fila(f, "Camisetas activas", buffer);
    snprintf(buffer, sizeof(buffer), "%d", r->camisetas_usadas);
    resumen_anio_html_fila(f, "Camisetas usadas en el año", buffer);
    snprintf(buffer, sizeof(buffer), "%s (%d partidos)", r->camiseta_top, r->camiseta_top_partidos);
    resumen_anio_html_fila(f, "Camiseta mas usada", buffer);

    snprintf(buffer, sizeof(buffer), "%d", r->lesiones);
    resumen_anio_html_fila(f, "Lesiones en el año", buffer);
    snprintf(buffer, sizeof(buffer), "%d", r->lesiones_activas);
    resumen_anio_html_fila(f, "Lesiones activas", buffer);

    snprintf(buffer, sizeof(buffer), "%d", r->canchas_activas);
    resumen_anio_html_fila(f, "Canchas activas", buffer);
    snprintf(buffer, sizeof(buffer), "%d", r->canchas_usadas);
    resumen_anio_html_fila(f, "Canchas usadas en el año", buffer);
    snprintf(buffer, sizeof(buffer), "%s (%d partidos)", r->cancha_top, r->cancha_top_partidos);
    resumen_anio_html_fila(f, "Cancha mas usada", buffer);

    fprintf(f, "</tbody>\n</table>\n</div>\n");
}

static void resumen_anio_html_detalle(FILE *f, const char *etiqueta, const char *valor)
{
    if (!etiqueta || !valor || valor[0] == '\0')
    {
        return;
    }

    fprintf(f, "<tr><th scope=\"row\">%s</th><td>%s</td></tr>\n", html_texto(etiqueta),
            html_texto(valor));
}

static void resumen_anio_html_mejor(FILE *f, const ResumenAnio *r)
{
    const ResumenAnioPartido *p = &r->mejor;
    char texto[640];
    char temp[512];
    int usados;

    fprintf(f, "<div class=\"section-card\">\n");
    fprintf(f, "<h2>Mejor partido del año</h2>\n");
    fprintf(f, "<div class=\"content\">\n");

    if (r->mejor_id <= 0)
    {
        fprintf(f, "<p>Sin partidos destacados en el año.</p>\n");
        fprintf(f, "</div>\n</div>\n");
        return;
    }

    fprintf(f, "<table>\n<tbody>\n");

    resumen_anio_texto_rendimiento(p, texto, sizeof(texto));
    resumen_anio_html_detalle(f, "Rendimiento", texto);
    resumen_anio_html_detalle(f, "Fecha",
                              p->fecha_hora[0] != '\0' ? p->fecha_hora : "Sin registrar");
    resumen_anio_html_detalle(f, "Rival", p->rival[0] != '\0' ? p->rival : "Sin registrar");
    resumen_anio_html_detalle(f, "Tipo de rival", p->tipo_rival);

    resumen_anio_texto_marcador(p, texto, sizeof(texto));
    resumen_anio_html_detalle(f, "Marcador", texto);

    if (p->goles_detalle[0] != '\0')
    {
        snprintf(texto, sizeof(texto), "%d [%s]", p->goles, p->goles_detalle);
        resumen_anio_html_detalle(f, "Goles", texto);
    }
    else if (p->goles > 0)
    {
        snprintf(texto, sizeof(texto), "%d", p->goles);
        resumen_anio_html_detalle(f, "Goles", texto);
    }

    if (p->asistencias_detalle[0] != '\0')
    {
        snprintf(texto, sizeof(texto), "%d [%s]", p->asistencias, p->asistencias_detalle);
        resumen_anio_html_detalle(f, "Asistencias", texto);
    }
    else if (p->asistencias > 0)
    {
        snprintf(texto, sizeof(texto), "%d", p->asistencias);
        resumen_anio_html_detalle(f, "Asistencias", texto);
    }

    usados = 0;
    texto[0] = '\0';
    resumen_anio_append(texto, sizeof(texto), &usados, p->cancha);
    if (p->estado_cancha > 0)
    {
        snprintf(temp, sizeof(temp), "estado %s",
                 resumen_anio_estado_cancha_texto(p->estado_cancha));
        resumen_anio_append(texto, sizeof(texto), &usados, temp);
    }
    if (p->condicion_cancha[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "condicion %s", p->condicion_cancha);
        resumen_anio_append(texto, sizeof(texto), &usados, temp);
    }
    resumen_anio_html_detalle(f, "Cancha", texto);

    resumen_anio_html_detalle(f, "Camiseta", p->camiseta);

    resumen_anio_texto_clima(p, texto, sizeof(texto));
    resumen_anio_html_detalle(f, "Clima", texto);

    resumen_anio_texto_fisico(p, texto, sizeof(texto));
    resumen_anio_html_detalle(f, "Fisico", texto);

    resumen_anio_texto_contexto(p, texto, sizeof(texto));
    resumen_anio_html_detalle(f, "Contexto", texto);

    resumen_anio_texto_arbitraje(p, texto, sizeof(texto));
    resumen_anio_html_detalle(f, "Arbitraje", texto);

    resumen_anio_html_detalle(f, "Lo mejor", p->lo_mejor);
    resumen_anio_html_detalle(f, "A mejorar", p->que_mejorar);
    resumen_anio_html_detalle(f, "Comentario", p->comentario);
    resumen_anio_html_detalle(f, "Tags", p->tags);
    resumen_anio_html_detalle(f, "Eventos clave", p->eventos_clave);

    fprintf(f, "</tbody>\n</table>\n");
    fprintf(f, "</div>\n</div>\n");
}

static void resumen_anio_escribir_html(FILE *f, const ResumenAnio *r)
{
    char titulo[128];

    snprintf(titulo, sizeof(titulo), "Resumen del Año %d", r->anio);
    export_write_html_begin(f, titulo);

    fprintf(f, "<div class=\"alert alert-info\">Jugador: <strong>%s</strong> "
            "&mdash; generado el %s</div>\n",
            html_texto(r->jugador), html_texto(r->fecha_gen));

    if (r->partidos == 0)
    {
        fprintf(f, "<div class=\"alert alert-warn\">Sin partidos registrados en %d. "
                "Igual se muestra el equipamiento y el resto de registros del año.</div>\n",
                r->anio);
    }

    resumen_anio_html_stats(f, r);
    resumen_anio_html_ofensiva(f, r);
    resumen_anio_html_meses(f, r);
    resumen_anio_html_equipamiento(f, r);
    resumen_anio_html_mejor(f, r);
    export_write_html_footer(f, NULL);
}

/* ── Formato PDF ──────────────────────────────────────────────────────────── */

typedef struct
{
    struct pdf_doc *pdf;
    struct pdf_object *page;
    float y;
    float margin;
    int warned;
} ResumenAnioPdfCtx;

/**
 * @brief Escribe una linea en el PDF, saltando de pagina cuando hace falta.
 *
 * pdfgen convierte UTF-8 a WinAnsi, que incluye acentos y enie, asi que el
 * texto se envia tal cual. Solo si trae un caracter que WinAnsi no cubre se
 * reintenta con la version ASCII basica para no perder la linea.
 */
static ResumenAnioPdfCtx resumen_anio_pdf_linea(ResumenAnioPdfCtx ctx, const char *texto,
        float tam, float salto, const char *fuente)
{
    char ascii[512];

    if (!ctx.pdf || !ctx.page || !texto)
    {
        return ctx;
    }

    if (ctx.y < ctx.margin + salto)
    {
        ctx.page = pdf_append_page(ctx.pdf);
        if (!ctx.page)
        {
            return ctx;
        }
        pdf_page_set_size(ctx.pdf, ctx.page, PDF_A4_WIDTH, PDF_A4_HEIGHT);
        ctx.y = pdf_page_height(ctx.page) - ctx.margin;
    }

    pdf_set_font(ctx.pdf, fuente ? fuente : "Helvetica");
    int rc = pdf_add_text(ctx.pdf, ctx.page, texto, tam, ctx.margin, ctx.y, PDF_BLACK);

    if (rc < 0)
    {
        int errval = 0;
        const char *err = pdf_get_err(ctx.pdf, &errval);

        sanitizar_ascii_basico(texto, ascii, sizeof(ascii));
        rc = pdf_add_text(ctx.pdf, ctx.page, ascii, tam, ctx.margin, ctx.y, PDF_BLACK);
        if (rc < 0 && !ctx.warned)
        {
            printf("Advertencia PDF: no se pudo escribir texto (%d): %s\n", errval,
                   err ? err : "error desconocido");
            ctx.warned = 1;
        }
    }

    ctx.y -= salto;
    return ctx;
}

static ResumenAnioPdfCtx resumen_anio_pdf_cabecera(ResumenAnioPdfCtx ctx, const ResumenAnio *r)
{
    char titulo[128];
    char linea[256];

    snprintf(titulo, sizeof(titulo), "RESUMEN DEL AÑO %d", r->anio);
    ctx = resumen_anio_pdf_linea(ctx, titulo, 18.0f, 24.0f, "Helvetica");

    snprintf(linea, sizeof(linea), "Jugador: %s", r->jugador);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Generado: %s", r->fecha_gen);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 18.0f, "Helvetica");

    if (r->partidos == 0)
    {
        snprintf(linea, sizeof(linea), "Sin partidos registrados en el año %d.", r->anio);
        ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 16.0f, "Helvetica");
    }

    return ctx;
}

static ResumenAnioPdfCtx resumen_anio_pdf_partidos(ResumenAnioPdfCtx ctx, const ResumenAnio *r)
{
    char linea[256];

    ctx = resumen_anio_pdf_linea(ctx, "PARTIDOS Y RESULTADOS", 13.0f, 16.0f, "Helvetica");
    snprintf(linea, sizeof(linea),
             "Partidos: %d    Victorias: %d    Empates: %d    Derrotas: %d",
             r->partidos, r->victorias, r->empates, r->derrotas);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Goles: %d    Asistencias: %d", r->goles, r->asistencias);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Promedio goles/partido: %.2f    Promedio asistencias/partido: %.2f",
             resumen_anio_promedio(r->goles, r->partidos),
             resumen_anio_promedio(r->asistencias, r->partidos));
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Efectividad (puntos): %.1f%%", resumen_anio_efectividad(r));
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");

    if (r->partidos_con_marcador > 0)
    {
        snprintf(linea, sizeof(linea), "Goles a favor: %d    Goles en contra: %d    Diferencia: %+d",
                 r->goles_favor, r->goles_contra, r->goles_favor - r->goles_contra);
        ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    }

    return resumen_anio_pdf_linea(ctx, "", 10.0f, 6.0f, "Helvetica");
}

static ResumenAnioPdfCtx resumen_anio_pdf_meses(ResumenAnioPdfCtx ctx, const ResumenAnio *r)
{
    char linea[128];
    int i;

    ctx = resumen_anio_pdf_linea(ctx, "PARTIDOS MES A MES", 13.0f, 16.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "%-12s %4s %4s %4s %4s %4s %4s",
             "Mes", "PJ", "G", "A", "V", "E", "D");
    ctx = resumen_anio_pdf_linea(ctx, linea, 9.0f, 12.0f, "Courier");
    for (i = 0; i < RESUMEN_ANIO_MESES; i++)
    {
        snprintf(linea, sizeof(linea), "%-12s %4d %4d %4d %4d %4d %4d",
                 resumen_anio_mes_nombre(i),
                 r->mes_partidos[i], r->mes_goles[i], r->mes_asistencias[i],
                 r->mes_victorias[i], r->mes_empates[i], r->mes_derrotas[i]);
        ctx = resumen_anio_pdf_linea(ctx, linea, 9.0f, 12.0f, "Courier");
    }

    return resumen_anio_pdf_linea(ctx, "", 10.0f, 6.0f, "Helvetica");
}

static ResumenAnioPdfCtx resumen_anio_pdf_equipamiento(ResumenAnioPdfCtx ctx, const ResumenAnio *r)
{
    char linea[256];

    ctx = resumen_anio_pdf_linea(ctx, "EQUIPAMIENTO, LESIONES Y CANCHAS", 13.0f, 16.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Camisetas activas: %d    Camisetas usadas: %d",
             r->camisetas_activas, r->camisetas_usadas);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Camiseta mas usada: %s (%d partidos)",
             r->camiseta_top, r->camiseta_top_partidos);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Lesiones en el año: %d    Lesiones activas: %d",
             r->lesiones, r->lesiones_activas);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Canchas activas: %d    Canchas usadas: %d",
             r->canchas_activas, r->canchas_usadas);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 13.0f, "Helvetica");
    snprintf(linea, sizeof(linea), "Cancha mas usada: %s (%d partidos)",
             r->cancha_top, r->cancha_top_partidos);
    ctx = resumen_anio_pdf_linea(ctx, linea, 10.0f, 16.0f, "Helvetica");

    return ctx;
}

static ResumenAnioPdfCtx resumen_anio_pdf_detalle(ResumenAnioPdfCtx ctx, const char *etiqueta,
        const char *valor)
{
    char texto[1024];
    char ascii[1024];
    float alto = 0.0f;
    float ancho = PDF_A4_WIDTH - (ctx.margin * 2.0f);
    int rc;

    if (!ctx.pdf || !ctx.page || !etiqueta || !valor || valor[0] == '\0')
    {
        return ctx;
    }

    if (ctx.y < ctx.margin + 30.0f)
    {
        ctx.page = pdf_append_page(ctx.pdf);
        if (!ctx.page)
        {
            return ctx;
        }
        pdf_page_set_size(ctx.pdf, ctx.page, PDF_A4_WIDTH, PDF_A4_HEIGHT);
        ctx.y = pdf_page_height(ctx.page) - ctx.margin;
    }

    snprintf(texto, sizeof(texto), "%s: %s", etiqueta, valor);
    pdf_set_font(ctx.pdf, "Helvetica");
    rc = pdf_add_text_wrap(ctx.pdf, ctx.page, texto, 10.0f, ctx.margin, ctx.y, 0.0f, PDF_BLACK,
                           ancho, 0, &alto);

    if (rc < 0)
    {
        sanitizar_ascii_basico(texto, ascii, sizeof(ascii));
        alto = 0.0f;
        rc = pdf_add_text_wrap(ctx.pdf, ctx.page, ascii, 10.0f, ctx.margin, ctx.y, 0.0f, PDF_BLACK,
                               ancho, 0, &alto);
    }

    if (rc < 0 && !ctx.warned)
    {
        int errval = 0;
        const char *err = pdf_get_err(ctx.pdf, &errval);

        printf("Advertencia PDF: no se pudo escribir texto (%d): %s\n", errval,
               err ? err : "error desconocido");
        ctx.warned = 1;
    }

    ctx.y -= (alto > 0.0f ? alto : 13.0f) + 2.0f;
    return ctx;
}

static ResumenAnioPdfCtx resumen_anio_pdf_mejor(ResumenAnioPdfCtx ctx, const ResumenAnio *r)
{
    const ResumenAnioPartido *p = &r->mejor;
    char texto[768];
    char temp[160];
    int usados;

    ctx = resumen_anio_pdf_linea(ctx, "MEJOR PARTIDO DEL AÑO", 13.0f, 16.0f, "Helvetica");

    if (r->mejor_id <= 0)
    {
        return resumen_anio_pdf_linea(ctx, "Sin partidos destacados en el año.", 10.0f, 13.0f,
                                      "Helvetica");
    }

    resumen_anio_texto_rendimiento(p, texto, sizeof(texto));
    ctx = resumen_anio_pdf_detalle(ctx, "Rendimiento", texto);
    ctx = resumen_anio_pdf_detalle(ctx, "Fecha",
                                   p->fecha_hora[0] != '\0' ? p->fecha_hora : "Sin registrar");
    ctx = resumen_anio_pdf_detalle(ctx, "Rival", p->rival[0] != '\0' ? p->rival : "Sin registrar");
    ctx = resumen_anio_pdf_detalle(ctx, "Tipo de rival", p->tipo_rival);

    resumen_anio_texto_marcador(p, texto, sizeof(texto));
    ctx = resumen_anio_pdf_detalle(ctx, "Marcador", texto);

    if (p->goles_detalle[0] != '\0')
    {
        snprintf(texto, sizeof(texto), "%d [%s]", p->goles, p->goles_detalle);
        ctx = resumen_anio_pdf_detalle(ctx, "Goles", texto);
    }
    else if (p->goles > 0)
    {
        snprintf(texto, sizeof(texto), "%d", p->goles);
        ctx = resumen_anio_pdf_detalle(ctx, "Goles", texto);
    }

    if (p->asistencias_detalle[0] != '\0')
    {
        snprintf(texto, sizeof(texto), "%d [%s]", p->asistencias, p->asistencias_detalle);
        ctx = resumen_anio_pdf_detalle(ctx, "Asistencias", texto);
    }
    else if (p->asistencias > 0)
    {
        snprintf(texto, sizeof(texto), "%d", p->asistencias);
        ctx = resumen_anio_pdf_detalle(ctx, "Asistencias", texto);
    }

    usados = 0;
    texto[0] = '\0';
    resumen_anio_append(texto, sizeof(texto), &usados, p->cancha);
    if (p->estado_cancha > 0)
    {
        snprintf(temp, sizeof(temp), "estado %s",
                 resumen_anio_estado_cancha_texto(p->estado_cancha));
        resumen_anio_append(texto, sizeof(texto), &usados, temp);
    }
    if (p->condicion_cancha[0] != '\0')
    {
        snprintf(temp, sizeof(temp), "condicion %s", p->condicion_cancha);
        resumen_anio_append(texto, sizeof(texto), &usados, temp);
    }
    ctx = resumen_anio_pdf_detalle(ctx, "Cancha", texto);

    ctx = resumen_anio_pdf_detalle(ctx, "Camiseta", p->camiseta);

    resumen_anio_texto_clima(p, texto, sizeof(texto));
    ctx = resumen_anio_pdf_detalle(ctx, "Clima", texto);

    resumen_anio_texto_fisico(p, texto, sizeof(texto));
    ctx = resumen_anio_pdf_detalle(ctx, "Fisico", texto);

    resumen_anio_texto_contexto(p, texto, sizeof(texto));
    ctx = resumen_anio_pdf_detalle(ctx, "Contexto", texto);

    resumen_anio_texto_arbitraje(p, texto, sizeof(texto));
    ctx = resumen_anio_pdf_detalle(ctx, "Arbitraje", texto);

    ctx = resumen_anio_pdf_detalle(ctx, "Lo mejor", p->lo_mejor);
    ctx = resumen_anio_pdf_detalle(ctx, "A mejorar", p->que_mejorar);
    ctx = resumen_anio_pdf_detalle(ctx, "Comentario", p->comentario);
    ctx = resumen_anio_pdf_detalle(ctx, "Tags", p->tags);
    ctx = resumen_anio_pdf_detalle(ctx, "Eventos clave", p->eventos_clave);

    return ctx;
}

static int resumen_anio_escribir_pdf(const ResumenAnio *r, const char *ruta)
{
    struct pdf_info info;
    struct pdf_doc *pdf;
    struct pdf_object *page;
    ResumenAnioPdfCtx ctx;

    memset(&info, 0, sizeof(info));
    snprintf(info.creator, sizeof(info.creator), "MiFutbolC");
    snprintf(info.producer, sizeof(info.producer), "MiFutbolC");
    snprintf(info.title, sizeof(info.title), "Resumen del año %d", r->anio);
    snprintf(info.author, sizeof(info.author), "MiFutbolC");
    snprintf(info.subject, sizeof(info.subject), "Resumen anual del perfil futbolistico");
    get_datetime(info.date, (int)sizeof(info.date));

    pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    if (!pdf)
    {
        printf("Error al crear el documento PDF.\n");
        return 0;
    }

    page = pdf_append_page(pdf);
    if (!page)
    {
        pdf_destroy(pdf);
        printf("Error al crear la pagina del PDF.\n");
        return 0;
    }

    pdf_page_set_size(pdf, page, PDF_A4_WIDTH, PDF_A4_HEIGHT);
    ctx.pdf = pdf;
    ctx.page = page;
    ctx.y = pdf_page_height(page) - 40.0f;
    ctx.margin = 40.0f;
    ctx.warned = 0;

    ctx = resumen_anio_pdf_cabecera(ctx, r);
    ctx = resumen_anio_pdf_partidos(ctx, r);
    ctx = resumen_anio_pdf_meses(ctx, r);
    ctx = resumen_anio_pdf_equipamiento(ctx, r);
    (void)resumen_anio_pdf_mejor(ctx, r);

    if (pdf_save(pdf, ruta) < 0)
    {
        int errval = 0;
        const char *err = pdf_get_err(pdf, &errval);

        printf("Error al guardar el PDF (%d): %s\n", errval, err ? err : "error desconocido");
        pdf_destroy(pdf);
        return 0;
    }

    pdf_destroy(pdf);
    return 1;
}

/* ── Generacion y menu ────────────────────────────────────────────────────── */

static int resumen_anio_generar_txt(const ResumenAnio *r)
{
    char nombre[64];
    char ruta[1024];
    FILE *f = NULL;

    snprintf(nombre, sizeof(nombre), "resumen_anio_%d.txt", r->anio);
    snprintf(ruta, sizeof(ruta), "%s", get_export_path(nombre));
    fopen_s(&f, ruta, "w");
    if (!f)
    {
        printf("Error al crear el archivo TXT: %s\n", ruta);
        return 0;
    }

    resumen_anio_escribir_txt(f, r);
    fclose(f);
    ui_printf("Resumen TXT generado: %s\n", ruta);
    return 1;
}

static int resumen_anio_generar_html(const ResumenAnio *r)
{
    char nombre[64];
    char ruta[1024];
    FILE *f = NULL;

    snprintf(nombre, sizeof(nombre), "resumen_anio_%d.html", r->anio);
    snprintf(ruta, sizeof(ruta), "%s", get_export_path(nombre));
    fopen_s(&f, ruta, "w");
    if (!f)
    {
        printf("Error al crear el archivo HTML: %s\n", ruta);
        return 0;
    }

    resumen_anio_escribir_html(f, r);
    fclose(f);
    ui_printf("Resumen HTML generado: %s\n", ruta);
    return 1;
}

static int resumen_anio_generar_pdf(const ResumenAnio *r)
{
    char nombre[64];
    char ruta[1024];

    snprintf(nombre, sizeof(nombre), "resumen_anio_%d.pdf", r->anio);
    snprintf(ruta, sizeof(ruta), "%s", get_export_path(nombre));
    if (!resumen_anio_escribir_pdf(r, ruta))
    {
        return 0;
    }

    ui_printf("Resumen PDF generado: %s\n", ruta);
    return 1;
}

static void resumen_anio_mostrar_resumen(const ResumenAnio *r)
{
    ui_printf("\nPeriodo %d\n", r->anio);
    ui_printf("Partidos: %d | Goles: %d | Asistencias: %d\n",
              r->partidos, r->goles, r->asistencias);
    ui_printf("Victorias: %d | Empates: %d | Derrotas: %d\n",
              r->victorias, r->empates, r->derrotas);
    ui_printf("Camisetas usadas: %d | Lesiones: %d | Canchas usadas: %d\n",
              r->camisetas_usadas, r->lesiones, r->canchas_usadas);
}

/**
 * @brief Genera el resumen anual en el formato indicado.
 * @param modo 0=TXT, 1=HTML, 2=PDF, 3=los tres formatos.
 */
static void generar_resumen_anio_formato(int modo)
{
    ResumenAnio r;
    int generados = 0;
    int anio = resumen_anio_anio_actual();

    clear_screen();
    print_header("RESUMEN ANUAL");

    resumen_anio_recolectar(&r, anio);

    if (modo == 0 || modo == 3)
    {
        generados += resumen_anio_generar_txt(&r);
    }
    if (modo == 1 || modo == 3)
    {
        generados += resumen_anio_generar_html(&r);
    }
    if (modo == 2 || modo == 3)
    {
        generados += resumen_anio_generar_pdf(&r);
    }

    if (generados == 0)
    {
        printf("\nNo se pudo generar ningun archivo del resumen.\n");
    }
    else
    {
        printf("\nArchivos guardados en la carpeta de exportacion.\n");
        resumen_anio_mostrar_resumen(&r);
    }

    app_log_event("RESUMEN_COMPARTIBLE", "Resumen anual generado");
    pause_console();
}

void generar_resumen_anio_txt(void)
{
    generar_resumen_anio_formato(0);
}

void generar_resumen_anio_html(void)
{
    generar_resumen_anio_formato(1);
}

void generar_resumen_anio_pdf(void)
{
    generar_resumen_anio_formato(2);
}

void generar_resumen_anio_todos(void)
{
    generar_resumen_anio_formato(3);
}

static void menu_resumen_anio(void)
{
    MenuItem items[] =
    {
        {1, "Resumen Anual en TXT", generar_resumen_anio_txt},
        {2, "Resumen Anual en HTML", generar_resumen_anio_html},
        {3, "Resumen Anual en PDF", generar_resumen_anio_pdf},
        {4, "Generar los tres formatos", generar_resumen_anio_todos},
        {0, "Volver", NULL}
    };
    ejecutar_menu("RESUMEN ANUAL", items, 5);
}
