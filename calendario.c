#include "calendario.h"
#include "db.h"
#include "utils.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, 0) == SQLITE_OK;
}

static int obtener_tiempo_local(time_t instante, struct tm *out_tm)
{
#ifdef _WIN32
    return localtime_s(out_tm, &instante) == 0;
#else
    return localtime_r(&instante, out_tm) != NULL;
#endif
}

static const char *linea_division_eventos(void)
{
    if (consola_soporta_unicode())
    {
        return "────────────────────────────────────────────────────────";
    }
    return "--------------------------------------------------------";
}

static const char *simbolo_partido_calendario(void)
{
    if (consola_soporta_unicode())
    {
        return "⚽";
    }
    return "P";
}

static int dias_en_mes(int mes, int anio)
{
    int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Año bisiesto
    if (mes == 2 && ((anio % 4 == 0 && anio % 100 != 0) || (anio % 400 == 0)))
    {
        return 29;
    }

    return dias[mes - 1];
}

static int primer_dia_semana(int mes, int anio)
{
    struct tm fecha = {0};
    fecha.tm_mday = 1;
    fecha.tm_mon = mes - 1;
    fecha.tm_year = anio - 1900;

    mktime(&fecha);

    return fecha.tm_wday;
}

static void obtener_icono_dia(int dia, int mes, int anio, char *icono, size_t tam)
{
    char fecha[11];
    snprintf(fecha, sizeof(fecha), "%04d-%02d-%02d", anio, mes, dia);

    sqlite3_stmt *stmt;
    int tiene_partido = 0;
    int tiene_recordatorio = 0;
    int tiene_finanza = 0;

    // Verificar partidos
    const char *sql_partido = "SELECT COUNT(*) FROM partido WHERE fecha = ?;";
    if (preparar_stmt(sql_partido, &stmt))
    {
        sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            tiene_partido = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
    }

    // Verificar recordatorios
    const char *sql_recordatorio = "SELECT COUNT(*) FROM recordatorios WHERE fecha = ?;";
    if (preparar_stmt(sql_recordatorio, &stmt))
    {
        sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            tiene_recordatorio = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
    }

    // Verificar finanzas
    const char *sql_finanza = "SELECT COUNT(*) FROM finanzas WHERE fecha = ?;";
    if (preparar_stmt(sql_finanza, &stmt))
    {
        sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            tiene_finanza = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
    }

    // Asignar icono según prioridad
    if (tiene_partido)
    {
        snprintf(icono, tam, "%s", simbolo_partido_calendario());
    }
    else if (tiene_recordatorio)
    {
        snprintf(icono, tam, "!");
    }
    else if (tiene_finanza)
    {
        snprintf(icono, tam, "$");
    }
    else
    {
        snprintf(icono, tam, " ");
    }
}

static int mostrar_eventos_partidos(const char *fecha)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_partidos =
        "SELECT cancha, hora, goles, asistencias FROM partido WHERE fecha = ?;";
    int eventos = 0;

    if (!preparar_stmt(sql_partidos, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_STATIC);

    int tiene_partidos = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!tiene_partidos)
        {
            printf("%s PARTIDOS:\n", consola_soporta_unicode() ? "⚽" : "P");
            printf("%s\n", linea_division_eventos());
            tiene_partidos = 1;
        }

        const unsigned char *cancha = sqlite3_column_text(stmt, 0);
        const unsigned char *hora = sqlite3_column_text(stmt, 1);
        int goles = sqlite3_column_int(stmt, 2);
        int asistencias = sqlite3_column_int(stmt, 3);

        printf("  %s - %s | Goles: %d, Asistencias: %d\n",
               hora, cancha, goles, asistencias);
        eventos++;
    }

    if (tiene_partidos)
    {
        printf("\n");
    }

    sqlite3_finalize(stmt);
    return eventos;
}

static int mostrar_eventos_recordatorios(const char *fecha)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_recordatorios =
        "SELECT hora, descripcion FROM recordatorios WHERE fecha = ?;";
    int eventos = 0;

    if (!preparar_stmt(sql_recordatorios, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_STATIC);

    int tiene_recordatorios = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!tiene_recordatorios)
        {
            printf("%s RECORDATORIOS:\n", consola_soporta_unicode() ? "⏰" : "R");
            printf("%s\n", linea_division_eventos());
            tiene_recordatorios = 1;
        }

        const unsigned char *hora = sqlite3_column_text(stmt, 0);
        const unsigned char *desc = sqlite3_column_text(stmt, 1);

        printf("  %s - %s\n", hora, desc);
        eventos++;
    }

    if (tiene_recordatorios)
    {
        printf("\n");
    }

    sqlite3_finalize(stmt);
    return eventos;
}

