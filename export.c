/**
 * @file export.c
 * @brief Funciones para exportar datos de la base de datos a diferentes formatos
 *
 * Este archivo contiene funciones para exportar datos de camisetas, lesiones,
 * partidos y estadísticas a formatos CSV, TXT, JSON y HTML.
 */

#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>

/**
 * @brief Estructura para almacenar estadísticas de partidos
 */
typedef struct
{
    double avg_goles;
    double avg_asistencias;
    double avg_rendimiento;
    double avg_cansancio;
    double avg_animo;
    int total_partidos;
} Estadisticas;

/**
 * @brief Construye la ruta completa para un archivo de exportación
 *
 * Combina el directorio de datos con el nombre del archivo proporcionado
 * para crear una ruta completa.
 *
 * @param filename Nombre del archivo a exportar
 * @return Cadena de caracteres con la ruta completa del archivo
 */
static char* get_export_path(const char* filename)
{
    static char path[1024];
    const char* data_dir = get_data_dir();
    strcpy(path, data_dir);
    strcat(path, "\\");
    strcat(path, filename);
    return path;
}

/**
 * @brief Convierte el número de resultado a texto
 *
 * @param resultado Número del resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA)
 * @return Cadena de texto correspondiente al resultado
 */
static const char *resultado_to_text(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "VICTORIA";
    case 2:
        return "EMPATE";
    case 3:
        return "DERROTA";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Convierte el número de clima a texto
 *
 * @param clima Número del clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio)
 * @return Cadena de texto correspondiente al clima
 */
