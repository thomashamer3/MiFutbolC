/**
 * @file export_pdf.c
 * @brief Generación de informes en PDF con libharu.
 */

#include "export_pdf.h"
#include "export_camisetas.h"
#include "export_camisetas_mejorado.h"
#include "export_partidos.h"
#include "export_lesiones.h"
#include "export_lesiones_mejorado.h"
#include "export_estadisticas.h"
#include "export_estadisticas_generales.h"
#include "export_records_rankings.h"
#include "export.h"
#include "utils.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#include <ctype.h>
#include <hpdf.h>

#define PDF_MARGIN 40.0f
#define PDF_BODY_SIZE 10.0f
#define PDF_SUBTITLE_SIZE 12.0f
#define PDF_TITLE_SIZE 16.0f
#define PDF_SECTION_SIZE 12.0f
#define PDF_FOOTER_SIZE 8.0f

typedef struct
{
    HPDF_Doc pdf;
    HPDF_Page page;
    HPDF_Font font_regular;
    HPDF_Font font_bold;
    HPDF_Font font_italic;
    float margin;
    float y;
    HPDF_Page *pages;
    int page_count;
    int page_cap;
} PdfCtx;

static jmp_buf g_env;

static void pdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data)
{
    (void)error_no;
    (void)detail_no;
    (void)user_data;
    longjmp(g_env, 1);
}

