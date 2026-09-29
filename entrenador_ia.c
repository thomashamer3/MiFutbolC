#include "entrenador_ia.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

/* Configuracion del nivel de intervencion (definidas mas abajo) */
static int obtener_nivel_intervencion(void);
static void guardar_nivel_intervencion(int nivel);

static void iniciar_pantalla_ia(const char *titulo)
{
    clear_screen();
    print_header(titulo);
}

static void limpiar_buffer_linea(void)
{
    int flag;
    while ((flag = getchar()) != '\n' && flag != EOF)
    {
        /* Descarta caracteres restantes del buffer de entrada. */
    }
}

static int leer_confirmacion_sn(const char *prompt)
{
    int respuesta;
    printf("%s", prompt);
    respuesta = getchar();
    limpiar_buffer_linea();
    return (respuesta != EOF && (respuesta == 's' || respuesta == 'S')) ? 1 : 0;
}

static void formatear_fecha_yyyy_mm_dd(time_t fecha, char *fecha_str, size_t tam)
{
    struct tm tm_fecha;
#ifdef _WIN32
    localtime_s(&tm_fecha, &fecha);
#else
    localtime_r(&fecha, &tm_fecha);
#endif
    strftime(fecha_str, tam, "%Y-%m-%d", &tm_fecha);
}

static void bind_rango_fechas_yyyy_mm_dd(sqlite3_stmt *stmt, time_t fecha_inicio, time_t fecha_fin)
{
    char fecha_inicio_str[20];
    char fecha_fin_str[20];

    formatear_fecha_yyyy_mm_dd(fecha_inicio, fecha_inicio_str, sizeof(fecha_inicio_str));
    formatear_fecha_yyyy_mm_dd(fecha_fin, fecha_fin_str, sizeof(fecha_fin_str));

    sqlite3_bind_text(stmt, 1, fecha_inicio_str, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fecha_fin_str, -1, DB_TRANSIENT);
}

static void ejecutar_upsert_perfil(const char *sql, int aceptados, int ignorados,
                                   float indice_prudencia)
{
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, sql))
    {
        sqlite3_bind_int(stmt, 1, aceptados);
        sqlite3_bind_int(stmt, 2, ignorados);
        sqlite3_bind_double(stmt, 3, indice_prudencia);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

/*
 * Interpreta las fechas de partido. En la base se guardan como "YYYY-MM-DD" o
 * "YYYY-MM-DD HH:MM" (convert_display_date_to_storage), aunque tambien se acepta
 * el formato de presentacion "DD/MM/YYYY".
 */
static int parsear_fecha_partido(const char *fecha_str, struct tm *tm_fecha)
{
    if (!fecha_str || fecha_str[0] == '\0')
    {
        return 0;
    }

    // Longitud minima esperada: "YYYY-MM-DD" o "DD/MM/YYYY"
    if (strlen(fecha_str) < 8)
    {
        return 0;
    }

    memset(tm_fecha, 0, sizeof(*tm_fecha));

    int anio = 0;
    int mes = 0;
    int dia = 0;
    int hora = 0;
    int minuto = 0;
    int leidos;

    if (fecha_str[4] == '-')
    {
#ifdef _WIN32
        leidos = sscanf_s(fecha_str, "%d-%d-%d %d:%d", &anio, &mes, &dia, &hora, &minuto);
#else
        leidos = sscanf(fecha_str, "%d-%d-%d %d:%d", &anio, &mes, &dia, &hora, &minuto);
#endif
    }
    else
    {
#ifdef _WIN32
        leidos = sscanf_s(fecha_str, "%d/%d/%d %d:%d", &dia, &mes, &anio, &hora, &minuto);
#else
        leidos = sscanf(fecha_str, "%d/%d/%d %d:%d", &dia, &mes, &anio, &hora, &minuto);
#endif
    }

    if (leidos < 3 || anio < 1900 || mes < 1 || mes > 12 || dia < 1 || dia > 31)
    {
        return 0;
    }

    tm_fecha->tm_year = anio - 1900;
    tm_fecha->tm_mon = mes - 1;
    tm_fecha->tm_mday = dia;
    tm_fecha->tm_hour = hora;
    tm_fecha->tm_min = minuto;
    tm_fecha->tm_isdst = -1;
    return 1;
}

static int dias_desde_fecha(const char *fecha_str, time_t ahora)
{
    struct tm tm_fecha;
    if (!parsear_fecha_partido(fecha_str, &tm_fecha))
    {
        return 0;
    }

    time_t fecha_partido = mktime(&tm_fecha);
    return (int)((ahora - fecha_partido) / ((time_t)60 * 60 * 24));
}

static void agregar_consejo(Consejo **consejos, int *num_consejos, int *capacidad,
                            const char *mensaje, NivelConsejo nivel, CategoriaConsejo categoria)
{
    if (*num_consejos >= *capacidad)
    {
        int nueva_capacidad = (*capacidad == 0) ? 8 : (*capacidad * 2);
        Consejo *tmp = realloc(*consejos, (size_t)nueva_capacidad * sizeof(Consejo));
        if (!tmp)
        {
            return;
        }
        *consejos = tmp;
        *capacidad = nueva_capacidad;
    }

    (*consejos)[*num_consejos].mensaje = strdup(mensaje);
    (*consejos)[*num_consejos].nivel = nivel;
    (*consejos)[*num_consejos].categoria = categoria;
    (*num_consejos)++;
}

// Tabla para historial de consejos
static const char *CREATE_CONSEJOS_TABLE = "CREATE TABLE IF NOT EXISTS consejos_historial ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "fecha INTEGER NOT NULL,"
        "consejo TEXT NOT NULL,"
        "seguido INTEGER NOT NULL DEFAULT 0);";

// Tabla para perfil de usuario
static const char *CREATE_PERFIL_TABLE = "CREATE TABLE IF NOT EXISTS perfil_usuario_ia ("
        "id INTEGER PRIMARY KEY,"
        "consejos_aceptados INTEGER DEFAULT 0,"
        "consejos_ignorados INTEGER DEFAULT 0,"
        "indice_prudencia REAL DEFAULT 0.5);";

// Tabla para configuracion de la IA (nivel de intervencion y control de frecuencia)
static const char *CREATE_IA_CONFIG_TABLE = "CREATE TABLE IF NOT EXISTS ia_config ("
        "id INTEGER PRIMARY KEY,"
        "nivel_intervencion INTEGER NOT NULL DEFAULT 2,"
        "ultima_alerta INTEGER NOT NULL DEFAULT 0,"
        "ultimo_riesgo REAL NOT NULL DEFAULT 0);";

// Fila unica de configuracion con los valores por defecto (nivel Moderado, sin avisos previos)
static const char *SQL_IA_CONFIG_DEFAULT =
    "INSERT OR IGNORE INTO ia_config (id, nivel_intervencion, ultima_alerta, ultimo_riesgo) "
    "VALUES (1, 2, 0, 0);";

// Horas minimas entre dos avisos automaticos consecutivos cuando el riesgo no es critico
#define IA_HORAS_ENFRIAMIENTO 24
#define IA_SEGUNDOS_ENFRIAMIENTO ((time_t)IA_HORAS_ENFRIAMIENTO * 60 * 60)

// Margen sobre el umbral del nivel a partir del cual el aviso se considera critico
// y no espera al periodo de enfriamiento
#define IA_MARGEN_RIESGO_CRITICO 1.0F

// Inicializar tablas de IA
void init_ia_tables(void)
{
    sqlite3_exec(db, CREATE_CONSEJOS_TABLE, 0, 0, 0);
    sqlite3_exec(db, CREATE_PERFIL_TABLE, 0, 0, 0);
    sqlite3_exec(db, CREATE_IA_CONFIG_TABLE, 0, 0, 0);
    sqlite3_exec(db, SQL_IA_CONFIG_DEFAULT, 0, 0, 0);
}

// Funciones auxiliares para strings
const char *nivel_a_string(NivelConsejo nivel)
{
    switch (nivel)
    {
    case CONSEJO_INFO:
        return "INFO";
    case CONSEJO_ADVERTENCIA:
        return "ADVERTENCIA";
    case CONSEJO_CRITICO:
        return "CRITICO";
    default:
        return "UNKNOWN";
    }
}

const char *categoria_a_string(CategoriaConsejo categoria)
{
    switch (categoria)
    {
    case CATEGORIA_FISICO:
        return "Fisico";
    case CATEGORIA_MENTAL:
        return "Mental";
    case CATEGORIA_DEPORTIVO:
        return "Deportivo";
    case CATEGORIA_SALUD:
        return "Salud";
    case CATEGORIA_GESTION:
        return "Gestion";
    default:
        return "Unknown";
    }
}

// Evaluar estado del jugador basado en datos historicos
EstadoJugador evaluar_estado_jugador(void)
{
    EstadoJugador estado = {0};
    sqlite3_stmt *stmt;
    const char *sql = "SELECT rendimiento_general, cansancio, estado_animo, fecha_hora "
                      "FROM partido "
                      "ORDER BY fecha_hora DESC LIMIT 10;"; // ultimos 10 partidos

    if (!preparar_stmt(&stmt, sql))
    {
        return estado;
    }

    int count = 0;
    int partidos_consecutivos = 0;
    int derrotas_consecutivas = 0;
    time_t now = time(NULL);
    int dias_sin_jugar = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < 10)
    {
        int rendimiento = sqlite3_column_int(stmt, 0);
        int cansancio = sqlite3_column_int(stmt, 1);
        int animo = sqlite3_column_int(stmt, 2);
        const char *fecha_str = (const char *)sqlite3_column_text(stmt, 3);

        estado.rendimiento_promedio += (float)rendimiento;
        estado.cansancio_promedio += (float)cansancio;
        estado.estado_animo_promedio += (float)animo;

        // Calcular dias desde ultimo partido
        if (count == 0 && fecha_str)
        {
            dias_sin_jugar = dias_desde_fecha(fecha_str, now);
        }

        // Contar partidos consecutivos (ultimos 7 dias)
        if (fecha_str && count < 7 && dias_desde_fecha(fecha_str, now) <= 7)
        {
            partidos_consecutivos++;
        }

        count++;
    }

    sqlite3_finalize(stmt);

    if (count > 0)
    {
        estado.rendimiento_promedio /= (float)count;
        estado.cansancio_promedio /= (float)count;
        estado.estado_animo_promedio /= (float)count;
    }

    estado.partidos_consecutivos = partidos_consecutivos;
    estado.dias_descanso = dias_sin_jugar;

    // Evaluar derrotas consecutivas
    const char *sql_derrotas = "SELECT resultado FROM partido ORDER BY fecha_hora DESC LIMIT 5;";
    if (preparar_stmt(&stmt, sql_derrotas))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int resultado = sqlite3_column_int(stmt, 0);
            if (resultado == 0) // Derrota
            {
                derrotas_consecutivas++;
            }
            else
            {
                break; // Si no es derrota, salir
            }
        }
        sqlite3_finalize(stmt);
    }
    estado.derrotas_consecutivas = derrotas_consecutivas;

    // Evaluar riesgo de lesion basado en cansancio y partidos consecutivos
    // Factor de descanso: reducir cansancio efectivo segun dias sin jugar
    // 7 dias = 50% reduccion, 14+ dias = cansancio descartado
    float factor_descanso = 1.0F;
    if (estado.dias_descanso >= 14)
    {
        factor_descanso = 0.0F;
    }
    else if (estado.dias_descanso >= 7)
    {
        factor_descanso = 0.5F;
    }
    else if (estado.dias_descanso > 0)
    {
        factor_descanso = 1.0F - ((float)estado.dias_descanso / 14.0F);
    }

    float cansancio_efectivo = estado.cansancio_promedio * factor_descanso;

    estado.riesgo_lesion = (cansancio_efectivo / 10.0F) +
                           ((float)estado.partidos_consecutivos / 3.0F) +
                           ((float)estado.derrotas_consecutivas / 2.0F);

    return estado;
}

