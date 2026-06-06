#include "recordatorios.h"
#include "utils.h"
#include "menu.h"
#include "db.h"
#include "cJSON.h"
#include "export_partidos_helpers.h"
#include "backup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STORAGE_PATH "Importaciones/recordatorios.json"
#define MAX_NOTA 512
#define MAX_TEMATICA 64
#define MAX_FECHA 64

#define PERIODICIDAD_UNA_VEZ   0
#define PERIODICIDAD_DIARIO    1
#define PERIODICIDAD_SEMANAL   2
#define PERIODICIDAD_MENSUAL   3

typedef struct
{
    long long id;
    char fecha[MAX_FECHA];
    char nota[MAX_NOTA];
    char tematica[MAX_TEMATICA];
    int periodicidad;
    char fecha_fin[MAX_FECHA];
} Reminder;

static const char *periodicidad_str(int p)
{
    switch (p)
    {
    case PERIODICIDAD_DIARIO:
        return "Diario";
    case PERIODICIDAD_SEMANAL:
        return "Semanal";
    case PERIODICIDAD_MENSUAL:
        return "Mensual";
    default:
        return "Una vez";
    }
}

static FILE *open_file_portable(const char *path, const char *mode)
{
#if defined(_WIN32) && defined(_MSC_VER)
    FILE *f = NULL;
    if (fopen_s(&f, path, mode) != 0)
    {
        return NULL;
    }
    return f;
#else
    return fopen(path, mode);
#endif
}

static size_t safe_strlen_s(const char *s, size_t max_len)
{
    size_t i = 0;
    if (!s) return 0;
    while (i < max_len && s[i]) ++i;
    return i;
}

static char *read_text_file(const char *path)
{
    FILE *f = open_file_portable(path, "rb");
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
    fclose(f);

    if (read > 0 && read <= (size_t)len)
        buf[read] = '\0';
    else
        buf[0] = '\0';

    return buf;
}

static void reminder_from_json(Reminder *dst, cJSON const *it, int fallback_id)
{
    cJSON const *jid = cJSON_GetObjectItemCaseSensitive(it, "id");
    cJSON const *jfecha = cJSON_GetObjectItemCaseSensitive(it, "fecha");
    cJSON const *jnota = cJSON_GetObjectItemCaseSensitive(it, "nota");
    cJSON const *jtema = cJSON_GetObjectItemCaseSensitive(it, "tematica");
    cJSON const *jper = cJSON_GetObjectItemCaseSensitive(it, "periodicidad");
    cJSON const *jfin = cJSON_GetObjectItemCaseSensitive(it, "fecha_fin");

    dst->id = jid && cJSON_IsNumber(jid) ? (long long)jid->valuedouble : fallback_id;
    strncpy_s(dst->fecha, MAX_FECHA, jfecha && cJSON_IsString(jfecha) ? jfecha->valuestring : "", MAX_FECHA - 1);
    dst->fecha[MAX_FECHA - 1] = '\0';
    strncpy_s(dst->nota, MAX_NOTA, jnota && cJSON_IsString(jnota) ? jnota->valuestring : "", MAX_NOTA - 1);
    dst->nota[MAX_NOTA - 1] = '\0';
    strncpy_s(dst->tematica, MAX_TEMATICA, jtema && cJSON_IsString(jtema) ? jtema->valuestring : "", MAX_TEMATICA - 1);
    dst->tematica[MAX_TEMATICA - 1] = '\0';
    dst->periodicidad = jper && cJSON_IsNumber(jper) ? (int)jper->valuedouble : PERIODICIDAD_UNA_VEZ;
    strncpy_s(dst->fecha_fin, MAX_FECHA, jfin && cJSON_IsString(jfin) ? jfin->valuestring : "", MAX_FECHA - 1);
    dst->fecha_fin[MAX_FECHA - 1] = '\0';
}

static Reminder *parse_reminders_array(const cJSON *root, int *out_count)
{
    int count = cJSON_GetArraySize(root);
    Reminder *arr = (Reminder*)calloc((size_t)count, sizeof(Reminder));
    if (!arr)
    {
        return NULL;
    }

    for (int i = 0; i < count; i++)
    {
        cJSON const *it = cJSON_GetArrayItem(root, i);
        if (it && cJSON_IsObject(it))
        {
            reminder_from_json(&arr[i], it, i + 1);
        }
        else
        {
            arr[i].id = i + 1;
            arr[i].fecha[0] = '\0';
            arr[i].nota[0] = '\0';
            arr[i].tematica[0] = '\0';
            arr[i].periodicidad = PERIODICIDAD_UNA_VEZ;
            arr[i].fecha_fin[0] = '\0';
        }
    }

    *out_count = count;
    return arr;
}

