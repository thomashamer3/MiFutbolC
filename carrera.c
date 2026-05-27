#include "carrera.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ======== helpers internos ======== */

#define SQL_EXPR_ANIO_FECHA_SEGURO \
    "CASE " \
    "  WHEN fecha_hora LIKE '____-__-__%' THEN CAST(substr(fecha_hora, 1, 4) AS INTEGER) " \
    "  WHEN fecha_hora LIKE '__/__/____%' THEN CAST(substr(fecha_hora, 7, 4) AS INTEGER) " \
    "  ELSE NULL " \
    "END"

#define SQL_EXPR_ANIO_FECHA_FLEX \
    "CASE " \
    "  WHEN fecha_hora LIKE '____-__-__%' THEN CAST(substr(fecha_hora, 1, 4) AS INTEGER) " \
    "  ELSE CAST(substr(fecha_hora, 7, 4) AS INTEGER) " \
    "END"

#define SQL_EXPR_MES_FECHA \
    "CASE " \
    "  WHEN fecha_hora LIKE '____-__-__%' THEN CAST(substr(fecha_hora, 6, 2) AS INTEGER) " \
    "  WHEN fecha_hora LIKE '__/__/____%' THEN CAST(substr(fecha_hora, 4, 2) AS INTEGER) " \
    "  ELSE NULL " \
    "END"

#define SQL_EXPR_DIA_FECHA \
    "CASE " \
    "  WHEN fecha_hora LIKE '____-__-__%' THEN CAST(substr(fecha_hora, 9, 2) AS INTEGER) " \
    "  WHEN fecha_hora LIKE '__/__/____%' THEN CAST(substr(fecha_hora, 1, 2) AS INTEGER) " \
    "  ELSE NULL " \
    "END"

#define SQL_FECHA_ORD(col) \
    "CASE " \
    "  WHEN " col " LIKE '____-__-__%' THEN substr(" col ", 1, 10) " \
    "  WHEN " col " LIKE '__/__/____%' THEN substr(" col ", 7, 4) || '-' || substr(" col ", 4, 2) || '-' || substr(" col ", 1, 2) " \
    "  ELSE " col " " \
    "END"

#define SEP_MAYOR " ========================================\n"
#define SEP_MENOR " ----------------------------------------\n"

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static int preparar_stmt_msg(sqlite3_stmt **stmt, const char *sql)
{
    if (preparar_stmt(stmt, sql))
        return 1;
    printf("Error al consultar la base de datos.\n");
    return 0;
}

static void imprimir_titulo_carrera_usuario(void)
{
    char *nombre_usuario = get_user_name();

    if (nombre_usuario && nombre_usuario[0] != '\0')
        printf("\n  Carrera Futbolistica de %s\n", nombre_usuario);
    else
        printf("\n  Carrera Futbolistica del Usuario\n");

    free(nombre_usuario);
}

static int iniciar_vista_carrera(const char *titulo)
{
    clear_screen();
    print_header(titulo);

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return 0;
    }

    return 1;
}

typedef struct
{
    int tipo; /* 1=victorias, 2=invicto, 3=derrotas */
    int victorias;
    int derrotas;
    int invicto;
} RachaActual;

static void actualizar_racha_actual(RachaActual *racha, int resultado, int *detener)
{
    if (racha->tipo == 0)
    {
        if (resultado == 1)
            racha->tipo = 1;
        else if (resultado == 3)
            racha->tipo = 3;
        else
            racha->tipo = 2;
    }

    if (racha->tipo == 1)
    {
        if (resultado == 1)
            racha->victorias++;
        else
            *detener = 1;
        return;
    }

    if (racha->tipo == 3)
    {
        if (resultado == 3)
            racha->derrotas++;
        else
            *detener = 1;
        return;
    }

    if (resultado != 3)
        racha->invicto++;
    else
        *detener = 1;
}

static int obtener_racha_actual(RachaActual *racha)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT resultado FROM partido ORDER BY fecha_hora DESC";

    racha->tipo = 0;
    racha->victorias = 0;
    racha->derrotas = 0;
    racha->invicto = 0;

    if (!preparar_stmt(&stmt, sql))
        return 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int detener = 0;
        int resultado = sqlite3_column_int(stmt, 0);
        actualizar_racha_actual(racha, resultado, &detener);
        if (detener)
            break;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static void imprimir_racha_actual(const RachaActual *racha)
{
    printf("\n  ---- Racha actual ----\n");

    if (racha->tipo == 1 && racha->victorias > 0)
        printf("  Victorias consecutivas : %d\n", racha->victorias);
    else if (racha->tipo == 3 && racha->derrotas > 0)
        printf("  Derrotas consecutivas  : %d\n", racha->derrotas);
    else if (racha->tipo == 2 && racha->invicto > 0)
        printf("  Partidos invicto       : %d\n", racha->invicto);
}

/* ========================================================
 * 1. CARRERA FUTBOLISTICA
 * Partidos, Victorias, Empates, Derrotas, Goles, Asistencias,
 * Promedio goles/partido
 * ======================================================== */
