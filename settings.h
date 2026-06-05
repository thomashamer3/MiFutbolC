/**
 * @file settings.h
 * @brief Declaraciones para el sistema de configuración avanzada
 *
 * Incluye temas de interfaz, internacionalización y otras preferencias.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdio.h>

// Enumeraciones para configuraciones
typedef enum
{
    THEME_LIGHT = 0,
    THEME_DARK = 1,
    THEME_BLUE = 2,
    THEME_GREEN = 3,
    THEME_RED = 4,
    THEME_PURPLE = 5,
    THEME_CLASSIC = 6,
    THEME_HIGH_CONTRAST = 7
} ThemeType;

typedef enum
{
    TEXT_SIZE_SMALL = 0,
    TEXT_SIZE_MEDIUM = 1,
    TEXT_SIZE_LARGE = 2
} TextSizeType;

typedef enum
{
    LANGUAGE_SPANISH = 0,
    LANGUAGE_ENGLISH = 1
} LanguageType;

typedef enum
{
    MODE_SIMPLE = 0,
    MODE_ADVANCED = 1,
    MODE_CUSTOM = 2
} ModeType;

// Estructura para almacenar configuración
typedef struct
{
    ThemeType theme;
    LanguageType language;
    ModeType mode;
    TextSizeType text_size;
    int music_autoplay;
    float music_volume;
    int music_repeat_mode;
    int music_eq_enabled;
    float music_eq_bass_db;
    float music_eq_mid_db;
    float music_eq_treble_db;
    float music_volume_step;
    int dashboard_enabled;
} AppSettings;

/**
 * @brief Inicializa el sistema de configuración
 *
 * Carga configuración desde base de datos o establece valores por defecto.
 */
void settings_init();

/**
 * @brief Guarda la configuración actual en la base de datos
 */
void settings_save();

/**
 * @brief Obtiene la configuración actual
 */
AppSettings* settings_get();

/**
 * @brief Establece el tema de la interfaz
 */
void settings_set_theme(ThemeType theme);

/**
 * @brief Establece el idioma de la aplicación
 */
void settings_set_language(LanguageType language);

/**
 * @brief Establece el tamaño de texto de la aplicación
 */
void settings_set_text_size(TextSizeType text_size);

/**
 * @brief Aplica el tema actual a la consola
 */
void settings_apply_theme();

/**
 * @brief Obtiene el texto correspondiente al idioma actual
 */
const char* get_text(const char* key);

/**
 * @brief Establece el modo de la aplicación
 */
void settings_set_mode(ModeType mode);

/**
 * @brief Obtiene el modo actual de la aplicación
 */
ModeType settings_get_mode();

/**
 * @brief Habilita o deshabilita la reproduccion automatica al iniciar.
 */
void settings_set_music_autoplay(int enabled);

/**
 * @brief Indica si la musica debe reproducirse al iniciar.
 */
int settings_get_music_autoplay(void);

/**
 * @brief Establece el volumen del reproductor de musica (0.0 a 1.0).
 */
void settings_set_music_volume(float volume);

/**
 * @brief Obtiene el volumen del reproductor de musica (0.0 a 1.0).
 */
float settings_get_music_volume(void);

/**
 * @brief Establece el modo de repeticion de musica.
 */
void settings_set_music_repeat_mode(int mode);

/**
 * @brief Obtiene el modo de repeticion de musica.
 */
int settings_get_music_repeat_mode(void);

/**
 * @brief Habilita o deshabilita el ecualizador de musica.
 */
void settings_set_music_eq_enabled(int enabled);

/**
 * @brief Indica si el ecualizador de musica esta habilitado.
 */
int settings_get_music_eq_enabled(void);

/**
 * @brief Establece la ganancia de graves del ecualizador (dB).
 */
void settings_set_music_eq_bass_db(float db);

/**
 * @brief Obtiene la ganancia de graves del ecualizador (dB).
 */
float settings_get_music_eq_bass_db(void);

/**
 * @brief Establece la ganancia de medios del ecualizador (dB).
 */
void settings_set_music_eq_mid_db(float db);

/**
 * @brief Obtiene la ganancia de medios del ecualizador (dB).
 */
float settings_get_music_eq_mid_db(void);

/**
 * @brief Establece la ganancia de agudos del ecualizador (dB).
 */
void settings_set_music_eq_treble_db(float db);

/**
 * @brief Obtiene la ganancia de agudos del ecualizador (dB).
 */
float settings_get_music_eq_treble_db(void);

/**
 * @brief Establece el paso de cambio de volumen (fraccion 0.01 a 0.20).
 */
void settings_set_music_volume_step(float step);

/**
 * @brief Obtiene el paso de cambio de volumen (fraccion 0.01 a 0.20).
 */
float settings_get_music_volume_step(void);

/**
 * @brief Habilita o deshabilita la muestra del Dashboard al iniciar la aplicacion.
 */
void settings_set_dashboard_enabled(int enabled);

/**
 * @brief Indica si el Dashboard debe mostrarse al iniciar la aplicacion.
 */
int settings_get_dashboard_enabled(void);


// Funciones wrapper para internacionalización de menús
const char* get_menu_camisetas();
const char* get_menu_canchas();
const char* get_menu_partidos();
const char* get_menu_equipos();
const char* get_menu_estadisticas();
const char* get_menu_logros();
const char* get_menu_analisis();
const char* get_menu_bienestar();
const char* get_menu_lesiones();
const char* get_menu_financiamiento();
const char* get_menu_exportar();
const char* get_menu_importar();
const char* get_menu_torneos();
const char* get_menu_temporada();
const char* get_menu_entrenador_ia();
const char* get_menu_dashboard();
const char* get_menu_calendario();
const char* get_menu_carrera();
const char* get_menu_recordatorios();
const char* get_menu_colecciones();
const char* get_menu_musica();
const char* get_menu_settings();
const char* get_menu_records_rankings();
const char* get_menu_exit();
const char* get_menu_title();
const char* get_settings_theme();
const char* get_settings_language();
const char* get_menu_usuario();
const char* get_show_current();
const char* get_reset_defaults();
const char* get_menu_back();

/**
 * @brief Menú para configurar menús personalizados en modo Custom
 */
void menu_custom_menus();

/**
 * @brief Verifica si un menú está habilitado en modo Custom
 */
int is_custom_menu_enabled(const char* menu_name);

/**
 * @brief Establece el estado de un menú en modo Custom
 */
void set_custom_menu_enabled(const char* menu_name, int enabled);

/**
 * @brief Menú principal de configuración
 */
void menu_settings();

/**
 * @brief Verifica si hay actualizaciones disponibles de forma silenciosa
 * 
 * Esta función verifica si hay una nueva versión disponible en GitHub
 * y muestra una notificación discreta si es así.
 * 
 * @param mostrar_mensaje Si es 0, solo verifica sin mostrar nada si está actualizado
 *                        Si es 1, muestra mensaje tanto si hay actualización como si no
 */
void verificar_actualizacion_disponible(int mostrar_mensaje);

#endif /* SETTINGS_H */
