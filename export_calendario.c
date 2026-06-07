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
#define MAX_EVENTOS 2000

typedef struct
{
    char fecha[64];
    char tipo[32];
    char titulo[512];
    char detalle[512];
    long long id_origen;
} EventoCalendario;

static int comparar_eventos(const void *a, const void *b)
{
    const EventoCalendario *ea = (const EventoCalendario *)a;
    const EventoCalendario *eb = (const EventoCalendario *)b;
    return strcmp(ea->fecha, eb->fecha);
}

static void extraer_campo_calendario(char *dst, size_t dst_size, const cJSON *obj, const char *campo)
{
    cJSON const *jval = cJSON_GetObjectItemCaseSensitive(obj, campo);
    if (jval && cJSON_IsString(jval))
        strncpy_s(dst, dst_size, jval->valuestring, dst_size - 1);
    else
        dst[0] = '\0';
}

static int procesar_item_recordatorio(const cJSON *it, EventoCalendario *evento, long long id_fallback)
{
    if (!it || !cJSON_IsObject(it))
        return 0;

    extraer_campo_calendario(evento->fecha, sizeof(evento->fecha), it, "fecha");
    strncpy_s(evento->tipo, sizeof(evento->tipo), "recordatorio", sizeof(evento->tipo) - 1);
    extraer_campo_calendario(evento->titulo, sizeof(evento->titulo), it, "tematica");
    extraer_campo_calendario(evento->detalle, sizeof(evento->detalle), it, "nota");

    cJSON const *jid = cJSON_GetObjectItemCaseSensitive(it, "id");
    if (jid && cJSON_IsNumber(jid))
        evento->id_origen = (long long)jid->valuedouble;
    else
        evento->id_origen = id_fallback;
    return 1;
}

static cJSON* cargar_json_array_calendario(const char *path)
{
    FILE *f;
    errno_t err = fopen_s(&f, path, "rb");
    if (err != 0 || f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0)
    {
        fclose(f);
        return NULL;
    }
    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
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
        return NULL;
    }
    return root;
}

static int cargar_recordatorios(EventoCalendario *eventos, int offset, int max)
{
    cJSON *root = cargar_json_array_calendario(RECORDATORIOS_PATH);
    if (!root) return 0;

    int count = cJSON_GetArraySize(root);
    int total = 0;
    int max_alcanzado = 0;
    for (int i = 0; i < count && !max_alcanzado; i++)
    {
        if (procesar_item_recordatorio(cJSON_GetArrayItem(root, i), &eventos[offset + total], (long long)(i + 1)))
        {
            total++;
            if ((offset + total) >= max)
                max_alcanzado = 1;
        }
    }

    cJSON_Delete(root);
    return total;
}

static int cargar_partidos(EventoCalendario *eventos, int offset, int max)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha_hora, goles, asistencias, "
                      "COALESCE(rendimiento_general, 0) "
                      "FROM partido ORDER BY fecha_hora";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && (offset + total) < max)
    {
        int idx = offset + total;
        const char *fecha = (const char*)sqlite3_column_text(stmt, 1);
        strncpy_s(eventos[idx].fecha, sizeof(eventos[idx].fecha),
                  fecha ? fecha : "", sizeof(eventos[idx].fecha) - 1);
        strncpy_s(eventos[idx].tipo, sizeof(eventos[idx].tipo), "partido", sizeof(eventos[idx].tipo) - 1);

        char titulo[128];
        snprintf(titulo, sizeof(titulo), "Partido #%d (%d goles, %d asistencias)",
                 sqlite3_column_int(stmt, 0),
                 sqlite3_column_int(stmt, 2),
                 sqlite3_column_int(stmt, 3));
        strncpy_s(eventos[idx].titulo, sizeof(eventos[idx].titulo),
                  titulo, sizeof(eventos[idx].titulo) - 1);

        char detalle[128];
        snprintf(detalle, sizeof(detalle), "Rendimiento: %d/10",
                 sqlite3_column_int(stmt, 4));
        strncpy_s(eventos[idx].detalle, sizeof(eventos[idx].detalle),
                  detalle, sizeof(eventos[idx].detalle) - 1);

        eventos[idx].id_origen = sqlite3_column_int(stmt, 0);
        total++;
    }

    sqlite3_finalize(stmt);
    return total;
}

static int cargar_eventos(EventoCalendario *eventos, int max)
{
    int total = cargar_partidos(eventos, 0, max / 2);
    total += cargar_recordatorios(eventos, total, max);
    qsort(eventos, (size_t)total, sizeof(EventoCalendario), comparar_eventos);
    return total;
}

