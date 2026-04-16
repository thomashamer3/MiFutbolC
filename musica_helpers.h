#ifndef MUSICA_HELPERS_H
#define MUSICA_HELPERS_H

int musica_extension_igual_ci(const char *ext, const char *ref);
const char *musica_obtener_extension_archivo(const char *nombre);
int musica_es_audio_soportado(const char *nombre);
int musica_es_txt_playlist(const char *nombre);
const char *musica_basename_portable(const char *ruta);
int musica_compare_ci(const char *a, const char *b);
int musica_contiene_subcadena_ci(const char *texto, const char *patron);

#endif /* MUSICA_HELPERS_H */
