#include "openmeteo_client.h"
#include "cJSON.h"
#include "utils.h"
#include <stdbool.h>
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
        {
            freq[codes[i]]++;
        }
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

static int es_fecha_futura(const OpenMeteoParams *params)
{
    time_t ahora = time(NULL);
    struct tm tm_hoy;
#ifdef _WIN32
    localtime_s(&tm_hoy, &ahora);
#else
    localtime_r(&ahora, &tm_hoy);
#endif
    int hoy_anio = tm_hoy.tm_year + 1900;
    int hoy_mes = tm_hoy.tm_mon + 1;
    int hoy_dia = tm_hoy.tm_mday;

    if (params->anio > hoy_anio || (params->anio == hoy_anio && params->mes > hoy_mes) ||
            (params->anio == hoy_anio && params->mes == hoy_mes && params->dia > hoy_dia))
    {
        printf("Fecha futura (%04d-%02d-%02d): saltando consulta.\n", params->anio, params->mes,
               params->dia);
        return 1;
    }
    return 0;
}

static bool contiene_shell_metachar(const char *s)
{
    if (!s) return true;
    while (*s)
    {
        unsigned char c = (unsigned char)*s;
        if (c <= 0x1F || c == 0x7F) return true;
        switch (c)
        {
        case ';':
        case '|':
        case '&':
        case '`':
        case '$':
        case '(':
        case ')':
        case '{':
        case '}':
        case '<':
        case '>':
        case '!':
        case '#':
        case '~':
        case '%':
        case '*':
        case '?':
        case '\\':
        case '\'':
        case '"':
        case '^':
        case '[':
        case ']':
            return true;
        default:
            break;
        }
        s++;
    }
    return false;
}

static bool son_parametros_validos(const OpenMeteoParams *params)
{
    if (params->latitud < -90.0 || params->latitud > 90.0) return false;
    if (params->longitud < -180.0 || params->longitud > 180.0) return false;
    if (params->anio < 1900 || params->anio > 2100) return false;
    if (params->mes < 1 || params->mes > 12) return false;
    if (params->dia < 1 || params->dia > 31) return false;
    return true;
}

