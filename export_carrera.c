#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include "pdfgen.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include "direct.h"
#endif
#include <string.h>

static int escribir_seccion_csv(FILE *f, const char *sql, const char *cabecera,
                                void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    fprintf(f, "%s\n", cabecera);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        escribir_fila(f, stmt);
    }
    sqlite3_finalize(stmt);
    fprintf(f, "\n");
    return 1;
}

static void escribir_seccion_txt(FILE *f, const char *titulo, const char *sql,
                                 void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    fprintf(f, "=== %s ===\n\n", titulo);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        escribir_fila(f, stmt);
        count++;
    }
    if (count == 0)
        fprintf(f, "Sin registros.\n\n");
    sqlite3_finalize(stmt);
    fprintf(f, "\n");
}

static void escribir_seccion_json(cJSON *arr, const char *sql,
                                  void (*escribir_objeto)(cJSON *item, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        escribir_objeto(item, stmt);
        cJSON_AddItemToArray(arr, item);
    }
    sqlite3_finalize(stmt);
}

static void escribir_seccion_html(FILE *f, const char *titulo, const char *sql,
                                  const char *cabeceras[],
                                  void (*escribir_fila)(FILE *f, sqlite3_stmt *stmt))
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    fprintf(f, "<h2>%s</h2><table border='1'><tr>", titulo);
    for (int i = 0; cabeceras[i] != NULL; i++)
        fprintf(f, "<th>%s</th>", cabeceras[i]);
    fprintf(f, "</tr>");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        escribir_fila(f, stmt);
    }
    fprintf(f, "</table><br>\n");
    sqlite3_finalize(stmt);
}

static void escribir_fila_identidad_csv(FILE *f, sqlite3_stmt *stmt)
{
    char nombre_limpio[256];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 1), nombre_limpio, sizeof(nombre_limpio));
    fprintf(f, "%d,%s,%s,%s\n",
            sqlite3_column_int(stmt, 0),
            nombre_limpio,
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3));
}

static void escribir_fila_identidad_txt(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "ID: %d\n  Nombre: %s\n  Posicion: %s\n  Club Inicios: %s\n\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3));
}

static void escribir_objeto_identidad(cJSON *item, sqlite3_stmt *stmt)
{
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre_apodo", (const char*)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "posicion", (const char*)sqlite3_column_text(stmt, 2));
    cJSON_AddStringToObject(item, "club_inicios", (const char*)sqlite3_column_text(stmt, 3));
}

static void escribir_fila_identidad_html(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3));
}

static void escribir_fila_hito_csv(FILE *f, sqlite3_stmt *stmt)
{
    char desc_limpio[256];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 2), desc_limpio, sizeof(desc_limpio));
    fprintf(f, "%d,%s,%s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            desc_limpio);
}

static void escribir_fila_hito_txt(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "ID: %d - Tipo: %s - Descripcion: %s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2));
}

static void escribir_objeto_hito(cJSON *item, sqlite3_stmt *stmt)
{
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "tipo", (const char*)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "descripcion", (const char*)sqlite3_column_text(stmt, 2));
}

static void escribir_fila_hito_html(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2));
}

static void escribir_fila_resumen_csv(FILE *f, sqlite3_stmt *stmt)
{
    char res_limpio[512];
    sanitizar_ascii_basico((const char*)sqlite3_column_text(stmt, 2), res_limpio, sizeof(res_limpio));
    fprintf(f, "%d,%s,%s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            res_limpio);
}

static void escribir_fila_resumen_txt(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "ID: %d - Anio: %s - Resumen: %s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2));
}

static void escribir_objeto_resumen(cJSON *item, sqlite3_stmt *stmt)
{
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "anio", (const char*)sqlite3_column_text(stmt, 1));
    cJSON_AddStringToObject(item, "resumen", (const char*)sqlite3_column_text(stmt, 2));
}

static void escribir_fila_resumen_html(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2));
}

static const char *SQL_IDENTIDAD = "SELECT id, nombre, posiciones, COALESCE(historia, '') FROM carrera_identidad WHERE id = 1";
static const char *SQL_HITOS = "SELECT id, tipo_hito, COALESCE(nota, '') FROM carrera_partido_hito ORDER BY id";
static const char *SQL_RESUMENES = "SELECT id, COALESCE(periodo_inicio, ''), resumen FROM carrera_resumen_narrativo ORDER BY id";