static int mostrar_eventos_finanzas(const char *fecha)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_finanzas =
        "SELECT tipo, monto, descripcion FROM finanzas WHERE fecha = ?;";
    int eventos = 0;

    if (!preparar_stmt(sql_finanzas, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_STATIC);

    int tiene_finanzas = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!tiene_finanzas)
        {
            printf("%s FINANZAS:\n", consola_soporta_unicode() ? "💰" : "$ ");
            printf("%s\n", linea_division_eventos());
            tiene_finanzas = 1;
        }

        int tipo = sqlite3_column_int(stmt, 0);
        int monto = sqlite3_column_int(stmt, 1);
        const unsigned char *desc = sqlite3_column_text(stmt, 2);

        printf("  %s $%d - %s\n",
               tipo == 0 ? "INGRESO" : "GASTO", monto, desc);
        eventos++;
    }

    if (tiene_finanzas)
    {
        printf("\n");
    }

    sqlite3_finalize(stmt);
    return eventos;
}

static void imprimir_encabezado_calendario_mes(int usar_unicode,
        const char *nombre_mes,
        int anio)
{
    if (usar_unicode)
    {
        printf("╔══════════════════════════════════════════════════════════════╗\n");
        printf("║              %s %d%-35s║\n", nombre_mes, anio, "");
        printf("╠══════════════════════════════════════════════════════════════╣\n");
        printf("║   L   M   M   J   V   S   D                                  ║\n");
        printf("╟──────────────────────────────────────────────────────────────╢\n");
        return;
    }

    printf("+--------------------------------------------------------------+\n");
    printf("|              %s %d%-35s|\n", nombre_mes, anio, "");
    printf("+--------------------------------------------------------------+\n");
    printf("|   L   M   M   J   V   S   D                                  |\n");
    printf("|--------------------------------------------------------------|\n");
}

static int es_dia_hoy(int dia, int mes, int anio, int hay_hoy, const struct tm *tm_hoy)
{
    return (hay_hoy &&
            dia == tm_hoy->tm_mday &&
            mes == tm_hoy->tm_mon + 1 &&
            anio == tm_hoy->tm_year + 1900);
}

static void imprimir_celda_dia(int dia, const char *icono, int es_hoy)
{
    if (es_hoy)
    {
        printf(" [%2d%s]", dia, icono);
    }
    else
    {
        printf("  %2d%s ", dia, icono);
    }
}

static void imprimir_salto_semana_si_corresponde(int pos, int dia_actual, int dias, int usar_unicode)
{
    if (pos % 7 == 0 && dia_actual <= dias)
    {
        printf(" %s\n%s", usar_unicode ? "║" : "|", usar_unicode ? "║" : "|");
    }
}

static void completar_ultima_semana(int *pos)
{
    while (*pos % 7 != 0)
    {
        printf("     ");
        (*pos)++;
    }
}

static void imprimir_pie_calendario_mes(int usar_unicode)
{
    if (usar_unicode)
    {
        printf("╚══════════════════════════════════════════════════════════════╝\n");
    }
    else
    {
        printf("+--------------------------------------------------------------+\n");
    }
}

void mostrar_calendario_mes(int mes, int anio)
{
    clear_screen();
    int usar_unicode = consola_soporta_unicode();

    const char *nombres_meses[] =
    {
        "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
        "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
    };

    printf("\n");
    imprimir_encabezado_calendario_mes(usar_unicode, nombres_meses[mes - 1], anio);

    int dias = dias_en_mes(mes, anio);
    int primer_dia = primer_dia_semana(mes, anio);

    // Ajustar para que Lunes sea primer día (en lugar de Domingo)
    int dia_inicio = (primer_dia == 0) ? 6 : primer_dia - 1;

    int dia_actual = 1;
    int pos = 0;
    time_t ahora = time(NULL);
    struct tm tm_hoy = {0};
    int hay_hoy = obtener_tiempo_local(ahora, &tm_hoy);

    printf("%s", usar_unicode ? "║" : "|");

    // Espacios iniciales
    for (int i = 0; i < dia_inicio; i++)
    {
        printf("     ");
        pos++;
    }

    // Días del mes
    while (dia_actual <= dias)
    {
        char icono[4] = " ";
        obtener_icono_dia(dia_actual, mes, anio, icono, sizeof(icono));

        int hoy = es_dia_hoy(dia_actual, mes, anio, hay_hoy, &tm_hoy);
        imprimir_celda_dia(dia_actual, icono, hoy);

        pos++;
        dia_actual++;

        // Nueva linea cada 7 dias
        imprimir_salto_semana_si_corresponde(pos, dia_actual, dias, usar_unicode);
    }

    // Completar ultima linea
    completar_ultima_semana(&pos);

    printf(" %s\n", usar_unicode ? "║" : "|");
    imprimir_pie_calendario_mes(usar_unicode);
    printf("\nLeyenda: [Hoy]  %s=Partido  !=Recordatorio  $=Finanza\n", simbolo_partido_calendario());
    printf("\n");
}