static const char *clima_to_text(int clima)
{
    switch (clima)
    {
    case 1:
        return "Despejado";
    case 2:
        return "Nublado";
    case 3:
        return "Lluvia";
    case 4:
        return "Ventoso";
    case 5:
        return "Mucho Calor";
    case 6:
        return "Mucho Frio";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Convierte el número de dia a texto
 *
 * @param dia Número del dia (1=Dia, 2=Tarde, 3=Noche)
 * @return Cadena de texto correspondiente al dia
 */
static const char *dia_to_text(int dia)
{
    switch (dia)
    {
    case 1:
        return "Dia";
    case 2:
        return "Tarde";
    case 3:
        return "Noche";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Exporta las camisetas a un archivo CSV
 *
 * Crea un archivo CSV con todas las camisetas registradas en la base de datos,
 * incluyendo ID y nombre. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_camisetas_csv()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.csv"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.csv"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.txt"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.txt"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.json"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.json"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de camisetas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("camisetas.html"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("camisetas.html"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.csv"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.csv"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.txt"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.txt"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.json"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.json"));
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
    sqlite3_stmt *check_stmt;
    int count = 0;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM lesion", -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    if (count == 0)
    {
        printf("No hay registros de lesiones para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("lesiones.html"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("lesiones.html"));
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
        fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
                sqlite3_column_text(stmt, 0),
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
        fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
                sqlite3_column_text(stmt, 0),
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
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "cancha", (const char *)sqlite3_column_text(stmt, 0));
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
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                sqlite3_column_text(stmt, 0),
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

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("partidos.html"));
    fclose(f);
}

/* ===================== ESTADISTICAS ===================== */

/**
 * @brief Exporta las estadísticas a un archivo CSV
 *
 * Crea un archivo CSV con las estadísticas agrupadas por camiseta,
 * incluyendo nombre, suma de goles, suma de asistencias, número de partidos, victorias, empates y derrotas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_csv()
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
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Camiseta,Goles,Asistencias,Partidos,Victorias,Empates,Derrotas\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s,%d,%d,%d,%d,%d,%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.csv"));
    fclose(f);
}

/**
 * @brief Exporta las estadísticas a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias y número de partidos. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_txt()
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
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.txt"), "w");
    if (!f)
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s | G:%d A:%d P:%d V:%d E:%d D:%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.txt"));
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
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateArray();

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
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
        cJSON_AddNumberToObject(item, "victorias", sqlite3_column_int(stmt, 4));
        cJSON_AddNumberToObject(item, "empates", sqlite3_column_int(stmt, 5));
        cJSON_AddNumberToObject(item, "derrotas", sqlite3_column_int(stmt, 6));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.json"));
    fclose(f);
}

/**
 * @brief Exporta las estadísticas a un archivo HTML
 *
 * Crea un archivo HTML con una tabla que muestra las estadísticas
 * agrupadas por camiseta, incluyendo nombre, suma de goles, suma de asistencias, número de partidos, victorias, empates y derrotas. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_estadisticas_html()
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
        printf("No hay registros de estadisticas para exportar.\n");
        return;
    }

    FILE *f = fopen(get_export_path("estadisticas.html"), "w");
    if (!f)
        return;

    fprintf(f,
            "<html><body><h1>Estadisticas</h1><table border='1'>"
            "<tr><th>Camiseta</th><th>Goles</th><th>Asistencias</th><th>Partidos</th><th>Victorias</th><th>Empates</th><th>Derrotas</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*), "
                       "SUM(CASE WHEN p.resultado=1 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=2 THEN 1 ELSE 0 END), "
                       "SUM(CASE WHEN p.resultado=3 THEN 1 ELSE 0 END) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6));

    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("estadisticas.html"));
    fclose(f);
}

/* ===================== ANALISIS ===================== */

/**
 * @brief Calcula estadísticas generales de todos los partidos
 *
 * @param stats Puntero a la estructura donde almacenar las estadísticas
 */
static void calcular_estadisticas_generales(Estadisticas *stats)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                       "FROM partido",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->avg_goles = sqlite3_column_double(stmt, 1);
        stats->avg_asistencias = sqlite3_column_double(stmt, 2);
        stats->avg_rendimiento = sqlite3_column_double(stmt, 3);
        stats->avg_cansancio = sqlite3_column_double(stmt, 4);
        stats->avg_animo = sqlite3_column_double(stmt, 5);
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Calcula estadísticas de los últimos 5 partidos
 *
 * @param stats Puntero a la estructura donde almacenar las estadísticas
 */
static void calcular_estadisticas_ultimos5(Estadisticas *stats)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                       "FROM (SELECT * FROM partido ORDER BY fecha_hora DESC LIMIT 5)",
                       -1, &stmt, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->avg_goles = sqlite3_column_double(stmt, 1);
        stats->avg_asistencias = sqlite3_column_double(stmt, 2);
        stats->avg_rendimiento = sqlite3_column_double(stmt, 3);
        stats->avg_cansancio = sqlite3_column_double(stmt, 4);
        stats->avg_animo = sqlite3_column_double(stmt, 5);
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Calcula la racha más larga de victorias y derrotas
 *
 * @param mejor_racha_victorias Puntero donde almacenar la mejor racha de victorias
 * @param peor_racha_derrotas Puntero donde almacenar la peor racha de derrotas
 */
static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT resultado FROM partido ORDER BY fecha_hora",
                       -1, &stmt, NULL);

    int racha_actual_v = 0, max_racha_v = 0;
    int racha_actual_d = 0, max_racha_d = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        if (resultado == 1)
        {
            // VICTORIA
            racha_actual_v++;
            if (racha_actual_v > max_racha_v)
                max_racha_v = racha_actual_v;
            racha_actual_d = 0;
        }
        else if (resultado == 3)
        {
            // DERROTA
            racha_actual_d++;
            if (racha_actual_d > max_racha_d)
                max_racha_d = racha_actual_d;
            racha_actual_v = 0;
        }
        else
        {
            // EMPATE
            racha_actual_v = 0;
            racha_actual_d = 0;
        }
    }

    *mejor_racha_victorias = max_racha_v;
    *peor_racha_derrotas = max_racha_d;
    sqlite3_finalize(stmt);
}

/**
 * @brief Genera un mensaje motivacional basado en el rendimiento
 *
 * @param ultimos Puntero a estadísticas de últimos 5 partidos
 * @param generales Puntero a estadísticas generales
 * @return Cadena de texto con el mensaje motivacional
 */
