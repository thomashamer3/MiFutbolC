/**
 * @file estadisticas_meta.h
 * @brief Declaraciones de funciones para estadísticas avanzadas y meta-análisis
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * el análisis avanzado de estadísticas de fútbol.
 */

#ifndef ESTADISTICAS_META_H
#define ESTADISTICAS_META_H

/**
 * @brief Muestra la consistencia del rendimiento (variabilidad)
 */
void mostrar_consistencia_rendimiento(void);

/**
 * @brief Muestra los partidos atípicos (muy buenos/muy malos)
 */
void mostrar_partidos_outliers(void);

/**
 * @brief Muestra la dependencia del contexto
 */
void mostrar_dependencia_contexto(void);

/**
 * @brief Muestra el impacto real del cansancio
 */
void mostrar_impacto_real_cansancio(void);

/**
 * @brief Muestra el impacto real del estado de ánimo
 */
void mostrar_impacto_real_estado_animo(void);

/**
 * @brief Muestra la eficiencia: goles por partido vs rendimiento
 */
void mostrar_eficiencia_goles_vs_rendimiento(void);

/**
 * @brief Muestra la eficiencia: asistencias por partido vs cansancio
 */
void mostrar_eficiencia_asistencias_vs_cansancio(void);

/**
 * @brief Muestra el rendimiento obtenido por esfuerzo
 */
void mostrar_rendimiento_por_esfuerzo(void);

/**
 * @brief Muestra el rendimiento agrupado por dolor/molestia física
 */
void mostrar_rendimiento_por_dolor_fisico(void);

/**
 * @brief Muestra el rendimiento agrupado por calidad de arbitraje
 */
void mostrar_rendimiento_por_arbitraje(void);

/**
 * @brief Muestra el rendimiento agrupado por temperatura registrada
 */
void mostrar_rendimiento_por_temperatura(void);

/** Grupos de lluvia: sin lluvia, leve, moderada/fuerte. */
#define CLIMA_REAL_GRUPOS_LLUVIA 3
/** Grupos de viento: calmo, moderado, fuerte. */
#define CLIMA_REAL_GRUPOS_VIENTO 3
/** Grupos de cielo (weather_code WMO): despejado, nublado, niebla, lluvia, nieve, tormenta. */
#define CLIMA_REAL_GRUPOS_CIELO 6
/** Partidos minimos que necesita un grupo para ser "la mejor condicion". */
#define CLIMA_REAL_MIN_PARTIDOS_MEJOR 3

/**
 * @brief Metricas de un grupo de condiciones reales de clima (Open-Meteo)
 *
 * Los contadores con_* indican cuantos partidos del grupo tienen la metrica
 * no NULL; si es 0, el promedio correspondiente no tiene sentido (N/A).
 */
typedef struct
{
    const char *etiqueta;
    int partidos;
    int con_rendimiento;
    double rendimiento_promedio;
    int con_goles;
    double goles_promedio;
    int con_asistencias;
    double asistencias_promedio;
    int con_resultado; /**< Partidos con resultado cargado (1, 2 o 3) */
    double victorias_pct;
} ClimaRealGrupo;

/**
 * @brief Resultado de agrupar los partidos por condiciones reales de clima
 *
 * Los grupos de cada arreglo estan en orden logico (de menos a mas lluvia,
 * viento, etc.). @c mejor apunta a un grupo de este mismo struct (no copiar
 * el struct si se va a usar @c mejor) o es NULL si ningun grupo llega a
 * CLIMA_REAL_MIN_PARTIDOS_MEJOR partidos con rendimiento.
 */
typedef struct
{
    int total_partidos;
    int partidos_con_datos; /**< Partidos con weather_code no NULL */
    double cobertura_pct;
    ClimaRealGrupo lluvia[CLIMA_REAL_GRUPOS_LLUVIA];
    ClimaRealGrupo viento[CLIMA_REAL_GRUPOS_VIENTO];
    ClimaRealGrupo cielo[CLIMA_REAL_GRUPOS_CIELO];
    const ClimaRealGrupo *mejor;
} ClimaRealStats;

/**
 * @brief Calcula el rendimiento agrupado por lluvia, viento y cielo reales
 *
 * No es interactiva: solo consulta la base abierta y llena @p stats.
 *
 * @param stats Destino del calculo
 * @return 1 si pudo consultar la base, 0 si @p stats es NULL o fallo una consulta
 */
int calcular_rendimiento_clima_real(ClimaRealStats *stats);

/**
 * @brief Igual que calcular_rendimiento_clima_real(), solo con los partidos de un anio
 *
 * El anio se toma de la fecha normalizada del partido (acepta "aaaa-mm-dd" y
 * "dd/mm/aaaa"). La cobertura tambien se calcula sobre los partidos del anio.
 *
 * @param stats Destino del calculo
 * @param anio Anio calendario; 0 equivale a todos los partidos
 * @return 1 si pudo consultar la base, 0 si @p stats es NULL, @p anio es negativo
 *         o fallo una consulta
 */
int calcular_rendimiento_clima_real_anio(ClimaRealStats *stats, int anio);

/**
 * @brief Muestra el rendimiento por condiciones reales de clima (Open-Meteo)
 */
void mostrar_rendimiento_por_clima_real(void);

/**
 * @brief Muestra partidos exigentes bien rendidos
 */
void mostrar_partidos_exigentes_bien_rendidos(void);

/**
 * @brief Muestra partidos fáciles mal rendidos
 */
void mostrar_partidos_faciles_mal_rendidos(void);

/**
 * @brief Calculadora de efectividad general: goles/partido, asistencias/partido, efectividad pase, conversion
 */
void mostrar_efectividad_general(void);

/**
 * @brief Muestra tendencia de rendimiento con sparklines en los ultimos 30 partidos
 */
void mostrar_tendencia_rendimiento_sparkline(void);

#endif
