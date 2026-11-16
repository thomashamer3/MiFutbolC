#ifndef OPENMETEO_CLIENT_H
#define OPENMETEO_CLIENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    double latitud;
    double longitud;
    int anio;
    int mes;
    int dia;
    int hora;
    int minuto;
} OpenMeteoParams;

typedef struct
{
    double temp_c;
    double apparent_temp_c;
    double precip_mm;
    double wind_kmh;
    int humidity;
    int weather_code;
    char *clima_json;
    int has_sun_data;
    int sunrise_hour;
    int sunrise_minute;
    int sunset_hour;
    int sunset_minute;
} OpenMeteoResult;

/**
 * @brief Consulta Open-Meteo (bloqueante, curl) y rellena @p out_result.
 * @return 1 si hay datos validos, 0 si fallo o el dia aun no esta publicado.
 */
int openmeteo_fetch(const OpenMeteoParams *params, OpenMeteoResult *out_result);

/**
 * @brief Arma la URL del archivo historico de Open-Meteo para @p params.
 * @return 1 si la URL entra en @p url; 0 si los parametros son invalidos,
 *         la fecha es futura o el buffer es chico.
 */
int openmeteo_build_url(const OpenMeteoParams *params, char *url, size_t tam);

/**
 * @brief Interpreta la respuesta JSON de Open-Meteo.
 *
 * Rellena promedios, moda de weather_code, datos de sol y clima_json (liberar
 * con openmeteo_result_free). Devuelve 0 si el JSON es invalido o si ninguna
 * hora trae temperature_2m numerica (datos aun no publicados).
 */
int openmeteo_parse(const char *json, OpenMeteoResult *out_result);

/** @brief 1 si la fecha de @p params es posterior a hoy (hora local). */
int openmeteo_fecha_es_futura(const OpenMeteoParams *params);

/** @brief Activa (1) o desactiva (0) la salida por consola del cliente. */
void openmeteo_set_silencioso(int silencioso);

void openmeteo_result_free(OpenMeteoResult *result);

#ifdef __cplusplus
}
#endif

#endif