// Generar consejos basados en reglas
void generar_consejos(EstadoJugador estado, Consejo **consejos, int *num_consejos)
{
    *consejos = NULL;
    *num_consejos = 0;
    int capacidad = 0;

    // Regla 1: Cansancio alto + partidos consecutivos
    if (estado.cansancio_promedio > 8 && estado.partidos_consecutivos >= 3)
    {
        agregar_consejo(consejos, num_consejos, &capacidad,
                        "Se recomienda descanso para reducir riesgo de lesion", CONSEJO_ADVERTENCIA,
                        CATEGORIA_FISICO);
    }

    // Regla 2: Rendimiento bajo
    if (estado.rendimiento_promedio < 3)
    {
        agregar_consejo(consejos, num_consejos, &capacidad,
                        "Rendimiento bajo detectado. Considerar rotacion de jugadores",
                        CONSEJO_ADVERTENCIA, CATEGORIA_DEPORTIVO);
    }

    // Regla 3: Estado de animo bajo + racha negativa
    if (estado.estado_animo_promedio < 3 && estado.derrotas_consecutivas >= 2)
    {
        agregar_consejo(consejos, num_consejos, &capacidad,
                        "Confianza baja por racha negativa. Motivar al equipo", CONSEJO_ADVERTENCIA,
                        CATEGORIA_MENTAL);
    }

    // Regla 4: Riesgo de lesion critico
    if (estado.riesgo_lesion > 3.0)
    {
        agregar_consejo(consejos, num_consejos, &capacidad,
                        "Riesgo de lesion muy elevado. Descanso obligatorio", CONSEJO_CRITICO,
                        CATEGORIA_SALUD);
    }

    // Regla 5: Demasiado descanso
    if (estado.dias_descanso > 14)
    {
        agregar_consejo(consejos, num_consejos, &capacidad,
                        "Demasiado tiempo sin jugar. Considerar partido amistoso", CONSEJO_INFO,
                        CATEGORIA_DEPORTIVO);
    }

    // Si no hay consejos especificos, dar consejo general positivo
    if (*num_consejos == 0)
    {
        agregar_consejo(consejos, num_consejos, &capacidad,
                        "Estado general bueno. Mantener rutina actual", CONSEJO_INFO,
                        CATEGORIA_FISICO);
    }
}