static void new_page(PdfCtx *ctx)
{
    ctx->page = HPDF_AddPage(ctx->pdf);
    HPDF_Page_SetSize(ctx->page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    ctx->y = HPDF_Page_GetHeight(ctx->page) - ctx->margin;

    if (ctx->page_cap <= ctx->page_count)
    {
        int new_cap = ctx->page_cap == 0 ? 8 : ctx->page_cap * 2;
        HPDF_Page *new_pages = (HPDF_Page *)realloc(ctx->pages, sizeof(HPDF_Page) * new_cap);
        if (new_pages)
        {
            ctx->pages = new_pages;
            ctx->page_cap = new_cap;
        }
    }

    if (ctx->pages && ctx->page_count < ctx->page_cap)
    {
        ctx->pages[ctx->page_count++] = ctx->page;
    }
}

static void ensure_space(PdfCtx *ctx, float needed)
{
    if (ctx->y - needed < ctx->margin)
        new_page(ctx);
}

static void write_text_line(PdfCtx *ctx, const char *text, HPDF_Font font, float size, float leading)
{
    if (!text)
        return;
    ensure_space(ctx, leading);
    HPDF_Page_BeginText(ctx->page);
    HPDF_Page_SetFontAndSize(ctx->page, font, size);
    HPDF_Page_TextOut(ctx->page, ctx->margin, ctx->y, text);
    HPDF_Page_EndText(ctx->page);
    ctx->y -= leading;
}

static void write_blank_line(PdfCtx *ctx, float leading)
{
    ensure_space(ctx, leading);
    ctx->y -= leading;
}

static const char *skip_spaces(const char *p)
{
    while (p && (*p == ' ' || *p == '\t'))
        p++;
    return p;
}

static const char *read_word(const char *p, char *word, size_t max_len)
{
    size_t wlen = 0;
    if (!p || !word || max_len == 0)
        return p;

    while (*p && *p != ' ' && *p != '\t' && wlen + 1 < max_len)
    {
        word[wlen++] = *p++;
    }
    word[wlen] = '\0';
    return p;
}

static void flush_line(PdfCtx *ctx, char *line, HPDF_Font font, float size, float leading)
{
    if (line && line[0] != '\0')
    {
        write_text_line(ctx, line, font, size, leading);
        line[0] = '\0';
    }
}

static void write_long_word(PdfCtx *ctx, const char *word, HPDF_Font font, float size, float leading, float max_width)
{
    char chunk[256];
    size_t clen = 0;

    for (size_t i = 0; word && word[i] != '\0'; i++)
    {
        if (clen + 1 >= sizeof(chunk))
        {
            chunk[clen] = '\0';
            write_text_line(ctx, chunk, font, size, leading);
            clen = 0;
        }

        chunk[clen++] = word[i];
        chunk[clen] = '\0';

        if (HPDF_Page_TextWidth(ctx->page, chunk) > max_width && clen > 1)
        {
            chunk[clen - 1] = '\0';
            write_text_line(ctx, chunk, font, size, leading);
            clen = 0;
            chunk[clen++] = word[i];
            chunk[clen] = '\0';
        }
    }

    if (clen > 0)
    {
        chunk[clen] = '\0';
        write_text_line(ctx, chunk, font, size, leading);
    }
}

static int try_append_word(PdfCtx *ctx, char *line, size_t line_size, const char *word, float max_width)
{
    if (!ctx || !line || !word || line_size == 0)
        return 0;

    size_t line_len = strlen_s(line, SIZE_MAX);
    size_t word_len = strlen_s(word, SIZE_MAX);
    int fits_in_buffer = 0;

    if (line_len == 0)
    {
        fits_in_buffer = (word_len < line_size);
    }
    else
    {
        fits_in_buffer = (line_len + 1 + word_len < line_size);
    }

    if (!fits_in_buffer)
        return 0;

    char candidate[512];
    if (line_len == 0)
    {
        memcpy(candidate, word, word_len + 1);
    }
    else
    {
        memcpy(candidate, line, line_len);
        candidate[line_len] = ' ';
        memcpy(candidate + line_len + 1, word, word_len + 1);
    }

    if (HPDF_Page_TextWidth(ctx->page, candidate) > max_width)
        return 0;

    size_t cand_len = strlen_s(candidate, SIZE_MAX);
    memcpy(line, candidate, cand_len + 1);
    return 1;
}

static void write_wrapped_text(PdfCtx *ctx, const char *text, HPDF_Font font, float size, float leading)
{
    if (!text)
        return;

    if (text[0] == '\0')
    {
        write_blank_line(ctx, leading);
        return;
    }

    HPDF_Page_SetFontAndSize(ctx->page, font, size);
    float max_width = HPDF_Page_GetWidth(ctx->page) - (2.0f * ctx->margin);

    char line[512];
    line[0] = '\0';

    const char *p = text;
    while (p && *p)
    {
        p = skip_spaces(p);
        if (!p || *p == '\0')
            break;

        char word[256];
        p = read_word(p, word, sizeof(word));
        if (word[0] == '\0')
            continue;

        if (try_append_word(ctx, line, sizeof(line), word, max_width))
            continue;

        flush_line(ctx, line, font, size, leading);

        if (HPDF_Page_TextWidth(ctx->page, word) <= max_width)
        {
            size_t word_len = strlen_s(word, SIZE_MAX);
            size_t copy_len = word_len < sizeof(line) ? word_len : sizeof(line) - 1;
            memcpy(line, word, copy_len);
            line[copy_len] = '\0';
            continue;
        }

        write_long_word(ctx, word, font, size, leading, max_width);
    }

    flush_line(ctx, line, font, size, leading);
}

static void write_section_header(PdfCtx *ctx, const char *title)
{
    if (!title)
        return;

    float leading = PDF_SECTION_SIZE + 6.0f;
    ensure_space(ctx, leading + 10.0f);

    float page_width = HPDF_Page_GetWidth(ctx->page);
    float band_height = PDF_SECTION_SIZE + 6.0f;
    float band_y = ctx->y - PDF_SECTION_SIZE - 2.0f;

    HPDF_Page_SetRGBFill(ctx->page, 0.92f, 0.94f, 0.98f);
    HPDF_Page_Rectangle(ctx->page, ctx->margin, band_y, page_width - (2.0f * ctx->margin), band_height);
    HPDF_Page_Fill(ctx->page);
    HPDF_Page_SetRGBFill(ctx->page, 0.0f, 0.0f, 0.0f);

    HPDF_Page_BeginText(ctx->page);
    HPDF_Page_SetFontAndSize(ctx->page, ctx->font_bold, PDF_SECTION_SIZE);
    HPDF_Page_TextOut(ctx->page, ctx->margin + 6.0f, ctx->y, title);
    HPDF_Page_EndText(ctx->page);

    ctx->y = band_y - 8.0f;
    write_blank_line(ctx, PDF_BODY_SIZE + 2.0f);
}

static void add_page_footers(const PdfCtx *ctx)
{
    if (!ctx || !ctx->pdf || !ctx->font_regular || !ctx->pages)
        return;

    int pages = ctx->page_count;
    for (int i = 0; i < pages; i++)
    {
        HPDF_Page page = ctx->pages[i];
        if (!page)
            continue;

        char footer[64];
        snprintf(footer, sizeof(footer), "MiFutbolC - Pagina %d de %d", i + 1, pages);

        float page_width = HPDF_Page_GetWidth(page);
        float y = ctx->margin * 0.5f;
        float text_width = HPDF_Page_TextWidth(page, footer);

        HPDF_Page_SetRGBStroke(page, 0.8f, 0.8f, 0.8f);
        HPDF_Page_SetLineWidth(page, 0.3f);
        HPDF_Page_MoveTo(page, ctx->margin, y + 6.0f);
        HPDF_Page_LineTo(page, page_width - ctx->margin, y + 6.0f);
        HPDF_Page_Stroke(page);

        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, ctx->font_regular, PDF_FOOTER_SIZE);
        HPDF_Page_TextOut(page, (page_width - text_width) / 2.0f, y, footer);
        HPDF_Page_EndText(page);
    }
}

