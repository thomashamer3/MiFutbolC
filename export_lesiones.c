#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>

/* ============================================================================
 * CONSULTAS SQL ESTÁTICAS - Centralizadas para mantenimiento
 * ============================================================================ */

static const char *SQL_COUNT_LESIONES = "SELECT COUNT(*) FROM lesion";

static const char *SQL_LESIONES = "SELECT id, jugador, tipo, descripcion, fecha FROM lesion";

/* ============================================================================
 * HELPER ESTÁTICOS
 * ============================================================================ */

/** @brief Verifica si hay lesiones para exportar */
static int has_lesiones(void)
{
    sqlite3_stmt *stmt;
    int count = 0;
    int result = 0;

    if (sqlite3_prepare_v2(db, SQL_COUNT_LESIONES, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        result = count > 0;
    }

    return result;
}

/** @brief Escribe lesiones en formato CSV */
static void write_lesiones_csv(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_LESIONES, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe lesiones en formato TXT */
static void write_lesiones_txt(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_LESIONES, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d - %s | %s | %s | %s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe lesiones en formato HTML */
static void write_lesiones_html(FILE *file)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_LESIONES, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file,
                "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe lesiones en formato JSON */
static void write_lesiones_json(FILE *file)
{
    cJSON *root = cJSON_CreateArray();
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, SQL_LESIONES, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(item, "jugador", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(item, "tipo", (const char *)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(item, "descripcion", (const char *)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

/* ============================================================================
 * EXPORTACIÓN LESIONES (4 formatos)
 * ============================================================================ */

/**
 * @brief Exporta las lesiones a un archivo CSV
 *
 * Crea un archivo CSV con todas las lesiones registradas en la base de datos,
 * incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_csv()
{
    if (!has_lesiones())
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.csv"), "w");
    if (!f)
    {
        printf("Error al crear el archivo CSV\n");
        return;
    }

    fprintf(f, "id,jugador,tipo,descripcion,fecha\n");
    write_lesiones_csv(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.csv"));
}

/**
 * @brief Exporta las lesiones a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de todas las lesiones
 * registradas, incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_txt()
{
    if (!has_lesiones())
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.txt"), "w");
    if (!f)
    {
        printf("Error al crear el archivo TXT\n");
        return;
    }

    fprintf(f, "LISTADO DE LESIONES\n\n");
    write_lesiones_txt(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.txt"));
}

/**
 * @brief Exporta las lesiones a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando todas las lesiones
 * registradas, incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_json()
{
    if (!has_lesiones())
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.json"), "w");
    if (!f)
    {
        printf("Error al crear el archivo JSON\n");
        return;
    }

    write_lesiones_json(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.json"));
}

/**
 * @brief Exporta las lesiones a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra todas las lesiones
 * registradas, incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_html()
{
    if (!has_lesiones())
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.html"), "w");
    if (!f)
    {
        printf("Error al crear el archivo HTML\n");
        return;
    }

    fprintf(f,
            "<html><body><h1>Lesiones</h1><table border='1'>"
            "<tr><th>ID</th><th>Jugador</th><th>Tipo</th><th>Descripción</th><th>Fecha</th></tr>");

    write_lesiones_html(f);

    fprintf(f, "</table></body></html>");

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.html"));
}
