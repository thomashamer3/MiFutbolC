#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include "direct.h"
#endif
#include <string.h>

#define RECORDATORIOS_PATH "Importaciones/recordatorios.json"

typedef struct
{
    long long id;
    char fecha[64];
    char nota[512];
    char tematica[64];
} Reminder;

static int cargar_recordatorios(Reminder **out_arr, int *out_count)
{
    *out_arr = NULL;
    *out_count = 0;

    FILE *f;
    errno_t err = fopen_s(&f, RECORDATORIOS_PATH, "rb");
    if (err != 0 || f == NULL)
    {
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0)
    {
        fclose(f);
        return 0;
    }

    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }

    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (read > 0 && read <= (size_t)len)
        buf[read] = '\0';
    else
        buf[0] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root))
    {
        if (root) cJSON_Delete(root);
        return 0;
    }

    int count = cJSON_GetArraySize(root);
    Reminder *arr = (Reminder*)calloc((size_t)count, sizeof(Reminder));
    if (!arr)
    {
        cJSON_Delete(root);
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        cJSON *it = cJSON_GetArrayItem(root, i);
        if (it && cJSON_IsObject(it))
        {
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(it, "id");
            cJSON *jfecha = cJSON_GetObjectItemCaseSensitive(it, "fecha");
            cJSON *jnota = cJSON_GetObjectItemCaseSensitive(it, "nota");
            cJSON *jtema = cJSON_GetObjectItemCaseSensitive(it, "tematica");

            arr[i].id = jid && cJSON_IsNumber(jid) ? (long long)jid->valuedouble : (long long)(i + 1);
            strncpy_s(arr[i].fecha, sizeof(arr[i].fecha),
                      jfecha && cJSON_IsString(jfecha) ? jfecha->valuestring : "", sizeof(arr[i].fecha) - 1);
            strncpy_s(arr[i].nota, sizeof(arr[i].nota),
                      jnota && cJSON_IsString(jnota) ? jnota->valuestring : "", sizeof(arr[i].nota) - 1);
            strncpy_s(arr[i].tematica, sizeof(arr[i].tematica),
                      jtema && cJSON_IsString(jtema) ? jtema->valuestring : "", sizeof(arr[i].tematica) - 1);
        }
        else
        {
            arr[i].id = i + 1;
            arr[i].fecha[0] = '\0';
            arr[i].nota[0] = '\0';
            arr[i].tematica[0] = '\0';
        }
    }

    cJSON_Delete(root);
    *out_arr = arr;
    *out_count = count;
    return 1;
}

void exportar_recordatorios_csv()
{
    Reminder *arr = NULL;
    int count = 0;
    if (!cargar_recordatorios(&arr, &count))
    {
        printf("No hay recordatorios para exportar.\n");
        return;
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("recordatorios.csv"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo CSV.\n");
        free(arr);
        return;
    }

    fprintf(f, "id,fecha,tematica,nota\n");
    for (int i = 0; i < count; i++)
    {
        char fecha_limpio[64], tema_limpio[64], nota_limpio[512];
        sanitizar_ascii_basico(arr[i].fecha, fecha_limpio, sizeof(fecha_limpio));
        sanitizar_ascii_basico(arr[i].tematica, tema_limpio, sizeof(tema_limpio));
        sanitizar_ascii_basico(arr[i].nota, nota_limpio, sizeof(nota_limpio));
        fprintf(f, "%lld,%s,%s,%s\n", arr[i].id, fecha_limpio, tema_limpio, nota_limpio);
    }

    fclose(f);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.csv"));
}

void exportar_recordatorios_txt()
{
    Reminder *arr = NULL;
    int count = 0;
    if (!cargar_recordatorios(&arr, &count))
    {
        printf("No hay recordatorios para exportar.\n");
        return;
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("recordatorios.txt"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo TXT.\n");
        free(arr);
        return;
    }

    fprintf(f, "RECORDATORIOS\n\n");
    for (int i = 0; i < count; i++)
    {
        fprintf(f, "ID: %lld\n  Fecha: %s\n  Tematica: %s\n  Nota: %s\n\n",
                arr[i].id, arr[i].fecha, arr[i].tematica, arr[i].nota);
    }

    fclose(f);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.txt"));
}

void exportar_recordatorios_json()
{
    Reminder *arr = NULL;
    int count = 0;
    if (!cargar_recordatorios(&arr, &count))
    {
        printf("No hay recordatorios para exportar.\n");
        return;
    }

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", (double)arr[i].id);
        cJSON_AddStringToObject(item, "fecha", arr[i].fecha);
        cJSON_AddStringToObject(item, "tematica", arr[i].tematica);
        cJSON_AddStringToObject(item, "nota", arr[i].nota);
        cJSON_AddItemToArray(root, item);
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("recordatorios.json"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo JSON.\n");
        cJSON_Delete(root);
        free(arr);
        return;
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
    fclose(f);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.json"));
}

void exportar_recordatorios_html()
{
    Reminder *arr = NULL;
    int count = 0;
    if (!cargar_recordatorios(&arr, &count))
    {
        printf("No hay recordatorios para exportar.\n");
        return;
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("recordatorios.html"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo HTML.\n");
        free(arr);
        return;
    }

    fprintf(f, "<html><body><h1>Recordatorios</h1><table border='1'>"
            "<tr><th>ID</th><th>Fecha</th><th>Tematica</th><th>Nota</th></tr>");

    for (int i = 0; i < count; i++)
    {
        fprintf(f, "<tr><td>%lld</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                arr[i].id, arr[i].fecha, arr[i].tematica, arr[i].nota);
    }

    fprintf(f, "</table></body></html>");
    fclose(f);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.html"));
}