static Reminder *load_reminders_from_path(const char *path, int *out_count)
{
    if (!out_count || !path)
    {
        return NULL;
    }

    *out_count = 0;
    char *buf = read_text_file(path);
    if (!buf)
    {
        return NULL;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root))
    {
        if (root) cJSON_Delete(root);
        return NULL;
    }

    Reminder *arr = parse_reminders_array(root, out_count);

    cJSON_Delete(root);
    return arr;
}

static Reminder *load_reminders(int *out_count)
{
    return load_reminders_from_path(STORAGE_PATH, out_count);
}

static int write_reminders_to_file(const Reminder *arr, int count, const char *path)
{
    if (count > 0 && !arr) return 0;
    cJSON *root = cJSON_CreateArray();
    if (!root) return 0;

    for (int i = 0; i < count; i++)
    {
        long long id_value = 0;
        const char *fecha = "";
        const char *nota = "";
        const char *tematica = "";
        int periodicidad = PERIODICIDAD_UNA_VEZ;
        const char *fecha_fin = "";

        if (arr)
        {
            id_value = arr[i].id;
            fecha = arr[i].fecha;
            nota = arr[i].nota;
            tematica = arr[i].tematica;
            periodicidad = arr[i].periodicidad;
            fecha_fin = arr[i].fecha_fin;
        }

        cJSON *obj = cJSON_CreateObject();
        if (!obj) continue;
        cJSON_AddNumberToObject(obj, "id", (double)id_value);
        cJSON_AddStringToObject(obj, "fecha", fecha);
        cJSON_AddStringToObject(obj, "nota", nota);
        cJSON_AddStringToObject(obj, "tematica", tematica);
        cJSON_AddNumberToObject(obj, "periodicidad", (double)periodicidad);
        cJSON_AddStringToObject(obj, "fecha_fin", fecha_fin);
        cJSON_AddItemToArray(root, obj);
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return 0;

    FILE *f = open_file_portable(path, "wb");
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
    return load_reminders_from_path(path, out_count);
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
        char tematica_mostrar[MAX_TEMATICA + 32];
        const char *sufijo = periodicidad_str(arr[i].periodicidad);
        if (arr[i].periodicidad == PERIODICIDAD_UNA_VEZ)
        {
            strncpy_s(tematica_mostrar, sizeof(tematica_mostrar), arr[i].tematica, sizeof(tematica_mostrar) - 1);
        }
        else
        {
            snprintf(tematica_mostrar, sizeof(tematica_mostrar), "%s (%s)", arr[i].tematica, sufijo);
        }
        tematica_mostrar[sizeof(tematica_mostrar) - 1] = '\0';

        printf("[%lld] %s - %s\n", arr[i].id, tematica_mostrar, arr[i].fecha);
        if (arr[i].fecha_fin[0] != '\0')
            printf("    Hasta: %s\n", arr[i].fecha_fin);
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

static void elegir_periodicidad(char *out, int size)
{
    printf("Elija periodicidad:\n");
    printf("0. Una vez\n");
    printf("1. Diario\n");
    printf("2. Semanal\n");
    printf("3. Mensual\n");
    int sel = input_int_rango(">", 0, 3);
    snprintf(out, (size_t)size, "%d", sel);
    out[size - 1] = '\0';
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

    char per_buf[16];
    elegir_periodicidad(per_buf, sizeof(per_buf));
    nuevo.periodicidad = atoi(per_buf);
    if (nuevo.periodicidad != PERIODICIDAD_UNA_VEZ)
    {
        input_date("Fecha fin (dd/mm/yyyy hh:mm) - opcional, dejar vacío si no aplica:", nuevo.fecha_fin, MAX_FECHA);
        if (strnlen_s(nuevo.fecha_fin, (size_t)-1) == 0)
        {
            nuevo.fecha_fin[0] = '\0';
        }
    }
    else
    {
        nuevo.fecha_fin[0] = '\0';
    }

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

static void editar_fecha_recordatorio(Reminder *r)
{
    if (!r) return;
    printf("Valor actual fecha: %s\n", r->fecha);
    input_date("Nueva fecha (dd/mm/yyyy hh:mm):", r->fecha, MAX_FECHA);
}

static void editar_nota_recordatorio(Reminder *r)
{
    char buf[MAX_NOTA];
    if (!r) return;

    printf("Valor actual nota:\n%s\n", r->nota);
    input_string_extended("Nueva nota:", buf, MAX_NOTA);
    if (buf[0] != '\0')
    {
        strncpy_s(r->nota, MAX_NOTA, buf, MAX_NOTA - 1);
        r->nota[MAX_NOTA - 1] = '\0';
    }
}

static void editar_tematica_recordatorio(Reminder *r)
{
    char tema[MAX_TEMATICA];
    if (!r) return;

    printf("Valor actual temática: %s\n", r->tematica);
    elegir_tematica(tema, MAX_TEMATICA);
    if (tema[0] != '\0')
    {
        strncpy_s(r->tematica, MAX_TEMATICA, tema, MAX_TEMATICA - 1);
        r->tematica[MAX_TEMATICA - 1] = '\0';
    }
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

    printf("\n¿Que desea modificar?\n");
    printf("1) Todos los atributos\n");
    printf("2) Solo fecha\n");
    printf("3) Solo nota\n");
    printf("4) Solo temática\n");
    printf("5) Cancelar\n");
    int opcion = input_int_rango(">", 1, 5);

    if (opcion == 5)
    {
        ui_puts("Edicion cancelada.");
        free(arr);
        return;
    }

    if (opcion == 1 || opcion == 2)
    {
        editar_fecha_recordatorio(&arr[idx]);
    }
    if (opcion == 1 || opcion == 3)
    {
        editar_nota_recordatorio(&arr[idx]);
    }
    if (opcion == 1 || opcion == 4)
    {
        editar_tematica_recordatorio(&arr[idx]);
    }

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
            char tematica_mostrar[MAX_TEMATICA + 32];
            const char *sufijo = periodicidad_str(arr[i].periodicidad);
            if (arr[i].periodicidad == PERIODICIDAD_UNA_VEZ)
            {
                strncpy_s(tematica_mostrar, sizeof(tematica_mostrar), arr[i].tematica, sizeof(tematica_mostrar) - 1);
            }
            else
            {
                snprintf(tematica_mostrar, sizeof(tematica_mostrar), "%s (%s)", arr[i].tematica, sufijo);
            }
            tematica_mostrar[sizeof(tematica_mostrar) - 1] = '\0';
            ui_printf("[%lld] %s - %s\n    %s\n", arr[i].id, tematica_mostrar, arr[i].fecha, arr[i].nota);
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

static int fecha_coincide_con_hoy(struct tm *tm_fecha)
{
    time_t now = time(NULL);
    struct tm tm_hoy;
    if (localtime_s(&tm_hoy, &now) != 0) return 0;
    return (tm_fecha->tm_year == tm_hoy.tm_year &&
            tm_fecha->tm_mon == tm_hoy.tm_mon &&
            tm_fecha->tm_mday == tm_hoy.tm_mday);
}

/* Forward declaration for parse_storage_datetime_to_tm (defined in AGENDA section) */
static int parse_storage_datetime_to_tm(const char *s, struct tm *out_tm);

static int fecha_previo_o_hoy(struct tm *tm_fecha)
{
    time_t now = time(NULL);
    struct tm tm_hoy;
    if (localtime_s(&tm_hoy, &now) != 0) return 0;
    time_t t_fecha = mktime(tm_fecha);
    time_t t_hoy = mktime(&tm_hoy);
    return (t_fecha <= t_hoy);
}

static int agregar_dias_a_fecha(const char *fecha_orig, int dias, char *out, int size)
{
    struct tm tm;
    if (!parse_storage_datetime_to_tm(fecha_orig, &tm)) return 0;
    time_t t = mktime(&tm);
    if (t == (time_t)-1) return 0;
    t += (time_t)dias * 86400;
    if (localtime_s(&tm, &t) != 0) return 0;
    snprintf(out, (size_t)size, "%04d-%02d-%02d %02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min);
    out[size - 1] = '\0';
    return 1;
}

static int agregar_mes_a_fecha(const char *fecha_orig, char *out, int size)
{
    struct tm tm;
    if (!parse_storage_datetime_to_tm(fecha_orig, &tm)) return 0;
    tm.tm_mon++;
    if (tm.tm_mon > 11)
    {
        tm.tm_mon = 0;
        tm.tm_year++;
    }
    snprintf(out, (size_t)size, "%04d-%02d-%02d %02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min);
    out[size - 1] = '\0';
    return 1;
}

static int fecha_str_vacia_o_nula(const char *s)
{
    return !s || s[0] == '\0';
}

static int fecha_pasada_o_hoy(const char *fecha_str)
{
    struct tm tm;
    if (!parse_storage_datetime_to_tm(fecha_str, &tm)) return 0;
    return fecha_previo_o_hoy(&tm);
}

static void verificar_recordatorios_recurrentes()
{
    int count = 0;
    Reminder *arr = load_reminders(&count);
    if (!arr || count == 0)
    {
        mostrar_no_hay_registros("recordatorios");
        free(arr);
        return;
    }

    int nuevos = 0;
    int capacidad = count + 16;
    Reminder *expandido = (Reminder*)malloc(sizeof(Reminder) * (size_t)capacidad);
    if (!expandido)
    {
        mostrar_error_operacion("recordatorios", "memoria");
        free(arr);
        return;
    }
    memcpy(expandido, arr, sizeof(Reminder) * (size_t)count);
    free(arr);
    int actual = count;

    for (int i = 0; i < actual; i++)
    {
        if (expandido[i].periodicidad == PERIODICIDAD_UNA_VEZ) continue;

        if (!fecha_pasada_o_hoy(expandido[i].fecha)) continue;

        if (!fecha_str_vacia_o_nula(expandido[i].fecha_fin))
        {
            struct tm tm_fin;
            if (parse_storage_datetime_to_tm(expandido[i].fecha_fin, &tm_fin))
            {
                struct tm tm_actual;
                if (parse_storage_datetime_to_tm(expandido[i].fecha, &tm_actual))
                {
                    if (mktime(&tm_actual) > mktime(&tm_fin)) continue;
                }
            }
        }

        struct tm tm_fecha;
        if (!parse_storage_datetime_to_tm(expandido[i].fecha, &tm_fecha)) continue;

        if (!fecha_coincide_con_hoy(&tm_fecha))
        {
            int avanzar = 0;
            switch (expandido[i].periodicidad)
            {
            case PERIODICIDAD_DIARIO:
                avanzar = 1;
                break;
            case PERIODICIDAD_SEMANAL:
                avanzar = 7;
                break;
            case PERIODICIDAD_MENSUAL:
                avanzar = 30;
                break;
            }
            if (avanzar == 0) continue;

            char nueva_fecha[MAX_FECHA];
            int ok = 0;
            for (int intento = 0; intento < 31; intento++)
            {
                char temp[MAX_FECHA];
                if (expandido[i].periodicidad == PERIODICIDAD_MENSUAL)
                    ok = agregar_mes_a_fecha(expandido[i].fecha, temp, MAX_FECHA);
                else
                    ok = agregar_dias_a_fecha(expandido[i].fecha, avanzar * (intento + 1), temp, MAX_FECHA);
                if (!ok) break;

                struct tm tm_temp;
                if (!parse_storage_datetime_to_tm(temp, &tm_temp))
                {
                    ok = 0;
                    break;
                }

                if (fecha_coincide_con_hoy(&tm_temp))
                {
                    strncpy_s(nueva_fecha, MAX_FECHA, temp, MAX_FECHA - 1);
                    nueva_fecha[MAX_FECHA - 1] = '\0';
                    ok = 1;
                    break;
                }

                if (fecha_previo_o_hoy(&tm_temp))
                {
                    strncpy_s(expandido[i].fecha, MAX_FECHA, temp, MAX_FECHA - 1);
                    expandido[i].fecha[MAX_FECHA - 1] = '\0';
                    continue;
                }

                break;
            }

            if (ok)
            {
                if (actual >= capacidad)
                {
                    capacidad *= 2;
                    Reminder *tmp = (Reminder*)realloc(expandido, sizeof(Reminder) * (size_t)capacidad);
                    if (!tmp) break;
                    expandido = tmp;
                }

                Reminder dup;
                memset(&dup, 0, sizeof(dup));
                dup.id = obtener_siguiente_id_local(expandido, actual);
                strncpy_s(dup.fecha, MAX_FECHA, nueva_fecha, MAX_FECHA - 1);
                dup.fecha[MAX_FECHA - 1] = '\0';
                strncpy_s(dup.nota, MAX_NOTA, expandido[i].nota, MAX_NOTA - 1);
                dup.nota[MAX_NOTA - 1] = '\0';
                strncpy_s(dup.tematica, MAX_TEMATICA, expandido[i].tematica, MAX_TEMATICA - 1);
                dup.tematica[MAX_TEMATICA - 1] = '\0';
                dup.periodicidad = expandido[i].periodicidad;
                strncpy_s(dup.fecha_fin, MAX_FECHA, expandido[i].fecha_fin, MAX_FECHA - 1);
                dup.fecha_fin[MAX_FECHA - 1] = '\0';

                expandido[actual++] = dup;
                nuevos++;
            }
        }
    }

    if (nuevos > 0)
    {
        ui_printf("Se generaron %d recordatorio(s) recurrente(s) para hoy.\n", nuevos);
        if (!save_reminders(expandido, actual))
            mostrar_error_operacion("recordatorios", "guardar");
    }
    else
    {
        ui_puts("No hay recordatorios recurrentes pendientes para hoy.");
    }

    free(expandido);

    verificar_backup_programado();
}

/* ===================== AGENDA (Partidos + Recordatorios) ===================== */
typedef struct
{
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
    int parts;
#if defined(_WIN32) && defined(_MSC_VER)
    parts = sscanf_s(s, "%4d-%2d-%2d %2d:%2d", &y, &m, &d, &H, &M);
#else
    parts = sscanf(s, "%4d-%2d-%2d %2d:%2d", &y, &m, &d, &H, &M);
#endif
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
#if defined(_WIN32) && defined(_MSC_VER)
    parts = sscanf_s(s, "%2d/%2d/%4d %2d:%2d", &d, &m, &y, &H, &M);
#else
    parts = sscanf(s, "%2d/%2d/%4d %2d:%2d", &d, &m, &y, &H, &M);
#endif
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

        char titulo_tmp[128];
        const char *sufijo = periodicidad_str(rarr[i].periodicidad);
        if (rarr[i].periodicidad == PERIODICIDAD_UNA_VEZ)
        {
            strncpy_s(titulo_tmp, sizeof(titulo_tmp), rarr[i].tematica, sizeof(titulo_tmp)-1);
        }
        else
        {
            snprintf(titulo_tmp, sizeof(titulo_tmp), "%s (%s)", rarr[i].tematica, sufijo);
        }
        strncpy_s(it.titulo, sizeof(it.titulo), titulo_tmp, sizeof(it.titulo)-1);
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
    if (!items)
    {
        mostrar_error_operacion("agenda", "memoria");
        return;
    }

    if (!add_reminders_to_items(&items, &cap, &nitems))
    {
        free(items);
        return;
    }
    if (!add_partidos_to_items(&items, &cap, &nitems))
    {
        free(items);
        return;
    }

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

static void accion_listar_recordatorios(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Listar recordatorios");
    int count = 0;
    Reminder *arr = load_reminders(&count);
    listar_recordatorios(arr, count);
    free(arr);
    pause_console();
}

static void accion_agregar_recordatorio(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Agregar recordatorio");
    agregar_recordatorio();
    pause_console();
}

static void accion_editar_recordatorio(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Editar recordatorio");
    editar_recordatorio();
    pause_console();
}

static void accion_eliminar_recordatorio(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Eliminar recordatorio");
    eliminar_recordatorio();
    pause_console();
}

static void accion_filtrar_recordatorios(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Filtrar por tematica");
    filtrar_por_tematica();
    pause_console();
}

static void accion_exportar_recordatorios(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Exportar recordatorios");
    export_recordatorios();
    pause_console();
}

static void accion_importar_recordatorios(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Importar recordatorios");
    import_recordatorios();
    pause_console();
}

static void accion_mostrar_agenda(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Agenda");
    mostrar_agenda();
    pause_console();
}

static void accion_verificar_recurrentes(void)
{
    app_log_event("RECORDATORIOS", "Opcion seleccionada: Verificar recordatorios recurrentes");
    verificar_recordatorios_recurrentes();
    pause_console();
}

void menu_recordatorios(void)
{
    MenuItem items[] =
    {
        {1, "Listar recordatorios", &accion_listar_recordatorios},
        {2, "Agregar recordatorio", &accion_agregar_recordatorio},
        {3, "Editar recordatorio", &accion_editar_recordatorio},
        {4, "Eliminar recordatorio", &accion_eliminar_recordatorio},
        {5, "Filtrar por tematica", &accion_filtrar_recordatorios},
        {6, "Exportar recordatorios", &accion_exportar_recordatorios},
        {7, "Importar recordatorios", &accion_importar_recordatorios},
        {8, "Agenda", &accion_mostrar_agenda},
        {9, "Verificar recordatorios recurrentes", &accion_verificar_recurrentes},
        {0, "Volver", NULL}
    };

    ejecutar_menu("RECORDATORIOS", items, 10);
}
