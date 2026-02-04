/**
 * @file analisis.c
 * @brief Módulo para el análisis de rendimiento en partidos de fútbol.
 *
 * Este archivo contiene funciones para analizar el rendimiento comparando
 * los últimos 5 partidos con promedios generales, y calculando rachas.
 */

#include "analisis.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "settings.h"
#include "entrenador_ia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static int preparar_stmt_con_mensaje(sqlite3_stmt **stmt, const char *sql)
{
    if (preparar_stmt(stmt, sql))
    {
        return 1;
    }

    printf("Error al consultar la base de datos.\n");
    return 0;
}

static int existe_id_entidad(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s WHERE id = ? LIMIT 1", tabla);

    if (!preparar_stmt(&stmt, sql))
        return 0;

    sqlite3_bind_int(stmt, 1, id);
    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

static void solicitar_fecha_yyyy_mm_dd(const char *prompt, char *buffer, int size)
{
    input_date(prompt, buffer, size);
}

/**
 * @brief Calcula estadísticas generales de todos los partidos
 *
 * Establece línea base de rendimiento histórico para comparaciones.
 *
 * @param stats Puntero a la estructura donde almacenar las estadísticas
 */
static void calcular_estadisticas_generales(Estadisticas *stats)
{
    calcular_estadisticas(stats,
                          "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM partido");
}

/**
 * @brief Calcula estadísticas de los últimos 5 partidos
 *
 * @param stats Puntero a la estructura donde almacenar las estadísticas
 */
static void calcular_estadisticas_ultimos5(Estadisticas *stats)
{
    calcular_estadisticas(stats,
                          "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM (SELECT * FROM partido ORDER BY fecha_hora DESC LIMIT 5)");
}

/**
 * @brief Calcula la racha más larga de victorias y derrotas
 *
 * @param mejor_racha_victorias Puntero donde almacenar la mejor racha de victorias
 * @param peor_racha_derrotas Puntero donde almacenar la peor racha de derrotas
 */
static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT resultado FROM partido ORDER BY fecha_hora",
                       -1, &stmt, NULL);

    int racha_actual_v = 0;
    int max_racha_v = 0;
    int racha_actual_d = 0;
    int max_racha_d = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        actualizar_rachas(resultado, &racha_actual_v, &max_racha_v,
                          &racha_actual_d, &max_racha_d);
    }

    *mejor_racha_victorias = max_racha_v;
    *peor_racha_derrotas = max_racha_d;
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra los últimos 5 partidos
 *
 * Facilita la visualización rápida del rendimiento reciente para contextualizar
 * las estadísticas comparativas.
 */
