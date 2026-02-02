#include "qr.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <qrencode.h>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @file qr.c
 * @brief Implementación del sistema de códigos QR para MiFutbolC
 *
 * Este módulo permite generar códigos QR con información de partidos,
 * estadísticas, temporadas y camisetas para compartir fácilmente.
 *
 * Utiliza la librería libqrencode para generar códigos QR de alta calidad
 * que se guardan como imágenes BMP. Los datos se codifican en formato JSON
 * para facilitar su interpretación por otras aplicaciones.
 *
 * Características:
 * - Generación de códigos QR con nivel de corrección H (30% de error)
 * - Imágenes en formato BMP de 24 bits
 * - Escala configurable (10x10 píxeles por módulo)
 * - Margen de seguridad de 4 módulos
 *
 * @note Requiere libqrencode instalada en el sistema
 */

// ========== FUNCIONES DE GENERACIÓN DE JSON ==========

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

typedef char *(*QrJsonFn)(int id);

static void construir_nombre_archivo(const char *filename_fmt, int id, char *out, size_t out_size)
{
    if (strcmp(filename_fmt, "partido_%d") == 0)
    {
        snprintf(out, out_size, "partido_%d", id);
    }
    else if (strcmp(filename_fmt, "temporada_%d") == 0)
    {
        snprintf(out, out_size, "temporada_%d", id);
    }
    else if (strcmp(filename_fmt, "camiseta_%d") == 0)
    {
        snprintf(out, out_size, "camiseta_%d", id);
    }
    else
    {
        snprintf(out, out_size, "qr_%d", id);
    }
}

static void imprimir_mensaje_exito(const char *success_fmt, int id)
{
    if (strcmp(success_fmt, "Codigo QR generado para estadisticas del partido %d\n") == 0)
    {
        printf("Codigo QR generado para estadísticas del partido %d\n", id);
    }
    else if (strcmp(success_fmt, "Codigo QR generado para resumen de temporada %d\n") == 0)
    {
        printf("Codigo QR generado para resumen de temporada %d\n", id);
    }
    else if (strcmp(success_fmt, "Codigo QR generado para INFORMACION de camiseta %d\n") == 0)
    {
        printf("Codigo QR generado para información de camiseta %d\n", id);
    }
    else
    {
        printf("Codigo QR generado (ID %d)\n", id);
    }
}

static int generar_qr_desde_json(const char *tabla, int id, const char *filename_fmt,
                                 const char *success_fmt, QrJsonFn generar_json)
{
    if (!existe_id(tabla, id))
    {
        printf("El %s con ID %d ", tabla, id);
        mostrar_no_existe("no existe");
        return 0;
    }

    char *json_data = generar_json(id);
    if (!json_data)
    {
        printf("Error al obtener datos del %s.\n", tabla);
        return 0;
    }

    char filename[100];
    construir_nombre_archivo(filename_fmt, id, filename, sizeof(filename));

    int result = generar_qr_png(json_data, filename);
    free(json_data);

    if (result)
    {
        imprimir_mensaje_exito(success_fmt, id);
    }

    return result;
}

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

    if (preparar_stmt(sql, &stmt))
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

    if (preparar_stmt(sql, &stmt))
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

    if (preparar_stmt(sql, &stmt))
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

/**
 * @brief Estructura para parámetros de generación de QR
 */
typedef struct
{
    int margin;
    int scale;
    int size;
    int img_size;
    QRcode *qrcode;
} QrParams;

/**
 * @brief Determina si una coordenada está en el margen
 */
static int esta_en_margen(int x, int y, const QrParams *params)
{
    return (x < params->margin || x >= params->img_size - params->margin ||
            y < params->margin || y >= params->img_size - params->margin);
}

/**
 * @brief Establece el color de un píxel (blanco o negro)
 */
static void establecer_color_pixel(unsigned char *row_buffer, int x, int es_negro)
{
    unsigned char color = es_negro ? 0 : 255;
    row_buffer[x * 3] = color;     // B
    row_buffer[x * 3 + 1] = color; // G
    row_buffer[x * 3 + 2] = color; // R
}

/**
 * @brief Procesa un píxel del código QR
 */
static void procesar_pixel_qr(unsigned char *row_buffer, int x, int y, const QrParams *params)
{
    // Si está en el margen, píxel blanco
    if (esta_en_margen(x, y, params))
    {
        establecer_color_pixel(row_buffer, x, 0);
        return;
    }

    // Coordenadas en el QR code
    int qr_x = (x - params->margin) / params->scale;
    int qr_y = (y - params->margin) / params->scale;

    if (qr_x >= params->size || qr_y >= params->size)
    {
        establecer_color_pixel(row_buffer, x, 0);
        return;
    }

    // Obtener el módulo del QR (invertir Y porque BMP va de abajo arriba)
    int qr_y_inverted = params->size - 1 - qr_y;
    unsigned char module = params->qrcode->data[qr_y_inverted * params->size + qr_x];

    // Determinar si es píxel negro (bit activo)
    int es_negro = (module & 0x01) != 0;
    establecer_color_pixel(row_buffer, x, es_negro);
}

/**
 * @brief Genera imagen PNG del codigo QR usando libqrencode
 */