// Mostrar consejos actuales
void mostrar_consejos_actuales(void)
{
    iniciar_pantalla_ia("Consejos Actuales del Entrenador IA");

    EstadoJugador estado = evaluar_estado_jugador();
    Consejo *consejos = NULL;
    int num_consejos = 0;

    generar_consejos(estado, &consejos, &num_consejos);

    printf("\nEstado Actual del Jugador:\n");
    printf("Rendimiento promedio: %.1f/10\n", estado.rendimiento_promedio);
    printf("Cansancio promedio: %.1f/10\n", estado.cansancio_promedio);
    printf("Estado de animo promedio: %.1f/10\n", estado.estado_animo_promedio);
    printf("Partidos consecutivos: %d\n", estado.partidos_consecutivos);
    printf("Derrotas consecutivas: %d\n", estado.derrotas_consecutivas);
    printf("Dias de descanso: %d\n", estado.dias_descanso);
    printf("Riesgo de lesion: %.1f/5\n\n", estado.riesgo_lesion);

    printf("Consejos del Entrenador IA:\n");
    printf("==========================\n\n");

    for (int i = 0; i < num_consejos; i++)
    {
        printf("%s %s: %s\n\n", categoria_a_string(consejos[i].categoria),
               nivel_a_string(consejos[i].nivel), consejos[i].mensaje);

        // Preguntar si siguio el consejo
        int seguido = leer_confirmacion_sn("Seguiste este consejo? (s/n): ");
        guardar_consejo_historial(consejos[i].mensaje, seguido);
    }

    // Liberar memoria
    for (int i = 0; i < num_consejos; i++)
    {
        free(consejos[i].mensaje);
    }
    free(consejos);

    pause_console();
}

// Mostrar historial de consejos
void mostrar_historial_consejos(void)
{
    iniciar_pantalla_ia("Historial de Consejos");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT fecha, consejo, seguido FROM consejos_historial ORDER BY fecha DESC;";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("Error accediendo al historial.\n");
        pause_console();
        return;
    }

    printf("\nHistorial de Consejos:\n");
    printf("=====================\n\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        time_t fecha = sqlite3_column_int64(stmt, 0);
        const char *consejo = (const char *)sqlite3_column_text(stmt, 1);
        int seguido = sqlite3_column_int(stmt, 2);

        char fecha_str[20];
        formatear_fecha_yyyy_mm_dd(fecha, fecha_str, sizeof(fecha_str));

        printf("%s - %s [%s]\n", fecha_str, consejo, seguido ? "Seguido" : "Ignorado");

        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        mostrar_no_hay_registros("historial de consejos");
    }

    pause_console();
}

// Funcion auxiliar para obtener estadisticas de partidos en un rango de fechas
static void obtener_estadisticas_periodo(time_t fecha_inicio, time_t fecha_fin, float *rendimiento,
        float *cansancio, int *victorias, int *derrotas,
        int *lesiones)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT rendimiento_general, cansancio, resultado "
        "FROM partido "
        "WHERE (substr(fecha_hora,7,4)||'-'||substr(fecha_hora,4,2)||'-'||substr(fecha_hora,1,2)) "
        "BETWEEN ? AND ? "
        "ORDER BY fecha_hora DESC;";

    *rendimiento = 0.0F;
    *cansancio = 0.0F;
    *victorias = 0;
    *derrotas = 0;
    int count = 0;

    if (preparar_stmt(&stmt, sql))
    {
        bind_rango_fechas_yyyy_mm_dd(stmt, fecha_inicio, fecha_fin);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            *rendimiento += (float)sqlite3_column_int(stmt, 0);
            *cansancio += (float)sqlite3_column_int(stmt, 1);
            int resultado = sqlite3_column_int(stmt, 2);

            if (resultado == 1)
            {
                (*victorias)++;
            }
            else if (resultado == 0)
            {
                (*derrotas)++;
            }

            count++;
        }
        sqlite3_finalize(stmt);
    }

    if (count > 0)
    {
        *rendimiento /= (float)count;
        *cansancio /= (float)count;
    }

    // Contar lesiones en el periodo
    const char *sql_lesiones =
        "SELECT COUNT(*) FROM lesion "
        "WHERE (substr(fecha,7,4)||'-'||substr(fecha,4,2)||'-'||substr(fecha,1,2)) BETWEEN ? AND "
        "?;";

    if (preparar_stmt(&stmt, sql_lesiones))
    {
        bind_rango_fechas_yyyy_mm_dd(stmt, fecha_inicio, fecha_fin);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            *lesiones = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
}

// Estructura para estadisticas de un periodo
typedef struct
{
    float rendimiento;
    float cansancio;
    int victorias;
    int derrotas;
    int lesiones;
} EstadisticasPeriodo;

// Estructura para historial de consejos
typedef struct
{
    int id;
    time_t fecha;
    char consejo[256];
    int seguido;
} ConsejoHistorial;

// Funcion auxiliar para seleccionar un consejo del historial
static ConsejoHistorial *seleccionar_consejo_historial(ConsejoHistorial consejos[], int count)
{
    printf("\nSelecciona el ID del consejo (0 para cancelar): ");
    int id_seleccionado = input_int("");

    if (id_seleccionado == 0)
    {
        return NULL;
    }

    for (int i = 0; i < count; i++)
    {
        if (consejos[i].id == id_seleccionado)
        {
            return &consejos[i];
        }
    }

    printf("\nID no valido.\n");
    return NULL;
}

// Funcion auxiliar para mostrar tabla de comparacion
static void mostrar_tabla_comparacion(const EstadisticasPeriodo *antes,
                                      const EstadisticasPeriodo *despues)
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("COMPARACIoN DE ESTADiSTICAS (14 dias antes vs 14 dias despues)\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    printf("Metrica                  | Antes    | Despues  | Cambio\n");
    printf("-------------------------|----------|----------|------------\n");
    printf("Rendimiento promedio     | %-8.1f | %-8.1f | %+.1f\n", antes->rendimiento,
           despues->rendimiento, despues->rendimiento - antes->rendimiento);
    printf("Cansancio promedio       | %-8.1f | %-8.1f | %+.1f\n", antes->cansancio,
           despues->cansancio, despues->cansancio - antes->cansancio);
    printf("Victorias                | %-8d | %-8d | %+d\n", antes->victorias, despues->victorias,
           despues->victorias - antes->victorias);
    printf("Derrotas                 | %-8d | %-8d | %+d\n", antes->derrotas, despues->derrotas,
           despues->derrotas - antes->derrotas);
    printf("Lesiones                 | %-8d | %-8d | %+d\n\n", antes->lesiones, despues->lesiones,
           despues->lesiones - antes->lesiones);
}

