#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include "equipo.h"
#include "ascii_art.h"
#include "entrenador_ia.h"
#include "financiamiento.h"
#include "random_utils.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include "compat_windows.h"
#endif
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <process.h>
#else
#include "process.h"
#endif
#include <memory.h>
#include <limits.h>

#ifndef UNUSED
#if defined(__GNUC__) || defined(__clang__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif
#endif

// Prototipos de funciones estaticas usadas antes de su definicion
static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo);
static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo);
static UNUSED void guardar_estadisticas_equipo(const Equipo *equipo, int const *estadisticas, int const *asistencias,
        int resultado, int cancha_id, char const *fecha_simulacion);
static int crear_cancha_inline(void);
static int crear_camiseta_inline(void);
static const char *estado_cancha_to_text(int estado_cancha);
static const char *dolor_fisico_to_text(int dolor_fisico);
static const char *arbitraje_score_to_text(int arbitraje_score);
static const char *tarjeta_to_text(int tarjeta);
static void modificar_campo_texto_partido(const char *campo, const char *prompt, const char *mensaje_exito, int buffer_size);
static void menu_modificar_rendimiento_y_estado_partido(void);
static void menu_modificar_detalle_ampliado_partido(void);
static void modificar_detalle_evento_partido(const char *campo, const char *etiqueta, int es_asistencia, const char *mensaje_exito);

// Declaracion externa para funcion de financiamiento
extern void obtener_fecha_actual(char *fecha);

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

static const char *stmt_text_or_default(sqlite3_stmt *stmt, int columna, const char *fallback)
{
    const unsigned char *valor = sqlite3_column_text(stmt, columna);
    return valor ? (const char *)valor : fallback;
}

static const char *tipo_partido_to_text(int tipo_partido)
{
    switch (tipo_partido)
    {
    case 1:
        return "Amistoso";
    case 2:
        return "Torneo";
    case 3:
        return "Entrenamiento";
    default:
        return "Amistoso o Torneo";
    }
}

static const char *dia_to_text_por_hora(int hora)
{
    if (hora < 6)
    {
        return "Madrugada";
    }
    if (hora < 12)
    {
        return "Manana";
    }
    if (hora < 15)
    {
        return "Mediodia";
    }
    if (hora < 19)
    {
        return "Tarde";
    }
    if (hora < 21)
    {
        return "Atardecer";
    }
    return "Noche";
}

static int dia_codigo_por_hora(int hora)
{
    if (hora < 6)
    {
        return 1;
    }
    if (hora < 12)
    {
        return 2;
    }
    if (hora < 15)
    {
        return 3;
    }
    if (hora < 19)
    {
        return 4;
    }
    if (hora < 21)
    {
        return 5;
    }
    return 6;
}

static int obtener_hora_desde_fecha_hora(const char *fecha_hora, int *hora_out)
{
    int hora = -1;
    int minuto = -1;
    const char *inicio_hora;
    const char *separador;

    if (!fecha_hora || !hora_out)
    {
        return 0;
    }

    separador = strrchr(fecha_hora, ' ');
    if (!separador)
    {
        separador = strrchr(fecha_hora, 'T');
    }
    inicio_hora = separador ? (separador + 1) : fecha_hora;

#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(inicio_hora, "%d:%d", &hora, &minuto) != 2)
#else
    if (sscanf(inicio_hora, "%d:%d", &hora, &minuto) != 2)
#endif
    {
        return 0;
    }

    if (hora < 0 || hora > 23 || minuto < 0 || minuto > 59)
    {
        return 0;
    }

    *hora_out = hora;
    return 1;
}

static int calcular_dia_desde_fecha_hora(const char *fecha_hora, int *dia_out)
{
    int hora = 0;

    if (!dia_out || !obtener_hora_desde_fecha_hora(fecha_hora, &hora))
    {
        return 0;
    }

    *dia_out = dia_codigo_por_hora(hora);
    return 1;
}

static void solicitar_fecha_hora_partido(char *fecha, size_t fecha_size)
{
    while (1)
    {
        int hora_ingresada = 0;

        printf("\nFecha y Hora del partido (dd/mm/yyyy hh:mm):\n");
        printf("(Presione Enter para usar fecha/hora actual): ");
        fgets(fecha, (int)fecha_size, stdin);
        fecha[strcspn(fecha, "\n")] = 0;

        if (fecha[0] == '\0' || (fecha[0] == ' ' && fecha[1] == '\0'))
        {
            get_datetime(fecha, (int)fecha_size);
            printf("Usando fecha/hora actual: %s\n", fecha);
            return;
        }

        trim_whitespace(fecha);
        if (obtener_hora_desde_fecha_hora(fecha, &hora_ingresada))
        {
            printf("Fecha/hora ingresada: %s\n", fecha);
            return;
        }

        printf("Formato invalido. Debe incluir hora (ejemplo: 25/05/2026 22:20).\n");
    }
}

static const char *dia_partido_para_listado(const char *fecha_hora, int dia_codigo)
{
    int hora = 0;

    if (obtener_hora_desde_fecha_hora(fecha_hora, &hora))
    {
        return dia_to_text_por_hora(hora);
    }

    return dia_to_text(dia_codigo);
}

static void imprimir_marcador_global_si_disponible(sqlite3_stmt *stmt)
{
    int goles_equipo = sqlite3_column_int(stmt, 28);
    int goles_rival = sqlite3_column_int(stmt, 29);
    if (goles_equipo >= 0 && goles_rival >= 0)
    {
        ui_printf_centered_line("Marcador global: %d-%d", goles_equipo, goles_rival);
    }
}

static void imprimir_temperatura_partido(sqlite3_stmt *stmt)
{
    if (sqlite3_column_type(stmt, 34) == SQLITE_NULL)
    {
        ui_printf_centered_line("Temperatura: N/A");
    }
    else
    {
        ui_printf_centered_line("Temperatura: %.1f C", sqlite3_column_double(stmt, 34));
    }
}

static void imprimir_bloque_base_partido(sqlite3_stmt *stmt, const char *fecha_con_dia, int tipo_partido)
{
    const char *fecha_hora = stmt_text_or_default(stmt, 2, "");
    const char *formato_partido = stmt_text_or_default(stmt, 30, "");
    const char *dia_texto = dia_partido_para_listado(fecha_hora, sqlite3_column_int(stmt, 12));

    if (formato_partido[0] == '\0')
    {
        formato_partido = "No especificado";
    }

    ui_printf_centered_line("Partido: %d", sqlite3_column_int(stmt, 0));
    ui_printf_centered_line("Tipo: %s", tipo_partido_to_text(tipo_partido));
    ui_printf_centered_line("Cancha: %s", stmt_text_or_default(stmt, 1, "N/A"));
    ui_printf_centered_line("Fecha: %s", fecha_con_dia);
    ui_printf_centered_line("Goles: %d, Asistencias: %d", sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
    ui_printf_centered_line("Camiseta: %s", stmt_text_or_default(stmt, 5, "N/A"));

    if (tipo_partido == 3)
    {
        ui_printf_centered_line("Resultado: N/A (Entrenamiento)");
    }
    else
    {
        ui_printf_centered_line("Resultado: %s", resultado_to_text(sqlite3_column_int(stmt, 6)));
    }

    ui_printf_centered_line("Rendimiento General: %d/10", sqlite3_column_int(stmt, 7));
    ui_printf_centered_line("Cansancio: %d/10", sqlite3_column_int(stmt, 8));
    ui_printf_centered_line("Estado de Animo: %d/10", sqlite3_column_int(stmt, 9));
    ui_printf_centered_line("Comentario Personal: %s", stmt_text_or_default(stmt, 10, "N/A"));
    ui_printf_centered_line("Clima: %s", clima_to_text(sqlite3_column_int(stmt, 11)));
    ui_printf_centered_line("Dia: %s", dia_texto);
    ui_printf_centered_line("Precio: %d", sqlite3_column_int(stmt, 13));
    ui_printf_centered_line("Estado de cancha: %s", estado_cancha_to_text(sqlite3_column_int(stmt, 27)));
    imprimir_marcador_global_si_disponible(stmt);
    ui_printf_centered_line("Formato: %s", formato_partido);
    ui_printf_centered_line("Tarjeta: %s", tarjeta_to_text(sqlite3_column_int(stmt, 31)));
    ui_printf_centered_line("Goles en contra: %d", sqlite3_column_int(stmt, 32));
    ui_printf_centered_line("Detalle Goles: %s", stmt_text_or_default(stmt, 39, "N/A"));
    ui_printf_centered_line("Detalle Asistencias: %s", stmt_text_or_default(stmt, 40, "N/A"));
}

static void imprimir_bloque_detallado_partido(sqlite3_stmt *stmt, int tipo_partido)
{
    if (tipo_partido != 2 && tipo_partido != 3)
    {
        return;
    }

    if (tipo_partido == 2)
    {
        ui_printf_centered_line("Rival: %s (%s)",
                                stmt_text_or_default(stmt, 15, "N/A"),
                                stmt_text_or_default(stmt, 16, "N/A"));
    }

    ui_printf_centered_line("Posicion: %s | Minutos: %d",
                            stmt_text_or_default(stmt, 17, "N/A"),
                            sqlite3_column_int(stmt, 18));
    ui_printf_centered_line("Intensidad del partido (objetiva): %d",
                            sqlite3_column_int(stmt, 19));
    ui_printf_centered_line("Dolor/molestia fisica: %s",
                            dolor_fisico_to_text(sqlite3_column_int(stmt, 33)));
    imprimir_temperatura_partido(stmt);
    ui_printf_centered_line("Condicion Cancha: %s | Arbitraje: %s",
                            stmt_text_or_default(stmt, 21, "N/A"),
                            arbitraje_score_to_text(sqlite3_column_int(stmt, 35)));
    ui_printf_centered_line("Eventos Clave: %s", stmt_text_or_default(stmt, 23, "N/A"));
    ui_printf_centered_line("Lo mejor: %s", stmt_text_or_default(stmt, 36, "N/A"));
    ui_printf_centered_line("Que mejorar: %s", stmt_text_or_default(stmt, 37, "N/A"));
    ui_printf_centered_line("Tags: %s", stmt_text_or_default(stmt, 38, "N/A"));
    ui_printf_centered_line("Ratings T/F/M: %d/%d/%d",
                            sqlite3_column_int(stmt, 24),
                            sqlite3_column_int(stmt, 25),
                            sqlite3_column_int(stmt, 26));
}

static int mostrar_partidos_desde_stmt(sqlite3_stmt *stmt)
{
    int hay = 0;
    char fecha_con_dia[48];

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        format_date_with_weekday_for_display((const char *)sqlite3_column_text(stmt, 2),
                                             fecha_con_dia, sizeof(fecha_con_dia));
        int tipo_partido = sqlite3_column_int(stmt, 14);

        imprimir_bloque_base_partido(stmt, fecha_con_dia, tipo_partido);
        imprimir_bloque_detallado_partido(stmt, tipo_partido);
        ui_printf_centered_line("----------------------------------------");
        hay = 1;
    }

    return hay;
}

typedef struct
{
    int cancha_id;
    int camiseta_id;
    int tipo_partido;
    int goles_min;
    int goles_max;
    int asistencias_min;
    int asistencias_max;
    int precio_min;
    int precio_max;
    int rendimiento_min;
    int rendimiento_max;
    int cansancio_min;
    int cansancio_max;
    int estado_animo_min;
    int estado_animo_max;
    int clima;
    int dia;
    char tag[64];
    int solo_favoritos;
    int presencia_flags;
} PartidoListadoFiltros;

#define PARTIDO_PRESENCIA_TODOS      0
#define PARTIDO_PRESENCIA_CON_DATOS  1
#define PARTIDO_PRESENCIA_SIN_DATOS  2
#define PARTIDO_PRESENCIA_GOLES_MASK 0x03
#define PARTIDO_PRESENCIA_ASIST_MASK 0x0C

static int partido_listado_get_modo_presencia_goles(const PartidoListadoFiltros *filtros)
{
    return filtros ? (filtros->presencia_flags & PARTIDO_PRESENCIA_GOLES_MASK) : PARTIDO_PRESENCIA_TODOS;
}

static int partido_listado_get_modo_presencia_asistencias(const PartidoListadoFiltros *filtros)
{
    return filtros ? ((filtros->presencia_flags & PARTIDO_PRESENCIA_ASIST_MASK) >> 2) : PARTIDO_PRESENCIA_TODOS;
}

static void partido_listado_set_modo_presencia_goles(PartidoListadoFiltros *filtros, int modo)
{
    if (!filtros)
    {
        return;
    }

    filtros->presencia_flags &= ~PARTIDO_PRESENCIA_GOLES_MASK;
    filtros->presencia_flags |= (modo & PARTIDO_PRESENCIA_GOLES_MASK);
}

static void partido_listado_set_modo_presencia_asistencias(PartidoListadoFiltros *filtros, int modo)
{
    if (!filtros)
    {
        return;
    }

    filtros->presencia_flags &= ~PARTIDO_PRESENCIA_ASIST_MASK;
    filtros->presencia_flags |= ((modo & PARTIDO_PRESENCIA_GOLES_MASK) << 2);
}

static const char *partido_listado_texto_presencia_goles(int modo)
{
    switch (modo)
    {
    case PARTIDO_PRESENCIA_CON_DATOS:
        return "Con goles";
    case PARTIDO_PRESENCIA_SIN_DATOS:
        return "Sin goles";
    default:
        return "Todos";
    }
}

static const char *partido_listado_texto_presencia_asistencias(int modo)
{
    switch (modo)
    {
    case PARTIDO_PRESENCIA_CON_DATOS:
        return "Con asistencias";
    case PARTIDO_PRESENCIA_SIN_DATOS:
        return "Sin asistencias";
    default:
        return "Todos";
    }
}

static int partido_listado_paginacion_es_valida(int valor)
{
    return valor == 0 || (valor >= 5 && valor <= 50 && (valor % 5) == 0);
}

static const char *partido_listado_texto_paginacion(int valor)
{
    return (valor == 0) ? "Todos" : "Por paginas";
}

static void partido_listado_init_config_table(void)
{
    sqlite3_stmt *stmt;

    if (preparar_stmt("CREATE TABLE IF NOT EXISTS partido_listado_config ("
                      "id INTEGER PRIMARY KEY CHECK(id = 1), "
                      "partidos_por_pagina INTEGER NOT NULL DEFAULT 5)",
                      &stmt))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    if (preparar_stmt("INSERT OR IGNORE INTO partido_listado_config(id, partidos_por_pagina) VALUES(1, 5)", &stmt))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

static int partido_listado_cargar_paginacion(void)
{
    sqlite3_stmt *stmt;
    int valor = 5;

    partido_listado_init_config_table();

    if (!preparar_stmt("SELECT IFNULL(partidos_por_pagina, 5) FROM partido_listado_config WHERE id = 1", &stmt))
    {
        return valor;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        valor = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (!partido_listado_paginacion_es_valida(valor))
    {
        return 5;
    }

    return valor;
}

static void partido_listado_guardar_paginacion(int valor)
{
    sqlite3_stmt *stmt;

    if (!partido_listado_paginacion_es_valida(valor))
    {
        return;
    }

    partido_listado_init_config_table();

    if (!preparar_stmt("INSERT OR REPLACE INTO partido_listado_config(id, partidos_por_pagina) VALUES(1, ?)", &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void partido_listado_limpiar_filtros(PartidoListadoFiltros *filtros)
{
    if (!filtros)
    {
        return;
    }

    filtros->cancha_id = -1;
    filtros->camiseta_id = -1;
    filtros->tipo_partido = -1;
    filtros->goles_min = -1;
    filtros->goles_max = -1;
    filtros->asistencias_min = -1;
    filtros->asistencias_max = -1;
    filtros->precio_min = -1;
    filtros->precio_max = -1;
    filtros->rendimiento_min = -1;
    filtros->rendimiento_max = -1;
    filtros->cansancio_min = -1;
    filtros->cansancio_max = -1;
    filtros->estado_animo_min = -1;
    filtros->estado_animo_max = -1;
    filtros->clima = -1;
    filtros->dia = -1;
    filtros->tag[0] = '\0';
    filtros->solo_favoritos = 0;
    filtros->presencia_flags = 0;
}

static size_t partido_listado_strnlen_seguro(const char *texto, size_t max_len)
{
    if (!texto)
    {
        return 0;
    }

#if defined(__STDC_LIB_EXT1__)
    return strnlen_s(texto, max_len);
#elif defined(_MSC_VER)
    return strnlen_s(texto, max_len);
#else
    size_t len = 0;

    while (len < max_len && texto[len] != '\0')
    {
        ++len;
    }
    return len;
#endif
}

static void partido_listado_append_clause(char *destino, size_t destino_size, const char *clausula)
{
    size_t usados;

    if (!destino || !clausula || destino_size == 0)
    {
        return;
    }

    usados = partido_listado_strnlen_seguro(destino, destino_size);
    if (usados >= destino_size - 1)
    {
        return;
    }

    snprintf(destino + usados, destino_size - usados, "%s", clausula);
}

static void partido_listado_append_si_activo(int condicion, char *where_clause, size_t where_size, const char *clausula)
{
    if (condicion)
    {
        partido_listado_append_clause(where_clause, where_size, clausula);
    }
}

static void partido_listado_append_rango_min_max(char *where_clause,
        size_t where_size,
        int minimo,
        const char *clausula_minimo,
        int maximo,
        const char *clausula_maximo)
{
    partido_listado_append_si_activo(minimo >= 0, where_clause, where_size, clausula_minimo);
    partido_listado_append_si_activo(maximo >= 0, where_clause, where_size, clausula_maximo);
}

static void partido_listado_append_por_presencia(int modo,
        char *where_clause,
        size_t where_size,
        const char *clausula_con_datos,
        const char *clausula_sin_datos)
{
    if (modo == PARTIDO_PRESENCIA_CON_DATOS)
    {
        partido_listado_append_clause(where_clause, where_size, clausula_con_datos);
    }
    else if (modo == PARTIDO_PRESENCIA_SIN_DATOS)
    {
        partido_listado_append_clause(where_clause, where_size, clausula_sin_datos);
    }
}

static void partido_listado_append_filtros_identidad(const PartidoListadoFiltros *filtros, char *where_clause, size_t where_size)
{
    if (filtros->cancha_id > 0)
    {
        partido_listado_append_clause(where_clause, where_size, " AND p.cancha_id = ?");
    }

    if (filtros->camiseta_id > 0)
    {
        partido_listado_append_clause(where_clause, where_size, " AND p.camiseta_id = ?");
    }

    if (filtros->tipo_partido >= 1 && filtros->tipo_partido <= 3)
    {
        partido_listado_append_clause(where_clause, where_size, " AND IFNULL(p.tipo_partido, 1) = ?");
    }
}

static void partido_listado_append_filtros_rendimiento(const PartidoListadoFiltros *filtros, char *where_clause, size_t where_size)
{
    int modo_goles = partido_listado_get_modo_presencia_goles(filtros);
    int modo_asistencias = partido_listado_get_modo_presencia_asistencias(filtros);

    partido_listado_append_rango_min_max(where_clause,
                                         where_size,
                                         filtros->goles_min,
                                         " AND p.goles >= ?",
                                         filtros->goles_max,
                                         " AND p.goles <= ?");

    partido_listado_append_rango_min_max(where_clause,
                                         where_size,
                                         filtros->asistencias_min,
                                         " AND p.asistencias >= ?",
                                         filtros->asistencias_max,
                                         " AND p.asistencias <= ?");

    partido_listado_append_por_presencia(modo_goles,
                                         where_clause,
                                         where_size,
                                         " AND IFNULL(p.goles, 0) > 0",
                                         " AND IFNULL(p.goles, 0) = 0");

    partido_listado_append_por_presencia(modo_asistencias,
                                         where_clause,
                                         where_size,
                                         " AND IFNULL(p.asistencias, 0) > 0",
                                         " AND IFNULL(p.asistencias, 0) = 0");

    partido_listado_append_rango_min_max(where_clause,
                                         where_size,
                                         filtros->precio_min,
                                         " AND IFNULL(p.precio, 0) >= ?",
                                         filtros->precio_max,
                                         " AND IFNULL(p.precio, 0) <= ?");

    partido_listado_append_rango_min_max(where_clause,
                                         where_size,
                                         filtros->rendimiento_min,
                                         " AND IFNULL(p.rendimiento_general, 0) >= ?",
                                         filtros->rendimiento_max,
                                         " AND IFNULL(p.rendimiento_general, 0) <= ?");

    partido_listado_append_rango_min_max(where_clause,
                                         where_size,
                                         filtros->cansancio_min,
                                         " AND IFNULL(p.cansancio, 0) >= ?",
                                         filtros->cansancio_max,
                                         " AND IFNULL(p.cansancio, 0) <= ?");

    partido_listado_append_rango_min_max(where_clause,
                                         where_size,
                                         filtros->estado_animo_min,
                                         " AND IFNULL(p.estado_animo, 0) >= ?",
                                         filtros->estado_animo_max,
                                         " AND IFNULL(p.estado_animo, 0) <= ?");
}

static void partido_listado_append_filtros_contexto(const PartidoListadoFiltros *filtros, char *where_clause, size_t where_size)
{
    if (filtros->clima >= 1)
    {
        partido_listado_append_clause(where_clause, where_size, " AND IFNULL(p.clima, 0) = ?");
    }

    if (filtros->dia >= 1)
    {
        partido_listado_append_clause(where_clause, where_size,
                                      " AND IFNULL(p.dia, 0) = ?");
    }
}

static void partido_listado_append_filtros_opcionales(const PartidoListadoFiltros *filtros, char *where_clause, size_t where_size)
{
    if (filtros->tag[0] != '\0')
    {
        partido_listado_append_clause(where_clause, where_size, " AND LOWER(IFNULL(p.tags, '')) LIKE LOWER(?)");
    }

    if (filtros->solo_favoritos)
    {
        partido_listado_append_clause(where_clause, where_size, " AND EXISTS (SELECT 1 FROM partido_meta pm WHERE pm.partido_id = p.id AND pm.favorito = 1)");
    }
}

static void partido_listado_construir_where_clause(const PartidoListadoFiltros *filtros, char *where_clause, size_t where_size)
{
    if (!filtros || !where_clause || where_size == 0)
    {
        return;
    }

    snprintf(where_clause, where_size, "WHERE 1=1");

    partido_listado_append_filtros_identidad(filtros, where_clause, where_size);
    partido_listado_append_filtros_rendimiento(filtros, where_clause, where_size);
    partido_listado_append_filtros_contexto(filtros, where_clause, where_size);
    partido_listado_append_filtros_opcionales(filtros, where_clause, where_size);
}

static int partido_listado_bind_int_si(sqlite3_stmt *stmt, int indice, int condicion, int valor)
{
    if (condicion)
    {
        sqlite3_bind_int(stmt, indice, valor);
        return indice + 1;
    }

    return indice;
}

static int partido_listado_bind_tag_si(sqlite3_stmt *stmt, int indice, const char *tag)
{
    char tag_pattern[96];

    if (!tag || tag[0] == '\0')
    {
        return indice;
    }

    snprintf(tag_pattern, sizeof(tag_pattern), "%%%s%%", tag);
    sqlite3_bind_text(stmt, indice, tag_pattern, -1, SQLITE_TRANSIENT);
    return indice + 1;
}

static int partido_listado_bind_filtros(sqlite3_stmt *stmt, const PartidoListadoFiltros *filtros, int indice_inicial)
{
    int indice = indice_inicial;

    if (!stmt || !filtros)
    {
        return indice;
    }

    indice = partido_listado_bind_int_si(stmt, indice, filtros->cancha_id > 0, filtros->cancha_id);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->camiseta_id > 0, filtros->camiseta_id);
    indice = partido_listado_bind_int_si(stmt, indice,
                                         filtros->tipo_partido >= 1 && filtros->tipo_partido <= 3,
                                         filtros->tipo_partido);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->goles_min >= 0, filtros->goles_min);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->goles_max >= 0, filtros->goles_max);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->asistencias_min >= 0, filtros->asistencias_min);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->asistencias_max >= 0, filtros->asistencias_max);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->precio_min >= 0, filtros->precio_min);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->precio_max >= 0, filtros->precio_max);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->rendimiento_min >= 0, filtros->rendimiento_min);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->rendimiento_max >= 0, filtros->rendimiento_max);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->cansancio_min >= 0, filtros->cansancio_min);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->cansancio_max >= 0, filtros->cansancio_max);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->estado_animo_min >= 0, filtros->estado_animo_min);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->estado_animo_max >= 0, filtros->estado_animo_max);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->clima >= 1, filtros->clima);
    indice = partido_listado_bind_int_si(stmt, indice, filtros->dia >= 1, filtros->dia);
    indice = partido_listado_bind_tag_si(stmt, indice, filtros->tag);

    return indice;
}

