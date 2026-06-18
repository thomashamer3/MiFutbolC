/**
 * @file media.h
 * @brief Referencias a medios externos (videos, fotos, articulos, audio)
 *
 * No almacena archivos, solo referencias (URLs) a YouTube, Google Drive,
 * etc. que documentan la carrera futbolistica.
 */

#ifndef MEDIA_H
#define MEDIA_H

/**
 * @brief Menu principal del modulo de referencias multimedia
 */
void menu_media(void);

/**
 * @brief Agrega una nueva referencia multimedia
 */
void media_crear(void);

/**
 * @brief Lista todas las referencias multimedia
 */
void media_listar(void);

/**
 * @brief Edita una referencia multimedia existente
 */
void media_editar(void);

/**
 * @brief Elimina una referencia multimedia
 */
void media_eliminar(void);

/**
 * @brief Filtra referencias por tipo (Video, Foto, Articulo, Audio)
 */
void media_filtrar_por_tipo(void);

#endif
