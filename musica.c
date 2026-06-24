
/* ---- miniaudio: un solo TU define la implementacion ---- */
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "musica.h"
#include "musica_helpers.h"
#include "db.h"
#include "utils.h"
#include "settings.h"
#include "ascii_art.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#endif

/* API interna de settings usada por el modulo de musica para guardar EQ en una sola escritura. */
void settings_set_music_eq_profile(int enabled, float bass_db, float mid_db, float treble_db);

/* Wrapper portable de fopen: usa fopen_s en MSVC, fopen en GCC/MinGW */
#ifdef _MSC_VER
#  define FOPEN_PORTABLE(fp, path, mode)                    \
    do {                                                    \
        if (fopen_s(&(fp), (path), (mode)) != 0) (fp) = NULL; \
    } while (0)
#else
#  define FOPEN_PORTABLE(fp, path, mode) \
    do { (fp) = fopen((path), (mode)); } while (0)
#endif

/* ---- Constantes ---- */
#define MUSICA_DIR_FALLBACK "Musica"
#define MAX_PISTAS    256
#define MAX_NOMBRE    512
#define MAX_RUTA      512
#define PASO_VOLUMEN  0.1f
#define BARRA_ANCHO   20    /* caracteres de la barra de volumen */
#define PROG_ANCHO    30    /* ancho de la barra de progreso */

/* ---- Fade ---- */
#define FADE_IN_MS    400U  /* ms de fade-in al cargar pista */
#define FADE_OUT_MS   250U  /* ms de fade-out al cambiar pista */

/* ---- Ecualizador ---- */
#define EQ_BASS_FREQ    200.0   /* Hz - graves */
#define EQ_MID_FREQ    1000.0   /* Hz - medios */
#define EQ_TREBLE_FREQ 8000.0   /* Hz - agudos */
#define EQ_Q             0.7    /* Q / pendiente de filtro */
#define EQ_DB_STEP       3.0f   /* dB por pulsacion */
#define EQ_DB_MIN       (-12.0f)
#define EQ_DB_MAX        12.0f

/* ---- Playlists ---- */
#define MAX_PLAYLIST_NAME  128

#ifdef MA_HAS_VORBIS
#define AUDIO_FORMATOS_TEXTO ".mp3, .wav, .flac, .ogg"
#else
#define AUDIO_FORMATOS_TEXTO ".mp3, .wav, .flac"
#endif

/* ---- Tipos ---- */
typedef enum
{
    ESTADO_DETENIDO = 0,
    ESTADO_REPRODUCIENDO,
    ESTADO_PAUSADO
} EstadoReproductor;

typedef enum
{
    REPETIR_NINGUNO = 0,
    REPETIR_PISTA,
    REPETIR_LISTA,
    REPETIR_ALEATORIO
} ModoRepeticion;

typedef enum
{
    FADE_ACCION_NINGUNA = 0,
    FADE_ACCION_CAMBIAR_PISTA,
    FADE_ACCION_DETENER
} FadeAccion;

typedef struct
{
    char nombre[MAX_NOMBRE];
    char ruta[MAX_RUTA];
} Pista;

/* ---- Estado global del reproductor ---- */
static ma_engine g_engine;
static ma_sound g_sonido;
static int g_engine_listo = 0;
static int g_sonido_listo = 0;
static Pista *g_pistas    = NULL;
static int    g_cap_pistas = 0;
static int g_num_pistas = 0;
static int g_pista_actual = -1;
static EstadoReproductor g_estado = ESTADO_DETENIDO;
static float g_volumen = 0.8f;
static ModoRepeticion g_modo_rep = REPETIR_NINGUNO;
static unsigned int g_rand_seed = 12345;

/* ---- Ecualizador 3 bandas (nodos del grafo de audio de miniaudio) ---- */
static ma_loshelf_node  g_eq_bass;
static ma_peak_node     g_eq_mid;
static ma_hishelf_node  g_eq_treble;
static int              g_eq_listo     = 0;   /* 1 = nodos inicializados */
static int              g_eq_activo    = 0;   /* 1 = cadena EQ conectada */
static float            g_eq_bass_db   = 0.0f;
static float            g_eq_mid_db    = 0.0f;
static float            g_eq_treble_db = 0.0f;

/* Permite buscar por nombre dentro del listado actual */
static char g_filtro_busqueda[MAX_NOMBRE] = {0};

/* Playlist actualmente activa ("" = catalogo completo) */
static char g_playlist_activa[MAX_PLAYLIST_NAME] = {0};

/* Historial de pistas en modo shuffle (ring buffer LIFO, max 20 entradas) */
#define HISTORIAL_SHUFFLE_MAX 20
static int g_historial_shuffle[HISTORIAL_SHUFFLE_MAX];
static int g_historial_head  = 0;
static int g_historial_count = 0;

/* Sleep timer: timestamp en ms en que se detiene la musica (0 = inactivo) */
static ma_uint64 g_sleep_timer_deadline_ms = 0;
static int       g_sleep_timer_minutos     = 0;

/* Paso de volumen configurable (fraccion 0-1, se carga desde settings) */
static float g_paso_volumen = 0.1f;

/* Flag: la posicion guardada ya fue restaurada en esta sesion */
static int g_resume_restaurado = 0;

/* Estado de acciones pendientes durante fade-out no bloqueante */
static FadeAccion g_fade_accion = FADE_ACCION_NINGUNA;
static int g_fade_indice_objetivo = -1;
static int g_fade_reproducir_objetivo = 0;
static ma_uint64 g_fade_deadline_ms = 0;

static int pistas_grow(void)
{
    int new_cap = g_cap_pistas ? g_cap_pistas * 2 : 64;
    Pista *p = realloc(g_pistas, (size_t)new_cap * sizeof(Pista));
    if (!p)
    {
        return 0;
    }
    g_pistas    = p;
    g_cap_pistas = new_cap;
    return 1;
}

static const char *musica_get_dir(void)
{
    const char *dir = get_music_dir();
    if (dir && dir[0] != '\0')
    {
        return dir;
    }
    return MUSICA_DIR_FALLBACK;
}

#define MUSICA_DIR musica_get_dir()

/* ============================================================
 * Utilidades internas
 * ============================================================ */

static float clampf_local(float v, float lo, float hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

static ma_uint64 tiempo_ms_actual(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (ma_uint64)tv.tv_sec * 1000U + (tv.tv_usec / 1000U);
#endif
}

static void cancelar_fade_pendiente(void)
{
    g_fade_accion = FADE_ACCION_NINGUNA;
    g_fade_indice_objetivo = -1;
    g_fade_reproducir_objetivo = 0;
    g_fade_deadline_ms = 0;
}

static int hay_fade_pendiente(void)
{
    return g_fade_accion != FADE_ACCION_NINGUNA;
}

static void programar_fade(FadeAccion accion, int indice_objetivo, int reproducir_objetivo)
{
    if (!g_sonido_listo || g_estado != ESTADO_REPRODUCIENDO)
    {
        return;
    }

    ma_sound_set_fade_in_milliseconds(&g_sonido, -1, 0.0F,
                                      (ma_uint64)FADE_OUT_MS);
    g_fade_accion = accion;
    g_fade_indice_objetivo = indice_objetivo;
    g_fade_reproducir_objetivo = reproducir_objetivo ? 1 : 0;
    g_fade_deadline_ms = tiempo_ms_actual() + (ma_uint64)FADE_OUT_MS + (ma_uint64)20;
}

static void inicializar_shuffle_seed(void)
{
    unsigned int seed = (unsigned int)time(NULL);
    seed ^= (unsigned int)(uintptr_t)&g_engine;
    seed ^= (unsigned int)(uintptr_t)&g_pistas;
    if (seed == 0u)
    {
        seed = 12345u;
    }
    g_rand_seed = seed;
}

static void historial_shuffle_push(int indice)
{
    g_historial_shuffle[g_historial_head] = indice;
    g_historial_head = (g_historial_head + 1) % HISTORIAL_SHUFFLE_MAX;
    if (g_historial_count < HISTORIAL_SHUFFLE_MAX)
    {
        g_historial_count++;
    }
}

static int historial_shuffle_pop(void)
{
    if (g_historial_count == 0)
    {
        return -1;
    }
    g_historial_count--;
    g_historial_head = (g_historial_head + HISTORIAL_SHUFFLE_MAX - 1) % HISTORIAL_SHUFFLE_MAX;
    return g_historial_shuffle[g_historial_head];
}

static void historial_shuffle_limpiar(void)
{
    g_historial_head  = 0;
    g_historial_count = 0;
}

/** Crea la carpeta "Musica" si no existe */
static void crear_dir_musica(void)
{
#ifdef _WIN32
    CreateDirectoryA(MUSICA_DIR, NULL); /* No error si ya existe */
#else
    mkdir(MUSICA_DIR, 0755); /* No error si ya existe */
#endif
}

static int comparar_pistas_por_nombre(const void *a, const void *b)
{
    const Pista *pa = (const Pista *)a;
    const Pista *pb = (const Pista *)b;
    return musica_compare_ci(pa->nombre, pb->nombre);
}

static void ordenar_pistas_catalogo(void)
{
    if (g_num_pistas > 1)
    {
        qsort(g_pistas, (size_t)g_num_pistas, sizeof(g_pistas[0]),
              comparar_pistas_por_nombre);
    }
}

/** Escanea MUSICA_DIR y llena g_pistas / g_num_pistas */
static void escanear_directorio(void)
{
    g_num_pistas = 0;

    crear_dir_musica();

#ifdef _WIN32
    char patron[MAX_RUTA];
    snprintf(patron, sizeof(patron), "%s\\*", MUSICA_DIR);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(patron, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }
        if (!musica_es_audio_soportado(fd.cFileName))
        {
            continue;
        }
        if (g_num_pistas >= g_cap_pistas && !pistas_grow())
        {
            break;
        }

        snprintf(g_pistas[g_num_pistas].nombre,
                 MAX_NOMBRE, "%s", fd.cFileName);
        snprintf(g_pistas[g_num_pistas].ruta,
                 MAX_RUTA, "%s\\%s", MUSICA_DIR, fd.cFileName);
        g_num_pistas++;
    }
    while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
#else
    DIR *dir = opendir(MUSICA_DIR);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!musica_es_audio_soportado(entry->d_name))
            continue;
        if (g_num_pistas >= g_cap_pistas && !pistas_grow())
            break;
        snprintf(g_pistas[g_num_pistas].nombre,
                 MAX_NOMBRE, "%s", entry->d_name);
        snprintf(g_pistas[g_num_pistas].ruta,
                 MAX_RUTA, "%s/%s", MUSICA_DIR, entry->d_name);
        g_num_pistas++;
    }
    closedir(dir);
#endif

    g_playlist_activa[0] = '\0';
    historial_shuffle_limpiar();
    ordenar_pistas_catalogo();
}

/* ============================================================
 * Motor de audio
 * ============================================================ */

/** Devuelve la cantidad de canales del engine garantizando que sea un valor
 *  utilizable por miniaudio (los buses almacenan los canales en ma_uint8, por
 *  lo que el limite es 255).  En entornos sin dispositivo de audio real
 *  (backend null de Linux, dispositivo virtual, etc.) ma_engine_get_channels
 *  puede devolver 0 o un valor fuera de rango, lo que hace fallar el assert
 *  interno de ma_node_input_bus_init (miniaudio.h:73954).  En ese caso se
 *  hace fallback a stereo. */
static ma_uint32 canales_engine_validos(void)
{
    ma_uint32 ch = ma_engine_get_channels(&g_engine);
    if (ch == 0 || ch >= 256)
    {
        fprintf(stderr, "Aviso: canales de audio no validos (%u), usando fallback stereo.\n", (unsigned)ch);
        ch = 2;
    }
    return ch;
}

