#include "recordatorios.h"
#include "utils.h"
#include "cJSON.h"
#include "export_partidos_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STORAGE_PATH "Importaciones/recordatorios.json"
#define MAX_NOTA 512
#define MAX_TEMATICA 64
#define MAX_FECHA 64

typedef struct {
    long long id;
    char fecha[MAX_FECHA];
    char nota[MAX_NOTA];
    char tematica[MAX_TEMATICA];
} Reminder;

static size_t safe_strlen_s(const char *s, size_t max_len)
{
    size_t i = 0;
    if (!s) return 0;
    while (i < max_len && s[i]) ++i;
    return i;
}

static Reminder *load_reminders(int *out_count)
{
    *out_count = 0;
    FILE *f = fopen(STORAGE_PATH, "rb");
    if (!f)
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
    /* Asegurar terminador usando la longitud real leída */
    if (read > 0 && read <= (size_t)len)
        buf[read] = '\0';
    else
        buf[0] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root))
    {
        if (root) cJSON_Delete(root);
        return NULL;
    }

    int count = cJSON_GetArraySize(root);
    Reminder *arr = (Reminder*)malloc(sizeof(Reminder) * (size_t)count);
    if (!arr)
    {
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < count; i++)
    {
        cJSON const *it = cJSON_GetArrayItem(root, i);
        cJSON const *jid = cJSON_GetObjectItemCaseSensitive(it, "id");
        cJSON const *jfecha = cJSON_GetObjectItemCaseSensitive(it, "fecha");
        cJSON const *jnota = cJSON_GetObjectItemCaseSensitive(it, "nota");
        cJSON const *jtema = cJSON_GetObjectItemCaseSensitive(it, "tematica");

        arr[i].id = jid && cJSON_IsNumber(jid) ? (long long)jid->valuedouble : (i + 1);
        strncpy_s(arr[i].fecha, MAX_FECHA, jfecha && cJSON_IsString(jfecha) ? jfecha->valuestring : "", MAX_FECHA - 1);
        arr[i].fecha[MAX_FECHA - 1] = '\0';
        strncpy_s(arr[i].nota, MAX_NOTA, jnota && cJSON_IsString(jnota) ? jnota->valuestring : "", MAX_NOTA - 1);
        arr[i].nota[MAX_NOTA - 1] = '\0';
        strncpy_s(arr[i].tematica, MAX_TEMATICA, jtema && cJSON_IsString(jtema) ? jtema->valuestring : "", MAX_TEMATICA - 1);
        arr[i].tematica[MAX_TEMATICA - 1] = '\0';
    }

    cJSON_Delete(root);
    *out_count = count;
    return arr;
}

