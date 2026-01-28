#include "qr.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @file qr.c
 * @brief Implementación del sistema de codigos QR para MiFutbolC
 *
 * Este módulo permite generar codigos QR con INFORMACION de partidos,
 * estadisticas, temporadas y camisetas para compartir fácilmente.
 */

// ========== FUNCIONES DE GENERACIÓN DE JSON ==========

char* obtener_estadisticas_partido_json(int partido_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT p.id, p.fecha_hora, p.goles, p.asistencias, p.rendimiento_general, "
                      "p.cansancio, p.estado_animo, p.clima, p.dia, p.comentario_personal, "
                      "c.nombre as cancha, cam.nombre as camiseta, "
                      "CASE WHEN p.resultado = 1 THEN 'VICTORIA' "
                      "     WHEN p.resultado = 2 THEN 'EMPATE' "
                      "     WHEN p.resultado = 3 THEN 'DERROTA' END as resultado "
                      "FROM partido p "
                      "JOIN cancha c ON p.cancha_id = c.id "
                      "JOIN camiseta cam ON p.camiseta_id = cam.id "
                      "WHERE p.id = ?;";

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "partido_estadisticas");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, partido_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            cJSON_AddNumberToObject(json, "partido_id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(json, "fecha_hora", (const char*)sqlite3_column_text(stmt, 1));
            cJSON_AddNumberToObject(json, "goles", sqlite3_column_int(stmt, 2));
            cJSON_AddNumberToObject(json, "asistencias", sqlite3_column_int(stmt, 3));
            cJSON_AddNumberToObject(json, "rendimiento_general", sqlite3_column_int(stmt, 4));
            cJSON_AddNumberToObject(json, "cansancio", sqlite3_column_int(stmt, 5));
            cJSON_AddNumberToObject(json, "estado_animo", sqlite3_column_int(stmt, 6));
            cJSON_AddNumberToObject(json, "clima", sqlite3_column_int(stmt, 7));
            cJSON_AddNumberToObject(json, "dia", sqlite3_column_int(stmt, 8));
            cJSON_AddStringToObject(json, "comentario", (const char*)sqlite3_column_text(stmt, 9));
            cJSON_AddStringToObject(json, "cancha", (const char*)sqlite3_column_text(stmt, 10));
            cJSON_AddStringToObject(json, "camiseta", (const char*)sqlite3_column_text(stmt, 11));
            cJSON_AddStringToObject(json, "resultado", (const char*)sqlite3_column_text(stmt, 12));
        }
        sqlite3_finalize(stmt);
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

char* obtener_estadisticas_jugador_json(int partido_id, int jugador_id)
{
    // Esta función se implementaría cuando haya estadisticas detalladas por jugador
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "jugador_partido_estadisticas");
    cJSON_AddNumberToObject(json, "partido_id", partido_id);
    cJSON_AddNumberToObject(json, "jugador_id", jugador_id);
    cJSON_AddStringToObject(json, "mensaje", "Funcionalidad de estadisticas por jugador próximamente");

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

char* obtener_resumen_temporada_json(int temporada_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT t.nombre, t.anio, tr.total_partidos, tr.total_goles, "
                      "tr.promedio_goles_partido, tr.total_lesiones, e.nombre as campeon, "
                      "j.nombre as goleador, tr.mejor_goleador_goles "
                      "FROM temporada t "
                      "LEFT JOIN temporada_resumen tr ON t.id = tr.temporada_id "
                      "LEFT JOIN equipo e ON tr.equipo_campeon_id = e.id "
                      "LEFT JOIN jugador j ON tr.mejor_goleador_jugador_id = j.id "
                      "WHERE t.id = ?;";

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "temporada_resumen");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            cJSON_AddNumberToObject(json, "temporada_id", temporada_id);
            cJSON_AddStringToObject(json, "nombre", (const char*)sqlite3_column_text(stmt, 0));
            cJSON_AddNumberToObject(json, "anio", sqlite3_column_int(stmt, 1));
            cJSON_AddNumberToObject(json, "total_partidos", sqlite3_column_int(stmt, 2));
            cJSON_AddNumberToObject(json, "total_goles", sqlite3_column_int(stmt, 3));
            cJSON_AddNumberToObject(json, "promedio_goles_partido", sqlite3_column_double(stmt, 4));
            cJSON_AddNumberToObject(json, "total_lesiones", sqlite3_column_int(stmt, 5));
            cJSON_AddStringToObject(json, "equipo_campeon", (const char*)sqlite3_column_text(stmt, 6));
            cJSON_AddStringToObject(json, "mejor_goleador", (const char*)sqlite3_column_text(stmt, 7));
            cJSON_AddNumberToObject(json, "goles_mejor_goleador", sqlite3_column_int(stmt, 8));
        }
        sqlite3_finalize(stmt);
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