static int partido_listado_contar_total(const PartidoListadoFiltros *filtros)
{
    char where_clause[2048];
    char sql[4096];
    int total = 0;
    sqlite3_stmt *stmt;

    partido_listado_construir_where_clause(filtros, where_clause, sizeof(where_clause));
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) "
             "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
             "JOIN cancha can ON p.cancha_id = can.id "
             "%s",
             where_clause);

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    (void)partido_listado_bind_filtros(stmt, filtros, 1);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return total;
}

static int partido_listado_mostrar_pagina_actual(int pagina_actual,
        int partidos_por_pagina,
        int total_partidos,
        int orden_desc,
        const PartidoListadoFiltros *filtros)
{
    char where_clause[2048];
    char sql[4096];
    const char *orden_sql = orden_desc ? "DESC" : "ASC";
    int indice_bind;
    int offset = 0;
    int limite = partidos_por_pagina;
    int hay = 0;
    sqlite3_stmt *stmt;

    partido_listado_construir_where_clause(filtros, where_clause, sizeof(where_clause));

    snprintf(sql, sizeof(sql),
             "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio, "
             "IFNULL(p.tipo_partido, 1), IFNULL(p.rival_nombre, ''), IFNULL(p.tipo_rival, ''), IFNULL(p.posicion_jugada, ''), "
             "IFNULL(p.minutos_jugados, 0), IFNULL(p.intensidad, 0), IFNULL(p.esfuerzo_percibido, 0), IFNULL(p.condicion_cancha, ''), "
             "IFNULL(p.arbitraje, ''), IFNULL(p.eventos_clave, ''), IFNULL(p.rating_tecnico, 0), IFNULL(p.rating_fisico, 0), IFNULL(p.rating_mental, 0), "
             "IFNULL(p.estado_cancha, 0), IFNULL(p.goles_equipo, -1), IFNULL(p.goles_rival, -1), IFNULL(p.formato_partido, ''), IFNULL(p.tarjeta, 1), IFNULL(p.goles_en_contra, 0), "
             "IFNULL(p.dolor_fisico, 0), p.temperatura_c, IFNULL(p.arbitraje_score, 0), IFNULL(p.lo_mejor, ''), IFNULL(p.que_mejorar, ''), IFNULL(p.tags, ''), "
             "IFNULL(p.goles_detalle, ''), IFNULL(p.asistencias_detalle, '') "
             "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
             "JOIN cancha can ON p.cancha_id = can.id "
             "%s "
             "ORDER BY p.id %s LIMIT ? OFFSET ?",
             where_clause,
             orden_sql);

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    if (partidos_por_pagina == 0)
    {
        limite = total_partidos;
        offset = 0;
    }
    else if (orden_desc)
    {
        offset = (pagina_actual - 1) * partidos_por_pagina;
    }
    else
    {
        /* En ASC se muestran primero los bloques mas recientes, manteniendo ASC dentro de la pagina. */
        offset = total_partidos - (pagina_actual * partidos_por_pagina);
        if (offset < 0)
        {
            limite = partidos_por_pagina + offset;
            offset = 0;
        }
        if (limite < 0)
        {
            limite = 0;
        }
    }

    indice_bind = partido_listado_bind_filtros(stmt, filtros, 1);
    sqlite3_bind_int(stmt, indice_bind, limite);
    sqlite3_bind_int(stmt, indice_bind + 1, offset);

    hay = mostrar_partidos_desde_stmt(stmt);
    sqlite3_finalize(stmt);

    return hay;
}

static const char *partido_listado_texto_tipo(int tipo_partido)
{
    switch (tipo_partido)
    {
    case 1:
        return "Amistoso";
    case 2:
        return "Torneo";
    case 3:
        return "Entrenamiento";
    default:
        return "Todos";
    }
}

static int partido_listado_contar_filtros_activos(const PartidoListadoFiltros *filtros)
{
    int modo_goles;
    int modo_asistencias;

    if (!filtros)
    {
        return 0;
    }

    modo_goles = partido_listado_get_modo_presencia_goles(filtros);
    modo_asistencias = partido_listado_get_modo_presencia_asistencias(filtros);

    return (filtros->cancha_id > 0) +
           (filtros->camiseta_id > 0) +
           (filtros->tipo_partido >= 1 && filtros->tipo_partido <= 3) +
           (filtros->goles_min >= 0 || filtros->goles_max >= 0) +
           (filtros->asistencias_min >= 0 || filtros->asistencias_max >= 0) +
           (modo_goles != PARTIDO_PRESENCIA_TODOS) +
           (modo_asistencias != PARTIDO_PRESENCIA_TODOS) +
           (filtros->precio_min >= 0 || filtros->precio_max >= 0) +
           (filtros->rendimiento_min >= 0 || filtros->rendimiento_max >= 0) +
           (filtros->cansancio_min >= 0 || filtros->cansancio_max >= 0) +
           (filtros->estado_animo_min >= 0 || filtros->estado_animo_max >= 0) +
           (filtros->clima >= 1) +
           (filtros->dia >= 1) +
           (filtros->tag[0] != '\0') +
           (filtros->solo_favoritos ? 1 : 0);
}

static int partido_listado_menu_paginacion(int valor_actual)
{
    const int opciones[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
    const int cantidad_opciones = (int)(sizeof(opciones) / sizeof(opciones[0]));

    while (1)
    {
        clear_screen();
        print_header("PAGINACION");
        if (valor_actual == 0)
        {
            ui_printf_centered_line("Actual: Todos los partidos");
        }
        else
        {
            ui_printf_centered_line("Actual: %d partidos por pagina", valor_actual);
        }
        ui_printf_centered_line("Seleccione un nuevo valor:");

        for (int i = 0; i < cantidad_opciones; i++)
        {
            ui_printf_centered_line("%d) %d partidos", i + 1, opciones[i]);
        }
        ui_printf_centered_line("%d) Todos", cantidad_opciones + 1);
        ui_printf_centered_line("0) Volver");

        int opcion = input_int("Opcion: ");
        if (opcion == 0)
        {
            return valor_actual;
        }

        if (opcion >= 1 && opcion <= cantidad_opciones)
        {
            return opciones[opcion - 1];
        }

        if (opcion == cantidad_opciones + 1)
        {
            return 0;
        }

        ui_printf_centered_line("Opcion invalida.");
        pause_console();
    }
}

static const char *partido_listado_texto_orden(int orden_desc)
{
    return orden_desc ? "Del Mas Reciente Primero" : "Del Mas Antiguo Primero";
}

static int partido_listado_menu_orden(int orden_actual_desc)
{
    while (1)
    {
        clear_screen();
        print_header("ORDEN DE LISTADO");
        ui_printf_centered_line("Actual: %s", partido_listado_texto_orden(orden_actual_desc));
        ui_printf_centered_line("1)Mas Antiguo Primero");
        ui_printf_centered_line("2)Mas Reciente Primero");
        ui_printf_centered_line("0) Volver");

        int opcion = input_int("Opcion: ");
        if (opcion == 0)
        {
            return orden_actual_desc;
        }
        if (opcion == 1)
        {
            return 1;
        }
        if (opcion == 2)
        {
            return 0;
        }

        ui_printf_centered_line("Opcion invalida.");
        pause_console();
    }
}

static int partido_listado_pedir_opcional_0_todos(const char *prompt, int min, int max)
{
    int valor = input_int(prompt);

    while (valor != 0 && (valor < min || valor > max))
    {
        char prompt_error[128];
        snprintf(prompt_error, sizeof(prompt_error), "Valor invalido. Ingrese 0 o un valor entre %d y %d: ", min, max);
        valor = input_int(prompt_error);
    }

    return (valor == 0) ? -1 : valor;
}

static int partido_listado_pedir_opcional_menos1_todos(const char *prompt, int min, int max)
{
    int valor = input_int(prompt);

    while (valor != -1 && (valor < min || valor > max))
    {
        char prompt_error[128];
        snprintf(prompt_error, sizeof(prompt_error), "Valor invalido. Ingrese -1 o un valor entre %d y %d: ", min, max);
        valor = input_int(prompt_error);
    }

    return valor;
}

static void partido_listado_configurar_rango(const char *nombre,
        int min,
        int max,
        int max_span,
        int *min_out,
        int *max_out)
{
    char prompt_min[128];
    char prompt_max[128];
    int min_val;
    int max_val;

    if (!min_out || !max_out)
    {
        return;
    }

    snprintf(prompt_min, sizeof(prompt_min), "%s minimo (-1 sin filtro): ", nombre);
    snprintf(prompt_max, sizeof(prompt_max), "%s maximo (-1 sin filtro): ", nombre);

    while (1)
    {
        min_val = partido_listado_pedir_opcional_menos1_todos(prompt_min, min, max);
        max_val = partido_listado_pedir_opcional_menos1_todos(prompt_max, min, max);

        if (min_val >= 0 && max_val >= 0 && max_val < min_val)
        {
            int tmp = min_val;
            min_val = max_val;
            max_val = tmp;
        }

        if (max_span > 0 && min_val >= 0 && max_val >= 0 && (max_val - min_val) > max_span)
        {
            ui_printf_centered_line("El rango de %s no puede superar %d.", nombre, max_span);
            pause_console();
            continue;
        }

        break;
    }

    *min_out = min_val;
    *max_out = max_val;
}

static void partido_listado_mostrar_opciones_cancha(void)
{
    sqlite3_stmt *stmt;

    ui_printf_centered_line("Opciones de cancha:");
    ui_printf_centered_line("0) Todas");

    if (!preparar_stmt("SELECT id, nombre FROM cancha WHERE IFNULL(activa, 1) = 1 ORDER BY id", &stmt))
    {
        ui_printf_centered_line("(No se pudieron cargar canchas)");
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d) %s",
                                sqlite3_column_int(stmt, 0),
                                stmt_text_or_default(stmt, 1, "N/A"));
    }

    sqlite3_finalize(stmt);
}

static void partido_listado_mostrar_opciones_camiseta(void)
{
    sqlite3_stmt *stmt;

    ui_printf_centered_line("Opciones de camiseta:");
    ui_printf_centered_line("0) Todas");

    if (!preparar_stmt("SELECT id, nombre FROM camiseta WHERE IFNULL(activa, 1) = 1 ORDER BY id", &stmt))
    {
        ui_printf_centered_line("(No se pudieron cargar camisetas)");
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d) %s",
                                sqlite3_column_int(stmt, 0),
                                stmt_text_or_default(stmt, 1, "N/A"));
    }

    sqlite3_finalize(stmt);
}

static void partido_listado_mostrar_opciones_tipo_partido(void)
{
    ui_printf_centered_line("Opciones de tipo:");
    ui_printf_centered_line("0) Todos");
    ui_printf_centered_line("1) Amistoso");
    ui_printf_centered_line("2) Torneo");
    ui_printf_centered_line("3) Entrenamiento");
}

static void partido_listado_mostrar_opciones_clima(void)
{
    ui_printf_centered_line("Opciones de clima:");
    ui_printf_centered_line("0) Todos");
    ui_printf_centered_line("1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso");
    ui_printf_centered_line("5=Mucho Calor, 6=Mucho Frio, 7=Frio, 8=Calor");
    ui_printf_centered_line("9=Llovizna leve, 10=Lluvia Moderada, 11=Lluvia fuerte, 12=Cancha inundada");
}

static void partido_listado_mostrar_opciones_franja(void)
{
    ui_printf_centered_line("Opciones de franja horaria:");
    ui_printf_centered_line("0) Todas");
    ui_printf_centered_line("1=Madrugada, 2=Manana, 3=Mediodia");
    ui_printf_centered_line("4=Tarde, 5=Atardecer, 6=Noche");
}

static void partido_listado_mostrar_ayuda_rango(const char *nombre, int min, int max, int max_span)
{
    ui_printf_centered_line("%s: ingrese MIN y MAX", nombre);
    ui_printf_centered_line("Use -1 para dejar sin filtro");
    ui_printf_centered_line("Rango permitido: %d a %d", min, max);

    if (max_span > 0)
    {
        ui_printf_centered_line("Diferencia maxima entre MIN y MAX: %d", max_span);
    }
}

static void partido_listado_mostrar_ayuda_tag(void)
{
    ui_printf_centered_line("Filtro por tag (busqueda parcial)");
    ui_printf_centered_line("Ejemplos: final, torneo, importante");
    ui_printf_centered_line("Enter vacio para limpiar el filtro");
}

static void partido_listado_mostrar_ayuda_favoritos(void)
{
    ui_printf_centered_line("Solo favoritos:");
    ui_printf_centered_line("Si = muestra solo partidos marcados como favorito");
    ui_printf_centered_line("No = no aplica ese filtro");
}

static void partido_listado_imprimir_resumen_rango(const char *label, int min_val, int max_val)
{
    if (min_val < 0 && max_val < 0)
    {
        ui_printf_centered_line("%s: Todos", label);
        return;
    }

    if (min_val >= 0 && max_val >= 0)
    {
        ui_printf_centered_line("%s: %d a %d", label, min_val, max_val);
        return;
    }

    if (min_val >= 0)
    {
        ui_printf_centered_line("%s: >= %d", label, min_val);
        return;
    }

    ui_printf_centered_line("%s: <= %d", label, max_val);
}

static void partido_listado_imprimir_resumen_filtros(const PartidoListadoFiltros *filtros)
{
    char tag_mostrar[64];
    int modo_goles = partido_listado_get_modo_presencia_goles(filtros);
    int modo_asistencias = partido_listado_get_modo_presencia_asistencias(filtros);

    if (filtros->cancha_id > 0)
    {
        ui_printf_centered_line("1) Cancha ID: %d", filtros->cancha_id);
    }
    else
    {
        ui_printf_centered_line("1) Cancha ID: Todas");
    }

    if (filtros->camiseta_id > 0)
    {
        ui_printf_centered_line("2) Camiseta ID: %d", filtros->camiseta_id);
    }
    else
    {
        ui_printf_centered_line("2) Camiseta ID: Todas");
    }

    ui_printf_centered_line("3) Tipo de partido: %s", partido_listado_texto_tipo(filtros->tipo_partido));
    partido_listado_imprimir_resumen_rango("4) Goles", filtros->goles_min, filtros->goles_max);
    partido_listado_imprimir_resumen_rango("5) Asistencias", filtros->asistencias_min, filtros->asistencias_max);
    partido_listado_imprimir_resumen_rango("6) Precio", filtros->precio_min, filtros->precio_max);
    partido_listado_imprimir_resumen_rango("7) Rendimiento", filtros->rendimiento_min, filtros->rendimiento_max);
    partido_listado_imprimir_resumen_rango("8) Cansancio", filtros->cansancio_min, filtros->cansancio_max);
    partido_listado_imprimir_resumen_rango("9) Estado de animo", filtros->estado_animo_min, filtros->estado_animo_max);

    if (filtros->clima >= 1)
    {
        ui_printf_centered_line("10) Clima: %d", filtros->clima);
    }
    else
    {
        ui_printf_centered_line("10) Clima: Todos");
    }

    if (filtros->dia >= 1)
    {
        ui_printf_centered_line("11) Franja horaria: %s", dia_to_text(filtros->dia));
    }
    else
    {
        ui_printf_centered_line("11) Franja horaria: Todas");
    }

    snprintf(tag_mostrar, sizeof(tag_mostrar), "%s", filtros->tag[0] ? filtros->tag : "(sin filtro)");
    ui_printf_centered_line("12) Tag contiene: %s", tag_mostrar);
    ui_printf_centered_line("13) Solo favoritos: %s", filtros->solo_favoritos ? "Si" : "No");
    ui_printf_centered_line("14) Limpiar filtros");
    ui_printf_centered_line("15) Presencia de goles: %s", partido_listado_texto_presencia_goles(modo_goles));
    ui_printf_centered_line("16) Presencia de asistencias: %s", partido_listado_texto_presencia_asistencias(modo_asistencias));
}