static int write_reminders_to_file(const Reminder *arr, int count, const char *path)
{
    if (count > 0 && !arr) return 0;
    cJSON *root = cJSON_CreateArray();
    if (!root) return 0;

    for (int i = 0; i < count; i++)
    {
        cJSON *obj = cJSON_CreateObject();
        if (!obj) continue;
        cJSON_AddNumberToObject(obj, "id", (double)arr[i].id);
        cJSON_AddStringToObject(obj, "fecha", arr[i].fecha);
        cJSON_AddStringToObject(obj, "nota", arr[i].nota);
        cJSON_AddStringToObject(obj, "tematica", arr[i].tematica);
        cJSON_AddItemToArray(root, obj);
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return 0;

    FILE *f = fopen(path, "wb");
    if (!f)
    {
        free(out);
        return 0;
    }
    /* Use bounded length to avoid unbounded reads (c:S3519). Cap at 10 MiB. */
    size_t out_len = safe_strlen_s(out, 10 * 1024 * 1024);
    fwrite(out, 1, out_len, f);
    fclose(f);
    free(out);
    return 1;
}

static int save_reminders(const Reminder *arr, int count)
{
    return write_reminders_to_file(arr, count, STORAGE_PATH);
}

static Reminder *load_reminders_from_file(const char *path, int *out_count)
{
    *out_count = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
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
    if (read > 0 && read <= (size_t)len)
        buf[read] = '\0';
    else
        buf[0] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root))
    {
        if (root) cJSON_Delete(root);
        return NULL;
    }

    int count = cJSON_GetArraySize(root);
    Reminder *arr = (Reminder*)malloc(sizeof(Reminder) * (size_t)count);
    if (!arr)
    {
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < count; i++)
    {
        cJSON const *it = cJSON_GetArrayItem(root, i);
        cJSON const *jid = cJSON_GetObjectItemCaseSensitive(it, "id");
        cJSON const *jfecha = cJSON_GetObjectItemCaseSensitive(it, "fecha");
        cJSON const *jnota = cJSON_GetObjectItemCaseSensitive(it, "nota");
        cJSON const *jtema = cJSON_GetObjectItemCaseSensitive(it, "tematica");

        arr[i].id = jid && cJSON_IsNumber(jid) ? (long long)jid->valuedouble : (i + 1);
        strncpy_s(arr[i].fecha, MAX_FECHA, jfecha && cJSON_IsString(jfecha) ? jfecha->valuestring : "", MAX_FECHA - 1);
        arr[i].fecha[MAX_FECHA - 1] = '\0';
        strncpy_s(arr[i].nota, MAX_NOTA, jnota && cJSON_IsString(jnota) ? jnota->valuestring : "", MAX_NOTA - 1);
        arr[i].nota[MAX_NOTA - 1] = '\0';
        strncpy_s(arr[i].tematica, MAX_TEMATICA, jtema && cJSON_IsString(jtema) ? jtema->valuestring : "", MAX_TEMATICA - 1);
        arr[i].tematica[MAX_TEMATICA - 1] = '\0';
    }

    cJSON_Delete(root);
    *out_count = count;
    return arr;
}

static long long obtener_siguiente_id_local(const Reminder *arr, int count)
{
    long long max = 0;
    for (int i = 0; i < count; i++)
        if (arr[i].id > max) max = arr[i].id;
    return max + 1;
}

static void listar_recordatorios(const Reminder *arr, int count)
{
    if (count <= 0)
    {
        mostrar_no_hay_registros("recordatorios");
        return;
    }

    for (int i = 0; i < count; i++)
    {
        printf("[%lld] %s - %s\n", arr[i].id, arr[i].tematica, arr[i].fecha);
        printf("    %s\n", arr[i].nota);
        printf("----------------------------------------\n");
    }
}

static void elegir_tematica(char *out, int size)
{
    const char *opciones[] = {"Partidos", "Canchas", "Equipos", "Financiamiento", "Torneos", "Bienestar", "Otra"};
    int n = (int)(sizeof(opciones) / sizeof(opciones[0]));
    printf("Elija temática:\n");
    for (int i = 0; i < n; i++)
    {
        printf("%d. %s\n", i + 1, opciones[i]);
    }
    int sel = input_int_rango(">", 1, n);
    if (sel >= 1 && sel <= n - 1)
    {
        strncpy_s(out, size, opciones[sel - 1], size - 1);
        out[size - 1] = '\0';
    }
    else if (sel == n)
    {
        input_string("Ingrese temática personalizada:", out, size);
    }
    else
    {
        strncpy_s(out, size, "General", size - 1);
        out[size - 1] = '\0';
    }
}

static void agregar_recordatorio()
{
    int count = 0;
    Reminder *arr = load_reminders(&count);

    if (count < 0) count = 0;

    Reminder nuevo;
    memset(&nuevo, 0, sizeof(nuevo));
    nuevo.id = obtener_siguiente_id_local(arr, count);
    input_date("Fecha (dd/mm/yyyy hh:mm):", nuevo.fecha, MAX_FECHA);
    input_string_extended("Nota:", nuevo.nota, MAX_NOTA);
    elegir_tematica(nuevo.tematica, MAX_TEMATICA);

    size_t new_count = (size_t)count + 1;
    Reminder *newarr = (Reminder*)realloc(arr, sizeof(Reminder) * new_count);
    if (!newarr)
    {
        mostrar_error_operacion("recordatorio", "agregar");
        free(arr);
        return;
    }
    newarr[(size_t)count] = nuevo;
    if (!save_reminders(newarr, (int)new_count))
    {
        mostrar_error_operacion("recordatorio", "guardar");
    }
    else
    {
        ui_puts("Recordatorio agregado.");
    }
    free(newarr);
}

