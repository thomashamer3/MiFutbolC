/**
 * @file resumen_compartible.h
 * @brief API publica para generar resumenes compartibles
 *
 * Genera un resumen visual optimizado del perfil futbolistico
 * en formato HTML/Markdown listo para compartir en redes sociales.
 */

#ifndef RESUMEN_COMPARTIBLE_H
#define RESUMEN_COMPARTIBLE_H

#include <stdio.h>

/**
 * @brief Muestra el menu de resumenes compartibles
 */
void menu_resumen_compartible(void);

/**
 * @brief Genera un resumen HTML del perfil futbolistico
 */
void generar_resumen_html(void);

/**
 * @brief Genera un resumen Markdown del perfil
 */
void generar_resumen_markdown(void);

/**
 * @brief Genera un resumen de estadisticas destacadas
 */
void generar_resumen_estadisticas(void);

/**
 * @brief Genera un resumen de la mejor temporada
 */
void generar_resumen_mejor_temporada(void);

/**
 * @brief Genera el resumen anual en formato TXT (pide el año; Enter = año en curso)
 *
 * Incluye partidos, goles, asistencias, victorias, empates, derrotas,
 * camisetas, lesiones y canchas del año elegido.
 */
void generar_resumen_anio_txt(void);

/**
 * @brief Genera el resumen anual en formato HTML
 */
void generar_resumen_anio_html(void);

/**
 * @brief Genera el resumen anual en formato PDF
 */
void generar_resumen_anio_pdf(void);

/**
 * @brief Genera el resumen anual en TXT, HTML y PDF
 */
void generar_resumen_anio_todos(void);

/* ─── Resumen general: calculo separado de la escritura ─────────────────── */

/** Partidos que muestra la forma reciente (chips V/E/D). */
#define RESUMEN_ULTIMOS_N 10
/** Partidos que dibuja la tendencia de rendimiento (sparkline). */
#define RESUMEN_TENDENCIA_N 20
/** Elementos de cada ranking (camisetas, canchas, rivales). */
#define RESUMEN_TOP_N 3
/** Anios distintos que guarda la evolucion (los mas recientes). */
#define RESUMEN_MAX_ANIOS 12
/** Franjas horarias de la columna partido.dia (1 Madrugada .. 6 Noche). */
#define RESUMEN_FRANJAS 6

/** Un partido resumido para records, forma reciente y ultimo partido. */
typedef struct
{
    int id;
    char fecha[16];  /**< "dd/mm/aaaa" o "" si no hay partido */
    char rival[96];
    int goles;
    int asistencias;
    int resultado;   /**< 1 V, 2 E, 3 D, 0 sin cargar */
    int rendimiento; /**< 1..10, 0 sin cargar */
} ResumenPartidoCorto;

/** Un elemento de ranking con sus partidos y su % de victorias. */
typedef struct
{
    char nombre[128];
    int partidos;
    int con_resultado; /**< Partidos con resultado 1, 2 o 3 */
    int victorias;
    double victorias_pct; /**< Sobre con_resultado; 0 si no hay */
    int goles;
} ResumenTopItem;

typedef struct
{
    ResumenTopItem items[RESUMEN_TOP_N];
    int cantidad;
} ResumenTop;

/** Totales de un anio calendario. */
typedef struct
{
    int anio;
    int partidos;
    int goles;
    int asistencias;
    int victorias;
} ResumenAnioItem;

/** Partidos y victorias de un grupo (dia de la semana o franja horaria). */
typedef struct
{
    int partidos;
    int victorias;
} ResumenConteo;

/** Totales y efectividad de todos los partidos considerados. */
typedef struct
{
    int partidos;
    int goles;
    int asistencias;
    int victorias;
    int empates;
    int derrotas;
    int con_resultado;
    double efectividad_pct; /**< (3V + E) / (3 * con_resultado) */
    double goles_pp;
    double asistencias_pp;
    double ga_pp;
} ResumenGeneralTotales;

/** Produccion ofensiva: minutos, partidos con gol o asistencia y multigoles. */
typedef struct
{
    int partidos_con_minutos;
    int minutos_total;
    double ga_por_90; /**< Solo sobre partidos con minutos cargados */

    int partidos_con_gol;
    int partidos_con_asistencia;
    int dobletes;   /**< Partidos con exactamente 2 goles */
    int hat_tricks; /**< Partidos con 3 goles o mas */
} ResumenGeneralAtaque;

/** Mejores rachas historicas y la racha en curso. */
typedef struct
{
    int mejor_racha_victorias;
    int mejor_racha_invicto;
    int mejor_racha_marcando;
    int racha_victorias_actual;
    char racha_tipo; /**< 'P' positiva, 'N' negativa, 'X' neutral */
    int racha_longitud;
} ResumenGeneralRachas;

/** Partidos destacados y el ultimo jugado. */
typedef struct
{
    ResumenPartidoCorto max_goles;
    ResumenPartidoCorto max_asistencias;
    ResumenPartidoCorto mejor_rendimiento;
    ResumenPartidoCorto ultimo;
} ResumenGeneralRecords;