// Funcion auxiliar para evaluar metricas y contar mejoras/empeoramientos
static void evaluar_metricas(const EstadisticasPeriodo *antes, const EstadisticasPeriodo *despues,
                             int *mejoras, int *empeoramientos)
{
    *mejoras = 0;
    *empeoramientos = 0;

    if (despues->rendimiento > antes->rendimiento)
    {
        (*mejoras)++;
    }
    else if (despues->rendimiento < antes->rendimiento)
    {
        (*empeoramientos)++;
    }

    if (despues->cansancio < antes->cansancio)
    {
        (*mejoras)++;
    }
    else if (despues->cansancio > antes->cansancio)
    {
        (*empeoramientos)++;
    }

    if (despues->victorias > antes->victorias)
    {
        (*mejoras)++;
    }
    else if (despues->victorias < antes->victorias)
    {
        (*empeoramientos)++;
    }

    if (despues->derrotas < antes->derrotas)
    {
        (*mejoras)++;
    }
    else if (despues->derrotas > antes->derrotas)
    {
        (*empeoramientos)++;
    }

    if (despues->lesiones < antes->lesiones)
    {
        (*mejoras)++;
    }
    else if (despues->lesiones > antes->lesiones)
    {
        (*empeoramientos)++;
    }
}

// Funcion auxiliar para mostrar evaluacion cuando se siguio el consejo
static void mostrar_evaluacion_seguido(int decision_acertada, const EstadisticasPeriodo *antes,
                                       const EstadisticasPeriodo *despues)
{
    printf("Decision tomada: SEGUIR el consejo\n\n");

    if (decision_acertada)
    {
        printf("✓ DECISIoN ACERTADA\n\n");
        printf("Seguir el consejo resulto en mejoras observables:\n");
        if (despues->rendimiento > antes->rendimiento)
        {
            printf("  • Rendimiento mejoro en %.1f puntos\n",
                   despues->rendimiento - antes->rendimiento);
        }
        if (despues->cansancio < antes->cansancio)
        {
            printf("  • Cansancio se redujo en %.1f puntos\n",
                   antes->cansancio - despues->cansancio);
        }
        if (despues->victorias > antes->victorias)
        {
            printf("  • Mas victorias (%d)\n", despues->victorias - antes->victorias);
        }
        if (despues->lesiones < antes->lesiones)
        {
            printf("  • Menos lesiones (%d)\n", antes->lesiones - despues->lesiones);
        }
    }
    else
    {
        printf("✗ DECISIoN CUESTIONABLE\n\n");
        printf("Seguir el consejo no genero los resultados esperados:\n");
        if (despues->rendimiento < antes->rendimiento)
        {
            printf("  • Rendimiento empeoro en %.1f puntos\n",
                   antes->rendimiento - despues->rendimiento);
        }
        if (despues->cansancio > antes->cansancio)
        {
            printf("  • Cansancio aumento en %.1f puntos\n", despues->cansancio - antes->cansancio);
        }
        if (despues->derrotas > antes->derrotas)
        {
            printf("  • Mas derrotas (%d)\n", despues->derrotas - antes->derrotas);
        }
        if (despues->lesiones > antes->lesiones)
        {
            printf("  • Mas lesiones (%d)\n", despues->lesiones - antes->lesiones);
        }
    }
}

// Funcion auxiliar para mostrar evaluacion cuando se ignoro el consejo
static void mostrar_evaluacion_ignorado(int decision_acertada, int mejoras,
                                        const EstadisticasPeriodo *antes,
                                        const EstadisticasPeriodo *despues)
{
    printf("Decision tomada: IGNORAR el consejo\n\n");

    if (decision_acertada)
    {
        printf("✓ DECISIoN RAZONABLE\n\n");
        printf("Ignorar el consejo no tuvo consecuencias negativas graves.\n");
        if (mejoras > 0)
        {
            printf("De hecho, algunas metricas mejoraron:\n");
            if (despues->rendimiento > antes->rendimiento)
            {
                printf("  • Rendimiento mejoro en %.1f puntos\n",
                       despues->rendimiento - antes->rendimiento);
            }
            if (despues->victorias > antes->victorias)
            {
                printf("  • Mas victorias (%d)\n", despues->victorias - antes->victorias);
            }
        }
    }
    else
    {
        printf("✗ DECISIoN ERRoNEA\n\n");
        printf("Ignorar el consejo resulto en deterioro del rendimiento:\n");
        if (despues->rendimiento < antes->rendimiento)
        {
            printf("  • Rendimiento cayo %.1f puntos\n", antes->rendimiento - despues->rendimiento);
        }
        if (despues->cansancio > antes->cansancio)
        {
            printf("  • Cansancio aumento %.1f puntos\n", despues->cansancio - antes->cansancio);
        }
        if (despues->derrotas > antes->derrotas)
        {
            printf("  • Mas derrotas (%d)\n", despues->derrotas - antes->derrotas);
        }
        if (despues->lesiones > antes->lesiones)
        {
            printf("  • Mas lesiones (%d) - CRiTICO\n", despues->lesiones - antes->lesiones);
        }
        printf("\n  Recomendacion: En el futuro, considera seguir este tipo de consejos.\n");
    }
}