static const char *mensaje_motivacional(const Estadisticas *ultimos, const Estadisticas *generales)
{
    double diff_goles = ultimos->avg_goles - generales->avg_goles;
    double diff_rendimiento = ultimos->avg_rendimiento - generales->avg_rendimiento;

    if (diff_goles > 0.5 && diff_rendimiento > 0.5)
    {
        return "Excelente. Estas en racha ascendente. Sigue asi, tu esfuerzo esta dando frutos. Mantien la consistencia y continua trabajando duro en los entrenamientos.";
    }
    else if (diff_goles < -0.5 || diff_rendimiento < -0.5)
    {
        return "No te desanimes. Todos tenemos dias dificiles. Analiza que puedes mejorar: Revisa tu preparacion fisica y tecnica. Habla con tu entrenador sobre estrategias. Recuerda: el futbol es un deporte de perseverancia.";
    }
    else
    {
        return "Buen trabajo manteniendo el nivel. La consistencia es clave en el futbol. Sigue entrenando y manten la motivacion alta. Cada partido es una oportunidad!";
    }
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo CSV
 *
 * Crea un archivo CSV con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_csv()
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
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.csv"), "w");
    if (!f)
        return;

    fprintf(f, "Tipo,Promedio_Goles,Promedio_Asistencias,Promedio_Rendimiento,Promedio_Cansancio,Promedio_Animo,Total_Partidos\n");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    fprintf(f, "Generales,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
            generales.avg_goles, generales.avg_asistencias, generales.avg_rendimiento,
            generales.avg_cansancio, generales.avg_animo, generales.total_partidos);

    fprintf(f, "Ultimos5,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
            ultimos5.avg_goles, ultimos5.avg_asistencias, ultimos5.avg_rendimiento,
            ultimos5.avg_cansancio, ultimos5.avg_animo, ultimos5.total_partidos);

    fprintf(f, "Rachas,%d,%d\n", mejor_racha_v, peor_racha_d);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "Mensaje,%s\n", msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.csv"));
    fclose(f);
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo de texto plano
 *
 * Crea un archivo de texto con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_txt()
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
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.txt"), "w");
    if (!f)
        return;

    fprintf(f, "ANALISIS DE RENDIMIENTO\n\n");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    fprintf(f, "ESTADISTICAS GENERALES:\n");
    fprintf(f, "Total Partidos: %d\n", generales.total_partidos);
    fprintf(f, "Promedio Goles: %.2f\n", generales.avg_goles);
    fprintf(f, "Promedio Asistencias: %.2f\n", generales.avg_asistencias);
    fprintf(f, "Promedio Rendimiento: %.2f\n", generales.avg_rendimiento);
    fprintf(f, "Promedio Cansancio: %.2f\n", generales.avg_cansancio);
    fprintf(f, "Promedio Estado Animo: %.2f\n\n", generales.avg_animo);

    fprintf(f, "ULTIMOS 5 PARTIDOS:\n");
    fprintf(f, "Total Partidos: %d\n", ultimos5.total_partidos);
    fprintf(f, "Promedio Goles: %.2f\n", ultimos5.avg_goles);
    fprintf(f, "Promedio Asistencias: %.2f\n", ultimos5.avg_asistencias);
    fprintf(f, "Promedio Rendimiento: %.2f\n", ultimos5.avg_rendimiento);
    fprintf(f, "Promedio Cansancio: %.2f\n", ultimos5.avg_cansancio);
    fprintf(f, "Promedio Estado Animo: %.2f\n\n", ultimos5.avg_animo);

    fprintf(f, "RACHAS:\n");
    fprintf(f, "Mejor racha de victorias: %d partidos\n", mejor_racha_v);
    fprintf(f, "Peor racha de derrotas: %d partidos\n\n", peor_racha_d);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "ANALISIS MOTIVACIONAL:\n%s\n", msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.txt"));
    fclose(f);
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo JSON
 *
 * Crea un archivo JSON con un objeto conteniendo todas las estadísticas
 * del análisis de rendimiento. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_json()
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
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.json"), "w");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    cJSON *generales_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(generales_obj, "total_partidos", generales.total_partidos);
    cJSON_AddNumberToObject(generales_obj, "avg_goles", generales.avg_goles);
    cJSON_AddNumberToObject(generales_obj, "avg_asistencias", generales.avg_asistencias);
    cJSON_AddNumberToObject(generales_obj, "avg_rendimiento", generales.avg_rendimiento);
    cJSON_AddNumberToObject(generales_obj, "avg_cansancio", generales.avg_cansancio);
    cJSON_AddNumberToObject(generales_obj, "avg_animo", generales.avg_animo);
    cJSON_AddItemToObject(root, "generales", generales_obj);

    cJSON *ultimos5_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(ultimos5_obj, "total_partidos", ultimos5.total_partidos);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_goles", ultimos5.avg_goles);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_asistencias", ultimos5.avg_asistencias);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_rendimiento", ultimos5.avg_rendimiento);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_cansancio", ultimos5.avg_cansancio);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_animo", ultimos5.avg_animo);
    cJSON_AddItemToObject(root, "ultimos5", ultimos5_obj);

    cJSON *rachas_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rachas_obj, "mejor_racha_victorias", mejor_racha_v);
    cJSON_AddNumberToObject(rachas_obj, "peor_racha_derrotas", peor_racha_d);
    cJSON_AddItemToObject(root, "rachas", rachas_obj);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    cJSON_AddStringToObject(root, "mensaje_motivacional", msg);

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    printf("Archivo exportado a: %s\n", get_export_path("analisis.json"));
    fclose(f);
}