char* obtener_info_camiseta_json(int camiseta_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre, sorteada FROM camiseta WHERE id = ?;";

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "tipo", "camiseta_info");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, camiseta_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            cJSON_AddNumberToObject(json, "camiseta_id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(json, "nombre", (const char*)sqlite3_column_text(stmt, 1));
            cJSON_AddNumberToObject(json, "sorteada", sqlite3_column_int(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

// ========== FUNCIONES DE GENERACIÓN DE QR ==========

int generar_qr_png(const char* texto, const char* filename)
{
    // POR IMPLEMENTAR: Requiere librería externa libqrencode
    // Por ahora simulamos la generación guardando el texto en un archivo

    const char *export_dir = get_export_dir();
    char filepath[500];
    snprintf(filepath, sizeof(filepath), "%s\\%s_qr_data.txt", export_dir, filename);

    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        printf("Error al crear archivo de datos QR.\n");
        return 0;
    }

    fprintf(file, "QR CODE DATA\n");
    fprintf(file, "============\n\n");
    fprintf(file, "Contenido del QR:\n%s\n\n", texto);
    fprintf(file, "Para generar el código QR real, instala libqrencode:\n");
    fprintf(file, "sudo apt-get install libqrencode-dev  (Linux)\n");
    fprintf(file, "brew install qrencode              (macOS)\n");
    fprintf(file, "vcpkg install qrencode             (Windows)\n\n");
    fprintf(file, "Comando ejemplo: qrencode -o %s.png \"%s\"\n", filename, texto);

    fclose(file);

    printf("Datos QR guardados en: %s\n", filepath);
    printf("Nota: Para generar imagen PNG real, instala libqrencode\n");

    return 1;
}

int generar_qr_partido(int partido_id)
{
    if (!existe_id("partido", partido_id))
    {
        printf("El partido con ID %d no existe.\n", partido_id);
        return 0;
    }

    char* json_data = obtener_estadisticas_partido_json(partido_id);
    if (!json_data)
    {
        printf("Error al obtener datos del partido.\n");
        return 0;
    }

    char filename[100];
    snprintf(filename, sizeof(filename), "partido_%d", partido_id);

    int result = generar_qr_png(json_data, filename);

    free(json_data);

    if (result)
    {
        printf("✅ Código QR generado para estadisticas del partido %d\n", partido_id);
    }

    return result;
}

int generar_qr_jugador_partido(int partido_id, int jugador_id)
{
    if (!existe_id("partido", partido_id))
    {
        printf("El partido con ID %d ", partido_id);
        mostrar_no_existe("no existe");
        return 0;
    }

    char* json_data = obtener_estadisticas_jugador_json(partido_id, jugador_id);
    if (!json_data)
    {
        printf("Error al obtener datos del jugador en el partido.\n");
        return 0;
    }

    char filename[100];
    snprintf(filename, sizeof(filename), "jugador_%d_partido_%d", jugador_id, partido_id);

    int result = generar_qr_png(json_data, filename);

    free(json_data);

    if (result)
    {
        printf("✅ Código QR generado para estadisticas del jugador %d en partido %d\n", jugador_id, partido_id);
    }

    return result;
}

int generar_qr_temporada(int temporada_id)
{
    if (!existe_id("temporada", temporada_id))
    {
        printf("La temporada con ID %d ", temporada_id);
        mostrar_no_existe("no existe");
        return 0;
    }

    char* json_data = obtener_resumen_temporada_json(temporada_id);
    if (!json_data)
    {
        printf("Error al obtener datos de la temporada.\n");
        return 0;
    }

    char filename[100];
    snprintf(filename, sizeof(filename), "temporada_%d", temporada_id);

    int result = generar_qr_png(json_data, filename);

    free(json_data);

    if (result)
    {
        printf("✅ Código QR generado para resumen de temporada %d\n", temporada_id);
    }

    return result;
}

int generar_qr_camiseta(int camiseta_id)
{
    if (!existe_id("camiseta", camiseta_id))
    {
        printf("La camiseta con ID %d ", camiseta_id);
        mostrar_no_existe("no existe");
        return 0;
    }

    char* json_data = obtener_info_camiseta_json(camiseta_id);
    if (!json_data)
    {
        printf("Error al obtener datos de la camiseta.\n");
        return 0;
    }

    char filename[100];
    snprintf(filename, sizeof(filename), "camiseta_%d", camiseta_id);

    int result = generar_qr_png(json_data, filename);

    free(json_data);

    if (result)
    {
        printf("✅ Código QR generado para INFORMACION de camiseta %d\n", camiseta_id);
    }

    return result;
}

// ========== MENÚ PRINCIPAL ==========

// Funciones auxiliares para el menú
static void qr_partido()
{
    printf("\nIngrese el ID del partido: ");
    int partido_id = input_int("");
    generar_qr_partido(partido_id);
    pause_console();
}

static void qr_jugador_partido()
{
    printf("\nIngrese el ID del partido: ");
    int partido_id = input_int("");
    printf("Ingrese el ID del jugador: ");
    int jugador_id = input_int("");
    generar_qr_jugador_partido(partido_id, jugador_id);
    pause_console();
}

static void qr_temporada()
{
    printf("\nIngrese el ID de la temporada: ");
    int temporada_id = input_int("");
    generar_qr_temporada(temporada_id);
    pause_console();
}

static void qr_camiseta()
{
    printf("\nIngrese el ID de la camiseta: ");
    int camiseta_id = input_int("");
    generar_qr_camiseta(camiseta_id);
    pause_console();
}

void menu_qr()
{
    clear_screen();
    print_header("CODIGOS QR - COMPARTIR INFORMACION");

    printf("\nGenera codigos QR para compartir INFORMACION de MiFutbolC\n");
    printf("Los codigos QR contienen datos JSON que pueden ser escaneados\n");
    printf("por otras aplicaciones para importar estadisticas.\n\n");

    MenuItem items[] =
    {
        {1, "QR de Estadisticas de Partido", qr_partido},
        {2, "QR de Jugador en Partido", qr_jugador_partido},
        {3, "QR de Resumen de Temporada", qr_temporada},
        {4, "QR de INFORMACION de Camiseta", qr_camiseta},
        {0, "Volver al menu principal", NULL}
    };

    ejecutar_menu("CODIGOS QR", items, 5);
}

// Función auxiliar para ejecutar la opción seleccionada del menú QR
void procesar_opcion_qr(int opcion)
{
    switch (opcion)
    {
    case 1:   // QR de partido
    {
        printf("\nIngrese el ID del partido: ");
        int partido_id = input_int("");
        generar_qr_partido(partido_id);
        break;
    }
    case 2:   // QR de jugador en partido
    {
        printf("\nIngrese el ID del partido: ");
        int partido_id = input_int("");
        printf("Ingrese el ID del jugador: ");
        int jugador_id = input_int("");
        generar_qr_jugador_partido(partido_id, jugador_id);
        break;
    }
    case 3:   // QR de temporada
    {
        printf("\nIngrese el ID de la temporada: ");
        int temporada_id = input_int("");
        generar_qr_temporada(temporada_id);
        break;
    }
    case 4:   // QR de camiseta
    {
        printf("\nIngrese el ID de la camiseta: ");
        int camiseta_id = input_int("");
        generar_qr_camiseta(camiseta_id);
        break;
    }
    default:
    {
        printf("Opción no válida.\n");
        break;
    }
    }

    if (opcion >= 1 && opcion <= 4)
    {
        printf("\nDesea generar otro código QR? (s/n): ");
        int respuesta = getchar();
        while (getchar() != '\n'); // Limpiar buffer

        if (respuesta == 's' || respuesta == 'S')
        {
            menu_qr();
        }
    }
}