// Funcion auxiliar para mostrar conclusion
static void mostrar_conclusion(int mejoras)
{
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("CONCLUSIoN\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    float efectividad = (float)mejoras / 5.0F * 100.0F;
    printf("Efectividad de la decision: %.0f%% (%d de 5 metricas mejoraron)\n\n", efectividad,
           mejoras);

    if (efectividad >= 60)
    {
        printf("Tu decision fue acertada. Continua tomando decisiones similares.\n");
    }
    else if (efectividad >= 40)
    {
        printf("Resultados mixtos. Analiza mejor el contexto antes de decidir.\n");
    }
    else
    {
        printf("La decision no fue optima. Aprende de esta experiencia.\n");
    }
}

// Evaluar decision pasada
void evaluar_decision_pasada(void)
{
    iniciar_pantalla_ia("Evaluar Decision Pasada");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha, consejo, seguido FROM consejos_historial ORDER BY fecha DESC LIMIT 20;";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("Error accediendo al historial.\n");
        pause_console();
        return;
    }

    printf("\nSelecciona un consejo para evaluar:\n\n");

    ConsejoHistorial consejos_lista[20];
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < 20)
    {
        consejos_lista[count].id = sqlite3_column_int(stmt, 0);
        consejos_lista[count].fecha = sqlite3_column_int64(stmt, 1);
        const char *consejo = (const char *)sqlite3_column_text(stmt, 2);
#ifdef _WIN32
        strncpy_s(consejos_lista[count].consejo, sizeof(consejos_lista[count].consejo), consejo,
                  _TRUNCATE);
#else
        if (consejo)
        {
            size_t len = strlen_s(consejo, sizeof(consejos_lista[count].consejo));
            size_t copy_len = len < sizeof(consejos_lista[count].consejo) - 1
                              ? len
                              : sizeof(consejos_lista[count].consejo) - 1;
            memcpy(consejos_lista[count].consejo, consejo, copy_len);
            consejos_lista[count].consejo[copy_len] = '\0';
        }
        else
        {
            consejos_lista[count].consejo[0] = '\0';
        }
#endif
        consejos_lista[count].seguido = sqlite3_column_int(stmt, 3);

        char fecha_str[20];
        formatear_fecha_yyyy_mm_dd(consejos_lista[count].fecha, fecha_str, sizeof(fecha_str));

        printf("%d. %s - %s [%s]\n", consejos_lista[count].id, fecha_str, consejo,
               consejos_lista[count].seguido ? "Seguido" : "Ignorado");
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
    {
        mostrar_no_hay_registros("consejos para evaluar");
        pause_console();
        return;
    }

    ConsejoHistorial *consejo_seleccionado = seleccionar_consejo_historial(consejos_lista, count);
    if (!consejo_seleccionado)
    {
        pause_console();
        return;
    }

    // Analisis de impacto
    iniciar_pantalla_ia("Analisis de Impacto de Decision");

    char fecha_str[20];
    formatear_fecha_yyyy_mm_dd(consejo_seleccionado->fecha, fecha_str, sizeof(fecha_str));

    printf("\nConsejo: %s\n", consejo_seleccionado->consejo);
    printf("Fecha: %s\n", fecha_str);
    printf("Decision: %s\n\n", consejo_seleccionado->seguido ? "SEGUIDO" : "IGNORADO");

    // Definir periodos
    time_t fecha_consejo = consejo_seleccionado->fecha;
    time_t fecha_antes_inicio = fecha_consejo - ((time_t)14 * 24 * 60 * 60);
    time_t fecha_antes_fin = fecha_consejo - ((time_t)1 * 24 * 60 * 60);
    time_t fecha_despues_inicio = fecha_consejo + ((time_t)1 * 24 * 60 * 60);
    time_t fecha_despues_fin = fecha_consejo + ((time_t)14 * 24 * 60 * 60);

    // Obtener estadisticas
    EstadisticasPeriodo stats_antes = {0};
    EstadisticasPeriodo stats_despues = {0};

    obtener_estadisticas_periodo(fecha_antes_inicio, fecha_antes_fin, &stats_antes.rendimiento,
                                 &stats_antes.cansancio, &stats_antes.victorias,
                                 &stats_antes.derrotas, &stats_antes.lesiones);
    obtener_estadisticas_periodo(fecha_despues_inicio, fecha_despues_fin,
                                 &stats_despues.rendimiento, &stats_despues.cansancio,
                                 &stats_despues.victorias, &stats_despues.derrotas,
                                 &stats_despues.lesiones);

    mostrar_tabla_comparacion(&stats_antes, &stats_despues);

    // Evaluacion del impacto
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("EVALUACIoN DEL IMPACTO\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    int mejoras;
    int empeoramientos;
    evaluar_metricas(&stats_antes, &stats_despues, &mejoras, &empeoramientos);

    int decision_acertada = (mejoras > empeoramientos);

    if (consejo_seleccionado->seguido)
    {
        mostrar_evaluacion_seguido(decision_acertada, &stats_antes, &stats_despues);
    }
    else
    {
        decision_acertada = (mejoras >= empeoramientos);
        mostrar_evaluacion_ignorado(decision_acertada, mejoras, &stats_antes, &stats_despues);
    }

    mostrar_conclusion(mejoras);
    pause_console();
}

// Configurar nivel de intervencion
void configurar_nivel_intervencion(void)
{
    iniciar_pantalla_ia("Configurar Nivel de Intervencion IA");

    int nivel_actual = obtener_nivel_intervencion();

    printf("\nNiveles de intervencion disponibles:\n");
    printf("0. Silencioso  - No interrumpe automaticamente al crear partidos\n");
    printf("1. Conservador - Solo avisa ante riesgo de lesion critico\n");
    printf("2. Moderado    - Avisa ante riesgo alto (maximo una vez por dia)\n");
    printf("3. Agresivo    - Avisa ante el menor indicio de riesgo\n\n");

    printf("Nivel actual: %d\n\n", nivel_actual);

    printf("Selecciona nivel (0-3): ");
    int nivel = input_int("");

    if (nivel < NIVEL_IA_SILENCIOSO || nivel > NIVEL_IA_AGRESIVO)
    {
        printf("\nNivel no valido. No se realizaron cambios.\n");
        pause_console();
        return;
    }

    guardar_nivel_intervencion(nivel);

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "Nivel de intervencion IA actualizado a %d", nivel);
    app_log_event("IA", log_msg);

    printf("\nNivel configurado: %d\n", nivel);
    if (nivel == NIVEL_IA_SILENCIOSO)
    {
        printf("La IA ya no avisara automaticamente antes de los partidos.\n");
    }
    else
    {
        printf("La IA avisara como maximo una vez cada %d horas "
               "(de inmediato si el riesgo es critico).\n", IA_HORAS_ENFRIAMIENTO);
    }

    pause_console();
}

// Guardar consejo en historial
void guardar_consejo_historial(const char *consejo, int seguido)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO consejos_historial (fecha, consejo, seguido) VALUES (?, ?, ?);";

    if (preparar_stmt(&stmt, sql))
    {
        sqlite3_bind_int64(stmt, 1, time(NULL));
        sqlite3_bind_text(stmt, 2, consejo, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, seguido);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        actualizar_perfil_usuario(seguido);
    }
}

