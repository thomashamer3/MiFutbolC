#include "entrenador_ia.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Tabla para historial de consejos
static const char *CREATE_CONSEJOS_TABLE =
    "CREATE TABLE IF NOT EXISTS consejos_historial ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "fecha INTEGER NOT NULL,"
    "consejo TEXT NOT NULL,"
    "seguido INTEGER NOT NULL DEFAULT 0);";

// Tabla para perfil de usuario
static const char *CREATE_PERFIL_TABLE =
    "CREATE TABLE IF NOT EXISTS perfil_usuario_ia ("
    "id INTEGER PRIMARY KEY,"
    "consejos_aceptados INTEGER DEFAULT 0,"
    "consejos_ignorados INTEGER DEFAULT 0,"
    "indice_prudencia REAL DEFAULT 0.5);";

// Inicializar tablas de IA
void init_ia_tables()
{
    sqlite3_exec(db, CREATE_CONSEJOS_TABLE, 0, 0, 0);
    sqlite3_exec(db, CREATE_PERFIL_TABLE, 0, 0, 0);
}

// Funciones auxiliares para strings
const char* nivel_a_string(NivelConsejo nivel)
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

const char* categoria_a_string(CategoriaConsejo categoria)
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

// Evaluar estado del jugador basado en datos históricos
EstadoJugador evaluar_estado_jugador()
{
    EstadoJugador estado = {0};
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT rendimiento_general, cansancio, estado_animo, fecha_hora "
        "FROM partido "
        "ORDER BY fecha_hora DESC LIMIT 10;"; // Últimos 10 partidos

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
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
        const char *fecha_str = (const char*)sqlite3_column_text(stmt, 3);

        estado.rendimiento_promedio += (float)rendimiento;
        estado.cansancio_promedio += (float)cansancio;
        estado.estado_animo_promedio += (float)animo;

        // Calcular días desde último partido
        if (count == 0 && fecha_str)
        {
struct tm tm_fecha = {0};
sscanf_s(fecha_str, "%d-%d-%d", &tm_fecha.tm_year, &tm_fecha.tm_mon, &tm_fecha.tm_mday);
            tm_fecha.tm_year -= 1900;
            tm_fecha.tm_mon -= 1;
            time_t fecha_partido = mktime(&tm_fecha);
            dias_sin_jugar = (int)((now - fecha_partido) / (60 * 60 * 24));
        }

        // Contar partidos consecutivos (últimos 7 días)
        if (fecha_str && count < 7)
        {
struct tm tm_fecha = {0};
sscanf_s(fecha_str, "%d-%d-%d", &tm_fecha.tm_year, &tm_fecha.tm_mon, &tm_fecha.tm_mday);
            tm_fecha.tm_year -= 1900;
            tm_fecha.tm_mon -= 1;
            time_t fecha_partido = mktime(&tm_fecha);
            if ((int)((now - fecha_partido) / (60 * 60 * 24)) <= 7)
            {
                partidos_consecutivos++;
            }
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
    const char *sql_derrotas =
        "SELECT resultado FROM partido ORDER BY fecha_hora DESC LIMIT 5;";
    if (sqlite3_prepare_v2(db, sql_derrotas, -1, &stmt, 0) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int resultado = sqlite3_column_int(stmt, 0);
            if (resultado == 0)   // Derrota
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

    // Evaluar riesgo de lesión basado en cansancio y partidos consecutivos
    estado.riesgo_lesion = (estado.cansancio_promedio / 10.0f) +
                           ((float)estado.partidos_consecutivos / 3.0f) +
                           ((float)estado.derrotas_consecutivas / 2.0f);

    return estado;
}

// Generar consejos basados en reglas
void generar_consejos(EstadoJugador estado, Consejo **consejos, int *num_consejos)
{
    *consejos = NULL;
    *num_consejos = 0;

    // Regla 1: Cansancio alto + partidos consecutivos
    if (estado.cansancio_promedio > 8 && estado.partidos_consecutivos >= 3)
    {
        *consejos = realloc(*consejos, (*num_consejos + 1) * sizeof(Consejo));
        (*consejos)[*num_consejos].mensaje = strdup("Se recomienda descanso para reducir riesgo de lesión");
        (*consejos)[*num_consejos].nivel = CONSEJO_ADVERTENCIA;
        (*consejos)[*num_consejos].categoria = CATEGORIA_FISICO;
        (*num_consejos)++;
    }

    // Regla 2: Rendimiento bajo
    if (estado.rendimiento_promedio < 3)
    {
        *consejos = realloc(*consejos, (*num_consejos + 1) * sizeof(Consejo));
        (*consejos)[*num_consejos].mensaje = strdup("Rendimiento bajo detectado. Considerar rotación de jugadores");
        (*consejos)[*num_consejos].nivel = CONSEJO_ADVERTENCIA;
        (*consejos)[*num_consejos].categoria = CATEGORIA_DEPORTIVO;
        (*num_consejos)++;
    }

    // Regla 3: Estado de ánimo bajo + racha negativa
    if (estado.estado_animo_promedio < 3 && estado.derrotas_consecutivas >= 2)
    {
        *consejos = realloc(*consejos, (*num_consejos + 1) * sizeof(Consejo));
        (*consejos)[*num_consejos].mensaje = strdup("Confianza baja por racha negativa. Motivar al equipo");
        (*consejos)[*num_consejos].nivel = CONSEJO_ADVERTENCIA;
        (*consejos)[*num_consejos].categoria = CATEGORIA_MENTAL;
        (*num_consejos)++;
    }

    // Regla 4: Riesgo de lesión crítico
    if (estado.riesgo_lesion > 3.0)
    {
        *consejos = realloc(*consejos, (*num_consejos + 1) * sizeof(Consejo));
        (*consejos)[*num_consejos].mensaje = strdup("Riesgo de lesión muy elevado. Descanso obligatorio");
        (*consejos)[*num_consejos].nivel = CONSEJO_CRITICO;
        (*consejos)[*num_consejos].categoria = CATEGORIA_SALUD;
        (*num_consejos)++;
    }

    // Regla 5: Demasiado descanso
    if (estado.dias_descanso > 14)
    {
        *consejos = realloc(*consejos, (*num_consejos + 1) * sizeof(Consejo));
        (*consejos)[*num_consejos].mensaje = strdup("Demasiado tiempo sin jugar. Considerar partido amistoso");
        (*consejos)[*num_consejos].nivel = CONSEJO_INFO;
        (*consejos)[*num_consejos].categoria = CATEGORIA_DEPORTIVO;
        (*num_consejos)++;
    }

    // Si no hay consejos específicos, dar consejo general positivo
    if (*num_consejos == 0)
    {
        *consejos = realloc(*consejos, (*num_consejos + 1) * sizeof(Consejo));
        (*consejos)[*num_consejos].mensaje = strdup("Estado general bueno. Mantener rutina actual");
        (*consejos)[*num_consejos].nivel = CONSEJO_INFO;
        (*consejos)[*num_consejos].categoria = CATEGORIA_FISICO;
        (*num_consejos)++;
    }
}

// Mostrar consejos actuales
void mostrar_consejos_actuales()
{
    clear_screen();
    print_header("Consejos Actuales del Entrenador IA");

    EstadoJugador estado = evaluar_estado_jugador();
    Consejo *consejos = NULL;
    int num_consejos = 0;

    generar_consejos(estado, &consejos, &num_consejos);

    printf("\nEstado Actual del Jugador:\n");
    printf("Rendimiento promedio: %.1f/10\n", estado.rendimiento_promedio);
    printf("Cansancio promedio: %.1f/10\n", estado.cansancio_promedio);
    printf("Estado de ánimo promedio: %.1f/10\n", estado.estado_animo_promedio);
    printf("Partidos consecutivos: %d\n", estado.partidos_consecutivos);
    printf("Derrotas consecutivas: %d\n", estado.derrotas_consecutivas);
    printf("Días de descanso: %d\n", estado.dias_descanso);
    printf("Riesgo de lesión: %.1f/5\n\n", estado.riesgo_lesion);

    printf("Consejos del Entrenador IA:\n");
    printf("==========================\n\n");

    for (int i = 0; i < num_consejos; i++)
    {
        printf("%s %s: %s\n\n",
               categoria_a_string(consejos[i].categoria),
               nivel_a_string(consejos[i].nivel),
               consejos[i].mensaje);

        // Preguntar si siguió el consejo
        printf("¿Seguiste este consejo? (s/n): ");
        int respuesta = getchar();
        while (getchar() != '\n'); // Limpiar buffer

        int seguido = (respuesta != EOF && (respuesta == 's' || respuesta == 'S')) ? 1 : 0;
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
void mostrar_historial_consejos()
{
    clear_screen();
    print_header("Historial de Consejos");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT fecha, consejo, seguido FROM consejos_historial ORDER BY fecha DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
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
        const char *consejo = (const char*)sqlite3_column_text(stmt, 1);
        int seguido = sqlite3_column_int(stmt, 2);

        struct tm tm_fecha;
        char fecha_str[20];
        localtime_s(&tm_fecha, &fecha);
        strftime(fecha_str, sizeof(fecha_str), "%Y-%m-%d", &tm_fecha);

        printf("%s - %s [%s]\n",
               fecha_str,
               consejo,
               seguido ? "Seguido" : "Ignorado");

        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        mostrar_no_hay_registros("historial de consejos");
    }

    pause_console();
}

// Evaluar decisión pasada
void evaluar_decision_pasada()
{
    clear_screen();
    print_header("Evaluar Decisión Pasada");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha, consejo, seguido FROM consejos_historial ORDER BY fecha DESC LIMIT 10;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error accediendo al historial.\n");
        pause_console();
        return;
    }

    printf("\nSelecciona un consejo para evaluar:\n\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        time_t fecha = sqlite3_column_int64(stmt, 1);
        const char *consejo = (const char*)sqlite3_column_text(stmt, 2);
        int seguido = sqlite3_column_int(stmt, 3);

        struct tm tm_fecha;
        char fecha_str[20];
        localtime_s(&tm_fecha, &fecha);
        strftime(fecha_str, sizeof(fecha_str), "%Y-%m-%d", &tm_fecha);

        printf("%d. %s - %s [%s]\n",
               id, fecha_str, consejo,
               seguido ? "Seguido" : "Ignorado");
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        mostrar_no_hay_registros("consejos para evaluar");
        pause_console();
        return;
    }

    printf("\nSelecciona el ID del consejo: ");
    int id_seleccionado = input_int("");
    (void)id_seleccionado; // Evitar warning de variable no usada

    // Aquí se podría implementar lógica más compleja para evaluar el impacto
    printf("\nEvaluación completada. Esta funcionalidad se expandirá en futuras versiones.\n");

    pause_console();
}

// Configurar nivel de intervención
void configurar_nivel_intervencion()
{
    clear_screen();
    print_header("Configurar Nivel de Intervención IA");

    printf("\nNiveles de intervención disponibles:\n");
    printf("1. Conservador - Solo consejos críticos\n");
    printf("2. Moderado - Consejos de advertencia y críticos\n");
    printf("3. Agresivo - Todos los consejos\n\n");

    printf("Selecciona nivel (1-3): ");
    int nivel = input_int("");

    // Por ahora solo mostrar selección, se implementará en futuras versiones
    printf("\nNivel configurado: %d\n", nivel);
    printf("Esta funcionalidad se implementará completamente en futuras versiones.\n");

    pause_console();
}

// Guardar consejo en historial
void guardar_consejo_historial(const char *consejo, int seguido)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO consejos_historial (fecha, consejo, seguido) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
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
PerfilUsuarioIA obtener_perfil_usuario()
{
    PerfilUsuarioIA perfil = {0, 0, 0.5};
    sqlite3_stmt *stmt;
    const char *sql = "SELECT consejos_aceptados, consejos_ignorados, indice_prudencia FROM perfil_usuario_ia LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
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
    const char *sql_select = "SELECT consejos_aceptados, consejos_ignorados FROM perfil_usuario_ia LIMIT 1;";
    const char *sql_update = "UPDATE perfil_usuario_ia SET consejos_aceptados = ?, consejos_ignorados = ?, indice_prudencia = ? WHERE id = 1;";
    const char *sql_insert = "INSERT INTO perfil_usuario_ia (id, consejos_aceptados, consejos_ignorados, indice_prudencia) VALUES (1, ?, ?, ?);";

    int aceptados = 0;
    int ignorados = 0;

    // Obtener valores actuales
    if (sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0) == SQLITE_OK)
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

    // Calcular índice de prudencia
    float indice_prudencia;
    if (aceptados + ignorados == 0)
    {
        indice_prudencia = 0.5f;
    }
    else
    {
        indice_prudencia = (float)aceptados / (float)(aceptados + ignorados);
    }

    // Actualizar o insertar
    if (aceptados + ignorados > 1)   // Ya existe registro
    {
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, aceptados);
            sqlite3_bind_int(stmt, 2, ignorados);
            sqlite3_bind_double(stmt, 3, indice_prudencia);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else     // Primer registro
    {
        if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, aceptados);
            sqlite3_bind_int(stmt, 2, ignorados);
            sqlite3_bind_double(stmt, 3, indice_prudencia);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}

// Funciones de activación
void activar_ia_antes_partido()
{
    // Esta función se llamaría antes de crear un partido
    EstadoJugador estado = evaluar_estado_jugador();

    if (estado.riesgo_lesion > 2.5 || estado.cansancio_promedio > 8)
    {
        printf("\nIA: Alto riesgo detectado. ¿Deseas ver consejos antes de continuar? (s/n): ");
        int respuesta = getchar();
        while (getchar() != '\n');

        if (respuesta != EOF && (respuesta == 's' || respuesta == 'S'))
        {
            mostrar_consejos_actuales();
        }
    }
}

void activar_ia_antes_torneo()
{
    // Similar para torneos
    printf("\n🤖 IA: Analizando estado antes de torneo...\n");
    EstadoJugador estado = evaluar_estado_jugador();

    if (estado.partidos_consecutivos > 5)
    {
        printf("⚠️ IA: Muchos partidos consecutivos. Recomendado descansar antes del torneo.\n");
        pause_console();
    }
}

void activar_ia_estadisticas()
{
    // Se activa al abrir estadísticas
    PerfilUsuarioIA perfil = obtener_perfil_usuario();
    const char* tipo_usuario;
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
    printf("\n🤖 IA: Perfil de usuario - %s (Prudencia: %.1f%%)\n",
           tipo_usuario,
           perfil.indice_prudencia * 100);
}

// Menú principal de la IA
void menu_entrenador_ia()
{
    init_ia_tables();

    MenuItem items[] =
    {
        {1, "Ver consejos actuales", mostrar_consejos_actuales},
        {2, "Ver historial de consejos", mostrar_historial_consejos},
        {3, "Evaluar decisión pasada", evaluar_decision_pasada},
        {4, "Configurar nivel de intervención", configurar_nivel_intervencion},
        {0, "Volver al menú principal", NULL}
    };

    ejecutar_menu("Entrenador Virtual IA", items, 5);
}
