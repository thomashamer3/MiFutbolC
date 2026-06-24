#include "cJSON.h"
#include "db.h"
#include "export.h"
#include "utils.h"
#include <stdio.h>

static int hay_datos_bienestar(void)
{
    return hay_registros("bienestar_objetivo") || hay_registros("bienestar_plan_entrenamiento") ||
           hay_registros("bienestar_comida") || hay_registros("bienestar_habito") ||
           hay_registros("bienestar_salud");
}

static sqlite3_stmt *obtener_datos_bienestar(int *count)
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
        const char *count_sql = "SELECT COUNT(*) FROM ("
                                "SELECT 1 FROM bienestar_objetivo "
                                "UNION ALL SELECT 1 FROM bienestar_plan_entrenamiento "
                                "UNION ALL SELECT 1 FROM bienestar_comida "
                                "UNION ALL SELECT 1 FROM bienestar_habito "
                                "UNION ALL SELECT 1 FROM bienestar_salud)";
        if (sqlite3_prepare_v2(db, count_sql, -1, &count_stmt, NULL) == SQLITE_OK)
        {
            if (sqlite3_step(count_stmt) == SQLITE_ROW)
            {
                *count = sqlite3_column_int(count_stmt, 0);
            }
            sqlite3_finalize(count_stmt);
        }
    }

    return stmt;
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

static void write_csv_header(FILE *file, void *context)
{
    (void)context;
    fprintf(file, "tipo,nombre,descripcion,fecha,valor\n");
}

static void write_csv_row(FILE *file, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    char tipo_limpio[64];
    char nombre_limpio[256];
    char desc_limpio[256];
    char fecha_limpio[64];
    char valor_limpio[64];
    sanitizar_ascii_basico((const char *)sqlite3_column_text(stmt, 0), tipo_limpio,
                           sizeof(tipo_limpio));
    sanitizar_ascii_basico((const char *)sqlite3_column_text(stmt, 1), nombre_limpio,
                           sizeof(nombre_limpio));
    sanitizar_ascii_basico((const char *)sqlite3_column_text(stmt, 2), desc_limpio,
                           sizeof(desc_limpio));
    sanitizar_ascii_basico((const char *)sqlite3_column_text(stmt, 3), fecha_limpio,
                           sizeof(fecha_limpio));
    sanitizar_ascii_basico((const char *)sqlite3_column_text(stmt, 4), valor_limpio,
                           sizeof(valor_limpio));
    fprintf(file, "%s,%s,%s,%s,%s\n", tipo_limpio, nombre_limpio, desc_limpio, fecha_limpio,
            valor_limpio);
}

static void write_txt_header(FILE *file, void *context)
{
    (void)context;
    fprintf(file, "DATOS DE BIENESTAR\n\n");
}

static void write_txt_row(FILE *file, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(file,
            "Tipo: %s\n"
            "  Nombre: %s\n"
            "  Descripcion: %s\n"
            "  Fecha: %s\n"
            "  Valor: %s\n\n",
            sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
            sqlite3_column_text(stmt, 4));
}

static void write_json_row(FILE *file, sqlite3_stmt *stmt, void *context) /* NOSONAR */
{
    (void)file;
    cJSON *root = (cJSON *)context;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "tipo", (const char *)sqlite3_column_text(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "descripcion", (const char *)sqlite3_column_text(stmt, 2));
    cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 3));
    cJSON_AddStringToObject(item, "valor", (const char *)sqlite3_column_text(stmt, 4));
    cJSON_AddItemToArray(root, item);
}

static void write_html_header(FILE *file, void *context)
{
    (void)context;
    fprintf(
        file,
        "<html><body><h1>Bienestar</h1><table border='1'>"
        "<tr><th>Tipo</th><th>Nombre</th><th>Descripcion</th><th>Fecha</th><th>Valor</th></tr>");
}

static void write_html_row(FILE *file, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(file, "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
            sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
            sqlite3_column_text(stmt, 4));
}

/** @} */

/** @name Funciones de exportacion de bienestar */
/** @{ */

EXPORT_FORMAT_ROWS(exportar_bienestar_csv, obtener_datos_bienestar, "bienestar.csv", NULL,
                   write_csv_header, write_csv_row, NULL)
EXPORT_FORMAT_ROWS(exportar_bienestar_txt, obtener_datos_bienestar, "bienestar.txt", NULL,
                   write_txt_header, write_txt_row, NULL)
EXPORT_FORMAT_ROWS(exportar_bienestar_json, obtener_datos_bienestar, "bienestar.json",
                   cJSON_CreateArray(), NULL, write_json_row, export_write_json_footer)
EXPORT_FORMAT_ROWS(exportar_bienestar_html, obtener_datos_bienestar, "bienestar.html", NULL,
                   write_html_header, write_html_row, export_write_html_footer)

void exportar_bienestar_all(void)
{
    ExportConfig configs[] =
    {
        {"bienestar.csv", NULL, write_csv_header, write_csv_row, NULL},
        {"bienestar.txt", NULL, write_txt_header, write_txt_row, NULL},
        {"bienestar.json", cJSON_CreateArray(), NULL, write_json_row, export_write_json_footer},
        {"bienestar.html", NULL, write_html_header, write_html_row, export_write_html_footer}
    };
    export_all_formats(obtener_datos_bienestar, configs, 4);
}

/** @} */