static int partido_listado_aplicar_opcion_filtro_identidad(PartidoListadoFiltros *filtros, int opcion)
{
    if (!filtros)
    {
        return 0;
    }

    switch (opcion)
    {
    case 1:
        partido_listado_mostrar_opciones_cancha();
        filtros->cancha_id = partido_listado_pedir_opcional_0_todos("ID de cancha (0=todas): ", 1, 1000000);
        return 1;
    case 2:
        partido_listado_mostrar_opciones_camiseta();
        filtros->camiseta_id = partido_listado_pedir_opcional_0_todos("ID de camiseta (0=todas): ", 1, 1000000);
        return 1;
    case 3:
        partido_listado_mostrar_opciones_tipo_partido();
        filtros->tipo_partido = partido_listado_pedir_opcional_0_todos("Tipo (0=todos, 1=Amistoso, 2=Torneo, 3=Entrenamiento): ", 1, 3);
        return 1;
    default:
        return 0;
    }
}

static int partido_listado_aplicar_opcion_filtro_rango(PartidoListadoFiltros *filtros, int opcion)
{
    switch (opcion)
    {
    case 4:
        partido_listado_mostrar_ayuda_rango("Goles", 0, 10, 0);
        partido_listado_configurar_rango("Goles", 0, 10, 0, &filtros->goles_min, &filtros->goles_max);
        return 1;
    case 5:
        partido_listado_mostrar_ayuda_rango("Asistencias", 0, 10, 0);
        partido_listado_configurar_rango("Asistencias", 0, 10, 0, &filtros->asistencias_min, &filtros->asistencias_max);
        return 1;
    case 6:
        partido_listado_mostrar_ayuda_rango("Precio", 0, 1000000, 0);
        partido_listado_configurar_rango("Precio", 0, 1000000, 0, &filtros->precio_min, &filtros->precio_max);
        return 1;
    case 7:
        partido_listado_mostrar_ayuda_rango("Rendimiento", 0, 10, 0);
        partido_listado_configurar_rango("Rendimiento", 0, 10, 0, &filtros->rendimiento_min, &filtros->rendimiento_max);
        return 1;
    case 8:
        partido_listado_mostrar_ayuda_rango("Cansancio", 0, 10, 0);
        partido_listado_configurar_rango("Cansancio", 0, 10, 0, &filtros->cansancio_min, &filtros->cansancio_max);
        return 1;
    case 9:
        partido_listado_mostrar_ayuda_rango("Estado de animo", 0, 10, 0);
        partido_listado_configurar_rango("Estado de animo", 0, 10, 0, &filtros->estado_animo_min, &filtros->estado_animo_max);
        return 1;
    default:
        return 0;
    }
}

static int partido_listado_aplicar_opcion_filtro_extra(PartidoListadoFiltros *filtros, int opcion)
{
    int modo;

    switch (opcion)
    {
    case 10:
        partido_listado_mostrar_opciones_clima();
        filtros->clima = partido_listado_pedir_opcional_0_todos("Clima (0=todos, 1..12): ", 1, 12);
        return 1;
    case 11:
        partido_listado_mostrar_opciones_franja();
        filtros->dia = partido_listado_pedir_opcional_0_todos("Franja horaria (0=todas, 1..6): ", 1, 6);
        return 1;
    case 12:
        partido_listado_mostrar_ayuda_tag();
        input_string_extended("Tag (texto, Enter para limpiar): ", filtros->tag, (int)sizeof(filtros->tag));
        trim_whitespace(filtros->tag);
        return 1;
    case 13:
        partido_listado_mostrar_ayuda_favoritos();
        filtros->solo_favoritos = !filtros->solo_favoritos;
        return 1;
    case 14:
        partido_listado_limpiar_filtros(filtros);
        return 1;
    case 15:
        modo = (partido_listado_get_modo_presencia_goles(filtros) + 1) % 3;
        partido_listado_set_modo_presencia_goles(filtros, modo);
        return 1;
    case 16:
        modo = (partido_listado_get_modo_presencia_asistencias(filtros) + 1) % 3;
        partido_listado_set_modo_presencia_asistencias(filtros, modo);
        return 1;
    default:
        return 0;
    }
}

static int partido_listado_aplicar_opcion_filtro(PartidoListadoFiltros *filtros, int opcion)
{
    if (!filtros)
    {
        return 0;
    }

    if (partido_listado_aplicar_opcion_filtro_identidad(filtros, opcion))
    {
        return 1;
    }

    if (partido_listado_aplicar_opcion_filtro_rango(filtros, opcion))
    {
        return 1;
    }

    return partido_listado_aplicar_opcion_filtro_extra(filtros, opcion);
}

static void partido_listado_menu_filtros(PartidoListadoFiltros *filtros)
{
    if (!filtros)
    {
        return;
    }

    while (1)
    {
        clear_screen();
        print_header("FILTROS DE PARTIDOS");
        partido_listado_imprimir_resumen_filtros(filtros);
        ui_printf_centered_line("0) Volver");

        int opcion = input_int("Opcion: ");
        if (opcion == 0)
        {
            return;
        }

        if (!partido_listado_aplicar_opcion_filtro(filtros, opcion))
        {
            ui_printf_centered_line("Opcion invalida.");
            pause_console();
        }
    }
}

static void partido_listado_ir_pagina_anterior(int *pagina_actual)
{
    if (!pagina_actual)
    {
        return;
    }

    if (*pagina_actual > 1)
    {
        (*pagina_actual)--;
        return;
    }

    ui_printf_centered_line("Ya esta en la primera pagina.");
    pause_console();
}

static void partido_listado_ir_pagina_siguiente(int total_partidos, int total_paginas, int *pagina_actual)
{
    if (!pagina_actual)
    {
        return;
    }

    if (total_partidos <= 0 || *pagina_actual >= total_paginas)
    {
        ui_printf_centered_line("Ya esta en la ultima pagina.");
        pause_console();
        return;
    }

    (*pagina_actual)++;
}

static void partido_listado_ir_a_pagina(int total_partidos, int total_paginas, int *pagina_actual)
{
    if (!pagina_actual)
    {
        return;
    }

    if (total_partidos <= 0)
    {
        ui_printf_centered_line("No hay paginas disponibles con los filtros actuales.");
        pause_console();
        return;
    }

    int destino = input_int("Numero de pagina: ");
    if (destino >= 1 && destino <= total_paginas)
    {
        *pagina_actual = destino;
        return;
    }

    ui_printf_centered_line("Pagina invalida (1 a %d).", total_paginas);
    pause_console();
}

static void partido_listado_cambiar_paginacion(int *partidos_por_pagina, int *pagina_actual)
{
    if (!partidos_por_pagina || !pagina_actual)
    {
        return;
    }

    int nuevo_valor = partido_listado_menu_paginacion(*partidos_por_pagina);
    if (nuevo_valor != *partidos_por_pagina)
    {
        *partidos_por_pagina = nuevo_valor;
        partido_listado_guardar_paginacion(*partidos_por_pagina);
        *pagina_actual = 1;
    }
}

static void partido_listado_cambiar_orden(int *orden_desc, int *pagina_actual)
{
    if (!orden_desc || !pagina_actual)
    {
        return;
    }

    *orden_desc = partido_listado_menu_orden(*orden_desc);
    *pagina_actual = 1;
}

static void partido_listado_abrir_filtros(PartidoListadoFiltros *filtros, int *pagina_actual)
{
    if (!filtros || !pagina_actual)
    {
        return;
    }

    partido_listado_menu_filtros(filtros);
    *pagina_actual = 1;
}

static int partido_listado_manejar_opcion(int opcion,
        int total_partidos,
        int total_paginas,
        int *pagina_actual,
        int *partidos_por_pagina,
        int *orden_desc,
        PartidoListadoFiltros *filtros)
{
    switch (opcion)
    {
    case 0:
        return 0;
    case 1:
        partido_listado_ir_pagina_anterior(pagina_actual);
        return 1;
    case 2:
        partido_listado_ir_pagina_siguiente(total_partidos, total_paginas, pagina_actual);
        return 1;
    case 3:
        partido_listado_ir_a_pagina(total_partidos, total_paginas, pagina_actual);
        return 1;
    case 4:
        partido_listado_cambiar_paginacion(partidos_por_pagina, pagina_actual);
        return 1;
    case 5:
        partido_listado_abrir_filtros(filtros, pagina_actual);
        return 1;
    case 6:
        partido_listado_cambiar_orden(orden_desc, pagina_actual);
        return 1;
    default:
        ui_printf_centered_line("Opcion invalida.");
        pause_console();
        return 1;
    }
}

typedef struct
{
    int goles_equipo;
    int goles_rival;
    int tarjeta;
    int goles_en_contra;
} DatosPartidoFormalMarcador;

typedef struct
{
    int dolor_fisico;
    int arbitraje_score;
    int estado_cancha;
    int temperatura_registrada;
    double temperatura_c;
    char condicion_cancha[60];
    char arbitraje[60];
} DatosPartidoFormalContexto;

typedef struct
{
    char lo_mejor[300];
    char que_mejorar[300];
    char tags[300];
    char eventos_clave[300];
} DatosPartidoFormalNotas;

typedef struct
{
    int tecnico;
    int fisico;
    int mental;
} DatosPartidoFormalRating;

typedef struct
{
    char rival_nombre[100];
    char tipo_rival[40];
    char formato_partido[24];
    char posicion_jugada[40];
    int minutos_jugados;
    int intensidad;
    DatosPartidoFormalMarcador marcador;
    DatosPartidoFormalContexto contexto;
    DatosPartidoFormalNotas notas;
    DatosPartidoFormalRating rating;
} DatosPartidoFormal;

typedef struct
{
    int cancha_id;
    int goles;
    int asistencias;
    int camiseta;
    int resultado;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    char comentario_personal[256];
    int clima;
    int dia;
    int precio;
    int tipo_partido;
    char goles_detalle[512];
    char asistencias_detalle[512];
    DatosPartidoFormal formal;
} DatosPartido;

typedef struct
{
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} EstadisticasPartido;

typedef struct
{
    Equipo equipo_local;
    Equipo equipo_visitante;
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} DatosSimulacion;

static int secure_rand(int max)
{
    if (max <= 0)
        return 0;
    unsigned char rand_bytes[4];
    if (secure_random_bytes(rand_bytes, sizeof(rand_bytes)) == 0)
    {
        unsigned int r = (rand_bytes[0] << 24) | (rand_bytes[1] << 16) |
                         (rand_bytes[2] << 8) | rand_bytes[3];
        return (int)(r % max);
    }
    return (int)(((unsigned int)(time(NULL) ^ clock())) % max);
}

static int cancha_esta_activa(int cancha_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT IFNULL(activa, 1) FROM cancha WHERE id = ?", &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, cancha_id);
    int activa = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activa = sqlite3_column_int(stmt, 0) == 1;
    }
    sqlite3_finalize(stmt);
    return activa;
}

static int camiseta_esta_activa(int camiseta_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT IFNULL(activa, 1) FROM camiseta WHERE id = ?", &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, camiseta_id);
    int activa = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activa = sqlite3_column_int(stmt, 0) == 1;
    }
    sqlite3_finalize(stmt);
    return activa;
}

static int verificar_prerrequisitos_partido()
{
    sqlite3_stmt *stmt_count_canchas;
    if (!preparar_stmt("SELECT COUNT(*) FROM cancha WHERE IFNULL(activa, 1) = 1", &stmt_count_canchas))
    {
        return 0;
    }
    sqlite3_step(stmt_count_canchas);
    int count_canchas = sqlite3_column_int(stmt_count_canchas, 0);
    sqlite3_finalize(stmt_count_canchas);

    sqlite3_stmt *stmt_count_camisetas;
    if (!preparar_stmt("SELECT COUNT(*) FROM camiseta WHERE IFNULL(activa, 1) = 1", &stmt_count_camisetas))
    {
        return 0;
    }
    sqlite3_step(stmt_count_camisetas);
    int count_camisetas = sqlite3_column_int(stmt_count_camisetas, 0);
    sqlite3_finalize(stmt_count_camisetas);

    if (count_canchas == 0 || count_camisetas == 0)
    {
        if (count_canchas == 0 && count_camisetas == 0)
            printf("No se puede crear un partido porque no hay canchas ni camisetas activas registradas.\n");
        else if (count_canchas == 0)
            printf("No se puede crear un partido porque no hay canchas activas registradas.\n");
        else
            printf("No se puede crear un partido porque no hay camisetas activas registradas.\n");
        pause_console();
        return 0;
    }
    return 1;
}

static void listar_canchas_disponibles()
{
    ui_printf_centered_line("Canchas disponibles:");
    sqlite3_stmt *stmt_canchas;
    if (!preparar_stmt("SELECT id, nombre FROM cancha WHERE IFNULL(activa, 1) = 1 ORDER BY id", &stmt_canchas))
    {
        return;
    }
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d | %s", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);
}

static void listar_camisetas_disponibles()
{
    ui_printf_centered_line("Camisetas disponibles:");
    sqlite3_stmt *stmt_camisetas;
    if (!preparar_stmt("SELECT id, nombre FROM camiseta WHERE IFNULL(activa, 1) = 1 ORDER BY id", &stmt_camisetas))
    {
        return;
    }
    while (sqlite3_step(stmt_camisetas) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d | %s", sqlite3_column_int(stmt_camisetas, 0), sqlite3_column_text(stmt_camisetas, 1));
    }
    sqlite3_finalize(stmt_camisetas);
}

/* Muestra el listado de canchas con la opcion "Nueva Cancha" al final */
static void listar_canchas_con_nueva()
{
    listar_canchas_disponibles();
    ui_printf_centered_line("-1 | [+ Nueva Cancha]");
}

/* Muestra el listado de camisetas con la opcion "Nueva Camiseta" al final */
static void listar_camisetas_con_nueva()
{
    listar_camisetas_disponibles();
    ui_printf_centered_line("-1 | [+ Nueva Camiseta]");
}

static int crear_entidad_inline(const char *tabla,
                                const char *prompt_nombre,
                                int tam_nombre,
                                const char *error_creacion,
                                const char *error_guardado,
                                const char *etiqueta_entidad)
{
    char nombre[100];
    int tam_efectivo = tam_nombre;

    if (tam_efectivo <= 0 || tam_efectivo > (int)sizeof(nombre))
    {
        tam_efectivo = (int)sizeof(nombre);
    }

    input_string(prompt_nombre, nombre, tam_efectivo);
    trim_whitespace(nombre);
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        return 0;
    }

    long long id = obtener_siguiente_id(tabla);
    char sql[96];
    snprintf(sql, sizeof(sql), "INSERT INTO %s(id, nombre) VALUES(?, ?)", tabla);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("%s\n", error_creacion);
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        printf("%s \"%s\" creada con ID %lld.\n", etiqueta_entidad, nombre, id);
        return (int)id;
    }

    printf("%s\n", error_guardado);
    return 0;
}

/* Crea una cancha nueva de forma rapida durante la creacion de un partido.
   Devuelve el nuevo ID, o 0 si fallo. */
static int crear_cancha_inline(void)
{
    return crear_entidad_inline("cancha",
                                "Nombre de la nueva cancha: ",
                                100,
                                "Error al crear la cancha.",
                                "Error al guardar la cancha.",
                                "Cancha");
}

/* Crea una camiseta nueva de forma rapida durante la creacion de un partido.
   Devuelve el nuevo ID, o 0 si fallo. */
static int crear_camiseta_inline(void)
{
    return crear_entidad_inline("camiseta",
                                "Nombre y Numero de la nueva camiseta: ",
                                50,
                                "Error al crear la camiseta.",
                                "Error al guardar la camiseta.",
                                "Camiseta");
}

/* Pide un ID de cancha; si el usuario ingresa -1 ofrece crear una nueva.
   Devuelve 0 si el usuario cancela. */
static int pedir_cancha_o_nueva(int permite_cancelar)
{
    while (1)
    {
        int id = input_int("ID Cancha, (-1 Nueva Cancha, 0 para Cancelar): ");
        if (permite_cancelar && id == 0)
            return 0;
        if (id == -1)
        {
            int nuevo_id = crear_cancha_inline();
            if (nuevo_id > 0)
                return nuevo_id;
            /* Si fallo volver a mostrar lista */
            listar_canchas_con_nueva();
            continue;
        }
        if (existe_id("cancha", id))
        {
            if (cancha_esta_activa(id))
                return id;

            printf("La cancha esta inactiva. Seleccione una cancha activa o cree una nueva.\n");
            continue;
        }
        printf("La cancha no existe. Intente nuevamente.\n");
    }
}

/* Pide un ID de camiseta; si el usuario ingresa -1 ofrece crear una nueva. */
static int pedir_camiseta_o_nueva(void)
{
    while (1)
    {
        int id = input_int("ID Camiseta, (-1 Nueva Camiseta): ");
        if (id == -1)
        {
            int nuevo_id = crear_camiseta_inline();
            if (nuevo_id > 0)
                return nuevo_id;
            listar_camisetas_con_nueva();
            continue;
        }
        if (existe_id("camiseta", id))
        {
            if (camiseta_esta_activa(id))
                return id;

            printf("La camiseta esta inactiva. Seleccione una camiseta activa o cree una nueva.\n");
            continue;
        }
        printf("La camiseta no existe. Intente nuevamente.\n");
    }
}

static void inicializar_datos_partido(DatosPartido *datos)
{
    memset(datos, 0, sizeof(*datos));
    strcpy_s(datos->comentario_personal, sizeof(datos->comentario_personal), "");
    datos->tipo_partido = 1;
    strcpy_s(datos->goles_detalle, sizeof(datos->goles_detalle), "");
    strcpy_s(datos->asistencias_detalle, sizeof(datos->asistencias_detalle), "");
    strcpy_s(datos->formal.rival_nombre, sizeof(datos->formal.rival_nombre), "");
    strcpy_s(datos->formal.tipo_rival, sizeof(datos->formal.tipo_rival), "");
    strcpy_s(datos->formal.formato_partido, sizeof(datos->formal.formato_partido), "");
    strcpy_s(datos->formal.posicion_jugada, sizeof(datos->formal.posicion_jugada), "");
    datos->formal.minutos_jugados = 0;
    datos->formal.intensidad = 0;
    datos->formal.contexto.dolor_fisico = 0;
    datos->formal.contexto.arbitraje_score = 0;
    datos->formal.contexto.estado_cancha = 0;
    datos->formal.marcador.goles_equipo = -1;
    datos->formal.marcador.goles_rival = -1;
    datos->formal.marcador.tarjeta = 1;
    datos->formal.marcador.goles_en_contra = 0;
    datos->formal.contexto.temperatura_registrada = 0;
    datos->formal.contexto.temperatura_c = 0.0;
    strcpy_s(datos->formal.contexto.condicion_cancha, sizeof(datos->formal.contexto.condicion_cancha), "");
    strcpy_s(datos->formal.contexto.arbitraje, sizeof(datos->formal.contexto.arbitraje), "");
    strcpy_s(datos->formal.notas.lo_mejor, sizeof(datos->formal.notas.lo_mejor), "");
    strcpy_s(datos->formal.notas.que_mejorar, sizeof(datos->formal.notas.que_mejorar), "");
    strcpy_s(datos->formal.notas.tags, sizeof(datos->formal.notas.tags), "");
    strcpy_s(datos->formal.notas.eventos_clave, sizeof(datos->formal.notas.eventos_clave), "");
    datos->formal.rating.tecnico = 0;
    datos->formal.rating.fisico = 0;
    datos->formal.rating.mental = 0;
}

static UNUSED int pedir_id_existente(const char *prompt, const char *tabla,
                                     const char *mensaje_error, int permite_cancelar)
{
    while (1)
    {
        int id = input_int(prompt);
        if (permite_cancelar && id == 0)
        {
            return 0;
        }

        if (existe_id(tabla, id))
        {
            return id;
        }

        printf("%s\n", mensaje_error);
    }
}

