#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include "pdfgen.h"
#include <stdio.h>
#include <stdlib.h>

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

void exportar_carrera_csv(void)
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

void exportar_carrera_txt(void)
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

void exportar_carrera_json(void)
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

    char *json_string = cJSON_PrintUnformatted(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("carrera.json"));
}

void exportar_carrera_html(void)
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

typedef void (*pdf_fila_callback)(struct pdf_doc *pdf, float y, sqlite3_stmt *stmt, float margin, float wrap_w, char *buffer, size_t buf_size);

typedef struct
{
    struct pdf_doc *pdf;
    float margin;
    float wrap_w;
    float small_h;
    float line_h;
} PdfSeccionCtx;

static void escribir_fila_hito_pdf(struct pdf_doc *pdf, float y, sqlite3_stmt *stmt, float margin, float wrap_w, char *buffer, size_t buf_size)
{
    snprintf(buffer, buf_size, "#%d [%s] %s",
             sqlite3_column_int(stmt, 0),
             (const char*)sqlite3_column_text(stmt, 1),
             (const char*)sqlite3_column_text(stmt, 2));
    pdf_add_text_wrap(pdf, NULL, buffer, 10, margin, y, 0, PDF_BLACK, wrap_w, PDF_ALIGN_LEFT, NULL);
}

static void escribir_fila_resumen_pdf(struct pdf_doc *pdf, float y, sqlite3_stmt *stmt, float margin, float wrap_w, char *buffer, size_t buf_size)
{
    snprintf(buffer, buf_size, "[%s] %s",
             (const char*)sqlite3_column_text(stmt, 1),
             (const char*)sqlite3_column_text(stmt, 2));
    pdf_add_text_wrap(pdf, NULL, buffer, 10, margin, y, 0, PDF_BLACK, wrap_w, PDF_ALIGN_LEFT, NULL);
}

static void escribir_identidad_pdf(struct pdf_doc *pdf, float *y, float margin, float line_h)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, SQL_IDENTIDAD, -1, &stmt, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Nombre: %s",
                     (const char*)sqlite3_column_text(stmt, 1));
            pdf_add_text(pdf, NULL, buffer, 10, margin, *y, PDF_BLACK);
            *y -= line_h;

            snprintf(buffer, sizeof(buffer), "Posicion: %s",
                     (const char*)sqlite3_column_text(stmt, 2));
            pdf_add_text(pdf, NULL, buffer, 10, margin, *y, PDF_BLACK);
            *y -= line_h;

            snprintf(buffer, sizeof(buffer), "Club Inicios: %s",
                     (const char*)sqlite3_column_text(stmt, 3));
            pdf_add_text(pdf, NULL, buffer, 10, margin, *y, PDF_BLACK);
            *y -= line_h;
        }
        sqlite3_finalize(stmt);
    }
}
static void escribir_seccion_pdf(PdfSeccionCtx ctx, float *y,
                                 const char *sql, const char *titulo,
                                 const char *sin_registros,
                                 pdf_fila_callback escribir_fila)
{
    if (*y < 60)
    {
        pdf_append_page(ctx.pdf);
        *y = PDF_A4_HEIGHT - ctx.margin;
    }
    pdf_add_text(ctx.pdf, NULL, titulo, 14, ctx.margin, *y, PDF_BLACK);
    *y -= 18;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            if (*y < 40)
            {
                pdf_append_page(ctx.pdf);
                *y = PDF_A4_HEIGHT - ctx.margin;
            }
            char buffer[512];
            escribir_fila(ctx.pdf, *y, stmt, ctx.margin, ctx.wrap_w, buffer, sizeof(buffer));
            *y -= ctx.small_h;
            count++;
        }
        sqlite3_finalize(stmt);

        if (count == 0)
        {
            pdf_add_text(ctx.pdf, NULL, sin_registros, 10, ctx.margin, *y, PDF_BLACK);
            *y -= ctx.line_h;
        }
    }
}

void exportar_carrera_pdf(void)
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

    pdf_add_text(pdf, NULL, "Identidad", 14, margin, y, PDF_BLACK);
    y -= 18;
    escribir_identidad_pdf(pdf, &y, margin, line_h);
    y -= 8;

    PdfSeccionCtx ctx = {pdf, margin, wrap_w, small_h, line_h};
    escribir_seccion_pdf(ctx, &y, SQL_HITOS, "Hitos de la Carrera",
                         "Sin hitos registrados.", escribir_fila_hito_pdf);
    y -= 8;

    escribir_seccion_pdf(ctx, &y, SQL_RESUMENES, "Resumenes Narrativos",
                         "Sin resumenes registrados.", escribir_fila_resumen_pdf);

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

/* ============================================================================
 * HELPERS BATCH (stmt externo, sin prepare/finalize interno)
 * ============================================================================ */

static void carrera_csv_section(FILE *f, sqlite3_stmt *sid, sqlite3_stmt *shit, sqlite3_stmt *sres)
{
    fprintf(f, "=== IDENTIDAD ===\n");
    fprintf(f, "id,nombre_apodo,posicion,club_inicios\n");
    while (sqlite3_step(sid) == SQLITE_ROW) escribir_fila_identidad_csv(f, sid);
    fprintf(f, "\n=== HITOS ===\n");
    fprintf(f, "id,tipo,descripcion\n");
    while (sqlite3_step(shit) == SQLITE_ROW) escribir_fila_hito_csv(f, shit);
    fprintf(f, "\n=== RESUMENES ===\n");
    fprintf(f, "id,anio,resumen\n");
    while (sqlite3_step(sres) == SQLITE_ROW) escribir_fila_resumen_csv(f, sres);
    fprintf(f, "\n");
}