static int inicializar_engine(void)
{
    if (g_engine_listo)
    {
        return 1;
    }

    ma_engine_config cfg = ma_engine_config_init();
    cfg.channels = 2;
    fprintf(stderr, "DEBUG: inicializar_engine: cfg.channels=%u\n", (unsigned)cfg.channels);
    ma_result res = ma_engine_init(&cfg, &g_engine);
    if (res != MA_SUCCESS)
    {
        ui_printf("Error: No se pudo inicializar el motor de audio (codigo %d).\n", res);
        return 0;
    }
    {
        ma_uint32 ch = ma_engine_get_channels(&g_engine);
        fprintf(stderr, "DEBUG: inicializar_engine: engine channels=%u\n", (unsigned)ch);
        if (ch == 0 || ch >= 256)
        {
            fprintf(stderr, "FATAL: engine has %u channels, cannot continue\n", (unsigned)ch);
            musica_cleanup();
            return 0;
        }
    }
    g_engine_listo = 1;
    atexit(musica_cleanup);
    inicializar_shuffle_seed();

    g_volumen = clampf_local(settings_get_music_volume(), 0.0f, 1.0f);
    g_modo_rep = (ModoRepeticion)settings_get_music_repeat_mode();
    g_eq_activo = settings_get_music_eq_enabled() ? 1 : 0;
    g_eq_bass_db = clampf_local(settings_get_music_eq_bass_db(), EQ_DB_MIN, EQ_DB_MAX);
    g_eq_mid_db = clampf_local(settings_get_music_eq_mid_db(), EQ_DB_MIN, EQ_DB_MAX);
    g_eq_treble_db = clampf_local(settings_get_music_eq_treble_db(), EQ_DB_MIN, EQ_DB_MAX);
    g_paso_volumen  = clampf_local(settings_get_music_volume_step(), 0.01f, 0.20f);

    /* Inicializar nodos del ecualizador en el grafo del engine */
    {
        ma_uint32 ch = canales_engine_validos();
        ma_uint32 sr = ma_engine_get_sample_rate(&g_engine);

        if (sr == 0)
        {
            fprintf(stderr, "Aviso: sample rate no valido, usando fallback 44100 Hz.\n");
            sr = 44100;
        }

        ma_loshelf_node_config bassCfg =
            ma_loshelf_node_config_init(ch, sr, (double)g_eq_bass_db,   EQ_Q, EQ_BASS_FREQ);
        ma_peak_node_config    midCfg  =
            ma_peak_node_config_init   (ch, sr, (double)g_eq_mid_db,    EQ_Q, EQ_MID_FREQ);
        ma_hishelf_node_config trebCfg =
            ma_hishelf_node_config_init(ch, sr, (double)g_eq_treble_db, EQ_Q, EQ_TREBLE_FREQ);

        if (ma_loshelf_node_init(ma_engine_get_node_graph(&g_engine),
                                 &bassCfg, NULL, &g_eq_bass) == MA_SUCCESS &&
                ma_peak_node_init   (ma_engine_get_node_graph(&g_engine),
                                     &midCfg,  NULL, &g_eq_mid)  == MA_SUCCESS &&
                ma_hishelf_node_init(ma_engine_get_node_graph(&g_engine),
                                     &trebCfg, NULL, &g_eq_treble) == MA_SUCCESS)
        {
            /* Cadena fija: bass -> mid -> treble -> endpoint */
            ma_node_attach_output_bus(&g_eq_bass,   0, &g_eq_mid,    0);
            ma_node_attach_output_bus(&g_eq_mid,    0, &g_eq_treble, 0);
            ma_node_attach_output_bus(&g_eq_treble, 0,
                                      ma_engine_get_endpoint(&g_engine), 0);
            g_eq_listo = 1;
        }
    }
    return 1;
}

static void descargar_sonido(void)
{
    cancelar_fade_pendiente();

    if (g_sonido_listo)
    {
        ma_sound_stop(&g_sonido);
        ma_sound_uninit(&g_sonido);
        g_sonido_listo = 0;
    }
    g_estado = ESTADO_DETENIDO;
}

static int cargar_pista(int indice)
{
    if (indice < 0 || indice >= g_num_pistas)
    {
        return 0;
    }

    descargar_sonido();

    ma_result res = ma_sound_init_from_file(
                        &g_engine,
                        g_pistas[indice].ruta,
                        MA_SOUND_FLAG_STREAM, /* Streaming: carga por bloques */
                        NULL, NULL,
                        &g_sonido);

    if (res != MA_SUCCESS)
    {
        ui_printf("Error: No se pudo cargar '%s' (codigo %d).\n",
                  g_pistas[indice].nombre, res);
        return 0;
    }

    /* Rerouting EQ: si el EQ esta activo, desconectar del endpoint y
       conectar a la cadena bass->mid->treble->endpoint */
    if (g_eq_activo && g_eq_listo)
    {
        ma_node_detach_output_bus(&g_sonido, 0);
        ma_node_attach_output_bus(&g_sonido, 0, &g_eq_bass, 0);
    }

    /* Fade-in: el volumen comenzara en 0 y subira cuando se llame start() */
    ma_sound_set_fade_in_milliseconds(&g_sonido, 0.0f, g_volumen,
                                      (ma_uint64)FADE_IN_MS);

    g_pista_actual = indice;
    g_sonido_listo = 1;
    return 1;
}

static void procesar_fade_pendiente(void)
{
    if (!hay_fade_pendiente())
    {
        return;
    }
    if (tiempo_ms_actual() < g_fade_deadline_ms)
    {
        return;
    }

    FadeAccion accion = g_fade_accion;
    int indice = g_fade_indice_objetivo;
    int reproducir_objetivo = g_fade_reproducir_objetivo;
    cancelar_fade_pendiente();

    if (accion == FADE_ACCION_DETENER)
    {
        if (g_sonido_listo)
        {
            ma_sound_stop(&g_sonido);
            ma_sound_seek_to_pcm_frame(&g_sonido, 0);
        }
        g_estado = ESTADO_DETENIDO;
        return;
    }

    if (accion == FADE_ACCION_CAMBIAR_PISTA)
    {
        if (!cargar_pista(indice))
        {
            return;
        }

        if (reproducir_objetivo)
        {
            ma_sound_start(&g_sonido);
            g_estado = ESTADO_REPRODUCIENDO;
        }
    }
}

static int cambiar_pista_con_fade(int indice)
{
    if (indice < 0 || indice >= g_num_pistas)
    {
        return 0;
    }

    if (g_sonido_listo && g_estado == ESTADO_REPRODUCIENDO)
    {
        programar_fade(FADE_ACCION_CAMBIAR_PISTA, indice, 1);
        return 1;
    }

    if (!cargar_pista(indice))
    {
        return 0;
    }

    ma_sound_start(&g_sonido);
    g_estado = ESTADO_REPRODUCIENDO;
    return 1;
}

/* ============================================================
 * Controles de reproduccion
 * ============================================================ */

static void reproducir(void)
{
    if (!g_engine_listo)
    {
        return;
    }

    if (g_num_pistas == 0)
    {
        ui_printf("No hay pistas en la carpeta '%s'.\n", MUSICA_DIR);
        pause_console();
        return;
    }

    if (g_estado == ESTADO_PAUSADO && g_sonido_listo)
    {
        /* Reanudar */
        ma_sound_start(&g_sonido);
        g_estado = ESTADO_REPRODUCIENDO;
        return;
    }

    /* Nueva pista: si no hay ninguna cargada, cargar la primera */
    if ((!g_sonido_listo || g_pista_actual < 0) && !cargar_pista(0))
    {
        return;
    }

    ma_sound_start(&g_sonido);
    g_estado = ESTADO_REPRODUCIENDO;
}

static void pausar(void)
{
    if (!g_sonido_listo || g_estado != ESTADO_REPRODUCIENDO)
    {
        return;
    }

    ma_sound_stop(&g_sonido); /* miniaudio: stop == pause, mantiene posicion */
    g_estado = ESTADO_PAUSADO;
}

static void detener(void)
{
    if (!g_sonido_listo)
    {
        return;
    }

    if (g_estado == ESTADO_REPRODUCIENDO)
    {
        /* Fade-out no bloqueante: se completa en el loop principal. */
        programar_fade(FADE_ACCION_DETENER, -1, 0);
        return;
    }

    ma_sound_stop(&g_sonido);
    ma_sound_seek_to_pcm_frame(&g_sonido, 0); /* Rebobinar al inicio */
    g_estado = ESTADO_DETENIDO;
}

/** LCG minimo para shuffle sin stdlib rand (reproducible, hilo-no-critico) */
static int siguiente_pista_aleatoria(void)
{
    if (g_num_pistas <= 1)
    {
        return 0;
    }
    g_rand_seed = g_rand_seed * 1664525u + 1013904223u;
    int sig = (int)(g_rand_seed % (unsigned int)g_num_pistas);
    /* Evitar repetir la misma pista si hay mas de una */
    if (sig == g_pista_actual)
    {
        sig = (sig + 1) % g_num_pistas;
    }
    return sig;
}

static void siguiente_pista(void)
{
    if (g_num_pistas == 0)
    {
        return;
    }

    int sig;
    if (g_pista_actual < 0)
    {
        sig = 0;
    }
    else if (g_modo_rep == REPETIR_ALEATORIO)
    {
        historial_shuffle_push(g_pista_actual);
        sig = siguiente_pista_aleatoria();
    }
    else
    {
        sig = (g_pista_actual + 1) % g_num_pistas;
    }

    /* Cambio sincrono: cargar_pista llama descargar_sonido internamente */
    if (cargar_pista(sig))
    {
        ma_sound_start(&g_sonido);
        g_estado = ESTADO_REPRODUCIENDO;
    }
}

static void pista_anterior(void)
{
    if (g_num_pistas == 0)
    {
        return;
    }

    int ant;
    if (g_modo_rep == REPETIR_ALEATORIO)
    {
        ant = historial_shuffle_pop();
        if (ant < 0 || ant >= g_num_pistas)
        {
            ant = (g_pista_actual > 0) ? g_pista_actual - 1 : g_num_pistas - 1;
        }
    }
    else if (g_pista_actual <= 0)
    {
        ant = g_num_pistas - 1;
    }
    else
    {
        ant = g_pista_actual - 1;
    }

    /* Cambio sincrono: cargar_pista llama descargar_sonido internamente */
    if (cargar_pista(ant))
    {
        ma_sound_start(&g_sonido);
        g_estado = ESTADO_REPRODUCIENDO;
    }
}

static void subir_volumen(void)
{
    g_volumen += g_paso_volumen;
    if (g_volumen > 1.0f)
    {
        g_volumen = 1.0f;
    }
    if (g_sonido_listo)
    {
        ma_sound_set_volume(&g_sonido, g_volumen);
    }
    settings_set_music_volume(g_volumen);
}

static void bajar_volumen(void)
{
    g_volumen -= g_paso_volumen;
    if (g_volumen < 0.0f)
    {
        g_volumen = 0.0f;
    }
    if (g_sonido_listo)
    {
        ma_sound_set_volume(&g_sonido, g_volumen);
    }
    settings_set_music_volume(g_volumen);
}

static void mover_cursor_segundos(int delta_segundos)
{
    if (!g_engine_listo || !g_sonido_listo)
    {
        return;
    }

    ma_uint64 cur = 0;
    ma_uint64 len = 0;
    ma_sound_get_cursor_in_pcm_frames(&g_sonido, &cur);
    ma_sound_get_length_in_pcm_frames(&g_sonido, &len);
    ma_uint32 sr = ma_engine_get_sample_rate(&g_engine);

    if (sr == 0)
    {
        return;
    }

    ma_int64 delta_frames = (ma_int64)delta_segundos * (ma_int64)sr;
    ma_int64 nuevo = (ma_int64)cur + delta_frames;
    if (nuevo < 0)
    {
        nuevo = 0;
    }
    if (len > 0 && (ma_uint64)nuevo > len)
    {
        nuevo = (ma_int64)len;
    }

    ma_sound_seek_to_pcm_frame(&g_sonido, (ma_uint64)nuevo);
}