static int pedir_entero_minimo(const char *prompt_inicial, int minimo, const char *prompt_error)
{
    int valor = input_int(prompt_inicial);
    while (valor < minimo)
    {
        valor = input_int(prompt_error);
    }
    return valor;
}

static int pedir_entero_en_rango(const char *prompt_inicial, int min, int max, const char *prompt_error)
{
    int valor = input_int(prompt_inicial);
    while (valor < min || valor > max)
    {
        valor = input_int(prompt_error);
    }
    return valor;
}

static const char *const TIPOS_GOL[] =
{
    "Derecha",
    "Izquierda",
    "Cabeza",
    "Pecho",
    "Rodilla",
    "Rebote raro",
    "Vaselina",
    "Tiro libre",
    "Penal",
    "Gol olimpico",
    "Jugada"
};

static const char *const TIPOS_ASISTENCIA[] =
{
    "Pase corto",
    "Pase filtrado",
    "Centro",
    "Pelota parada",
    "Pared (1-2)",
    "Pase largo"
};

static const char *detalle_tipo_to_text(int indice, int es_asistencia)
{
    const char *const *lista = es_asistencia ? TIPOS_ASISTENCIA : TIPOS_GOL;
    int cantidad = es_asistencia
                   ? (int)(sizeof(TIPOS_ASISTENCIA) / sizeof(TIPOS_ASISTENCIA[0]))
                   : (int)(sizeof(TIPOS_GOL) / sizeof(TIPOS_GOL[0]));

    if (indice < 0 || indice >= cantidad)
    {
        return "N/A";
    }
    return lista[indice];
}

static int detalle_tipo_cantidad(int es_asistencia)
{
    return es_asistencia
           ? (int)(sizeof(TIPOS_ASISTENCIA) / sizeof(TIPOS_ASISTENCIA[0]))
           : (int)(sizeof(TIPOS_GOL) / sizeof(TIPOS_GOL[0]));
}

static void detalle_tipo_imprimir_lista(int es_asistencia, const char *etiqueta_tipo)
{
    int cantidad = detalle_tipo_cantidad(es_asistencia);
    const char *const *lista = es_asistencia ? TIPOS_ASISTENCIA : TIPOS_GOL;

    printf("\nTipos de %s disponibles:\n", etiqueta_tipo);
    for (int i = 0; i < cantidad; i++)
    {
        printf("  %2d) %s\n", i + 1, lista[i]);
    }
    printf("   0) Finalizar (dejar lo cargado hasta ahora)\n");
}

static void detalle_tipo_mostrar_cargados(const char *etiqueta_tipo, const char *buffer)
{
    if (!buffer || buffer[0] == '\0')
    {
        printf("%s cargados: (ninguno)\n", etiqueta_tipo);
        return;
    }
    printf("%s cargados: %s\n", etiqueta_tipo, buffer);
}

/**
 * @brief Pide al usuario el detalle de varios eventos (goles o asistencias).
 *
 * Muestra una lista numerada de tipos. El usuario elige uno y se agrega
 * al buffer. Se repite hasta que el usuario haya alcanzado la cantidad
 * objetivo de eventos o elija "Finalizar".
 *
 * @param cantidad_objetivo Cantidad de eventos que pidio el usuario (goles/asistencias)
 * @param es_asistencia 0 = goles, !=0 = asistencias
 * @param buffer Buffer destino donde se concatenan los tipos separados por coma
 * @param buffer_size Tamano del buffer
 */
static void pedir_detalle_evento(int cantidad_objetivo,
                                 int es_asistencia,
                                 char *buffer,
                                 size_t buffer_size)
{
    if (!buffer || buffer_size == 0 || cantidad_objetivo <= 0)
    {
        return;
    }

    buffer[0] = '\0';
    const char *etiqueta_tipo = es_asistencia ? "asistencia" : "gol";
    int cantidad_tipos = detalle_tipo_cantidad(es_asistencia);
    int cargados = 0;

    while (cargados < cantidad_objetivo)
    {
        printf("\n--- Detalle de %s (%d de %d) ---\n",
               etiqueta_tipo, cargados, cantidad_objetivo);
        detalle_tipo_mostrar_cargados(
            es_asistencia ? "Asistencias" : "Goles", buffer);
        detalle_tipo_imprimir_lista(es_asistencia, etiqueta_tipo);

        char prompt[64];
        snprintf(prompt, sizeof(prompt),
                 "Tipo de %s #%d (0 para finalizar): ",
                 etiqueta_tipo, cargados + 1);
        int opcion = input_int(prompt);

        if (opcion == 0)
        {
            break;
        }

        if (opcion < 1 || opcion > cantidad_tipos)
        {
            printf("Opcion invalida. Ingrese un numero entre 1 y %d, o 0 para finalizar.\n",
                   cantidad_tipos);
            pause_console();
            continue;
        }

        const char *tipo = detalle_tipo_to_text(opcion - 1, es_asistencia);

        if (buffer[0] == '\0')
        {
            snprintf(buffer, buffer_size, "%s", tipo);
        }
        else
        {
            size_t usados = strlen_s(buffer, buffer_size);
            if (usados + 1 + strlen_s(tipo, 32) + 1 < buffer_size)
            {
                snprintf(buffer + usados, buffer_size - usados, ",%s", tipo);
            }
            else
            {
                printf("No hay mas espacio para agregar tipos. Se finaliza el detalle.\n");
                pause_console();
                break;
            }
        }
        cargados++;
    }
}

static const char *estado_cancha_to_text(int estado_cancha)
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

static const char *dolor_fisico_to_text(int dolor_fisico)
{
    switch (dolor_fisico)
    {
    case 1:
        return "Leve";
    case 2:
        return "Moderada";
    case 3:
        return "Fuerte";
    case 0:
    default:
        return "Ninguna";
    }
}

static const char *arbitraje_score_to_text(int arbitraje_score)
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
        return "N/A";
    }
}

static const char *tarjeta_to_text(int tarjeta)
{
    switch (tarjeta)
    {
    case 2:
        return "Amarilla";
    case 3:
        return "Roja";
    case 1:
    default:
        return "No";
    }
}

static void mostrar_opciones_clima_partido(void)
{
    ui_printf_centered_line("Opciones de clima:");
    ui_printf_centered_line("1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio");
    ui_printf_centered_line("7=Frio, 8=Calor, 9=Llovizna leve, 10=Lluvia Moderada, 11=Lluvia fuerte, 12=Cancha inundada");
}

static void mostrar_opciones_estado_cancha_partido(void)
{
    ui_printf_centered_line("Estado de cancha:");
    ui_printf_centered_line("1=Excelente, 2=Buena, 3=Regular, 4=Mala, 5=Pesima");
}

static void mostrar_opciones_tarjeta_partido(void)
{
    ui_printf_centered_line("Tarjeta:");
    ui_printf_centered_line("1=No, 2=Amarilla, 3=Roja");
}

static void mostrar_opciones_dolor_fisico_partido(void)
{
    ui_printf_centered_line("Dolor / molestia fisica:");
    ui_printf_centered_line("0=Ninguna, 1=Leve, 2=Moderada, 3=Fuerte");
}

static void mostrar_opciones_arbitraje_partido(void)
{
    ui_printf_centered_line("Arbitraje:");
    ui_printf_centered_line("1=Muy malo, 2=Regular, 3=Normal, 4=Bueno, 5=Excelente");
}

static void mostrar_opciones_dia_partido(void)
{
    ui_printf_centered_line("Franja horaria del partido:");
    ui_printf_centered_line("1=Madrugada (00:00-06:00)");
    ui_printf_centered_line("2=Manana (06:00-12:00)");
    ui_printf_centered_line("3=Mediodia (12:00-15:00)");
    ui_printf_centered_line("4=Tarde (15:00-19:00)");
    ui_printf_centered_line("5=Atardecer (19:00-21:00)");
    ui_printf_centered_line("6=Noche (21:00-00:00)");
}

static void solicitar_temperatura_opcional(double *temperatura_c, int *registrada)
{
    char buffer[64];

    while (1)
    {
        input_string_extended("Temperatura en C (opcional, Enter para omitir): ", buffer, (int)sizeof(buffer));
        trim_whitespace(buffer);
        if (buffer[0] == '\0')
        {
            *temperatura_c = 0.0;
            *registrada = 0;
            return;
        }

        char *endptr = NULL;
        double valor = strtod(buffer, &endptr);
        while (endptr && *endptr && isspace((unsigned char)*endptr))
        {
            endptr++;
        }

        if (endptr && *endptr == '\0' && valor >= -30.0 && valor <= 60.0)
        {
            *temperatura_c = valor;
            *registrada = 1;
            return;
        }

        printf("Temperatura invalida. Ingrese un numero entre -30 y 60, o Enter para omitir.\n");
    }
}

static void solicitar_texto_no_vacio(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string_extended(prompt, buffer, size);
        trim_whitespace(buffer);
        if (buffer[0] != '\0')
        {
            return;
        }
        printf("El campo no puede estar vacio.\n");
    }
}

static int seleccionar_modalidad_partido(void)
{
    while (1)
    {
        clear_screen();
        print_header("TIPO DE PARTIDO");
        printf("1) Amistoso\n");
        printf("2) Torneo\n");
        printf("3) Modo entrenamiento (sin rival ni resultado)\n");
        printf("0) Cancelar\n");

        int opcion = input_int("Opcion: ");
        if (opcion == 0 || opcion == 1 || opcion == 2 || opcion == 3)
        {
            return opcion;
        }
        printf("Opcion invalida.\n");
    }
}

static int solicitar_futbol_partido(char *buffer, int size, int permitir_cancelar)
{
    while (1)
    {
        printf("Futbol:\n");
        printf("1) Futbol 5\n");
        printf("2) Futbol 7\n");
        printf("3) Futbol 8\n");
        printf("4) Futbol 11\n");
        if (permitir_cancelar)
        {
            printf("5) Cancelar\n");
        }

        switch (input_int("Opcion: "))
        {
        case 1:
            snprintf(buffer, size, "Futbol 5");
            return 1;
        case 2:
            snprintf(buffer, size, "Futbol 7");
            return 1;
        case 3:
            snprintf(buffer, size, "Futbol 8");
            return 1;
        case 4:
            snprintf(buffer, size, "Futbol 11");
            return 1;
        case 5:
            if (permitir_cancelar)
            {
                return 0;
            }
            printf("Opcion invalida.\n");
            break;
        default:
            printf("Opcion invalida.\n");
            break;
        }
    }
}

static void solicitar_tipo_rival(char *buffer, int size)
{
    while (1)
    {
        printf("Tipo de rival:\n");
        printf("1) Amistoso\n");
        printf("2) Torneo\n");
        printf("3) Entrenamiento\n");
        printf("4) Otro\n");

        int opcion = input_int("Opcion: ");
        switch (opcion)
        {
        case 1:
            snprintf(buffer, size, "Amistoso");
            return;
        case 2:
            snprintf(buffer, size, "Torneo");
            return;
        case 3:
            snprintf(buffer, size, "Entrenamiento");
            return;
        case 4:
            solicitar_texto_no_vacio("Tipo de rival (texto): ", buffer, size);
            return;
        default:
            printf("Opcion invalida.\n");
            break;
        }
    }
}

static void recopilar_notas_y_ratings_formales(DatosPartido *datos, const char *prompt_lo_mejor)
{
    if (!datos || !prompt_lo_mejor)
    {
        return;
    }

    input_string_extended(prompt_lo_mejor, datos->formal.notas.lo_mejor, sizeof(datos->formal.notas.lo_mejor));
    trim_whitespace(datos->formal.notas.lo_mejor);
    if (datos->formal.notas.lo_mejor[0] == '\0')
    {
        snprintf(datos->formal.notas.lo_mejor, sizeof(datos->formal.notas.lo_mejor), "(sin registro)");
    }

    input_string_extended("Que mejorar: ", datos->formal.notas.que_mejorar, sizeof(datos->formal.notas.que_mejorar));
    trim_whitespace(datos->formal.notas.que_mejorar);
    if (datos->formal.notas.que_mejorar[0] == '\0')
    {
        snprintf(datos->formal.notas.que_mejorar, sizeof(datos->formal.notas.que_mejorar), "(sin registro)");
    }

    input_string_extended("Tags (separados por coma, opcional): ", datos->formal.notas.tags, sizeof(datos->formal.notas.tags));
    trim_whitespace(datos->formal.notas.tags);

    input_string_extended("Eventos clave: ", datos->formal.notas.eventos_clave, sizeof(datos->formal.notas.eventos_clave));
    trim_whitespace(datos->formal.notas.eventos_clave);
    if (datos->formal.notas.eventos_clave[0] == '\0')
    {
        snprintf(datos->formal.notas.eventos_clave, sizeof(datos->formal.notas.eventos_clave), "(sin eventos)");
    }

    datos->formal.rating.tecnico = pedir_entero_en_rango("Rating tecnico (1-10): ",
                                   1, 10,
                                   "Valor invalido. Ingrese entre 1 y 10: ");
    datos->formal.rating.fisico = pedir_entero_en_rango("Rating fisico (1-10): ",
                                  1, 10,
                                  "Valor invalido. Ingrese entre 1 y 10: ");
    datos->formal.rating.mental = pedir_entero_en_rango("Rating mental (1-10): ",
                                  1, 10,
                                  "Valor invalido. Ingrese entre 1 y 10: ");
}

static void recopilar_datos_formales(DatosPartido *datos)
{
    solicitar_texto_no_vacio("Rival: ", datos->formal.rival_nombre, sizeof(datos->formal.rival_nombre));
    solicitar_tipo_rival(datos->formal.tipo_rival, sizeof(datos->formal.tipo_rival));
    datos->formal.marcador.goles_equipo = pedir_entero_minimo("Marcador global - goles de tu equipo: ", 0,
                                          "Valor invalido. Ingrese 0 o mas: ");
    datos->formal.marcador.goles_rival = pedir_entero_minimo("Marcador global - goles rival: ", 0,
                                         "Valor invalido. Ingrese 0 o mas: ");
    solicitar_texto_no_vacio("Posicion jugada: ", datos->formal.posicion_jugada, sizeof(datos->formal.posicion_jugada));
    datos->formal.minutos_jugados = pedir_entero_en_rango("Minutos jugados (0-180): ",
                                    0, 180,
                                    "Valor invalido. Ingrese entre 0 y 180: ");
    datos->formal.intensidad = pedir_entero_en_rango("Intensidad del partido (1-10): ",
                               1, 10,
                               "Valor invalido. Ingrese entre 1 y 10: ");
    mostrar_opciones_dolor_fisico_partido();
    datos->formal.contexto.dolor_fisico = pedir_entero_en_rango("Dolor/molestia fisica (0-3): ",
                                          0, 3,
                                          "Valor invalido. Ingrese entre 0 y 3: ");
    solicitar_temperatura_opcional(&datos->formal.contexto.temperatura_c, &datos->formal.contexto.temperatura_registrada);
    snprintf(datos->formal.contexto.condicion_cancha, sizeof(datos->formal.contexto.condicion_cancha), "%s",
             estado_cancha_to_text(datos->formal.contexto.estado_cancha));
    mostrar_opciones_tarjeta_partido();
    datos->formal.marcador.tarjeta = pedir_entero_en_rango("Tarjeta (1-3): ",
                                     1, 3,
                                     "Valor invalido. Ingrese 1, 2 o 3: ");
    datos->formal.marcador.goles_en_contra = pedir_entero_minimo("Goles en contra: ", 0,
            "Valor invalido. Ingrese 0 o mas: ");
    mostrar_opciones_arbitraje_partido();
    datos->formal.contexto.arbitraje_score = pedir_entero_en_rango("Arbitraje (1-5): ",
            1, 5,
            "Valor invalido. Ingrese entre 1 y 5: ");
    snprintf(datos->formal.contexto.arbitraje, sizeof(datos->formal.contexto.arbitraje), "%s",
             arbitraje_score_to_text(datos->formal.contexto.arbitraje_score));

    recopilar_notas_y_ratings_formales(datos, "Lo mejor del partido: ");
}

static void recopilar_datos_entrenamiento(DatosPartido *datos)
{
    strcpy_s(datos->formal.rival_nombre, sizeof(datos->formal.rival_nombre), "");
    strcpy_s(datos->formal.tipo_rival, sizeof(datos->formal.tipo_rival), "Entrenamiento");
    solicitar_texto_no_vacio("Posicion jugada: ", datos->formal.posicion_jugada, sizeof(datos->formal.posicion_jugada));
    datos->formal.minutos_jugados = pedir_entero_en_rango("Minutos entrenados (0-180): ",
                                    0, 180,
                                    "Valor invalido. Ingrese entre 0 y 180: ");
    datos->formal.intensidad = pedir_entero_en_rango("Intensidad del entrenamiento (1-10): ",
                               1, 10,
                               "Valor invalido. Ingrese entre 1 y 10: ");
    mostrar_opciones_dolor_fisico_partido();
    datos->formal.contexto.dolor_fisico = pedir_entero_en_rango("Dolor/molestia fisica (0-3): ",
                                          0, 3,
                                          "Valor invalido. Ingrese entre 0 y 3: ");
    solicitar_temperatura_opcional(&datos->formal.contexto.temperatura_c, &datos->formal.contexto.temperatura_registrada);
    mostrar_opciones_tarjeta_partido();
    datos->formal.marcador.tarjeta = pedir_entero_en_rango("Tarjeta (1-3): ",
                                     1, 3,
                                     "Valor invalido. Ingrese 1, 2 o 3: ");
    datos->formal.marcador.goles_en_contra = pedir_entero_minimo("Goles en contra: ", 0,
            "Valor invalido. Ingrese 0 o mas: ");
    snprintf(datos->formal.contexto.condicion_cancha, sizeof(datos->formal.contexto.condicion_cancha), "%s",
             estado_cancha_to_text(datos->formal.contexto.estado_cancha));
    datos->formal.contexto.arbitraje_score = 0;
    snprintf(datos->formal.contexto.arbitraje, sizeof(datos->formal.contexto.arbitraje), "Sin arbitro");

    recopilar_notas_y_ratings_formales(datos, "Lo mejor del entrenamiento: ");
}

static int recopilar_datos_partido_base(DatosPartido *datos, int solicita_resultado, int tipo_partido)
{
    if (!datos)
    {
        return 0;
    }

    inicializar_datos_partido(datos);

    listar_canchas_con_nueva();
    datos->cancha_id = pedir_cancha_o_nueva(1);
    if (datos->cancha_id == 0)
    {
        return 0;
    }

    mostrar_opciones_estado_cancha_partido();
    datos->formal.contexto.estado_cancha = pedir_entero_en_rango("Estado de cancha (1-5): ",
                                           1, 5,
                                           "Valor invalido. Ingrese entre 1 y 5: ");
    snprintf(datos->formal.contexto.condicion_cancha, sizeof(datos->formal.contexto.condicion_cancha), "%s",
             estado_cancha_to_text(datos->formal.contexto.estado_cancha));

    if (!solicitar_futbol_partido(datos->formal.formato_partido,
                                  (int)sizeof(datos->formal.formato_partido),
                                  1))
    {
        return 0;
    }

    datos->goles = pedir_entero_minimo("Goles: ", 0,
                                       "Goles invalidos. Ingrese 0 o mas: ");
    if (datos->goles > 0 && confirmar("Desea agregar el detalle de los goles?"))
    {
        pedir_detalle_evento(datos->goles, 0,
                             datos->goles_detalle, sizeof(datos->goles_detalle));
    }
    datos->asistencias = pedir_entero_minimo("Asistencias: ", 0,
                         "Asistencias invalidas. Ingrese 0 o mas: ");
    if (datos->asistencias > 0 && confirmar("Desea agregar el detalle de las asistencias?"))
    {
        pedir_detalle_evento(datos->asistencias, 1,
                             datos->asistencias_detalle, sizeof(datos->asistencias_detalle));
    }
    if (tipo_partido == 1)
    {
        datos->formal.marcador.goles_en_contra = pedir_entero_minimo("Goles en contra: ", 0,
                "Valor invalido. Ingrese 0 o mas: ");
    }
    if (solicita_resultado)
    {
        datos->resultado = pedir_entero_en_rango("Resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ",
                           1, 3,
                           "Resultado invalido. (1=VICTORIA, 2=EMPATE, 3=DERROTA):");
    }
    else
    {
        datos->resultado = 0;
    }

    listar_camisetas_con_nueva();
    datos->camiseta = pedir_camiseta_o_nueva();
    datos->rendimiento_general = pedir_entero_en_rango("Rendimiento general (1-10): ",
                                 1, 10,
                                 "Rendimiento invalido. Ingrese entre 1 y 10: ");
    datos->cansancio = pedir_entero_en_rango("Cansancio (1-10): ",
                       1, 10,
                       "Cansancio invalido. Ingrese entre 1 y 10:  ");
    datos->estado_animo = pedir_entero_en_rango("Estado de Animo (1-10): ",
                          1, 10,
                          "Estado de Animo invalido. Ingrese entre 1 y 10: ");
    input_string_extended("Comentario personal: ", datos->comentario_personal, 256);
    mostrar_opciones_clima_partido();
    datos->clima = pedir_entero_en_rango("Clima (1-12): ",
                                         1, 12,
                                         "Clima invalido. Ingrese entre 1 y 12: ");
    datos->dia = 0;
    datos->precio = pedir_entero_minimo("Precio del partido: ", 0,
                                        "Precio invalido. Ingrese 0 o mas: ");
    datos->tipo_partido = tipo_partido;

    return 1;
}