static void mostrar_carrera_futbolistica(void)
{
    if (!iniciar_vista_carrera("CARRERA FUTBOLISTICA"))
        return;

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "  COUNT(*) AS partidos, "
        "  SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
        "  SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
        "  SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
        "  COALESCE(SUM(goles), 0) AS goles, "
        "  COALESCE(SUM(asistencias), 0) AS asistencias "
        "FROM partido";

    if (!preparar_stmt_msg(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos disponibles.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int partidos     = sqlite3_column_int(stmt, 0);
    int victorias    = sqlite3_column_int(stmt, 1);
    int empates      = sqlite3_column_int(stmt, 2);
    int derrotas     = sqlite3_column_int(stmt, 3);
    int goles        = sqlite3_column_int(stmt, 4);
    int asistencias  = sqlite3_column_int(stmt, 5);
    double prom_goles = (partidos > 0) ? (double)goles / partidos : 0.0;

    sqlite3_finalize(stmt);

    imprimir_titulo_carrera_usuario();
    printf("%s\n", SEP_MAYOR);
    printf("  Partidos jugados : %d\n", partidos);
    printf("  Victorias        : %d\n", victorias);
    printf("  Empates          : %d\n", empates);
    printf("  Derrotas         : %d\n\n", derrotas);
    printf("  Goles            : %d\n", goles);
    printf("  Asistencias      : %d\n\n", asistencias);
    printf("  Promedio goles/partido : %.2f\n", prom_goles);

    /* Porcentajes de resultados */
    if (partidos > 0)
    {
        printf("\n  ---- Distribucion de resultados ----\n");
        printf("  Victorias : %5.1f%%\n", (double)victorias / partidos * 100.0);
        printf("  Empates   : %5.1f%%\n", (double)empates   / partidos * 100.0);
        printf("  Derrotas  : %5.1f%%\n", (double)derrotas  / partidos * 100.0);
    }
    RachaActual racha;
    if (obtener_racha_actual(&racha))
        imprimir_racha_actual(&racha);

    pause_console();
}

/* ========================================================
 * 2. TU HISTORIA FUTBOLISTICA
 * Primer partido (anio), Partidos, Goles, Asistencias,
 * Mejor anio con sus goles
 * ======================================================== */
static void mostrar_historia_futbolistica(void)
{
    if (!iniciar_vista_carrera("TU HISTORIA FUTBOLISTICA"))
        return;

    /* --- Datos globales, primer partido y mejor anio --- */
    sqlite3_stmt *stmt;
    const char *sql =
        "WITH por_anio AS ("
        "  SELECT " SQL_EXPR_ANIO_FECHA_FLEX " AS anio, SUM(goles) AS total_goles "
        "  FROM partido "
        "  GROUP BY anio"
        "), mejor AS ("
        "  SELECT anio, total_goles FROM por_anio ORDER BY total_goles DESC LIMIT 1"
        ") "
        "SELECT "
        "  COUNT(*), "
        "  COALESCE(SUM(goles), 0), "
        "  COALESCE(SUM(asistencias), 0), "
        "  MIN(" SQL_EXPR_ANIO_FECHA_SEGURO "), "
        "  MIN(fecha_hora), "
        "  COALESCE((SELECT anio FROM mejor), 0), "
        "  COALESCE((SELECT total_goles FROM mejor), 0) "
        "FROM partido";

    if (!preparar_stmt_msg(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos disponibles.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int partidos = sqlite3_column_int(stmt, 0);
    int goles = sqlite3_column_int(stmt, 1);
    int asistencias = sqlite3_column_int(stmt, 2);
    int anio_inicio = sqlite3_column_int(stmt, 3);
    const char *primera_fecha_txt = (const char *)sqlite3_column_text(stmt, 4);
    int mejor_anio = sqlite3_column_int(stmt, 5);
    int goles_mejor_anio = sqlite3_column_int(stmt, 6);
    char primera_fecha[64] = "";

    if (primera_fecha_txt)
        snprintf(primera_fecha, sizeof(primera_fecha), "%s", primera_fecha_txt);

    sqlite3_finalize(stmt);

    imprimir_titulo_carrera_usuario();
    printf("%s\n", SEP_MAYOR);
    printf("\n  Tu historia futbolistica\n");

    if (anio_inicio > 0)
        printf("  Primer partido   : %d\n", anio_inicio);
    else
        printf("  Primer partido   : %s\n", primera_fecha[0] ? primera_fecha : "N/A");

    printf("  Partidos jugados : %d\n", partidos);
    printf("  Goles            : %d\n", goles);
    printf("  Asistencias      : %d\n\n", asistencias);

    if (mejor_anio > 0)
    {
        printf("  Tu mejor anio    : %d\n", mejor_anio);
        printf("    Goles          : %d\n", goles_mejor_anio);
    }

    /* --- Desglose por anio --- */
    sqlite3_stmt *stmt3;
    const char *sql_anios =
        "SELECT "
        "  " SQL_EXPR_ANIO_FECHA_FLEX " AS anio, "
        "  COUNT(*) AS partidos, "
        "  SUM(goles) AS goles, "
        "  SUM(asistencias) AS asistencias, "
        "  ROUND(AVG(rendimiento_general), 1) AS rend_prom "
        "FROM partido "
        "GROUP BY anio "
        "ORDER BY anio ASC";

    if (preparar_stmt(&stmt3, sql_anios))
    {
        printf("\n  ---- Desglose por anio ----\n");
        printf("  %-6s  %8s  %6s  %6s  %6s\n", "Anio", "Partidos", "Goles", "Asist", "Rend");
        printf("  %-6s  %8s  %6s  %6s  %6s\n", "------", "--------", "------", "------", "------");

        while (sqlite3_step(stmt3) == SQLITE_ROW)
        {
            int a = sqlite3_column_int(stmt3, 0);
            int p = sqlite3_column_int(stmt3, 1);
            int g = sqlite3_column_int(stmt3, 2);
            int as = sqlite3_column_int(stmt3, 3);
            double r = sqlite3_column_double(stmt3, 4);
            printf("  %-6d  %8d  %6d  %6d  %6.1f\n", a, p, g, as, r);
        }
        sqlite3_finalize(stmt3);
    }

    pause_console();
}

/* ========================================================
 * 3. RESUMEN GENERAL DE CARRERA
 * Inicio carrera, Anios jugando, Promedio goles/anio
 * ======================================================== */
static void mostrar_resumen_carrera(void)
{
    if (!iniciar_vista_carrera("RESUMEN GENERAL DE CARRERA"))
        return;

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "  MIN(" SQL_EXPR_ANIO_FECHA_FLEX ") AS inicio, "
        "  MAX(" SQL_EXPR_ANIO_FECHA_FLEX ") AS fin, "
        "  COUNT(*) AS partidos, "
        "  COALESCE(SUM(goles), 0) AS goles, "
        "  COALESCE(SUM(asistencias), 0) AS asistencias, "
        "  ROUND(AVG(rendimiento_general), 2) AS rend_prom "
        "FROM partido";

    if (!preparar_stmt_msg(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos disponibles.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int anio_inicio  = sqlite3_column_int(stmt, 0);
    int anio_fin     = sqlite3_column_int(stmt, 1);
    int partidos     = sqlite3_column_int(stmt, 2);
    int goles        = sqlite3_column_int(stmt, 3);
    int asistencias  = sqlite3_column_int(stmt, 4);
    double rend_prom = sqlite3_column_double(stmt, 5);

    sqlite3_finalize(stmt);

    int anios_jugando = (anio_fin > anio_inicio) ? (anio_fin - anio_inicio + 1) : 1;
    double goles_por_anio = (double)goles / anios_jugando;
    double asist_por_anio = (double)asistencias / anios_jugando;
    double partidos_por_anio = (double)partidos / anios_jugando;

    imprimir_titulo_carrera_usuario();
    printf("%s\n", SEP_MAYOR);
    printf("\n  Resumen General de Carrera\n");
    printf("  Inicio carrera       : %d\n", anio_inicio);
    printf("  Anios jugando        : %d\n", anios_jugando);
    printf("  Total partidos       : %d\n\n", partidos);

    printf("  ---- Promedios por anio ----\n");
    printf("  Partidos/anio        : %.1f\n", partidos_por_anio);
    printf("  Goles/anio           : %.1f\n", goles_por_anio);
    printf("  Asistencias/anio     : %.1f\n\n", asist_por_anio);

    printf("  ---- Rendimiento global ----\n");
    printf("  Rendimiento promedio : %.2f\n", rend_prom);

    RachaActual racha;
    if (obtener_racha_actual(&racha))
        imprimir_racha_actual(&racha);

    pause_console();
}

typedef struct
{
    char nombre[128];
    int edad;
    char pie_habil[24];
    char posiciones[160];
    double altura_cm;
    double peso_kg;
    char estilo[80];
    int dorsal_favorito;
    char objetivos[320];
    char historia[512];
} CarreraIdentidad;

typedef struct
{
    int partidos;
    int victorias;
    int goles;
    int asistencias;
    double avg_goles;
    double avg_asistencias;
    double avg_rendimiento;
    double avg_animo;
    double dispersion_rendimiento;
    double momentum_delta;
    char etiqueta[32];
    char descripcion[180];
} PerfilDinamico;

static void identidad_set_defaults(CarreraIdentidad *identidad)
{
    if (!identidad)
        return;

    memset(identidad, 0, sizeof(*identidad));
    snprintf(identidad->pie_habil, sizeof(identidad->pie_habil), "Derecho");

    char *nombre_usuario = get_user_name();
    if (nombre_usuario && nombre_usuario[0] != '\0')
    {
        snprintf(identidad->nombre, sizeof(identidad->nombre), "%s", nombre_usuario);
    }
    free(nombre_usuario);
}

static void copiar_texto_limited(char *dest, size_t dest_size, const char *src)
{
    if (!dest || dest_size == 0)
        return;

    if (!src)
    {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static void identidad_cargar_fila(sqlite3_stmt *stmt, CarreraIdentidad *identidad)
{
    const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
    const char *pie_habil = (const char *)sqlite3_column_text(stmt, 2);
    const char *posiciones = (const char *)sqlite3_column_text(stmt, 3);
    const char *estilo = (const char *)sqlite3_column_text(stmt, 6);
    const char *objetivos = (const char *)sqlite3_column_text(stmt, 8);
    const char *historia = (const char *)sqlite3_column_text(stmt, 9);

    if (nombre)
        copiar_texto_limited(identidad->nombre, sizeof(identidad->nombre), nombre);
    identidad->edad = sqlite3_column_int(stmt, 1);
    if (pie_habil)
        copiar_texto_limited(identidad->pie_habil, sizeof(identidad->pie_habil), pie_habil);
    if (posiciones)
        copiar_texto_limited(identidad->posiciones, sizeof(identidad->posiciones), posiciones);
    identidad->altura_cm = sqlite3_column_double(stmt, 4);
    identidad->peso_kg = sqlite3_column_double(stmt, 5);
    if (estilo)
        copiar_texto_limited(identidad->estilo, sizeof(identidad->estilo), estilo);
    identidad->dorsal_favorito = sqlite3_column_int(stmt, 7);
    if (objetivos)
        copiar_texto_limited(identidad->objetivos, sizeof(identidad->objetivos), objetivos);
    if (historia)
        copiar_texto_limited(identidad->historia, sizeof(identidad->historia), historia);
}

static void identidad_completar_desde_salud(CarreraIdentidad *identidad)
{
    if (!identidad)
        return;

    if (identidad->altura_cm > 0.0 && identidad->peso_kg > 0.0)
        return;

    sqlite3_stmt *stmt_salud;
    const char *sql_salud = "SELECT altura_cm, peso_kg FROM bienestar_salud WHERE id = 1";
    if (!preparar_stmt(&stmt_salud, sql_salud))
    {
        return;
    }

    if (sqlite3_step(stmt_salud) == SQLITE_ROW)
    {
        if (identidad->altura_cm <= 0.0)
            identidad->altura_cm = sqlite3_column_double(stmt_salud, 0);
        if (identidad->peso_kg <= 0.0)
            identidad->peso_kg = sqlite3_column_double(stmt_salud, 1);
    }

    sqlite3_finalize(stmt_salud);
}

static int cargar_identidad(CarreraIdentidad *identidad)
{
    if (!identidad)
        return 0;

    identidad_set_defaults(identidad);

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT nombre, edad, pie_habil, posiciones, altura_cm, peso_kg, estilo, dorsal_favorito, objetivos, historia "
        "FROM carrera_identidad WHERE id = 1";

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
        identidad_cargar_fila(stmt, identidad);

    sqlite3_finalize(stmt);

    identidad_completar_desde_salud(identidad);

    return 1;
}

static int guardar_identidad(const CarreraIdentidad *identidad)
{
    if (!identidad)
        return 0;

    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO carrera_identidad (id, nombre, edad, pie_habil, posiciones, altura_cm, peso_kg, estilo, dorsal_favorito, objetivos, historia, updated_at) "
        "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, date('now')) "
        "ON CONFLICT(id) DO UPDATE SET "
        "nombre = excluded.nombre, edad = excluded.edad, pie_habil = excluded.pie_habil, "
        "posiciones = excluded.posiciones, altura_cm = excluded.altura_cm, peso_kg = excluded.peso_kg, "
        "estilo = excluded.estilo, dorsal_favorito = excluded.dorsal_favorito, objetivos = excluded.objetivos, "
        "historia = excluded.historia, updated_at = excluded.updated_at";

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, identidad->nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, identidad->edad);
    sqlite3_bind_text(stmt, 3, identidad->pie_habil, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, identidad->posiciones, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, identidad->altura_cm);
    sqlite3_bind_double(stmt, 6, identidad->peso_kg);
    sqlite3_bind_text(stmt, 7, identidad->estilo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, identidad->dorsal_favorito);
    sqlite3_bind_text(stmt, 9, identidad->objetivos, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, identidad->historia, -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static void mostrar_ficha_identidad(void)
{
    clear_screen();
    print_header("FICHA DE IDENTIDAD DEL JUGADOR");

    CarreraIdentidad identidad;
    if (!cargar_identidad(&identidad))
    {
        printf("No se pudo cargar la ficha de identidad.\n");
        pause_console();
        return;
    }

    printf("Nombre: %s\n", identidad.nombre[0] ? identidad.nombre : "N/A");
    printf("Edad: %d\n", identidad.edad);
    printf("Pie habil: %s\n", identidad.pie_habil[0] ? identidad.pie_habil : "N/A");
    printf("Posiciones: %s\n", identidad.posiciones[0] ? identidad.posiciones : "N/A");

    if (identidad.altura_cm > 0.0)
        printf("Altura: %.1f cm\n", identidad.altura_cm);
    else
        printf("Altura: N/A\n");

    if (identidad.peso_kg > 0.0)
        printf("Peso: %.1f kg\n", identidad.peso_kg);
    else
        printf("Peso: N/A\n");

    printf("Estilo: %s\n", identidad.estilo[0] ? identidad.estilo : "N/A");
    printf("Dorsal favorito: %d\n", identidad.dorsal_favorito);
    printf("Objetivos: %s\n", identidad.objetivos[0] ? identidad.objetivos : "N/A");
    printf("Historia: %s\n", identidad.historia[0] ? identidad.historia : "N/A");

    pause_console();
}

static void editar_ficha_identidad(void)
{
    clear_screen();
    print_header("EDITAR FICHA DE IDENTIDAD");

    CarreraIdentidad identidad;
    cargar_identidad(&identidad);

    char buffer[512];

    printf("Nombre actual: %s\n", identidad.nombre[0] ? identidad.nombre : "N/A");
    input_string("Nombre (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        copiar_texto_limited(identidad.nombre, sizeof(identidad.nombre), buffer);

    printf("Edad actual: %d\n", identidad.edad);
    input_string_extended("Edad (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        identidad.edad = atoi(buffer);

    printf("Pie habil actual: %s\n", identidad.pie_habil[0] ? identidad.pie_habil : "N/A");
    input_string("Pie habil (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        copiar_texto_limited(identidad.pie_habil, sizeof(identidad.pie_habil), buffer);

    printf("Posiciones actuales: %s\n", identidad.posiciones[0] ? identidad.posiciones : "N/A");
    input_string("Posiciones (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        copiar_texto_limited(identidad.posiciones, sizeof(identidad.posiciones), buffer);

    if (identidad.altura_cm > 0.0)
        printf("Altura actual: %.1f cm\n", identidad.altura_cm);
    else
        printf("Altura actual: N/A\n");
    input_string_extended("Altura cm (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        identidad.altura_cm = atof(buffer);

    if (identidad.peso_kg > 0.0)
        printf("Peso actual: %.1f kg\n", identidad.peso_kg);
    else
        printf("Peso actual: N/A\n");
    input_string_extended("Peso kg (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        identidad.peso_kg = atof(buffer);

    printf("Estilo actual: %s\n", identidad.estilo[0] ? identidad.estilo : "N/A");
    input_string("Estilo (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        copiar_texto_limited(identidad.estilo, sizeof(identidad.estilo), buffer);

    printf("Dorsal favorito actual: %d\n", identidad.dorsal_favorito);
    input_string_extended("Dorsal favorito (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        identidad.dorsal_favorito = atoi(buffer);

    printf("Objetivos actuales: %s\n", identidad.objetivos[0] ? identidad.objetivos : "N/A");
    input_string_extended("Objetivos (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        copiar_texto_limited(identidad.objetivos, sizeof(identidad.objetivos), buffer);

    printf("Historia actual: %s\n", identidad.historia[0] ? identidad.historia : "N/A");
    input_string_extended("Historia (Enter mantiene): ", buffer, (int)sizeof(buffer));
    if (buffer[0] != '\0')
        copiar_texto_limited(identidad.historia, sizeof(identidad.historia), buffer);

    if (guardar_identidad(&identidad))
        printf("Ficha de identidad actualizada.\n");
    else
        printf("No se pudo actualizar la ficha de identidad.\n");

    pause_console();
}

static void menu_identidad_jugador(void)
{
    MenuItem items[] =
    {
        {1, "Ver ficha", &mostrar_ficha_identidad},
        {2, "Editar ficha", &editar_ficha_identidad},
        {0, "Volver", NULL}
    };

    ejecutar_menu("IDENTIDAD DEL JUGADOR", items, 3);
}

static int calcular_momentum_delta(double *delta)
{
    if (!delta)
        return 0;

    *delta = 0.0;

    sqlite3_stmt *stmt;
    const char *sql_momentum =
        "SELECT "
        "  COALESCE((SELECT AVG(rendimiento_general) "
        "            FROM (SELECT rendimiento_general FROM partido ORDER BY id DESC LIMIT 5)), 0), "
        "  COALESCE((SELECT AVG(rendimiento_general) "
        "            FROM (SELECT rendimiento_general FROM partido ORDER BY id DESC LIMIT 5 OFFSET 5)), 0)";

    double actual = 0.0;
    double previo = 0.0;

    if (preparar_stmt(&stmt, sql_momentum))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            actual = sqlite3_column_double(stmt, 0);
            previo = sqlite3_column_double(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    *delta = actual - previo;
    return 1;
}

static int calcular_perfil_dinamico(PerfilDinamico *perfil)
{
    if (!perfil)
        return 0;

    memset(perfil, 0, sizeof(*perfil));

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT COUNT(*), "
        "COALESCE(SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(goles), 0), COALESCE(SUM(asistencias), 0), "
        "COALESCE(AVG(goles), 0), COALESCE(AVG(asistencias), 0), "
        "COALESCE(AVG(rendimiento_general), 0), COALESCE(AVG(estado_animo), 0) "
        "FROM partido";

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        perfil->partidos = sqlite3_column_int(stmt, 0);
        perfil->victorias = sqlite3_column_int(stmt, 1);
        perfil->goles = sqlite3_column_int(stmt, 2);
        perfil->asistencias = sqlite3_column_int(stmt, 3);
        perfil->avg_goles = sqlite3_column_double(stmt, 4);
        perfil->avg_asistencias = sqlite3_column_double(stmt, 5);
        perfil->avg_rendimiento = sqlite3_column_double(stmt, 6);
        perfil->avg_animo = sqlite3_column_double(stmt, 7);
    }
    sqlite3_finalize(stmt);

    const char *sql_dispersion =
        "SELECT COALESCE(AVG(ABS(rendimiento_general - (SELECT AVG(rendimiento_general) FROM partido))), 0) "
        "FROM partido";
    if (preparar_stmt(&stmt, sql_dispersion))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            perfil->dispersion_rendimiento = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    calcular_momentum_delta(&perfil->momentum_delta);

    double contrib_por_partido = perfil->avg_goles + perfil->avg_asistencias;
    double ratio_victorias = (perfil->partidos > 0)
                             ? (double)perfil->victorias * 100.0 / (double)perfil->partidos
                             : 0.0;

    if (perfil->avg_goles >= 1.20 && perfil->avg_asistencias < 0.70)
    {
        snprintf(perfil->etiqueta, sizeof(perfil->etiqueta), "Goleador");
        snprintf(perfil->descripcion, sizeof(perfil->descripcion), "Tu impacto principal llega por el gol.");
    }
    else if (perfil->avg_asistencias >= 1.00 && perfil->avg_goles < 0.90)
    {
        snprintf(perfil->etiqueta, sizeof(perfil->etiqueta), "Asistidor");
        snprintf(perfil->descripcion, sizeof(perfil->descripcion), "Generas juego y habilitas mucho a tus companeros.");
    }
    else if (perfil->avg_asistencias >= 0.80 && perfil->avg_goles >= 0.60)
    {
        snprintf(perfil->etiqueta, sizeof(perfil->etiqueta), "Playmaker");
        snprintf(perfil->descripcion, sizeof(perfil->descripcion), "Combinas creacion y definicion con buen equilibrio ofensivo.");
    }
    else if (perfil->avg_rendimiento >= 7.50 && perfil->avg_animo >= 7.00)
    {
        snprintf(perfil->etiqueta, sizeof(perfil->etiqueta), "Equilibrado");
        snprintf(perfil->descripcion, sizeof(perfil->descripcion), "Sostienes un rendimiento parejo en distintos contextos.");
    }
    else if (contrib_por_partido >= 0.70 && ratio_victorias < 45.0)
    {
        snprintf(perfil->etiqueta, sizeof(perfil->etiqueta), "Guerrero");
        snprintf(perfil->descripcion, sizeof(perfil->descripcion), "Aunque no siempre gane el equipo, mantienes aporte competitivo.");
    }
    else if (perfil->partidos >= 15 && perfil->dispersion_rendimiento <= 1.20)
    {
        snprintf(perfil->etiqueta, sizeof(perfil->etiqueta), "Regular");
        snprintf(perfil->descripcion, sizeof(perfil->descripcion), "Tu curva de rendimiento es estable partido a partido.");
    }
    else
    {
        snprintf(perfil->etiqueta, sizeof(perfil->etiqueta), "Inconsistente");
        snprintf(perfil->descripcion, sizeof(perfil->descripcion), "Hay picos altos y bajos marcados en tu rendimiento reciente.");
    }

    return 1;
}

static void mostrar_perfil_dinamico(void)
{
    if (!iniciar_vista_carrera("PERFIL DINAMICO DE JUGADOR"))
        return;

    PerfilDinamico perfil;
    if (!calcular_perfil_dinamico(&perfil) || perfil.partidos <= 0)
    {
        printf("No hay datos suficientes para calcular perfil dinamico.\n");
        pause_console();
        return;
    }

    printf("Perfil detectado: %s\n", perfil.etiqueta);
    printf("Descripcion: %s\n\n", perfil.descripcion);

    printf("Partidos: %d\n", perfil.partidos);
    printf("Victorias: %d\n", perfil.victorias);
    printf("Goles totales: %d | Asistencias totales: %d\n", perfil.goles, perfil.asistencias);
    printf("Promedio goles: %.2f | Promedio asistencias: %.2f\n", perfil.avg_goles, perfil.avg_asistencias);
    printf("Promedio rendimiento: %.2f | Promedio animo: %.2f\n", perfil.avg_rendimiento, perfil.avg_animo);
    printf("Dispersion rendimiento: %.2f\n", perfil.dispersion_rendimiento);

    if (perfil.momentum_delta > 0.40)
        printf("Momentum: ASCENDENTE (%.2f)\n", perfil.momentum_delta);
    else if (perfil.momentum_delta < -0.40)
        printf("Momentum: DESCENDENTE (%.2f)\n", perfil.momentum_delta);
    else
        printf("Momentum: ESTABLE (%.2f)\n", perfil.momentum_delta);

    pause_console();
}

static void listar_partidos_simple(int limite)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha_hora, goles, asistencias, rendimiento_general, resultado "
        "FROM partido ORDER BY id DESC LIMIT ?";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudieron listar partidos.\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, limite);

    printf("ID | Fecha | G | A | Rend | Resultado\n");
    printf("%s", SEP_MENOR);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        int goles = sqlite3_column_int(stmt, 2);
        int asistencias = sqlite3_column_int(stmt, 3);
        int rendimiento = sqlite3_column_int(stmt, 4);
        int resultado = sqlite3_column_int(stmt, 5);

        printf("%d | %s | %d | %d | %d | %s\n",
               id,
               fecha ? fecha : "N/A",
               goles,
               asistencias,
               rendimiento,
               resultado_to_text(resultado));
    }

    sqlite3_finalize(stmt);
}

static void seleccionar_tipo_hito(char *tipo_hito, size_t size)
{
    if (!tipo_hito || size == 0)
        return;

    printf("Tipo de hito:\n");
    printf("1) Memorable\n");
    printf("2) Triste\n");
    printf("3) Debut\n");
    printf("4) Clasico\n");
    printf("5) Lesion importante\n");
    printf("6) Mejor partido\n");
    printf("7) Otro\n");

    int opcion = input_int("> ");
    switch (opcion)
    {
    case 1:
        snprintf(tipo_hito, size, "Memorable");
        break;
    case 2:
        snprintf(tipo_hito, size, "Triste");
        break;
    case 3:
        snprintf(tipo_hito, size, "Debut");
        break;
    case 4:
        snprintf(tipo_hito, size, "Clasico");
        break;
    case 5:
        snprintf(tipo_hito, size, "Lesion importante");
        break;
    case 6:
        snprintf(tipo_hito, size, "Mejor partido");
        break;
    default:
        input_string("Tipo personalizado: ", tipo_hito, (int)size);
        if (tipo_hito[0] == '\0')
            snprintf(tipo_hito, size, "Memorable");
        break;
    }
}

static void guardar_hito_partido(void)
{
    if (!iniciar_vista_carrera("PARTIDOS QUE MARCARON"))
        return;

    listar_partidos_simple(30);
    int partido_id = input_int("ID de partido a etiquetar (0 cancelar): ");
    if (partido_id == 0)
        return;

    if (!existe_id("partido", partido_id))
    {
        printf("El partido no existe.\n");
        pause_console();
        return;
    }

    char tipo_hito[64] = "";
    char nota[240] = "";

    seleccionar_tipo_hito(tipo_hito, sizeof(tipo_hito));
    input_string_extended("Nota personal (opcional): ", nota, (int)sizeof(nota));

    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO carrera_partido_hito (partido_id, tipo_hito, nota, created_at) "
        "VALUES (?, ?, ?, date('now')) "
        "ON CONFLICT(partido_id) DO UPDATE SET "
        "tipo_hito = excluded.tipo_hito, nota = excluded.nota, created_at = excluded.created_at";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudo guardar el hito.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, tipo_hito, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, nota, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Partido marcado como hito.\n");
    else
        printf("Error guardando hito: %s\n", sqlite3_errmsg(db));

    sqlite3_finalize(stmt);
    pause_console();
}

static void listar_partidos_hito(void)
{
    if (!iniciar_vista_carrera("HITOS DE PARTIDOS"))
        return;

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT h.partido_id, p.fecha_hora, h.tipo_hito, COALESCE(h.nota, ''), p.goles, p.asistencias, p.rendimiento_general "
        "FROM carrera_partido_hito h "
        "JOIN partido p ON p.id = h.partido_id "
        "ORDER BY p.id DESC";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudieron cargar hitos.\n");
        pause_console();
        return;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int partido_id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo_hito = (const char *)sqlite3_column_text(stmt, 2);
        const char *nota = (const char *)sqlite3_column_text(stmt, 3);
        int goles = sqlite3_column_int(stmt, 4);
        int asistencias = sqlite3_column_int(stmt, 5);
        int rendimiento = sqlite3_column_int(stmt, 6);

        printf("Partido %d | %s | %s | G:%d A:%d R:%d\n",
               partido_id,
               fecha ? fecha : "N/A",
               tipo_hito ? tipo_hito : "Hito",
               goles,
               asistencias,
               rendimiento);
        if (nota && nota[0] != '\0')
            printf("  Nota: %s\n", nota);
        count++;
    }

    if (count == 0)
    {
        mostrar_no_hay_registros("hitos de partidos");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void eliminar_hito_partido(void)
{
    if (!iniciar_vista_carrera("ELIMINAR HITO DE PARTIDO"))
        return;

    listar_partidos_hito();
    int partido_id = input_int("ID de partido para quitar hito (0 cancelar): ");
    if (partido_id == 0)
        return;

    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM carrera_partido_hito WHERE partido_id = ?";
    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudo eliminar el hito.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Hito eliminado (si existia).\n");
    else
        printf("Error al eliminar hito: %s\n", sqlite3_errmsg(db));

    sqlite3_finalize(stmt);
    pause_console();
}

static void menu_partidos_hito(void)
{
    MenuItem items[] =
    {
        {1, "Marcar/actualizar partido", &guardar_hito_partido},
        {2, "Listar partidos marcados", &listar_partidos_hito},
        {3, "Eliminar marca", &eliminar_hito_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("PARTIDOS QUE MARCARON", items, 4);
}

static void mostrar_timeline_carrera(void)
{
    if (!iniciar_vista_carrera("TIMELINE DE CARRERA"))
        return;

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT fecha_ord, fecha_txt, titulo, detalle FROM ("
        "  SELECT "
        "    " SQL_FECHA_ORD("p.fecha_hora") " AS fecha_ord, "
        "    p.fecha_hora AS fecha_txt, "
        "    'Debut' AS titulo, "
        "    'Primer partido registrado' AS detalle "
        "  FROM partido p "
        "  WHERE p.id = (SELECT id FROM partido ORDER BY id ASC LIMIT 1) "
        "  UNION ALL "
        "  SELECT "
        "    " SQL_FECHA_ORD("p.fecha_hora") " AS fecha_ord, "
        "    p.fecha_hora AS fecha_txt, "
        "    'Primer gol' AS titulo, "
        "    'Se inaugura la cuenta goleadora' AS detalle "
        "  FROM partido p "
        "  WHERE p.id = (SELECT id FROM partido WHERE goles > 0 ORDER BY id ASC LIMIT 1) "
        "  UNION ALL "
        "  SELECT "
        "    " SQL_FECHA_ORD("p.fecha_hora") " AS fecha_ord, "
        "    p.fecha_hora AS fecha_txt, "
        "    'Primera asistencia' AS titulo, "
        "    'Se inaugura la cuenta de asistencias' AS detalle "
        "  FROM partido p "
        "  WHERE p.id = (SELECT id FROM partido WHERE asistencias > 0 ORDER BY id ASC LIMIT 1) "
        "  UNION ALL "
        "  SELECT "
        "    " SQL_FECHA_ORD("p.fecha_hora") " AS fecha_ord, "
        "    p.fecha_hora AS fecha_txt, "
        "    h.tipo_hito AS titulo, "
        "    COALESCE(h.nota, 'Partido marcado por el usuario') AS detalle "
        "  FROM carrera_partido_hito h "
        "  JOIN partido p ON p.id = h.partido_id "
        "  UNION ALL "
        "  SELECT "
        "    " SQL_FECHA_ORD("l.fecha") " AS fecha_ord, "
        "    l.fecha AS fecha_txt, "
        "    'Lesion' AS titulo, "
        "    COALESCE(l.tipo, 'Lesion') || ' - ' || COALESCE(l.descripcion, '') AS detalle "
        "  FROM lesion l "
        ") eventos "
        "WHERE fecha_ord IS NOT NULL "
        "ORDER BY fecha_ord ASC, fecha_txt ASC";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudo generar timeline.\n");
        pause_console();
        return;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha_txt = (const char *)sqlite3_column_text(stmt, 1);
        const char *titulo = (const char *)sqlite3_column_text(stmt, 2);
        const char *detalle = (const char *)sqlite3_column_text(stmt, 3);

        char fecha_display[64] = "";
        format_date_for_display(fecha_txt, fecha_display, (int)sizeof(fecha_display));
        if (fecha_display[0] == '\0' && fecha_txt)
            snprintf(fecha_display, sizeof(fecha_display), "%s", fecha_txt);

        printf("- %s | %s\n", fecha_display[0] ? fecha_display : "N/A", titulo ? titulo : "Evento");
        if (detalle && detalle[0] != '\0')
            printf("  %s\n", detalle);
        count++;
    }

    if (count == 0)
    {
        printf("No hay eventos para mostrar en timeline.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void hof_mostrar_mejor_partido(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha_hora, goles, asistencias, rendimiento_general "
        "FROM partido ORDER BY rendimiento_general DESC, (goles + asistencias) DESC, id DESC LIMIT 1";

    printf("MEJOR PARTIDO\n");
    printf("%s", SEP_MENOR);

    if (!preparar_stmt(&stmt, sql))
        return;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        printf("Partido %d | Fecha %s | G:%d A:%d | Rend:%d\n",
               sqlite3_column_int(stmt, 0),
               fecha ? fecha : "N/A",
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4));
    }
    else
    {
        printf("Sin datos de partidos.\n");
    }

    sqlite3_finalize(stmt);
}

static void hof_mostrar_peor_partido(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha_hora, goles, asistencias, rendimiento_general "
        "FROM partido ORDER BY rendimiento_general ASC, id DESC LIMIT 1";

    printf("\nPEOR PARTIDO\n");
    printf("%s", SEP_MENOR);

    if (!preparar_stmt(&stmt, sql))
        return;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        printf("Partido %d | Fecha %s | G:%d A:%d | Rend:%d\n",
               sqlite3_column_int(stmt, 0),
               fecha ? fecha : "N/A",
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4));
    }
    else
    {
        printf("Sin datos de partidos.\n");
    }

    sqlite3_finalize(stmt);
}

static void hof_mostrar_mejor_temporada(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT " SQL_EXPR_ANIO_FECHA_FLEX " AS anio, "
        "COUNT(*), SUM(goles), SUM(asistencias), ROUND(AVG(rendimiento_general), 2) "
        "FROM partido GROUP BY anio ORDER BY AVG(rendimiento_general) DESC, COUNT(*) DESC LIMIT 1";

    printf("\nMEJOR TEMPORADA (ANIO)\n");
    printf("%s", SEP_MENOR);

    if (!preparar_stmt(&stmt, sql))
        return;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Anio %d | PJ:%d | G:%d | A:%d | Rend:%.2f\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_int(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_double(stmt, 4));
    }
    else
    {
        printf("Sin temporadas para evaluar.\n");
    }

    sqlite3_finalize(stmt);
}

static void hof_mostrar_camiseta_legendaria(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT c.nombre, COUNT(p.id) AS pj, "
        "SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
        "ROUND(AVG(p.rendimiento_general), 2) AS rend "
        "FROM camiseta c "
        "JOIN partido p ON p.camiseta_id = c.id "
        "GROUP BY c.id "
        "HAVING COUNT(p.id) > 0 "
        "ORDER BY (CAST(SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END) AS REAL) / COUNT(p.id)) DESC, rend DESC, pj DESC "
        "LIMIT 1";

    printf("\nCAMISETA LEGENDARIA\n");
    printf("%s", SEP_MENOR);

    if (!preparar_stmt(&stmt, sql))
        return;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 0);
        printf("%s | PJ:%d | Victorias:%d | Rend:%.2f\n",
               camiseta ? camiseta : "N/A",
               sqlite3_column_int(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_double(stmt, 3));
    }
    else
    {
        printf("Sin datos de camisetas.\n");
    }

    sqlite3_finalize(stmt);
}

static void hof_mostrar_records_personales(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT COALESCE(MAX(goles), 0), COALESCE(MAX(asistencias), 0), COALESCE(MAX(rendimiento_general), 0) FROM partido";

    printf("\nRECORDS PERSONALES\n");
    printf("%s", SEP_MENOR);

    if (!preparar_stmt(&stmt, sql))
        return;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Max goles en un partido: %d\n", sqlite3_column_int(stmt, 0));
        printf("Max asistencias en un partido: %d\n", sqlite3_column_int(stmt, 1));
        printf("Max rendimiento en un partido: %d\n", sqlite3_column_int(stmt, 2));
    }

    sqlite3_finalize(stmt);
}

static void hof_mostrar_hitos_registrados(void)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*), COUNT(DISTINCT tipo_hito) FROM carrera_partido_hito";

    printf("\nHITOS REGISTRADOS\n");
    printf("%s", SEP_MENOR);

    if (!preparar_stmt(&stmt, sql))
        return;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Partidos marcados: %d\n", sqlite3_column_int(stmt, 0));
        printf("Tipos de hito usados: %d\n", sqlite3_column_int(stmt, 1));
    }

    sqlite3_finalize(stmt);
}

static void mostrar_hall_of_fame_personal(void)
{
    if (!iniciar_vista_carrera("HALL OF FAME PERSONAL"))
        return;

    hof_mostrar_mejor_partido();
    hof_mostrar_peor_partido();
    hof_mostrar_mejor_temporada();
    hof_mostrar_camiseta_legendaria();
    hof_mostrar_records_personales();
    hof_mostrar_hitos_registrados();

    pause_console();
}

static void generar_resumen_narrativo_automatico(void)
{
    if (!iniciar_vista_carrera("RESUMEN NARRATIVO AUTOMATICO"))
        return;

    PerfilDinamico perfil;
    calcular_perfil_dinamico(&perfil);

    sqlite3_stmt *stmt;
    const char *sql_periodo =
        "SELECT MIN(fecha_hora), MAX(fecha_hora), COUNT(*), "
        "COALESCE(SUM(goles), 0), COALESCE(SUM(asistencias), 0), "
        "COALESCE(SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END), 0) "
        "FROM partido";

    char fecha_inicio[64] = "N/A";
    char fecha_fin[64] = "N/A";
    int partidos = 0;
    int goles = 0;
    int asistencias = 0;
    int victorias = 0;

    if (preparar_stmt(&stmt, sql_periodo))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *f_inicio = (const char *)sqlite3_column_text(stmt, 0);
            const char *f_fin = (const char *)sqlite3_column_text(stmt, 1);
            partidos = sqlite3_column_int(stmt, 2);
            goles = sqlite3_column_int(stmt, 3);
            asistencias = sqlite3_column_int(stmt, 4);
            victorias = sqlite3_column_int(stmt, 5);

            if (f_inicio)
                snprintf(fecha_inicio, sizeof(fecha_inicio), "%s", f_inicio);
            if (f_fin)
                snprintf(fecha_fin, sizeof(fecha_fin), "%s", f_fin);
        }
        sqlite3_finalize(stmt);
    }

    int hitos = 0;
    int lesiones = 0;
    if (preparar_stmt(&stmt, "SELECT COUNT(*) FROM carrera_partido_hito"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            hitos = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (preparar_stmt(&stmt, "SELECT COUNT(*) FROM lesion"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            lesiones = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    const char *momentum_texto = "estable";
    if (perfil.momentum_delta > 0.40)
        momentum_texto = "ascendente";
    else if (perfil.momentum_delta < -0.40)
        momentum_texto = "descendente";

    double winrate = (partidos > 0) ? (double)victorias * 100.0 / (double)partidos : 0.0;

    char resumen[1200];
    int resumen_len = snprintf(resumen, sizeof(resumen),
                               "Entre %s y %s, disputaste %d partidos con %d goles y %d asistencias. "
                               "Tu perfil dinamico actual es %s, con rendimiento promedio %.2f y un momentum %s. "
                               "Acumulas %d hitos personales registrados y %d lesiones historicas. "
                               "Tu porcentaje de victorias es %.1f%%, lo que refleja una etapa de construccion continua en tu carrera.",
                               fecha_inicio,
                               fecha_fin,
                               partidos,
                               goles,
                               asistencias,
                               perfil.etiqueta[0] ? perfil.etiqueta : "Equilibrado",
                               perfil.avg_rendimiento,
                               momentum_texto,
                               hitos,
                               lesiones,
                               winrate);

    if (resumen_len < 0)
    {
        snprintf(resumen, sizeof(resumen), "No se pudo generar el resumen narrativo.");
    }
    else if ((size_t)resumen_len >= sizeof(resumen))
    {
        printf("Aviso: el resumen narrativo fue truncado por longitud.\n");
    }

    printf("Resumen generado:\n");
    printf("%s", SEP_MENOR);
    printf("%s\n", resumen);

    const char *sql_insert =
        "INSERT INTO carrera_resumen_narrativo (fecha, periodo_inicio, periodo_fin, perfil_dinamico, resumen) "
        "VALUES (date('now'), ?, ?, ?, ?)";
    if (preparar_stmt(&stmt, sql_insert))
    {
        sqlite3_bind_text(stmt, 1, fecha_inicio, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, fecha_fin, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, perfil.etiqueta[0] ? perfil.etiqueta : "Equilibrado", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, resumen, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    pause_console();
}

static void listar_resumenes_narrativos(void)
{
    clear_screen();
    print_header("HISTORIAL DE RESUMENES NARRATIVOS");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha, periodo_inicio, periodo_fin, perfil_dinamico, resumen "
        "FROM carrera_resumen_narrativo ORDER BY id DESC LIMIT 10";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudo listar resumenes narrativos.\n");
        pause_console();
        return;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        const char *inicio = (const char *)sqlite3_column_text(stmt, 2);
        const char *fin = (const char *)sqlite3_column_text(stmt, 3);
        const char *perfil = (const char *)sqlite3_column_text(stmt, 4);
        const char *resumen = (const char *)sqlite3_column_text(stmt, 5);

        printf("[%d] Fecha: %s | Periodo: %s -> %s | Perfil: %s\n",
               id,
               fecha ? fecha : "N/A",
               inicio ? inicio : "N/A",
               fin ? fin : "N/A",
               perfil ? perfil : "N/A");
        printf("%s\n\n", resumen ? resumen : "");
        count++;
    }

    if (count == 0)
    {
        printf("No hay resumenes narrativos guardados.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void menu_resumen_narrativo(void)
{
    MenuItem items[] =
    {
        {1, "Generar resumen automatico", &generar_resumen_narrativo_automatico},
        {2, "Ver historial", &listar_resumenes_narrativos},
        {0, "Volver", NULL}
    };

    ejecutar_menu("RESUMEN NARRATIVO", items, 3);
}

/* ========================================================
 * 10. MODO RETRO - HOY EN TU HISTORIA
 * ======================================================== */

#define RETRO_MAX_RECUERDOS 32

typedef enum
{
    RETRO_RECUERDO_PARTIDO = 1,
    RETRO_RECUERDO_HITO = 2
} RetroTipoRecuerdo;

typedef struct
{
    RetroTipoRecuerdo tipo;
    int partido_id;
    int anio_evento;
    int anios_atras;
    char fecha_mostrar[32];
    char titulo[160];
    char linea1[200];
    char linea2[200];
    char linea3[200];
    char linea4[220];
} RetroRecuerdo;

typedef struct
{
    int id;
    char fecha[32];
    char cancha[80];
    char camiseta[80];
    char rival[80];
    int resultado;
    int goles_equipo;
    int goles_rival;
    int goles;
    int asistencias;
    int rendimiento;
    int cansancio;
    int estado_animo;
} RetroComparacionPartido;

static const char *retro_nombre_mes(int mes)
{
    static const char *meses[] =
    {
        "enero", "febrero", "marzo", "abril", "mayo", "junio",
        "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"
    };

    if (mes < 1 || mes > 12)
        return "mes";

    return meses[mes - 1];
}

static void retro_obtener_fecha_actual(int *dia, int *mes, int *anio, int *semana_anio)
{
    time_t ahora = time(NULL);
    struct tm tm_ahora;
    char semana_buffer[8] = "0";

#ifdef _WIN32
    localtime_s(&tm_ahora, &ahora);
#else
    localtime_r(&ahora, &tm_ahora);
#endif

    if (dia)
        *dia = tm_ahora.tm_mday;
    if (mes)
        *mes = tm_ahora.tm_mon + 1;
    if (anio)
        *anio = tm_ahora.tm_year + 1900;

    strftime(semana_buffer, sizeof(semana_buffer), "%W", &tm_ahora);
    if (semana_anio)
        *semana_anio = atoi(semana_buffer);
}

static int retro_parse_2d(const char *src)
{
    if (!src)
        return -1;
    if (src[0] < '0' || src[0] > '9' || src[1] < '0' || src[1] > '9')
        return -1;
    return (src[0] - '0') * 10 + (src[1] - '0');
}

static int retro_parse_4d(const char *src)
{
    int v = 0;
    if (!src)
        return -1;
    for (int i = 0; i < 4; i++)
    {
        if (src[i] < '0' || src[i] > '9')
            return -1;
        v = v * 10 + (src[i] - '0');
    }
    return v;
}

static int retro_parse_fecha(const char *fecha_hora, int *dia, int *mes, int *anio)
{
    if (!fecha_hora || safe_strnlen(fecha_hora, 32) < 10)
        return 0;

    if (fecha_hora[4] == '-' && fecha_hora[7] == '-')
    {
        int y = retro_parse_4d(fecha_hora + 0);
        int m = retro_parse_2d(fecha_hora + 5);
        int d = retro_parse_2d(fecha_hora + 8);
        if (y < 0 || m < 1 || m > 12 || d < 1 || d > 31)
            return 0;

        if (dia)
            *dia = d;
        if (mes)
            *mes = m;
        if (anio)
            *anio = y;
        return 1;
    }

    if (fecha_hora[2] == '/' && fecha_hora[5] == '/')
    {
        int d = retro_parse_2d(fecha_hora + 0);
        int m = retro_parse_2d(fecha_hora + 3);
        int y = retro_parse_4d(fecha_hora + 6);
        if (y < 0 || m < 1 || m > 12 || d < 1 || d > 31)
            return 0;

        if (dia)
            *dia = d;
        if (mes)
            *mes = m;
        if (anio)
            *anio = y;
        return 1;
    }

    return 0;
}

static int retro_semana_del_anio(int dia, int mes, int anio)
{
    struct tm tm_fecha;
    time_t ts;
    char semana_buffer[8] = "0";

    memset(&tm_fecha, 0, sizeof(tm_fecha));
    tm_fecha.tm_mday = dia;
    tm_fecha.tm_mon = mes - 1;
    tm_fecha.tm_year = anio - 1900;

    ts = mktime(&tm_fecha);
    if (ts == (time_t)-1)
        return -1;

    strftime(semana_buffer, sizeof(semana_buffer), "%W", &tm_fecha);
    return atoi(semana_buffer);
}

static void retro_reset_recuerdo(RetroRecuerdo *r)
{
    if (!r)
        return;
    memset(r, 0, sizeof(*r));
}

static int retro_append_hito(RetroRecuerdo *recuerdos, int *count, int max_count,
                             int anio_actual, const char *fecha_raw,
                             const char *titulo, const char *detalle)
{
    int dia_evento = 0;
    int mes_evento = 0;
    int anio_evento = 0;
    RetroRecuerdo *r;

    if (!recuerdos || !count || *count >= max_count || !fecha_raw)
        return 0;

    if (!retro_parse_fecha(fecha_raw, &dia_evento, &mes_evento, &anio_evento))
        return 0;

    if (anio_evento >= anio_actual)
        return 0;

    r = &recuerdos[*count];
    retro_reset_recuerdo(r);
    r->tipo = RETRO_RECUERDO_HITO;
    r->anio_evento = anio_evento;
    r->anios_atras = anio_actual - anio_evento;
    format_date_for_display(fecha_raw, r->fecha_mostrar, (int)sizeof(r->fecha_mostrar));
    snprintf(r->titulo, sizeof(r->titulo), "HITO - %s", titulo ? titulo : "Momento clave");
    snprintf(r->linea1, sizeof(r->linea1), "%s", detalle ? detalle : "");

    (*count)++;
    return 1;
}

static int retro_cargar_partidos_hoy(RetroRecuerdo *recuerdos, int *count, int max_count,
                                     int dia_actual, int mes_actual, int anio_actual)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT p.id, p.fecha_hora, IFNULL(p.goles_equipo, -1), IFNULL(p.goles_rival, -1), "
        "IFNULL(p.goles, 0), IFNULL(p.asistencias, 0), IFNULL(p.rendimiento_general, 0), "
        "IFNULL(p.cansancio, 0), IFNULL(p.resultado, 0), IFNULL(can.nombre, ''), "
        "IFNULL(c.nombre, ''), TRIM(IFNULL(p.rival_nombre, '')), IFNULL(p.comentario_personal, '') "
        "FROM partido p "
        "LEFT JOIN cancha can ON can.id = p.cancha_id "
        "LEFT JOIN camiseta c ON c.id = p.camiseta_id "
        "WHERE " SQL_EXPR_DIA_FECHA " = ? "
        "  AND " SQL_EXPR_MES_FECHA " = ? "
        "  AND " SQL_EXPR_ANIO_FECHA_SEGURO " < ? "
        "ORDER BY " SQL_EXPR_ANIO_FECHA_SEGURO " DESC, p.id DESC "
        "LIMIT 8";

    if (!preparar_stmt(&stmt, sql))
        return 0;

    sqlite3_bind_int(stmt, 1, dia_actual);
    sqlite3_bind_int(stmt, 2, mes_actual);
    sqlite3_bind_int(stmt, 3, anio_actual);

    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max_count)
    {
        RetroRecuerdo *r = &recuerdos[*count];
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
        const char *cancha = (const char *)sqlite3_column_text(stmt, 9);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 10);
        const char *rival = (const char *)sqlite3_column_text(stmt, 11);
        int anio_evento = 0;

        if (!retro_parse_fecha(fecha_raw, NULL, NULL, &anio_evento) || anio_evento >= anio_actual)
            continue;

        retro_reset_recuerdo(r);
        r->tipo = RETRO_RECUERDO_PARTIDO;
        r->partido_id = sqlite3_column_int(stmt, 0);
        r->anio_evento = anio_evento;
        r->anios_atras = anio_actual - anio_evento;
        format_date_for_display(fecha_raw, r->fecha_mostrar, (int)sizeof(r->fecha_mostrar));

        snprintf(r->titulo, sizeof(r->titulo), "HACE %d ANIOS - %s", r->anios_atras, r->fecha_mostrar);

        if (sqlite3_column_int(stmt, 2) >= 0 && sqlite3_column_int(stmt, 3) >= 0 && rival && rival[0] != '\0')
        {
            snprintf(r->linea1, sizeof(r->linea1), "Tu equipo %d - %d %s",
                     sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3), rival);
        }
        else if (rival && rival[0] != '\0')
        {
            snprintf(r->linea1, sizeof(r->linea1), "Vs %s", rival);
        }
        else
        {
            snprintf(r->linea1, sizeof(r->linea1), "Partido registrado en tu historial");
        }

        snprintf(r->linea2, sizeof(r->linea2), "%s | Cancha: %s",
                 resultado_to_text(sqlite3_column_int(stmt, 8)),
                 (cancha && cancha[0] != '\0') ? cancha : "N/A");

        snprintf(r->linea3, sizeof(r->linea3),
                 "Tu actuacion: %d goles | %d asistencias | rendimiento %d/10",
                 sqlite3_column_int(stmt, 4),
                 sqlite3_column_int(stmt, 5),
                 sqlite3_column_int(stmt, 6));

        snprintf(r->linea4, sizeof(r->linea4), "Camiseta: %s | Cansancio: %d/10",
                 (camiseta && camiseta[0] != '\0') ? camiseta : "N/A",
                 sqlite3_column_int(stmt, 7));

        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static void retro_cargar_hito_primer_partido(RetroRecuerdo *recuerdos, int *count, int max_count,
        int dia_actual, int mes_actual, int anio_actual)
{
    sqlite3_stmt *stmt;
    const char *fecha_raw;
    int dia = 0;
    int mes = 0;
    int anio = 0;

    if (!preparar_stmt(&stmt, "SELECT fecha_hora FROM partido ORDER BY fecha_hora ASC, id ASC LIMIT 1"))
        return;

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return;
    }

    fecha_raw = (const char *)sqlite3_column_text(stmt, 0);
    if (retro_parse_fecha(fecha_raw, &dia, &mes, &anio) &&
            dia == dia_actual && mes == mes_actual && anio < anio_actual)
    {
        retro_append_hito(recuerdos, count, max_count, anio_actual, fecha_raw,
                          "Primer partido registrado",
                          "Empezaste a escribir tu historia ese dia.");
    }

    sqlite3_finalize(stmt);
}

static void retro_cargar_hito_primer_gol(RetroRecuerdo *recuerdos, int *count, int max_count,
        int dia_actual, int mes_actual, int anio_actual)
{
    sqlite3_stmt *stmt;
    const char *fecha_raw;
    int dia = 0;
    int mes = 0;
    int anio = 0;

    if (!preparar_stmt(&stmt,
                       "SELECT fecha_hora FROM partido "
                       "WHERE goles > 0 ORDER BY fecha_hora ASC, id ASC LIMIT 1"))
    {
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return;
    }

    fecha_raw = (const char *)sqlite3_column_text(stmt, 0);
    if (retro_parse_fecha(fecha_raw, &dia, &mes, &anio) &&
            dia == dia_actual && mes == mes_actual && anio < anio_actual)
    {
        retro_append_hito(recuerdos, count, max_count, anio_actual, fecha_raw,
                          "Aniversario de tu primer gol",
                          "Ese dia llego tu primer gol registrado en MiFutbolC.");
    }

    sqlite3_finalize(stmt);
}

static void retro_cargar_hito_primer_rival(RetroRecuerdo *recuerdos, int *count, int max_count,
        int dia_actual, int mes_actual, int anio_actual)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT TRIM(IFNULL(rival_nombre, '')) AS rival, MIN(fecha_hora) "
        "FROM partido "
        "WHERE TRIM(IFNULL(rival_nombre, '')) <> '' "
        "GROUP BY TRIM(IFNULL(rival_nombre, '')) "
        "ORDER BY MIN(fecha_hora) ASC";
    int agregados = 0;

    if (!preparar_stmt(&stmt, sql))
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *rival = (const char *)sqlite3_column_text(stmt, 0);
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
        int dia = 0;
        int mes = 0;
        int anio = 0;
        char detalle[180];

        if (*count >= max_count || agregados >= 2)
            break;

        if (!retro_parse_fecha(fecha_raw, &dia, &mes, &anio))
            continue;
        if (anio >= anio_actual || dia != dia_actual || mes != mes_actual)
            continue;

        snprintf(detalle, sizeof(detalle), "Primera vez que jugaste contra %s.",
                 (rival && rival[0] != '\0') ? rival : "ese rival");

        if (retro_append_hito(recuerdos, count, max_count, anio_actual, fecha_raw,
                              "Primer cruce con un rival", detalle))
        {
            agregados++;
        }
    }

    sqlite3_finalize(stmt);
}

static void retro_cargar_hito_primera_camiseta(RetroRecuerdo *recuerdos, int *count, int max_count,
        int dia_actual, int mes_actual, int anio_actual)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT IFNULL(c.nombre, ''), MIN(p.fecha_hora) "
        "FROM partido p "
        "JOIN camiseta c ON c.id = p.camiseta_id "
        "GROUP BY p.camiseta_id, c.nombre "
        "ORDER BY MIN(p.fecha_hora) ASC";
    int agregados = 0;

    if (!preparar_stmt(&stmt, sql))
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 0);
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
        int dia = 0;
        int mes = 0;
        int anio = 0;
        char detalle[180];

        if (*count >= max_count || agregados >= 2)
            break;

        if (!retro_parse_fecha(fecha_raw, &dia, &mes, &anio))
            continue;
        if (anio >= anio_actual || dia != dia_actual || mes != mes_actual)
            continue;

        snprintf(detalle, sizeof(detalle), "Primera vez que usaste la camiseta %s.",
                 (camiseta && camiseta[0] != '\0') ? camiseta : "seleccionada");

        if (retro_append_hito(recuerdos, count, max_count, anio_actual, fecha_raw,
                              "Debut de camiseta", detalle))
        {
            agregados++;
        }
    }

    sqlite3_finalize(stmt);
}

static void retro_cargar_hito_mejor_racha(RetroRecuerdo *recuerdos, int *count, int max_count,
        int semana_actual, int anio_actual)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT fecha_hora, resultado FROM partido ORDER BY fecha_hora ASC, id ASC";
    int cur_len = 0;
    int best_len = 0;
    char cur_inicio[32] = "";
    char cur_fin[32] = "";
    char best_inicio[32] = "";
    char best_fin[32] = "";
    int dia = 0;
    int mes = 0;
    int anio = 0;
    int semana_evento;
    char inicio_fmt[32] = "";
    char fin_fmt[32] = "";
    char detalle[200];

    if (!preparar_stmt(&stmt, sql))
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 0);
        int resultado = sqlite3_column_int(stmt, 1);

        if (resultado == 1)
        {
            if (cur_len == 0)
            {
                snprintf(cur_inicio, sizeof(cur_inicio), "%s", fecha_raw ? fecha_raw : "");
            }
            snprintf(cur_fin, sizeof(cur_fin), "%s", fecha_raw ? fecha_raw : "");
            cur_len++;

            if (cur_len > best_len)
            {
                best_len = cur_len;
                snprintf(best_inicio, sizeof(best_inicio), "%s", cur_inicio);
                snprintf(best_fin, sizeof(best_fin), "%s", cur_fin);
            }
        }
        else
        {
            cur_len = 0;
            cur_inicio[0] = '\0';
            cur_fin[0] = '\0';
        }
    }

    sqlite3_finalize(stmt);

    if (best_len < 3 || best_inicio[0] == '\0')
        return;

    if (!retro_parse_fecha(best_inicio, &dia, &mes, &anio))
        return;
    if (anio >= anio_actual)
        return;

    semana_evento = retro_semana_del_anio(dia, mes, anio);
    if (semana_evento < 0 || semana_evento != semana_actual)
        return;

    format_date_for_display(best_inicio, inicio_fmt, (int)sizeof(inicio_fmt));
    format_date_for_display(best_fin, fin_fmt, (int)sizeof(fin_fmt));
    snprintf(detalle, sizeof(detalle),
             "Esta semana de %d arrancaste tu mejor racha: %d victorias seguidas (%s -> %s).",
             anio, best_len, inicio_fmt, fin_fmt);

    retro_append_hito(recuerdos, count, max_count, anio_actual, best_inicio,
                      "Semana de mejor racha", detalle);
}

static int retro_cargar_recuerdos_hoy(RetroRecuerdo *recuerdos, int max_count,
                                      int dia_actual, int mes_actual, int anio_actual,
                                      int semana_actual)
{
    int count = 0;

    retro_cargar_partidos_hoy(recuerdos, &count, max_count, dia_actual, mes_actual, anio_actual);
    retro_cargar_hito_primer_partido(recuerdos, &count, max_count, dia_actual, mes_actual, anio_actual);
    retro_cargar_hito_primer_gol(recuerdos, &count, max_count, dia_actual, mes_actual, anio_actual);
    retro_cargar_hito_primer_rival(recuerdos, &count, max_count, dia_actual, mes_actual, anio_actual);
    retro_cargar_hito_primera_camiseta(recuerdos, &count, max_count, dia_actual, mes_actual, anio_actual);
    retro_cargar_hito_mejor_racha(recuerdos, &count, max_count, semana_actual, anio_actual);

    return count;
}

static void retro_mostrar_detalle_partido(int partido_id)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT p.id, p.fecha_hora, IFNULL(can.nombre, ''), IFNULL(c.nombre, ''), "
        "TRIM(IFNULL(p.rival_nombre, '')), IFNULL(p.resultado, 0), "
        "IFNULL(p.goles_equipo, -1), IFNULL(p.goles_rival, -1), "
        "IFNULL(p.goles, 0), IFNULL(p.asistencias, 0), IFNULL(p.rendimiento_general, 0), "
        "IFNULL(p.cansancio, 0), IFNULL(p.estado_animo, 0), IFNULL(p.posicion_jugada, ''), "
        "IFNULL(p.minutos_jugados, 0), IFNULL(p.intensidad, 0), IFNULL(p.comentario_personal, ''), "
        "IFNULL(p.lo_mejor, ''), IFNULL(p.que_mejorar, '') "
        "FROM partido p "
        "LEFT JOIN cancha can ON can.id = p.cancha_id "
        "LEFT JOIN camiseta c ON c.id = p.camiseta_id "
        "WHERE p.id = ?";
    char fecha_fmt[32] = "";

    clear_screen();
    print_header("MODO RETRO - DETALLE DEL PARTIDO");

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudo abrir el detalle del partido.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        printf("Partido no encontrado.\n");
        pause_console();
        return;
    }

    format_date_for_display((const char *)sqlite3_column_text(stmt, 1), fecha_fmt, (int)sizeof(fecha_fmt));

    printf("Partido #%d\n", sqlite3_column_int(stmt, 0));
    printf("Fecha: %s\n", fecha_fmt);
    printf("Resultado: %s\n", resultado_to_text(sqlite3_column_int(stmt, 5)));

    if (sqlite3_column_int(stmt, 6) >= 0 && sqlite3_column_int(stmt, 7) >= 0)
    {
        printf("Marcador global: %d - %d\n", sqlite3_column_int(stmt, 6), sqlite3_column_int(stmt, 7));
    }

    printf("Cancha: %s\n", (const char *)sqlite3_column_text(stmt, 2));
    printf("Rival: %s\n", (const char *)sqlite3_column_text(stmt, 4));
    printf("Camiseta: %s\n", (const char *)sqlite3_column_text(stmt, 3));
    printf("Actuacion: %d goles | %d asistencias\n",
           sqlite3_column_int(stmt, 8), sqlite3_column_int(stmt, 9));
    printf("Rendimiento: %d/10 | Cansancio: %d/10 | Estado de animo: %d/10\n",
           sqlite3_column_int(stmt, 10), sqlite3_column_int(stmt, 11), sqlite3_column_int(stmt, 12));
    printf("Posicion: %s | Minutos: %d | Intensidad: %d\n",
           (const char *)sqlite3_column_text(stmt, 13),
           sqlite3_column_int(stmt, 14),
           sqlite3_column_int(stmt, 15));
    printf("Comentario: %s\n", (const char *)sqlite3_column_text(stmt, 16));
    printf("Lo mejor: %s\n", (const char *)sqlite3_column_text(stmt, 17));
    printf("Que mejorar: %s\n", (const char *)sqlite3_column_text(stmt, 18));

    sqlite3_finalize(stmt);
    pause_console();
}

static int retro_cargar_partido_para_comparacion(int partido_id, RetroComparacionPartido *out)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT p.id, p.fecha_hora, IFNULL(can.nombre, ''), IFNULL(c.nombre, ''), "
        "TRIM(IFNULL(p.rival_nombre, '')), IFNULL(p.resultado, 0), "
        "IFNULL(p.goles_equipo, -1), IFNULL(p.goles_rival, -1), "
        "IFNULL(p.goles, 0), IFNULL(p.asistencias, 0), IFNULL(p.rendimiento_general, 0), "
        "IFNULL(p.cansancio, 0), IFNULL(p.estado_animo, 0) "
        "FROM partido p "
        "LEFT JOIN cancha can ON can.id = p.cancha_id "
        "LEFT JOIN camiseta c ON c.id = p.camiseta_id "
        "WHERE p.id = ?";

    if (!out)
        return 0;

    memset(out, 0, sizeof(*out));
    out->id = partido_id;

    if (!preparar_stmt(&stmt, sql))
        return 0;

    sqlite3_bind_int(stmt, 1, partido_id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    out->id = sqlite3_column_int(stmt, 0);
    format_date_for_display((const char *)sqlite3_column_text(stmt, 1), out->fecha, (int)sizeof(out->fecha));
    snprintf(out->cancha, sizeof(out->cancha), "%s", (const char *)sqlite3_column_text(stmt, 2));
    snprintf(out->camiseta, sizeof(out->camiseta), "%s", (const char *)sqlite3_column_text(stmt, 3));
    snprintf(out->rival, sizeof(out->rival), "%s", (const char *)sqlite3_column_text(stmt, 4));
    out->resultado = sqlite3_column_int(stmt, 5);
    out->goles_equipo = sqlite3_column_int(stmt, 6);
    out->goles_rival = sqlite3_column_int(stmt, 7);
    out->goles = sqlite3_column_int(stmt, 8);
    out->asistencias = sqlite3_column_int(stmt, 9);
    out->rendimiento = sqlite3_column_int(stmt, 10);
    out->cansancio = sqlite3_column_int(stmt, 11);
    out->estado_animo = sqlite3_column_int(stmt, 12);

    sqlite3_finalize(stmt);
    return 1;
}

static void retro_comparar_partidos(int partido_a, int partido_b)
{
    RetroComparacionPartido a;
    RetroComparacionPartido b;

    clear_screen();
    print_header("MODO RETRO - COMPARACION DE PARTIDOS");

    if (!retro_cargar_partido_para_comparacion(partido_a, &a) ||
            !retro_cargar_partido_para_comparacion(partido_b, &b))
    {
        printf("No se pudieron cargar ambos partidos para comparar.\n");
        pause_console();
        return;
    }

    printf("A) Partido #%d - %s\n", a.id, a.fecha);
    printf("   Rival: %s | Cancha: %s | Camiseta: %s\n", a.rival, a.cancha, a.camiseta);
    printf("B) Partido #%d - %s\n", b.id, b.fecha);
    printf("   Rival: %s | Cancha: %s | Camiseta: %s\n", b.rival, b.cancha, b.camiseta);
    printf("%s", SEP_MENOR);
    printf("Resultado: %s vs %s\n", resultado_to_text(a.resultado), resultado_to_text(b.resultado));
    printf("Goles: %d vs %d (delta %+d)\n", a.goles, b.goles, b.goles - a.goles);
    printf("Asistencias: %d vs %d (delta %+d)\n", a.asistencias, b.asistencias, b.asistencias - a.asistencias);
    printf("Rendimiento: %d/10 vs %d/10 (delta %+d)\n", a.rendimiento, b.rendimiento, b.rendimiento - a.rendimiento);
    printf("Cansancio: %d/10 vs %d/10 (delta %+d)\n", a.cansancio, b.cansancio, b.cansancio - a.cansancio);
    printf("Estado animo: %d/10 vs %d/10 (delta %+d)\n", a.estado_animo, b.estado_animo, b.estado_animo - a.estado_animo);

    if (a.goles_equipo >= 0 && a.goles_rival >= 0 && b.goles_equipo >= 0 && b.goles_rival >= 0)
    {
        printf("Marcador global: %d-%d vs %d-%d\n",
               a.goles_equipo, a.goles_rival,
               b.goles_equipo, b.goles_rival);
    }

    pause_console();
}

static void retro_ver_recuerdos_mes(int mes_actual, int anio_actual)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT p.id, p.fecha_hora, IFNULL(can.nombre, ''), TRIM(IFNULL(p.rival_nombre, '')), "
        "IFNULL(p.goles, 0), IFNULL(p.asistencias, 0), IFNULL(p.rendimiento_general, 0), "
        "IFNULL(p.resultado, 0), IFNULL(c.nombre, '') "
        "FROM partido p "
        "LEFT JOIN cancha can ON can.id = p.cancha_id "
        "LEFT JOIN camiseta c ON c.id = p.camiseta_id "
        "WHERE " SQL_EXPR_MES_FECHA " = ? "
        "  AND " SQL_EXPR_ANIO_FECHA_SEGURO " < ? "
        "ORDER BY " SQL_EXPR_ANIO_FECHA_SEGURO " DESC, " SQL_EXPR_DIA_FECHA " ASC, p.id ASC";
    int cantidad = 0;

    clear_screen();
    print_header("MODO RETRO - RECUERDOS DEL MES");

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudieron consultar recuerdos del mes.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, mes_actual);
    sqlite3_bind_int(stmt, 2, anio_actual);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int anio_evento = 0;
        char fecha_fmt[32] = "";
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
        const char *rival = (const char *)sqlite3_column_text(stmt, 3);
        const char *cancha = (const char *)sqlite3_column_text(stmt, 2);
        const char *camiseta = (const char *)sqlite3_column_text(stmt, 8);

        if (!retro_parse_fecha(fecha_raw, NULL, NULL, &anio_evento) || anio_evento <= 0 || anio_evento >= anio_actual)
            continue;

        format_date_for_display(fecha_raw, fecha_fmt, (int)sizeof(fecha_fmt));
        printf("[%d] %s (hace %d anios)\n",
               sqlite3_column_int(stmt, 0), fecha_fmt, anio_actual - anio_evento);
        printf("    %s | Rival: %s | Cancha: %s\n",
               resultado_to_text(sqlite3_column_int(stmt, 7)),
               (rival && rival[0] != '\0') ? rival : "N/A",
               (cancha && cancha[0] != '\0') ? cancha : "N/A");
        printf("    Actuacion: %d goles | %d asistencias | rendimiento %d/10\n",
               sqlite3_column_int(stmt, 4), sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
        printf("    Camiseta: %s\n", (camiseta && camiseta[0] != '\0') ? camiseta : "N/A");
        printf("%s", SEP_MENOR);
        cantidad++;
    }

    sqlite3_finalize(stmt);

    if (cantidad == 0)
    {
        printf("No hay recuerdos registrados para %s en anios anteriores.\n",
               retro_nombre_mes(mes_actual));
    }

    pause_console();
}

static void retro_imprimir_recuerdo(const RetroRecuerdo *r)
{
    if (!r)
        return;

    if (r->tipo == RETRO_RECUERDO_PARTIDO)
    {
        printf("> %s\n", r->titulo);
        if (r->linea1[0] != '\0')
            printf("  %s\n", r->linea1);
        if (r->linea2[0] != '\0')
            printf("  %s\n", r->linea2);
        if (r->linea3[0] != '\0')
            printf("  %s\n", r->linea3);
        if (r->linea4[0] != '\0')
            printf("  %s\n", r->linea4);
    }
    else
    {
        printf("> %s - %s (hace %d anios)\n",
               r->titulo,
               r->fecha_mostrar[0] ? r->fecha_mostrar : "fecha N/A",
               r->anios_atras);
        if (r->linea1[0] != '\0')
            printf("  %s\n", r->linea1);
    }
}

static void retro_mostrar_hoy_sin_recuerdos(int dia_actual, int mes_actual, int anio_actual)
{
    clear_screen();
    print_header("MODO RETRO - HOY EN TU HISTORIA");
    printf("%02d/%02d/%04d - buscando en tu historial...\n\n",
           dia_actual, mes_actual, anio_actual);
    printf("Hoy no hay recuerdos para esta fecha.\n");
    printf("Segui cargando partidos y esta seccion se volvera cada vez mas emotiva.\n\n");
    pause_console();
}

static int retro_preparar_lista_menu_hoy(const RetroRecuerdo *recuerdos, int cantidad,
        int idx_partidos[4], int opt_detalles[4])
{
    int cantidad_partidos = 0;

    for (int i = 0; i < 4; i++)
    {
        idx_partidos[i] = -1;
        opt_detalles[i] = 0;
    }

    for (int i = 0; i < cantidad; i++)
    {
        retro_imprimir_recuerdo(&recuerdos[i]);
        printf("%s", SEP_MENOR);

        if (recuerdos[i].tipo == RETRO_RECUERDO_PARTIDO && cantidad_partidos < 4)
            idx_partidos[cantidad_partidos++] = i;
    }

    return cantidad_partidos;
}

static int retro_mostrar_menu_hoy_y_leer_opcion(const RetroRecuerdo *recuerdos,
        const int idx_partidos[4], int cantidad_partidos, int mes_actual,
        int opt_detalles[4], int *opt_comp, int *opt_mes)
{
    int menu_idx = 1;

    if (opt_comp)
        *opt_comp = 0;
    if (opt_mes)
        *opt_mes = 0;

    printf("Que queres hacer?\n");

    for (int i = 0; i < cantidad_partidos; i++)
    {
        opt_detalles[i] = menu_idx++;
        printf("[%d] Ver detalle completo del partido de %d\n",
               opt_detalles[i], recuerdos[idx_partidos[i]].anio_evento);
    }

    if (cantidad_partidos >= 2 && opt_comp)
    {
        *opt_comp = menu_idx++;
        printf("[%d] Comparar los dos partidos mas recientes entre si\n", *opt_comp);
    }

    if (opt_mes)
    {
        *opt_mes = menu_idx++;
        printf("[%d] Ver todos tus recuerdos de %s\n", *opt_mes, retro_nombre_mes(mes_actual));
    }

    printf("[0] Volver\n");
    return input_int("> ");
}

static int retro_manejar_detalle_hoy(int opcion, const RetroRecuerdo *recuerdos,
                                     const int idx_partidos[4], int cantidad_partidos, const int opt_detalles[4])
{
    for (int i = 0; i < cantidad_partidos; i++)
    {
        if (opcion == opt_detalles[i] && idx_partidos[i] >= 0)
        {
            retro_mostrar_detalle_partido(recuerdos[idx_partidos[i]].partido_id);
            return 1;
        }
    }

    return 0;
}

static void mostrar_modo_retro_hoy(void)
{
    RetroRecuerdo recuerdos[RETRO_MAX_RECUERDOS];
    int dia_actual = 0;
    int mes_actual = 0;
    int anio_actual = 0;
    int semana_actual = 0;
    int cantidad;
    int idx_partidos[4] = {-1, -1, -1, -1};
    int opt_detalles[4] = {0, 0, 0, 0};
    int cantidad_partidos = 0;

    retro_obtener_fecha_actual(&dia_actual, &mes_actual, &anio_actual, &semana_actual);
    cantidad = retro_cargar_recuerdos_hoy(recuerdos, RETRO_MAX_RECUERDOS,
                                          dia_actual, mes_actual, anio_actual, semana_actual);

    if (cantidad == 0)
    {
        retro_mostrar_hoy_sin_recuerdos(dia_actual, mes_actual, anio_actual);
        return;
    }

    while (1)
    {
        int opcion;
        int opt_comp = 0;
        int opt_mes = 0;

        clear_screen();
        print_header("MODO RETRO - HOY EN TU HISTORIA");
        printf("%02d/%02d/%04d - buscando en tu historial...\n\n",
               dia_actual, mes_actual, anio_actual);

        cantidad_partidos = retro_preparar_lista_menu_hoy(recuerdos, cantidad, idx_partidos, opt_detalles);
        opcion = retro_mostrar_menu_hoy_y_leer_opcion(recuerdos, idx_partidos, cantidad_partidos,
                 mes_actual, opt_detalles, &opt_comp, &opt_mes);
        if (opcion == 0)
            return;

        if (retro_manejar_detalle_hoy(opcion, recuerdos, idx_partidos, cantidad_partidos, opt_detalles))
            continue;

        if (opcion == opt_comp && idx_partidos[0] >= 0 && idx_partidos[1] >= 0)
        {
            retro_comparar_partidos(recuerdos[idx_partidos[0]].partido_id,
                                    recuerdos[idx_partidos[1]].partido_id);
            continue;
        }

        if (opcion == opt_mes)
        {
            retro_ver_recuerdos_mes(mes_actual, anio_actual);
            continue;
        }

        printf("Opcion invalida.\n");
        pause_console();
    }
}

void carrera_notificar_modo_retro_inicio(void)
{
    RetroRecuerdo recuerdos[RETRO_MAX_RECUERDOS];
    int dia_actual = 0;
    int mes_actual = 0;
    int anio_actual = 0;
    int semana_actual = 0;
    int cantidad;

    retro_obtener_fecha_actual(&dia_actual, &mes_actual, &anio_actual, &semana_actual);
    cantidad = retro_cargar_recuerdos_hoy(recuerdos, RETRO_MAX_RECUERDOS,
                                          dia_actual, mes_actual, anio_actual, semana_actual);

    if (cantidad <= 0)
        return;

    printf("\n[MODO RETRO] Hoy en tu historia hay %d recuerdo(s).\n", cantidad);
    if (confirmar("Queres abrir Modo Retro ahora?"))
    {
        mostrar_modo_retro_hoy();
    }
}

/* ========================================================
 * MENU PRINCIPAL DE CARRERA FUTBOLISTICA
 * ======================================================== */
void menu_carrera_futbolistica(void)
{
    MenuItem items[] =
    {
        {1, "Carrera Futbolistica", &mostrar_carrera_futbolistica},
        {2, "Tu Historia Futbolistica", &mostrar_historia_futbolistica},
        {3, "Resumen General de Carrera", &mostrar_resumen_carrera},
        {4, "Ficha de Identidad del Jugador", &menu_identidad_jugador},
        {5, "Perfil Dinamico de Jugador", &mostrar_perfil_dinamico},
        {6, "Partidos que Marcaron", &menu_partidos_hito},
        {7, "Timeline de Carrera", &mostrar_timeline_carrera},
        {8, "Hall of Fame Personal", &mostrar_hall_of_fame_personal},
        {9, "Resumen Narrativo Automatico", &menu_resumen_narrativo},
        {10, "Modo Retro: Hoy en tu Historia", &mostrar_modo_retro_hoy},
        {0, "Volver", NULL}
    };

    ejecutar_menu("CARRERA FUTBOLISTICA", items, 11);
}
