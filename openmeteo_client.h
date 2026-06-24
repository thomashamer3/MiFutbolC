#ifndef OPENMETEO_CLIENT_H
#define OPENMETEO_CLIENT_H

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
} OpenMeteoResult;

int openmeteo_fetch(const OpenMeteoParams *params, OpenMeteoResult *out_result);

void openmeteo_result_free(OpenMeteoResult *result);

#ifdef __cplusplus
}
#endif

#endif