static int recopilar_datos_partido(DatosPartido *datos)
{
    return recopilar_datos_partido_base(datos, 1, 1);
}

static int recopilar_datos_partido_formal(DatosPartido *datos)
{
    if (!recopilar_datos_partido_base(datos, 1, 2))
    {
        return 0;
    }

    recopilar_datos_formales(datos);
    return 1;
}

static int recopilar_datos_partido_entrenamiento(DatosPartido *datos)
{
    if (!recopilar_datos_partido_base(datos, 0, 3))
    {
        return 0;
    }

    recopilar_datos_entrenamiento(datos);
    return 1;
}

static void insertar_partido(long long id, DatosPartido const *datos, char const *fecha)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO partido(id, cancha_id,fecha_hora,mes_anio,goles,asistencias,camiseta_id,resultado,rendimiento_general,cansancio,estado_animo,comentario_personal,clima,dia,precio,"
                "tipo_partido,rival_nombre,tipo_rival,posicion_jugada,minutos_jugados,intensidad,esfuerzo_percibido,condicion_cancha,arbitraje,eventos_clave,rating_tecnico,rating_fisico,rating_mental,"
                "estado_cancha,goles_equipo,goles_rival,formato_partido,tarjeta,goles_en_contra,dolor_fisico,temperatura_c,arbitraje_score,lo_mejor,que_mejorar,tags,goles_detalle,asistencias_detalle)"
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                &stmt))
    {
        printf("Error al preparar insercion de partido: %s\n", sqlite3_errmsg(db));
        pause_console();
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, datos->cancha_id);
    /* Convertir fecha a formato de almacenamiento (YYYY-MM-DD HH:MM) */
    char fecha_storage[64] = {0};
    convert_display_date_to_storage(fecha, fecha_storage, sizeof(fecha_storage));
    sqlite3_bind_text(stmt, 3, fecha_storage, -1, SQLITE_TRANSIENT);
    char mes_anio[8] = {0};
    if (strlen_s(fecha_storage, sizeof(fecha_storage)) >= 7 && fecha_storage[4] == '-')
    {
        snprintf(mes_anio, sizeof(mes_anio), "%.7s", fecha_storage);
    }
    sqlite3_bind_text(stmt, 4, mes_anio, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, datos->goles);
    sqlite3_bind_int(stmt, 6, datos->asistencias);
    sqlite3_bind_int(stmt, 7, datos->camiseta);
    sqlite3_bind_int(stmt, 8, datos->resultado);
    sqlite3_bind_int(stmt, 9, datos->rendimiento_general);
    sqlite3_bind_int(stmt, 10, datos->cansancio);
    sqlite3_bind_int(stmt, 11, datos->estado_animo);
    sqlite3_bind_text(stmt, 12, datos->comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, datos->clima);
    sqlite3_bind_int(stmt, 14, datos->dia);
    sqlite3_bind_int(stmt, 15, datos->precio);
    sqlite3_bind_int(stmt, 16, datos->tipo_partido);
    sqlite3_bind_text(stmt, 17, datos->formal.rival_nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 18, datos->formal.tipo_rival, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 19, datos->formal.posicion_jugada, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 20, datos->formal.minutos_jugados);
    sqlite3_bind_int(stmt, 21, datos->formal.intensidad);
    sqlite3_bind_int(stmt, 22, 0);
    sqlite3_bind_text(stmt, 23, datos->formal.contexto.condicion_cancha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 24, datos->formal.contexto.arbitraje, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 25, datos->formal.notas.eventos_clave, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 26, datos->formal.rating.tecnico);
    sqlite3_bind_int(stmt, 27, datos->formal.rating.fisico);
    sqlite3_bind_int(stmt, 28, datos->formal.rating.mental);
    sqlite3_bind_int(stmt, 29, datos->formal.contexto.estado_cancha);
    sqlite3_bind_int(stmt, 30, datos->formal.marcador.goles_equipo);
    sqlite3_bind_int(stmt, 31, datos->formal.marcador.goles_rival);
    sqlite3_bind_text(stmt, 32, datos->formal.formato_partido, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 33, datos->formal.marcador.tarjeta);
    sqlite3_bind_int(stmt, 34, datos->formal.marcador.goles_en_contra);
    sqlite3_bind_int(stmt, 35, datos->formal.contexto.dolor_fisico);
    if (datos->formal.contexto.temperatura_registrada)
    {
        sqlite3_bind_double(stmt, 36, datos->formal.contexto.temperatura_c);
    }
    else
    {
        sqlite3_bind_null(stmt, 36);
    }
    sqlite3_bind_int(stmt, 37, datos->formal.contexto.arbitraje_score);
    sqlite3_bind_text(stmt, 38, datos->formal.notas.lo_mejor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 39, datos->formal.notas.que_mejorar, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 40, datos->formal.notas.tags, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 41, datos->goles_detalle, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 42, datos->asistencias_detalle, -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(stmt);
    if (result == SQLITE_DONE)
    {
        char descripcion[100];
        snprintf(descripcion, sizeof(descripcion), "ID: %lld", id);
        mostrar_alerta_operacion("Partido", "Creado", descripcion);
    }
    else
    {
        printf("Error al crear el partido: %s\n", sqlite3_errmsg(db));
        pause_console();
    }
    sqlite3_finalize(stmt);
}

static void crear_transaccion_partido(long long partido_id, int precio)
{
    // Verificar si el partido ya tiene una transaccion asociada
    sqlite3_stmt *stmt_check;
    const char *sql_check = "SELECT COUNT(*) FROM financiamiento WHERE tipo = 1 AND categoria = 6 AND item_especifico LIKE ?";
    char item_pattern[256];
    snprintf(item_pattern, sizeof(item_pattern), "Partido ID: %lld%%", partido_id);

    if (preparar_stmt(sql_check, &stmt_check))
    {
        sqlite3_bind_text(stmt_check, 1, item_pattern, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt_check) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt_check, 0);
            if (count > 0)
            {
                printf("El partido ya tiene una transaccion financiera asociada.\n");
                sqlite3_finalize(stmt_check);
                return;
            }
        }
        sqlite3_finalize(stmt_check);
    }

    // Crear la transaccion financiera
    TransaccionFinanciera transaccion;
    transaccion.id = (int)obtener_siguiente_id("financiamiento");
    obtener_fecha_actual(transaccion.fecha);
    transaccion.tipo = GASTO;
    transaccion.categoria = CANCHAS;
    transaccion.monto = precio;
    strcpy_s(transaccion.descripcion, sizeof(transaccion.descripcion), "Pago por alquiler de cancha");

    // Obtener detalles del partido para el item_especifico
    sqlite3_stmt *stmt_partido;
    const char *sql_partido = "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                              "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                              "JOIN cancha can ON p.cancha_id = can.id WHERE p.id = ?";

    if (preparar_stmt(sql_partido, &stmt_partido))
    {
        sqlite3_bind_int64(stmt_partido, 1, partido_id);

        if (sqlite3_step(stmt_partido) == SQLITE_ROW)
        {
            // Formatear la fecha para visualizacion
            char fecha_formateada[20];
            format_date_for_display((const char *)sqlite3_column_text(stmt_partido, 2), fecha_formateada, sizeof(fecha_formateada));

            // Crear el string con los detalles del partido
            snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "(%lld |Cancha:%s |Fecha:%s | G:%d A:%d |Camiseta:%s | %s)",
                     sqlite3_column_int64(stmt_partido, 0),
                     sqlite3_column_text(stmt_partido, 1),
                     fecha_formateada,
                     sqlite3_column_int(stmt_partido, 3),
                     sqlite3_column_int(stmt_partido, 4),
                     sqlite3_column_text(stmt_partido, 5),
                     resultado_to_text(sqlite3_column_int(stmt_partido, 6)));
        }
        else
        {
            snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "Partido ID: %lld (no encontrado)", partido_id);
        }
        sqlite3_finalize(stmt_partido);
    }
    else
    {
        snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "Partido ID: %lld", partido_id);
    }

    // Insertar la transaccion en la base de datos
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO financiamiento (id, fecha, tipo, categoria, descripcion, monto, item_especifico) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, transaccion.id);
        sqlite3_bind_text(stmt, 2, transaccion.fecha, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, transaccion.tipo);
        sqlite3_bind_int(stmt, 4, transaccion.categoria);
        sqlite3_bind_text(stmt, 5, transaccion.descripcion, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, transaccion.monto);
        sqlite3_bind_text(stmt, 7, transaccion.item_especifico, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Transaccion financiera creada para el partido con ID %lld\n", partido_id);
        }
        else
        {
            printf("Error al crear la transaccion financiera: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        printf("Error al preparar la consulta de transaccion: %s\n", sqlite3_errmsg(db));
    }
}

void crear_partido()
{
    // Activar IA antes de crear partido
    activar_ia_antes_partido();

    if (!verificar_prerrequisitos_partido())
        return;

    int modalidad = seleccionar_modalidad_partido();
    if (modalidad == 0)
    {
        return;
    }

    DatosPartido datos;
    int datos_ok = 0;
    if (modalidad == 1)
    {
        datos_ok = recopilar_datos_partido(&datos);
    }
    else if (modalidad == 2)
    {
        datos_ok = recopilar_datos_partido_formal(&datos);
    }
    else
    {
        datos_ok = recopilar_datos_partido_entrenamiento(&datos);
    }

    if (!datos_ok)
        return;

    char fecha[32];
    solicitar_fecha_hora_partido(fecha, sizeof(fecha));

    if (!calcular_dia_desde_fecha_hora(fecha, &datos.dia))
    {
        char fecha_actual[32];
        get_datetime(fecha_actual, sizeof(fecha_actual));
        snprintf(fecha, sizeof(fecha), "%s", fecha_actual);
        if (!calcular_dia_desde_fecha_hora(fecha, &datos.dia))
        {
            datos.dia = 6;
        }
        printf("No se pudo interpretar la hora ingresada. Se uso fecha/hora actual: %s\n", fecha);
    }

    printf("Franja horaria calculada automaticamente: %s\n", dia_to_text(datos.dia));

    long long id = obtener_siguiente_id("partido");
    insertar_partido(id, &datos, fecha);

    // Crear transaccion financiera si el precio es mayor a 0
    if (datos.precio > 0)
    {
        crear_transaccion_partido(id, datos.precio);
    }
}

static int partido_listado_calcular_total_paginas(int total_partidos, int partidos_por_pagina)
{
    if (partidos_por_pagina > 0 && total_partidos > 0)
    {
        return (total_partidos + partidos_por_pagina - 1) / partidos_por_pagina;
    }
    return 1;
}

static void partido_listado_normalizar_pagina_actual(int *pagina_actual, int total_paginas)
{
    if (!pagina_actual)
    {
        return;
    }

    if (*pagina_actual > total_paginas)
    {
        *pagina_actual = total_paginas;
    }
    if (*pagina_actual < 1)
    {
        *pagina_actual = 1;
    }
}

static void partido_listado_imprimir_estado(int pagina_actual,
        int total_paginas,
        int total_partidos,
        int partidos_por_pagina,
        int orden_desc,
        const PartidoListadoFiltros *filtros)
{
    clear_screen();
    print_header("LISTADO DE PARTIDOS");
    ui_printf_centered_line("Pagina %d de %d | Total: %d partidos", pagina_actual, total_paginas, total_partidos);

    if (partidos_por_pagina == 0)
    {
        ui_printf_centered_line("Paginacion: %s", partido_listado_texto_paginacion(partidos_por_pagina));
    }
    else
    {
        ui_printf_centered_line("Paginacion: %d por pagina", partidos_por_pagina);
    }

    ui_printf_centered_line("Orden: %s", partido_listado_texto_orden(orden_desc));
    ui_printf_centered_line("Filtros activos: %d", partido_listado_contar_filtros_activos(filtros));
    ui_printf_centered_line("----------------------------------------");
}

static void partido_listado_mostrar_resultados(int pagina_actual,
        int partidos_por_pagina,
        int total_partidos,
        int orden_desc,
        const PartidoListadoFiltros *filtros)
{
    int hay = 0;

    if (total_partidos > 0)
    {
        hay = partido_listado_mostrar_pagina_actual(pagina_actual,
                partidos_por_pagina,
                total_partidos,
                orden_desc,
                filtros);
    }

    if (total_partidos <= 0 || !hay)
    {
        ui_printf_centered_line("No hay partidos para los filtros seleccionados.");
    }
}

static void partido_listado_imprimir_menu(int paginacion_todos)
{
    ui_printf_centered_line("----------------------------------------");

    if (!paginacion_todos)
    {
        ui_printf_centered_line("1) Pagina anterior");
        ui_printf_centered_line("2) Pagina siguiente");
        ui_printf_centered_line("3) Ir a pagina");
    }

    ui_printf_centered_line("4) Paginacion");
    ui_printf_centered_line("5) Filtros");
    ui_printf_centered_line("6) Orden");
    ui_printf_centered_line("0) Volver");
}

static int partido_listado_navegacion_deshabilitada(int paginacion_todos, int opcion)
{
    if (paginacion_todos && (opcion == 1 || opcion == 2 || opcion == 3))
    {
        ui_printf_centered_line("Navegacion por pagina deshabilitada en modo Todos.");
        pause_console();
        return 1;
    }

    return 0;
}

void listar_partidos()
{
    int partidos_por_pagina = partido_listado_cargar_paginacion();
    int orden_desc = 0;
    int pagina_actual = 1;
    PartidoListadoFiltros filtros;

    partido_listado_limpiar_filtros(&filtros);

    while (1)
    {
        int total_partidos = partido_listado_contar_total(&filtros);
        int total_paginas = partido_listado_calcular_total_paginas(total_partidos, partidos_por_pagina);
        int paginacion_todos = (partidos_por_pagina == 0);

        partido_listado_normalizar_pagina_actual(&pagina_actual, total_paginas);
        partido_listado_imprimir_estado(pagina_actual,
                                        total_paginas,
                                        total_partidos,
                                        partidos_por_pagina,
                                        orden_desc,
                                        &filtros);
        partido_listado_mostrar_resultados(pagina_actual,
                                           partidos_por_pagina,
                                           total_partidos,
                                           orden_desc,
                                           &filtros);
        partido_listado_imprimir_menu(paginacion_todos);

        int opcion = input_int("Opcion: ");

        if (partido_listado_navegacion_deshabilitada(paginacion_todos, opcion))
        {
            continue;
        }

        if (!partido_listado_manejar_opcion(opcion,
                                            total_partidos,
                                            total_paginas,
                                            &pagina_actual,
                                            &partidos_por_pagina,
                                            &orden_desc,
                                            &filtros))
        {
            return;
        }
    }
}