static void retroceder_10s(void)
{
    mover_cursor_segundos(-10);
}

static void avanzar_10s(void)
{
    mover_cursor_segundos(10);
}

/**
 * Avanza automaticamente si la pista termino (llamado antes de dibujar el menu).
 */
static void verificar_fin_pista(void)
{
    if (hay_fade_pendiente())
    {
        return;
    }

    if (!g_sonido_listo || g_estado != ESTADO_REPRODUCIENDO)
    {
        return;
    }

    if (!ma_sound_at_end(&g_sonido))
    {
        return;
    }

    /* La pista termino */
    switch (g_modo_rep)
    {
    case REPETIR_PISTA:
        ma_sound_seek_to_pcm_frame(&g_sonido, 0);
        ma_sound_start(&g_sonido);
        break;

    case REPETIR_LISTA:
    case REPETIR_ALEATORIO:
        siguiente_pista();
        break;

    case REPETIR_NINGUNO:
    default:
        if (g_pista_actual >= 0 && g_pista_actual < g_num_pistas - 1)
        {
            siguiente_pista(); /* Avanza si no es la ultima */
        }
        else
        {
            detener();
        }
        break;
    }
}

/* ============================================================
 * Interfaz de usuario
 * ============================================================ */

/** Dibuja una barra de progreso ASCII/Unicode para el volumen */
static void dibujar_barra_volumen(void)
{
    int llenos = (int)(g_volumen * BARRA_ANCHO + 0.5f);
    int vacios = BARRA_ANCHO - llenos;
    int porcentaje = (int)(g_volumen * 100.0f + 0.5f);

    int unicode = consola_soporta_unicode();

    ui_printf("  Volumen: [");
    for (int i = 0; i < llenos; i++)
    {
        ui_printf("%s", unicode ? "\u2588" : "#");
    }
    for (int i = 0; i < vacios; i++)
    {
        ui_printf("%s", unicode ? "\u2591" : "-");
    }
    ui_printf("] %3d%%\n", porcentaje);
}

/** Renderiza la barra de progreso de la pista activa */
static void dibujar_barra_progreso(int unicode)
{
    if (!g_engine_listo || !g_sonido_listo)
    {
        return;
    }

    ma_uint64 cur = 0;
    ma_uint64 len = 0;
    ma_sound_get_cursor_in_pcm_frames(&g_sonido, &cur);
    ma_sound_get_length_in_pcm_frames(&g_sonido, &len);
    ma_uint32 sr = ma_engine_get_sample_rate(&g_engine);

    float pos_s = (sr > 0) ? (float)cur / (float)sr : 0.0f;
    float tot_s = (sr > 0 && len > 0) ? (float)len / (float)sr : 0.0f;

    int rellenos = (tot_s > 0.0f)
                   ? (int)(pos_s / tot_s * PROG_ANCHO + 0.5f) : 0;
    if (rellenos > PROG_ANCHO)
    {
        rellenos = PROG_ANCHO;
    }
    int vacios = PROG_ANCHO - rellenos;

    ui_printf("  Progreso: [");
    for (int i = 0; i < rellenos; i++)
    {
        ui_printf("%s", unicode ? "\u2580" : "#");
    }
    for (int i = 0; i < vacios; i++)
    {
        ui_printf("%s", unicode ? "\u2591" : "-");
    }
    if (tot_s > 0.0f)
    {
        float rem_s = tot_s - pos_s;
        if (rem_s < 0.0f)
        {
            rem_s = 0.0f;
        }
        ui_printf("] %02d:%02d / %02d:%02d (-%02d:%02d)\n",
                  (int)pos_s / 60, (int)pos_s % 60,
                  (int)tot_s / 60, (int)tot_s % 60,
                  (int)rem_s / 60, (int)rem_s % 60);
    }
    else
    {
        ui_printf("] %02d:%02d / --:--\n", (int)pos_s / 60, (int)pos_s % 60);
    }
}

/** Renderiza el nombre y progreso de la pista activa (o mensaje de sin pista) */
static void dibujar_pista_actual(int unicode,
                                 const char *flechaP,
                                 const char *pausa,
                                 const char *stop)
{
    if (g_pista_actual < 0 || g_num_pistas == 0)
    {
        ui_printf("  %s Ninguna pista seleccionada\n", stop);
        return;
    }

    const char *icono;
    if (g_estado == ESTADO_REPRODUCIENDO)
    {
        icono = flechaP;
    }
    else if (g_estado == ESTADO_PAUSADO)
    {
        icono = pausa;
    }
    else
    {
        icono = stop;
    }
    ui_printf("  %s Reproduciendo [%d/%d]:\n", icono,
              g_pista_actual + 1, g_num_pistas);
    ui_printf("    %s\n", g_pistas[g_pista_actual].nombre);
    dibujar_barra_progreso(unicode);
}

/** Renderiza estado, volumen, modo repeticion y EQ */
static void dibujar_info_estado(void)
{
    static const char * const ESTADOS[] =
    { "DETENIDO", "REPRODUCIENDO", "PAUSADO" };
    ui_printf("  Estado  : %s\n", ESTADOS[(int)g_estado]);

    dibujar_barra_volumen();

    const char *rep_str;
    if (g_modo_rep == REPETIR_PISTA)
    {
        rep_str = "Repetir pista";
    }
    else if (g_modo_rep == REPETIR_LISTA)
    {
        rep_str = "Repetir lista";
    }
    else if (g_modo_rep == REPETIR_ALEATORIO)
    {
        rep_str = "Aleatorio (shuffle)";
    }
    else
    {
        rep_str = "Sin repeticion";
    }
    ui_printf("  Repetir : %s\n", rep_str);

    if (g_eq_activo)
    {
        ui_printf("  EQ      : ACTIVO  B:%+.0f  M:%+.0f  A:%+.0f dB\n",
                  (double)g_eq_bass_db, (double)g_eq_mid_db,
                  (double)g_eq_treble_db);
    }
    else
    {
        ui_printf("  EQ      : Desactivado\n");
    }

    if (g_filtro_busqueda[0] != '\0')
    {
        ui_printf("  Filtro  : \"%s\"\n", g_filtro_busqueda);
    }
    else
    {
        ui_printf("  Filtro  : (sin filtro)\n");
    }

    if (g_playlist_activa[0] != '\0')
    {
        ui_printf("  Playlist: %s\n", g_playlist_activa);
    }
    else
    {
        ui_printf("  Playlist: (catalogo completo)\n");
    }

    if (g_sleep_timer_deadline_ms != 0)
    {
        ma_uint64 ahora = tiempo_ms_actual();
        int restante_s = (g_sleep_timer_deadline_ms > ahora)
                         ? (int)((g_sleep_timer_deadline_ms - ahora) / 1000U) : 0;
        ui_printf("  Timer   : apagado en %02d:%02d\n", restante_s / 60, restante_s % 60);
    }

    ui_printf("  AutoIni : %s\n", settings_get_music_autoplay() ? "Activada" : "Desactivada");
}

/** Imprime las opciones del menu principal del reproductor */
static void dibujar_opciones_reproductor(const char *linea)
{
    for (int i = 0; i < 50; i++)
    {
        ui_printf("%s", linea);
    }
    ui_printf("\n");
    ui_printf("  [1] Reproducir / Pausar\n");
    ui_printf("  [2] Detener\n");
    ui_printf("  [3] Pista anterior\n");
    ui_printf("  [4] Pista siguiente\n");
    ui_printf("  [5] Seleccionar pista de la lista\n");
    ui_printf("  [6] Subir volumen (+%.0f%%)\n", (double)(g_paso_volumen * 100.0f));
    ui_printf("  [7] Bajar volumen (-%.0f%%)\n", (double)(g_paso_volumen * 100.0f));
    ui_printf("  [8] Cambiar modo repeticion / shuffle\n");
    ui_printf("  [9] Actualizar lista\n");
    ui_printf("  [10] Agregar cancion a la carpeta\n");
    ui_printf("  [11] Eliminar cancion de la carpeta\n");
    ui_printf("  [12] Ecualizador (3 bandas)\n");
    ui_printf("  [13] Playlists\n");
    ui_printf("  [14] Musica al iniciar (ON/OFF)\n");
    ui_printf("  [15] Buscar pista por nombre\n");
    ui_printf("  [16] Retroceder 10 segundos\n");
    ui_printf("  [17] Avanzar 10 segundos\n");
    ui_printf("  [18] Limpiar filtro de busqueda\n");
    ui_printf("  [19] Temporizador de apagado\n");
    ui_printf("  [20] Saltar a tiempo (MM:SS)\n");
    ui_printf("  [21] Informacion de pista activa\n");
    ui_printf("  [22] Renombrar pista\n");
    ui_printf("  [23] Exportar catalogo como playlist\n");
    ui_printf("  [24] Configurar paso de volumen\n");
    for (int i = 0; i < 50; i++)
    {
        ui_printf("%s", linea);
    }
    ui_printf("\n");
    ui_printf("  [0] Volver al menu principal\n\n");
}

/** Renderiza el encabezado y la vista del reproductor */
static void dibujar_reproductor(void)
{
    clear_screen();
    print_header("MUSICA");

    int unicode = consola_soporta_unicode();
    const char *linea   = unicode ? "\u2550" : "=";
    const char *nota    = unicode ? "\u266b" : "*";
    const char *flechaP = unicode ? "\u25ba" : ">";
    const char *pausa   = unicode ? "\u23f8" : "||";
    const char *stop    = unicode ? "\u25a0" : "[]";

    ui_printf("\n");
    for (int i = 0; i < 50; i++)
    {
        ui_printf("%s", linea);
    }
    ui_printf("\n");
    ui_printf("   %s  REPRODUCTOR DE MUSICA  %s\n", nota, nota);
    for (int i = 0; i < 50; i++)
    {
        ui_printf("%s", linea);
    }
    ui_printf("\n");
    ui_printf("  Carpeta : %s/\n", MUSICA_DIR);
    ui_printf("  Pistas  : %d archivo(s) de audio\n", g_num_pistas);
    ui_printf("\n");

    dibujar_pista_actual(unicode, flechaP, pausa, stop);
    dibujar_info_estado();
    dibujar_opciones_reproductor(linea);
}

/** Muestra la lista de pistas para seleccionar */
static void mostrar_lista_pistas(void)
{
    clear_screen();
    print_header("Lista de Pistas");

    if (g_num_pistas == 0)
    {
        ui_printf("  No se encontraron archivos de audio en '%s/'.\n\n", MUSICA_DIR);
        ui_printf("  Formatos compatibles: %s\n", AUDIO_FORMATOS_TEXTO);
        ui_printf("  Coloque sus archivos de audio dentro de la carpeta '%s/'\n", MUSICA_DIR);
        ui_printf("  y presione [9] para actualizar la lista.\n\n");
        pause_console();
        return;
    }

    int *indices = malloc((size_t)g_num_pistas * sizeof(int));
    if (!indices)
    {
        pause_console();
        return;
    }
    int visibles = 0;
    for (int i = 0; i < g_num_pistas; i++)
    {
        if (g_filtro_busqueda[0] != '\0' &&
                !musica_contiene_subcadena_ci(g_pistas[i].nombre, g_filtro_busqueda))
        {
            continue;
        }

        const char *marca = (i == g_pista_actual) ? " [*]" : "    ";
        ui_printf("  %s %3d. %s\n", marca, visibles + 1, g_pistas[i].nombre);
        indices[visibles] = i;
        visibles++;
    }
    ui_printf("\n");

    if (visibles == 0)
    {
        ui_printf("  No hay pistas que coincidan con el filtro actual.\n");
        free(indices);
        pause_console();
        return;
    }

    int seleccion = input_int("  Seleccione pista (0 para cancelar): ");
    if (seleccion <= 0 || seleccion > visibles)
    {
        free(indices);
        return;
    }

    cambiar_pista_con_fade(indices[seleccion - 1]);
    free(indices);
}