static void carrera_txt_section(FILE *f, sqlite3_stmt *sid, sqlite3_stmt *shit, sqlite3_stmt *sres)
{
    fprintf(f, "CARRERA FUTBOLISTICA\n\n");
    fprintf(f, "=== IDENTIDAD ===\n\n");
    while (sqlite3_step(sid) == SQLITE_ROW) escribir_fila_identidad_txt(f, sid);
    fprintf(f, "\n=== HITOS ===\n\n");
    while (sqlite3_step(shit) == SQLITE_ROW) escribir_fila_hito_txt(f, shit);
    fprintf(f, "\n=== RESUMENES NARRATIVOS ===\n\n");
    while (sqlite3_step(sres) == SQLITE_ROW) escribir_fila_resumen_txt(f, sres);
    fprintf(f, "\n");
}

static void carrera_json_section(cJSON *root, sqlite3_stmt *sid, sqlite3_stmt *shit, sqlite3_stmt *sres)
{
    cJSON *identidad = cJSON_CreateArray();
    while (sqlite3_step(sid) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        escribir_objeto_identidad(item, sid);
        cJSON_AddItemToArray(identidad, item);
    }
    cJSON_AddItemToObject(root, "identidad", identidad);
    cJSON *hitos = cJSON_CreateArray();
    while (sqlite3_step(shit) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        escribir_objeto_hito(item, shit);
        cJSON_AddItemToArray(hitos, item);
    }
    cJSON_AddItemToObject(root, "hitos", hitos);
    cJSON *resumenes = cJSON_CreateArray();
    while (sqlite3_step(sres) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        escribir_objeto_resumen(item, sres);
        cJSON_AddItemToArray(resumenes, item);
    }
    cJSON_AddItemToObject(root, "resumenes", resumenes);
}

static void carrera_html_section(FILE *f, sqlite3_stmt *sid, sqlite3_stmt *shit, sqlite3_stmt *sres)
{
    fprintf(f, "<html><body><h1>Carrera Futbolistica</h1>\n");
    fprintf(f, "<h2>Identidad</h2><table border='1'><tr><th>ID</th><th>Nombre/Apodo</th><th>Posicion</th><th>Club Inicios</th></tr>");
    while (sqlite3_step(sid) == SQLITE_ROW) escribir_fila_identidad_html(f, sid);
    fprintf(f, "</table><br>\n<h2>Hitos</h2><table border='1'><tr><th>ID</th><th>Tipo</th><th>Descripcion</th></tr>");
    while (sqlite3_step(shit) == SQLITE_ROW) escribir_fila_hito_html(f, shit);
    fprintf(f, "</table><br>\n<h2>Resumenes Narrativos</h2><table border='1'><tr><th>ID</th><th>Anio</th><th>Resumen</th></tr>");
    while (sqlite3_step(sres) == SQLITE_ROW) escribir_fila_resumen_html(f, sres);
    fprintf(f, "</table></body></html>\n");
}

void exportar_carrera_all(void)
{
    if (!hay_registros("carrera_identidad") && !hay_registros("carrera_partido_hito") && !hay_registros("carrera_resumen_narrativo"))
    {
        printf("No hay datos de carrera para exportar.\n");
        return;
    }

    sqlite3_stmt *sid;
    sqlite3_stmt *shit;
    sqlite3_stmt *sres;
    if (sqlite3_prepare_v2(db, SQL_IDENTIDAD, -1, &sid, NULL) != SQLITE_OK) return;
    if (sqlite3_prepare_v2(db, SQL_HITOS, -1, &shit, NULL) != SQLITE_OK)
    {
        sqlite3_finalize(sid);
        return;
    }
    if (sqlite3_prepare_v2(db, SQL_RESUMENES, -1, &sres, NULL) != SQLITE_OK)
    {
        sqlite3_finalize(sid);
        sqlite3_finalize(shit);
        return;
    }

    FILE *f;
    f = abrir_archivo_exportacion("carrera.csv", "Error CSV");
    if (f)
    {
        carrera_csv_section(f, sid, shit, sres);
        fclose(f);
        printf("Exportado: %s\n", get_export_path("carrera.csv"));
    }
    sqlite3_reset(sid);
    sqlite3_reset(shit);
    sqlite3_reset(sres);

    f = abrir_archivo_exportacion("carrera.txt", "Error TXT");
    if (f)
    {
        carrera_txt_section(f, sid, shit, sres);
        fclose(f);
        printf("Exportado: %s\n", get_export_path("carrera.txt"));
    }
    sqlite3_reset(sid);
    sqlite3_reset(shit);
    sqlite3_reset(sres);

    cJSON *root = cJSON_CreateObject();
    carrera_json_section(root, sid, shit, sres);
    f = abrir_archivo_exportacion("carrera.json", "Error JSON");
    if (f)
    {
        char *json_str = cJSON_PrintUnformatted(root);
        fprintf(f, "%s", json_str);
        free(json_str);
        fclose(f);
        printf("Exportado: %s\n", get_export_path("carrera.json"));
    }
    cJSON_Delete(root);
    sqlite3_reset(sid);
    sqlite3_reset(shit);
    sqlite3_reset(sres);

    f = abrir_archivo_exportacion("carrera.html", "Error HTML");
    if (f)
    {
        carrera_html_section(f, sid, shit, sres);
        fclose(f);
        printf("Exportado: %s\n", get_export_path("carrera.html"));
    }

    sqlite3_finalize(sid);
    sqlite3_finalize(shit);
    sqlite3_finalize(sres);

    exportar_carrera_pdf();
}