static char *descargar_json_clima(const char *cmd)
{
    /* cmd is validated upstream: built from snprintf'd doubles/ints + compile-time constant */
    FILE *fp = popen(cmd, "r"); /* NOSONAR */
    if (!fp)
    {
        printf("Error: popen() fallo\n");
        return NULL;
    }

    size_t capacity = READ_BUF_SIZE;
    size_t total = 0;
    char *data = (char *)malloc(capacity);
    if (!data)
    {
        pclose(fp);
        return NULL;
    }

    size_t nread;
    while ((nread = fread(data + total, 1, capacity - total, fp)) > 0)
    {
        total += nread;
        if (capacity - total < 1024)
        {
            capacity *= 2;
            char *tmp = (char *)realloc(data, capacity);
            if (!tmp)
            {
                free(data);
                pclose(fp);
                return NULL;
            }
            data = tmp;
        }
    }

    int exit_code = pclose(fp);

    if (total == 0)
    {
        printf("Error: curl no devolvio datos (exit code %d)\n", exit_code);
        free(data);
        return NULL;
    }

    data[total] = '\0';

    /* Saltar BOM UTF-8 si existe */
    if ((unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
            (unsigned char)data[2] == 0xBF)
    {
        memmove(data, data + 3, total - 2);
    }
    return data;
}

static double acumular_array(cJSON const *arr, int i, int *count)
{
    if (!arr || !cJSON_IsArray(arr)) return 0.0;
    cJSON const *item = cJSON_GetArrayItem(arr, i);
    if (item && cJSON_IsNumber(item))
    {
        if (count) (*count)++;
        return item->valuedouble;
    }
    return 0.0;
}

static int obtener_codigo_weather(cJSON const *code_arr, int i)
{
    if (!code_arr || !cJSON_IsArray(code_arr)) return -1;
    cJSON const *c = cJSON_GetArrayItem(code_arr, i);
    return (c && cJSON_IsNumber(c)) ? (int)c->valuedouble : -1;
}

static int parsear_resultado_json(cJSON const *root, OpenMeteoResult *out_result)
{
    cJSON const *hourly = cJSON_GetObjectItem(root, "hourly");
    if (!hourly)
    {
        printf("Error: respuesta no contiene 'hourly'\n");
        return 0;
    }

    cJSON const *temp_arr = cJSON_GetObjectItem(hourly, "temperature_2m");
    if (!temp_arr || !cJSON_IsArray(temp_arr))
    {
        printf("Error: 'temperature_2m' no encontrado o no es array\n");
        return 0;
    }

    int count = cJSON_GetArraySize(temp_arr);
    if (count == 0)
    {
        printf("Error: array temperature_2m vacio\n");
        return 0;
    }

    cJSON const *apparent_arr = cJSON_GetObjectItem(hourly, "apparent_temperature");
    cJSON const *precip_arr = cJSON_GetObjectItem(hourly, "precipitation");
    cJSON const *wind_arr = cJSON_GetObjectItem(hourly, "wind_speed_10m");
    cJSON const *code_arr = cJSON_GetObjectItem(hourly, "weather_code");

    double temp_sum = 0.0;
    double apparent_sum = 0.0;
    double precip_sum = 0.0;
    double wind_sum = 0.0;
    int valid_count = 0;
    int apparent_count = 0;
    int *wcodes = (int *)malloc((size_t)count * sizeof(int));
    if (!wcodes) return 0;

    for (int i = 0; i < count; i++)
    {
        temp_sum += acumular_array(temp_arr, i, &valid_count);
        apparent_sum += acumular_array(apparent_arr, i, &apparent_count);
        precip_sum += acumular_array(precip_arr, i, NULL);
        wind_sum += acumular_array(wind_arr, i, NULL);
        wcodes[i] = obtener_codigo_weather(code_arr, i);
    }

    out_result->temp_c = (valid_count > 0) ? (temp_sum / valid_count) : 0.0;
    out_result->apparent_temp_c = (apparent_count > 0) ? (apparent_sum / apparent_count) : 0.0;
    out_result->precip_mm = precip_sum;
    out_result->wind_kmh = (valid_count > 0) ? (wind_sum / valid_count) : 0.0;
    out_result->weather_code = weather_code_mode(wcodes, count);
    free(wcodes);
    return 1;
}

int openmeteo_fetch(const OpenMeteoParams *params, OpenMeteoResult *out_result)
{
    if (!params || !out_result) return 0;

    out_result->temp_c = 0.0;
    out_result->apparent_temp_c = 0.0;
    out_result->precip_mm = 0.0;
    out_result->wind_kmh = 0.0;
    out_result->weather_code = 0;
    out_result->clima_json = NULL;

    if (!son_parametros_validos(params)) return 0;
    if (es_fecha_futura(params)) return 0;

    char date_str[11];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", params->anio, params->mes, params->dia);

    char lat_str[32];
    char lon_str[32];
    snprintf(lat_str, sizeof(lat_str), "%.6f", params->latitud);
    snprintf(lon_str, sizeof(lon_str), "%.6f", params->longitud);

    if (contiene_shell_metachar(lat_str) || contiene_shell_metachar(lon_str) ||
            contiene_shell_metachar(date_str))
    {
        printf("Error: parametros invalidos detectados\n");
        return 0;
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "curl.exe -s --compressed --max-time 15 \"%s?latitude=%s&longitude=%s"
             "&start_date=%s&end_date=%s"
             "&hourly=temperature_2m,apparent_temperature,"
             "precipitation,weather_code,wind_speed_10m"
             "&timezone=auto\" 2>&1",
             OWM_ARCHIVE_URL_BASE, lat_str, lon_str, date_str, date_str);

    char *json_data = descargar_json_clima(cmd);
    if (!json_data) return 0;

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);

    if (!root)
    {
        const char *err = cJSON_GetErrorPtr();
        if (err) printf("Error parseando JSON cerca de:\n%s\n", err);
        else printf("Error: JSON invalido (razon desconocida)\n");
        return 0;
    }

    int ok = parsear_resultado_json(root, out_result);

    if (ok)
    {
        char *clima_str = cJSON_PrintUnformatted(root);
        if (clima_str) out_result->clima_json = clima_str;
    }

    cJSON_Delete(root);
    return ok;
}

void openmeteo_result_free(OpenMeteoResult *result)
{
    if (result)
    {
        free(result->clima_json);
        result->clima_json = NULL;
    }
}