static void mostrar_ultimos5_partidos()
{
    printf("\nULTIMOS 5 PARTIDOS:\n");
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "SELECT id, fecha_hora, goles, asistencias, rendimiento_general, resultado "
                       "FROM partido ORDER BY id DESC LIMIT 5"))
    {
        return;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        int goles = sqlite3_column_int(stmt, 2);
        int asistencias = sqlite3_column_int(stmt, 3);
        int rendimiento = sqlite3_column_int(stmt, 4);
        int resultado = sqlite3_column_int(stmt, 5);

        printf("%d | %s | G:%d A:%d | Rend:%d | %s\n",
               id, fecha, goles, asistencias, rendimiento, resultado_to_text(resultado));
        count++;
    }

    if (count == 0)
    {
        printf("No hay partidos registrados.\n");
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra comparación de estadísticas últimos 5 vs general
 *
 * Permite identificar tendencias recientes respecto al rendimiento histórico
 * para tomar decisiones de mejora.
 *
 * @param ultimos Estadísticas de últimos 5 partidos
 * @param generales Estadísticas generales
 */
static void mostrar_comparacion_estadisticas(const Estadisticas *ultimos, const Estadisticas *generales)
{
    printf("\nCOMPARACION ULTIMOS 5 VS PROMEDIO GENERAL:\n");
    printf("----------------------------------------\n");
    printf("Goles:        %.1f vs %.1f\n", ultimos->avg_goles, generales->avg_goles);
    printf("Asistencias:  %.1f vs %.1f\n", ultimos->avg_asistencias, generales->avg_asistencias);
    printf("Rendimiento:  %.1f vs %.1f\n", ultimos->avg_rendimiento, generales->avg_rendimiento);
    printf("Cansancio:    %.1f vs %.1f\n", ultimos->avg_cansancio, generales->avg_cansancio);
    printf("Estado Animo: %.1f vs %.1f\n", ultimos->avg_animo, generales->avg_animo);
}

/**
 * @brief Muestra rachas de victorias y derrotas
 *
 * Ayuda a entender patrones de consistencia en el rendimiento competitivo.
 *
 * @param mejor_racha_v Mejor racha de victorias
 * @param peor_racha_d Peor racha de derrotas
 */
static void mostrar_rachas(int mejor_racha_v, int peor_racha_d)
{
    printf("\nRACHAS:\n");
    printf("----------------------------------------\n");
    printf("Mejor racha de victorias: %d partidos\n", mejor_racha_v);
    printf("Peor racha de derrotas: %d partidos\n", peor_racha_d);
}

/**
 * @brief Genera un mensaje motivacional basado en el rendimiento
 *
 * Proporciona retroalimentación psicológica para mantener la motivación
 * y enfoque en el desarrollo deportivo.
 *
 * @param ultimos Puntero a estadísticas de últimos 5 partidos
 * @param generales Puntero a estadísticas generales
 */
static void mensaje_motivacional(const Estadisticas *ultimos, const Estadisticas *generales)
{
    printf("\nANALISIS MOTIVACIONAL:\n");
    printf("----------------------------------------\n");

    double diff_goles = ultimos->avg_goles - generales->avg_goles;
    double diff_rendimiento = ultimos->avg_rendimiento - generales->avg_rendimiento;

    if (diff_goles > 0.5 && diff_rendimiento > 0.5)
    {
        printf("Excelente Estas en racha ascendente. Sigue asi, tu esfuerzo está dando frutos.\n");
        printf("Manten la consistencia y continua trabajando duro en los entrenamientos.\n");
    }
    else if (diff_goles < -0.5 || diff_rendimiento < -0.5)
    {
        printf("No te desanimes. Todos tenemos dias dificiles. Analiza que puedes mejorar:\n");
        printf("- Revisa tu preparación física y tecnica.\n");
        printf("- Habla con tu entrenador sobre estrategias.\n");
        printf("- Recuerda: el fútbol es un deporte de perseverancia.\n");
    }
    else
    {
        printf("Buen trabajo manteniendo el nivel. La consistencia es clave en el futbol.\n");
        printf("Sigue entrenando y manten la motivacion alta. Cada partido es una oportunidad!\n");
    }
}

/**
 * @brief Muestra el análisis básico de rendimiento
 */
static void mostrar_analisis_basico()
{
    clear_screen();
    print_header("ANALISIS DE RENDIMIENTO");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    if (generales.total_partidos == 0)
    {
        printf("No hay suficientes datos para realizar el analisis.\n");
        printf("Registra al menos algunos partidos para ver estadisticas.\n");
        pause_console();
        return;
    }

    mostrar_ultimos5_partidos();
    mostrar_comparacion_estadisticas(&ultimos5, &generales);
    mostrar_rachas(mejor_racha_v, peor_racha_d);
    mensaje_motivacional(&ultimos5, &generales);

    pause_console();
}

/**
 * @brief Estructura para métricas de comparación
 */
typedef struct
{
    double goles;
    double asistencias;
    double rendimiento;
    int partidos;
} MetricasComparacion;

/**
 * @brief Determina el ganador basado en la diferencia
 *
 * @param diff Diferencia entre métricas
 * @param nombre1 Nombre del primer elemento
 * @param nombre2 Nombre del segundo elemento
 * @return Nombre del ganador o "Empate"
 */
static const char *determinar_ganador(double diff, const char *nombre1, const char *nombre2)
{
    if (diff > 0)
        return nombre1;
    if (diff < 0)
        return nombre2;
    return "Empate";
}

/**
 * @brief Calcula métricas para una condición específica
 */
static void calcular_metricas_por_condicion(MetricasComparacion *metricas, const char *condicion_sql)
{
    sqlite3_stmt *stmt;
    char sql[512];

    memset(metricas, 0, sizeof(*metricas));

    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general) "
             "FROM partido WHERE %s", condicion_sql);

    if (!preparar_stmt(&stmt, sql))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        metricas->partidos = sqlite3_column_int(stmt, 0);
        metricas->goles = sqlite3_column_double(stmt, 1);
        metricas->asistencias = sqlite3_column_double(stmt, 2);
        metricas->rendimiento = sqlite3_column_double(stmt, 3);
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra comparación entre dos métricas
 */
