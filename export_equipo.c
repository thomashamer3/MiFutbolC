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

static sqlite3_stmt* obtener_datos_equipos(int *count)
{
    sqlite3_stmt *stmt;

    if (!preparar_consulta_con_verificacion(&stmt, "equipo", "equipos para exportar",
                                            "SELECT e.id, e.nombre, e.tipo, e.tipo_futbol, e.partido_id, "
                                            "COUNT(j.id) as num_jugadores, "
                                            "COALESCE(GROUP_CONCAT(DISTINCT j.posicion), '') as posiciones "
                                            "FROM equipo e "
                                            "LEFT JOIN jugador j ON e.id = j.equipo_id "
                                            "GROUP BY e.id "
                                            "ORDER BY e.id",
                                            count))
    {
        return NULL;
    }

    return stmt;
}

static const char* tipo_equipo_to_text(int tipo)
{
    switch (tipo)
    {
    case 0:
        return "FIJO";
    case 1:
        return "MOMENTANEO";
    default:
        return "DESCONOCIDO";
    }
}

static const char* tipo_futbol_to_text(int tipo)
{
    switch (tipo)
    {
    case 0:
        return "FUTBOL_5";
    case 1:
        return "FUTBOL_7";
    case 2:
        return "FUTBOL_8";
    case 3:
        return "FUTBOL_11";
    default:
        return "DESCONOCIDO";
    }
}

static const char* posicion_to_text(int pos)
{
    switch (pos)
    {
    case 0:
        return "ARQUERO";
    case 1:
        return "DEFENSOR";
    case 2:
        return "MEDIOCAMPISTA";
    case 3:
        return "DELANTERO";
    default:
        return "DESCONOCIDO";
    }
}

static void posiciones_to_text(const char *posiciones_csv, char *out, size_t out_size)
{
    char buffer[256];
    const char *token;
    char *saveptr;
    int first = 1;

    strncpy_s(buffer, sizeof(buffer), posiciones_csv, _TRUNCATE);
    out[0] = '\0';

    token = strtok_r(buffer, ",", &saveptr);
    while (token != NULL)
    {
        int pos = atoi(token);
        const char *label = posicion_to_text(pos);
        if (!first)
            strncat_s(out, out_size, ", ", _TRUNCATE);
        strncat_s(out, out_size, label, _TRUNCATE);
        first = 0;
        token = strtok_r(NULL, ",", &saveptr);
    }
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

static void write_csv_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "id,nombre,tipo,tipo_futbol,num_jugadores,posiciones\n");
}

static void write_csv_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    fprintf(f, "%d,%s,%d,%d,%d,%s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_text(stmt, 6));
}

static void write_txt_header(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "LISTADO DE EQUIPOS\n\n");
}

static void write_txt_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    char pos_etiquetas[256] = "";
    const char *pos_raw = (const char *)sqlite3_column_text(stmt, 6);
    if (pos_raw && pos_raw[0] != '\0')
        posiciones_to_text(pos_raw, pos_etiquetas, sizeof(pos_etiquetas));
    else
        snprintf(pos_etiquetas, sizeof(pos_etiquetas), "SIN JUGADORES");

    fprintf(f, "ID: %d\n"
            "  Nombre: %s\n"
            "  Tipo: %s\n"
            "  Tipo de Futbol: %s\n"
            "  Partido ID: %d\n"
            "  Numero de Jugadores: %d\n"
            "  Posiciones: %s\n\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            tipo_equipo_to_text(sqlite3_column_int(stmt, 2)),
            tipo_futbol_to_text(sqlite3_column_int(stmt, 3)),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            pos_etiquetas);
}

static void write_json_row(FILE *f, sqlite3_stmt *stmt, void *context) /* NOSONAR */
{
    (void)f;
    cJSON *root = (cJSON *)context;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "tipo", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "tipo_futbol", sqlite3_column_int(stmt, 3));
    cJSON_AddNumberToObject(item, "partido_id", sqlite3_column_int(stmt, 4));
    cJSON_AddNumberToObject(item, "num_jugadores", sqlite3_column_int(stmt, 5));
    cJSON_AddStringToObject(item, "posiciones", (const char *)sqlite3_column_text(stmt, 6));
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
            "<html><body><h1>Equipos</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Tipo</th><th>Tipo de Futbol</th>"
            "<th>Numero de Jugadores</th><th>Posiciones</th></tr>");
}

static void write_html_row(FILE *f, sqlite3_stmt *stmt, void *context)
{
    (void)context;
    char pos_etiquetas[256] = "";
    const char *pos_raw = (const char *)sqlite3_column_text(stmt, 6);
    if (pos_raw && pos_raw[0] != '\0')
        posiciones_to_text(pos_raw, pos_etiquetas, sizeof(pos_etiquetas));
    else
        snprintf(pos_etiquetas, sizeof(pos_etiquetas), "SIN JUGADORES");

    fprintf(f,
            "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            tipo_equipo_to_text(sqlite3_column_int(stmt, 2)),
            tipo_futbol_to_text(sqlite3_column_int(stmt, 3)),
            sqlite3_column_int(stmt, 5),
            pos_etiquetas);
}

static void write_html_footer(FILE *f, void *context)
{
    (void)context;
    fprintf(f, "</table></body></html>");
}

/** @} */

/** @name Funciones de exportacion de equipos */
/** @{ */

void exportar_equipos_csv()
{
    ExportConfig config =
    {
        .filename = "equipos.csv",
        .context = NULL,
        .write_header = write_csv_header,
        .write_row = write_csv_row,
        .write_footer = NULL
    };
    int count;
    sqlite3_stmt *stmt = obtener_datos_equipos(&count);
    if (!stmt) return;
    export_generic_rows(&config, stmt);
}

void exportar_equipos_txt()
{
    ExportConfig config =
    {
        .filename = "equipos.txt",
        .context = NULL,
        .write_header = write_txt_header,
        .write_row = write_txt_row,
        .write_footer = NULL
    };
    int count;
    sqlite3_stmt *stmt = obtener_datos_equipos(&count);
    if (!stmt) return;
    export_generic_rows(&config, stmt);
}

void exportar_equipos_json()
{
    cJSON *root = cJSON_CreateArray();
    ExportConfig config =
    {
        .filename = "equipos.json",
        .context = root,
        .write_header = NULL,
        .write_row = write_json_row,
        .write_footer = write_json_footer
    };
    int count;
    sqlite3_stmt *stmt = obtener_datos_equipos(&count);
    if (!stmt) return;
    export_generic_rows(&config, stmt);
}

void exportar_equipos_html()
{
    ExportConfig config =
    {
        .filename = "equipos.html",
        .context = NULL,
        .write_header = write_html_header,
        .write_row = write_html_row,
        .write_footer = write_html_footer
    };
    int count;
    sqlite3_stmt *stmt = obtener_datos_equipos(&count);
    if (!stmt) return;
    export_generic_rows(&config, stmt);
}

/** @} */
