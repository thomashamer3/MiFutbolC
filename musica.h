/**
 * @file musica.h
 * @brief Reproductor de musica integrado para MiFutbolC
 *
 * Proporciona reproduccion de pistas de audio mediante miniaudio.
 * Soporta lista de reproduccion, pausa, volumen y modos de repeticion.
 */

#ifndef MUSICA_H
#define MUSICA_H

/**
 * @brief Garantiza que exista la carpeta "Musica/".
 *
 * Si no existe al iniciar la aplicacion, la crea vacia para que el usuario
 * pueda copiar sus archivos de audio.
 */
void musica_asegurar_directorio(void);

/**
 * @brief Abre el menu del reproductor de musica.
 *
 * Inicializa el motor de audio en la primera llamada, escanea la
 * carpeta "Musica/" en busca de archivos compatibles y presenta la interfaz
 * interactiva de reproduccion.
 */
void menu_musica(void);

/**
 * @brief Inicia musica automaticamente al arranque de la aplicacion.
 *
 * Si hay archivos de audio en "Musica/", inicia reproduccion de la primera
 * pista sin mostrar el menu interactivo.
 */
void musica_iniciar_automatica(void);

/**
 * @brief Libera todos los recursos de audio.
 *
 * Detiene la reproduccion, descarga el sonido activo y apaga el motor
 * de miniaudio. Debe llamarse antes de salir de la aplicacion si se
 * uso el reproductor durante la sesion.
 */
void musica_cleanup(void);

#endif /* MUSICA_H */
