#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>
/* ===================== PARTIDOS ===================== */

/**
 * @brief Exporta los partidos a un archivo CSV
 *
 * Crea un archivo CSV con todos los partidos registrados en la base de datos,
 * incluyendo cancha, fecha, goles, asistencias, camiseta y otros campos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.csv"), "w");
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
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partidos.csv"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.txt"), "w");
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
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partidos.txt"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.json"), "w");
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
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(item, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(item, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(item, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(item, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(item, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(item, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(item, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        cJSON_AddItemToArray(root, item);
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partidos.json"));
    fclose(f);
}

/**
 * @brief Exporta los partidos a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra todos los partidos
 * registrados, incluyendo cancha, fecha, goles, asistencias, camiseta y otros campos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partidos.html"), "w");
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
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partidos.html"));
    fclose(f);
}

/* ===================== PARTIDOS ESPECIFICOS ===================== */

/**
 * @brief Exporta el partido con más goles a un archivo CSV
 */
void exportar_partido_mas_goles_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más goles a un archivo de texto plano
 */
void exportar_partido_mas_goles_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO CON MAS GOLES\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más goles a un archivo JSON
 */
void exportar_partido_mas_goles_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más goles a un archivo HTML
 */
void exportar_partido_mas_goles_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_goles.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido con Mas Goles</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_goles.html"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo CSV
 */
void exportar_partido_mas_asistencias_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo de texto plano
 */
void exportar_partido_mas_asistencias_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO CON MAS ASISTENCIAS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo JSON
 */
void exportar_partido_mas_asistencias_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido con más asistencias a un archivo HTML
 */
void exportar_partido_mas_asistencias_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_mas_asistencias.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido con Mas Asistencias</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_mas_asistencias.html"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo CSV
 */
void exportar_partido_menos_goles_reciente_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo de texto plano
 */
void exportar_partido_menos_goles_reciente_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO MAS RECIENTE CON MENOS GOLES\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo JSON
 */
void exportar_partido_menos_goles_reciente_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos goles a un archivo HTML
 */
void exportar_partido_menos_goles_reciente_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_goles_reciente.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido Mas Reciente con Menos Goles</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_goles_reciente.html"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo CSV
 */
void exportar_partido_menos_asistencias_reciente_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.csv"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo de texto plano
 */
void exportar_partido_menos_asistencias_reciente_txt()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.txt"), "w");
    if (!f)
        return;

    fprintf(f, "PARTIDO MAS RECIENTE CON MENOS ASISTENCIAS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.txt"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo JSON
 */
void exportar_partido_menos_asistencias_reciente_json()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        cJSON_AddStringToObject(root, "cancha", cancha_trimmed);
        cJSON_AddStringToObject(root, "fecha", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(root, "goles", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(root, "asistencias", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(root, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(root, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
        cJSON_AddStringToObject(root, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
        cJSON_AddStringToObject(root, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
        cJSON_AddNumberToObject(root, "rendimiento_general", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "cansancio", sqlite3_column_int(stmt, 9));
        cJSON_AddNumberToObject(root, "estado_animo", sqlite3_column_int(stmt, 10));
        cJSON_AddStringToObject(root, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.json"));
    fclose(f);
}

/**
 * @brief Exporta el partido más reciente con menos asistencias a un archivo HTML
 */
void exportar_partido_menos_asistencias_reciente_html()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de partidos para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("partido_menos_asistencias_reciente.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Partido Mas Reciente con Menos Asistencias</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
        trim_trailing_spaces(cancha_trimmed);
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                cancha_trimmed,
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4),
                resultado_to_text(sqlite3_column_int(stmt, 5)),
                clima_to_text(sqlite3_column_int(stmt, 6)),
                dia_to_text(sqlite3_column_int(stmt, 7)),
                sqlite3_column_int(stmt, 8),
                sqlite3_column_int(stmt, 9),
                sqlite3_column_int(stmt, 10),
                sqlite3_column_text(stmt, 11));
        free(cancha_trimmed);
    }

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partido_menos_asistencias_reciente.html"));
    fclose(f);
}