static void mostrar_comparacion_dos_metricas(const MetricasComparacion *m1, const MetricasComparacion *m2,
        const char *nombre1, const char *nombre2)
{
    printf("\nCOMPARACION: %s vs %s\n", nombre1, nombre2);
    printf("----------------------------------------\n");

    if (m1->partidos == 0 && m2->partidos == 0)
    {
        printf("No hay datos suficientes para comparar.\n");
        return;
    }

    printf("%-15s %-15s %-15s %-15s %-15s\n", "Metrica", nombre1, nombre2, "Diferencia", "Porcentaje");
    printf("----------------------------------------\n");

    // Goles
    double diff_goles = m1->goles - m2->goles;
    double pct_goles = (m2->goles != 0) ? (diff_goles / m2->goles) * 100 : 0;
    printf("%-15s %-15.2f %-15.2f %-15.2f %-15.1f%%\n", "Goles", m1->goles, m2->goles, diff_goles, pct_goles);

    // Asistencias
    double diff_asist = m1->asistencias - m2->asistencias;
    double pct_asist = (m2->asistencias != 0) ? (diff_asist / m2->asistencias) * 100 : 0;
    printf("%-15s %-15.2f %-15.2f %-15.2f %-15.1f%%\n", "Asistencias", m1->asistencias, m2->asistencias, diff_asist, pct_asist);

    // Rendimiento
    double diff_rend = m1->rendimiento - m2->rendimiento;
    double pct_rend = (m2->rendimiento != 0) ? (diff_rend / m2->rendimiento) * 100 : 0;
    printf("%-15s %-15.2f %-15.2f %-15.2f %-15.1f%%\n", "Rendimiento", m1->rendimiento, m2->rendimiento, diff_rend, pct_rend);

    printf("----------------------------------------\n");

    // Determinar ganadores por métrica
    printf("GANADORES POR METRICA:\n");
    printf("  Goles: %s\n", determinar_ganador(diff_goles, nombre1, nombre2));
    printf("  Asistencias: %s\n", determinar_ganador(diff_asist, nombre1, nombre2));
    printf("  Rendimiento: %s\n", determinar_ganador(diff_rend, nombre1, nombre2));
}

/**
 * @brief Función auxiliar para listar entidades y obtener dos IDs
 * Centraliza lógica repetida en comparadores para evitar duplicación
 *
 * @param tabla Nombre de la tabla a consultar
 * @param titulo Título a mostrar
 * @param id1 Puntero para almacenar primer ID
 * @param id2 Puntero para almacenar segundo ID
 * @return 1 si se obtuvieron dos IDs válidos, 0 si hay menos de 2 entidades
 */