// Obtener perfil del usuario
PerfilUsuarioIA obtener_perfil_usuario(void)
{
    PerfilUsuarioIA perfil = {0, 0, 0.5F};
    sqlite3_stmt *stmt;
    const char *sql = "SELECT consejos_aceptados, consejos_ignorados, indice_prudencia FROM "
                      "perfil_usuario_ia LIMIT 1;";

    if (preparar_stmt(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            perfil.consejos_aceptados = sqlite3_column_int(stmt, 0);
            perfil.consejos_ignorados = sqlite3_column_int(stmt, 1);
            perfil.indice_prudencia = (float)sqlite3_column_double(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    return perfil;
}

// Actualizar perfil del usuario
void actualizar_perfil_usuario(int consejo_seguido)
{
    sqlite3_stmt *stmt;
    const char *sql_select =
        "SELECT consejos_aceptados, consejos_ignorados FROM perfil_usuario_ia LIMIT 1;";
    const char *sql_update = "UPDATE perfil_usuario_ia SET consejos_aceptados = ?, "
                             "consejos_ignorados = ?, indice_prudencia = ? WHERE id = 1;";
    const char *sql_insert = "INSERT INTO perfil_usuario_ia (id, consejos_aceptados, "
                             "consejos_ignorados, indice_prudencia) VALUES (1, ?, ?, ?);";

    int aceptados = 0;
    int ignorados = 0;

    // Obtener valores actuales
    if (preparar_stmt(&stmt, sql_select))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            aceptados = sqlite3_column_int(stmt, 0);
            ignorados = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // Actualizar contadores
    if (consejo_seguido)
    {
        aceptados++;
    }
    else
    {
        ignorados++;
    }

    // Calcular indice de prudencia
    float indice_prudencia;
    if (aceptados + ignorados == 0)
    {
        indice_prudencia = 0.5F;
    }
    else
    {
        indice_prudencia = (float)aceptados / (float)(aceptados + ignorados);
    }

    // Actualizar o insertar
    if (aceptados + ignorados > 1)
    {
        ejecutar_upsert_perfil(sql_update, aceptados, ignorados, indice_prudencia);
    }
    else // Primer registro
    {
        ejecutar_upsert_perfil(sql_insert, aceptados, ignorados, indice_prudencia);
    }
}

// Umbral de riesgo de lesion que dispara el aviso segun el nivel configurado
static float umbral_riesgo_para_nivel(int nivel)
{
    switch (nivel)
    {
    case NIVEL_IA_CONSERVADOR:
        return 4.0F;
    case NIVEL_IA_AGRESIVO:
        return 2.0F;
    case NIVEL_IA_MODERADO:
    default:
        return 3.0F;
    }
}

// Nivel de intervencion configurado (Moderado por defecto)
static int obtener_nivel_intervencion(void)
{
    sqlite3_stmt *stmt;
    int nivel = NIVEL_IA_MODERADO;

    if (preparar_stmt(&stmt, "SELECT nivel_intervencion FROM ia_config WHERE id = 1;"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            nivel = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (nivel < NIVEL_IA_SILENCIOSO || nivel > NIVEL_IA_AGRESIVO)
    {
        nivel = NIVEL_IA_MODERADO;
    }

    return nivel;
}

static void guardar_nivel_intervencion(int nivel)
{
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, "UPDATE ia_config SET nivel_intervencion = ? WHERE id = 1;"))
    {
        sqlite3_bind_int(stmt, 1, nivel);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// Fecha y riesgo del ultimo aviso automatico mostrado (0 si nunca se mostro)
static void obtener_ultima_alerta_ia(time_t *ultima_alerta, float *ultimo_riesgo)
{
    sqlite3_stmt *stmt;

    *ultima_alerta = (time_t)0;
    *ultimo_riesgo = 0.0F;

    if (preparar_stmt(&stmt, "SELECT ultima_alerta, ultimo_riesgo FROM ia_config WHERE id = 1;"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            *ultima_alerta = (time_t)sqlite3_column_int64(stmt, 0);
            *ultimo_riesgo = (float)sqlite3_column_double(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
}

static void registrar_alerta_ia(time_t cuando, float riesgo)
{
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt,
                      "UPDATE ia_config SET ultima_alerta = ?, ultimo_riesgo = ? WHERE id = 1;"))
    {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)cuando);
        sqlite3_bind_double(stmt, 2, (double)riesgo);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// Funciones de activacion
void activar_ia_antes_partido(void)
{
#ifndef UNIT_TEST
    // Garantiza que la tabla de configuracion exista (tambien en bases ya creadas)
    init_ia_tables();
#endif

    int nivel = obtener_nivel_intervencion();
    if (nivel == NIVEL_IA_SILENCIOSO)
    {
        return;
    }

    // Esta funcion se llamaria antes de crear un partido
    EstadoJugador estado = evaluar_estado_jugador();

    float factor_descanso_trigger = 1.0F;
    if (estado.dias_descanso >= 14)
        factor_descanso_trigger = 0.0F;
    else if (estado.dias_descanso >= 7)
        factor_descanso_trigger = 0.5F;
    else if (estado.dias_descanso > 0)
        factor_descanso_trigger = 1.0F - ((float)estado.dias_descanso / 14.0F);

    float cansancio_trigger = estado.cansancio_promedio * factor_descanso_trigger;

    float umbral = umbral_riesgo_para_nivel(nivel);
    int riesgo_alto = estado.riesgo_lesion > umbral;
    int fatiga_extrema = cansancio_trigger > 8.5F;

    if (!riesgo_alto && !fatiga_extrema)
    {
        return;
    }

    time_t ultima_alerta = (time_t)0;
    float ultimo_riesgo = 0.0F;
    obtener_ultima_alerta_ia(&ultima_alerta, &ultimo_riesgo);

    time_t ahora = time(NULL);
    int riesgo_critico = estado.riesgo_lesion >= (umbral + IA_MARGEN_RIESGO_CRITICO);

    // No repetir el aviso en cada partido: como maximo una vez cada 24 horas,
    // salvo que el riesgo sea critico.
    if (!riesgo_critico && ultima_alerta > (time_t)0 &&
            (ahora - ultima_alerta) < IA_SEGUNDOS_ENFRIAMIENTO)
    {
        return;
    }

    char log_msg[160];
    snprintf(log_msg, sizeof(log_msg),
             "Aviso IA antes de partido: riesgo %.2f (umbral %.2f, nivel %d, aviso previo %.2f)",
             estado.riesgo_lesion, umbral, nivel, ultimo_riesgo);
    app_log_event("IA", log_msg);

    registrar_alerta_ia(ahora, estado.riesgo_lesion);

    if (leer_confirmacion_sn(
                "\nIA: Alto riesgo detectado. Deseas ver consejos antes de continuar? (s/n): "))
    {
        mostrar_consejos_actuales();
    }
}

void activar_ia_antes_torneo(void)
{
    // Similar para torneos
    printf("\nIA: Analizando estado antes de torneo...\n");
    EstadoJugador estado = evaluar_estado_jugador();

    if (estado.partidos_consecutivos > 5)
    {
        printf("IA: Muchos partidos consecutivos. Recomendado descansar antes del torneo.\n");
        pause_console();
    }
}

void activar_ia_estadisticas(void)
{
    // Se activa al abrir estadisticas
    PerfilUsuarioIA perfil = obtener_perfil_usuario();
    const char *tipo_usuario;
    if (perfil.indice_prudencia > 0.6)
    {
        tipo_usuario = "Prudente";
    }
    else if (perfil.indice_prudencia < 0.4)
    {
        tipo_usuario = "Arriesgado";
    }
    else
    {
        tipo_usuario = "Moderado";
    }
    printf("\nIA: Perfil de usuario - %s (Prudencia: %.1f%%)\n", tipo_usuario,
           perfil.indice_prudencia * 100);
}

// Menu principal de la IA
void menu_entrenador_ia(void)
{
#ifndef UNIT_TEST
    init_ia_tables();
#endif

    MenuItem items[] = {{1, "Ver consejos actuales", &mostrar_consejos_actuales},
        {2, "Ver historial de consejos", &mostrar_historial_consejos},
        {3, "Evaluar decision pasada", &evaluar_decision_pasada},
        {4, "Configurar nivel de intervencion", &configurar_nivel_intervencion},
        {5, "Predecir resultado de partido", &predecir_resultado_partido},
        {6, "Recomendar formacion optima", &recomendar_formacion},
        {7, "Ver alertas de rendimiento", &mostrar_alertas_rendimiento},
        {8, "Sugerir periodo de descanso", &sugerir_descanso},
        {9, "Analizar puntos debiles", &analizar_puntos_debiles},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ENTRENADOR IA", items, 10);
}

// ═══════════════════════════════════════════════════════════════════════════
// NUEVAS FUNCIONES MEJORADAS DEL ENTRENADOR IA
// ═══════════════════════════════════════════════════════════════════════════

void predecir_resultado_partido(void)
{
    iniciar_pantalla_ia("Prediccion de Resultado");

    EstadoJugador estado = evaluar_estado_jugador();

    // Calcular probabilidad de victoria basada en métricas
    float prob_victoria = 50.0F; // Base 50%

    // Ajustar por rendimiento
    prob_victoria += (estado.rendimiento_promedio - 5.0F) * 5.0F;

    // Ajustar por cansancio (negativo)
    prob_victoria -= (estado.cansancio_promedio - 5.0F) * 3.0F;

    // Ajustar por estado de ánimo
    prob_victoria += (estado.estado_animo_promedio - 5.0F) * 4.0F;

    // Ajustar por racha
    if (estado.derrotas_consecutivas > 0)
    {
        prob_victoria -= (float)estado.derrotas_consecutivas * 5.0F;
    }

    // Limitar entre 5% y 95%
    if (prob_victoria < 5.0F)
    {
        prob_victoria = 5.0F;
    }
    if (prob_victoria > 95.0F)
    {
        prob_victoria = 95.0F;
    }

    float prob_empate = (100.0F - prob_victoria) * 0.3F;
    float prob_derrota = 100.0F - prob_victoria - prob_empate;

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║             PREDICCIoN PARA EL PRoXIMO PARTIDO              ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║  Probabilidad de VICTORIA:  %.0f%%%-27s║\n", prob_victoria, "");
    printf("║  Probabilidad de EMPATE:    %.0f%%%-27s║\n", prob_empate, "");
    printf("║  Probabilidad de DERROTA:   %.0f%%%-27s║\n", prob_derrota, "");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("Factores clave:\n");
    printf("  • Rendimiento actual: %.1f/10\n", estado.rendimiento_promedio);
    printf("  • Cansancio: %.1f/10\n", estado.cansancio_promedio);
    printf("  • Estado de animo: %.1f/10\n", estado.estado_animo_promedio);
    if (estado.derrotas_consecutivas > 0)
    {
        printf("  • Racha negativa: %d derrotas consecutivas\n", estado.derrotas_consecutivas);
    }
    printf("\n");

    pause_console();
}

void recomendar_formacion(void)
{
    iniciar_pantalla_ia("Recomendacion de Formacion");

    EstadoJugador estado = evaluar_estado_jugador();

    printf("\nAnalizando estado del equipo...\n\n");

    const char *formacion_recomendada;
    const char *razon;

    // Lógica de recomendación
    if (estado.cansancio_promedio > 7.0F)
    {
        formacion_recomendada = "4-5-1 (Defensiva)";
        razon = "Equipo cansado - Priorizar defensa y conservar energia";
    }
    else if (estado.rendimiento_promedio >= 7.0F)
    {
        formacion_recomendada = "4-3-3 (Ofensiva)";
        razon = "Buen rendimiento - Aprovechar momento ofensivo";
    }
    else if (estado.derrotas_consecutivas >= 2)
    {
        formacion_recomendada = "4-4-2 (Equilibrada)";
        razon = "Racha negativa - Buscar equilibrio y confianza";
    }
    else if (estado.estado_animo_promedio < 4.0F)
    {
        formacion_recomendada = "5-3-2 (Conservadora)";
        razon = "Moral baja - Asegurar defensa y jugar simple";
    }
    else
    {
        formacion_recomendada = "4-2-3-1 (Balanceada)";
        razon = "Estado normal - Formacion balanceada es optima";
    }

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              FORMACIoN RECOMENDADA                           ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║  %s%-44s║\n", formacion_recomendada, "");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("Razon: %s\n\n", razon);

    printf("Consejos adicionales:\n");
    if (estado.cansancio_promedio > 6.0F)
    {
        printf("  • Considerar rotacion de jugadores\n");
    }
    if (estado.riesgo_lesion > 2.0F)
    {
        printf("  • Evitar jugadores con alto riesgo de lesion\n");
    }
    if (estado.estado_animo_promedio < 5.0F)
    {
        printf("  • Dar charla motivacional antes del partido\n");
    }
    printf("\n");

    pause_console();
}

void mostrar_alertas_rendimiento(void)
{
    iniciar_pantalla_ia("Alertas de Rendimiento");

    sqlite3_stmt *stmt = NULL;
    int alertas_encontradas = 0;

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              ALERTAS DE RENDIMIENTO DETECTADAS              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    // Alerta 1: Rendimiento bajo en días específicos
    const char *sql_dias =
        "SELECT strftime('%w', fecha) as dia, AVG(rendimiento_general) as promedio "
        "FROM partido GROUP BY dia HAVING promedio < 4.0;";

    if (preparar_stmt(&stmt, sql_dias))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int dia = sqlite3_column_int(stmt, 0);
            float promedio = (float)sqlite3_column_double(stmt, 1);

            const char *nombre_dia[] = {"Domingo", "Lunes",   "Martes", "Miercoles",
                                        "Jueves",  "Viernes", "Sabado"
                                       };

            printf("⚠️  ALERTA: Rendimiento bajo los %s (%.1f/10)\n", nombre_dia[dia], promedio);
            printf("    Recomendacion: Evitar partidos importantes este dia\n\n");
            alertas_encontradas++;
        }
        sqlite3_finalize(stmt);
    }

    // Alerta 2: Partidos con mucho cansancio
    const char *sql_cansancio = "SELECT COUNT(*) FROM partido WHERE cansancio >= 8;";

    if (preparar_stmt(&stmt, sql_cansancio))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt, 0);
            if (count > 5)
            {
                printf("⚠️  ALERTA: %d partidos con cansancio critico (>=8)\n", count);
                printf("    Recomendacion: Aumentar periodos de descanso\n\n");
                alertas_encontradas++;
            }
        }
        sqlite3_finalize(stmt);
    }

    // Alerta 3: Tendencia negativa reciente
    const char *sql_tendencia = "SELECT AVG(rendimiento_general) FROM (SELECT rendimiento_general "
                                "FROM partido ORDER BY fecha DESC, hora DESC LIMIT 5);";

    if (preparar_stmt(&stmt, sql_tendencia))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            float promedio_reciente = (float)sqlite3_column_double(stmt, 0);
            if (promedio_reciente < 4.5F)
            {
                printf("  ALERTA: Tendencia negativa en ultimos 5 partidos (%.1f/10)\n",
                       promedio_reciente);
                printf("    Recomendacion: Analizar tacticas y motivacion del equipo\n\n");
                alertas_encontradas++;
            }
        }
        sqlite3_finalize(stmt);
    }

    if (alertas_encontradas == 0)
    {
        printf("No se detectaron alertas criticas.\n");
        printf("   El rendimiento esta dentro de parametros normales.\n\n");
    }

    pause_console();
}