static void buscar_pista_por_nombre_menu(void)
{
    clear_screen();
    print_header("Buscar Pista");

    ui_printf("  Ingrese parte del nombre para filtrar el catalogo.\n");
    ui_printf("  Deje vacio y presione Enter para limpiar el filtro.\n\n");

    input_string("  Buscar: ", g_filtro_busqueda, (int)sizeof(g_filtro_busqueda));

    if (g_filtro_busqueda[0] == '\0')
    {
        ui_printf("\n  Filtro limpiado.\n");
        pause_console();
        return;
    }

    int coincidencias = 0;
    for (int i = 0; i < g_num_pistas; i++)
    {
        if (musica_contiene_subcadena_ci(g_pistas[i].nombre, g_filtro_busqueda))
        {
            coincidencias++;
        }
    }

    ui_printf("\n  Coincidencias encontradas: %d\n", coincidencias);
    pause_console();
}

static void limpiar_filtro_busqueda(void)
{
    if (g_filtro_busqueda[0] == '\0')
    {
        ui_printf("  El filtro ya estaba vacio.\n");
        pause_console();
        return;
    }

    g_filtro_busqueda[0] = '\0';
    ui_printf("  Filtro limpiado correctamente.\n");
    pause_console();
}

static void procesar_sleep_timer(void)
{
    if (g_sleep_timer_deadline_ms == 0)
    {
        return;
    }
    if (tiempo_ms_actual() < g_sleep_timer_deadline_ms)
    {
        return;
    }

    g_sleep_timer_deadline_ms = 0;
    g_sleep_timer_minutos = 0;

    if (g_sonido_listo && g_estado == ESTADO_REPRODUCIENDO)
    {
        programar_fade(FADE_ACCION_DETENER, -1, 0);
    }
}

static void accion_sleep_timer(void)
{
    clear_screen();
    print_header("Temporizador de Apagado");

    if (g_sleep_timer_deadline_ms != 0)
    {
        ma_uint64 ahora = tiempo_ms_actual();
        int restante_s = (g_sleep_timer_deadline_ms > ahora)
                         ? (int)((g_sleep_timer_deadline_ms - ahora) / 1000U) : 0;
        ui_printf("  Temporizador activo: se detendra en %02d:%02d\n",
                  restante_s / 60, restante_s % 60);
        ui_printf("  Ingrese 0 para cancelarlo o un nuevo tiempo en minutos.\n\n");
    }
    else
    {
        ui_printf("  No hay temporizador activo.\n\n");
    }

    int minutos = input_int("  Minutos hasta apagado (0 = cancelar/desactivar): ");
    if (minutos <= 0)
    {
        if (g_sleep_timer_deadline_ms != 0)
        {
            g_sleep_timer_deadline_ms = 0;
            g_sleep_timer_minutos = 0;
            ui_printf("  Temporizador cancelado.\n");
        }
        else
        {
            ui_printf("  Sin cambios.\n");
        }
        pause_console();
        return;
    }

    g_sleep_timer_minutos = minutos;
    g_sleep_timer_deadline_ms = tiempo_ms_actual() + (ma_uint64)minutos * 60000U;
    ui_printf("  El audio se detendra en %d minuto(s).\n", minutos);
    pause_console();
}

static void seek_a_tiempo_exacto_menu(void)
{
    if (!g_engine_listo || !g_sonido_listo)
    {
        ui_printf("  No hay pista cargada.\n");
        pause_console();
        return;
    }

    ma_uint64 len = 0;
    ma_sound_get_length_in_pcm_frames(&g_sonido, &len);
    ma_uint32 sr = ma_engine_get_sample_rate(&g_engine);
    float tot_s = (sr > 0 && len > 0) ? (float)len / (float)sr : 0.0f;

    ui_printf("  Duracion total: %02d:%02d\n",
              (int)tot_s / 60, (int)tot_s % 60);
    ui_printf("  Formato: MM:SS (ej: 1:30) o solo segundos (ej: 90)\n\n");

    char entrada[16] = {0};
    input_string_extended("  Ir a: ", entrada, (int)sizeof(entrada));

    int min_seek = 0;
    int seg_seek = 0;
    char *sep = strchr(entrada, ':');
    if (sep)
    {
        *sep = '\0';
        min_seek = atoi(entrada);
        seg_seek = atoi(sep + 1);
    }
    else
    {
        seg_seek = atoi(entrada);
    }

    int tiempo_s = min_seek * 60 + seg_seek;
    if (tiempo_s < 0)
    {
        tiempo_s = 0;
    }

    if (sr == 0)
    {
        ui_printf("  Error: sample rate desconocido.\n");
        pause_console();
        return;
    }

    ma_uint64 frame = (ma_uint64)tiempo_s * (ma_uint64)sr;
    if (len > 0 && frame > len)
    {
        frame = len;
    }

    ma_sound_seek_to_pcm_frame(&g_sonido, frame);
    ui_printf("  Reproduccion saltada a %02d:%02d.\n", tiempo_s / 60, tiempo_s % 60);
    pause_console();
}

/* ============================================================
 * Gestion de canciones
 * ============================================================ */

/**
 * Copia un archivo de audio desde una ruta arbitraria a la carpeta Musica/.
 * Retorna 1 si se copio correctamente, 0 si hubo error.
 */
static int copiar_archivo_audio(const char *ruta_origen, const char *nombre_destino)
{
    crear_dir_musica();

    char ruta_destino[MAX_RUTA * 2];
#ifdef _WIN32
    snprintf(ruta_destino, sizeof(ruta_destino), "%s\\%s", MUSICA_DIR, nombre_destino);
#else
    snprintf(ruta_destino, sizeof(ruta_destino), "%s/%s", MUSICA_DIR, nombre_destino);
#endif

    /* Verificar que no sobreescriba una pista que se esta reproduciendo */
    if (g_sonido_listo && g_pista_actual >= 0 &&
            strcmp(g_pistas[g_pista_actual].ruta, ruta_destino) == 0)
    {
        ui_printf("  Error: esa pista esta en reproduccion. Detenla primero.\n");
        return 0;
    }

#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(ruta_destino);
    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        ui_printf("  Ya existe un archivo con ese nombre.\n");
        ui_printf("  Desea sobreescribirlo? [s/n]: ");
        char conf[8] = {0};
        input_string("", conf, (int)sizeof(conf));
        if (conf[0] != 's' && conf[0] != 'S')
        {
            ui_printf("  Copia cancelada por el usuario.\n");
            return 0;
        }
    }

    if (!CopyFileA(ruta_origen, ruta_destino, FALSE))
    {
        ui_printf("  Error al copiar el archivo (codigo Windows %lu).\n",
                  (unsigned long)GetLastError());
        return 0;
    }
#else
    FILE *src = NULL;
    FOPEN_PORTABLE(src, ruta_origen, "rb");
    if (!src)
    {
        ui_printf("  Error: no se pudo abrir el archivo origen.\n");
        return 0;
    }
    FILE *dst = NULL;
    FOPEN_PORTABLE(dst, ruta_destino, "wb");
    if (!dst)
    {
        fclose(src);
        ui_printf("  Error: no se pudo crear el archivo destino.\n");
        return 0;
    }
    char buf[8192];
    size_t leido;
    int ok = 1;
    while ((leido = fread(buf, 1, sizeof(buf), src)) > 0)
    {
        size_t escrito = fwrite(buf, 1, leido, dst);
        if (escrito != leido)
        {
            ok = 0;
            break;
        }
    }
    if (ferror(src))
        ok = 0;
    if (fflush(dst) != 0)
        ok = 0;
    fclose(src);
    if (fclose(dst) != 0)
        ok = 0;

    if (!ok)
    {
        remove(ruta_destino); /* Evitar dejar archivos parciales */
        ui_printf("  Error: fallo de E/S durante la copia del archivo.\n");
        return 0;
    }
#endif
    return 1;
}

/** Submenu: el usuario introduce la ruta de un audio y lo copia a Musica/ */
static void agregar_cancion_menu(void)
{
    clear_screen();
    print_header("Agregar Cancion a la Carpeta de Musica");

    ui_printf("  Introduzca la ruta completa del archivo de audio que desea agregar.\n");
    ui_printf("  Formatos compatibles: %s\n", AUDIO_FORMATOS_TEXTO);
    ui_printf("  Ejemplo Windows: C:\\Musica\\MiCancion.mp3\n");
    ui_printf("  Ejemplo Linux  : /home/user/Musica/MiCancion.mp3\n\n");
    ui_printf("  (Ingrese 0 para cancelar)\n\n");

    char ruta[1024] = {0};
    input_string("  Ruta del archivo: ", ruta, (int)sizeof(ruta));

    if (ruta[0] == '0' && ruta[1] == '\0')
    {
        ui_printf("  Operacion cancelada.\n");
        pause_console();
        return;
    }

    if (!musica_es_audio_soportado(ruta))
    {
        ui_printf("  Error: formato no soportado. Use %s\n", AUDIO_FORMATOS_TEXTO);
        pause_console();
        return;
    }

    /* Verificar que el archivo existe */
    FILE *f = NULL;
    FOPEN_PORTABLE(f, ruta, "rb");
    if (!f)
    {
        ui_printf("  Error: no se encontro el archivo:\n  %s\n", ruta);
        pause_console();
        return;
    }
    fclose(f);

    const char *nombre_auto = musica_basename_portable(ruta);

    /* Permitir nombre personalizado */
    ui_printf("\n  Nombre actual del archivo: %s\n", nombre_auto);
    ui_printf("  Ingrese un nombre nuevo (mantenga extension) o Enter para conservar el actual:\n");

    char nombre_nuevo[MAX_NOMBRE] = {0};
    input_string("  Nombre: ", nombre_nuevo, (int)sizeof(nombre_nuevo));

    const char *nombre_final;
    if (nombre_nuevo[0] == '\0')
    {
        nombre_final = nombre_auto;
    }
    else
    {
        /* Si no tiene extension, reutiliza la extension del archivo origen */
        const char *ext_nuevo = musica_obtener_extension_archivo(nombre_nuevo);
        if (!ext_nuevo)
        {
            const char *ext_auto = musica_obtener_extension_archivo(nombre_auto);
            size_t ext_len = ext_auto ? strlen_s(ext_auto, MAX_NOMBRE) : 0;
            size_t ln = strlen_s(nombre_nuevo, sizeof(nombre_nuevo));
            if (!ext_auto || ext_len == 0 || ln + ext_len >= sizeof(nombre_nuevo))
            {
                ui_printf("  Error: nombre demasiado largo o extension invalida.\n");
                pause_console();
                return;
            }
            strcat_s(nombre_nuevo, sizeof(nombre_nuevo), ext_auto);
        }

        if (!musica_es_audio_soportado(nombre_nuevo))
        {
            ui_printf("  Error: formato no soportado. Use %s\n", AUDIO_FORMATOS_TEXTO);
            pause_console();
            return;
        }

        nombre_final = nombre_nuevo;
    }

    if (copiar_archivo_audio(ruta, nombre_final))
    {
        ui_printf("\n  Cancion agregada correctamente: %s\n", nombre_final);
        ui_printf("  Use [9] Actualizar Lista para verla en el reproductor.\n");
        /* Actualizar lista automaticamente */
        escanear_directorio();
        ui_printf("  Lista actualizada: %d pista(s) encontradas.\n", g_num_pistas);
    }

    pause_console();
}

