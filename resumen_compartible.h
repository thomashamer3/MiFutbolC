/**
 * @file resumen_compartible.h
 * @brief API publica para generar resumenes compartibles
 *
 * Genera un resumen visual optimizado del perfil futbolistico
 * en formato HTML/Markdown listo para compartir en redes sociales.
 */

#ifndef RESUMEN_COMPARTIBLE_H
#define RESUMEN_COMPARTIBLE_H

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
 * @brief Genera el resumen del año actual en formato TXT
 *
 * Incluye partidos, goles, asistencias, victorias, empates, derrotas,
 * camisetas, lesiones y canchas del año en curso.
 */
void generar_resumen_anio_txt(void);

/**
 * @brief Genera el resumen del año actual en formato HTML
 */
void generar_resumen_anio_html(void);

/**
 * @brief Genera el resumen del año actual en formato PDF
 */
void generar_resumen_anio_pdf(void);

/**
 * @brief Genera el resumen del año actual en TXT, HTML y PDF
 */
void generar_resumen_anio_todos(void);

#endif
