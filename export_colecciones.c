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

static void escribir_fila_coleccion_csv(FILE *f, sqlite3_stmt *stmt)
{
    char nombre_limpio[256];
    char desc_limpio[256];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 2), desc_limpio, sizeof(desc_limpio));
    fprintf(f, "%d,%s,%s\n",
            sqlite3_column_int(stmt, 0),
            nombre_limpio,
            desc_limpio);
}

static void escribir_fila_coleccion_txt(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "ID: %d\n  Nombre: %s\n  Descripcion: %s\n  Fecha: %s\n\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3));
}

static void escribir_objeto_coleccion(cJSON *item, sqlite3_stmt *stmt)
{
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char*)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "descripcion", (const char*)sqlite3_column_text(stmt, 2));
}

static void escribir_fila_coleccion_html(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2));
}

static void escribir_fila_item_csv(FILE *f, sqlite3_stmt *stmt)
{
    char nombre_limpio[256];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));
    fprintf(f, "%d,%s,%d,%d,%d\n",
            sqlite3_column_int(stmt, 0),
            nombre_limpio,
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 4));
}

static void escribir_fila_item_txt(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "ID: %d - Nombre: %s - Tipo: %d - Rareza: %d\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 2));
}

static void escribir_objeto_item(cJSON *item, sqlite3_stmt *stmt)
{
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char*)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "tipo", sqlite3_column_int(stmt, 3));
    cJSON_AddNumberToObject(item, "rareza", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "coleccion_id", sqlite3_column_int(stmt, 4));
}

static void escribir_fila_item_html(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "<tr><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 4));
}

static void escribir_fila_inventario_csv(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "%d,%d,%d,%d\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3));
}

static void escribir_fila_inventario_txt(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "ID: %d - Coleccion: %d - Item: %d - Cantidad: %d\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3));
}

static void escribir_objeto_inventario(cJSON *item, sqlite3_stmt *stmt)
{
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddNumberToObject(item, "coleccion_id", sqlite3_column_int(stmt, 1));
    cJSON_AddNumberToObject(item, "item_id", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "cantidad", sqlite3_column_int(stmt, 3));
}

static void escribir_fila_inventario_html(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "<tr><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3));
}

static const char *SQL_COLECCIONES = "SELECT id, nombre, descripcion, COALESCE(fecha_creacion, '') FROM coleccion ORDER BY id";
static const char *SQL_ITEMS = "SELECT i.id, COALESCE(i.nombre, ''), COALESCE(i.estado, 0), i.tipo, COALESCE(i.camiseta_id, 0) FROM inventario_item i ORDER BY i.id";
static const char *SQL_INVENTARIO = "SELECT ROW_NUMBER() OVER () AS id, ci.coleccion_id, ci.inventario_id AS item_id, 1 AS cantidad FROM coleccion_inventario ci ORDER BY ci.coleccion_id, ci.inventario_id";

void exportar_colecciones_csv()
{
    if (!hay_registros("coleccion") && !hay_registros("inventario_item") && !hay_registros("coleccion_inventario"))
    {
        printf("No hay datos de colecciones para exportar.\n");
        return;
    }

    FILE *f = abrir_archivo_exportacion("colecciones.csv", "Error al crear el archivo CSV");
    if (!f) return;

    fprintf(f, "=== COLECCIONES ===\n");
    escribir_seccion_csv(f, SQL_COLECCIONES, "id,nombre,descripcion", escribir_fila_coleccion_csv);

    fprintf(f, "=== ITEMS ===\n");
    escribir_seccion_csv(f, SQL_ITEMS, "id,nombre,tipo,rareza,coleccion_id", escribir_fila_item_csv);

    fprintf(f, "=== INVENTARIO ===\n");
    escribir_seccion_csv(f, SQL_INVENTARIO, "id,coleccion_id,item_id,cantidad", escribir_fila_inventario_csv);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("colecciones.csv"));
}

void exportar_colecciones_txt()
{
    if (!hay_registros("coleccion") && !hay_registros("inventario_item") && !hay_registros("coleccion_inventario"))
    {
        printf("No hay datos de colecciones para exportar.\n");
        return;
    }

    FILE *f = abrir_archivo_exportacion("colecciones.txt", "Error al crear el archivo TXT");
    if (!f) return;

    fprintf(f, "COLECCIONES E INVENTARIO\n\n");
    escribir_seccion_txt(f, "COLECCIONES", SQL_COLECCIONES, escribir_fila_coleccion_txt);
    escribir_seccion_txt(f, "ITEMS", SQL_ITEMS, escribir_fila_item_txt);
    escribir_seccion_txt(f, "INVENTARIO", SQL_INVENTARIO, escribir_fila_inventario_txt);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("colecciones.txt"));
}

void exportar_colecciones_json()
{
    if (!hay_registros("coleccion") && !hay_registros("inventario_item") && !hay_registros("coleccion_inventario"))
    {
        printf("No hay datos de colecciones para exportar.\n");
        return;
    }

    cJSON *root = cJSON_CreateObject();

    cJSON *colecciones = cJSON_CreateArray();
    escribir_seccion_json(colecciones, SQL_COLECCIONES, escribir_objeto_coleccion);
    cJSON_AddItemToObject(root, "colecciones", colecciones);

    cJSON *items = cJSON_CreateArray();
    escribir_seccion_json(items, SQL_ITEMS, escribir_objeto_item);
    cJSON_AddItemToObject(root, "items", items);

    cJSON *inventario = cJSON_CreateArray();
    escribir_seccion_json(inventario, SQL_INVENTARIO, escribir_objeto_inventario);
    cJSON_AddItemToObject(root, "inventario", inventario);

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("colecciones.json"), "w");
    if (err != 0 || f == NULL)
    {
        printf("Error al crear el archivo JSON.\n");
        cJSON_Delete(root);
        return;
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("colecciones.json"));
}

void exportar_colecciones_html()
{
    if (!hay_registros("coleccion") && !hay_registros("inventario_item") && !hay_registros("coleccion_inventario"))
    {
        printf("No hay datos de colecciones para exportar.\n");
        return;
    }

    FILE *f = abrir_archivo_exportacion("colecciones.html", "Error al crear el archivo HTML");
    if (!f) return;

    fprintf(f, "<html><body><h1>Colecciones e Inventario</h1>\n");

    const char *cab_colecciones[] = {"ID", "Nombre", "Descripcion", NULL};
    escribir_seccion_html(f, "Colecciones", SQL_COLECCIONES, cab_colecciones, escribir_fila_coleccion_html);

    const char *cab_items[] = {"ID", "Nombre", "Tipo", "Rareza", "Coleccion ID", NULL};
    escribir_seccion_html(f, "Items", SQL_ITEMS, cab_items, escribir_fila_item_html);

    const char *cab_inv[] = {"ID", "Coleccion ID", "Item ID", "Cantidad", NULL};
    escribir_seccion_html(f, "Inventario", SQL_INVENTARIO, cab_inv, escribir_fila_inventario_html);

    fprintf(f, "</body></html>");
    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("colecciones.html"));
}