/** Submenu: lista canciones y permite eliminar una */
static void eliminar_cancion_menu(void)
{
    clear_screen();
    print_header("Eliminar Cancion de la Carpeta de Musica");

    if (g_num_pistas == 0)
    {
        ui_printf("  No hay canciones en la carpeta '%s/'.\n\n", MUSICA_DIR);
        pause_console();
        return;
    }

    ui_printf("  Canciones disponibles:\n\n");
    for (int i = 0; i < g_num_pistas; i++)
    {
        const char *marca = (i == g_pista_actual && g_sonido_listo) ? " [EN REPRODUCCION]" : "";
        ui_printf("  %3d. %s%s\n", i + 1, g_pistas[i].nombre, marca);
    }
    ui_printf("\n");

    int sel = input_int("  Seleccione la cancion a eliminar (0 para cancelar): ");
    if (sel <= 0 || sel > g_num_pistas)
    {
        ui_printf("  Operacion cancelada.\n");
        pause_console();
        return;
    }

    int idx = sel - 1;

    /* No borrar pista en reproduccion */
    if (g_sonido_listo && g_pista_actual == idx)
    {
        ui_printf("  No se puede eliminar la pista en reproduccion.\n");
        ui_printf("  Detengala primero con la opcion [2].\n");
        pause_console();
        return;
    }

    ui_printf("\n  Va a eliminar: %s\n", g_pistas[idx].nombre);
    ui_printf("  Esta accion es IRREVERSIBLE. Confirmar? [s/n]: ");

    char conf[8] = {0};
    input_string("", conf, (int)sizeof(conf));
    if (conf[0] != 's' && conf[0] != 'S')
    {
        ui_printf("  Eliminacion cancelada.\n");
        pause_console();
        return;
    }

    char ruta_borrar[MAX_RUTA];
    strncpy_s(ruta_borrar, sizeof(ruta_borrar), g_pistas[idx].ruta, _TRUNCATE);

    if (remove(ruta_borrar) == 0)
    {
        ui_printf("  Cancion eliminada: %s\n", g_pistas[idx].nombre);
        /* Si la pista eliminada era posterior a la actual, ajustar indice */
        if (g_pista_actual > idx)
        {
            g_pista_actual--;
        }
        escanear_directorio();
        ui_printf("  Lista actualizada: %d pista(s) restantes.\n", g_num_pistas);
    }
    else
    {
        ui_printf("  Error: no se pudo eliminar el archivo.\n");
        ui_printf("  Verifique permisos de escritura en la carpeta '%s/'.\n", MUSICA_DIR);
    }

    pause_console();
}

/* ============================================================
 * Ecualizador de 3 bandas
 * ============================================================ */

/** Aplica EQ routing al sonido activo. Si eq_activo, lo enruta por la cadena
    bass->mid->treble->endpoint; si no, lo conecta directo al endpoint. */
static void eq_reroute_sonido(void)
{
    if (!g_sonido_listo || !g_eq_listo)
    {
        return;
    }
    ma_node_detach_output_bus(&g_sonido, 0);
    if (g_eq_activo)
    {
        ma_node_attach_output_bus(&g_sonido, 0, &g_eq_bass, 0);
    }
    else
    {
        ma_node_attach_output_bus(&g_sonido, 0, ma_engine_get_endpoint(&g_engine), 0);
    }
}

/** Reinicia el nodo de graves con la ganancia actual */
static void eq_update_bass(void)
{
    if (!g_eq_listo)
    {
        return;
    }
    ma_loshelf_config cfg = ma_loshelf2_config_init(
                                ma_format_f32,
                                canales_engine_validos(),
                                ma_engine_get_sample_rate(&g_engine),
                                (double)g_eq_bass_db, EQ_Q, EQ_BASS_FREQ);
    ma_loshelf_node_reinit(&cfg, &g_eq_bass);
}

/** Reinicia el nodo de medios con la ganancia actual */
static void eq_update_mid(void)
{
    if (!g_eq_listo)
    {
        return;
    }
    ma_peak_config cfg = ma_peak2_config_init(
                             ma_format_f32,
                             canales_engine_validos(),
                             ma_engine_get_sample_rate(&g_engine),
                             (double)g_eq_mid_db, EQ_Q, EQ_MID_FREQ);
    ma_peak_node_reinit(&cfg, &g_eq_mid);
}

/** Reinicia el nodo de agudos con la ganancia actual */
static void eq_update_treble(void)
{
    if (!g_eq_listo)
    {
        return;
    }
    ma_hishelf_config cfg = ma_hishelf2_config_init(
                                ma_format_f32,
                                canales_engine_validos(),
                                ma_engine_get_sample_rate(&g_engine),
                                (double)g_eq_treble_db, EQ_Q, EQ_TREBLE_FREQ);
    ma_hishelf_node_reinit(&cfg, &g_eq_treble);
}

static float eq_limitar_db(float valor)
{
    if (valor < EQ_DB_MIN)
    {
        return EQ_DB_MIN;
    }
    if (valor > EQ_DB_MAX)
    {
        return EQ_DB_MAX;
    }
    return valor;
}

static void eq_ajustar_banda(float *valor_db, float delta, void (*update_fn)(void))
{
    *valor_db = eq_limitar_db(*valor_db + delta);
    update_fn();
}

static void eq_restablecer_bandas(void)
{
    g_eq_bass_db = 0.0f;
    g_eq_mid_db = 0.0f;
    g_eq_treble_db = 0.0f;
    eq_update_bass();
    eq_update_mid();
    eq_update_treble();
}

static void eq_guardar_preferencias(void)
{
    settings_set_music_eq_profile(g_eq_activo ? 1 : 0,
                                  g_eq_bass_db,
                                  g_eq_mid_db,
                                  g_eq_treble_db);
}

static int procesar_opcion_ecualizador(int op)
{
    if (op == 0)
    {
        return 1;
    }

    if (op == 1)
    {
        g_eq_activo = !g_eq_activo;
        eq_reroute_sonido();
        eq_guardar_preferencias();
        return 0;
    }

    if (op == 8)
    {
        eq_restablecer_bandas();
        eq_guardar_preferencias();
        return 0;
    }

    typedef struct
    {
        int opcion;
        float *valor_db;
        float delta;
        void (*update_fn)(void);
    } EqAjuste;

    static EqAjuste ajustes[] =
    {
        {2, &g_eq_bass_db, EQ_DB_STEP,  eq_update_bass},
        {3, &g_eq_bass_db, -EQ_DB_STEP, eq_update_bass},
        {4, &g_eq_mid_db,  EQ_DB_STEP,  eq_update_mid},
        {5, &g_eq_mid_db,  -EQ_DB_STEP, eq_update_mid},
        {6, &g_eq_treble_db, EQ_DB_STEP,  eq_update_treble},
        {7, &g_eq_treble_db, -EQ_DB_STEP, eq_update_treble}
    };

    for (int i = 0; i < (int)(sizeof(ajustes) / sizeof(ajustes[0])); i++)
    {
        if (ajustes[i].opcion == op)
        {
            eq_ajustar_banda(ajustes[i].valor_db, ajustes[i].delta, ajustes[i].update_fn);
            eq_guardar_preferencias();
            return 0;
        }
    }

    return 0;
}

/** Menu interactivo del ecualizador */
static void menu_ecualizador(void)
{
    if (!g_eq_listo)
    {
        ui_printf("  El ecualizador no pudo inicializarse con este motor de audio.\n");
        pause_console();
        return;
    }

    int salir_eq = 0;
    while (!salir_eq)
    {
        clear_screen();
        print_header("Ecualizador de 3 Bandas");
        ui_printf("\n");
        ui_printf("  Estado : %s\n", g_eq_activo ? "ACTIVO" : "Desactivado");
        ui_printf("\n");
        ui_printf("  Banda       Frec.   Ganancia\n");
        ui_printf("  ------------------------------------\n");
        ui_printf("  Graves (B)  %4.0f Hz  %+.1f dB\n", EQ_BASS_FREQ,   (double)g_eq_bass_db);
        ui_printf("  Medios  (M) %4.0f Hz  %+.1f dB\n", EQ_MID_FREQ,    (double)g_eq_mid_db);
        ui_printf("  Agudos  (A) %4.0f Hz  %+.1f dB\n", EQ_TREBLE_FREQ, (double)g_eq_treble_db);
        ui_printf("\n");
        ui_printf("  [1] Activar / Desactivar EQ\n");
        ui_printf("  [2] Graves  +%.0f dB     [3] Graves  -%.0f dB\n",
                  (double)EQ_DB_STEP, (double)EQ_DB_STEP);
        ui_printf("  [4] Medios  +%.0f dB     [5] Medios  -%.0f dB\n",
                  (double)EQ_DB_STEP, (double)EQ_DB_STEP);
        ui_printf("  [6] Agudos  +%.0f dB     [7] Agudos  -%.0f dB\n",
                  (double)EQ_DB_STEP, (double)EQ_DB_STEP);
        ui_printf("  [8] Restablecer (0 dB en todas las bandas)\n");
        ui_printf("  [0] Volver\n\n");

        int op = input_int("  Opcion: ");
        salir_eq = procesar_opcion_ecualizador(op);
    }
}

/* ============================================================
 * Playlists
 * ============================================================ */

typedef struct
{
    char nombre[MAX_PLAYLIST_NAME];
} NombrePlaylist;

static int comparar_playlists_por_nombre(const void *a, const void *b)
{
    const NombrePlaylist *pa = (const NombrePlaylist *)a;
    const NombrePlaylist *pb = (const NombrePlaylist *)b;
    return musica_compare_ci(pa->nombre, pb->nombre);
}

static void ordenar_playlists(NombrePlaylist *lista, int cantidad)
{
    if (lista && cantidad > 1)
    {
        qsort(lista, (size_t)cantidad, sizeof(lista[0]),
              comparar_playlists_por_nombre);
    }
}

/** Escanea Musica/ en busca de .txt (playlists).
    Retorna numero de playlists encontradas (max MAX_PISTAS). */
static int escanear_playlists(NombrePlaylist *lista, int max)
{
    int n = 0;
    crear_dir_musica();

#ifdef _WIN32
    char patron[MAX_RUTA];
    snprintf(patron, sizeof(patron), "%s\\*.txt", MUSICA_DIR);
    WIN32_FIND_DATAA fd;
    HANDLE hf = FindFirstFileA(patron, &fd);
    if (hf == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    do
    {
        if (n >= max)
        {
            break;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }
        snprintf(lista[n].nombre, MAX_PLAYLIST_NAME, "%.*s", MAX_PLAYLIST_NAME - 1, fd.cFileName);
        n++;
    }
    while (FindNextFileA(hf, &fd));
    FindClose(hf);
#else
    DIR *dir = opendir(MUSICA_DIR);
    if (!dir) return 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && n < max)
    {
        if (!musica_es_txt_playlist(entry->d_name)) continue;
        snprintf(lista[n].nombre, MAX_PLAYLIST_NAME, "%s", entry->d_name);
        n++;
    }
    closedir(dir);
#endif

    ordenar_playlists(lista, n);
    return n;
}

static void construir_ruta_playlist(const char *nombre_txt, char *ruta, size_t ruta_sz)
{
#ifdef _WIN32
    snprintf(ruta, ruta_sz, "%s\\%s", MUSICA_DIR, nombre_txt);
#else
    snprintf(ruta, ruta_sz, "%s/%s", MUSICA_DIR, nombre_txt);
#endif
}

static int guardar_nombres_playlist(const char *nombre_txt,
                                    char nombres[][MAX_NOMBRE],
                                    int cantidad)
{
    char ruta[MAX_RUTA * 2];
    construir_ruta_playlist(nombre_txt, ruta, sizeof(ruta));

    FILE *f = NULL;
    FOPEN_PORTABLE(f, ruta, "w");
    if (!f)
    {
        return 0;
    }

    fprintf(f, "# MiFutbolC Playlist\n");
    for (int i = 0; i < cantidad; i++)
    {
        fprintf(f, "%s\n", nombres[i]);
    }
    fclose(f);
    return 1;
}