void eliminar_partido()
{
    print_header("ELIMINAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Eliminar (0 para cancelar): ");

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        return;
    }

    if (!confirmar("Seguro que desea eliminar este partido?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt("DELETE FROM partido WHERE id = ?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    mostrar_alerta_operacion("Partido", "Eliminado", NULL);
}

static int current_partido_id;


static void modificar_campo_partido(const char *campo, const char *prompt, const char *mensaje_exito,
                                    int min_val, int max_val, void (*mostrar_lista)(void))
{
    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (min_val != 0 || max_val != 0)
    {
        while (valor < min_val || valor > max_val)
        {
            char prompt_error[256];
            snprintf(prompt_error, sizeof(prompt_error), "Valor invalido. Ingrese entre %d y %d: ", min_val, max_val);
            valor = input_int(prompt_error);
        }
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    pause_console();
}

static void modificar_campo_texto_partido(const char *campo, const char *prompt, const char *mensaje_exito, int buffer_size)
{
    if (buffer_size <= 0)
    {
        printf("Tamaño de buffer invalido\n");
        return;
    }

    char *valor = (char *)malloc((size_t)buffer_size);
    if (!valor)
    {
        printf("Error de memoria\n");
        return;
    }

    printf("%s", prompt);
    size_t valor_size = (size_t)buffer_size;
    if (valor_size > (size_t)INT_MAX)
    {
        free(valor);
        return;
    }

    if (fgets(valor, (int)valor_size, stdin) == NULL)
    {
        valor[0] = '\0';
    }
    else
    {
        valor[strcspn(valor, "\n")] = '\0';
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        free(valor);
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    free(valor);
    pause_console();
}

static void buscar_partidos_generico(const char *header, const char *campo, const char *prompt,
                                     void (*mostrar_lista)(void), int validar_id)
{
    print_header(header);

    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (validar_id && !existe_id(campo, valor))
    {
        printf("El %s no existe.\n", campo);
        pause_console();
        return;
    }

    char sql[1900];
    snprintf(sql, sizeof(sql),
             "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio, "
             "IFNULL(p.tipo_partido, 1), IFNULL(p.rival_nombre, ''), IFNULL(p.tipo_rival, ''), IFNULL(p.posicion_jugada, ''), "
             "IFNULL(p.minutos_jugados, 0), IFNULL(p.intensidad, 0), IFNULL(p.esfuerzo_percibido, 0), IFNULL(p.condicion_cancha, ''), "
             "IFNULL(p.arbitraje, ''), IFNULL(p.eventos_clave, ''), IFNULL(p.rating_tecnico, 0), IFNULL(p.rating_fisico, 0), IFNULL(p.rating_mental, 0), "
             "IFNULL(p.estado_cancha, 0), IFNULL(p.goles_equipo, -1), IFNULL(p.goles_rival, -1), IFNULL(p.formato_partido, ''), IFNULL(p.tarjeta, 1), IFNULL(p.goles_en_contra, 0), "
             "IFNULL(p.dolor_fisico, 0), p.temperatura_c, IFNULL(p.arbitraje_score, 0), IFNULL(p.lo_mejor, ''), IFNULL(p.que_mejorar, ''), IFNULL(p.tags, ''), "
             "IFNULL(p.goles_detalle, ''), IFNULL(p.asistencias_detalle, '') "
             "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
             "JOIN cancha can ON p.cancha_id = can.id "
             "WHERE p.%s = ?",
             campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);

    int hay = mostrar_partidos_desde_stmt(stmt);

    if (!hay)
        printf("No se encontraron partidos con ese criterio.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

static void modificar_cancha_partido()
{
    listar_canchas_disponibles();
    int cancha = 0;
    while (1)
    {
        cancha = input_int("Nuevo ID Cancha (0 para cancelar): ");
        if (cancha == 0)
            return;
        if (existe_id("cancha", cancha) && cancha_esta_activa(cancha))
            break;
        printf("La cancha no existe o esta inactiva. Intente nuevamente.\n");
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET cancha_id=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, cancha);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Cancha Modificada", NULL);
}

static void modificar_fecha_hora_partido()
{
    char fecha[20];
    char hora[10];
    char fecha_hora[30];
    printf("Nueva fecha (dd/mm/yyyy): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;
    printf("Nueva hora (hh:mm): ");
    fgets(hora, sizeof(hora), stdin);
    hora[strcspn(hora, "\n")] = 0;
    snprintf(fecha_hora, sizeof(fecha_hora), "%s %s", fecha, hora);
    /* Convertir a formato de almacenamiento antes de actualizar */
    char fecha_storage[64] = {0};
    convert_display_date_to_storage(fecha_hora, fecha_storage, sizeof(fecha_storage));
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET fecha_hora=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, fecha_storage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Fecha y Hora Modificadas", NULL);
}

static void modificar_goles_partido()
{
    modificar_campo_partido("goles", "Nuevos goles: ", "Goles modificados correctamente", 0, 0, NULL);
}

static void modificar_asistencias_partido()
{
    modificar_campo_partido("asistencias", "Nuevas asistencias: ", "Asistencias modificadas correctamente", 0, 0, NULL);
}

static void modificar_resultado_partido()
{
    modificar_campo_partido("resultado", "Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ", "Resultado modificado correctamente", 1, 3, NULL);
}

static void modificar_camiseta_partido()
{
    listar_camisetas_disponibles();
    int camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", camiseta) || !camiseta_esta_activa(camiseta))
    {
        printf("La camiseta no existe o esta inactiva\n");
        return;
    }
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET camiseta_id=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, camiseta);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Camiseta Modificada", NULL);
}

static void modificar_clima_partido()
{
    modificar_campo_partido("clima", "Nuevo clima (1-12): ", "Clima modificado correctamente", 1, 12, mostrar_opciones_clima_partido);
}

static void modificar_dia_partido()
{
    mostrar_opciones_dia_partido();
    modificar_campo_partido("dia", "Nuevo dia (1-6): ", "Dia modificado correctamente", 1, 6, NULL);
}

static void modificar_comentario_partido()
{
    modificar_campo_texto_partido("comentario_personal", "Nuevo comentario personal: ", "Comentario modificado correctamente", 256);
}

static void modificar_precio_partido()
{
    modificar_campo_partido("precio", "Nuevo precio del partido: ", "Precio modificado correctamente", 0, 0, NULL);
}

static void modificar_rendimiento_general_partido()
{
    modificar_campo_partido("rendimiento_general", "Nuevo rendimiento general (1-10): ",
                            "Rendimiento general modificado correctamente", 1, 10, NULL);
}

static void modificar_cansancio_partido()
{
    modificar_campo_partido("cansancio", "Nuevo cansancio (0-10): ",
                            "Cansancio modificado correctamente", 0, 10, NULL);
}

static void modificar_estado_animo_partido()
{
    modificar_campo_partido("estado_animo", "Nuevo estado de animo (0-10): ",
                            "Estado de animo modificado correctamente", 0, 10, NULL);
}

static void modificar_minutos_jugados_partido()
{
    modificar_campo_partido("minutos_jugados", "Nuevos minutos jugados (0-180): ",
                            "Minutos jugados modificados correctamente", 0, 180, NULL);
}

static void modificar_intensidad_partido()
{
    modificar_campo_partido("intensidad", "Nueva intensidad (1-10): ",
                            "Intensidad modificada correctamente", 1, 10, NULL);
}

static void modificar_dolor_fisico_partido()
{
    mostrar_opciones_dolor_fisico_partido();
    modificar_campo_partido("dolor_fisico", "Nuevo dolor/molestia fisica (0-3): ",
                            "Dolor fisico modificado correctamente", 0, 3, NULL);
}

static void modificar_rating_tecnico_partido()
{
    modificar_campo_partido("rating_tecnico", "Nuevo rating tecnico (1-10): ",
                            "Rating tecnico modificado correctamente", 1, 10, NULL);
}

static void modificar_rating_fisico_partido()
{
    modificar_campo_partido("rating_fisico", "Nuevo rating fisico (1-10): ",
                            "Rating fisico modificado correctamente", 1, 10, NULL);
}

static void modificar_rating_mental_partido()
{
    modificar_campo_partido("rating_mental", "Nuevo rating mental (1-10): ",
                            "Rating mental modificado correctamente", 1, 10, NULL);
}

static void modificar_tipo_partido_partido()
{
    printf("Modo de carga:\n");
    printf("1) Carga rapida\n");
    printf("2) Carga completa\n");
    printf("3) Entrenamiento\n");
    modificar_campo_partido("tipo_partido", "Nuevo modo de carga (1-3): ",
                            "Modo de carga modificado correctamente", 1, 3, NULL);
}

static void modificar_rival_nombre_partido()
{
    modificar_campo_texto_partido("rival_nombre", "Nuevo rival: ", "Rival modificado correctamente", 100);
}

static void modificar_tipo_rival_partido()
{
    modificar_campo_texto_partido("tipo_rival", "Nuevo tipo de rival: ", "Tipo de rival modificado correctamente", 40);
}

static void modificar_posicion_jugada_partido()
{
    modificar_campo_texto_partido("posicion_jugada", "Nueva posicion jugada: ", "Posicion modificada correctamente", 40);
}

static void modificar_estado_cancha_partido()
{
    mostrar_opciones_estado_cancha_partido();
    int estado_cancha = pedir_entero_en_rango("Nuevo estado de cancha (1-5): ",
                        1, 5,
                        "Valor invalido. Ingrese entre 1 y 5: ");

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET estado_cancha=?, condicion_cancha=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, estado_cancha);
    sqlite3_bind_text(stmt, 2, estado_cancha_to_text(estado_cancha), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Estado de Cancha Modificado", NULL);
}

static void modificar_condicion_cancha_partido()
{
    modificar_campo_texto_partido("condicion_cancha", "Nueva condicion de cancha: ",
                                  "Condicion de cancha modificada correctamente", 60);
}

static void modificar_marcador_global_partido()
{
    int goles_equipo = input_int("Nuevos goles de tu equipo (-1 sin dato): ");
    while (goles_equipo < -1)
    {
        goles_equipo = input_int("Valor invalido. Ingrese -1 o un numero mayor: ");
    }

    int goles_rival = input_int("Nuevos goles del rival (-1 sin dato): ");
    while (goles_rival < -1)
    {
        goles_rival = input_int("Valor invalido. Ingrese -1 o un numero mayor: ");
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET goles_equipo=?, goles_rival=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, goles_equipo);
    sqlite3_bind_int(stmt, 2, goles_rival);
    sqlite3_bind_int(stmt, 3, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Marcador Global Modificado", NULL);
}

static void modificar_formato_partido_partido()
{
    char futbol[24];
    if (!solicitar_futbol_partido(futbol, (int)sizeof(futbol), 1))
    {
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET formato_partido=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, futbol, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Futbol Modificado", NULL);
}

static void modificar_tarjeta_partido()
{
    mostrar_opciones_tarjeta_partido();
    modificar_campo_partido("tarjeta", "Nueva tarjeta (1-3): ",
                            "Tarjeta modificada correctamente", 1, 3, NULL);
}

static void modificar_goles_en_contra_partido()
{
    int goles_en_contra = input_int("Nuevos goles en contra: ");
    while (goles_en_contra < 0)
    {
        goles_en_contra = input_int("Valor invalido. Ingrese 0 o mas: ");
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET goles_en_contra=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, goles_en_contra);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Goles en Contra Modificados", NULL);
}

static void modificar_temperatura_partido()
{
    char buffer[64];
    input_string_extended("Nueva temperatura C (Enter para dejar vacio): ", buffer, (int)sizeof(buffer));
    trim_whitespace(buffer);

    sqlite3_stmt *stmt;
    if (buffer[0] == '\0')
    {
        if (!preparar_stmt("UPDATE partido SET temperatura_c=NULL WHERE id=?", &stmt))
        {
            pause_console();
            return;
        }

        sqlite3_bind_int(stmt, 1, current_partido_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        mostrar_alerta_operacion("Partido", "Temperatura Limpiada", NULL);
        return;
    }

    char *endptr = NULL;
    double temperatura = strtod(buffer, &endptr);
    while (endptr && *endptr && isspace((unsigned char)*endptr))
    {
        endptr++;
    }

    if (!endptr || *endptr != '\0' || temperatura < -30.0 || temperatura > 60.0)
    {
        printf("Temperatura invalida. Ingrese un valor numerico entre -30 y 60, o Enter para vaciar.\n");
        pause_console();
        return;
    }

    if (!preparar_stmt("UPDATE partido SET temperatura_c=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_double(stmt, 1, temperatura);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Temperatura Modificada", NULL);
}

static void modificar_arbitraje_score_partido()
{
    mostrar_opciones_arbitraje_partido();
    int arbitraje_score = pedir_entero_en_rango("Nuevo arbitraje (1-5): ",
                          1, 5,
                          "Valor invalido. Ingrese entre 1 y 5: ");

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET arbitraje_score=?, arbitraje=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, arbitraje_score);
    sqlite3_bind_text(stmt, 2, arbitraje_score_to_text(arbitraje_score), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Arbitraje Modificado", NULL);
}

static void modificar_arbitraje_texto_partido()
{
    modificar_campo_texto_partido("arbitraje", "Nuevo texto de arbitraje: ",
                                  "Texto de arbitraje modificado correctamente", 60);
}

static void modificar_eventos_clave_partido()
{
    modificar_campo_texto_partido("eventos_clave", "Nuevos eventos clave: ",
                                  "Eventos clave modificados correctamente", 300);
}

static void modificar_detalle_evento_partido(const char *campo,
        const char *etiqueta,
        int es_asistencia,
        const char *mensaje_exito)
{
    sqlite3_stmt *stmt;
    int cantidad = 0;
    char detalle_actual[512] = {0};

    if (!preparar_stmt("SELECT goles, asistencias, IFNULL(goles_detalle, ''), IFNULL(asistencias_detalle, '') "
                       "FROM partido WHERE id = ?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, current_partido_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cantidad = sqlite3_column_int(stmt, es_asistencia ? 1 : 0);
        const char *det = (const char *)sqlite3_column_text(stmt, es_asistencia ? 3 : 2);
        if (det)
        {
            strncpy_s(detalle_actual, sizeof(detalle_actual), det, sizeof(detalle_actual) - 1);
        }
    }
    sqlite3_finalize(stmt);

    printf("Detalle actual de %s: %s\n", etiqueta,
           detalle_actual[0] ? detalle_actual : "(sin detalle)");

    if (cantidad <= 0)
    {
        printf("No se puede modificar el detalle: el partido no tiene %s registrados.\n",
               es_asistencia ? "asistencias" : "goles");
        printf("Modifique primero el numero de %s.\n",
               es_asistencia ? "asistencias" : "goles");
        pause_console();
        return;
    }

    char nuevo_detalle[512] = {0};
    pedir_detalle_evento(cantidad, es_asistencia, nuevo_detalle, sizeof(nuevo_detalle));

    if (nuevo_detalle[0] == '\0' && detalle_actual[0] == '\0')
    {
        printf("No se realizo ningun cambio.\n");
        pause_console();
        return;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, nuevo_detalle, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("%s\n", mensaje_exito);
    pause_console();
}

static void modificar_goles_detalle_partido()
{
    modificar_detalle_evento_partido("goles_detalle", "goles", 0,
                                     "Detalle de goles modificado correctamente");
}

static void modificar_asistencias_detalle_partido()
{
    modificar_detalle_evento_partido("asistencias_detalle", "asistencias", 1,
                                     "Detalle de asistencias modificado correctamente");
}

static void modificar_lo_mejor_partido()
{
    modificar_campo_texto_partido("lo_mejor", "Nuevo 'Lo mejor': ",
                                  "Campo 'Lo mejor' modificado correctamente", 300);
}

static void modificar_que_mejorar_partido()
{
    modificar_campo_texto_partido("que_mejorar", "Nuevo 'Que mejorar': ",
                                  "Campo 'Que mejorar' modificado correctamente", 300);
}

static void modificar_tags_partido()
{
    modificar_campo_texto_partido("tags", "Nuevos tags (separados por coma): ",
                                  "Tags modificados correctamente", 300);
}

static void menu_modificar_rendimiento_y_estado_partido()
{
    MenuItem items[] =
    {
        {1, "Rendimiento General", modificar_rendimiento_general_partido},
        {2, "Cansancio", modificar_cansancio_partido},
        {3, "Estado de Animo", modificar_estado_animo_partido},
        {4, "Minutos Jugados", modificar_minutos_jugados_partido},
        {5, "Intensidad", modificar_intensidad_partido},
        {6, "Dolor Fisico", modificar_dolor_fisico_partido},
        {7, "Rating Tecnico", modificar_rating_tecnico_partido},
        {8, "Rating Fisico", modificar_rating_fisico_partido},
        {9, "Rating Mental", modificar_rating_mental_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR RENDIMIENTO/ESTADO", items, 10);
}

static void menu_modificar_detalle_ampliado_partido()
{
    MenuItem items[] =
    {
        {1, "Tipo de Carga", modificar_tipo_partido_partido},
        {2, "Rival", modificar_rival_nombre_partido},
        {3, "Tipo de Rival", modificar_tipo_rival_partido},
        {4, "Posicion Jugada", modificar_posicion_jugada_partido},
        {5, "Estado de Cancha", modificar_estado_cancha_partido},
        {6, "Condicion de Cancha (Texto)", modificar_condicion_cancha_partido},
        {7, "Marcador Global", modificar_marcador_global_partido},
        {8, "Futbol", modificar_formato_partido_partido},
        {9, "Tarjeta", modificar_tarjeta_partido},
        {10, "Goles en Contra", modificar_goles_en_contra_partido},
        {11, "Temperatura", modificar_temperatura_partido},
        {12, "Arbitraje (Escala 1-5)", modificar_arbitraje_score_partido},
        {13, "Arbitraje (Texto)", modificar_arbitraje_texto_partido},
        {14, "Eventos Clave", modificar_eventos_clave_partido},
        {15, "Lo Mejor", modificar_lo_mejor_partido},
        {16, "Que Mejorar", modificar_que_mejorar_partido},
        {17, "Tags", modificar_tags_partido},
        {18, "Detalle Goles", modificar_goles_detalle_partido},
        {19, "Detalle Asistencias", modificar_asistencias_detalle_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR DETALLE AMPLIADO", items, 20);
}

static void cargar_detalle_partido_actual(char *goles_detalle, size_t goles_size,
        char *asist_detalle, size_t asist_size)
{
    sqlite3_stmt *stmt;
    if (goles_detalle && goles_size > 0)
    {
        goles_detalle[0] = '\0';
    }
    if (asist_detalle && asist_size > 0)
    {
        asist_detalle[0] = '\0';
    }
    if (!preparar_stmt("SELECT IFNULL(goles_detalle, ''), IFNULL(asistencias_detalle, '') "
                       "FROM partido WHERE id = ?", &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, current_partido_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *g = (const char *)sqlite3_column_text(stmt, 0);
        const char *a = (const char *)sqlite3_column_text(stmt, 1);
        if (goles_detalle && goles_size > 0)
        {
            strncpy_s(goles_detalle, goles_size, g ? g : "", goles_size - 1);
        }
        if (asist_detalle && asist_size > 0)
        {
            strncpy_s(asist_detalle, asist_size, a ? a : "", asist_size - 1);
        }
    }
    sqlite3_finalize(stmt);
}

static int recopilar_datos_completos_partido(DatosPartido *datos)
{
    if (!datos)
        return 0;

    memset(datos, 0, sizeof(*datos));

    listar_canchas_disponibles();
    datos->cancha_id = input_int("Nuevo ID Cancha: ");
    if (!existe_id("cancha", datos->cancha_id) || !cancha_esta_activa(datos->cancha_id))
    {
        printf("La cancha no existe o esta inactiva\n");
        return 0;
    }
    char fecha[20];
    char hora[10];
    input_date("Nueva fecha (DD/MM/AAAA, Enter=hoy): ", fecha, 20);
    input_date("Nueva hora (HH:MM, Enter=ahora): ", hora, 10);
    snprintf(datos->comentario_personal, sizeof(datos->comentario_personal), "%s %s", fecha, hora);
    datos->goles = input_int("Nuevos goles: ");
    datos->asistencias = input_int("Nuevas asistencias: ");

    cargar_detalle_partido_actual(datos->goles_detalle, sizeof(datos->goles_detalle),
                                  datos->asistencias_detalle, sizeof(datos->asistencias_detalle));

    if (datos->goles > 0)
    {
        printf("Detalle actual de goles: %s\n",
               datos->goles_detalle[0] ? datos->goles_detalle : "(sin detalle)");
        if (confirmar("Desea modificar el detalle de los goles?"))
        {
            pedir_detalle_evento(datos->goles, 0,
                                 datos->goles_detalle, sizeof(datos->goles_detalle));
        }
    }
    if (datos->asistencias > 0)
    {
        printf("Detalle actual de asistencias: %s\n",
               datos->asistencias_detalle[0] ? datos->asistencias_detalle : "(sin detalle)");
        if (confirmar("Desea modificar el detalle de las asistencias?"))
        {
            pedir_detalle_evento(datos->asistencias, 1,
                                 datos->asistencias_detalle, sizeof(datos->asistencias_detalle));
        }
    }

    datos->resultado = input_int("Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (datos->resultado < 1 || datos->resultado > 3)
    {
        datos->resultado = input_int("Resultado invalido. Ingrese 1, 2 o 3: ");
    }
    listar_camisetas_disponibles();
    datos->camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", datos->camiseta) || !camiseta_esta_activa(datos->camiseta))
    {
        printf("La camiseta no existe o esta inactiva\n");
        return 0;
    }
    mostrar_opciones_clima_partido();
    datos->clima = input_int("Nuevo clima (1-12): ");
    while (datos->clima < 1 || datos->clima > 12)
    {
        datos->clima = input_int("Clima invalido. Ingrese entre 1 y 12: ");
    }
    mostrar_opciones_dia_partido();
    datos->dia = input_int("Nuevo dia (1-6): ");
    while (datos->dia < 1 || datos->dia > 6)
    {
        datos->dia = input_int("Dia invalido. Ingrese entre 1 y 6: ");
    }
    datos->precio = input_int("Nuevo precio del partido: ");

    return 1;
}

static void actualizar_partido_completo(DatosPartido const *datos, char const *fecha_hora)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "UPDATE partido "
                "SET cancha_id=?, fecha_hora=?, goles=?, asistencias=?, camiseta_id=?, resultado=?, clima=?, dia=?, precio=?, "
                "goles_detalle=?, asistencias_detalle=? "
                "WHERE id=?",
                &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, datos->cancha_id);
    /* Asegurar formato de almacenamiento (YYYY-MM-DD HH:MM) */
    char fecha_storage[64] = {0};
    convert_display_date_to_storage(fecha_hora, fecha_storage, sizeof(fecha_storage));
    sqlite3_bind_text(stmt, 2, fecha_storage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, datos->goles);
    sqlite3_bind_int(stmt, 4, datos->asistencias);
    sqlite3_bind_int(stmt, 5, datos->camiseta);
    sqlite3_bind_int(stmt, 6, datos->resultado);
    sqlite3_bind_int(stmt, 7, datos->clima);
    sqlite3_bind_int(stmt, 8, datos->dia);
    sqlite3_bind_int(stmt, 9, datos->precio);
    sqlite3_bind_text(stmt, 10, datos->goles_detalle, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, datos->asistencias_detalle, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Modificado", NULL);
}

static void modificar_todo_partido()
{
    DatosPartido datos;
    if (!recopilar_datos_completos_partido(&datos))
    {
        pause_console();
        return;
    }
    actualizar_partido_completo(&datos, datos.comentario_personal);
}

void modificar_partido()
{
    print_header("MODIFICAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Modificar (0 para cancelar): ");

    if (id == 0)
        return;

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        pause_console();
        return;
    }

    current_partido_id = id;

    MenuItem items[] =
    {
        {1, "Cancha", modificar_cancha_partido},
        {2, "Fecha y Hora", modificar_fecha_hora_partido},
        {3, "Goles", modificar_goles_partido},
        {4, "Asistencias", modificar_asistencias_partido},
        {5, "Resultado", modificar_resultado_partido},
        {6, "Camiseta", modificar_camiseta_partido},
        {7, "Clima", modificar_clima_partido},
        {8, "Dia", modificar_dia_partido},
        {9, "Comentario", modificar_comentario_partido},
        {10, "Precio", modificar_precio_partido},
        {11, "Rendimiento y Estado", menu_modificar_rendimiento_y_estado_partido},
        {12, "Detalle Ampliado", menu_modificar_detalle_ampliado_partido},
        {13, "Modificar Todo", modificar_todo_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR PARTIDO", items, 14);
}

static void buscar_por_camiseta()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CAMISETA", "camiseta_id", "ID de la camiseta: ", &listar_camisetas, 1);
}

static void buscar_por_goles()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR GOLES", "goles", "Numero de goles: ", NULL, 0);
}

static void buscar_por_asistencias()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR ASISTENCIAS", "asistencias", "Numero de asistencias: ", NULL, 0);
}

static void buscar_por_cancha()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CANCHA", "cancha_id", "ID de la cancha: ", &listar_canchas_disponibles, 1);
}

static void buscar_por_tag()
{
    print_header("BUSCAR PARTIDOS POR TAG");

    char tag[128];
    input_string_extended("Tag a buscar: ", tag, (int)sizeof(tag));
    trim_whitespace(tag);
    if (tag[0] == '\0')
    {
        printf("Debe ingresar un tag.\n");
        pause_console();
        return;
    }

    char patron[160];
    snprintf(patron, sizeof(patron), "%%%s%%", tag);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio, "
                "IFNULL(p.tipo_partido, 1), IFNULL(p.rival_nombre, ''), IFNULL(p.tipo_rival, ''), IFNULL(p.posicion_jugada, ''), "
                "IFNULL(p.minutos_jugados, 0), IFNULL(p.intensidad, 0), IFNULL(p.esfuerzo_percibido, 0), IFNULL(p.condicion_cancha, ''), "
                "IFNULL(p.arbitraje, ''), IFNULL(p.eventos_clave, ''), IFNULL(p.rating_tecnico, 0), IFNULL(p.rating_fisico, 0), IFNULL(p.rating_mental, 0), "
                "IFNULL(p.estado_cancha, 0), IFNULL(p.goles_equipo, -1), IFNULL(p.goles_rival, -1), IFNULL(p.formato_partido, ''), IFNULL(p.tarjeta, 1), IFNULL(p.goles_en_contra, 0), "
                "IFNULL(p.dolor_fisico, 0), p.temperatura_c, IFNULL(p.arbitraje_score, 0), IFNULL(p.lo_mejor, ''), IFNULL(p.que_mejorar, ''), IFNULL(p.tags, ''), "
                "IFNULL(p.goles_detalle, ''), IFNULL(p.asistencias_detalle, '') "
                "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                "JOIN cancha can ON p.cancha_id = can.id "
                "WHERE LOWER(IFNULL(p.tags, '')) LIKE LOWER(?) "
                "ORDER BY p.id ASC",
                &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, patron, -1, SQLITE_TRANSIENT);
    int hay = mostrar_partidos_desde_stmt(stmt);
    if (!hay)
    {
        printf("No se encontraron partidos para ese tag.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void buscar_partidos()
{
    MenuItem items[] =
    {
        {1, "Por Camiseta", buscar_por_camiseta},
        {2, "Por Goles", buscar_por_goles},
        {3, "Por Asistencias", buscar_por_asistencias},
        {4, "Por Cancha", buscar_por_cancha},
        {5, "Por Tag", buscar_por_tag},
        {0, "Volver", NULL}
    };

    ejecutar_menu("BUSQUEDA DE PARTIDOS", items, 6);
}

static void manejar_gol_local(Equipo const *equipo_local, int minuto, int jugador_idx, int asistente_idx,
                              int *estadisticas_local, int *asistencias_local, int *goles_local)
{
    if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
    }

    (*goles_local)++;
    estadisticas_local[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_local[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_local->jugadores[jugador_idx].nombre,
           equipo_local->jugadores[jugador_idx].numero,
           equipo_local->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_local->jugadores[asistente_idx].nombre,
               equipo_local->jugadores[asistente_idx].numero);
    }
}

static void manejar_gol_visitante(Equipo const *equipo_visitante, int minuto, int jugador_idx, int asistente_idx,
                                  int *estadisticas_visitante, int *asistencias_visitante, int *goles_visitante)
{
    if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
    }

    (*goles_visitante)++;
    estadisticas_visitante[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_visitante[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_visitante->jugadores[jugador_idx].nombre,
           equipo_visitante->jugadores[jugador_idx].numero,
           equipo_visitante->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_visitante->jugadores[asistente_idx].nombre,
               equipo_visitante->jugadores[asistente_idx].numero);
    }
}

static int verificar_equipos_disponibles()
{
    sqlite3_stmt *stmt_count;
    if (!preparar_stmt("SELECT COUNT(*) FROM equipo", &stmt_count))
    {
        return 0;
    }
    sqlite3_step(stmt_count);
    int total_equipos = sqlite3_column_int(stmt_count, 0);
    sqlite3_finalize(stmt_count);

    if (total_equipos < 2)
    {
        printf("Se necesitan al menos 2 equipos guardados para simular un partido.\n");
        printf("Por favor, cree equipos primero.\n");
        pause_console();
        return 0;
    }
    return 1;
}

static void mostrar_equipos_disponibles()
{
    printf("=== EQUIPOS DISPONIBLES ===\n\n");
    sqlite3_stmt *stmt_equipos;
    if (!preparar_stmt("SELECT id, nombre FROM equipo ORDER BY id", &stmt_equipos))
    {
        return;
    }

    while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
    {
        printf("%d. %s\n", sqlite3_column_int(stmt_equipos, 0),
               sqlite3_column_text(stmt_equipos, 1));
    }
    sqlite3_finalize(stmt_equipos);
}

static void seleccionar_equipos(int *equipo_local_id, int *equipo_visitante_id)
{
    // Seleccionar equipo local
    do
    {
        *equipo_local_id = input_int("\nSeleccione el equipo LOCAL (ID): ");
        if (!existe_id("equipo", *equipo_local_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (!existe_id("equipo", *equipo_local_id));

    // Seleccionar equipo visitante (diferente al local)
    do
    {
        *equipo_visitante_id = input_int("Seleccione el equipo VISITANTE (ID): ");
        if (*equipo_visitante_id == *equipo_local_id)
        {
            printf("El equipo visitante debe ser diferente al local.\n");
        }
        else if (!existe_id("equipo", *equipo_visitante_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (*equipo_visitante_id == *equipo_local_id || !existe_id("equipo", *equipo_visitante_id));
}

static int cargar_equipos(int equipo_local_id, int equipo_visitante_id, Equipo *equipo_local, Equipo *equipo_visitante)
{
    memset(equipo_local, 0, sizeof(Equipo));
    memset(equipo_visitante, 0, sizeof(Equipo));

    if (!cargar_equipo_desde_bd(equipo_local_id, equipo_local))
    {
        printf("Error al cargar el equipo local.\n");
        pause_console();
        return 0;
    }

    if (!cargar_equipo_desde_bd(equipo_visitante_id, equipo_visitante))
    {
        printf("Error al cargar el equipo visitante.\n");
        pause_console();
        return 0;
    }
    return 1;
}

static void mostrar_inicio_partido(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    printf("\n*** INICIANDO SIMULACION ***\n");
    printf("EQUIPO LOCAL: %s\n", equipo_local->nombre);
    printf("EQUIPO VISITANTE: %s\n\n", equipo_visitante->nombre);
}

static void mostrar_alineacion(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    clear_screen();
    printf("%s\n", ASCII_SIMULACION);
    printf("                    SIMULACION DE PARTIDO\n\n");

    printf("=== %s VS %s ===\n\n", equipo_local->nombre, equipo_visitante->nombre);

    // Mostrar cancha inicial
    mostrar_cancha_animada(0, 0);

    // Mostrar equipos alineados
    imprimir_alineacion_equipo("EQUIPO LOCAL", equipo_local);
    printf("\n");
    imprimir_alineacion_equipo("EQUIPO VISITANTE", equipo_visitante);

    printf("\n*** INICIO DEL PARTIDO ***\n");
    printf("La simulacion comenzara automaticamente en 3 segundos...\n");
    Sleep(3000);
}

static void simular_partido_logica(Equipo const *equipo_local, Equipo const *equipo_visitante,
                                   EstadisticasPartido *estadisticas)
{
    for (int minuto = 1; minuto <= 60; minuto++)
    {
        clear_screen();
        print_header("SIMULACION DE PARTIDO");

        printf("=== %s %d - %d %s ===\n\n",
               equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);
        printf("MINUTO: %d\n\n", minuto);

        // Generar eventos aleatorios
        int evento = secure_rand(100);

        if (evento < 25) // Gol local
        {
            int jugador_idx = secure_rand(equipo_local->num_jugadores);
            int asistente_idx = secure_rand(equipo_local->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
            }

            manejar_gol_local(equipo_local, minuto, jugador_idx, asistente_idx,
                              estadisticas->estadisticas_local, estadisticas->asistencias_local, &estadisticas->goles_local);
        }
        else if (evento < 50) // Gol visitante
        {
            int jugador_idx = secure_rand(equipo_visitante->num_jugadores);
            int asistente_idx = secure_rand(equipo_visitante->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
            }

            manejar_gol_visitante(equipo_visitante, minuto, jugador_idx, asistente_idx,
                                  estadisticas->estadisticas_visitante, estadisticas->asistencias_visitante, &estadisticas->goles_visitante);
        }
        else if (evento < 70)
        {
            printf("*** Oportunidad de gol ***\n");
        }
        else
        {
            printf("*** El partido continua... ***\n");
        }

        mostrar_cancha_animada(minuto, (evento < 4) ? 1 : 0);
        Sleep(1000);
    }
}

static void mostrar_resultados(Equipo const *equipo_local, Equipo const *equipo_visitante,
                               EstadisticasPartido const *estadisticas)
{
    // Resultados finales
    clear_screen();
    print_header("FIN DEL PARTIDO");

    printf("*** RESULTADO FINAL ***\n\n");
    printf("*** 60 MINUTOS COMPLETADOS ***\n\n");

    printf("*** %s %d - %d %s ***\n\n",
           equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);

    if (estadisticas->goles_local > estadisticas->goles_visitante)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_local->nombre);
    }
    else if (estadisticas->goles_visitante > estadisticas->goles_local)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_visitante->nombre);
    }
    else
    {
        printf("*** ¡EMPATE! ***\n\n");
    }

    // Mostrar estadisticas
    printf("*** ESTADISTICAS DEL PARTIDO ***\n\n");

    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_local[i] > 0 || estadisticas->asistencias_local[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_local->jugadores[i].nombre,
                   equipo_local->jugadores[i].numero,
                   estadisticas->estadisticas_local[i], estadisticas->asistencias_local[i]);
        }
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_visitante[i] > 0 || estadisticas->asistencias_visitante[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_visitante->jugadores[i].nombre,
                   equipo_visitante->jugadores[i].numero,
                   estadisticas->estadisticas_visitante[i], estadisticas->asistencias_visitante[i]);
        }
    }
}

static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_equipo;
    const char *sql_equipo = "SELECT nombre, tipo, tipo_futbol, num_jugadores FROM equipo WHERE id = ?";

    if (!preparar_stmt(sql_equipo, &stmt_equipo))
    {
        return 0;
    }

    sqlite3_bind_int(stmt_equipo, 1, equipo_id);

    if (sqlite3_step(stmt_equipo) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt_equipo);
        return 0;
    }

    equipo->id = equipo_id;
    strncpy_s(equipo->nombre, sizeof(equipo->nombre), (const char *)sqlite3_column_text(stmt_equipo, 0), _TRUNCATE);
    equipo->tipo = sqlite3_column_int(stmt_equipo, 1);
    equipo->tipo_futbol = sqlite3_column_int(stmt_equipo, 2);
    equipo->num_jugadores = sqlite3_column_int(stmt_equipo, 3);
    equipo->partido_id = -1;

    sqlite3_finalize(stmt_equipo);

    // Cargar jugadores
    return cargar_jugadores_equipo(equipo_id, equipo);
}

static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero";

    if (!preparar_stmt(sql_jugadores, &stmt_jugadores))
    {
        return 0;
    }

    sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

    int jugador_idx = 0;
    while (sqlite3_step(stmt_jugadores) == SQLITE_ROW && jugador_idx < 11)
    {
        strncpy_s(equipo->jugadores[jugador_idx].nombre,
                  sizeof(equipo->jugadores[jugador_idx].nombre),
                  (const char *)sqlite3_column_text(stmt_jugadores, 0),
                  _TRUNCATE);
        equipo->jugadores[jugador_idx].numero = sqlite3_column_int(stmt_jugadores, 1);
        equipo->jugadores[jugador_idx].posicion = sqlite3_column_int(stmt_jugadores, 2);
        equipo->jugadores[jugador_idx].es_capitan = sqlite3_column_int(stmt_jugadores, 3);
        jugador_idx++;
    }

    sqlite3_finalize(stmt_jugadores);
    return 1;
}

static void determinar_resultado_partido(int goles_local, int goles_visitante,
        int *resultado_local, int *resultado_visitante)
{
    if (goles_local > goles_visitante)
    {
        *resultado_local = 1;     // VICTORIA
        *resultado_visitante = 3; // DERROTA
    }
    else if (goles_visitante > goles_local)
    {
        *resultado_local = 3;     // DERROTA
        *resultado_visitante = 1; // VICTORIA
    }
    else
    {
        *resultado_local = 2;     // EMPATE
        *resultado_visitante = 2; // EMPATE
    }
}

static UNUSED int obtener_cancha_defecto()
{
    int cancha_id = 1;
    sqlite3_stmt *stmt_cancha;
    if (!preparar_stmt("SELECT id FROM cancha ORDER BY id LIMIT 1", &stmt_cancha))
    {
        return cancha_id;
    }
    if (sqlite3_step(stmt_cancha) == SQLITE_ROW)
    {
        cancha_id = sqlite3_column_int(stmt_cancha, 0);
    }
    sqlite3_finalize(stmt_cancha);
    return cancha_id;
}

static UNUSED void guardar_resultados_simulacion(DatosSimulacion const *datos_simulacion)
{
    char fecha_simulacion[20] = "2023-01-01 00:00";
    get_datetime(fecha_simulacion, sizeof(fecha_simulacion));

    // Determinar resultados
    int resultado_local;
    int resultado_visitante;
    determinar_resultado_partido(datos_simulacion->goles_local, datos_simulacion->goles_visitante,
                                 &resultado_local, &resultado_visitante);

    // Obtener cancha por defecto
    int cancha_id = obtener_cancha_defecto();

    // Guardar estadisticas para cada jugador del equipo local
    guardar_estadisticas_equipo(&datos_simulacion->equipo_local, datos_simulacion->estadisticas_local,
                                datos_simulacion->asistencias_local, resultado_local, cancha_id, fecha_simulacion);

    // Guardar estadisticas para cada jugador del equipo visitante
    guardar_estadisticas_equipo(&datos_simulacion->equipo_visitante, datos_simulacion->estadisticas_visitante,
                                datos_simulacion->asistencias_visitante, resultado_visitante, cancha_id, fecha_simulacion);

    printf("*** RESULTADOS GUARDADOS EN LA BASE DE DATOS ***\n");
}

static UNUSED void guardar_estadisticas_equipo(Equipo const *equipo, int const *estadisticas, int const *asistencias,
        int resultado, int cancha_id, char const *fecha_simulacion)
{
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        if (estadisticas[i] > 0 || asistencias[i] > 0)
        {
            // Buscar o crear camiseta para este jugador
            int camiseta_id = 1; // Usar camiseta por defecto

            DatosPartido datos = {0};
            datos.cancha_id = cancha_id;
            datos.goles = estadisticas[i];
            datos.asistencias = asistencias[i];
            datos.camiseta = camiseta_id;
            datos.resultado = resultado;
            datos.rendimiento_general = 8;
            datos.cansancio = 5;
            datos.estado_animo = 7;
            strncpy_s(datos.comentario_personal, sizeof(datos.comentario_personal), "Partido simulado", _TRUNCATE);
            datos.clima = 1;
            datos.dia = 1;

            long long partido_id = obtener_siguiente_id("partido");
            insertar_partido(partido_id, &datos, fecha_simulacion);
        }
    }
}

static UNUSED DatosSimulacion crear_datos_simulacion(Equipo equipo_local, Equipo equipo_visitante,
        EstadisticasPartido const *estadisticas)
{
    DatosSimulacion datos_simulacion = {0};
    datos_simulacion.equipo_local = equipo_local;
    datos_simulacion.equipo_visitante = equipo_visitante;
    memcpy(datos_simulacion.estadisticas_local, estadisticas->estadisticas_local, sizeof(estadisticas->estadisticas_local));
    memcpy(datos_simulacion.estadisticas_visitante, estadisticas->estadisticas_visitante, sizeof(estadisticas->estadisticas_visitante));
    memcpy(datos_simulacion.asistencias_local, estadisticas->asistencias_local, sizeof(estadisticas->asistencias_local));
    memcpy(datos_simulacion.asistencias_visitante, estadisticas->asistencias_visitante, sizeof(estadisticas->asistencias_visitante));
    datos_simulacion.goles_local = estadisticas->goles_local;
    datos_simulacion.goles_visitante = estadisticas->goles_visitante;
    return datos_simulacion;
}

void simular_partido_guardados()
{
    clear_screen();
    print_header("SIMULAR PARTIDO CON EQUIPOS GUARDADOS");

    // Verificar que hay al menos 2 equipos disponibles
    if (!verificar_equipos_disponibles())
    {
        return;
    }

    // Mostrar equipos disponibles
    mostrar_equipos_disponibles();

    // Seleccionar equipos
    int equipo_local_id;
    int equipo_visitante_id;
    seleccionar_equipos(&equipo_local_id, &equipo_visitante_id);

    // Cargar equipos desde la base de datos
    Equipo equipo_local;
    Equipo equipo_visitante;
    if (!cargar_equipos(equipo_local_id, equipo_visitante_id, &equipo_local, &equipo_visitante))
    {
        return;
    }

    // Mostrar informacion inicial
    mostrar_inicio_partido(&equipo_local, &equipo_visitante);

    // Mostrar alineacion y comenzar partido
    mostrar_alineacion(&equipo_local, &equipo_visitante);

    // Preparar arrays para estadisticas
    EstadisticasPartido estadisticas = {0};

    // Ejecutar simulacion del partido
    simular_partido_logica(&equipo_local, &equipo_visitante, &estadisticas);

    // Mostrar resultados finales
    mostrar_resultados(&equipo_local, &equipo_visitante, &estadisticas);

    printf("Simulacion finalizada. Este partido no se guarda en Partidos.\n");

    printf("\nPresione Enter para volver al menu...");
    getchar();
}

#define TACTIC_W 40
#define TACTIC_H 20

static UNUSED void tactica_init_grid(char grid[TACTIC_H][TACTIC_W + 1])
{
    for (int y = 0; y < TACTIC_H; y++)
    {
        for (int x = 0; x < TACTIC_W; x++)
        {
            grid[y][x] = '.';
        }
        grid[y][TACTIC_W] = '\0';
    }
}

static UNUSED void tactica_build_grid_string(char grid[TACTIC_H][TACTIC_W + 1], char *out, size_t size)
{
    size_t used = 0;
    if (!out || size == 0)
    {
        return;
    }

    for (int y = 0; y < TACTIC_H; y++)
    {
        for (int x = 0; x < TACTIC_W; x++)
        {
            if (used + 1 >= size)
            {
                out[used] = '\0';
                return;
            }
            out[used++] = grid[y][x];
        }
        if (used + 1 >= size)
        {
            out[used] = '\0';
            return;
        }
        out[used++] = '\n';
    }
    out[used] = '\0';
}

static UNUSED void tactica_guardar_diagrama(int partido_id, const char *nombre, const char *grid_text)
{
    const char *sql =
        "INSERT INTO tactica_diagrama (partido_id, nombre, fecha, grid) "
        "VALUES (?, ?, date('now'), ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error preparando insercion.\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, grid_text, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error guardando diagrama: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

#ifndef RSIZE_MAX_STR
#define RSIZE_MAX_STR (SIZE_MAX >> 1)
#endif

#ifndef TACTICA_TRUNCATE
#define TACTICA_TRUNCATE ((size_t)-1)
#endif

static int tactica_strncpy_s(char *dest, size_t destsz, const char *src, size_t count)
{
    if (!dest || !src || destsz == 0 || destsz > RSIZE_MAX_STR)
        return 1;

#if defined(__STDC_LIB_EXT1__)
    return strncpy_s(dest, destsz, src, count == TACTICA_TRUNCATE ? _TRUNCATE : count);
#elif defined(_MSC_VER)
    return strncpy_s(dest, destsz, src, count == TACTICA_TRUNCATE ? _TRUNCATE : count);
#else
    size_t max_src = (count == TACTICA_TRUNCATE) ? (destsz - 1) : count;
    size_t src_len = strnlen(src, max_src);
    size_t to_copy = src_len;
    if (to_copy > destsz - 1)
        to_copy = destsz - 1;

    if (to_copy > 0)
        memcpy(dest, src, to_copy);

    dest[to_copy] = '\0';
    return 0;
#endif
}

static size_t tactica_strlen_secure(const char *s, size_t max)
{
    if (!s)
        return 0;

#if defined(__STDC_LIB_EXT1__)
    return strlen_s(s, max);
#elif defined(_MSC_VER)
    return strnlen_s(s, max);
#else
    return strnlen(s, max);
#endif
}

static void tactica_trim_newline(char *s)
{
    if (!s)
        return;

    size_t len = tactica_strlen_secure(s, RSIZE_MAX_STR);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[--len] = '\0';
    }
}

static void tactica_leer_nombre_diagrama(char *nombre, size_t size)
{
    ui_printf("Nombre del diagrama: ");
    if (!fgets(nombre, (int)size, stdin))
    {
        if (size > 0)
            nombre[0] = '\0';
        return;
    }

    tactica_trim_newline(nombre);

    if (nombre[0] == '\0' && tactica_strncpy_s(nombre, size, "Diagrama", TACTICA_TRUNCATE) != 0 && size > 0)
    {
        nombre[0] = '\0';
    }
}

static void tactica_mostrar_partidos_disponibles(void)
{
    const char *sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
                      "JOIN cancha can ON p.cancha_id = can.id ORDER BY p.id ASC";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
        return;

    ui_printf_centered_line("--- Partidos disponibles ---");
    ui_printf_centered_line("ID | Cancha | Fecha");
    ui_printf_centered_line("---------------------------");

    int count = 0;
    char fecha_fmt[20];
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2),
                                fecha_fmt, sizeof(fecha_fmt));
        ui_printf_centered_line("%d | %s | %s",
                                sqlite3_column_int(stmt, 0),
                                (const char *)sqlite3_column_text(stmt, 1),
                                fecha_fmt);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        ui_printf_centered_line("No hay partidos registrados.");
    ui_printf("\n");
}

static void tactica_mostrar_grid_con_ejes(char grid[TACTIC_H][TACTIC_W + 1])
{
    ui_printf("     ");
    for (int x = 0; x < TACTIC_W; x += 5)
        ui_printf("%-5d", x);
    ui_printf("\n");

    for (int y = 0; y < TACTIC_H; y++)
        ui_printf("%2d   %s\n", y, grid[y]);
}

static int tactica_colocar(const char *args, char grid[TACTIC_H][TACTIC_W + 1],
                           char *grid_text, size_t grid_text_size)
{
    int x = -1;
    int y = -1;
    char c = '\0';
#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(args, "%d %d %c", &x, &y, &c, 1) == 3)
#else
    if (sscanf(args, "%d %d %c", &x, &y, &c) == 3)
#endif
    {
        if (x >= 0 && x < TACTIC_W && y >= 0 && y < TACTIC_H)
        {
            grid[y][x] = c;
            tactica_build_grid_string(grid, grid_text, grid_text_size);
        }
        else
        {
            ui_printf("Fuera de rango (Col: 0..%d, Fila: 0..%d).\n", TACTIC_W - 1, TACTIC_H - 1);
            pause_console();
        }
    }
    else
    {
        ui_printf("Formato: Col Fila Caracter (ej: 10 5 X)\n");
        pause_console();
    }
    return 0;
}

static int tactica_borrar(const char *args, char grid[TACTIC_H][TACTIC_W + 1],
                          char *grid_text, size_t grid_text_size)
{
    int x = -1;
    int y = -1;
#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(args, "%d %d", &x, &y) == 2)
#else
    if (sscanf(args, "%d %d", &x, &y) == 2)
#endif
    {
        if (x >= 0 && x < TACTIC_W && y >= 0 && y < TACTIC_H)
        {
            grid[y][x] = '.';
            tactica_build_grid_string(grid, grid_text, grid_text_size);
        }
        else
        {
            ui_printf("Fuera de rango.\n");
            pause_console();
        }
    }
    else
    {
        ui_printf("Formato: d Col Fila (ej: d 10 5)\n");
        pause_console();
    }

    return 0;
}

static int tactica_procesar_comando(const char *line, char grid[TACTIC_H][TACTIC_W + 1], char *grid_text,
                                    size_t grid_text_size, int partido_id, const char *nombre)
{
    if (!line || !grid || !grid_text)
        return 0;

    char cmd = line[0];
    switch (cmd)
    {
    case 'q':
    case 'Q':
        return 1;

    case 'r':
    case 'R':
        tactica_init_grid(grid);
        tactica_build_grid_string(grid, grid_text, grid_text_size);
        return 0;

    case 's':
    case 'S':
        tactica_build_grid_string(grid, grid_text, grid_text_size);
        tactica_guardar_diagrama(partido_id, nombre, grid_text);
        ui_printf("Diagrama guardado.\n");
        pause_console();
        return 1;

    case 'p':
    case 'P':
        return tactica_colocar(line + 1, grid, grid_text, grid_text_size);

    case 'd':
    case 'D':
        return tactica_borrar(line + 1, grid, grid_text, grid_text_size);

    default:
        if (isdigit((unsigned char)cmd))
            return tactica_colocar(line, grid, grid_text, grid_text_size);
        ui_printf("Comando no reconocido.\n");
        pause_console();
        return 0;
    }
}

static int tactica_listar_diagramas_simple(int con_pause)
{
    const char *sql =
        "SELECT id, partido_id, nombre, fecha FROM tactica_diagrama ORDER BY id DESC LIMIT 20";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando diagramas.\n");
        if (con_pause)
        {
            pause_console();
        }
        return 0;
    }

    ui_printf_centered_line("ID | Partido | Fecha | Nombre");
    ui_printf_centered_line("-----------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int partido_id = sqlite3_column_int(stmt, 1);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);

        ui_printf_centered_line("%d | %d | %s | %s", id, partido_id, fecha ? fecha : "", nombre ? nombre : "");
        count++;
    }

    sqlite3_finalize(stmt);
    if (count == 0)
    {
        ui_printf_centered_line("No hay diagramas registrados.");
    }

    if (con_pause)
    {
        pause_console();
    }

    return count;
}

static void tactica_ver_diagrama()
{
    clear_screen();
    print_header("DIAGRAMAS TACTICOS");

    int count = tactica_listar_diagramas_simple(0);
    if (count == 0)
    {
        pause_console();
        return;
    }

    int id = input_int("ID del diagrama (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    const char *sql = "SELECT nombre, grid FROM tactica_diagrama WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando diagrama.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        const char *grid = (const char *)sqlite3_column_text(stmt, 1);

        clear_screen();
        print_header(nombre ? nombre : "DIAGRAMA");
        if (grid && grid[0])
        {
            ui_puts(grid);
        }
        else
        {
            ui_puts("(Sin contenido)");
        }
    }
    else
    {
        printf("Diagrama no encontrado.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void tactica_crear_diagrama()
{
    clear_screen();
    print_header("CREAR DIAGRAMA TACTICO");

    tactica_mostrar_partidos_disponibles();

    int partido_id = input_int("ID de partido (0 para cancelar): ");
    if (partido_id <= 0)
        return;

    if (!existe_id("partido", partido_id))
    {
        printf("Partido no encontrado.\n");
        pause_console();
        return;
    }

    char nombre[128] = {0};
    tactica_leer_nombre_diagrama(nombre, sizeof(nombre));

    char grid[TACTIC_H][TACTIC_W + 1];
    tactica_init_grid(grid);

    char grid_text[(TACTIC_H * (TACTIC_W + 1)) + 1];
    tactica_build_grid_string(grid, grid_text, sizeof(grid_text));

    for (;;)
    {
        clear_screen();
        print_header("EDITAR DIAGRAMA");
        tactica_mostrar_grid_con_ejes(grid);
        ui_printf("\nComandos:\n");
        ui_printf("  Col Fila C -> colocar caracter C (ej: 10 5 X)\n");
        ui_printf("  d Col Fila -> borrar posicion\n");
        ui_printf("  r          -> reiniciar diagrama\n");
        ui_printf("  s          -> guardar diagrama\n");
        ui_printf("  q          -> cancelar\n");
        ui_printf("\nIngrese comando: ");

        char line[64] = {0};
        if (!fgets(line, sizeof(line), stdin))
            return;

        tactica_trim_newline(line);

        if (line[0] == '\0')
            continue;

        if (tactica_procesar_comando(line, grid, grid_text, sizeof(grid_text), partido_id, nombre))
            return;
    }
}

void menu_tacticas_partido()
{
    MenuItem items[] =
    {
        {1, "Crear diagrama", tactica_crear_diagrama},
        {2, "Ver diagramas", tactica_ver_diagrama},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("ANALISIS TACTICO");
    ejecutar_menu("ANALISIS TACTICO", items, 3);
}

/* ------------------- Favoritos y Etiquetas (Tags) para partidos ------------------- */

/* Longitud maxima para una etiqueta */
#define PARTIDO_TAG_MAX_LEN 64
/* Usar `trim_whitespace` global definida en `utils.c` */

static void partido_init_meta_tables(void)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_meta = "CREATE TABLE IF NOT EXISTS partido_meta (partido_id INTEGER PRIMARY KEY, favorito INTEGER DEFAULT 0)";
    if (preparar_stmt(sql_meta, &stmt))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char *sql_tags = "CREATE TABLE IF NOT EXISTS partido_tag (id INTEGER PRIMARY KEY AUTOINCREMENT, partido_id INTEGER NOT NULL, tag TEXT NOT NULL, UNIQUE(partido_id, tag))";
    if (preparar_stmt(sql_tags, &stmt))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

static int partido_obtener_favorito(int partido_id)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT favorito FROM partido_meta WHERE partido_id = ?";
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, partido_id);
    int fav = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        fav = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return fav;
}

static void partido_marcar_favorito(int partido_id, int favorito)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR REPLACE INTO partido_meta (partido_id, favorito) VALUES (?, ?)";
    if (!preparar_stmt(sql, &stmt))
        return;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_int(stmt, 2, favorito ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static int partido_agregar_tag(int partido_id, const char *tag)
{
    if (!tag || tag[0] == '\0')
        return 0;
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR IGNORE INTO partido_tag (partido_id, tag) VALUES (?, ?)";
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, tag, -1, SQLITE_TRANSIENT);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return res;
}

static int partido_quitar_tag(int partido_id, const char *tag)
{
    if (!tag || tag[0] == '\0')
        return 0;
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM partido_tag WHERE partido_id = ? AND tag = ?";
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, tag, -1, SQLITE_TRANSIENT);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return res;
}

static int partido_listar_tags_print(int partido_id)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT tag FROM partido_tag WHERE partido_id = ? ORDER BY tag ASC";
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando etiquetas.\n");
        return 0;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    int count = 0;
    ui_printf("Etiquetas: ");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *t = (const char *)sqlite3_column_text(stmt, 0);
        if (count)
            ui_printf(", ");
        ui_printf("%s", t ? t : "");
        count++;
    }
    ui_printf("\n");
    sqlite3_finalize(stmt);
    return count;
}

static int partido_prompt_tag_input(const char *prompt, char *out, size_t size)
{
    if (!prompt || !out || size == 0)
        return 0;
    ui_printf("%s", prompt);
    /* fgets expects an int for size; cast explicitly to avoid implicit
       conversion warnings when passing size_t (e.g. sizeof buffers). */
    if (!fgets(out, (int)size, stdin))
        return 0;
    tactica_trim_newline(out);
    trim_whitespace(out);
    return out[0] != '\0';
}

static void partido_ui_agregar_tag(int partido_id)
{
    char tag[PARTIDO_TAG_MAX_LEN] = {0};
    if (!partido_prompt_tag_input("Etiqueta (ej: Final, Amistoso, Importante): ", tag, sizeof(tag)))
    {
        ui_printf("Etiqueta vacia.\n");
        pause_console();
        return;
    }
    if (partido_agregar_tag(partido_id, tag))
        ui_printf("Etiqueta añadida.\n");
    else
        ui_printf("No se pudo agregar etiqueta o ya existe.\n");
    pause_console();
}

static void partido_ui_quitar_tag(int partido_id)
{
    char tag[PARTIDO_TAG_MAX_LEN] = {0};
    if (!partido_prompt_tag_input("Etiqueta a quitar: ", tag, sizeof(tag)))
    {
        ui_printf("Etiqueta vacía.\n");
        pause_console();
        return;
    }
    if (partido_quitar_tag(partido_id, tag))
        ui_printf("Etiqueta eliminada.\n");
    else
        ui_printf("Etiqueta no encontrada.\n");
    pause_console();
}

/* Helper: UI loop to manage tags for a specific partido ID. Extracted to
   reduce cognitive complexity of the parent menu function. */
static void partido_manage_tags_for_id(int id)
{
    for (;;)
    {
        clear_screen();
        print_header("ETIQUETAS DEL PARTIDO");
        ui_printf("Partido ID: %d\n\n", id);
        partido_listar_tags_print(id);

        ui_printf("1) Agregar etiqueta\n");
        ui_printf("2) Quitar etiqueta\n");
        ui_printf("0) Volver\n");
        int opt2 = input_int("Opcion: ");
        if (opt2 == 0)
            break;

        switch (opt2)
        {
        case 1:
            partido_ui_agregar_tag(id);
            break;
        case 2:
            partido_ui_quitar_tag(id);
            break;
        default:
            ui_printf("Opcion invalida.\n");
            pause_console();
            break;
        }
    }
}

/* Helper: UI to input an optional tag and list partidos with that tag (or
   all partidos that have tags). Extracted to simplify the parent menu. */
static int partido_mostrar_partidos_con_tag(const char *tag);
static void partido_list_tags_ui(void)
{
    char tag[PARTIDO_TAG_MAX_LEN] = {0};
    ui_printf("Etiqueta (dejar vacia para listar todos con tags): ");
    if (!fgets(tag, (int)sizeof(tag), stdin))
        tag[0] = '\0';
    tactica_trim_newline(tag);
    trim_whitespace(tag);

    int res = partido_mostrar_partidos_con_tag(tag[0] ? tag : NULL);
    if (res == 0)
    {
        if (tag[0])
            ui_printf("No hay partidos con la etiqueta '%s'.\n", tag);
        else
            ui_printf("No hay partidos con etiquetas.\n");
    }
    pause_console();
}

static int partido_mostrar_favoritos(void)
{
    partido_init_meta_tables();
    const char *sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
                      "JOIN cancha can ON p.cancha_id = can.id "
                      "JOIN partido_meta m ON p.id = m.partido_id "
                      "WHERE m.favorito = 1 ORDER BY p.id ASC";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando partidos favoritos.\n");
        return 0;
    }

    ui_printf_centered_line("--- Partidos favoritos ---");
    ui_printf_centered_line("ID | Cancha | Fecha");
    ui_printf_centered_line("---------------------------");

    int count = 0;
    char fecha_fmt[20];
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2),
                                fecha_fmt, sizeof(fecha_fmt));
        ui_printf_centered_line("%d | %s | %s",
                                sqlite3_column_int(stmt, 0),
                                (const char *)sqlite3_column_text(stmt, 1),
                                fecha_fmt);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        ui_printf_centered_line("No hay partidos favoritos.");
    ui_printf("\n");
    return count;
}

static void menu_marcar_favorito_partido()
{
    for (;;)
    {
        clear_screen();
        print_header("FAVORITOS");

        ui_printf("1) Marcar/Desmarcar favorito\n");
        ui_printf("2) Listar solo favoritos\n");
        ui_printf("0) Volver\n");
        int opt = input_int("Opcion: ");
        if (opt == 0)
            return;

        switch (opt)
        {
        case 1:
        {
            tactica_mostrar_partidos_disponibles();
            int id = input_int("ID de partido (0 para cancelar): ");
            if (id == 0)
                break;
            if (!existe_id("partido", id))
            {
                printf("Partido no encontrado.\n");
                pause_console();
                break;
            }
            {
                int fav = partido_obtener_favorito(id);
                partido_marcar_favorito(id, !fav);
                ui_printf("Partido %s favorito.\n", !fav ? "marcado como" : "desmarcado como");
            }
            pause_console();
            break;
        }
        case 2:
        {
            clear_screen();
            print_header("PARTIDOS FAVORITOS");
            partido_mostrar_favoritos();
            pause_console();
            break;
        }
        default:
            ui_printf("Opcion no reconocida.\n");
            pause_console();
            break;
        }
    }
}

static int partido_mostrar_partidos_con_tag(const char *tag)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = NULL;

    if (tag && tag[0] != '\0')
    {
        sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
              "JOIN cancha can ON p.cancha_id = can.id "
              "JOIN partido_tag pt ON p.id = pt.partido_id "
              "WHERE pt.tag = ? ORDER BY p.id ASC";
        if (!preparar_stmt(sql, &stmt))
        {
            printf("Error consultando partidos con etiquetas.\n");
            return 0;
        }
        sqlite3_bind_text(stmt, 1, tag, -1, SQLITE_TRANSIENT);
    }
    else
    {
        sql = "SELECT p.id, can.nombre, p.fecha_hora, GROUP_CONCAT(pt.tag, ', ') as tags FROM partido p "
              "JOIN cancha can ON p.cancha_id = can.id "
              "JOIN partido_tag pt ON p.id = pt.partido_id "
              "GROUP BY p.id ORDER BY p.id ASC";
        if (!preparar_stmt(sql, &stmt))
        {
            printf("Error consultando partidos con etiquetas.\n");
            return 0;
        }
    }

    ui_printf_centered_line("--- Partidos con etiquetas ---");
    ui_printf_centered_line("ID | Cancha | Fecha | Etiquetas");
    ui_printf_centered_line("-------------------------------------------");

    int count = 0;
    char fecha_fmt[20];
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *cancha = (const char *)sqlite3_column_text(stmt, 1);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 2);
        format_date_for_display(fecha, fecha_fmt, sizeof(fecha_fmt));

        if (tag && tag[0] != '\0')
        {
            ui_printf_centered_line("%d | %s | %s | %s", id,
                                    cancha ? cancha : "",
                                    fecha_fmt,
                                    tag);
        }
        else
        {
            const char *tags = (const char *)sqlite3_column_text(stmt, 3);
            ui_printf_centered_line("%d | %s | %s | %s", id,
                                    cancha ? cancha : "",
                                    fecha_fmt,
                                    tags ? tags : "");
        }
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        return 0;

    ui_printf("\n");
    return count;
}