void mostrar_eventos_dia(int dia, int mes, int anio)
{
    clear_screen();
    int usar_unicode = consola_soporta_unicode();

    char fecha[11];
    snprintf(fecha, sizeof(fecha), "%04d-%02d-%02d", anio, mes, dia);

    printf("\n");
    if (usar_unicode)
    {
        printf("╔══════════════════════════════════════════════════════════════╗\n");
        printf("║              Eventos del dia %02d/%02d/%04d%-19s║\n", dia, mes, anio, "");
        printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    }
    else
    {
        printf("+--------------------------------------------------------------+\n");
        printf("|              Eventos del dia %02d/%02d/%04d%-19s|\n", dia, mes, anio, "");
        printf("+--------------------------------------------------------------+\n\n");
    }

    int eventos_totales = 0;
    eventos_totales += mostrar_eventos_partidos(fecha);
    eventos_totales += mostrar_eventos_recordatorios(fecha);
    eventos_totales += mostrar_eventos_finanzas(fecha);

    if (eventos_totales == 0)
    {
        printf("  (No hay eventos registrados para este dia)\n\n");
    }

    pause_console();
}

void mostrar_calendario()
{
    time_t ahora = time(NULL);
    struct tm tm_info = {0};
    if (!obtener_tiempo_local(ahora, &tm_info))
    {
        return;
    }

    mostrar_calendario_mes(tm_info.tm_mon + 1, tm_info.tm_year + 1900);
    pause_console();
}

static void avanzar_mes(int *mes_actual, int *anio_actual)
{
    (*mes_actual)++;
    if (*mes_actual > 12)
    {
        *mes_actual = 1;
        (*anio_actual)++;
    }
}

static void retroceder_mes(int *mes_actual, int *anio_actual)
{
    (*mes_actual)--;
    if (*mes_actual < 1)
    {
        *mes_actual = 12;
        (*anio_actual)--;
    }
}

static void volver_a_hoy(int *mes_actual, int *anio_actual)
{
    time_t hoy = time(NULL);
    struct tm tm_hoy_buf;
    if (obtener_tiempo_local(hoy, &tm_hoy_buf))
    {
        *mes_actual = tm_hoy_buf.tm_mon + 1;
        *anio_actual = tm_hoy_buf.tm_year + 1900;
    }
}

static void ver_eventos_mes_actual(int mes_actual, int anio_actual)
{
    printf("\nIngrese dia (1-%d): ", dias_en_mes(mes_actual, anio_actual));
    int dia = input_int("");

    if (dia >= 1 && dia <= dias_en_mes(mes_actual, anio_actual))
    {
        mostrar_eventos_dia(dia, mes_actual, anio_actual);
    }
    else
    {
        printf("\nDia invalido.\n");
        pause_console();
    }
}

void menu_calendario()
{
    time_t ahora = time(NULL);
    struct tm tm_info = {0};
    if (!obtener_tiempo_local(ahora, &tm_info))
    {
        return;
    }

    int mes_actual = tm_info.tm_mon + 1;
    int anio_actual = tm_info.tm_year + 1900;
    int salir = 0;

    while (!salir)
    {
        mostrar_calendario_mes(mes_actual, anio_actual);

        printf("Opciones:\n");
        printf("  [N] Mes siguiente\n");
        printf("  [P] Mes anterior\n");
        printf("  [V] Ver eventos de un dia\n");
        printf("  [H] Volver a hoy\n");
        printf("  [0] Volver\n\n");
        printf("Seleccione: ");

        char opcion[10];
        if (!fgets(opcion, sizeof(opcion), stdin))
        {
            salir = 1;
            continue;
        }

        opcion[strcspn(opcion, "\n")] = '\0';

        switch (opcion[0])
        {
        case '0':
            salir = 1;
            break;
        case 'N':
        case 'n':
            avanzar_mes(&mes_actual, &anio_actual);
            break;
        case 'P':
        case 'p':
            retroceder_mes(&mes_actual, &anio_actual);
            break;
        case 'H':
        case 'h':
            volver_a_hoy(&mes_actual, &anio_actual);
            break;
        case 'V':
        case 'v':
            ver_eventos_mes_actual(mes_actual, anio_actual);
            break;
        default:
            break;
        }
    }
}