void exportar_carrera_csv()
{
    if (!hay_registros("carrera_identidad") && !hay_registros("carrera_partido_hito") && !hay_registros("carrera_resumen_narrativo"))
    {
        printf("No hay datos de carrera para exportar.\n");
        return;
    }

    FILE *f = abrir_archivo_exportacion("carrera.csv", "Error al crear el archivo CSV");
    if (!f) return;

    fprintf(f, "=== IDENTIDAD ===\n");
    escribir_seccion_csv(f, SQL_IDENTIDAD, "id,nombre_apodo,posicion,club_inicios", escribir_fila_identidad_csv);

    fprintf(f, "=== HITOS ===\n");
    escribir_seccion_csv(f, SQL_HITOS, "id,tipo,descripcion", escribir_fila_hito_csv);

    fprintf(f, "=== RESUMENES ===\n");
    escribir_seccion_csv(f, SQL_RESUMENES, "id,anio,resumen", escribir_fila_resumen_csv);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("carrera.csv"));
}

void exportar_carrera_txt()
{
    if (!hay_registros("carrera_identidad") && !hay_registros("carrera_partido_hito") && !hay_registros("carrera_resumen_narrativo"))
    {
        printf("No hay datos de carrera para exportar.\n");
        return;
    }

    FILE *f = abrir_archivo_exportacion("carrera.txt", "Error al crear el archivo TXT");
    if (!f) return;

    fprintf(f, "CARRERA FUTBOLISTICA\n\n");
    escribir_seccion_txt(f, "IDENTIDAD", SQL_IDENTIDAD, escribir_fila_identidad_txt);
    escribir_seccion_txt(f, "HITOS", SQL_HITOS, escribir_fila_hito_txt);
    escribir_seccion_txt(f, "RESUMENES NARRATIVOS", SQL_RESUMENES, escribir_fila_resumen_txt);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("carrera.txt"));
}

void exportar_carrera_json()
{
    if (!hay_registros("carrera_identidad") && !hay_registros("carrera_partido_hito") && !hay_registros("carrera_resumen_narrativo"))
    {
        printf("No hay datos de carrera para exportar.\n");
        return;
    }

    cJSON *root = cJSON_CreateObject();

    cJSON *identidad = cJSON_CreateArray();
    escribir_seccion_json(identidad, SQL_IDENTIDAD, escribir_objeto_identidad);
    cJSON_AddItemToObject(root, "identidad", identidad);

    cJSON *hitos = cJSON_CreateArray();
    escribir_seccion_json(hitos, SQL_HITOS, escribir_objeto_hito);
    cJSON_AddItemToObject(root, "hitos", hitos);

    cJSON *resumenes = cJSON_CreateArray();
    escribir_seccion_json(resumenes, SQL_RESUMENES, escribir_objeto_resumen);
    cJSON_AddItemToObject(root, "resumenes", resumenes);

    FILE *f;
    errno_t err = fopen_s(&f, get_export_path("carrera.json"), "w");
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
    printf("Archivo exportado a: %s\n", get_export_path("carrera.json"));
}

void exportar_carrera_html()
{
    if (!hay_registros("carrera_identidad") && !hay_registros("carrera_partido_hito") && !hay_registros("carrera_resumen_narrativo"))
    {
        printf("No hay datos de carrera para exportar.\n");
        return;
    }

    FILE *f = abrir_archivo_exportacion("carrera.html", "Error al crear el archivo HTML");
    if (!f) return;

    fprintf(f, "<html><body><h1>Carrera Futbolistica</h1>\n");

    const char *cab_identidad[] = {"ID", "Nombre/Apodo", "Posicion", "Club Inicios", NULL};
    escribir_seccion_html(f, "Identidad", SQL_IDENTIDAD, cab_identidad, escribir_fila_identidad_html);

    const char *cab_hitos[] = {"ID", "Tipo", "Descripcion", NULL};
    escribir_seccion_html(f, "Hitos", SQL_HITOS, cab_hitos, escribir_fila_hito_html);

    const char *cab_resumenes[] = {"ID", "Anio", "Resumen", NULL};
    escribir_seccion_html(f, "Resumenes Narrativos", SQL_RESUMENES, cab_resumenes, escribir_fila_resumen_html);

    fprintf(f, "</body></html>");
    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("carrera.html"));
}