void exportar_calendario_csv()
{
    EventoCalendario *eventos = malloc(sizeof(EventoCalendario) * MAX_EVENTOS);
    if (!eventos)
    {
        printf("Error de memoria.\n");
        return;
    }
    int total = cargar_eventos(eventos, MAX_EVENTOS);
    if (total == 0)
    {
        printf("No hay eventos para exportar.\n");
        free(eventos);
        return;
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("calendario.csv"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo CSV.\n");
        free(eventos);
        return;
    }

    fprintf(f, "fecha,tipo,titulo,detalle\n");
    for (int i = 0; i < total; i++)
    {
        char fecha_limpio[64];
        char tipo_limpio[32];
        char titulo_limpio[512];
        char detalle_limpio[512];
        sanitizar_ascii_basico(eventos[i].fecha, fecha_limpio, sizeof(fecha_limpio));
        sanitizar_ascii_basico(eventos[i].tipo, tipo_limpio, sizeof(tipo_limpio));
        sanitizar_ascii_basico(eventos[i].titulo, titulo_limpio, sizeof(titulo_limpio));
        sanitizar_ascii_basico(eventos[i].detalle, detalle_limpio, sizeof(detalle_limpio));
        fprintf(f, "%s,%s,%s,%s\n", fecha_limpio, tipo_limpio, titulo_limpio, detalle_limpio);
    }

    fclose(f);
    free(eventos);
    printf("Archivo exportado a: %s\n", get_export_path("calendario.csv"));
}

void exportar_calendario_txt()
{
    EventoCalendario *eventos = malloc(sizeof(EventoCalendario) * MAX_EVENTOS);
    if (!eventos)
    {
        printf("Error de memoria.\n");
        return;
    }
    int total = cargar_eventos(eventos, MAX_EVENTOS);
    if (total == 0)
    {
        printf("No hay eventos para exportar.\n");
        free(eventos);
        return;
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("calendario.txt"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo TXT.\n");
        free(eventos);
        return;
    }

    fprintf(f, "CALENDARIO DE EVENTOS\n\n");
    for (int i = 0; i < total; i++)
    {
        fprintf(f, "[%s] %s\n  %s\n  %s\n\n",
                eventos[i].fecha, eventos[i].tipo,
                eventos[i].titulo, eventos[i].detalle);
    }

    fclose(f);
    free(eventos);
    printf("Archivo exportado a: %s\n", get_export_path("calendario.txt"));
}

void exportar_calendario_json()
{
    EventoCalendario *eventos = malloc(sizeof(EventoCalendario) * MAX_EVENTOS);
    if (!eventos)
    {
        printf("Error de memoria.\n");
        return;
    }
    int total = cargar_eventos(eventos, MAX_EVENTOS);
    if (total == 0)
    {
        printf("No hay eventos para exportar.\n");
        free(eventos);
        return;
    }

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < total; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "fecha", eventos[i].fecha);
        cJSON_AddStringToObject(item, "tipo", eventos[i].tipo);
        cJSON_AddStringToObject(item, "titulo", eventos[i].titulo);
        cJSON_AddStringToObject(item, "detalle", eventos[i].detalle);
        cJSON_AddNumberToObject(item, "id", (double)eventos[i].id_origen);
        cJSON_AddItemToArray(root, item);
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("calendario.json"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo JSON.\n");
        cJSON_Delete(root);
        free(eventos);
        return;
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
    fclose(f);
    free(eventos);
    printf("Archivo exportado a: %s\n", get_export_path("calendario.json"));
}

void exportar_calendario_html()
{
    EventoCalendario *eventos = malloc(sizeof(EventoCalendario) * MAX_EVENTOS);
    if (!eventos)
    {
        printf("Error de memoria.\n");
        return;
    }
    int total = cargar_eventos(eventos, MAX_EVENTOS);
    if (total == 0)
    {
        printf("No hay eventos para exportar.\n");
        free(eventos);
        return;
    }

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("calendario.html"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo HTML.\n");
        free(eventos);
        return;
    }

    fprintf(f, "<html><body><h1>Calendario de Eventos</h1><table border='1'>"
            "<tr><th>Fecha</th><th>Tipo</th><th>Titulo</th><th>Detalle</th></tr>");

    for (int i = 0; i < total; i++)
    {
        fprintf(f, "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                eventos[i].fecha, eventos[i].tipo,
                eventos[i].titulo, eventos[i].detalle);
    }

    fprintf(f, "</table></body></html>");
    fclose(f);
    free(eventos);
    printf("Archivo exportado a: %s\n", get_export_path("calendario.html"));
}