/** Forma reciente (chips V/E/D) y tendencia de rendimiento. */
typedef struct
{
    int ultimos_n;
    ResumenPartidoCorto ultimos[RESUMEN_ULTIMOS_N]; /**< Del mas viejo al mas nuevo */
    int tendencia_n;
    int tendencia[RESUMEN_TENDENCIA_N]; /**< Rendimiento, del mas viejo al mas nuevo */
} ResumenGeneralForma;

/** Promedios de rendimiento, animo y cansancio (con_* = partidos con el dato). */
typedef struct
{
    int con_rendimiento;
    double rendimiento_prom;
    int con_animo;
    double animo_prom;
    int con_cansancio;
    double cansancio_prom;
} ResumenGeneralPromedios;

/** Clima mas frecuente, cobertura de clima real y la mejor condicion. */
typedef struct
{
    char clima_frecuente[128];
    int clima_real_partidos;
    double clima_real_cobertura_pct;
    int hay_clima_mejor;
    char clima_mejor[64];
    int clima_mejor_partidos;
    double clima_mejor_rendimiento;
    double clima_mejor_victorias_pct;
} ResumenGeneralClima;

/** Datos de carrera que no dependen de los partidos filtrados. */
typedef struct
{
    char posicion[128];
    char torneo[128];
    int lesiones;
    int logros_total;
    int logros_completados;
} ResumenGeneralCarrera;

/**
 * @brief Todos los datos del "Resumen general" (opciones 1 HTML y 2 Markdown)
 *
 * Lo llena resumen_general_calcular() sin interaccion; los escritores solo
 * formatean. Los campos con_* indican cuantos partidos tienen el dato cargado:
 * si valen 0 el promedio correspondiente no se muestra.
 */
typedef struct
{
    char jugador[128];
    char fecha_gen[64];
    char primer_partido_fecha[16]; /**< "dd/mm/aaaa" o "" */

    ResumenGeneralTotales totales;
    ResumenGeneralAtaque ataque;
    ResumenGeneralRachas rachas;
    ResumenGeneralRecords records;
    ResumenGeneralForma forma;
    ResumenGeneralPromedios promedios;

    int num_anios;
    ResumenAnioItem anios[RESUMEN_MAX_ANIOS]; /**< En orden ascendente */

    ResumenTop camisetas;
    ResumenTop canchas;
    ResumenTop rivales;

    ResumenConteo dias_semana[7]; /**< 0 Domingo .. 6 Sabado */
    ResumenConteo franjas[RESUMEN_FRANJAS]; /**< Indice dia - 1 */

    ResumenGeneralClima clima;
    ResumenGeneralCarrera carrera;
} ResumenGeneral;

/**
 * @brief Calcula el resumen general sobre la base abierta
 *
 * No es interactiva. Ordena los partidos por fecha normalizada (acepta
 * "aaaa-mm-dd" y "dd/mm/aaaa") con desempate por hora e id.
 *
 * @param r Destino del calculo
 * @return 1 si pudo consultar la base, 0 si @p r es NULL o fallo la consulta
 */
int resumen_general_calcular(ResumenGeneral *r);

/**
 * @brief Calcula el resumen general solo con los partidos de un anio
 *
 * Mismo calculo que resumen_general_calcular() (totales, rachas, records,
 * forma, tendencia, promedios, rankings, dia/franja y clima real), filtrando
 * partidos, rankings y clima real por el anio de la fecha normalizada.
 * Con @p anio > 0 no se llenan los datos de carrera que no dependen del
 * anio: posicion, clima frecuente, torneo, lesiones y logros quedan vacios
 * o en 0 (el Resumen Anual tiene los suyos).
 *
 * @param r Destino del calculo
 * @param anio Anio calendario; 0 equivale a resumen_general_calcular()
 * @return 1 si pudo consultar la base, 0 si @p r es NULL, @p anio es negativo
 *         o fallo la consulta
 */
int resumen_general_calcular_anio(ResumenGeneral *r, int anio);

/**
 * @brief Escribe el resumen general como HTML autocontenido
 */
void resumen_general_escribir_html(FILE *f, const ResumenGeneral *r);

/**
 * @brief Escribe el resumen general como Markdown (tablas, sin HTML)
 */
void resumen_general_escribir_markdown(FILE *f, const ResumenGeneral *r);

/**
 * @brief Escribe la hoja de estilo comun de los resumenes HTML
 *
 * Tokens de color claro/oscuro (prefers-color-scheme), tipografia, hero,
 * paneles, cifras, filas, barras, chips V/E/D, graficos SVG e impresion.
 * Escribe solo el contenido, sin la etiqueta style. Pensada para que el
 * Resumen Anual reutilice el mismo estilo.
 *
 * @param f Archivo de salida
 */
void resumen_escribir_css_comun(FILE *f);

/* ─── Resumen anual: calculo separado de la escritura ───────────────────── */

/** Meses del resumen anual. */
#define RESUMEN_ANIO_MESES 12
/** Partidos del anio que dibuja la tendencia (los mas recientes si hay mas). */
#define RESUMEN_ANIO_TENDENCIA_MAX 60

