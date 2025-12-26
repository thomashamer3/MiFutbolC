#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define EXPORT_PATH "data"

/**
 * @brief Exporta las camisetas a un archivo CSV
 *
 * Crea un archivo CSV con todas las camisetas registradas en la base de datos,
 * incluyendo ID y nombre. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_csv()
{
    FILE *f = fopen(EXPORT_PATH "/camisetas.csv", "w");
    if (!f)
        return;

    fprintf(f, "id,nombre\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM camiseta", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%d,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta las camisetas a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de todas las camisetas
 * registradas, incluyendo ID y nombre. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_txt()
{
    FILE *f = fopen(EXPORT_PATH "/camisetas.txt", "w");
    if (!f)
        return;

    fprintf(f, "LISTADO DE CAMISETAS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM camiseta", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%d - %s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta las camisetas a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando todas las camisetas
 * registradas, incluyendo ID y nombre. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_json()
{
    FILE *f = fopen(EXPORT_PATH "/camisetas.json", "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateArray();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM camiseta", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(item, "nombre", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta las camisetas a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra todas las camisetas
 * registradas, incluyendo ID y nombre. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_html()
{
    FILE *f = fopen(EXPORT_PATH "/camisetas.html", "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Camisetas</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM camiseta", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%d</td><td>%s</td></tr>",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1));

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    fclose(f);
}

/* ===================== LESIONES ===================== */

/**
 * @brief Exporta las lesiones a un archivo CSV
 *
 * Crea un archivo CSV con todas las lesiones registradas en la base de datos,
 * incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_csv()
{
    FILE *f = fopen(EXPORT_PATH "/lesiones.csv", "w");
    if (!f)
        return;

    fprintf(f, "id,jugador,tipo,descripcion,fecha\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, jugador, tipo, descripcion, fecha FROM lesion", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%d,%s,%s,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta las lesiones a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de todas las lesiones
 * registradas, incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_txt()
{
    FILE *f = fopen(EXPORT_PATH "/lesiones.txt", "w");
    if (!f)
        return;

    fprintf(f, "LISTADO DE LESIONES\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, jugador, tipo, descripcion, fecha FROM lesion", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%d - %s | %s | %s | %s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta las lesiones a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando todas las lesiones
 * registradas, incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_json()
{
    FILE *f = fopen(EXPORT_PATH "/lesiones.json", "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateArray();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, jugador, tipo, descripcion, fecha FROM lesion", -1, &stmt, NULL);

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
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta las lesiones a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra todas las lesiones
 * registradas, incluyendo ID, jugador, tipo, descripción y fecha. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_lesiones_html()
{
    FILE *f = fopen(EXPORT_PATH "/lesiones.html", "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Lesiones</h1><table border='1'>"
            "<tr><th>ID</th><th>Jugador</th><th>Tipo</th><th>Descripción</th><th>Fecha</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, jugador, tipo, descripcion, fecha FROM lesion", -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    fclose(f);
}

/* ===================== PARTIDOS ===================== */

/**
 * @brief Exporta los partidos a un archivo CSV
 *
 * Crea un archivo CSV con todos los partidos registrados en la base de datos,
 * incluyendo cancha, fecha, goles, asistencias, camiseta y otros campos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_csv()
{
    FILE *f = fopen(EXPORT_PATH "/partidos.csv", "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s,%s,%d,%d,%s,%d,%d,%d,%d,%d,%d,%s\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta los partidos a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de todos los partidos
 * registrados, incluyendo cancha, fecha, goles, asistencias, camiseta y otros campos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_txt()
{
    FILE *f = fopen(EXPORT_PATH "/partidos.txt", "w");
    if (!f)
        return;

    fprintf(f, "LISTADO DE PARTIDOS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%d Cli:%d Dia:%d RG:%d Can:%d EA:%d | %s\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta los partidos a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando todos los partidos
 * registrados, incluyendo cancha, fecha, goles, asistencias, camiseta y otros campos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_json()
{
    FILE *f = fopen(EXPORT_PATH "/partidos.json", "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateArray();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "cancha", (const char *)sqlite3_column_text(stmt, 0));
        cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddNumberToObject(item, "resultado", sqlite3_column_int(stmt, 5));
        cJSON_AddNumberToObject(item, "clima", sqlite3_column_int(stmt, 6));
        cJSON_AddNumberToObject(item, "dia", sqlite3_column_int(stmt, 7));
        cJSON_AddNumberToObject(item, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(item, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(item, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(item, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    fclose(f);
}

void exportar_partidos_html()
{
    FILE *f = fopen(EXPORT_PATH "/partidos.html", "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partidos</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    fclose(f);
}

/* ===================== ESTADISTICAS ===================== */

/**
 * @brief Exporta las estadísticas a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias y número de partidos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_txt()
{
    FILE *f = fopen(EXPORT_PATH "/estadisticas.txt", "w");
    if (!f)
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s | G:%d A:%d P:%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta las estadísticas a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias y número de partidos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_json()
{
    FILE *f = fopen(EXPORT_PATH "/estadisticas.json", "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateArray();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 0));
        cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 1));
        cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(item, "partidos", sqlite3_column_int(stmt, 3));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    fclose(f);
}

void exportar_estadisticas_html()
{
    FILE *f = fopen(EXPORT_PATH "/estadisticas.html", "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Estadisticas</h1><table border='1'>"
            "<tr><th>Camiseta</th><th>Goles</th><th>Asistencias</th><th>Partidos</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td></tr>",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3));

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    fclose(f);
}