static int cargar_nombres_playlist(const char *nombre_txt,
                                   char nombres[][MAX_NOMBRE],
                                   int max)
{
    char ruta[MAX_RUTA * 2];
    construir_ruta_playlist(nombre_txt, ruta, sizeof(ruta));

    FILE *f = NULL;
    FOPEN_PORTABLE(f, ruta, "r");
    if (!f)
    {
        return -1;
    }

    int n = 0;
    char linea[MAX_NOMBRE];
    while (fgets(linea, (int)sizeof(linea), f) != NULL && n < max)
    {
        size_t ln = strlen_s(linea, sizeof(linea));
        while (ln > 0 && (linea[ln - 1] == '\n' || linea[ln - 1] == '\r'))
        {
            linea[--ln] = '\0';
        }

        if (linea[0] == '\0' || linea[0] == '#')
        {
            continue;
        }
        if (!musica_es_audio_soportado(linea))
        {
            continue;
        }

        strncpy_s(nombres[n], MAX_NOMBRE, linea, _TRUNCATE);
        n++;
    }
    fclose(f);
    return n;
}

static int buscar_tema_en_playlist(char nombres[][MAX_NOMBRE], int n, const char *tema)
{
    for (int i = 0; i < n; i++)
    {
        if (strcmp(nombres[i], tema) == 0)
        {
            return i;
        }
    }
    return -1;
}

static void mostrar_selector_crear_playlist(const char *nombre,
        const int seleccionadas[],
        int total_sel)
{
    clear_screen();
    print_header("Crear Playlist (Seleccion de Temas)");
    ui_printf("  Playlist: %s\n", nombre);
    ui_printf("  Seleccionadas: %d\n\n", total_sel);

    for (int i = 0; i < g_num_pistas; i++)
    {
        ui_printf("  [%c] %3d. %s\n", seleccionadas[i] ? 'x' : ' ', i + 1, g_pistas[i].nombre);
    }

    ui_printf("\n  Use numero para marcar/desmarcar tema\n");
    ui_printf("  [0] Guardar playlist   [-1] Cancelar\n\n");
}

/* Retorna: -1 cancelar, 0 continuar, 1 listo para guardar, 2 sin seleccion */
static int procesar_opcion_selector_playlist(int op,
        int seleccionadas[],
        int *total_sel)
{
    if (op == -1)
    {
        return -1;
    }

    if (op == 0)
    {
        return (*total_sel > 0) ? 1 : 2;
    }

    if (op < 1 || op > g_num_pistas)
    {
        return 0;
    }

    int idx = op - 1;
    if (seleccionadas[idx])
    {
        seleccionadas[idx] = 0;
        (*total_sel)--;
    }
    else
    {
        seleccionadas[idx] = 1;
        (*total_sel)++;
    }
    return 0;
}

static int construir_lista_temas_seleccionados(const int seleccionadas[],
        char temas[][MAX_NOMBRE])
{
    int n = 0;
    for (int i = 0; i < g_num_pistas; i++)
    {
        if (n >= g_num_pistas)
        {
            break;
        }
        if (!seleccionadas[i])
        {
            continue;
        }

        strncpy_s(temas[n], MAX_NOMBRE, g_pistas[i].nombre, _TRUNCATE);
        n++;
    }
    return n;
}

static void mostrar_temas_playlist_actual(char temas[][MAX_NOMBRE], int n)
{
    if (n == 0)
    {
        ui_printf("  (Sin temas)\n");
        return;
    }

    for (int i = 0; i < n; i++)
    {
        ui_printf("  %3d. %s\n", i + 1, temas[i]);
    }
}

static void editar_playlist_agregar_tema(char temas[][MAX_NOMBRE], int *n)
{
    if (g_num_pistas <= 0)
    {
        ui_printf("  No hay temas disponibles en la carpeta Musica/.\n");
        pause_console();
        return;
    }
    if (*n >= g_num_pistas)
    {
        ui_printf("  La playlist ya alcanzo el maximo de temas.\n");
        pause_console();
        return;
    }

    clear_screen();
    print_header("Agregar Tema a Playlist");
    for (int i = 0; i < g_num_pistas; i++)
    {
        ui_printf("  %3d. %s\n", i + 1, g_pistas[i].nombre);
    }
    ui_printf("\n");

    int sel = input_int("  Tema a agregar (0=cancelar): ");
    if (sel <= 0 || sel > g_num_pistas)
    {
        return;
    }

    const char *tema = g_pistas[sel - 1].nombre;
    if (buscar_tema_en_playlist(temas, *n, tema) >= 0)
    {
        ui_printf("  Ese tema ya esta en la playlist.\n");
        pause_console();
        return;
    }

    strncpy_s(temas[*n], MAX_NOMBRE, tema, _TRUNCATE);
    (*n)++;
    ui_printf("  Tema agregado.\n");
    pause_console();
}

static void editar_playlist_quitar_tema(char temas[][MAX_NOMBRE], int *n)
{
    if (*n <= 0)
    {
        ui_printf("  No hay temas para quitar.\n");
        pause_console();
        return;
    }

    int sel = input_int("  Tema a quitar (0=cancelar): ");
    if (sel <= 0 || sel > *n)
    {
        return;
    }

    int idx = sel - 1;
    for (int i = idx; i < *n - 1; i++)
    {
        strncpy_s(temas[i], MAX_NOMBRE, temas[i + 1], _TRUNCATE);
    }
    (*n)--;

    ui_printf("  Tema quitado.\n");
    pause_console();
}

static void editar_playlist_guardar(const char *nombre_txt,
                                    char temas[][MAX_NOMBRE],
                                    int n)
{
    if (guardar_nombres_playlist(nombre_txt, temas, n))
    {
        ui_printf("  Cambios guardados correctamente.\n");
    }
    else
    {
        ui_printf("  Error: no se pudo guardar la playlist.\n");
    }
    pause_console();
}

/** Crea una playlist seleccionando canciones concretas del catalogo actual */
static void guardar_playlist(void)
{
    if (g_num_pistas == 0)
    {
        ui_printf("  No hay pistas cargadas para guardar.\n");
        pause_console();
        return;
    }

    char nombre[MAX_PLAYLIST_NAME] = {0};
    input_string("  Nombre de la playlist (sin .txt): ", nombre, (int)sizeof(nombre));
    if (nombre[0] == '\0')
    {
        ui_printf("  Operacion cancelada.\n");
        pause_console();
        return;
    }
    if (!musica_es_txt_playlist(nombre) && strcat_s(nombre, sizeof(nombre), ".txt") != 0)
    {
        ui_printf("  Error: nombre de playlist demasiado largo.\n");
        pause_console();
        return;
    }

    int *seleccionadas = calloc((size_t)g_num_pistas, sizeof(int));
    if (!seleccionadas)
    {
        pause_console();
        return;
    }
    int total_sel = 0;
    int listo_para_guardar = 0;

    while (!listo_para_guardar)
    {
        mostrar_selector_crear_playlist(nombre, seleccionadas, total_sel);
        int op = input_int("  Opcion: ");

        int estado = procesar_opcion_selector_playlist(op, seleccionadas, &total_sel);
        if (estado == -1)
        {
            ui_printf("  Operacion cancelada.\n");
            free(seleccionadas);
            pause_console();
            return;
        }
        if (estado == 2)
        {
            ui_printf("\n  Debe seleccionar al menos un tema.\n");
            pause_console();
            continue;
        }
        if (estado == 1)
        {
            listo_para_guardar = 1;
        }
    }

    char (*temas)[MAX_NOMBRE] = malloc((size_t)g_num_pistas * sizeof(*temas));
    if (!temas)
    {
        free(seleccionadas);
        pause_console();
        return;
    }
    int n = construir_lista_temas_seleccionados(seleccionadas, temas);
    free(seleccionadas);

    if (!guardar_nombres_playlist(nombre, temas, n))
    {
        ui_printf("  Error: no se pudo guardar la playlist.\n");
        free(temas);
        pause_console();
        return;
    }

    ui_printf("  Playlist guardada: %s (%d pista/s)\n", nombre, n);
    free(temas);
    pause_console();
}

static void editar_playlist_archivo(const char *nombre_txt)
{
    if (g_num_pistas <= 0)
    {
        pause_console();
        return;
    }
    char (*temas)[MAX_NOMBRE] = malloc((size_t)g_num_pistas * sizeof(*temas));
    if (!temas)
    {
        pause_console();
        return;
    }

    int n = cargar_nombres_playlist(nombre_txt, temas, g_num_pistas);
    if (n < 0)
    {
        ui_printf("  Error: no se pudo abrir '%s'.\n", nombre_txt);
        free(temas);
        pause_console();
        return;
    }

    int salir = 0;
    while (!salir)
    {
        clear_screen();
        print_header("Editar Playlist");
        ui_printf("  Playlist: %s\n\n", nombre_txt);

        mostrar_temas_playlist_actual(temas, n);

        ui_printf("\n  [1] Agregar tema\n");
        ui_printf("  [2] Quitar tema\n");
        ui_printf("  [3] Guardar cambios\n");
        ui_printf("  [0] Volver (sin guardar)\n\n");

        int op = input_int("  Opcion: ");
        switch (op)
        {
        case 1:
            editar_playlist_agregar_tema(temas, &n);
            break;
        case 2:
            editar_playlist_quitar_tema(temas, &n);
            break;
        case 3:
            editar_playlist_guardar(nombre_txt, temas, n);
            salir = 1;
            break;
        case 0:
            salir = 1;
            break;
        default:
            break;
        }
    }
    free(temas);
}

/** Carga una playlist .txt: sustituye g_pistas con los archivos listados
    que realmente existan en Musica/ */
static void cargar_playlist_archivo(const char *nombre_txt)
{
    char ruta[MAX_RUTA * 2];
#ifdef _WIN32
    snprintf(ruta, sizeof(ruta), "%s\\%s", MUSICA_DIR, nombre_txt);
#else
    snprintf(ruta, sizeof(ruta), "%s/%s", MUSICA_DIR, nombre_txt);
#endif

    FILE *f = NULL;
    FOPEN_PORTABLE(f, ruta, "r");
    if (!f)
    {
        ui_printf("  Error: no se pudo abrir la playlist '%s'.\n", nombre_txt);
        pause_console();
        return;
    }

    /* Detener reproduccion actual para reemplazar el catalogo con la playlist. */
    descargar_sonido();
    g_pista_actual = -1;
    g_num_pistas   = 0;

    char linea[MAX_NOMBRE];
    int  cargadas = 0;
    while (fgets(linea, (int)sizeof(linea), f) != NULL)
    {
        /* Quitar salto de linea */
        size_t ln = strlen_s(linea, sizeof(linea));
        while (ln > 0 && (linea[ln - 1] == '\n' || linea[ln - 1] == '\r'))
        {
            linea[--ln] = '\0';
        }

        if (linea[0] == '#' || linea[0] == '\0')
        {
            continue; /* Comentarios y lineas vacias */
        }

        if (!musica_es_audio_soportado(linea))
        {
            continue;
        }

        /* Verificar que el archivo exista */
        const char *music_dir = MUSICA_DIR;
        char ruta_audio[MAX_RUTA];
        size_t max_nombre_len = 0;
        size_t dir_len = strlen_s(music_dir, (size_t)MAX_RUTA * 4);
        if (sizeof(ruta_audio) > dir_len + 2)
        {
            max_nombre_len = sizeof(ruta_audio) - dir_len - 2;
        }
        if (max_nombre_len == 0)
        {
            continue;
        }
#ifdef _WIN32
        snprintf(ruta_audio, sizeof(ruta_audio), "%s\\%.*s", music_dir, (int)max_nombre_len, linea);
#else
        snprintf(ruta_audio, sizeof(ruta_audio), "%s/%.*s", music_dir, (int)max_nombre_len, linea);
#endif
        FILE *chk = NULL;
        FOPEN_PORTABLE(chk, ruta_audio, "rb");
        if (!chk)
        {
            continue;
        }
        fclose(chk);

        if (cargadas >= g_cap_pistas && !pistas_grow())
        {
            break;
        }
        snprintf(g_pistas[cargadas].nombre, MAX_NOMBRE, "%.*s", MAX_NOMBRE - 1, linea);
        snprintf(g_pistas[cargadas].ruta,   MAX_RUTA,   "%.*s", MAX_RUTA - 1, ruta_audio);
        cargadas++;
    }
    fclose(f);
    g_num_pistas = cargadas;

    snprintf(g_playlist_activa, sizeof(g_playlist_activa), "%s", nombre_txt);
    ui_printf("  Playlist '%s' cargada: %d pista(s) válidas.\n",
              nombre_txt, g_num_pistas);
    pause_console();
}