/**
 * @brief Exporta el análisis de rendimiento a un archivo HTML
 *
 * Crea una página HTML con las estadísticas presentadas en formato web.
 * El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_analisis_html()
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
        printf("No hay registros de partidos para exportar analisis.\n");
        return;
    }

    FILE *f = fopen(get_export_path("analisis.html"), "w");
    if (!f)
        return;

    fprintf(f, "<html><body><h1>Analisis de Rendimiento</h1>");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v, peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    fprintf(f, "<h2>Estadisticas Generales</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Total Partidos</th><td>%d</td></tr>", generales.total_partidos);
    fprintf(f, "<tr><th>Promedio Goles</th><td>%.2f</td></tr>", generales.avg_goles);
    fprintf(f, "<tr><th>Promedio Asistencias</th><td>%.2f</td></tr>", generales.avg_asistencias);
    fprintf(f, "<tr><th>Promedio Rendimiento</th><td>%.2f</td></tr>", generales.avg_rendimiento);
    fprintf(f, "<tr><th>Promedio Cansancio</th><td>%.2f</td></tr>", generales.avg_cansancio);
    fprintf(f, "<tr><th>Promedio Estado Animo</th><td>%.2f</td></tr>", generales.avg_animo);
    fprintf(f, "</table>");

    fprintf(f, "<h2>Ultimos 5 Partidos</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Total Partidos</th><td>%d</td></tr>", ultimos5.total_partidos);
    fprintf(f, "<tr><th>Promedio Goles</th><td>%.2f</td></tr>", ultimos5.avg_goles);
    fprintf(f, "<tr><th>Promedio Asistencias</th><td>%.2f</td></tr>", ultimos5.avg_asistencias);
    fprintf(f, "<tr><th>Promedio Rendimiento</th><td>%.2f</td></tr>", ultimos5.avg_rendimiento);
    fprintf(f, "<tr><th>Promedio Cansancio</th><td>%.2f</td></tr>", ultimos5.avg_cansancio);
    fprintf(f, "<tr><th>Promedio Estado Animo</th><td>%.2f</td></tr>", ultimos5.avg_animo);
    fprintf(f, "</table>");

    fprintf(f, "<h2>Rachas</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Mejor Racha Victorias</th><td>%d partidos</td></tr>", mejor_racha_v);
    fprintf(f, "<tr><th>Peor Racha Derrotas</th><td>%d partidos</td></tr>", peor_racha_d);
    fprintf(f, "</table>");

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "<h2>Analisis Motivacional</h2>");
    fprintf(f, "<p>%s</p>", msg);

    fprintf(f, "</body></html>");
    printf("Archivo exportado a: %s\n", get_export_path("analisis.html"));
    fclose(f);
}