/** Textos libres del mejor partido del anio. */
typedef struct
{
    char goles_detalle[512];
    char asistencias_detalle[512];
    char posicion_jugada[64];
    char formato_partido[64];
    char arbitraje[64];
    char comentario[512];
    char lo_mejor[512];
    char que_mejorar[512];
    char tags[256];
    char eventos_clave[512];
} ResumenAnioPartidoTextos;

/** Ratings y datos fisicos del mejor partido del anio. */
typedef struct
{
    int rating_tecnico;
    int rating_fisico;
    int rating_mental;
    int minutos_jugados;
    int intensidad;
    int esfuerzo_percibido;
    int cansancio;
    int estado_animo;
    int dolor_fisico;
} ResumenAnioPartidoFisico;

/** Clima y cancha del mejor partido del anio. */
typedef struct
{
    int codigo;
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
} ResumenAnioPartidoMeteo;

/** Todas las columnas del mejor partido del anio. */
typedef struct
{
    int id;
    char fecha_hora[64];
    char rival[128];
    char tipo_rival[64];
    int resultado;
    int goles;
    int asistencias;
    int goles_equipo; /**< -1 si no se cargo */
    int goles_rival;  /**< -1 si no se cargo */
    int diferencia_gol;
    int rendimiento_general;
    int tipo_partido;
    int tarjeta;
    char cancha[128];
    char camiseta[128];
    int arbitraje_score;
    ResumenAnioPartidoFisico fisico;
    ResumenAnioPartidoMeteo meteo;
    ResumenAnioPartidoTextos textos;
} ResumenAnioPartido;

/** Totales de un mes del anio. */
typedef struct
{
    int partidos;
    int goles;
    int asistencias;
    int victorias;
    int empates;
    int derrotas;
    int con_rendimiento;     /**< Partidos del mes con rendimiento cargado */
    double rendimiento_prom; /**< 0 si con_rendimiento es 0 */
} ResumenAnioMes;

/** Camisetas o canchas: activas hoy, usadas en el anio y la mas usada. */
typedef struct
{
    int activas;
    int usadas;
    char top[128];
    int top_partidos;
} ResumenAnioGrupo;

/** Totales de un anio para comparar con el anterior. */
typedef struct
{
    int anio;
    int partidos;
    int goles;
    int asistencias;
    double ga_pp;
    int con_resultado;
    double efectividad_pct;
    int con_rendimiento;
    double rendimiento_prom;
} ResumenAnioTotales;

/**
 * @brief Todos los datos del Resumen Anual (TXT, HTML y PDF)
 *
 * Lo llena resumen_anio_calcular() sin interaccion. @c g es el calculo del
 * resumen general filtrado por el anio (totales, efectividad, ataque,
 * rachas, records, forma de los ultimos 10, promedios, rankings, dia y
 * franja, clima real). @c anterior solo es valido si @c hay_anterior vale 1.
 */
typedef struct
{
    int anio;
    char jugador[128];
    char fecha_gen[64];

    ResumenGeneral g;

    int hay_anterior; /**< 1 si el anio anterior tiene partidos */
    ResumenAnioTotales anterior;

    ResumenAnioMes meses[RESUMEN_ANIO_MESES];
    int mejor_mes;    /**< 0..11 (mayor rendimiento promedio) o -1 */
    int mes_goleador; /**< 0..11 (mas goles + asistencias) o -1 */

    int tendencia_n;
    int tendencia[RESUMEN_ANIO_TENDENCIA_MAX]; /**< Rendimiento, en orden cronologico */

    int goles_favor;
    int goles_contra;
    int partidos_con_marcador;

    int mejor_id; /**< Partido destacado del anio, 0 si no hay */
    ResumenAnioPartido mejor;
    ResumenAnioGrupo camiseta;
    ResumenAnioGrupo cancha;
    int lesiones;
    int lesiones_activas;
} ResumenAnio;

/**
 * @brief Calcula el resumen de un anio sobre la base abierta
 *
 * No es interactiva. El mejor mes es el de mayor rendimiento promedio entre
 * los meses con al menos 2 partidos con rendimiento (si ninguno llega, entre
 * los que tienen 1); empata el de mas partidos y despues el primero.
 *
 * @param r Destino del calculo
 * @param anio Anio calendario (mayor que 0)
 * @return 1 si pudo consultar la base, 0 si @p r es NULL, @p anio no es valido
 *         o fallo la consulta
 */
int resumen_anio_calcular(ResumenAnio *r, int anio);

/**
 * @brief Escribe el resumen anual como texto plano UTF-8 con barras ASCII
 */
void resumen_anio_escribir_txt(FILE *f, const ResumenAnio *r);

/**
 * @brief Escribe el resumen anual como HTML autocontenido (estilo comun)
 */
void resumen_anio_escribir_html(FILE *f, const ResumenAnio *r);

/**
 * @brief Escribe el resumen anual como PDF A4 en @p ruta
 * @return 1 si se guardo, 0 si fallo
 */
int resumen_anio_escribir_pdf(const ResumenAnio *r, const char *ruta);

#endif