static void draw_cover(PdfCtx *ctx, const char *title, const char *subtitle, const char *usuario,
                       const char *datetime, const char *export_dir, int total_sections)
{
    if (!ctx)
        return;

    new_page(ctx);

    float page_width = HPDF_Page_GetWidth(ctx->page);
    float page_height = HPDF_Page_GetHeight(ctx->page);

    HPDF_Page_SetRGBFill(ctx->page, 0.10f, 0.35f, 0.65f);
    HPDF_Page_Rectangle(ctx->page, 0, page_height - 140.0f, page_width, 140.0f);
    HPDF_Page_Fill(ctx->page);

    HPDF_Page_SetRGBFill(ctx->page, 1.0f, 1.0f, 1.0f);
    HPDF_Page_BeginText(ctx->page);
    HPDF_Page_SetFontAndSize(ctx->page, ctx->font_bold, 24.0f);
    float title_width = HPDF_Page_TextWidth(ctx->page, title);
    HPDF_Page_TextOut(ctx->page, (page_width - title_width) / 2.0f, page_height - 85.0f, title);
    HPDF_Page_EndText(ctx->page);

    HPDF_Page_BeginText(ctx->page);
    HPDF_Page_SetFontAndSize(ctx->page, ctx->font_regular, PDF_SUBTITLE_SIZE);
    float sub_width = HPDF_Page_TextWidth(ctx->page, subtitle);
    HPDF_Page_TextOut(ctx->page, (page_width - sub_width) / 2.0f, page_height - 110.0f, subtitle);
    HPDF_Page_EndText(ctx->page);

    HPDF_Page_SetRGBFill(ctx->page, 0.0f, 0.0f, 0.0f);
    ctx->y = page_height - 190.0f;

    write_text_line(ctx, "Resumen del informe", ctx->font_bold, PDF_SECTION_SIZE, PDF_SECTION_SIZE + 6.0f);
    write_blank_line(ctx, PDF_BODY_SIZE + 2.0f);

    char line[512];
    snprintf(line, sizeof(line), "Usuario: %s", usuario ? usuario : "Usuario Desconocido");
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
    snprintf(line, sizeof(line), "Fecha y hora: %s", datetime ? datetime : "-");
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
    snprintf(line, sizeof(line), "Directorio de exportacion: %s", export_dir ? export_dir : "-");
    write_wrapped_text(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
    snprintf(line, sizeof(line), "Secciones incluidas: %d", total_sections);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);

    write_blank_line(ctx, PDF_BODY_SIZE + 10.0f);
    write_wrapped_text(ctx, "Documento generado automaticamente por MiFutbolC.", ctx->font_italic,
                       PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
}

static void format_datetime_filename(const char *src, char *dst, size_t size)
{
    if (!dst || size == 0)
        return;

    dst[0] = '\0';
    if (!src)
        return;

    char *out = dst;
    size_t remaining = size;
    const char *p = src;
    while (*p != '\0' && remaining > 1)
    {
        char c = *p++;
        if (c == '/')
            c = '-';
        else if (c == ' ')
            c = '-';
        else if (c == ':')
            c = '.';
        *out++ = c;
        remaining--;
    }
    *out = '\0';
}

static void procesar_archivo_txt(const char *path, const char *titulo, PdfCtx *ctx)
{
    if (!path || !titulo || !ctx)
        return;

    write_section_header(ctx, titulo);

    FILE *f = NULL;
    errno_t err = fopen_s(&f, path, "r");
    if (err != 0 || !f)
    {
        write_wrapped_text(ctx, "[Archivo no disponible]", ctx->font_italic, PDF_BODY_SIZE, PDF_BODY_SIZE + 3.0f);
        write_blank_line(ctx, PDF_BODY_SIZE + 3.0f);
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), f))
    {
        size_t len = strlen_s(buffer, SIZE_MAX);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        {
            buffer[len - 1] = '\0';
            len--;
        }
        write_wrapped_text(ctx, buffer, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 3.0f);
    }

    fclose(f);
    write_blank_line(ctx, PDF_BODY_SIZE + 3.0f);
}

static int escribir_pdf(const char *filepath, PdfCtx *ctx)
{
    if (!filepath || !ctx || !ctx->pdf)
        return 0;

    HPDF_SaveToFile(ctx->pdf, filepath);
    return 1;
}

