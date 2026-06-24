#include "cJSON.h"
#include "db.h"
#include "export.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORDATORIOS_PATH "Importaciones/recordatorios.json"

typedef struct
{
    long long id;
    char fecha[64];
    char nota[512];
    char tematica[64];
} Reminder;

static cJSON *leer_json_recordatorios(void)
{
    long len = 0;
    char *buf = utils_file_read_to_buffer(RECORDATORIOS_PATH, &len);
    if (!buf)
    {
        return NULL;
    }
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root))
    {
        if (root)
        {
            cJSON_Delete(root);
        }
        return NULL;
    }
    return root;
}

static void extraer_datos_reminder(const cJSON *it, Reminder *r, int idx)
{
    cJSON const *jid = cJSON_GetObjectItemCaseSensitive(it, "id");
    cJSON const *jfecha = cJSON_GetObjectItemCaseSensitive(it, "fecha");
    cJSON const *jnota = cJSON_GetObjectItemCaseSensitive(it, "nota");
    cJSON const *jtema = cJSON_GetObjectItemCaseSensitive(it, "tematica");

    if (jid && cJSON_IsNumber(jid))
    {
        r->id = (long long)jid->valuedouble;
    }
    else
    {
        r->id = (long long)idx + 1;
    }

    if (jfecha && cJSON_IsString(jfecha))
    {
        strncpy_s(r->fecha, sizeof(r->fecha), jfecha->valuestring, sizeof(r->fecha) - 1);
    }
    else
    {
        r->fecha[0] = '\0';

        if (jnota && cJSON_IsString(jnota))
        {
            strncpy_s(r->nota, sizeof(r->nota), jnota->valuestring, sizeof(r->nota) - 1);
        }
        else
        {
            r->nota[0] = '\0';
        }

        if (jtema && cJSON_IsString(jtema))
        {
            strncpy_s(r->tematica, sizeof(r->tematica), jtema->valuestring,
                      sizeof(r->tematica) - 1);
        }
        else
        {
            r->tematica[0] = '\0';
        }
    }
}
static int cargar_recordatorios(Reminder **out_arr, int *out_count)
{
    *out_arr = NULL;
    *out_count = 0;

    cJSON *root = leer_json_recordatorios();
    if (!root)
    {
        return 0;
    }

    int count = cJSON_GetArraySize(root);
    Reminder *arr = (Reminder *)calloc((size_t)count, sizeof(Reminder));
    if (!arr)
    {
        cJSON_Delete(root);
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        cJSON const *it = cJSON_GetArrayItem(root, i);
        if (it && cJSON_IsObject(it))
        {
            extraer_datos_reminder(it, &arr[i], i);
        }
        else
        {
            arr[i].id = (long long)i + 1;
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

void exportar_recordatorios_csv(void)
{
    Reminder *arr = NULL;
    int count = 0;
    if (!cargar_recordatorios(&arr, &count))
    {
        printf("No hay recordatorios para exportar.\n");
        return;
    }

    FILE *file;
    errno_t err = fopen_s(&file, get_export_path("recordatorios.csv"), "w");
    if (err != 0 || file == NULL)
    {
        printf("Error al crear el archivo CSV.\n");
        free(arr);
        return;
    }

    fprintf(file, "id,fecha,tematica,nota\n");
    for (int i = 0; i < count; i++)
    {
        char fecha_limpio[64];
        char tema_limpio[64];
        char nota_limpio[512];
        sanitizar_ascii_basico(arr[i].fecha, fecha_limpio, sizeof(fecha_limpio));
        sanitizar_ascii_basico(arr[i].tematica, tema_limpio, sizeof(tema_limpio));
        sanitizar_ascii_basico(arr[i].nota, nota_limpio, sizeof(nota_limpio));
        fprintf(file, "%lld,%s,%s,%s\n", arr[i].id, fecha_limpio, tema_limpio, nota_limpio);
    }

    fclose(file);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.csv"));
}

void exportar_recordatorios_txt(void)
{
    Reminder *arr = NULL;
    int count = 0;
    if (!cargar_recordatorios(&arr, &count))
    {
        printf("No hay recordatorios para exportar.\n");
        return;
    }

    FILE *file;
    errno_t err = fopen_s(&file, get_export_path("recordatorios.txt"), "w");
    if (err != 0 || file == NULL)
    {
        printf("Error al crear el archivo TXT.\n");
        free(arr);
        return;
    }

    fprintf(file, "RECORDATORIOS\n\n");
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "ID: %lld\n  Fecha: %s\n  Tematica: %s\n  Nota: %s\n\n", arr[i].id,
                arr[i].fecha, arr[i].tematica, arr[i].nota);
    }

    fclose(file);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.txt"));
}

void exportar_recordatorios_json(void)
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

    FILE *file;
    errno_t err = fopen_s(&file, get_export_path("recordatorios.json"), "w");
    if (err != 0 || file == NULL)
    {
        printf("Error al crear el archivo JSON.\n");
        cJSON_Delete(root);
        free(arr);
        return;
    }

    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(file, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
    fclose(file);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.json"));
}

void exportar_recordatorios_html(void)
{
    Reminder *arr = NULL;
    int count = 0;
    if (!cargar_recordatorios(&arr, &count))
    {
        printf("No hay recordatorios para exportar.\n");
        return;
    }

    FILE *file;
    errno_t err = fopen_s(&file, get_export_path("recordatorios.html"), "w");
    if (err != 0 || file == NULL)
    {
        printf("Error al crear el archivo HTML.\n");
        free(arr);
        return;
    }

    fprintf(file, "<html><body><h1>Recordatorios</h1><table border='1'>"
            "<tr><th>ID</th><th>Fecha</th><th>Tematica</th><th>Nota</th></tr>");

    for (int i = 0; i < count; i++)
    {
        fprintf(file, "<tr><td>%lld</td><td>%s</td><td>%s</td><td>%s</td></tr>", arr[i].id,
                arr[i].fecha, arr[i].tematica, arr[i].nota);
    }

    fprintf(file, "</table></body></html>");
    fclose(file);
    free(arr);
    printf("Archivo exportado a: %s\n", get_export_path("recordatorios.html"));
}
