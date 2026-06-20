#include "openmeteo_client.h"
#include "cJSON.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OWM_ARCHIVE_URL_BASE "https://archive-api.open-meteo.com/v1/archive"
#define WEATHER_CODE_COUNT 100
#define READ_BUF_SIZE 8192

static int weather_code_mode(const int *codes, int count)
{
    int freq[WEATHER_CODE_COUNT] = {0};
    for (int i = 0; i < count; i++)
    {
        if (codes[i] >= 0 && codes[i] < WEATHER_CODE_COUNT)
            freq[codes[i]]++;
    }
    int best = 0;
    int best_count = 0;
    for (int i = 0; i < WEATHER_CODE_COUNT; i++)
    {
        if (freq[i] > best_count)
        {
            best_count = freq[i];
            best = i;
        }
    }
    return best;
}

int openmeteo_fetch(const OpenMeteoParams *params, OpenMeteoResult *out_result)
{
    if (!params || !out_result)
        return 0;

    out_result->temp_c = 0.0;
    out_result->apparent_temp_c = 0.0;
    out_result->precip_mm = 0.0;
    out_result->wind_kmh = 0.0;
    out_result->weather_code = 0;
    out_result->clima_json = NULL;

    /* Validar que la fecha no sea futura */
    time_t ahora = time(NULL);
    struct tm tm_hoy;
#ifdef _WIN32
    localtime_s(&tm_hoy, &ahora);
#else
    localtime_r(&ahora, &tm_hoy);
#endif
    int hoy_anio = tm_hoy.tm_year + 1900;
    int hoy_mes  = tm_hoy.tm_mon + 1;
    int hoy_dia  = tm_hoy.tm_mday;

    if (params->anio > hoy_anio ||
            (params->anio == hoy_anio && params->mes > hoy_mes) ||
            (params->anio == hoy_anio && params->mes == hoy_mes && params->dia > hoy_dia))
    {
        printf("Fecha futura (%04d-%02d-%02d): saltando consulta.\n",
               params->anio, params->mes, params->dia);
        return 0;
    }

    char date_str[11];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
             params->anio, params->mes, params->dia);

    /* LC_NUMERIC="C" garantiza separador decimal con punto */
    char lat_str[32];
    char lon_str[32];
    snprintf(lat_str, sizeof(lat_str), "%.6f", params->latitud);
    snprintf(lon_str, sizeof(lon_str), "%.6f", params->longitud);

    char url[512];
    snprintf(url, sizeof(url),
             "%s?latitude=%s&longitude=%s"
             "&start_date=%s&end_date=%s"
             "&hourly=temperature_2m,apparent_temperature,"
             "precipitation,weather_code,wind_speed_10m"
             "&timezone=auto",
             OWM_ARCHIVE_URL_BASE, lat_str, lon_str, date_str, date_str);

    /* Descargar con curl.exe */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "curl.exe -s --compressed --max-time 15 \"%s\" 2>&1", url);

    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        printf("Error: popen() fallo\n");
        return 0;
    }

    /* Leer toda la salida de curl en un buffer dinamico */
    size_t capacity = READ_BUF_SIZE;
    size_t total = 0;
    char *json_data = (char *)malloc(capacity);
    if (!json_data)
    {
        pclose(fp);
        return 0;
    }

    size_t nread;
    while ((nread = fread(json_data + total, 1, capacity - total, fp)) > 0)
    {
        total += nread;
        if (capacity - total < 1024)
        {
            capacity *= 2;
            char *tmp = (char *)realloc(json_data, capacity);
            if (!tmp)
            {
                free(json_data);
                pclose(fp);
                return 0;
            }
            json_data = tmp;
        }
    }

    int exit_code = pclose(fp);

    if (total == 0)
    {
        printf("Error: curl no devolvio datos (exit code %d)\n", exit_code);
        free(json_data);
        return 0;
    }

    json_data[total] = '\0';

    /* Saltar BOM UTF-8 si existe */
    char *json_ptr = json_data;
    if ((unsigned char)json_ptr[0] == 0xEF &&
            (unsigned char)json_ptr[1] == 0xBB &&
            (unsigned char)json_ptr[2] == 0xBF)
    {
        json_ptr += 3;
    }

    cJSON *root = cJSON_Parse(json_ptr);
    free(json_data);

    if (!root)
    {
        const char *err = cJSON_GetErrorPtr();
        if (err)
            printf("Error parseando JSON cerca de:\n%s\n", err);
        else
            printf("Error: JSON invalido (razon desconocida)\n");
        return 0;
    }

    cJSON *hourly = cJSON_GetObjectItem(root, "hourly");
    if (!hourly)
    {
        printf("Error: respuesta no contiene 'hourly'\n");
        cJSON_Delete(root);
        return 0;
    }

    cJSON *temp_arr = cJSON_GetObjectItem(hourly, "temperature_2m");
    cJSON *apparent_arr = cJSON_GetObjectItem(hourly, "apparent_temperature");
    cJSON *precip_arr = cJSON_GetObjectItem(hourly, "precipitation");
    cJSON *wind_arr = cJSON_GetObjectItem(hourly, "wind_speed_10m");
    cJSON *code_arr = cJSON_GetObjectItem(hourly, "weather_code");

    if (!temp_arr || !cJSON_IsArray(temp_arr))
    {
        printf("Error: 'temperature_2m' no encontrado o no es array\n");
        cJSON_Delete(root);
        return 0;
    }

    int count = cJSON_GetArraySize(temp_arr);
    if (count == 0)
    {
        printf("Error: array temperature_2m vacio\n");
        cJSON_Delete(root);
        return 0;
    }

    int valid_count = 0;
    int apparent_count = 0;
    double temp_sum = 0.0;
    double apparent_sum = 0.0;
    double precip_sum = 0.0;
    double wind_sum = 0.0;
    int *wcodes = (int *)malloc((size_t)count * sizeof(int));
    if (!wcodes)
    {
        cJSON_Delete(root);
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        cJSON *t = cJSON_GetArrayItem(temp_arr, i);
        if (t && cJSON_IsNumber(t))
        {
            temp_sum += t->valuedouble;
            valid_count++;
        }

        if (apparent_arr && cJSON_IsArray(apparent_arr))
        {
            cJSON *a = cJSON_GetArrayItem(apparent_arr, i);
            if (a && cJSON_IsNumber(a))
            {
                apparent_sum += a->valuedouble;
                apparent_count++;
            }
        }

        if (precip_arr && cJSON_IsArray(precip_arr))
        {
            cJSON *p = cJSON_GetArrayItem(precip_arr, i);
            if (p && cJSON_IsNumber(p))
                precip_sum += p->valuedouble;
        }

        if (wind_arr && cJSON_IsArray(wind_arr))
        {
            cJSON *w = cJSON_GetArrayItem(wind_arr, i);
            if (w && cJSON_IsNumber(w))
                wind_sum += w->valuedouble;
        }

        if (code_arr && cJSON_IsArray(code_arr))
        {
            cJSON *c = cJSON_GetArrayItem(code_arr, i);
            if (c && cJSON_IsNumber(c))
                wcodes[i] = (int)c->valuedouble;
            else
                wcodes[i] = -1;
        }
        else
        {
            wcodes[i] = -1;
        }
    }

    out_result->temp_c = (valid_count > 0) ? (temp_sum / valid_count) : 0.0;
    out_result->apparent_temp_c = (apparent_count > 0) ? (apparent_sum / apparent_count) : 0.0;
    out_result->precip_mm = precip_sum;
    out_result->wind_kmh = (valid_count > 0) ? (wind_sum / valid_count) : 0.0;
    out_result->weather_code = weather_code_mode(wcodes, count);
    free(wcodes);

    char *clima_str = cJSON_PrintUnformatted(root);
    if (clima_str)
    {
        out_result->clima_json = clima_str;
    }

    cJSON_Delete(root);
    return 1;
}

void openmeteo_result_free(OpenMeteoResult *result)
{
    if (result)
    {
        free(result->clima_json);
        result->clima_json = NULL;
    }
}
