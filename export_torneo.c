#include "export_torneo.h"
#include "export.h"
#include "db.h"
#include "utils.h"
#include "torneo.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include "direct.h"
#endif
#include <string.h>

static sqlite3_stmt* obtener_datos_torneos(int *count)
{
    sqlite3_stmt *stmt;

    if (!preparar_consulta_con_verificacion(&stmt, "torneo", "torneos para exportar",
                                            "SELECT t.id, t.nombre, t.tiene_equipo_fijo, "
                                            "t.equipo_fijo_id, t.cantidad_equipos, "
                                            "t.tipo_torneo, t.formato_torneo "
                                            "FROM torneo t ORDER BY t.id",
                                            count))
    {
        return NULL;
    }

    return stmt;
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

static void write_csv_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "id,nombre,tiene_equipo_fijo,cantidad_equipos,tipo,formato\n");
}

static void write_csv_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    int tipo = sqlite3_column_int(stmt, 5);
    int formato = sqlite3_column_int(stmt, 6);
    char nombre_limpio[256];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));

    fprintf(f, "%d,%s,%d,%d,%s,%s\n",
            sqlite3_column_int(stmt, 0),
            nombre_limpio,
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 4),
            get_nombre_tipo_torneo((TipoTorneos)tipo),
            get_nombre_formato_torneo((FormatoTorneos)formato));
}

static void write_txt_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "LISTADO DE TORNEOS\n\n");
}

static void write_txt_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    int tipo = sqlite3_column_int(stmt, 5);
    int formato = sqlite3_column_int(stmt, 6);
    int tiene_fijo = sqlite3_column_int(stmt, 2);
    int equipo_fijo_id = sqlite3_column_int(stmt, 3);
    char nombre_limpio[256];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));

    char equipo_fijo_str[64];
    if (tiene_fijo)
        snprintf(equipo_fijo_str, sizeof(equipo_fijo_str), "Si (ID: %d)", equipo_fijo_id);
    else
        snprintf(equipo_fijo_str, sizeof(equipo_fijo_str), "No");

    fprintf(f, "ID: %d - Nombre: %s\n"
            "  Tiene equipo fijo: %s\n"
            "  Cantidad de equipos: %d\n"
            "  Tipo de torneo: %s\n"
            "  Formato de torneo: %s\n\n",
            sqlite3_column_int(stmt, 0),
            nombre_limpio,
            equipo_fijo_str,
            sqlite3_column_int(stmt, 4),
            get_nombre_tipo_torneo((TipoTorneos)tipo),
            get_nombre_formato_torneo((FormatoTorneos)formato));
}

static void write_json_row(FILE *f, sqlite3_stmt *stmt, void *context) /* NOSONAR */
{
    (void)f;
    cJSON *root = (cJSON *)context;
    cJSON *item = cJSON_CreateObject();
    int tipo = sqlite3_column_int(stmt, 5);
    int formato = sqlite3_column_int(stmt, 6);
    char nombre_limpio[256];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));

    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", nombre_limpio);
    cJSON_AddNumberToObject(item, "tiene_equipo_fijo", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "equipo_fijo_id", sqlite3_column_int(stmt, 3));
    cJSON_AddNumberToObject(item, "cantidad_equipos", sqlite3_column_int(stmt, 4));
    cJSON_AddStringToObject(item, "tipo_torneo", get_nombre_tipo_torneo((TipoTorneos)tipo));
    cJSON_AddStringToObject(item, "formato_torneo", get_nombre_formato_torneo((FormatoTorneos)formato));
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
            "<html><body><h1>Torneos</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Equipo Fijo</th><th>Cantidad Equipos</th><th>Tipo</th><th>Formato</th></tr>");
}

static void write_html_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    int tipo = sqlite3_column_int(stmt, 5);
    int formato = sqlite3_column_int(stmt, 6);
    int tiene_fijo = sqlite3_column_int(stmt, 2);
    int equipo_fijo_id = sqlite3_column_int(stmt, 3);
    char nombre_limpio[256];
    char equipo_fijo_str[64];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));

    if (tiene_fijo)
        snprintf(equipo_fijo_str, sizeof(equipo_fijo_str), "Si (ID: %d)", equipo_fijo_id);
    else
        snprintf(equipo_fijo_str, sizeof(equipo_fijo_str), "No");

    fprintf(f,
            "<tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td><td>%s</td></tr>",
            sqlite3_column_int(stmt, 0),
            nombre_limpio,
            equipo_fijo_str,
            sqlite3_column_int(stmt, 4),
            get_nombre_tipo_torneo((TipoTorneos)tipo),
            get_nombre_formato_torneo((FormatoTorneos)formato));
}

static void write_html_footer(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "</table></body></html>");
}

/** @} */

/** @name Funciones de exportacion de torneos */
/** @{ */

EXPORT_FORMAT_ROWS(exportar_torneos_csv, obtener_datos_torneos, "torneos.csv", NULL, write_csv_header, write_csv_row, NULL)
EXPORT_FORMAT_ROWS(exportar_torneos_txt, obtener_datos_torneos, "torneos.txt", NULL, write_txt_header, write_txt_row, NULL)
EXPORT_FORMAT_ROWS(exportar_torneos_json, obtener_datos_torneos, "torneos.json", cJSON_CreateArray(), NULL, write_json_row, write_json_footer)
EXPORT_FORMAT_ROWS(exportar_torneos_html, obtener_datos_torneos, "torneos.html", NULL, write_html_header, write_html_row, write_html_footer)

/** @} */
