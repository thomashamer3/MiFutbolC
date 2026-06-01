#include "logros.h"
#include "utils.h"
#include "menu.h"
#include "db.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
    const char *nombre;
    const char *descripcion;
    int objetivo;
    const char *tipo; // tipos disponibles: "goles", "asistencias", "partidos", "goles+asistencias", "victorias", "empates", "derrotas", "rendimiento_general", "estado_animo", "canchas_distintas", "hat_tricks", "poker_asistencias", "rendimiento_perfecto", "animo_perfecto", "goles_victorias", "asistencias_victorias", "rendimiento_victorias", "animo_victorias", "goles_derrotas", "asistencias_derrotas", "rendimiento_empates", "animo_empates", y muchos mas (ver LOGRO_QUERIES)
} Logro;

typedef struct
{
    const char *tipo;
    const char *sql;
} LogroQuery;

typedef enum
{
    LOGRO_ESTADO_NO_INICIADO = 0,
    LOGRO_ESTADO_EN_PROGRESO = 1,
    LOGRO_ESTADO_COMPLETADO = 2
} LogroEstado;

typedef enum
{
    LOGRO_FILTRO_TODOS = 0,
    LOGRO_FILTRO_COMPLETADOS = 1,
    LOGRO_FILTRO_EN_PROGRESO = 2,
    LOGRO_FILTRO_NO_COMPLETADOS = 3
} LogroFiltro;

#define SQL_FROM_PARTIDO " FROM partido WHERE camiseta_id = ?"
#define SQL_SUM_EXPR(expr) "SELECT IFNULL(SUM(" expr "), 0)" SQL_FROM_PARTIDO
#define SQL_SUM(col) SQL_SUM_EXPR(col)
#define SQL_SUM_RESULTADO(col, resultado) SQL_SUM(col) " AND resultado = " #resultado
#define SQL_COUNT "SELECT COUNT(*)" SQL_FROM_PARTIDO
#define SQL_COUNT_WHERE(condicion) SQL_COUNT " AND " condicion
#define SQL_COUNT_DISTINCT(col) "SELECT COUNT(DISTINCT " col ")" SQL_FROM_PARTIDO
#define SQL_AVG_X10(col) "SELECT ROUND(IFNULL(AVG(" col "), 0) * 10)" SQL_FROM_PARTIDO
#define SQL_LAST(col) "SELECT IFNULL(" col ", 0)" SQL_FROM_PARTIDO " ORDER BY id DESC LIMIT 1"
#define SQL_RACHA_MAX(resultado) "SELECT COUNT(*) FROM (SELECT resultado, ROW_NUMBER() OVER (ORDER BY id) - ROW_NUMBER() OVER (PARTITION BY resultado ORDER BY id) as grp" SQL_FROM_PARTIDO ") WHERE resultado = " #resultado " GROUP BY grp ORDER BY COUNT(*) DESC LIMIT 1"

#define LOGRO_QUERY(tipo, sql) {tipo, sql}

