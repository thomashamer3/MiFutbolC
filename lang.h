/**
 * @file lang.h
 * @brief Sistema de internacionalización (i18n) mediante archivos .lang en JSON
 *
 * Archivos JSON en lang/ con estructura:
 * { "strings": { "clave": "traducción", ... } }
 */

#ifndef LANG_H
#define LANG_H

#define LANG_CODE_SIZE 16
#define LANG_MAX_KEYS 2048

/**
 * @brief Inicializa el sistema de idiomas
 *
 * Busca langs/<codigo>.json en el directorio de trabajo o en el
 * directorio de la aplicación. Si no encuentra el archivo, carga
 * un conjunto mínimo de traducciones en inglés como fallback.
 *
 * @return 1 si se cargó un archivo, 0 si se usó fallback
 */
int lang_init(void);

/**
 * @brief Cambia el idioma activo
 *
 * Carga langs/<codigo>.json. Si falla, mantiene el idioma anterior.
 *
 * @param lang_code Código ISO 639-1 (ej: "es", "en")
 */
void lang_set(const char *lang_code);

/**
 * @brief Obtiene el código del idioma actual
 * @return Código ISO ("es", "en", ...)
 */
const char *lang_get_current(void);

/**
 * @brief Traduce una clave al idioma actual
 *
 * Busca key en las traducciones cargadas. Si no la encuentra,
 * devuelve la propia key como fallback.
 *
 * @param key Clave de traducción
 * @return Texto traducido o la key si no se encuentra
 */
const char *tr(const char *key);

/**
 * @brief Libera recursos del sistema de idiomas
 */
void lang_cleanup(void);

#endif /* LANG_H */