void exportar_carrera_pdf()
{
    if (!hay_registros("carrera_identidad") && !hay_registros("carrera_partido_hito") && !hay_registros("carrera_resumen_narrativo"))
    {
        printf("No hay datos de carrera para exportar.\n");
        return;
    }

    struct pdf_info info =
    {
        .creator = "MiFutbolC",
        .producer = "MiFutbolC",
        .title = "Carrera Futbolistica",
        .author = "MiFutbolC",
        .subject = "Resumen de carrera",
        .date = ""
    };

    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    if (!pdf)
    {
        printf("Error al crear el documento PDF.\n");
        return;
    }
    pdf_set_font(pdf, "Helvetica");

    pdf_append_page(pdf);
    float margin = 40.0f;
    float y = PDF_A4_HEIGHT - margin;
    float page_w = pdf_width(pdf);
    float wrap_w = page_w - 2 * margin;
    const float line_h = 14.0f;
    const float small_h = 11.0f;

    // Title
    pdf_add_text(pdf, NULL, "Carrera Futbolistica", 18, margin, y, PDF_BLACK);
    y -= 24;

    // Identidad section
    pdf_add_text(pdf, NULL, "Identidad", 14, margin, y, PDF_BLACK);
    y -= 18;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, SQL_IDENTIDAD, -1, &stmt, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Nombre: %s",
                     (const char*)sqlite3_column_text(stmt, 1));
            pdf_add_text(pdf, NULL, buffer, 10, margin, y, PDF_BLACK);
            y -= line_h;

            snprintf(buffer, sizeof(buffer), "Posicion: %s",
                     (const char*)sqlite3_column_text(stmt, 2));
            pdf_add_text(pdf, NULL, buffer, 10, margin, y, PDF_BLACK);
            y -= line_h;

            snprintf(buffer, sizeof(buffer), "Club Inicios: %s",
                     (const char*)sqlite3_column_text(stmt, 3));
            pdf_add_text(pdf, NULL, buffer, 10, margin, y, PDF_BLACK);
            y -= line_h;
        }
        sqlite3_finalize(stmt);
    }

    y -= 8;

    // Hitos section
    if (y < 60)
    {
        pdf_append_page(pdf);
        y = PDF_A4_HEIGHT - margin;
    }
    pdf_add_text(pdf, NULL, "Hitos de la Carrera", 14, margin, y, PDF_BLACK);
    y -= 18;

    if (sqlite3_prepare_v2(db, SQL_HITOS, -1, &stmt, NULL) == SQLITE_OK)
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            if (y < 40)
            {
                pdf_append_page(pdf);
                y = PDF_A4_HEIGHT - margin;
            }
            char buffer[512];
            snprintf(buffer, sizeof(buffer), "#%d [%s] %s",
                     sqlite3_column_int(stmt, 0),
                     (const char*)sqlite3_column_text(stmt, 1),
                     (const char*)sqlite3_column_text(stmt, 2));
            pdf_add_text_wrap(pdf, NULL, buffer, 10, margin, y, 0, PDF_BLACK, wrap_w, PDF_ALIGN_LEFT, NULL);
            y -= small_h;
            count++;
        }
        sqlite3_finalize(stmt);
        if (count == 0)
        {
            pdf_add_text(pdf, NULL, "Sin hitos registrados.", 10, margin, y, PDF_BLACK);
            y -= line_h;
        }
    }

    y -= 8;

    // Resumenes section
    if (y < 60)
    {
        pdf_append_page(pdf);
        y = PDF_A4_HEIGHT - margin;
    }
    pdf_add_text(pdf, NULL, "Resumenes Narrativos", 14, margin, y, PDF_BLACK);
    y -= 18;

    if (sqlite3_prepare_v2(db, SQL_RESUMENES, -1, &stmt, NULL) == SQLITE_OK)
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            if (y < 40)
            {
                pdf_append_page(pdf);
                y = PDF_A4_HEIGHT - margin;
            }
            char buffer[512];
            snprintf(buffer, sizeof(buffer), "[%s] %s",
                     (const char*)sqlite3_column_text(stmt, 1),
                     (const char*)sqlite3_column_text(stmt, 2));
            pdf_add_text_wrap(pdf, NULL, buffer, 10, margin, y, 0, PDF_BLACK, wrap_w, PDF_ALIGN_LEFT, NULL);
            y -= small_h;
            count++;
        }
        sqlite3_finalize(stmt);
        if (count == 0)
        {
            pdf_add_text(pdf, NULL, "Sin resumenes registrados.", 10, margin, y, PDF_BLACK);
            y -= line_h;
        }
    }

    if (pdf_save(pdf, get_export_path("carrera.pdf")) < 0)
    {
        printf("Error al guardar el PDF.\n");
    }
    else
    {
        printf("Archivo exportado a: %s\n", get_export_path("carrera.pdf"));
    }

    pdf_destroy(pdf);
}
