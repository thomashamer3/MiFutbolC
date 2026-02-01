/**
 * @file qr.h
 * @brief Sistema de generación de códigos QR para datos deportivos
 *
 * Proporciona funcionalidades para generar códigos QR que contienen información
 * de partidos, jugadores, temporadas y camisetas en formato JSON. Los códigos
 * generados se guardan como imágenes PNG para fácil compartición y escaneo.
 */

#ifndef QR_H
#define QR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== FUNCIONES PARA GENERACIÓN DE QR ==========

/**
 * @brief Genera un código QR con estadísticas de un partido
 *
 * Crea un archivo de imagen PNG con un código QR que contiene
 * las estadísticas completas del partido en formato JSON.
 *
 * @param partido_id ID del partido
 * @return 1 si se generó exitosamente, 0 en caso de error
 */
int generar_qr_partido(int partido_id);

/**
 * @brief Genera un código QR con estadísticas de un jugador en un partido
 *
 * @param partido_id ID del partido
 * @param jugador_id ID del jugador
 * @return 1 si se generó exitosamente, 0 en caso de error
 */
int generar_qr_jugador_partido(int partido_id, int jugador_id);

/**
 * @brief Genera un código QR con resumen de temporada
 *
 * @param temporada_id ID de la temporada
 * @return 1 si se generó exitosamente, 0 en caso de error
 */
int generar_qr_temporada(int temporada_id);

/**
 * @brief Genera un código QR con información de una camiseta
 *
 * @param camiseta_id ID de la camiseta
 * @return 1 si se generó exitosamente, 0 en caso de error
 */
int generar_qr_camiseta(int camiseta_id);

/**
 * @brief Muestra el menú de generación de códigos QR
 */
void menu_qr();

// ========== FUNCIONES AUXILIARES ==========

/**
 * @brief Obtiene las estadísticas de un partido en formato JSON
 *
 * @param partido_id ID del partido
 * @return Cadena JSON con las estadísticas (debe ser liberada con free())
 */
char* obtener_estadisticas_partido_json(int partido_id);

/**
 * @brief Obtiene las estadísticas de un jugador en un partido en formato JSON
 *
 * @param partido_id ID del partido
 * @param jugador_id ID del jugador
 * @return Cadena JSON con las estadísticas (debe ser liberada con free())
 */
char* obtener_estadisticas_jugador_json(int partido_id, int jugador_id);

/**
 * @brief Obtiene el resumen de temporada en formato JSON
 *
 * @param temporada_id ID de la temporada
 * @return Cadena JSON con el resumen (debe ser liberada con free())
 */
char* obtener_resumen_temporada_json(int temporada_id);

/**
 * @brief Obtiene la información de una camiseta en formato JSON
 *
 * @param camiseta_id ID de la camiseta
 * @return Cadena JSON con la información (debe ser liberada con free())
 */
char* obtener_info_camiseta_json(int camiseta_id);

/**
 * @brief Genera un código QR a partir de texto y lo guarda como PNG
 *
 * @param texto El texto a codificar en el QR
 * @param filename Nombre del archivo PNG de salida (sin extensión)
 * @return 1 si se generó exitosamente, 0 en caso de error
 */
int generar_qr_png(const char* texto, const char* filename);

#endif // QR_H