static void iniciar_playlist_cargada(int aleatorio)
{
    if (g_num_pistas <= 0)
    {
        ui_printf("  La playlist no tiene pistas validas para reproducir.\n");
        pause_console();
        return;
    }

    if (aleatorio)
    {
        g_modo_rep = REPETIR_ALEATORIO;
        settings_set_music_repeat_mode((int)g_modo_rep);
        g_rand_seed ^= (g_num_pistas * 2654435761u);
        int idx = siguiente_pista_aleatoria();
        if (!cargar_pista(idx))
        {
            return;
        }
        ma_sound_start(&g_sonido);
        g_estado = ESTADO_REPRODUCIENDO;
        return;
    }

    g_modo_rep = REPETIR_LISTA;
    settings_set_music_repeat_mode((int)g_modo_rep);
    reproducir();
}

/** Submenu: seleccionar y cargar una playlist */
static void pl_accion_cargar(const NombrePlaylist *lista, int n)
{
    if (n == 0)
    {
        ui_printf("  No hay playlists disponibles.\n");
        pause_console();
        return;
    }
    int sel = input_int("  Seleccione playlist (0=cancelar): ");
    if (sel <= 0 || sel > n)
    {
        return;
    }

    clear_screen();
    print_header("Cargar Playlist");
    ui_printf("  Playlist seleccionada: %s\n\n", lista[sel - 1].nombre);
    ui_printf("  [1] Solo cargar playlist\n");
    ui_printf("  [2] Cargar y reproducir (orden)\n");
    ui_printf("  [3] Cargar y reproducir (aleatorio)\n");
    ui_printf("  [0] Cancelar\n\n");

    int modo = input_int("  Opcion: ");
    if (modo <= 0 || modo > 3)
    {
        return;
    }

    cargar_playlist_archivo(lista[sel - 1].nombre);

    if (modo == 2)
    {
        iniciar_playlist_cargada(0);
    }
    else if (modo == 3)
    {
        iniciar_playlist_cargada(1);
    }
}

/** Submenu: seleccionar y editar una playlist existente */
static void pl_accion_editar(const NombrePlaylist *lista, int n)
{
    if (n == 0)
    {
        ui_printf("  No hay playlists disponibles.\n");
        pause_console();
        return;
    }

    int sel = input_int("  Seleccione playlist a editar (0=cancelar): ");
    if (sel <= 0 || sel > n)
    {
        return;
    }

    escanear_directorio();
    editar_playlist_archivo(lista[sel - 1].nombre);
}

/** Submenu: seleccionar y eliminar una playlist con confirmacion */
static void pl_accion_eliminar(const NombrePlaylist *lista, int n)
{
    if (n == 0)
    {
        ui_printf("  No hay playlists para eliminar.\n");
        pause_console();
        return;
    }
    int sel = input_int("  Seleccione playlist a eliminar (0=cancelar): ");
    if (sel <= 0 || sel > n)
    {
        return;
    }

    char r[MAX_RUTA * 2];
#ifdef _WIN32
    snprintf(r, sizeof(r), "%s\\%s", MUSICA_DIR, lista[sel - 1].nombre);
#else
    snprintf(r, sizeof(r), "%s/%s",  MUSICA_DIR, lista[sel - 1].nombre);
#endif
    ui_printf("  Eliminar '%s'? [s/n]: ", lista[sel - 1].nombre);
    char conf[4] = {0};
    input_string("", conf, (int)sizeof(conf));
    if (conf[0] == 's' || conf[0] == 'S')
    {
        if (remove(r) == 0)
        {
            ui_printf("  Playlist eliminada.\n");
        }
        else
        {
            ui_printf("  Error al eliminar.\n");
        }
        pause_console();
    }
}

/** Menu de playlists */
static void menu_playlists(void)
{
    int salir_pl = 0;
    while (!salir_pl)
    {
        clear_screen();
        print_header("Playlists");

        NombrePlaylist lista[MAX_PISTAS];
        int n = escanear_playlists(lista, MAX_PISTAS);

        if (n > 0)
        {
            ui_printf("  Playlists guardadas:\n");
            for (int i = 0; i < n; i++)
            {
                ui_printf("    %2d. %s\n", i + 1, lista[i].nombre);
            }
            ui_printf("\n");
        }
        else
        {
            ui_printf("  (No hay playlists guardadas en '%s/')\n\n", MUSICA_DIR);
        }

        ui_printf("  [1] Crear playlist (seleccionar temas)\n");
        ui_printf("  [2] Cargar playlist\n");
        ui_printf("  [3] Editar playlist (agregar/quitar temas)\n");
        ui_printf("  [4] Eliminar playlist\n");
        ui_printf("  [0] Volver\n\n");

        int op = input_int("  Opcion: ");
        switch (op)
        {
        case 1:
            guardar_playlist();
            break;
        case 2:
            pl_accion_cargar(lista, n);
            break;
        case 3:
            pl_accion_editar(lista, n);
            break;
        case 4:
            pl_accion_eliminar(lista, n);
            break;
        case 0:
            salir_pl = 1;
            break;
        default:
            break;
        }
    }
}

/* ============================================================
 * Posicion de reanudacion (resume)
 * ============================================================ */

static void construir_ruta_resume(char *buf, size_t sz)
{
#ifdef _WIN32
    snprintf(buf, sz, "%s\\.resume", MUSICA_DIR);
#else
    snprintf(buf, sz, "%s/.resume", MUSICA_DIR);
#endif
}

static void guardar_posicion_resume(void)
{
    if (!g_sonido_listo || g_pista_actual < 0 || g_pista_actual >= g_num_pistas)
    {
        return;
    }
    if (g_estado == ESTADO_DETENIDO)
    {
        return;
    }

    ma_uint64 cur = 0;
    ma_sound_get_cursor_in_pcm_frames(&g_sonido, &cur);

    char ruta[MAX_RUTA];
    construir_ruta_resume(ruta, sizeof(ruta));

    FILE *f = NULL;
    FOPEN_PORTABLE(f, ruta, "w");
    if (!f)
    {
        return;
    }
    fprintf(f, "%s\n%llu\n", g_pistas[g_pista_actual].nombre,
            (unsigned long long)cur);
    fclose(f);
}

static void restaurar_posicion_resume(void)
{
    char ruta[MAX_RUTA];
    construir_ruta_resume(ruta, sizeof(ruta));

    FILE *f = NULL;
    FOPEN_PORTABLE(f, ruta, "r");
    if (!f)
    {
        return;
    }

    char nombre_guardado[MAX_NOMBRE] = {0};
    if (!fgets(nombre_guardado, (int)sizeof(nombre_guardado), f))
    {
        fclose(f);
        return;
    }
    size_t ln = strnlen_s(nombre_guardado, sizeof(nombre_guardado));
    while (ln > 0 && (nombre_guardado[ln - 1] == '\n' || nombre_guardado[ln - 1] == '\r'))
    {
        nombre_guardado[--ln] = '\0';
    }

    char frame_str[32] = {0};
    unsigned long long frame_guardado = 0;
    if (fgets(frame_str, (int)sizeof(frame_str), f))
    {
        frame_guardado = strtoull(frame_str, NULL, 10);
    }

    fclose(f);
    remove(ruta); /* Consumir el archivo una sola vez */

    int indice = -1;
    for (int i = 0; i < g_num_pistas; i++)
    {
        if (strcmp(g_pistas[i].nombre, nombre_guardado) == 0)
        {
            indice = i;
            break;
        }
    }
    if (indice < 0)
    {
        return;
    }

    if (!cargar_pista(indice))
    {
        return;
    }

    if (frame_guardado > 0)
    {
        ma_uint64 len = 0;
        ma_sound_get_length_in_pcm_frames(&g_sonido, &len);
        if (len > 0 && frame_guardado > len)
        {
            frame_guardado = 0;
        }
        ma_sound_seek_to_pcm_frame(&g_sonido, (ma_uint64)frame_guardado);
    }
    /* Pista cargada y posicionada; el usuario decide si reproducir */
}

/* ============================================================
 * API publica
 * ============================================================ */

void musica_cleanup(void)
{
    guardar_posicion_resume();

    if (g_sonido_listo)
    {
        ma_sound_stop(&g_sonido);
        ma_sound_uninit(&g_sonido);
        g_sonido_listo = 0;
    }

    /* Uninit EQ nodes antes del engine */
    if (g_eq_listo)
    {
        ma_hishelf_node_uninit(&g_eq_treble, NULL);
        ma_peak_node_uninit   (&g_eq_mid,    NULL);
        ma_loshelf_node_uninit(&g_eq_bass,   NULL);
        g_eq_listo  = 0;
        g_eq_activo = 0;
    }

    if (g_engine_listo)
    {
        ma_engine_uninit(&g_engine);
        g_engine_listo = 0;
    }

    g_estado = ESTADO_DETENIDO;
    g_pista_actual = -1;
}

void musica_asegurar_directorio(void)
{
    crear_dir_musica();
}

void musica_iniciar_automatica(void)
{
    if (g_estado == ESTADO_REPRODUCIENDO)
    {
        return;
    }

    if (!inicializar_engine())
    {
        return;
    }

    escanear_directorio();
    if (g_num_pistas <= 0)
    {
        return;
    }

    if ((!g_sonido_listo || g_pista_actual < 0) && !cargar_pista(0))
    {
        return;
    }

    ma_sound_start(&g_sonido);
    g_estado = ESTADO_REPRODUCIENDO;
}

static void info_pista_menu(void)
{
    clear_screen();
    print_header("Informacion de Pista");

    if (!g_sonido_listo || g_pista_actual < 0)
    {
        ui_printf("  No hay ninguna pista cargada.\n");
        pause_console();
        return;
    }

    ma_uint64 len = 0;
    ma_sound_get_length_in_pcm_frames(&g_sonido, &len);
    ma_uint32 sr = ma_engine_get_sample_rate(&g_engine);
    ma_uint32 ch = ma_engine_get_channels(&g_engine);

    float dur_s = (sr > 0 && len > 0) ? (float)len / (float)sr : 0.0f;
    const char *ext = musica_obtener_extension_archivo(g_pistas[g_pista_actual].nombre);

    ui_printf("  Nombre     : %s\n", g_pistas[g_pista_actual].nombre);
    ui_printf("  Duracion   : %02d:%02d (%llu frames)\n",
              (int)dur_s / 60, (int)dur_s % 60, (unsigned long long)len);
    ui_printf("  Formato    : %s\n", ext ? ext + 1 : "desconocido");
    ui_printf("  Sample Rate: %u Hz\n", (unsigned)sr);
    ui_printf("  Canales    : %u\n", (unsigned)ch);
    ui_printf("  Ruta       : %s\n", g_pistas[g_pista_actual].ruta);
    pause_console();
}