int generar_qr_png(const char* texto, const char* filename)
{
    if (!texto || !filename)
    {
        printf("Error: Parámetros inválidos para generar QR.\n");
        return 0;
    }

    // Generar el codigo QR usando libqrencode
    QRcode *qrcode = QRcode_encodeString(texto, 0, QR_ECLEVEL_H, QR_MODE_8, 1);

    if (!qrcode)
    {
        printf("Error al generar el codigo QR.\n");
        return 0;
    }

    // Configuración de la imagen
    int size = qrcode->width;
    int scale = 10; // Cada módulo del QR será de 10x10 píxeles
    int margin = 4 * scale; // Margen alrededor del QR
    int img_size = (size * scale) + (2 * margin);

    // Construir ruta completa del archivo
    const char *export_dir = get_export_dir();
    char filepath[500];
    snprintf(filepath, sizeof(filepath), "%s\\%s.png", export_dir, filename);

    // Crear el archivo PNG usando formato BMP simple (más fácil que PNG puro)
    // Usaremos BMP por simplicidad, pero lo nombraremos .png para compatibilidad
    FILE *file;
    errno_t err = fopen_s(&file, filepath, "wb");
    if (err != 0 || file == NULL)
    {
        printf("Error al crear archivo de imagen QR: %s\n", filepath);
        QRcode_free(qrcode);
        return 0;
    }

    // Escribir encabezado BMP
    int row_size = ((img_size * 3 + 3) / 4) * 4; // Alineación a 4 bytes
    int pixel_data_size = row_size * img_size;
    int file_size = 54 + pixel_data_size; // 54 = tamaño del encabezado BMP

    // File header (14 bytes)
    fputc('B', file);
    fputc('M', file); // Signature
    fwrite(&file_size, 4, 1, file);     // File size
    fwrite((int[])
    {
        0
    }, 4, 1, file);     // Reserved
    fwrite((int[])
    {
        54
    }, 4, 1, file);    // Pixel data offset

    // Info header (40 bytes)
    fwrite((int[])
    {
        40
    }, 4, 1, file);        // Info header size
    fwrite(&img_size, 4, 1, file);          // Width
    fwrite(&img_size, 4, 1, file);          // Height
    fwrite((short[])
    {
        1
    }, 2, 1, file);       // Planes
    fwrite((short[])
    {
        24
    }, 2, 1, file);      // Bits per pixel
    fwrite((int[])
    {
        0
    }, 4, 1, file);         // Compression
    fwrite(&pixel_data_size, 4, 1, file);   // Image size
    fwrite((int[])
    {
        2835
    }, 4, 1, file);      // X pixels per meter
    fwrite((int[])
    {
        2835
    }, 4, 1, file);      // Y pixels per meter
    fwrite((int[])
    {
        0
    }, 4, 1, file);         // Colors used
    fwrite((int[])
    {
        0
    }, 4, 1, file);         // Important colors

    // Escribir píxeles (BMP se escribe de abajo hacia arriba)
    unsigned char *row_buffer = (unsigned char*)calloc(row_size, 1);
    if (!row_buffer)
    {
        printf("Error de memoria al generar QR.\n");
        fclose(file);
        QRcode_free(qrcode);
        return 0;
    }

    // Preparar parámetros para procesamiento
    QrParams params = {margin, scale, size, img_size, qrcode};

    for (int y = img_size - 1; y >= 0; y--)
    {
        memset(row_buffer, 255, row_size); // Fondo blanco

        for (int x = 0; x < img_size; x++)
        {
            procesar_pixel_qr(row_buffer, x, y, &params);
        }

        fwrite(row_buffer, 1, row_size, file);
    }

    free(row_buffer);
    fclose(file);
    QRcode_free(qrcode);

    printf("Codigo QR generado exitosamente: %s\n", filepath);
    printf("   Tamanio: %dx%d píxeles\n", img_size, img_size);

    return 1;
}

int generar_qr_partido(int partido_id)
{
    return generar_qr_desde_json("partido", partido_id, "partido_%d",
                                 "Codigo QR generado para estadisticas del partido %d\n",
                                 obtener_estadisticas_partido_json);
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
        printf("Codigo QR generado para estadisticas del jugador %d en partido %d\n", jugador_id, partido_id);
    }

    return result;
}

int generar_qr_temporada(int temporada_id)
{
    return generar_qr_desde_json("temporada", temporada_id, "temporada_%d",
                                 "Codigo QR generado para resumen de temporada %d\n",
                                 obtener_resumen_temporada_json);
}

int generar_qr_camiseta(int camiseta_id)
{
    return generar_qr_desde_json("camiseta", camiseta_id, "camiseta_%d",
                                 "Codigo QR generado para INFORMACION de camiseta %d\n",
                                 obtener_info_camiseta_json);
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
        qr_partido();
        break;
    }
    case 2:   // QR de jugador en partido
    {
        qr_jugador_partido();
        break;
    }
    case 3:   // QR de temporada
    {
        qr_temporada();
        break;
    }
    case 4:   // QR de camiseta
    {
        qr_camiseta();
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
        printf("\nDesea generar otro codigo QR? (s/n): ");
        int respuesta = getchar();
        while (getchar() != '\n'); // Limpiar buffer

        if (respuesta == 's' || respuesta == 'S')
        {
            menu_qr();
        }
    }
}