static int listar_y_seleccionar_dos_entidades(const char *tabla, const char *titulo, int *id1, int *id2)
{
    sqlite3_stmt *stmt;

    // Nota: Reemplazar %s en la consulta manualmente
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, nombre FROM %s ORDER BY id", tabla);

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    printf("%s disponibles:\n", titulo);
    printf("----------------------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        printf("%d. %s\n", id, nombre);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count < 2)
    {
        printf("Se necesitan al menos 2 %s para comparar.\n", tabla);
        pause_console();
        return 0;
    }

    while (1)
    {
        *id1 = input_int("\nIngrese ID de la primera entidad (0 para cancelar): ");
        if (*id1 == 0)
            return 0;
        if (!existe_id_entidad(tabla, *id1))
        {
            printf("ID inválido.\n");
            continue;
        }

        *id2 = input_int("Ingrese ID de la segunda entidad (0 para cancelar): ");
        if (*id2 == 0)
            return 0;
        if (!existe_id_entidad(tabla, *id2))
        {
            printf("ID inválido.\n");
            continue;
        }
        if (*id1 == *id2)
        {
            printf("Los IDs deben ser diferentes.\n");
            continue;
        }
        return 1;
    }
}

/**
 * @brief Compara dos camisetas
 */
static void comparar_camisetas()
{
    clear_screen();
    print_header("COMPARADOR: CAMISETAS");

    int id1;
    int id2;
    if (!listar_y_seleccionar_dos_entidades("camiseta", "Camiseta", &id1, &id2))
        return;

    char sql1[128];
    char sql2[128];
    snprintf(sql1, sizeof(sql1), "camiseta_id = %d", id1);
    snprintf(sql2, sizeof(sql2), "camiseta_id = %d", id2);

    MetricasComparacion m1 = {0};
    MetricasComparacion m2 = {0};
    calcular_metricas_por_condicion(&m1, sql1);
    calcular_metricas_por_condicion(&m2, sql2);

    char nombre1[256] = "Camiseta A";
    char nombre2[256] = "Camiseta B";

    obtener_nombre_entidad("camiseta", id1, nombre1, sizeof(nombre1));
    obtener_nombre_entidad("camiseta", id2, nombre2, sizeof(nombre2));

    mostrar_comparacion_dos_metricas(&m1, &m2, nombre1, nombre2);
    pause_console();
}

/**
 * @brief Compara dos torneos
 */
static void comparar_torneos()
{
    clear_screen();
    print_header("COMPARADOR: TORNEOS");

    int id1;
    int id2;
    if (!listar_y_seleccionar_dos_entidades("torneo", "Torneo", &id1, &id2))
        return;

    char sql1[128];
    char sql2[128];
    snprintf(sql1, sizeof(sql1), "id IN (SELECT partido_id FROM partido_torneo WHERE torneo_id = %d)", id1);
    snprintf(sql2, sizeof(sql2), "id IN (SELECT partido_id FROM partido_torneo WHERE torneo_id = %d)", id2);

    MetricasComparacion m1 = {0};
    MetricasComparacion m2 = {0};
    calcular_metricas_por_condicion(&m1, sql1);
    calcular_metricas_por_condicion(&m2, sql2);

    char nombre1[256] = "Torneo A";
    char nombre2[256] = "Torneo B";

    obtener_nombre_entidad("torneo", id1, nombre1, sizeof(nombre1));
    obtener_nombre_entidad("torneo", id2, nombre2, sizeof(nombre2));

    mostrar_comparacion_dos_metricas(&m1, &m2, nombre1, nombre2);
    pause_console();
}

/**
 * @brief Compara dos períodos
 */
static void comparar_periodos()
{
    clear_screen();
    print_header("COMPARADOR: PERIODOS");

    printf("Formatos de fecha: DD/MM/AAAA\n");
    printf("Ejemplo: 2024-01-01 al 2024-06-30\n\n");

    char fecha1_inicio[20];
    char fecha1_fin[20];
    char fecha2_inicio[20];
    char fecha2_fin[20];

    printf("PRIMER PERIODO:\n");
    solicitar_fecha_yyyy_mm_dd("Fecha inicio (DD/MM/AAAA, Enter=hoy): ", fecha1_inicio, sizeof(fecha1_inicio));
    solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha1_fin, sizeof(fecha1_fin));
    while (strcmp(fecha1_fin, fecha1_inicio) < 0)
    {
        printf("La fecha de fin no puede ser anterior a la de inicio.\n");
        solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha1_fin, sizeof(fecha1_fin));
    }

    printf("\nSEGUNDO PERIODO:\n");
    solicitar_fecha_yyyy_mm_dd("Fecha inicio (DD/MM/AAAA, Enter=hoy): ", fecha2_inicio, sizeof(fecha2_inicio));
    solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha2_fin, sizeof(fecha2_fin));
    while (strcmp(fecha2_fin, fecha2_inicio) < 0)
    {
        printf("La fecha de fin no puede ser anterior a la de inicio.\n");
        solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha2_fin, sizeof(fecha2_fin));
    }

    char sql1[256];
    char sql2[256];
    snprintf(sql1, sizeof(sql1), "fecha_hora BETWEEN '%s' AND '%s'", fecha1_inicio, fecha1_fin);
    snprintf(sql2, sizeof(sql2), "fecha_hora BETWEEN '%s' AND '%s'", fecha2_inicio, fecha2_fin);

    MetricasComparacion m1 = {0};
    MetricasComparacion m2 = {0};
    calcular_metricas_por_condicion(&m1, sql1);
    calcular_metricas_por_condicion(&m2, sql2);

    char nombre1[256];
    char nombre2[256];
    snprintf(nombre1, sizeof(nombre1), "Periodo %s a %s", fecha1_inicio, fecha1_fin);
    snprintf(nombre2, sizeof(nombre2), "Periodo %s a %s", fecha2_inicio, fecha2_fin);

    mostrar_comparacion_dos_metricas(&m1, &m2, nombre1, nombre2);
    pause_console();
}

/**
 * @brief Compara dos condiciones
 */
