#include "musica_helpers.h"
#include "utils.h"

#include <ctype.h>
#include <stddef.h>

int musica_extension_igual_ci(const char *ext, const char *ref)
{
    if (!ext || !ref)
    {
        return 0;
    }

    while (*ext && *ref)
    {
        if (tolower((unsigned char)*ext) != tolower((unsigned char)*ref))
        {
            return 0;
        }
        ext++;
        ref++;
    }
    return (*ext == '\0' && *ref == '\0');
}

const char *musica_obtener_extension_archivo(const char *nombre)
{
    if (!nombre || nombre[0] == '\0')
    {
        return NULL;
    }

    const char *p = nombre;
    const char *ultimo_punto = NULL;
    while (*p)
    {
        if (*p == '/' || *p == '\\')
        {
            ultimo_punto = NULL;
        }
        else if (*p == '.')
        {
            ultimo_punto = p;
        }
        p++;
    }

    if (!ultimo_punto || ultimo_punto[1] == '\0')
    {
        return NULL;
    }
    return ultimo_punto;
}

int musica_es_audio_soportado(const char *nombre)
{
    const char *ext = musica_obtener_extension_archivo(nombre);
    if (!ext)
    {
        return 0;
    }

    if (musica_extension_igual_ci(ext, ".mp3") ||
            musica_extension_igual_ci(ext, ".wav") ||
            musica_extension_igual_ci(ext, ".flac"))
    {
        return 1;
    }

#ifdef MA_HAS_VORBIS
    if (musica_extension_igual_ci(ext, ".ogg"))
    {
        return 1;
    }
#endif

    return 0;
}

int musica_es_txt_playlist(const char *nombre)
{
    size_t len = strlen_s(nombre, 512);
    if (len < 5)
    {
        return 0;
    }

    const char *ext = nombre + len - 4;
    return (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 't' &&
            tolower((unsigned char)ext[2]) == 'x' &&
            tolower((unsigned char)ext[3]) == 't');
}

const char *musica_basename_portable(const char *ruta)
{
    if (!ruta)
    {
        return "";
    }

    const char *p = ruta;
    const char *ultimo = ruta;
    while (*p)
    {
        if (*p == '/' || *p == '\\')
        {
            ultimo = p + 1;
        }
        p++;
    }
    return ultimo;
}

int musica_compare_ci(const char *a, const char *b)
{
    if (!a && !b)
    {
        return 0;
    }
    if (!a)
    {
        return -1;
    }
    if (!b)
    {
        return 1;
    }

    while (*a && *b)
    {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb)
        {
            return ca - cb;
        }
        a++;
        b++;
    }

    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int musica_contiene_subcadena_ci(const char *texto, const char *patron)
{
    if (!texto || !patron)
    {
        return 0;
    }
    if (patron[0] == '\0')
    {
        return 1;
    }

    for (const char *t = texto; *t; ++t)
    {
        const char *a = t;
        const char *b = patron;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
        {
            a++;
            b++;
        }
        if (*b == '\0')
        {
            return 1;
        }
    }

    return 0;
}