void sugerir_descanso(void)
{
    iniciar_pantalla_ia("Sugerencia de Descanso");

    EstadoJugador estado = evaluar_estado_jugador();

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           ANALISIS DE NECESIDAD DE DESCANSO                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("Estado actual:\n");
    printf("  • Cansancio promedio: %.1f/10\n", estado.cansancio_promedio);
    printf("  • Partidos consecutivos: %d\n", estado.partidos_consecutivos);
    printf("  • Dias desde ultimo partido: %d\n", estado.dias_descanso);
    printf("  • Riesgo de lesion: %.1f/5\n\n", estado.riesgo_lesion);

    // Lógica de recomendación
    int dias_descanso_recomendados = 0;
    const char *urgencia;

    if (estado.cansancio_promedio >= 8.0F)
    {
        dias_descanso_recomendados = 7;
        urgencia = "URGENTE";
    }
    else if (estado.cansancio_promedio >= 6.0F || estado.partidos_consecutivos >= 4)
    {
        dias_descanso_recomendados = 5;
        urgencia = "Recomendado";
    }
    else if (estado.partidos_consecutivos >= 3)
    {
        dias_descanso_recomendados = 3;
        urgencia = "Sugerido";
    }
    else if (estado.dias_descanso >= 10)
    {
        dias_descanso_recomendados = 0;
        urgencia = "No necesario - Ya descansado";
    }
    else
    {
        dias_descanso_recomendados = 2;
        urgencia = "Mantenimiento";
    }

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("RECOMENDACIoN\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    if (dias_descanso_recomendados > 0)
    {
        printf("Descanso recomendado: %d dias\n", dias_descanso_recomendados);
        printf("Nivel de urgencia: %s\n\n", urgencia);

        if (estado.riesgo_lesion > 2.5F)
        {
            printf("⚠️  IMPORTANTE: Alto riesgo de lesion detectado!\n");
            printf("   El descanso es CRITICO para evitar lesiones.\n\n");
        }

        printf("Durante el descanso:\n");
        printf("  □ Evitar partidos oficiales\n");
        printf("  □ Reducir intensidad de entrenamientos\n");
        printf("  □ Enfocarse en recuperacion y estiramiento\n");
        if (estado.estado_animo_promedio < 5.0F)
        {
            printf("  □ Trabajar en aspectos mentales y motivacion\n");
        }
    }
    else
    {
        printf("✓ No requieres descanso adicional en este momento.\n");
        printf("  Estado general es bueno para continuar jugando.\n");
    }

    printf("\n");
    pause_console();
}

static int analizar_debilidad_goles(void)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_goles = "SELECT AVG(goles) FROM partido;";

    if (!preparar_stmt(&stmt, sql_goles))
    {
        return 0;
    }

    int identificada = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        float promedio_goles = (float)sqlite3_column_double(stmt, 0);
        if (promedio_goles < 1.5F)
        {
            printf(" DEBILIDAD: Capacidad ofensiva (%.1f goles/partido)\n", promedio_goles);
            printf("    Sugerencias:\n");
            printf("      • Practicar definicion y finalizacion\n");
            printf("      • Trabajar jugadas de ataque\n");
            printf("      • Revisar posicionamiento ofensivo\n\n");
            identificada = 1;
        }
    }

    sqlite3_finalize(stmt);
    return identificada;
}