static void comparar_condiciones()
{
    clear_screen();
    print_header("COMPARADOR: CONDICIONES");

    printf("Tipos de condicion:\n");
    printf("1. Clima (0=Soleado, 1=Lluvia, 2=Nublado)\n");
    printf("2. Dia de la semana (0=Lunes, 1=Martes, ..., 6=Domingo)\n");

    int tipo_condicion = 0;
    while (tipo_condicion < 1 || tipo_condicion > 2)
    {
        tipo_condicion = input_int("\nSeleccione tipo de condicion (1-2): ");
        if (tipo_condicion < 1 || tipo_condicion > 2)
            printf("Opción inválida.\n");
    }

    int valor1;
    int valor2;
    const char *campo = (tipo_condicion == 1) ? "clima" : "dia";
    const char *tipo_texto = (tipo_condicion == 1) ? "Clima" : "Dia";

    int min_val = 0;
    int max_val = (tipo_condicion == 1) ? 2 : 6;

    while (1)
    {
        valor1 = input_int("\nIngrese primer valor: ");
        valor2 = input_int("Ingrese segundo valor: ");

        if (valor1 < min_val || valor1 > max_val || valor2 < min_val || valor2 > max_val)
        {
            printf("Valores inválidos. Rango permitido: %d a %d.\n", min_val, max_val);
            continue;
        }
        if (valor1 == valor2)
        {
            printf("Los valores deben ser diferentes.\n");
            continue;
        }
        break;
    }

    char sql1[128];
    char sql2[128];
    snprintf(sql1, sizeof(sql1), "%s = %d", campo, valor1);
    snprintf(sql2, sizeof(sql2), "%s = %d", campo, valor2);

    MetricasComparacion m1 = {0};
    MetricasComparacion m2 = {0};
    calcular_metricas_por_condicion(&m1, sql1);
    calcular_metricas_por_condicion(&m2, sql2);

    char nombre1[256];
    char nombre2[256];
    snprintf(nombre1, sizeof(nombre1), "%s %d", tipo_texto, valor1);
    snprintf(nombre2, sizeof(nombre2), "%s %d", tipo_texto, valor2);

    mostrar_comparacion_dos_metricas(&m1, &m2, nombre1, nombre2);
    pause_console();
}

/**
 * @brief Muestra el menú del comparador avanzado
 */
static void mostrar_comparador_avanzado()
{
    clear_screen();
    print_header("COMPARADOR AVANZADO");

    MenuItem items[] =
    {
        {1, "Comparar Camisetas", comparar_camisetas},
        {2, "Comparar Torneos", comparar_torneos},
        {3, "Comparar Periodos", comparar_periodos},
        {4, "Comparar Condiciones", comparar_condiciones},
        {0, "Volver", NULL}
    };

    ejecutar_menu("COMPARADOR AVANZADO", items, 5);
}

/**
 * @brief Muestra el análisis completo de rendimiento
 */