static int mostrar_selector_pista_renombrar(void)
{
    for (int i = 0; i < g_num_pistas; i++)
    {
        const char *marca = (i == g_pista_actual && g_sonido_listo) ? " [CARGADA]" : "";
        ui_printf("  %3d. %s%s\n", i + 1, g_pistas[i].nombre, marca);
    }
    ui_printf("\n");

    int sel = input_int("  Pista a renombrar (0=cancelar): ");
    if (sel <= 0 || sel > g_num_pistas)
    {
        return -1;
    }

    return sel - 1;
}

static int puede_renombrar_pista(int idx)
{
    if (g_sonido_listo && g_pista_actual == idx && g_estado == ESTADO_REPRODUCIENDO)
    {
        ui_printf("  No se puede renombrar la pista en reproduccion. Pausela primero.\n");
        pause_console();
        return 0;
    }

    return 1;
}

static int solicitar_nombre_nuevo_pista(char *nombre_nuevo, size_t size)
{
    if (!nombre_nuevo || size == 0)
    {
        return 0;
    }

    input_string("  Nuevo nombre (con extension): ", nombre_nuevo, (int)size);
    if (nombre_nuevo[0] == '\0')
    {
        ui_printf("  Operacion cancelada.\n");
        pause_console();
        return 0;
    }

    if (!musica_es_audio_soportado(nombre_nuevo))
    {
        ui_printf("  Error: extension no soportada. Use %s\n", AUDIO_FORMATOS_TEXTO);
        pause_console();
        return 0;
    }

    return 1;
}

static void construir_ruta_musica_archivo(const char *nombre_archivo, char *ruta, size_t size)
{
#ifdef _WIN32
    snprintf(ruta, size, "%s\\%s", MUSICA_DIR, nombre_archivo);
#else
    snprintf(ruta, size, "%s/%s", MUSICA_DIR, nombre_archivo);
#endif
}

static int ejecutar_renombrado_pista(int idx, const char *nombre_nuevo)
{
    char ruta_nueva[MAX_RUTA];
    construir_ruta_musica_archivo(nombre_nuevo, ruta_nueva, sizeof(ruta_nueva));

    if (rename(g_pistas[idx].ruta, ruta_nueva) != 0)
    {
        ui_printf("  Error al renombrar el archivo.\n");
        pause_console();
        return 0;
    }

    strncpy_s(g_pistas[idx].nombre, MAX_NOMBRE, nombre_nuevo, _TRUNCATE);
    strncpy_s(g_pistas[idx].ruta,   MAX_RUTA,   ruta_nueva,  _TRUNCATE);
    ui_printf("  Pista renombrada correctamente: %s\n", nombre_nuevo);
    pause_console();
    return 1;
}

static void renombrar_pista_menu(void)
{
    clear_screen();
    print_header("Renombrar Pista");

    if (g_num_pistas == 0)
    {
        ui_printf("  No hay pistas disponibles.\n");
        pause_console();
        return;
    }

    int idx = mostrar_selector_pista_renombrar();
    if (idx < 0)
    {
        return;
    }

    if (!puede_renombrar_pista(idx))
    {
        return;
    }

    ui_printf("  Nombre actual: %s\n", g_pistas[idx].nombre);
    char nombre_nuevo[MAX_NOMBRE] = {0};
    if (!solicitar_nombre_nuevo_pista(nombre_nuevo, sizeof(nombre_nuevo)))
    {
        return;
    }

    (void)ejecutar_renombrado_pista(idx, nombre_nuevo);
}

static int pista_aplica_filtro_exportacion(int indice, int usar_filtro)
{
    if (!usar_filtro)
    {
        return 1;
    }

    return musica_contiene_subcadena_ci(g_pistas[indice].nombre, g_filtro_busqueda);
}

static int contar_pistas_exportacion(int usar_filtro)
{
    int total = 0;
    for (int i = 0; i < g_num_pistas; i++)
    {
        if (pista_aplica_filtro_exportacion(i, usar_filtro))
        {
            total++;
        }
    }

    return total;
}

static void mostrar_resumen_exportacion_playlist(int usar_filtro, int total)
{
    if (usar_filtro)
    {
        ui_printf("  Se exportaran %d pistas (filtro: \"%s\")\n\n", total, g_filtro_busqueda);
        return;
    }

    ui_printf("  Se exportaran %d pistas (catalogo completo)\n\n", total);
}

static int solicitar_nombre_playlist_exportacion(char *nombre, size_t size)
{
    input_string("  Nombre de la playlist (sin .txt): ", nombre, (int)size);
    if (nombre[0] == '\0')
    {
        ui_printf("  Operacion cancelada.\n");
        pause_console();
        return 0;
    }

    if (!musica_es_txt_playlist(nombre) && strcat_s(nombre, size, ".txt") != 0)
    {
        ui_printf("  Error: nombre demasiado largo.\n");
        pause_console();
        return 0;
    }

    return 1;
}

static int recolectar_temas_exportacion(int usar_filtro, char temas_exp[][MAX_NOMBRE], int max_temas)
{
    int n = 0;

    for (int i = 0; i < g_num_pistas; i++)
    {
        if (n >= max_temas)
        {
            break;
        }

        if (!pista_aplica_filtro_exportacion(i, usar_filtro))
        {
            continue;
        }

        strncpy_s(temas_exp[n], MAX_NOMBRE, g_pistas[i].nombre, _TRUNCATE);
        n++;
    }

    return n;
}

static void exportar_catalogo_playlist_menu(void)
{
    clear_screen();
    print_header("Exportar Catalogo como Playlist");

    if (g_num_pistas == 0)
    {
        ui_printf("  No hay pistas en el catalogo.\n");
        pause_console();
        return;
    }

    int usar_filtro = (g_filtro_busqueda[0] != '\0');
    int total = contar_pistas_exportacion(usar_filtro);
    mostrar_resumen_exportacion_playlist(usar_filtro, total);

    char nombre[MAX_PLAYLIST_NAME] = {0};
    if (!solicitar_nombre_playlist_exportacion(nombre, sizeof(nombre)))
    {
        return;
    }

    static char temas_exp[MAX_PISTAS][MAX_NOMBRE];
    int n = recolectar_temas_exportacion(usar_filtro, temas_exp, MAX_PISTAS);

    if (guardar_nombres_playlist(nombre, temas_exp, n))
    {
        ui_printf("  Playlist exportada: %s (%d pista/s)\n", nombre, n);
    }
    else
    {
        ui_printf("  Error: no se pudo guardar la playlist.\n");
    }
    pause_console();
}

static void accion_paso_volumen_menu(void)
{
    clear_screen();
    print_header("Paso de Volumen");

    ui_printf("  Paso actual : %.0f%%\n\n", (double)(g_paso_volumen * 100.0f));
    ui_printf("  [1]  1%% (ajuste fino)\n");
    ui_printf("  [2]  5%% (ajuste moderado)\n");
    ui_printf("  [3] 10%% (ajuste rapido) [default]\n");
    ui_printf("  [4] 20%% (ajuste maximo)\n");
    ui_printf("  [0] Cancelar\n\n");

    int op = input_int("  Opcion: ");
    float nuevo_paso;
    switch (op)
    {
    case 1:
        nuevo_paso = 0.01f;
        break;
    case 2:
        nuevo_paso = 0.05f;
        break;
    case 3:
        nuevo_paso = 0.10f;
        break;
    case 4:
        nuevo_paso = 0.20f;
        break;
    case 0:
        return;
    default:
        ui_printf("  Opcion invalida.\n");
        pause_console();
        return;
    }
    g_paso_volumen = nuevo_paso;
    settings_set_music_volume_step(g_paso_volumen);
    ui_printf("  Paso de volumen: %.0f%%\n", (double)(g_paso_volumen * 100.0f));
    pause_console();
}

static void accion_reproducir_pausar(void)
{
    if (g_estado == ESTADO_REPRODUCIENDO)
    {
        pausar();
    }
    else
    {
        reproducir();
    }
}

static void accion_cambiar_modo_repeticion(void)
{
    g_modo_rep = (ModoRepeticion)((g_modo_rep + 1) % 4);
    settings_set_music_repeat_mode((int)g_modo_rep);
    if (g_modo_rep == REPETIR_ALEATORIO)
    {
        g_rand_seed ^= (unsigned int)(g_pista_actual + 1) * 2654435761u;
    }
}

static void accion_musica_al_iniciar(void)
{
    settings_set_music_autoplay(settings_get_music_autoplay() ? 0 : 1);
    ui_printf("  Musica al iniciar: %s\n",
              settings_get_music_autoplay() ? "ACTIVADA" : "DESACTIVADA");
    pause_console();
}

static void accion_actualizar_lista(void)
{
    int estaba_reproduciendo = (g_estado == ESTADO_REPRODUCIENDO);
    descargar_sonido();
    g_pista_actual = -1;
    escanear_directorio();
    ui_printf("  Lista actualizada: %d pista(s) encontrada(s).\n", g_num_pistas);
    if (estaba_reproduciendo)
    {
        ui_printf("  La pista fue detenida al recargar la lista.\n");
    }
    pause_console();
}

typedef void (*AccionMenuMusicaFn)(void);

static int procesar_opcion_menu_musica(int opcion)
{
    typedef struct
    {
        int opcion;
        AccionMenuMusicaFn fn;
    } OpcionMenuMusica;

    if (opcion == 0)
    {
        return 1;
    }
    if (opcion == 8)
    {
        accion_cambiar_modo_repeticion();
        return 0;
    }
    if (opcion == 9)
    {
        accion_actualizar_lista();
        return 0;
    }
    if (opcion == 14)
    {
        accion_musica_al_iniciar();
        return 0;
    }

    static const OpcionMenuMusica opciones[] =
    {
        {1,  accion_reproducir_pausar},
        {2,  detener},
        {3,  pista_anterior},
        {4,  siguiente_pista},
        {5,  mostrar_lista_pistas},
        {6,  subir_volumen},
        {7,  bajar_volumen},
        {10, agregar_cancion_menu},
        {11, eliminar_cancion_menu},
        {12, menu_ecualizador},
        {13, menu_playlists},
        {15, buscar_pista_por_nombre_menu},
        {16, retroceder_10s},
        {17, avanzar_10s},
        {18, limpiar_filtro_busqueda},
        {19, accion_sleep_timer},
        {20, seek_a_tiempo_exacto_menu},
        {21, info_pista_menu},
        {22, renombrar_pista_menu},
        {23, exportar_catalogo_playlist_menu},
        {24, accion_paso_volumen_menu}
    };

    for (int i = 0; i < (int)(sizeof(opciones) / sizeof(opciones[0])); i++)
    {
        if (opciones[i].opcion == opcion)
        {
            opciones[i].fn();
            return 0;
        }
    }

    ui_printf("  Opcion invalida.\n");
    pause_console();
    return 0;
}

void menu_musica(void)
{
    /* Iniciar motor de audio la primera vez */
    if (!inicializar_engine())
    {
        pause_console();
        return;
    }

    /* Escanear directorio al entrar */
    escanear_directorio();

    /* Restaurar posicion guardada (solo la primera vez en esta sesion) */
    if (!g_resume_restaurado)
    {
        restaurar_posicion_resume();
        g_resume_restaurado = 1;
    }

    int salir = 0;
    while (!salir)
    {
        procesar_fade_pendiente();
        procesar_sleep_timer();
        verificar_fin_pista();
        dibujar_reproductor();

        int opcion = input_int("  Opcion: ");
        salir = procesar_opcion_menu_musica(opcion);
    }
    guardar_posicion_resume();
    /* El audio sigue reproduciendose en segundo plano al volver al menu */
}