static int find_index_by_id(const Reminder *arr, int count, long long id)
{
    for (int i = 0; i < count; i++)
        if (arr[i].id == id) return i;
    return -1;
}

static void editar_recordatorio()
{
    int count = 0;
    Reminder *arr = load_reminders(&count);
    if (!arr || count == 0)
    {
        mostrar_no_hay_registros("recordatorios");
        free(arr);
        return;
    }

    listar_recordatorios(arr, count);
    long long id = input_int("Ingrese ID a editar:");
    int idx = find_index_by_id(arr, count, id);
    if (idx < 0)
    {
        mostrar_no_existe("recordatorio");
        free(arr);
        return;
    }

    char buf[MAX_NOTA];
    printf("Valor actual fecha: %s\n", arr[idx].fecha);
    input_date("Nueva fecha (dejar vacio para mantener):", buf, MAX_FECHA);
    if (buf[0] != '\0') { strncpy_s(arr[idx].fecha, MAX_FECHA, buf, MAX_FECHA - 1); arr[idx].fecha[MAX_FECHA - 1] = '\0'; }

    printf("Valor actual nota:\n%s\n", arr[idx].nota);
    input_string_extended("Nueva nota (dejar vacio para mantener):", buf, MAX_NOTA);
    if (buf[0] != '\0') { strncpy_s(arr[idx].nota, MAX_NOTA, buf, MAX_NOTA - 1); arr[idx].nota[MAX_NOTA - 1] = '\0'; }

    printf("Valor actual temática: %s\n", arr[idx].tematica);
    char tema[MAX_TEMATICA];
    elegir_tematica(tema, MAX_TEMATICA);
    if (tema[0] != '\0') { strncpy_s(arr[idx].tematica, MAX_TEMATICA, tema, MAX_TEMATICA - 1); arr[idx].tematica[MAX_TEMATICA - 1] = '\0'; }

    if (!save_reminders(arr, count))
    {
        mostrar_error_operacion("recordatorio", "actualizar");
    }
    else
    {
        ui_puts("Recordatorio actualizado.");
    }
    free(arr);
}

static void eliminar_recordatorio()
{
    int count = 0;
    Reminder *arr = load_reminders(&count);
    if (!arr || count == 0)
    {
        mostrar_no_hay_registros("recordatorios");
        free(arr);
        return;
    }

    listar_recordatorios(arr, count);
    long long id = input_int("Ingrese ID a eliminar:");
    int idx = find_index_by_id(arr, count, id);
    if (idx < 0)
    {
        mostrar_no_existe("recordatorio");
        free(arr);
        return;
    }

    if (!confirmar("Confirma eliminar este recordatorio?"))
    {
        free(arr);
        return;
    }

    for (int i = idx; i < count - 1; i++) arr[i] = arr[i + 1];
    if (!save_reminders(arr, count - 1))
    {
        mostrar_error_operacion("recordatorio", "eliminar");
    }
    else
    {
        ui_puts("Recordatorio eliminado.");
    }
    free(arr);
}

static void filtrar_por_tematica()
{
    int count = 0;
    Reminder *arr = load_reminders(&count);
    if (!arr || count == 0)
    {
        mostrar_no_hay_registros("recordatorios");
        free(arr);
        return;
    }

    char tema[MAX_TEMATICA];
    input_string("Ingrese temática a filtrar:", tema, MAX_TEMATICA);
    int found = 0;
    for (int i = 0; i < count; i++)
    {
        if (strcasecmp(arr[i].tematica, tema) == 0)
        {
            if (!found) ui_printf("Recordatorios con temática: %s\n", tema);
            ui_printf("[%lld] %s - %s\n    %s\n", arr[i].id, arr[i].tematica, arr[i].fecha, arr[i].nota);
            found = 1;
        }
    }

    if (!found) ui_puts("No se encontraron recordatorios con esa temática.");
    free(arr);
}