void mostrar_analisis()
{
    clear_screen();
    print_header("ANALISIS Y COMPARADOR");

    MenuItem items[] =
    {
        {1, "Analisis Basico", mostrar_analisis_basico},
        {2, "Comparador Avanzado", mostrar_comparador_avanzado},
        {3, get_text("menu_entrenador_ia"), &menu_entrenador_ia},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ANALISIS Y COMPARADOR", items, 4);
}

/**
 * @brief Estructura para almacenar estadísticas mensuales
 */
typedef struct
{
    int mes;
    int anio;
    double avg_valor;
    int total_partidos;
} EstadisticasMensuales;

/**
 * @brief Convierte número de mes a nombre
 *
 * @param mes Número del mes (1-12)
 * @return Nombre del mes en español
 */
static const char *mes_to_text(int mes)
{
    switch (mes)
    {
    case 1:
        return "Enero";
    case 2:
        return "Febrero";
    case 3:
        return "Marzo";
    case 4:
        return "Abril";
    case 5:
        return "Mayo";
    case 6:
        return "Junio";
    case 7:
        return "Julio";
    case 8:
        return "Agosto";
    case 9:
        return "Septiembre";
    case 10:
        return "Octubre";
    case 11:
        return "Noviembre";
    case 12:
        return "Diciembre";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Calcula estadísticas mensuales para una métrica específica
 *
 * @param stats Array donde almacenar las estadísticas mensuales
 * @param max_stats Tamaño máximo del array
 * @param columna Nombre de la columna a promediar (goles, asistencias, rendimiento_general)
 * @return Número de meses con datos
 */
static int calcular_estadisticas_mensuales(EstadisticasMensuales *stats, int max_stats, const char *columna)
{
    sqlite3_stmt *stmt;
    char sql[512];

    /*
     * Se utiliza strftime('%m', fecha_hora) y strftime('%Y', fecha_hora) para agrupar los datos
     * a nivel mensual, permitiendo calcular promedios (AVG) por cada período mes-año.
     * El orden persistente es descendente para mostrar primero los datos más recientes.
     */
    snprintf(sql, sizeof(sql),
             "SELECT strftime('%%m', fecha_hora) as mes, strftime('%%Y', fecha_hora) as anio, "
             "AVG(%s), COUNT(*) "
             "FROM partido "
             "GROUP BY strftime('%%Y', fecha_hora), strftime('%%m', fecha_hora) "
             "ORDER BY anio DESC, mes DESC",
             columna);

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_stats)
    {
        stats[count].mes = atoi((const char *)sqlite3_column_text(stmt, 0));
        stats[count].anio = atoi((const char *)sqlite3_column_text(stmt, 1));
        stats[count].avg_valor = sqlite3_column_double(stmt, 2);
        stats[count].total_partidos = sqlite3_column_int(stmt, 3);
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/**
 * @brief Muestra la evolución mensual de una métrica
 *
 * @param titulo Título a mostrar
 * @param columna Nombre de la columna de la base de datos
 */
static void mostrar_evolucion_mensual(const char *titulo, const char *columna)
{
    clear_screen();
    print_header(titulo);

    EstadisticasMensuales stats[120]; // Máximo 10 años de datos
    int num_meses = calcular_estadisticas_mensuales(stats, 120, columna);

    if (num_meses == 0)
    {
        printf("No hay suficientes datos para mostrar la evolución mensual.\n");
        printf("Registra al menos algunos partidos para ver estadísticas.\n");
        pause_console();
        return;
    }

    printf("EVOLUCION MENSUAL:\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < num_meses; i++)
    {
        printf("%s %d: %.2f (%d partidos)\n",
               mes_to_text(stats[i].mes), stats[i].anio,
               stats[i].avg_valor, stats[i].total_partidos);
    }

    pause_console();
}

/**
 * @brief Encuentra el mejor o peor mes histórico
 *
 * @param mejor 1 para mejor mes, 0 para peor mes
 */
static void encontrar_mes_historico(int mejor)
{
    clear_screen();
    print_header(mejor ? "MEJOR MES HISTORICO" : "PEOR MES HISTORICO");

    sqlite3_stmt *stmt;
    const char *sql = mejor ?
                      "SELECT strftime('%m', fecha_hora) as mes, strftime('%Y', fecha_hora) as anio, "
                      "AVG(rendimiento_general), COUNT(*) "
                      "FROM partido "
                      "GROUP BY strftime('%Y', fecha_hora), strftime('%m', fecha_hora) "
                      "ORDER BY AVG(rendimiento_general) DESC LIMIT 1" :
                      "SELECT strftime('%m', fecha_hora) as mes, strftime('%Y', fecha_hora) as anio, "
                      "AVG(rendimiento_general), COUNT(*) "
                      "FROM partido "
                      "GROUP BY strftime('%Y', fecha_hora), strftime('%m', fecha_hora) "
                      "ORDER BY AVG(rendimiento_general) ASC LIMIT 1";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int mes = atoi((const char *)sqlite3_column_text(stmt, 0));
        int anio = atoi((const char *)sqlite3_column_text(stmt, 1));
        double avg_rendimiento = sqlite3_column_double(stmt, 2);
        int partidos = sqlite3_column_int(stmt, 3);

        printf("%s MES HISTORICO:\n", mejor ? "MEJOR" : "PEOR");
        printf("----------------------------------------\n");
        printf("Mes: %s %d\n", mes_to_text(mes), anio);
        printf("Rendimiento promedio: %.2f\n", avg_rendimiento);
        printf("Partidos jugados: %d\n", partidos);
    }
    else
    {
        printf("No hay suficientes datos para determinar el %s mes histórico.\n",
               mejor ? "mejor" : "peor");
    }

    sqlite3_finalize(stmt);
    pause_console();

}

/**
 * @brief Compara rendimiento al inicio vs fin de año
 */
static void comparar_inicio_fin_anio()
{
    clear_screen();
    print_header("INICIO VS FIN DE ANIO");

    sqlite3_stmt *stmt;
    /*
     * Clasifica los partidos en dos grandes semestres usando CAST y strftime.
     * Esto permite un análisis comparativo de la evolución del rendimiento entre
     * la primera y la segunda mitad del año calendario.
     */
    const char *sql =
        "SELECT "
        "CASE WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) <= 6 THEN 'Inicio' ELSE 'Fin' END as periodo, "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), COUNT(*) "
        "FROM partido "
        "GROUP BY CASE WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) <= 6 THEN 'Inicio' ELSE 'Fin' END";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    printf("COMPARACION INICIO VS FIN DE ANIO:\n");
    printf("----------------------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
        double avg_goles = sqlite3_column_double(stmt, 1);
        double avg_asistencias = sqlite3_column_double(stmt, 2);
        double avg_rendimiento = sqlite3_column_double(stmt, 3);
        int partidos = sqlite3_column_int(stmt, 4);

        printf("%s de año (Ene-Jun):\n", strcmp(periodo, "Inicio") == 0 ? "Inicio" : "Fin");
        printf("  Goles: %.2f\n", avg_goles);
        printf("  Asistencias: %.2f\n", avg_asistencias);
        printf("  Rendimiento: %.2f\n", avg_rendimiento);
        printf("  Partidos: %d\n\n", partidos);
        count++;
    }

    if (count == 0)
    {
        printf("No hay suficientes datos para comparar inicio vs fin de año.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Compara rendimiento en meses fríos vs cálidos
 */
static void comparar_meses_frios_calidos()
{
    clear_screen();
    print_header("MESES FRIOS VS CALIDOS");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "CASE "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) BETWEEN 6 AND 9 THEN 'Frios' "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) IN (12,1,2,3,4) THEN 'Calidos' "
        "  ELSE 'Otros' "
        "END as temporada, "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), COUNT(*) "
        "FROM partido "
        "GROUP BY CASE "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) BETWEEN 6 AND 9 THEN 'Frios' "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) IN (12,1,2,3,4) THEN 'Calidos' "
        "  ELSE 'Otros' "
        "END";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    printf("COMPARACION MESES FRIOS VS CALIDOS:\n");
    printf("----------------------------------------\n");
    printf("Meses frios: Junio, Julio, Agosto, Septiembre\n");
    printf("Meses calidos: Diciembre, Enero, Febrero, Marzo, Abril\n\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *temporada = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(temporada, "Otros") == 0)
            continue;

        double avg_goles = sqlite3_column_double(stmt, 1);
        double avg_asistencias = sqlite3_column_double(stmt, 2);
        double avg_rendimiento = sqlite3_column_double(stmt, 3);
        int partidos = sqlite3_column_int(stmt, 4);

        printf("Meses %s:\n", temporada);
        printf("  Goles: %.2f\n", avg_goles);
        printf("  Asistencias: %.2f\n", avg_asistencias);
        printf("  Rendimiento: %.2f\n", avg_rendimiento);
        printf("  Partidos: %d\n\n", partidos);
        count++;
    }

    if (count == 0)
    {
        printf("No hay suficientes datos en meses frios o calidos para comparar.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Calcula y muestra la tendencia de rendimiento
 *
 * @param stmt Statement preparado para calcular tendencia
 */
static void mostrar_tendencia(sqlite3_stmt *tend_stmt)
{
    double avg_primeros = 0;
    double avg_ultimos = 0;
    if (sqlite3_step(tend_stmt) == SQLITE_ROW)
        avg_primeros = sqlite3_column_double(tend_stmt, 0);
    if (sqlite3_step(tend_stmt) == SQLITE_ROW)
        avg_ultimos = sqlite3_column_double(tend_stmt, 0);

    double tendencia = avg_ultimos - avg_primeros;
    printf("\nTENDENCIA:\n");
    printf("Primeros 5 partidos: %.2f\n", avg_primeros);
    printf("Últimos 5 partidos: %.2f\n", avg_ultimos);

    const char *tendencia_texto;
    if (tendencia > 0.5)
    {
        tendencia_texto = "ASCENDENTE";
    }
    else if (tendencia < -0.5)
    {
        tendencia_texto = "DESCENDENTE";
    }
    else
    {
        tendencia_texto = "ESTABLE";
    }
    printf("Tendencia: %s (%.2f)\n", tendencia_texto, tendencia);

    sqlite3_finalize(tend_stmt);
}

/**
 * @brief Calcula y muestra el progreso total del jugador
 */
static void calcular_progreso_total()
{
    clear_screen();
    print_header("PROGRESO TOTAL DEL JUGADOR");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "COUNT(*), "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), "
        "MIN(fecha_hora), MAX(fecha_hora) "
        "FROM partido";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos suficientes para calcular el progreso total.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int total_partidos = sqlite3_column_int(stmt, 0);
    double avg_goles = sqlite3_column_double(stmt, 1);
    double avg_asistencias = sqlite3_column_double(stmt, 2);
    double avg_rendimiento = sqlite3_column_double(stmt, 3);
    const char *fecha_inicio = (const char *)sqlite3_column_text(stmt, 4);
    const char *fecha_fin = (const char *)sqlite3_column_text(stmt, 5);

    printf("PROGRESO TOTAL DEL JUGADOR:\n");
    printf("----------------------------------------\n");
    printf("Periodo: %s - %s\n", fecha_inicio ? fecha_inicio : "N/A", fecha_fin ? fecha_fin : "N/A");
    printf("Total de partidos: %d\n", total_partidos);
    printf("Promedio de goles: %.2f\n", avg_goles);
    printf("Promedio de asistencias: %.2f\n", avg_asistencias);
    printf("Promedio de rendimiento: %.2f\n", avg_rendimiento);

    // Calcular tendencia (comparar primeros vs últimos partidos)
    if (total_partidos < 10)
    {
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    /*
     * Algoritmo de Tendencia: Se comparan los promedios de los primeros 5 partidos vs los últimos 5.
     * Se usa UNION ALL para obtener ambos promedios en una sola ejecución de statement,
     * optimizando el acceso a la base de datos para el cálculo del delta de rendimiento.
     */
    sqlite3_stmt *tend_stmt;
    const char *tend_sql =
        "SELECT AVG(rendimiento_general) FROM "
        "(SELECT rendimiento_general FROM partido ORDER BY fecha_hora ASC LIMIT 5) "
        "UNION ALL "
        "SELECT AVG(rendimiento_general) FROM "
        "(SELECT rendimiento_general FROM partido ORDER BY fecha_hora DESC LIMIT 5)";

    if (!preparar_stmt(&tend_stmt, tend_sql))
    {
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    mostrar_tendencia(tend_stmt);
    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Muestra el menú de evolución temporal
 */
void mostrar_evolucion_temporal()
{
    clear_screen();
    print_header("EVOLUCION TEMPORAL");

    MenuItem items[] =
    {
        {1, "Evolucion Mensual de Goles", evolucion_mensual_goles},
        {2, "Evolucion Mensual de Asistencias", evolucion_mensual_asistencias},
        {3, "Evolucion Mensual de Rendimiento", evolucion_mensual_rendimiento},
        {4, "Mejor Mes Historico", mejor_mes_historico},
        {5, "Peor Mes Historico", peor_mes_historico},
        {6, "Inicio vs Fin de Anio", inicio_vs_fin_anio},
        {7, "Meses Frios vs Calidos", meses_frios_vs_calidos},
        {8, "Progreso Total del Jugador", progreso_total_jugador},
        {0, "Volver", NULL}
    };

    ejecutar_menu("EVOLUCION TEMPORAL", items, 9);
}

/**
 * @brief Muestra la evolución mensual de goles
 */
void evolucion_mensual_goles()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE GOLES", "goles");
}

/**
 * @brief Muestra la evolución mensual de asistencias
 */
void evolucion_mensual_asistencias()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE ASISTENCIAS", "asistencias");
}

/**
 * @brief Muestra la evolución mensual de rendimiento
 */
void evolucion_mensual_rendimiento()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE RENDIMIENTO", "rendimiento_general");
}

/**
 * @brief Muestra el mejor mes histórico
 */
void mejor_mes_historico()
{
    encontrar_mes_historico(1);
}

/**
 * @brief Muestra el peor mes histórico
 */
void peor_mes_historico()
{
    encontrar_mes_historico(0);
}

/**
 * @brief Compara el rendimiento al inicio vs fin de año
 */
void inicio_vs_fin_anio()
{
    comparar_inicio_fin_anio();
}

/**
 * @brief Compara el rendimiento en meses fríos vs cálidos
 */
void meses_frios_vs_calidos()
{
    comparar_meses_frios_calidos();
}

/**
 * @brief Muestra el progreso total del jugador
 */
void progreso_total_jugador()
{
    calcular_progreso_total();
}