int generar_informe_total_pdf(void)
{
    exportar_camisetas_txt_mejorado();
    exportar_partidos_txt();
    exportar_partido_mas_goles_txt();
    exportar_partido_mas_asistencias_txt();
    exportar_partido_menos_goles_reciente_txt();
    exportar_partido_menos_asistencias_reciente_txt();
    exportar_lesiones_txt();
    exportar_lesiones_txt_mejorado();
    exportar_estadisticas_txt();
    exportar_analisis_txt();
    exportar_estadisticas_generales_txt();
    exportar_estadisticas_por_mes_txt();
    exportar_estadisticas_por_anio_txt();
    exportar_records_rankings_txt();
    exportar_finanzas_resumen_txt();
    exportar_ranking_canchas_txt();
    exportar_partidos_por_clima_txt();
    exportar_lesiones_por_tipo_estado_txt();
    exportar_rachas_historial_txt();
    exportar_estado_animo_cansancio_txt();

    const char *export_dir = get_export_dir();
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    char fecha_hora[32];
    get_datetime(fecha_hora, sizeof(fecha_hora));

    char fecha_archivo[64];
    format_datetime_filename(fecha_hora, fecha_archivo, sizeof(fecha_archivo));

    char pdf_filename[128];
    snprintf(pdf_filename, sizeof(pdf_filename), "Informe Total %s.pdf", fecha_archivo);

    char pdf_path[512];
    snprintf(pdf_path, sizeof(pdf_path), "%s\\%s", export_dir, pdf_filename);

    const char *files[] =
    {
        "camisetas_mejorado.txt",
        "partidos.txt",
        "partido_mas_goles.txt",
        "partido_mas_asistencias.txt",
        "partido_menos_goles_reciente.txt",
        "partido_menos_asistencias_reciente.txt",
        "lesiones.txt",
        "lesiones_mejorado.txt",
        "estadisticas.txt",
        "analisis.txt",
        "estadisticas_generales.txt",
        "estadisticas_por_mes.txt",
        "estadisticas_por_anio.txt",
        "records_rankings.txt",
        "finanzas_resumen.txt",
        "ranking_canchas.txt",
        "partidos_por_clima.txt",
        "lesiones_por_tipo_estado.txt",
        "rachas_historial.txt",
        "estado_animo_cansancio.txt"
    };

    const char *titles[] =
    {
        "Camisetas (analisis avanzado)",
        "Partidos",
        "Partido con mas goles",
        "Partido con mas asistencias",
        "Partido menos goles reciente",
        "Partido menos asistencias reciente",
        "Lesiones",
        "Lesiones (analisis de impacto)",
        "Estadisticas",
        "Analisis",
        "Estadisticas generales",
        "Estadisticas por mes",
        "Estadisticas por anio",
        "Records y rankings",
        "Resumen financiero",
        "Ranking de canchas",
        "Partidos por clima",
        "Lesiones por tipo y estado",
        "Historial de rachas",
        "Estado de animo y cansancio"
    };

    char *usuario = get_user_name();
    const char *usuario_final = usuario ? usuario : "Usuario Desconocido";

    PdfCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.margin = PDF_MARGIN;

    ctx.pdf = HPDF_New(pdf_error_handler, NULL);
    if (!ctx.pdf)
    {
        printf("No se pudo crear el documento PDF.\n");
        return 0;
    }

    if (setjmp(g_env))
    {
        HPDF_Free(ctx.pdf);
        printf("Error al generar el PDF.\n");
        return 0;
    }

    HPDF_SetCompressionMode(ctx.pdf, HPDF_COMP_ALL);
    ctx.font_regular = HPDF_GetFont(ctx.pdf, "Helvetica", "WinAnsiEncoding");
    ctx.font_bold = HPDF_GetFont(ctx.pdf, "Helvetica-Bold", "WinAnsiEncoding");
    ctx.font_italic = HPDF_GetFont(ctx.pdf, "Helvetica-Oblique", "WinAnsiEncoding");

    draw_cover(&ctx, "MiFutbolC", "Informe total de exportacion", usuario_final, fecha_hora,
               export_dir, (int)(sizeof(files) / sizeof(files[0])));

    new_page(&ctx);

    char intro[256];
    snprintf(intro, sizeof(intro), "INFORME TOTAL MiFutbolC - %s", timestamp);
    write_text_line(&ctx, intro, ctx.font_bold, PDF_TITLE_SIZE, PDF_TITLE_SIZE + 6.0f);
    write_blank_line(&ctx, PDF_BODY_SIZE + 4.0f);

    for (int i = 0; i < (int)(sizeof(files) / sizeof(files[0])); i++)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s\\%s", export_dir, files[i]);
        procesar_archivo_txt(path, titles[i], &ctx);
    }

    add_page_footers(&ctx);

    int ok = escribir_pdf(pdf_path, &ctx);
    HPDF_Free(ctx.pdf);

    if (ctx.pages)
        free(ctx.pages);

    if (usuario)
        free(usuario);

    if (!ok)
    {
        printf("No se pudo generar el informe PDF.\n");
        return 0;
    }

    printf("Informe PDF generado exitosamente: %s\n", pdf_path);
    return 1;
}
