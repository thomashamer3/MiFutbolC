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

static int hay_datos_bienestar(void)
{
    return hay_registros("bienestar_objetivo")
           || hay_registros("bienestar_plan_entrenamiento")
           || hay_registros("bienestar_comida")
           || hay_registros("bienestar_habito")
           || hay_registros("bienestar_salud");
}

static sqlite3_stmt* obtener_datos_bienestar(int *count)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT 'objetivo' AS tipo, o.nombre, COALESCE(o.notas, '') AS descripcion, "
        "o.fecha_inicio AS fecha, o.estado AS valor "
        "FROM bienestar_objetivo o "
        "UNION ALL "
        "SELECT 'plan' AS tipo, p.rutina_semanal, COALESCE(p.notas, ''), "
        "'', CAST(p.frecuencia_semanal AS TEXT) "
        "FROM bienestar_plan_entrenamiento p "
        "UNION ALL "
        "SELECT 'comida' AS tipo, c.descripcion, c.calidad, c.fecha, c.tipo "
        "FROM bienestar_comida c "
        "UNION ALL "
        "SELECT 'habito' AS tipo, COALESCE(h.notas, ''), COALESCE(h.estado_animico, ''), "
        "h.fecha, CAST(h.dormi_bien AS TEXT) "
        "FROM bienestar_habito h "
        "UNION ALL "
        "SELECT 'salud' AS tipo, COALESCE(s.tipo_sangre, ''), COALESCE(s.notas, ''), "
        "COALESCE(s.ultima_revision, ''), CAST(COALESCE(s.peso_kg, 0) AS TEXT) "
        "FROM bienestar_salud s "
        "ORDER BY tipo, fecha";

    if (!hay_datos_bienestar())
    {
        printf("No hay datos de bienestar para exportar.\n");
        *count = 0;
        return NULL;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error preparando consulta de bienestar.\n");
        *count = 0;
        return NULL;
    }

    {
        sqlite3_stmt *count_stmt;
        const char *count_sql =
            "SELECT COUNT(*) FROM ("
            "SELECT 1 FROM bienestar_objetivo "
            "UNION ALL SELECT 1 FROM bienestar_plan_entrenamiento "
            "UNION ALL SELECT 1 FROM bienestar_comida "
            "UNION ALL SELECT 1 FROM bienestar_habito "
            "UNION ALL SELECT 1 FROM bienestar_salud)";
        if (sqlite3_prepare_v2(db, count_sql, -1, &count_stmt, NULL) == SQLITE_OK)
        {
            if (sqlite3_step(count_stmt) == SQLITE_ROW)
                *count = sqlite3_column_int(count_stmt, 0);
            sqlite3_finalize(count_stmt);
        }
    }

    return stmt;
}

typedef struct
{
    const char *filename;
    void *context;
    void (*write_header)(FILE *f, void *context);
    void (*write_row)(FILE *f, sqlite3_stmt *stmt, void *context);
    void (*write_footer)(FILE *f, void *context);
} ExportConfig;

static FILE* open_export_file(const char *filename, sqlite3_stmt *stmt)
{
    FILE *f;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || f == NULL)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }
    return f;
}

static void export_bienestar_generic(ExportConfig *config)
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_bienestar(&count);
    if (!stmt) return;

    FILE *f = open_export_file(config->filename, stmt);
    if (!f) return;

    if (config->write_header)
        config->write_header(f, config->context);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        config->write_row(f, stmt, config->context);
    }

    if (config->write_footer)
        config->write_footer(f, config->context);

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path(config->filename));
    fclose(f);
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

static void write_csv_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "tipo,nombre,descripcion,fecha,valor\n");
}

static void write_csv_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    char tipo_limpio[64];
    char nombre_limpio[256];
    char desc_limpio[256];
    char fecha_limpio[64];
    char valor_limpio[64];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 0), tipo_limpio, sizeof(tipo_limpio));
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 2), desc_limpio, sizeof(desc_limpio));
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 3), fecha_limpio, sizeof(fecha_limpio));
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 4), valor_limpio, sizeof(valor_limpio));
    fprintf(f, "%s,%s,%s,%s,%s\n", tipo_limpio, nombre_limpio, desc_limpio, fecha_limpio, valor_limpio);
}

static void write_txt_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "DATOS DE BIENESTAR\n\n");
}

static void write_txt_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f, "Tipo: %s\n"
            "  Nombre: %s\n"
            "  Descripcion: %s\n"
            "  Fecha: %s\n"
            "  Valor: %s\n\n",
            sqlite3_column_text(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3),
            sqlite3_column_text(stmt, 4));
}

static void write_json_row(FILE const *f, sqlite3_stmt *stmt, void *context)
{
    (void)f;
    cJSON *root = (cJSON *)context;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "tipo", (const char*)sqlite3_column_text(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char*)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "descripcion", (const char*)sqlite3_column_text(stmt, 2));
    cJSON_AddStringToObject(item, "fecha", (const char*)sqlite3_column_text(stmt, 3));
    cJSON_AddStringToObject(item, "valor", (const char*)sqlite3_column_text(stmt, 4));
    cJSON_AddItemToArray(root, item);
}

static void write_json_footer(FILE *f, void *context)
{
    cJSON *root = (cJSON *)context;
    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

static void write_html_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f,
            "<html><body><h1>Bienestar</h1><table border='1'>"
            "<tr><th>Tipo</th><th>Nombre</th><th>Descripcion</th><th>Fecha</th><th>Valor</th></tr>");
}

static void write_html_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f,
            "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
            sqlite3_column_text(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3),
            sqlite3_column_text(stmt, 4));
}

static void write_html_footer(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "</table></body></html>");
}

/** @} */

/** @name Funciones de exportacion de bienestar */
/** @{ */

void exportar_bienestar_csv()
{
    ExportConfig config =
    {
        .filename = "bienestar.csv",
        .context = NULL,
        .write_header = write_csv_header,
        .write_row = write_csv_row,
        .write_footer = NULL
    };
    export_bienestar_generic(&config);
}

void exportar_bienestar_txt()
{
    ExportConfig config =
    {
        .filename = "bienestar.txt",
        .context = NULL,
        .write_header = write_txt_header,
        .write_row = write_txt_row,
        .write_footer = NULL
    };
    export_bienestar_generic(&config);
}

void exportar_bienestar_json()
{
    cJSON *root = cJSON_CreateArray();
    ExportConfig config =
    {
        .filename = "bienestar.json",
        .context = root,
        .write_header = NULL,
        .write_row = write_json_row,
        .write_footer = write_json_footer
    };
    export_bienestar_generic(&config);
}

void exportar_bienestar_html()
{
    ExportConfig config =
    {
        .filename = "bienestar.html",
        .context = NULL,
        .write_header = write_html_header,
        .write_row = write_html_row,
        .write_footer = write_html_footer
    };
    export_bienestar_generic(&config);
}

/** @} */