static int analizar_debilidad_asistencias(void)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_asistencias = "SELECT AVG(asistencias) FROM partido;";

    if (!preparar_stmt(&stmt, sql_asistencias))
    {
        return 0;
    }

    int identificada = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        float promedio_asist = (float)sqlite3_column_double(stmt, 0);
        if (promedio_asist < 1.0F)
        {
            printf("  DEBILIDAD: Juego en equipo (%.1f asistencias/partido)\n", promedio_asist);
            printf("    Sugerencias:\n");
            printf("      • Mejorar comunicacion en cancha\n");
            printf("      • Practicar pases y movimientos sin balon\n");
            printf("      • Fomentar juego colectivo\n\n");
            identificada = 1;
        }
    }

    sqlite3_finalize(stmt);
    return identificada;
}

static int obtener_count_desde_sql(const char *sql, int *valor)
{
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    int flag = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *valor = sqlite3_column_int(stmt, 0);
        flag = 1;
    }

    sqlite3_finalize(stmt);
    return flag;
}

static int analizar_debilidad_lesiones(void)
{
    const char *sql_lesiones = "SELECT COUNT(*) FROM lesion;";
    const char *sql_partidos = "SELECT COUNT(*) FROM partido;";
    int total_lesiones = 0;
    int total_partidos = 0;

    if (!obtener_count_desde_sql(sql_lesiones, &total_lesiones) ||
            !obtener_count_desde_sql(sql_partidos, &total_partidos) || total_partidos <= 0)
    {
        return 0;
    }

    float tasa_lesiones = (float)total_lesiones / (float)total_partidos;
    if (tasa_lesiones <= 0.3F)
    {
        return 0;
    }

    printf(" DEBILIDAD: Alta tasa de lesiones (%.1f%%)\n", tasa_lesiones * 100);
    printf("    Sugerencias:\n");
    printf("      • Mejorar calentamiento pre-partido\n");
    printf("      • Aumentar trabajo de flexibilidad\n");
    printf("      • Revisar carga de entrenamientos\n");
    printf("      • Consultar con fisioterapeuta\n\n");
    return 1;
}

static int analizar_debilidad_consistencia(void)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_varianza =
        "SELECT MAX(rendimiento_general) - MIN(rendimiento_general) FROM partido;";

    if (!preparar_stmt(&stmt, sql_varianza))
    {
        return 0;
    }

    int identificada = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int varianza = sqlite3_column_int(stmt, 0);
        if (varianza > 6)
        {
            printf("  DEBILIDAD: Falta de consistencia (varianza: %d puntos)\n", varianza);
            printf("    Sugerencias:\n");
            printf("      • Establecer rutinas pre-partido\n");
            printf("      • Trabajar aspectos mentales\n");
            printf("      • Mantener formacion estable\n\n");
            identificada = 1;
        }
    }

    sqlite3_finalize(stmt);
    return identificada;
}

void analizar_puntos_debiles(void)
{
    iniciar_pantalla_ia("Analisis de Puntos Debiles");

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           ANALISIS DE AREAS DE MEJORA                       ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    int areas_identificadas = 0;
    areas_identificadas += analizar_debilidad_goles();
    areas_identificadas += analizar_debilidad_asistencias();
    areas_identificadas += analizar_debilidad_lesiones();
    areas_identificadas += analizar_debilidad_consistencia();

    if (areas_identificadas == 0)
    {
        printf(" No se identificaron debilidades criticas.\n");
        printf("   El equipo muestra un desarrollo equilibrado.\n");
        printf("   Continua con el trabajo actual.\n\n");
    }
    else
    {
        printf("═══════════════════════════════════════════════════════════════\n");
        printf("Total de areas identificadas: %d\n", areas_identificadas);
        printf("Prioriza trabajar en estos aspectos durante entrenamientos.\n\n");
    }

    pause_console();
}