static const LogroQuery LOGRO_QUERIES[] =
{
    LOGRO_QUERY("goles", SQL_SUM("goles")),
    LOGRO_QUERY("asistencias", SQL_SUM("asistencias")),
    LOGRO_QUERY("partidos", SQL_COUNT),
    LOGRO_QUERY("goles+asistencias", SQL_SUM_EXPR("goles + asistencias")),
    LOGRO_QUERY("victorias", SQL_COUNT_WHERE("resultado = 1")),
    LOGRO_QUERY("empates", SQL_COUNT_WHERE("resultado = 2")),
    LOGRO_QUERY("derrotas", SQL_COUNT_WHERE("resultado = 3")),
    LOGRO_QUERY("rendimiento_general", SQL_SUM("rendimiento_general")),
    LOGRO_QUERY("estado_animo", SQL_SUM("estado_animo")),
    LOGRO_QUERY("canchas_distintas", SQL_COUNT_DISTINCT("cancha_id")),
    LOGRO_QUERY("hat_tricks", SQL_COUNT_WHERE("goles >= 3")),
    LOGRO_QUERY("poker_asistencias", SQL_COUNT_WHERE("asistencias >= 4")),
    LOGRO_QUERY("rendimiento_perfecto", SQL_COUNT_WHERE("rendimiento_general = 10")),
    LOGRO_QUERY("animo_perfecto", SQL_COUNT_WHERE("estado_animo = 10")),
    LOGRO_QUERY("goles_victorias", SQL_SUM_RESULTADO("goles", 1)),
    LOGRO_QUERY("asistencias_victorias", SQL_SUM_RESULTADO("asistencias", 1)),
    LOGRO_QUERY("rendimiento_victorias", SQL_SUM_RESULTADO("rendimiento_general", 1)),
    LOGRO_QUERY("animo_victorias", SQL_SUM_RESULTADO("estado_animo", 1)),
    LOGRO_QUERY("goles_derrotas", SQL_SUM_RESULTADO("goles", 3)),
    LOGRO_QUERY("asistencias_derrotas", SQL_SUM_RESULTADO("asistencias", 3)),
    LOGRO_QUERY("rendimiento_empates", SQL_SUM_RESULTADO("rendimiento_general", 2)),
    LOGRO_QUERY("animo_empates", SQL_SUM_RESULTADO("estado_animo", 2)),
    // Nuevos tipos de logros
    LOGRO_QUERY("goles_empates", SQL_SUM_RESULTADO("goles", 2)),
    LOGRO_QUERY("asistencias_empates", SQL_SUM_RESULTADO("asistencias", 2)),
    LOGRO_QUERY("rendimiento_derrotas", SQL_SUM_RESULTADO("rendimiento_general", 3)),
    LOGRO_QUERY("animo_derrotas", SQL_SUM_RESULTADO("estado_animo", 3)),
    LOGRO_QUERY("partidos_sin_goles", SQL_COUNT_WHERE("goles = 0")),
    LOGRO_QUERY("partidos_sin_asistencias", SQL_COUNT_WHERE("asistencias = 0")),
    LOGRO_QUERY("partidos_con_goles", SQL_COUNT_WHERE("goles > 0")),
    LOGRO_QUERY("partidos_con_asistencias", SQL_COUNT_WHERE("asistencias > 0")),
    LOGRO_QUERY("partidos_con_contribucion", SQL_COUNT_WHERE("(goles > 0 OR asistencias > 0)")),
    LOGRO_QUERY("hat_tricks_dobles", SQL_COUNT_WHERE("goles >= 4")),
    LOGRO_QUERY("asistencias_dobles", SQL_COUNT_WHERE("asistencias >= 5")),
    LOGRO_QUERY("rendimiento_alto", SQL_COUNT_WHERE("rendimiento_general >= 8")),
    LOGRO_QUERY("animo_alto", SQL_COUNT_WHERE("estado_animo >= 8")),
    LOGRO_QUERY("rendimiento_bajo", SQL_COUNT_WHERE("rendimiento_general <= 3")),
    LOGRO_QUERY("animo_bajo", SQL_COUNT_WHERE("estado_animo <= 3")),
    LOGRO_QUERY("goles_por_partido_promedio", SQL_AVG_X10("goles")),
    LOGRO_QUERY("asistencias_por_partido_promedio", SQL_AVG_X10("asistencias")),
    LOGRO_QUERY("rendimiento_promedio", SQL_AVG_X10("rendimiento_general")),
    LOGRO_QUERY("animo_promedio", SQL_AVG_X10("estado_animo")),
    LOGRO_QUERY("partidos_con_rendimiento_alto", SQL_COUNT_WHERE("rendimiento_general >= 9")),
    LOGRO_QUERY("partidos_con_animo_alto", SQL_COUNT_WHERE("estado_animo >= 9")),
    LOGRO_QUERY("partidos_con_rendimiento_perfecto_y_animo", SQL_COUNT_WHERE("rendimiento_general = 10 AND estado_animo = 10")),
    LOGRO_QUERY("goles_en_primer_tiempo", SQL_SUM("goles")),
    LOGRO_QUERY("asistencias_en_segundo_tiempo", SQL_SUM("asistencias")),
    LOGRO_QUERY("victorias_consecutivas_max", SQL_RACHA_MAX(1)),
    LOGRO_QUERY("derrotas_consecutivas_max", SQL_RACHA_MAX(3)),
    LOGRO_QUERY("empates_consecutivos_max", SQL_RACHA_MAX(2)),
    LOGRO_QUERY("goles_en_ultimo_partido", SQL_LAST("goles")),
    LOGRO_QUERY("asistencias_en_ultimo_partido", SQL_LAST("asistencias")),
    LOGRO_QUERY("rendimiento_en_ultimo_partido", SQL_LAST("rendimiento_general")),
    LOGRO_QUERY("animo_en_ultimo_partido", SQL_LAST("estado_animo"))
};

#undef LOGRO_QUERY
#undef SQL_RACHA_MAX
#undef SQL_LAST
#undef SQL_AVG_X10
#undef SQL_COUNT_DISTINCT
#undef SQL_COUNT_WHERE
#undef SQL_COUNT
#undef SQL_SUM_RESULTADO
#undef SQL_SUM
#undef SQL_SUM_EXPR
#undef SQL_FROM_PARTIDO

