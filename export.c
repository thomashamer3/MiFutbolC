#include "export.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>

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

    fprintf(f, "[\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM camiseta", -1, &stmt, NULL);

    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(f, ",\n");
        fprintf(f,
                "  { \"id\": %d, \"nombre\": \"%s\" }",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1));
        first = 0;
    }

    fprintf(f, "\n]");
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

    fprintf(f, "[\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, jugador, tipo, descripcion, fecha FROM lesion", -1, &stmt, NULL);

    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(f, ",\n");
        fprintf(f,
                "  { \"id\": %d, \"jugador\": \"%s\", \"tipo\": \"%s\", \"descripcion\": \"%s\", \"fecha\": \"%s\" }",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
        first = 0;
    }

    fprintf(f, "\n]");
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
 * incluyendo cancha, fecha, goles, asistencias y camiseta. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_csv()
{
    FILE *f = fopen(EXPORT_PATH "/partidos.csv", "w");
    if (!f)
        return;

    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.cancha,p.fecha_hora,p.goles,p.asistencias,c.nombre "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s,%s,%d,%d,%s\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta los partidos a un archivo de texto plano
 *
 * Crea un archivo de texto con un listado formateado de todos los partidos
 * registrados, incluyendo cancha, fecha, goles, asistencias y camiseta. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_txt()
{
    FILE *f = fopen(EXPORT_PATH "/partidos.txt", "w");
    if (!f)
        return;

    fprintf(f, "LISTADO DE PARTIDOS\n\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.cancha,p.fecha_hora,p.goles,p.asistencias,c.nombre "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f, "%s | %s | G:%d A:%d | %s\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4));

    sqlite3_finalize(stmt);
    fclose(f);
}

/**
 * @brief Exporta los partidos a un archivo JSON
 *
 * Crea un archivo JSON con un array de objetos representando todos los partidos
 * registrados, incluyendo cancha, fecha, goles, asistencias y camiseta. El archivo se guarda en la ruta definida por EXPORT_PATH.
 */
void exportar_partidos_json()
{
    FILE *f = fopen(EXPORT_PATH "/partidos.json", "w");
    if (!f)
        return;

    fprintf(f, "[\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.cancha,p.fecha_hora,p.goles,p.asistencias,c.nombre "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id",
                       -1, &stmt, NULL);

    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(f, ",\n");
        fprintf(f,
                "  { \"cancha\":\"%s\", \"fecha\":\"%s\", \"goles\":%d, \"asistencias\":%d, \"camiseta\":\"%s\" }",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4));
        first = 0;
    }

    fprintf(f, "\n]");
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
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th></tr>");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.cancha,p.fecha_hora,p.goles,p.asistencias,c.nombre "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id",
                       -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        fprintf(f,
                "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td></tr>",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4));

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

    fprintf(f, "[\n");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT c.nombre, SUM(p.goles), SUM(p.asistencias), COUNT(*) "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                       "GROUP BY c.id",
                       -1, &stmt, NULL);

    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            fprintf(f, ",\n");
        fprintf(f,
                "  { \"camiseta\":\"%s\", \"goles\":%d, \"asistencias\":%d, \"partidos\":%d }",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2),
                sqlite3_column_int(stmt, 3));
        first = 0;
    }

    fprintf(f, "\n]");
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