static void menu_gestion_tags_partido()
{
    for (;;)
    {
        clear_screen();
        print_header("GESTIONAR ETIQUETAS (TAGS)");

        ui_printf("1) Gestionar etiquetas de un partido\n");
        ui_printf("2) Listar partidos con etiquetas\n");
        ui_printf("0) Volver\n");
        int opt = input_int("Opcion: ");
        if (opt == 0)
            return;

        switch (opt)
        {
        case 1:
        {
            tactica_mostrar_partidos_disponibles();
            int id = input_int("ID de partido (0 para cancelar): ");
            if (id == 0)
                break;
            if (!existe_id("partido", id))
            {
                printf("Partido no encontrado.\n");
                pause_console();
                break;
            }

            partido_manage_tags_for_id(id);
            break;
        }
        case 2:
            partido_list_tags_ui();
            break;
        default:
            ui_printf("Opcion invalida.\n");
            pause_console();
            break;
        }
    }
}

void menu_partidos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_partido},
        {2, "Listar", listar_partidos},
        {3, "Modificar", modificar_partido},
        {4, "Eliminar", eliminar_partido},
        {5, "Simular con Equipos Guardados", simular_partido_guardados},
        {6, "Analisis Tactico", menu_tacticas_partido},
        {7, "Favoritos", menu_marcar_favorito_partido},
        {8, "Etiquetas (Tags)", menu_gestion_tags_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("PARTIDOS", items, 9);
}