static void export_recordatorios()
{
    int count = 0;
    Reminder *arr = load_reminders(&count);
    if (!arr || count == 0)
    {
        mostrar_no_hay_registros("recordatorios");
        free(arr);
        return;
    }

    char path[260];
    strncpy_s(path, sizeof(path), "Exportaciones/recordatorios.json", sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    input_string("Ruta de exportación (enter para usar el valor por defecto):", path, sizeof(path));

    if (write_reminders_to_file(arr, count, path))
    {
        ui_printf("Recordatorios exportados a %s\n", path);
    }
    else
    {
        mostrar_error_operacion("recordatorios", "exportar");
    }
    free(arr);
}

static int merge_and_save_reminders(Reminder *newarr, int new_count)
{
    int exist_count = 0;
    Reminder *exist = load_reminders(&exist_count);
    if (!exist) exist_count = 0;

    size_t merged_count = (size_t)exist_count + (size_t)new_count;

    /* Comprobar overflow en la multiplicación */
    if (merged_count > SIZE_MAX / sizeof(Reminder))
    {
        free(exist);
        return 0;
    }

    Reminder *merged = (Reminder*)malloc(sizeof(Reminder) * merged_count);
    if (!merged)
    {
        free(exist);
        return 0;
    }

    /* Copiar existentes (si los hay) */
    if (exist_count > 0)
        memcpy(merged, exist, sizeof(Reminder) * (size_t)exist_count);

    long long maxid = 0;
    for (int i = 0; i < exist_count; i++) if (merged[i].id > maxid) maxid = merged[i].id;

    for (int i = 0; i < new_count; i++)
    {
        size_t idx = (size_t)exist_count + (size_t)i;
        newarr[i].id = ++maxid;
        merged[idx] = newarr[i];
    }

    int saved = save_reminders(merged, (int)merged_count);

    free(merged);
    free(exist);
    return saved;
}

static void import_recordatorios()
{
    char path[260];
    strncpy_s(path, sizeof(path), "Importaciones/recordatorios.json", sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    input_string("Ruta de importación (enter para usar Importaciones/recordatorios.json):", path, sizeof(path));

    int new_count = 0;
    Reminder *newarr = load_reminders_from_file(path, &new_count);
    if (!newarr || new_count == 0)
    {
        mostrar_error_operacion("recordatorios", "importar");
        free(newarr);
        return;
    }

    printf("1) Reemplazar todos los recordatorios\n2) Unir (fusionar) con los existentes\n");
    int op = input_int_rango(">", 1, 2);
    if (op == 1)
    {
        if (!save_reminders(newarr, new_count))
            mostrar_error_operacion("recordatorios", "guardar");
        else
            ui_puts("Importación completada (reemplazado).");
    }

    if (!merge_and_save_reminders(newarr, new_count))
    {
        mostrar_error_operacion("recordatorios", "importar");
    }
    else
    {
        ui_puts("Importación completada (fusionado).");
    }
    free(newarr);
}

/* ===================== AGENDA (Partidos + Recordatorios) ===================== */
typedef struct {
    time_t ts;
    char tipo[32];
    char fechastr[64];
    char titulo[128];
    char detalle[512];
} AgendaItem;

static int parse_storage_datetime_to_tm(const char *s, struct tm *out_tm)
{
    if (!s || !out_tm) return 0;
    int y=0;
    int m=0;
    int d=0;
    int H=0;
    int M=0;
    /* Intentar formato de almacenamiento: YYYY-MM-DD HH:MM */
    int parts = sscanf(s, "%4d-%2d-%2d %2d:%2d", &y, &m, &d, &H, &M);
    if (parts >= 3)
    {
        memset(out_tm, 0, sizeof(*out_tm));
        out_tm->tm_year = y - 1900;
        out_tm->tm_mon = m - 1;
        out_tm->tm_mday = d;
        out_tm->tm_hour = (parts >= 4) ? H : 0;
        out_tm->tm_min = (parts >= 5) ? M : 0;
        out_tm->tm_sec = 0;
        out_tm->tm_isdst = -1;
        return 1;
    }

    /* Fallback: aceptar formato dd/mm/YYYY HH:MM (por compatibilidad con JSON viejo) */
    parts = sscanf(s, "%2d/%2d/%4d %2d:%2d", &d, &m, &y, &H, &M);
    if (parts >= 3)
    {
        memset(out_tm, 0, sizeof(*out_tm));
        out_tm->tm_year = y - 1900;
        out_tm->tm_mon = m - 1;
        out_tm->tm_mday = d;
        out_tm->tm_hour = (parts >= 4) ? H : 0;
        out_tm->tm_min = (parts >= 5) ? M : 0;
        out_tm->tm_sec = 0;
        out_tm->tm_isdst = -1;
        return 1;
    }

    return 0;
}

static time_t parse_datetime_ts(const char *s)
{
    struct tm tm_struct;
    if (!parse_storage_datetime_to_tm(s, &tm_struct)) return (time_t)0;
    return mktime(&tm_struct);
}

static int agenda_item_cmp(const void *a, const void *b)
{
    const AgendaItem *x = a;
    const AgendaItem *y = b;
    if (x->ts < y->ts) return -1;
    if (x->ts > y->ts) return 1;
    return 0;
}
/* Helpers para manejo seguro del array dinámico de AgendaItem */
static int append_agenda_item(AgendaItem **items, size_t *cap, size_t *nitems, const AgendaItem *src)
{
    if (*nitems >= *cap)
    {
        size_t newcap = (*cap == 0) ? 16 : (*cap * 2);
        AgendaItem *tmp = (AgendaItem*)realloc(*items, sizeof(AgendaItem) * newcap);
        if (!tmp) return 0;
        *items = tmp;
        *cap = newcap;
    }
    (*items)[(*nitems)++] = *src;
    return 1;
}

static int add_reminders_to_items(AgendaItem **items, size_t *cap, size_t *nitems)
{
    int rcount = 0;
    Reminder *rarr = load_reminders(&rcount);
    if (!rarr || rcount == 0)
    {
        free(rarr);
        return 1; /* no hay recordatorios, no es error */
    }

    for (int i = 0; i < rcount; i++)
    {
        AgendaItem it;
        memset(&it, 0, sizeof(it));
        it.ts = parse_datetime_ts(rarr[i].fecha);
        strncpy_s(it.tipo, sizeof(it.tipo), "Recordatorio", sizeof(it.tipo)-1);
        strncpy_s(it.fechastr, sizeof(it.fechastr), rarr[i].fecha, sizeof(it.fechastr)-1);
        strncpy_s(it.titulo, sizeof(it.titulo), rarr[i].tematica, sizeof(it.titulo)-1);
        strncpy_s(it.detalle, sizeof(it.detalle), rarr[i].nota, sizeof(it.detalle)-1);

        if (!append_agenda_item(items, cap, nitems, &it))
        {
            free(rarr);
            mostrar_error_operacion("agenda", "memoria");
            return 0;
        }
    }
    free(rarr);
    return 1;
}

static int add_partidos_to_items(AgendaItem **items, size_t *cap, size_t *nitems)
{
    sqlite3_stmt *stmt = prepare_partido_query("ORDER BY p.fecha_hora ASC");
    if (!stmt) return 1; /* no hay partidos o error silencioso */

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AgendaItem it;
        memset(&it, 0, sizeof(it));
        const unsigned char *cancha = sqlite3_column_text(stmt, 0);
        const unsigned char *fecha = sqlite3_column_text(stmt, 1);
        const unsigned char *camiseta = sqlite3_column_text(stmt, 4);
        const unsigned char *resultado = sqlite3_column_text(stmt, 5);
        const unsigned char *comentario = sqlite3_column_text(stmt, 11);

        const char *fecha_s = fecha ? (const char*)fecha : "";
        it.ts = parse_datetime_ts(fecha_s);
        strncpy_s(it.tipo, sizeof(it.tipo), "Partido", sizeof(it.tipo)-1);
        strncpy_s(it.fechastr, sizeof(it.fechastr), fecha_s, sizeof(it.fechastr)-1);
        char titulo_tmp[128];
        snprintf(titulo_tmp, sizeof(titulo_tmp), "%s - %s",
                 cancha ? (const char*)cancha : "Cancha",
                 camiseta ? (const char*)camiseta : "");
        strncpy_s(it.titulo, sizeof(it.titulo), titulo_tmp, sizeof(it.titulo)-1);
        char detalle_tmp[512];
        snprintf(detalle_tmp, sizeof(detalle_tmp), "Resultado: %s %s",
                 resultado ? (const char*)resultado : "",
                 comentario ? (const char*)comentario : "");
        strncpy_s(it.detalle, sizeof(it.detalle), detalle_tmp, sizeof(it.detalle)-1);

        if (!append_agenda_item(items, cap, nitems, &it))
        {
            sqlite3_finalize(stmt);
            mostrar_error_operacion("agenda", "memoria");
            return 0;
        }
    }
    sqlite3_finalize(stmt);
    return 1;
}

static void mostrar_agenda()
{
    size_t cap = 0;
    size_t nitems = 0;
    AgendaItem *items = NULL;

    /* Inicialmente reservar una capacidad pequeña; append_agenda_item la aumentará */
    cap = 16;
    items = (AgendaItem*)malloc(sizeof(AgendaItem) * cap);
    if (!items) { mostrar_error_operacion("agenda", "memoria"); return; }

    if (!add_reminders_to_items(&items, &cap, &nitems)) { free(items); return; }
    if (!add_partidos_to_items(&items, &cap, &nitems)) { free(items); return; }

    if (nitems == 0)
    {
        ui_puts("No hay entradas en la agenda.");
        free(items);
        return;
    }

    qsort(items, nitems, sizeof(AgendaItem), agenda_item_cmp);

    time_t now = time(NULL);

    ui_puts("=== Agenda: Próximos eventos ===");
    int shown = 0;
    for (size_t i = 0; i < nitems; i++)
    {
        if (items[i].ts >= now)
        {
            ui_printf("%s | %s - %s\n", items[i].fechastr, items[i].tipo, items[i].titulo);
            ui_printf("    %s\n", items[i].detalle);
            ui_puts("----------------------------------------");
            shown = 1;
        }
    }
    if (!shown) ui_puts("(No hay eventos futuros)");

    ui_puts("\n=== Agenda: Eventos pasados ===");
    shown = 0;
    for (size_t i = 0; i < nitems; i++)
    {
        if (items[i].ts < now && items[i].ts != (time_t)0)
        {
            ui_printf("%s | %s - %s\n", items[i].fechastr, items[i].tipo, items[i].titulo);
            ui_printf("    %s\n", items[i].detalle);
            ui_puts("----------------------------------------");
            shown = 1;
        }
    }
    if (!shown) ui_puts("(No hay eventos pasados)");

    free(items);
}


void menu_recordatorios(void)
{
    while (1)
    {
        clear_screen();
        print_header("Recordatorios");
        printf("1. Listar recordatorios\n");
        printf("2. Agregar recordatorio\n");
        printf("3. Editar recordatorio\n");
        printf("4. Eliminar recordatorio\n");
        printf("5. Filtrar por temática\n");
        printf("6. Exportar recordatorios\n");
        printf("7. Importar recordatorios\n");
        printf("8. Agenda\n");
        printf("0. Volver\n");

        int op = input_int_rango(">", 0, 8);
        switch (op)
        {
            case 1: {
                int count = 0;
                Reminder *arr = load_reminders(&count);
                listar_recordatorios(arr, count);
                free(arr);
                pause_console();
                break;
            }
            case 2: agregar_recordatorio(); pause_console(); break;
            case 3: editar_recordatorio(); pause_console(); break;
            case 4: eliminar_recordatorio(); pause_console(); break;
            case 5: filtrar_por_tematica(); pause_console(); break;
            case 6: export_recordatorios(); pause_console(); break;
            case 7: import_recordatorios(); pause_console(); break;
            case 8: mostrar_agenda(); pause_console(); break;
            case 0: return;
            default: break;
        }
    }
}