#define NUM_QUERIES (sizeof(LOGRO_QUERIES) / sizeof(LogroQuery))

#define LOGRO_ITEM(nombre, descripcion, objetivo, tipo) {nombre, descripcion, objetivo, tipo}

#define LOGRO_SERIE_NIVELES_5(prefijo, accion, objeto, tipo, v1, v2, v3, v4, v5) \
    LOGRO_ITEM(prefijo " Novato", accion " " #v1 " " objeto, v1, tipo),         \
    LOGRO_ITEM(prefijo " Promedio", accion " " #v2 " " objeto, v2, tipo),        \
    LOGRO_ITEM(prefijo " Experto", accion " " #v3 " " objeto, v3, tipo),         \
    LOGRO_ITEM(prefijo " Maestro", accion " " #v4 " " objeto, v4, tipo),         \
    LOGRO_ITEM(prefijo " Leyenda", accion " " #v5 " " objeto, v5, tipo)

#define LOGRO_SERIE_NIVELES_4(prefijo, accion, objeto, tipo, v1, v2, v3, v4) \
    LOGRO_ITEM(prefijo " Novato", accion " " #v1 " " objeto, v1, tipo),      \
    LOGRO_ITEM(prefijo " Promedio", accion " " #v2 " " objeto, v2, tipo),     \
    LOGRO_ITEM(prefijo " Experto", accion " " #v3 " " objeto, v3, tipo),      \
    LOGRO_ITEM(prefijo " Maestro", accion " " #v4 " " objeto, v4, tipo)

#define LOGRO_SERIE_NIVELES_3(prefijo, accion, objeto, tipo, v1, v2, v3) \
    LOGRO_ITEM(prefijo " Novato", accion " " #v1 " " objeto, v1, tipo),    \
    LOGRO_ITEM(prefijo " Promedio", accion " " #v2 " " objeto, v2, tipo),   \
    LOGRO_ITEM(prefijo " Experto", accion " " #v3 " " objeto, v3, tipo)

#define LOGRO_SERIE_NIVELES_2(prefijo, accion, objeto, tipo, v1, v2) \
    LOGRO_ITEM(prefijo " Novato", accion " " #v1 " " objeto, v1, tipo), \
    LOGRO_ITEM(prefijo " Promedio", accion " " #v2 " " objeto, v2, tipo)

#define LOGRO_EXPERTO(nombre_base, descripcion, objetivo, tipo) LOGRO_ITEM(nombre_base " Experto", descripcion, objetivo, tipo)
#define LOGRO_EXPERTA(nombre_base, descripcion, objetivo, tipo) LOGRO_ITEM(nombre_base " Experta", descripcion, objetivo, tipo)
#define LOGRO_PAR(nombre1, desc1, obj1, nombre2, desc2, obj2, tipo) \
    LOGRO_ITEM(nombre1, desc1, obj1, tipo),                         \
    LOGRO_ITEM(nombre2, desc2, obj2, tipo)
#define LOGRO_RACHAS(sufijo, objetivo)                                                                     \
    LOGRO_ITEM("Racha de Victorias" sufijo, "Ganar " #objetivo " partidos consecutivos", objetivo, "victorias_consecutivas_max"), \
    LOGRO_ITEM("Racha de Derrotas" sufijo, "Perder " #objetivo " partidos consecutivos", objetivo, "derrotas_consecutivas_max"),   \
    LOGRO_ITEM("Racha de Empates" sufijo, "Empatar " #objetivo " partidos consecutivos", objetivo, "empates_consecutivos_max")

static const Logro LOGROS[] =
{
    LOGRO_ITEM("Primer Gol", "Anotar tu primer gol", 1, "goles"),
    LOGRO_SERIE_NIVELES_5("Goleador", "Anotar", "goles", "goles", 5, 10, 25, 50, 100),
    LOGRO_ITEM("Primera Asistencia", "Dar tu primera asistencia", 1, "asistencias"),
    LOGRO_SERIE_NIVELES_5("Asistente", "Dar", "asistencias", "asistencias", 5, 10, 25, 50, 100),
    LOGRO_ITEM("Debutante", "Jugar tu primer partido", 1, "partidos"),
    LOGRO_ITEM("Jugador Regular", "Jugar 5 partidos", 5, "partidos"),
    LOGRO_ITEM("Jugador Estrella", "Jugar 10 partidos", 10, "partidos"),
    LOGRO_ITEM("Jugador Veterano", "Jugar 25 partidos", 25, "partidos"),
    LOGRO_ITEM("Jugador Maestro", "Jugar 50 partidos", 50, "partidos"),
    LOGRO_ITEM("Jugador Leyenda", "Jugar 100 partidos", 100, "partidos"),
    LOGRO_SERIE_NIVELES_5("Contribuidor", "Acumular", "puntos (goles + asistencias)", "goles+asistencias", 10, 25, 50, 100, 250),
    // Victories
    LOGRO_ITEM("Primera Victoria", "Ganar tu primer partido", 1, "victorias"),
    LOGRO_SERIE_NIVELES_5("Ganador", "Ganar", "partidos", "victorias", 5, 10, 25, 50, 100),
    // Draws
    LOGRO_ITEM("Primer Empate", "Empatar tu primer partido", 1, "empates"),
    LOGRO_SERIE_NIVELES_5("Empatador", "Empatar", "partidos", "empates", 5, 10, 25, 50, 100),
    // Losses
    LOGRO_ITEM("Primera Derrota", "Perder tu primer partido", 1, "derrotas"),
    LOGRO_SERIE_NIVELES_5("Perdedor", "Perder", "partidos", "derrotas", 5, 10, 25, 50, 100),
    // General Performance
    LOGRO_ITEM("Rendimiento Inicial", "Acumular 10 puntos de rendimiento general", 10, "rendimiento_general"),
    LOGRO_SERIE_NIVELES_5("Rendimiento", "Acumular", "puntos de rendimiento general", "rendimiento_general", 50, 100, 250, 500, 1000),
    // Mood
    LOGRO_ITEM("Animo Inicial", "Acumular 10 puntos de estado de Animo", 10, "estado_animo"),
    LOGRO_SERIE_NIVELES_5("Animo", "Acumular", "puntos de estado de Animo", "estado_animo", 50, 100, 250, 500, 1000),
    // Distinct Pitches
    LOGRO_ITEM("Explorador de Canchas", "Jugar en 1 cancha distinta", 1, "canchas_distintas"),
    LOGRO_SERIE_NIVELES_4("Viajero", "Jugar en", "canchas distintas", "canchas_distintas", 5, 10, 25, 50),
    // Hat-Tricks
    LOGRO_ITEM("Primer Hat-Trick", "Anotar 3 o mas goles en un partido", 1, "hat_tricks"),
    LOGRO_SERIE_NIVELES_3("Hat-Tricker", "Anotar 3 o mas goles en", "partidos", "hat_tricks", 5, 10, 25),
    // Poker Assists
    LOGRO_ITEM("Primer Poker de Asistencias", "Dar 4 o mas asistencias en un partido", 1, "poker_asistencias"),
    LOGRO_SERIE_NIVELES_2("Poker Asistente", "Dar 4 o mas asistencias en", "partidos", "poker_asistencias", 5, 10),
    // Perfect Performance
    LOGRO_ITEM("Primer Rendimiento Perfecto", "Obtener rendimiento perfecto (10) en un partido", 1, "rendimiento_perfecto"),
    LOGRO_SERIE_NIVELES_3("Rendimiento Perfecto", "Obtener rendimiento perfecto en", "partidos", "rendimiento_perfecto", 5, 10, 25),
    // Perfect Mood
    LOGRO_ITEM("Primer Animo Perfecto", "Obtener animo perfecto (10) en un partido", 1, "animo_perfecto"),
    LOGRO_SERIE_NIVELES_3("Animo Perfecto", "Obtener animo perfecto en", "partidos", "animo_perfecto", 5, 10, 25),
    // Victory Achievements
    LOGRO_ITEM("Goleador Victorioso", "Anotar 10 goles en partidos ganados", 10, "goles_victorias"),
    LOGRO_ITEM("Asistente Victorioso", "Dar 10 asistencias en partidos ganados", 10, "asistencias_victorias"),
    LOGRO_ITEM("Rendimiento Victorioso", "Acumular 50 puntos de rendimiento en victorias", 50, "rendimiento_victorias"),
    LOGRO_ITEM("Animo Victorioso", "Acumular 50 puntos de animo en victorias", 50, "animo_victorias"),
    // Loss Achievements
    LOGRO_ITEM("Goleador en Derrotas", "Anotar 5 goles en partidos perdidos", 5, "goles_derrotas"),
    LOGRO_ITEM("Asistente en Derrotas", "Dar 5 asistencias en partidos perdidos", 5, "asistencias_derrotas"),
    // Draw Achievements
    LOGRO_ITEM("Rendimiento en Empates", "Acumular 25 puntos de rendimiento en empates", 25, "rendimiento_empates"),
    LOGRO_ITEM("Animo en Empates", "Acumular 25 puntos de animo en empates", 25, "animo_empates"),
    // Additional Achievements
    LOGRO_ITEM("Gol en Victoria", "Anotar en 5 partidos ganados", 5, "goles_victorias"),
    LOGRO_ITEM("Asistencia Clave", "Asistir en 5 partidos ganados", 5, "asistencias_victorias"),
    LOGRO_ITEM("Presente en la Derrota", "Anotar en 5 partidos perdidos", 5, "goles_derrotas"),
    LOGRO_ITEM("Asistencia en Derrota", "Asistir en 5 partidos perdidos", 5, "asistencias_derrotas"),
    // New Achievements for Draws
    LOGRO_ITEM("Primer Gol en Empate", "Anotar tu primer gol en un empate", 1, "goles_empates"),
    LOGRO_ITEM("Goleador en Empates", "Anotar 5 goles en empates", 5, "goles_empates"),
    LOGRO_ITEM("Asistente en Empates", "Dar 5 asistencias en empates", 5, "asistencias_empates"),
    LOGRO_ITEM("Contribuidor en Empates", "Acumular 10 puntos en empates", 10, "goles_empates"),
    // New Achievements for Losses
    LOGRO_ITEM("Rendimiento en Derrotas", "Acumular 50 puntos de rendimiento en derrotas", 50, "rendimiento_derrotas"),
    LOGRO_ITEM("Animo en Derrotas", "Acumular 50 puntos de animo en derrotas", 50, "animo_derrotas"),
    // No Contribution Achievements
    LOGRO_PAR("Primer Partido Sin Goles", "Jugar un partido sin anotar", 1,
              "5 Partidos Sin Goles", "Jugar 5 partidos sin anotar", 5,
              "partidos_sin_goles"),
    LOGRO_PAR("Primer Partido Sin Asistencias", "Jugar un partido sin asistir", 1,
              "5 Partidos Sin Asistencias", "Jugar 5 partidos sin asistir", 5,
              "partidos_sin_asistencias"),
    // Contribution Achievements
    LOGRO_PAR("Primer Gol Anotado", "Anotar en un partido", 1,
              "5 Partidos con Goles", "Anotar en 5 partidos", 5,
              "partidos_con_goles"),
    LOGRO_PAR("Primer Asistencia Dada", "Asistir en un partido", 1,
              "5 Partidos con Asistencias", "Asistir en 5 partidos", 5,
              "partidos_con_asistencias"),
    LOGRO_PAR("Contribuidor Inicial", "Contribuir en un partido", 1,
              "Contribuidor Regular", "Contribuir en 10 partidos", 10,
              "partidos_con_contribucion"),
    // Advanced Scoring
    LOGRO_PAR("Primer Hat-Trick Doble", "Anotar 4 o mas goles en un partido", 1,
              "Hat-Tricker Doble Novato", "Anotar 4 o mas goles en 3 partidos", 3,
              "hat_tricks_dobles"),
    LOGRO_PAR("Primer Poker de Asistencias Doble", "Dar 5 o mas asistencias en un partido", 1,
              "Poker Asistente Doble Novato", "Dar 5 o mas asistencias en 3 partidos", 3,
              "asistencias_dobles"),
    // High Performance
    LOGRO_PAR("Rendimiento Alto Inicial", "Obtener rendimiento >=8 en un partido", 1,
              "Rendimiento Alto Regular", "Obtener rendimiento >=8 en 10 partidos", 10,
              "rendimiento_alto"),
    LOGRO_PAR("Animo Alto Inicial", "Obtener animo >=8 en un partido", 1,
              "Animo Alto Regular", "Obtener animo >=8 en 10 partidos", 10,
              "animo_alto"),
    // Low Performance
    LOGRO_ITEM("Rendimiento Bajo", "Obtener rendimiento <=3 en un partido", 1, "rendimiento_bajo"),
    LOGRO_ITEM("Animo Bajo", "Obtener animo <=3 en un partido", 1, "animo_bajo"),
    // Average Achievements (Note: These use multiplied values, so objectives are *10)
    LOGRO_ITEM("Promedio Goleador", "Mantener promedio de 0.5 goles por partido", 5, "goles_por_partido_promedio"),
    LOGRO_ITEM("Promedio Asistente", "Mantener promedio de 0.5 asistencias por partido", 5, "asistencias_por_partido_promedio"),
    LOGRO_ITEM("Promedio Rendimiento Alto", "Mantener promedio de rendimiento >=7", 70, "rendimiento_promedio"),
    LOGRO_ITEM("Promedio Animo Alto", "Mantener promedio de animo >=7", 70, "animo_promedio"),
    // Near Perfect
    LOGRO_ITEM("Rendimiento Cercano a Perfecto", "Obtener rendimiento >=9 en un partido", 1, "partidos_con_rendimiento_alto"),
    LOGRO_ITEM("Animo Cercano a Perfecto", "Obtener animo >=9 en un partido", 1, "partidos_con_animo_alto"),
    LOGRO_ITEM("Dia Perfecto", "Obtener rendimiento y animo perfectos en un partido", 1, "partidos_con_rendimiento_perfecto_y_animo"),
    // Placeholder for time-based (since no time columns)
    LOGRO_ITEM("Goleador en Primer Tiempo", "Anotar 10 goles (simulado)", 10, "goles_en_primer_tiempo"),
    LOGRO_ITEM("Asistente en Segundo Tiempo", "Dar 10 asistencias (simulado)", 10, "asistencias_en_segundo_tiempo"),
    // Streak Achievements (May not work with standard SQLite)
    LOGRO_RACHAS("", 3),
    // Last Match Achievements
    LOGRO_ITEM("Ultimo Gol", "Anotar en el ultimo partido", 1, "goles_en_ultimo_partido"),
    LOGRO_ITEM("ultima Asistencia", "Asistir en el ultimo partido", 1, "asistencias_en_ultimo_partido"),
    LOGRO_ITEM("Ultimo Rendimiento Perfecto", "Rendimiento perfecto en el ultimo partido", 10, "rendimiento_en_ultimo_partido"),
    LOGRO_ITEM("Ultimo Animo Perfecto", "Animo perfecto en el ultimo partido", 10, "animo_en_ultimo_partido"),
    // More Tiered Achievements
    LOGRO_EXPERTO("Goleador en Empates", "Anotar 10 goles en empates", 10, "goles_empates"),
    LOGRO_EXPERTO("Asistente en Empates", "Dar 10 asistencias en empates", 10, "asistencias_empates"),
    LOGRO_EXPERTO("Rendimiento en Derrotas", "Acumular 100 puntos de rendimiento en derrotas", 100, "rendimiento_derrotas"),
    LOGRO_EXPERTO("Animo en Derrotas", "Acumular 100 puntos de animo en derrotas", 100, "animo_derrotas"),
    LOGRO_ITEM("10 Partidos Sin Goles", "Jugar 10 partidos sin anotar", 10, "partidos_sin_goles"),
    LOGRO_ITEM("10 Partidos Sin Asistencias", "Jugar 10 partidos sin asistir", 10, "partidos_sin_asistencias"),
    LOGRO_ITEM("10 Partidos con Goles", "Anotar en 10 partidos", 10, "partidos_con_goles"),
    LOGRO_ITEM("10 Partidos con Asistencias", "Asistir en 10 partidos", 10, "partidos_con_asistencias"),
    LOGRO_ITEM("Contribuidor Avanzado", "Contribuir en 25 partidos", 25, "partidos_con_contribucion"),
    LOGRO_EXPERTO("Hat-Tricker Doble", "Anotar 4 o mas goles en 10 partidos", 10, "hat_tricks_dobles"),
    LOGRO_EXPERTO("Poker Asistente Doble", "Dar 5 o mas asistencias en 10 partidos", 10, "asistencias_dobles"),
    LOGRO_EXPERTO("Rendimiento Alto", "Obtener rendimiento >=8 en 25 partidos", 25, "rendimiento_alto"),
    LOGRO_EXPERTO("Animo Alto", "Obtener animo >=8 en 25 partidos", 25, "animo_alto"),
    LOGRO_EXPERTO("Rendimiento Bajo", "Obtener rendimiento <=3 en 5 partidos", 5, "rendimiento_bajo"),
    LOGRO_EXPERTO("Animo Bajo", "Obtener animo <=3 en 5 partidos", 5, "animo_bajo"),
    LOGRO_EXPERTO("Promedio Goleador", "Mantener promedio de 1 gol por partido", 10, "goles_por_partido_promedio"),
    LOGRO_EXPERTO("Promedio Asistente", "Mantener promedio de 1 asistencia por partido", 10, "asistencias_por_partido_promedio"),
    LOGRO_EXPERTO("Rendimiento Cercano a Perfecto", "Obtener rendimiento >=9 en 10 partidos", 10, "partidos_con_rendimiento_alto"),
    LOGRO_EXPERTO("Animo Cercano a Perfecto", "Obtener animo >=9 en 10 partidos", 10, "partidos_con_animo_alto"),
    LOGRO_EXPERTO("Dia Perfecto", "Obtener rendimiento y animo perfectos en 5 partidos", 5, "partidos_con_rendimiento_perfecto_y_animo"),
    LOGRO_RACHAS(" Experta", 5)
};

#undef LOGRO_RACHAS
#undef LOGRO_PAR
#undef LOGRO_EXPERTA
#undef LOGRO_EXPERTO
#undef LOGRO_SERIE_NIVELES_2
#undef LOGRO_SERIE_NIVELES_3
#undef LOGRO_SERIE_NIVELES_4
#undef LOGRO_SERIE_NIVELES_5
#undef LOGRO_ITEM

#define NUM_LOGROS (sizeof(LOGROS) / sizeof(Logro))

static int s_logros_progreso[NUM_LOGROS];
static int s_logros_camiseta_id = -1;
static int s_logros_changes     = -1;

static const char *buscar_sql_logro(const char *tipo)
{
    for (size_t i = 0; i < NUM_QUERIES; i++)
    {
        if (strcmp(tipo, LOGRO_QUERIES[i].tipo) == 0)
        {
            return LOGRO_QUERIES[i].sql;
        }
    }

    return NULL;
}

static int ejecutar_consulta_progreso(const char *sql, int camiseta_id)
{
    sqlite3_stmt *stmt;
    int progreso = 0;

    if (!db_prepare_stmt_with_error(&stmt, sql, "Error al preparar la consulta"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, camiseta_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        progreso = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return progreso;
}

static int calcular_estado_logro(int progreso, int objetivo)
{
    if (progreso >= objetivo)
    {
        return LOGRO_ESTADO_COMPLETADO;
    }
    if (progreso > 0)
    {
        return LOGRO_ESTADO_EN_PROGRESO;
    }
    return LOGRO_ESTADO_NO_INICIADO;
}

static int filtro_permite_estado(int filtro, int estado)
{
    switch ((LogroFiltro)filtro)
    {
    case LOGRO_FILTRO_COMPLETADOS:
        return estado == LOGRO_ESTADO_COMPLETADO;
    case LOGRO_FILTRO_EN_PROGRESO:
        return estado == LOGRO_ESTADO_EN_PROGRESO;
    case LOGRO_FILTRO_NO_COMPLETADOS:
        return estado != LOGRO_ESTADO_COMPLETADO;
    case LOGRO_FILTRO_TODOS:
    default:
        return 1;
    }
}

static int obtener_progreso_logro(int camiseta_id, const char *tipo)
{
    const char *sql = buscar_sql_logro(tipo);
    if (!sql)
    {
        return 0; // Tipo no encontrado
    }

    return ejecutar_consulta_progreso(sql, camiseta_id);
}

static int obtener_estado_logro(int camiseta_id, const Logro *logro, int *progreso)
{
    *progreso = obtener_progreso_logro(camiseta_id, logro->tipo);
    return calcular_estado_logro(*progreso, logro->objetivo);
}

static void obtener_nombre_camiseta(int camiseta_id, char *nombre)
{
    obtener_nombre_entidad("camiseta", camiseta_id, nombre, 256);
}

static void mostrar_logro_individual(const Logro *logro, int estado, int progreso)
{
    const char *estado_texto;
    const char *color;

    switch ((LogroEstado)estado)
    {
    case LOGRO_ESTADO_NO_INICIADO:
        estado_texto = "[NO INICIADO]";
        color = "\x1b[31m"; // rojo
        break;
    case LOGRO_ESTADO_EN_PROGRESO:
        estado_texto = "[EN PROGRESO]";
        color = "\x1b[33m"; // amarillo
        break;
    case LOGRO_ESTADO_COMPLETADO:
        estado_texto = "[COMPLETADO]";
        color = "\x1b[32m"; // Verde
        break;
    default:
        estado_texto = "[DESCONOCIDO]";
        color = "\x1b[37m"; // blanco
        break;
    }
    // ARREGLAR COLOR CONSOLA
    ui_printf_centered_line("%s%s %s\x1b[0m", color, logro->nombre, estado_texto);
    ui_printf_centered_line("%s", logro->descripcion);
    ui_printf_centered_line("Progreso: %d/%d", progreso, logro->objetivo);
    ui_printf("\n");
}

static void mostrar_logros_camiseta(int camiseta_id, int filtro)
{
    char nombre_camiseta[100];
    obtener_nombre_camiseta(camiseta_id, nombre_camiseta);

    if (safe_strnlen(nombre_camiseta, sizeof(nombre_camiseta)) == 0) return; // Error already printed

    ui_printf_centered_line("LOGROS DE: %s", nombre_camiseta);
    ui_printf_centered_line("========================================");
    ui_printf("\n");

    int current_changes = sqlite3_total_changes(db);
    int cache_hit = (s_logros_camiseta_id == camiseta_id && s_logros_changes == current_changes);

    if (!cache_hit)
    {
        for (size_t i = 0; i < NUM_LOGROS; i++)
            s_logros_progreso[i] = obtener_progreso_logro(camiseta_id, LOGROS[i].tipo);
        s_logros_camiseta_id = camiseta_id;
        s_logros_changes     = current_changes;
    }

    int mostrados = 0;

    for (size_t i = 0; i < NUM_LOGROS; i++)
    {
        int progreso = s_logros_progreso[i];
        int estado   = calcular_estado_logro(progreso, LOGROS[i].objetivo);

        if (!filtro_permite_estado(filtro, estado))
            continue;

        mostrados++;
        mostrar_logro_individual(&LOGROS[i], estado, progreso);
    }

    if (mostrados == 0)
    {
        mostrar_no_hay_registros("logros que mostrar con el filtro seleccionado");
    }
}

static void listar_camisetas_con_partidos()
{
    ui_printf_centered_line("Camisetas disponibles:");
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt_with_error(&stmt,
                                    "SELECT DISTINCT c.id, c.nombre FROM camiseta c INNER JOIN partido p ON c.id = p.camiseta_id ORDER BY c.id",
                                    "Error al preparar la consulta"))
    {
        pause_console();
        return;
    }
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d | %s", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1));
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
    {
        mostrar_no_hay_registros("camisetas cargadas");
        pause_console();
    }
}

static int seleccionar_camiseta()
{
    int camiseta_id = input_int("ID de la camiseta,(0 para Cancelar): ");
    if (!existe_id("camiseta", camiseta_id))
    {
        printf("La camiseta no existe.\n");
        return -1;
    }
    return camiseta_id;
}

static void mostrar_logros_con_filtro(const char *titulo, int filtro)
{
    clear_screen();
    print_header(titulo);
    listar_camisetas_con_partidos();
    int camiseta_id = seleccionar_camiseta();
    if (camiseta_id == -1) return;
    mostrar_logros_camiseta(camiseta_id, filtro);
    pause_console();
}

#define LOGROS_FILTROS(X)                                                                                         \
    X(1, mostrar_todos_logros, "TODOS LOS LOGROS", LOGRO_FILTRO_TODOS, "Ver Todos los Logros")               \
    X(2, mostrar_logros_completados, "LOGROS COMPLETADOS", LOGRO_FILTRO_COMPLETADOS, "Logros Completados")   \
    X(3, mostrar_logros_en_progreso, "LOGROS EN PROGRESO", LOGRO_FILTRO_EN_PROGRESO, "Logros en Progreso")   \
    X(4, mostrar_logros_no_completados, "LOGROS NO COMPLETADOS", LOGRO_FILTRO_NO_COMPLETADOS, "Logros No Completados")

void mostrar_todos_logros()
{
    mostrar_logros_con_filtro("TODOS LOS LOGROS", LOGRO_FILTRO_TODOS);
}

void mostrar_logros_completados()
{
    mostrar_logros_con_filtro("LOGROS COMPLETADOS", LOGRO_FILTRO_COMPLETADOS);
}

void mostrar_logros_en_progreso()
{
    mostrar_logros_con_filtro("LOGROS EN PROGRESO", LOGRO_FILTRO_EN_PROGRESO);
}

void mostrar_logros_no_completados()
{
    mostrar_logros_con_filtro("LOGROS NO COMPLETADOS", LOGRO_FILTRO_NO_COMPLETADOS);
}

void menu_logros()
{
    MenuItem items[] =
    {
#define MENU_ITEM_LOGROS(id, fn, titulo, filtro, etiqueta) {id, etiqueta, fn},
        LOGROS_FILTROS(MENU_ITEM_LOGROS)
#undef MENU_ITEM_LOGROS
        {
            0, "Volver", NULL
        }
    };

    ejecutar_menu("LOGROS", items, (int)(sizeof(items) / sizeof(items[0])));
}

int logros_get_total(void)
{
    return (int)NUM_LOGROS;
}

int logros_get_completados_primera_camiseta(void)
{
    sqlite3_stmt *stmt;
    int camiseta_id = -1;

    if (db_prepare_stmt_with_error(&stmt,
                                   "SELECT camiseta_id FROM partido "
                                   "GROUP BY camiseta_id ORDER BY COUNT(*) DESC LIMIT 1;",
                                   "Error al preparar la consulta"))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            camiseta_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (camiseta_id < 0)
    {
        return 0;
    }

    int completados = 0;
    for (size_t i = 0; i < NUM_LOGROS; i++)
    {
        int progreso = 0;
        if (obtener_estado_logro(camiseta_id, &LOGROS[i], &progreso) == LOGRO_ESTADO_COMPLETADO)
        {
            completados++;
        }
    }
    return completados;
}
