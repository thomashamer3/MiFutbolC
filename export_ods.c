#include "export_ods.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdarg.h>

static uint32_t crc32_tab[256];
static int crc32_inited = 0;

static void crc32_init(void)
{
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        crc32_tab[i] = c;
    }
    crc32_inited = 1;
}

static uint32_t crc32_buf(const uint8_t *buf, size_t len)
{
    if (!crc32_inited) crc32_init();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc32_tab[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

static void zip_write_local_header(FILE *f, const char *name, uint32_t crc32val,
                                   uint32_t size, uint16_t dos_time, uint16_t dos_date,
                                   uint32_t *offset_out)
{
    uint16_t name_len = (uint16_t)strnlen_s(name, (size_t)-1);
    uint32_t offset = (uint32_t)ftell(f);
    if (offset_out) *offset_out = offset;

    uint8_t hdr[30] = {0};
    hdr[0] = 'P';
    hdr[1] = 'K';
    hdr[2] = 3;
    hdr[3] = 4;          // signature
    hdr[4] = 20;
    hdr[5] = 0;          // version needed
    hdr[6] = 0;
    hdr[7] = 0;          // flags
    hdr[8] = 0;
    hdr[9] = 0;          // method: stored
    hdr[10] = dos_time & 0xFF;
    hdr[11] = (dos_time >> 8) & 0xFF;
    hdr[12] = dos_date & 0xFF;
    hdr[13] = (dos_date >> 8) & 0xFF;
    hdr[14] = crc32val & 0xFF;
    hdr[15] = (crc32val >> 8) & 0xFF;
    hdr[16] = (crc32val >> 16) & 0xFF;
    hdr[17] = (crc32val >> 24) & 0xFF;
    hdr[18] = size & 0xFF;
    hdr[19] = (size >> 8) & 0xFF;
    hdr[20] = (size >> 16) & 0xFF;
    hdr[21] = (size >> 24) & 0xFF;
    hdr[22] = size & 0xFF;
    hdr[23] = (size >> 8) & 0xFF;
    hdr[24] = (size >> 16) & 0xFF;
    hdr[25] = (size >> 24) & 0xFF;
    hdr[26] = name_len & 0xFF;
    hdr[27] = (name_len >> 8) & 0xFF;
    // extra field length = 0
    fwrite(hdr, 1, 30, f);
    fwrite(name, 1, name_len, f);
}

static void zip_write_central_dir(FILE *f, const char *name, uint32_t crc32val,
                                  uint32_t size, uint16_t dos_time, uint16_t dos_date,
                                  uint32_t local_offset)
{
    uint16_t name_len = (uint16_t)strnlen_s(name, (size_t)-1);
    uint8_t cd[46] = {0};
    cd[0] = 'P';
    cd[1] = 'K';
    cd[2] = 1;
    cd[3] = 2;            // signature
    cd[4] = 20;
    cd[5] = 0;           // version made by
    cd[6] = 20;
    cd[7] = 0;           // version needed
    cd[8] = 0;
    cd[9] = 0;            // flags
    cd[10] = 0;
    cd[11] = 0;          // method: stored
    cd[12] = dos_time & 0xFF;
    cd[13] = (dos_time >> 8) & 0xFF;
    cd[14] = dos_date & 0xFF;
    cd[15] = (dos_date >> 8) & 0xFF;
    cd[16] = crc32val & 0xFF;
    cd[17] = (crc32val >> 8) & 0xFF;
    cd[18] = (crc32val >> 16) & 0xFF;
    cd[19] = (crc32val >> 24) & 0xFF;
    cd[20] = size & 0xFF;
    cd[21] = (size >> 8) & 0xFF;
    cd[22] = (size >> 16) & 0xFF;
    cd[23] = (size >> 24) & 0xFF;
    cd[24] = size & 0xFF;
    cd[25] = (size >> 8) & 0xFF;
    cd[26] = (size >> 16) & 0xFF;
    cd[27] = (size >> 24) & 0xFF;
    cd[28] = name_len & 0xFF;
    cd[29] = (name_len >> 8) & 0xFF;
    // extra field length = 0
    // file comment length = 0
    // disk number start = 0
    // internal file attributes = 0
    // external file attributes = 0
    cd[42] = local_offset & 0xFF;
    cd[43] = (local_offset >> 8) & 0xFF;
    cd[44] = (local_offset >> 16) & 0xFF;
    cd[45] = (local_offset >> 24) & 0xFF;
    fwrite(cd, 1, 46, f);
    fwrite(name, 1, name_len, f);
}

static void zip_write_end(FILE *f, uint32_t cd_offset, uint32_t cd_size, uint16_t entries)
{
    uint8_t eocd[22] = {0};
    eocd[0] = 'P';
    eocd[1] = 'K';
    eocd[2] = 5;
    eocd[3] = 6;        // signature
    eocd[4] = 0;
    eocd[5] = 0;        // disk number
    eocd[6] = 0;
    eocd[7] = 0;        // disk of cd
    eocd[8] = entries & 0xFF;
    eocd[9] = (entries >> 8) & 0xFF;
    eocd[10] = entries & 0xFF;
    eocd[11] = (entries >> 8) & 0xFF;
    eocd[12] = cd_size & 0xFF;
    eocd[13] = (cd_size >> 8) & 0xFF;
    eocd[14] = (cd_size >> 16) & 0xFF;
    eocd[15] = (cd_size >> 24) & 0xFF;
    eocd[16] = cd_offset & 0xFF;
    eocd[17] = (cd_offset >> 8) & 0xFF;
    eocd[18] = (cd_offset >> 16) & 0xFF;
    eocd[19] = (cd_offset >> 24) & 0xFF;
    // comment length = 0
    fwrite(eocd, 1, 22, f);
}

static uint16_t dos_time_now(void)
{
    time_t t = time(NULL);
    struct tm local_tm;
    if (localtime_s(&local_tm, &t) != 0) return 0;
    return (uint16_t)((local_tm.tm_sec / 2) | (local_tm.tm_min << 5) | (local_tm.tm_hour << 11));
}

static uint16_t dos_date_now(void)
{
    time_t t = time(NULL);
    struct tm local_tm;
    if (localtime_s(&local_tm, &t) != 0) return 0;
    return (uint16_t)(local_tm.tm_mday | ((local_tm.tm_mon + 1) << 5) | ((local_tm.tm_year - 80) << 9));
}

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static void ods_escape_xml(FILE *f, const char *texto)
{
    if (!texto)
    {
        return;
    }
    for (const char *p = texto; *p; p++)
    {
        switch (*p)
        {
        case '&':
            fprintf(f, "&amp;");
            break;
        case '<':
            fprintf(f, "&lt;");
            break;
        case '>':
            fprintf(f, "&gt;");
            break;
        case '"':
            fprintf(f, "&quot;");
            break;
        case '\'':
            fprintf(f, "&apos;");
            break;
        default:
            fputc(*p, f);
        }
    }
}

static void ods_escribir_fila_texto(FILE *f, const char *celdas[], int num_celdas)
{
    fprintf(f, "  <table:table-row>\n");
    for (int i = 0; i < num_celdas; i++)
    {
        fprintf(f, "   <table:table-cell office:value-type=\"string\"><text:p>");
        ods_escape_xml(f, celdas[i] ? celdas[i] : "");
        fprintf(f, "</text:p></table:table-cell>\n");
    }
    fprintf(f, "  </table:table-row>\n");
}

static int ods_exportar_tabla(const char *nombre_hoja, const char *sql,
                              const char *encabezados[], int num_cols)
{
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_%ld.ods", nombre_hoja, (long)time(NULL));
    char *path = get_export_path(filename);

    if (!path || !path[0])
    {
        printf("Error al obtener ruta de exportacion.\n");
        return 0;
    }

    FILE *f = NULL;
    if (fopen_s(&f, path, "w") != 0 || !f)
    {
        printf("Error al crear archivo: %s\n", path);
        return 0;
    }

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<office:document-content "
            "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
            "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
            "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\">\n");
    fprintf(f, "<office:body><office:spreadsheet>\n");
    fprintf(f, " <table:table table:name=\"%s\">\n", nombre_hoja);

    ods_escribir_fila_texto(f, encabezados, num_cols);

    sqlite3_stmt *stmt;
    if (preparar_stmt(sql, &stmt))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            fprintf(f, "  <table:table-row>\n");
            for (int i = 0; i < num_cols; i++)
            {
                const char *val = (const char*)sqlite3_column_text(stmt, i);
                fprintf(f, "   <table:table-cell office:value-type=\"string\"><text:p>");
                ods_escape_xml(f, val ? val : "");
                fprintf(f, "</text:p></table:table-cell>\n");
            }
            fprintf(f, "  </table:table-row>\n");
        }
        sqlite3_finalize(stmt);
    }

    fprintf(f, " </table:table>\n");
    fprintf(f, "</office:spreadsheet></office:body>\n");
    fprintf(f, "</office:document-content>\n");
    fclose(f);

    printf("Exportado a: %s\n", path);
    app_log_event("EXPORT_ODS", "Exportacion ODS generada");
    return 1;
}

static int exportar_partidos_ods_local(void)
{
    const char *headers[] = {"Fecha", "Goles", "Asistencias", "Resultado", "Clima", "Dia", "Cancha"};
    return ods_exportar_tabla("Partidos",
                              "SELECT p.fecha_hora, p.goles, p.asistencias, p.resultado, p.clima, p.dia, can.nombre "
                              "FROM partido p LEFT JOIN cancha can ON p.cancha_id = can.id WHERE p.resultado > 0 ORDER BY p.fecha_hora DESC",
                              headers, 7);
}
static int exportar_equipos_ods_local(void)
{
    const char *headers[] = {"Nombre", "Pais", "Ciudad", "Estadio"};
    return ods_exportar_tabla("Equipos",
                              "SELECT nombre, pais, ciudad, estadio FROM equipo ORDER BY nombre",
                              headers, 4);
}
static int exportar_canchas_ods_local(void)
{
    const char *headers[] = {"Nombre", "Ciudad", "Capacidad"};
    return ods_exportar_tabla("Canchas",
                              "SELECT nombre, ciudad, capacidad FROM cancha ORDER BY nombre",
                              headers, 3);
}
static int exportar_camisetas_ods_local(void)
{
    const char *headers[] = {"Nombre", "Club", "Temporada", "Talle"};
    return ods_exportar_tabla("Camisetas",
                              "SELECT nombre, club, temporada, talle FROM camisetas ORDER BY nombre",
                              headers, 4);
}
static int exportar_lesiones_ods_local(void)
{
    const char *headers[] = {"Jugador", "Descripcion", "Fecha", "Gravedad"};
    return ods_exportar_tabla("Lesiones",
                              "SELECT j.nombre, l.descripcion, l.fecha, l.gravedad "
                              "FROM lesion l LEFT JOIN jugador j ON l.jugador_id = j.id ORDER BY l.fecha DESC",
                              headers, 4);
}
static int exportar_torneo_ods_local(void)
{
    const char *headers[] = {"Nombre", "Tipo", "Fecha Inicio", "Fecha Fin", "Estado"};
    return ods_exportar_tabla("Torneos",
                              "SELECT nombre, tipo, fecha_inicio, fecha_fin, estado FROM torneo ORDER BY nombre",
                              headers, 5);
}

int exportar_partidos_ods(void)
{
    return exportar_partidos_ods_local();
}
int exportar_equipos_ods(void)
{
    return exportar_equipos_ods_local();
}
int exportar_canchas_ods(void)
{
    return exportar_canchas_ods_local();
}
int exportar_camisetas_ods(void)
{
    return exportar_camisetas_ods_local();
}
int exportar_lesiones_ods(void)
{
    return exportar_lesiones_ods_local();
}
int exportar_torneo_ods(void)
{
    return exportar_torneo_ods_local();
}

static void menu_exportar_partidos_ods_fn(void)
{
    exportar_partidos_ods_local();
}
static void menu_exportar_equipos_ods_fn(void)
{
    exportar_equipos_ods_local();
}
static void menu_exportar_canchas_ods_fn(void)
{
    exportar_canchas_ods_local();
}
static void menu_exportar_camisetas_ods_fn(void)
{
    exportar_camisetas_ods_local();
}
static void menu_exportar_lesiones_ods_fn(void)
{
    exportar_lesiones_ods_local();
}
static void menu_exportar_torneo_ods_fn(void)
{
    exportar_torneo_ods_local();
}

void menu_exportar_ods(void)
{
    MenuItem items[] =
    {
        {1, "Exportar Partidos a ODS", &menu_exportar_partidos_ods_fn},
        {2, "Exportar Equipos a ODS", &menu_exportar_equipos_ods_fn},
        {3, "Exportar Canchas a ODS", &menu_exportar_canchas_ods_fn},
        {4, "Exportar Camisetas a ODS", &menu_exportar_camisetas_ods_fn},
        {5, "Exportar Lesiones a ODS", &menu_exportar_lesiones_ods_fn},
        {6, "Exportar Torneos a ODS", &menu_exportar_torneo_ods_fn},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR A ODS", items, 7);
}

//------------------------------------------------------------------------------
// XLSX export (ZIP-wrapped ODS XML)
//------------------------------------------------------------------------------

typedef struct
{
    uint8_t *data;
    size_t len;
    size_t cap;
} DynBuf;

static void dynbuf_init(DynBuf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static int dynbuf_write(DynBuf *b, const void *src, size_t n)
{
    if (n == 0) return 1;
    if (n > SIZE_MAX - b->len) return 0;
    size_t required = b->len + n;
    if (required > b->cap)
    {
        size_t newcap = b->cap ? b->cap : 65536;
        while (newcap < required)
        {
            if (newcap > SIZE_MAX / 2) return 0;
            newcap *= 2;
        }
        uint8_t *p = (uint8_t *)realloc(b->data, newcap);
        if (!p) return 0;
        b->data = p;
        b->cap = newcap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 1;
}

static int dynbuf_printf(DynBuf *b, const char *fmt, ...) // NOSONAR
{
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap); // NOSONAR
    va_end(ap);
    if (n < 0) return 0;
    if ((size_t)n >= sizeof(tmp)) n = (int)(sizeof(tmp) - 1);
    return dynbuf_write(b, tmp, (size_t)n);
}

static void dynbuf_free(DynBuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void xlsx_escape_xml_buf(DynBuf *b, const char *texto)
{
    if (!texto) return;
    for (const char *p = texto; *p; p++)
    {
        switch (*p)
        {
        case '&':
            dynbuf_write(b, "&amp;", 5);
            break;
        case '<':
            dynbuf_write(b, "&lt;", 4);
            break;
        case '>':
            dynbuf_write(b, "&gt;", 4);
            break;
        case '"':
            dynbuf_write(b, "&quot;", 6);
            break;
        case '\'':
            dynbuf_write(b, "&apos;", 6);
            break;
        default:
            dynbuf_write(b, p, 1);
            break;
        }
    }
}

static void xlsx_write_type_data(DynBuf *buf, sqlite3_stmt *stmt, int col)
{
    int col_type = sqlite3_column_type(stmt, col);
    if (col_type == SQLITE_INTEGER)
    {
        char num[32];
        snprintf(num, sizeof(num), "%d", sqlite3_column_int(stmt, col));
        dynbuf_write(buf, num, strnlen_s(num, (size_t)-1));
    }
    else if (col_type == SQLITE_FLOAT)
    {
        char num[64];
        snprintf(num, sizeof(num), "%.2f", sqlite3_column_double(stmt, col));
        dynbuf_write(buf, num, strnlen_s(num, (size_t)-1));
    }
    else if (col_type != SQLITE_NULL)
    {
        const char *txt = (const char*)sqlite3_column_text(stmt, col);
        xlsx_escape_xml_buf(buf, txt);
    }
}

static void xlsx_write_cell(DynBuf *buf, sqlite3_stmt *stmt, int col)
{
    dynbuf_write(buf, "    <Cell><Data ss:Type=\"", 25);
    int col_type = sqlite3_column_type(stmt, col);
    if (col_type == SQLITE_INTEGER || col_type == SQLITE_FLOAT)
        dynbuf_write(buf, "Number", 6);
    else
        dynbuf_write(buf, "String", 6);
    dynbuf_write(buf, "\">", 2);
    xlsx_write_type_data(buf, stmt, col);
    dynbuf_write(buf, "</Data></Cell>\n", 15);
}

static int xlsx_generar_xml(const char *nombre_hoja, const char *sql,
                            const char *encabezados[], int num_cols, DynBuf *buf)
{
    dynbuf_write(buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", 40);
    dynbuf_write(buf, "<?mso-application progid=\"Excel.Sheet\"?>\n", 43);
    dynbuf_write(buf, "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n"
                 " xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n", 117);
    dynbuf_printf(buf, " <Worksheet ss:Name=\"%s\">\n", nombre_hoja);
    dynbuf_write(buf, "  <Table>\n", 10);

    // Header row
    dynbuf_write(buf, "   <Row>\n", 9);
    for (int i = 0; i < num_cols; i++)
    {
        dynbuf_write(buf, "    <Cell><Data ss:Type=\"String\">", 34);
        xlsx_escape_xml_buf(buf, encabezados[i] ? encabezados[i] : "");
        dynbuf_write(buf, "</Data></Cell>\n", 15);
    }
    dynbuf_write(buf, "   </Row>\n", 10);

    // Data rows
    sqlite3_stmt *stmt;
    if (preparar_stmt(sql, &stmt))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            dynbuf_write(buf, "   <Row>\n", 9);
            for (int i = 0; i < num_cols; i++)
                xlsx_write_cell(buf, stmt, i);
            dynbuf_write(buf, "   </Row>\n", 10);
        }
        sqlite3_finalize(stmt);
    }

    dynbuf_write(buf, "  </Table>\n", 11);
    dynbuf_write(buf, " </Worksheet>\n", 14);
    dynbuf_write(buf, "</Workbook>\n", 12);

    return 1;
}

static int xlsx_exportar_tabla(const char *nombre_hoja, const char *sql,
                               const char *encabezados[], int num_cols)
{
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_%ld.xlsx", nombre_hoja, (long)time(NULL));
    char *path = get_export_path(filename);
    if (!path || !path[0])
    {
        printf("Error al obtener ruta de exportacion.\n");
        return 0;
    }

    DynBuf buf;
    dynbuf_init(&buf);
    if (!xlsx_generar_xml(nombre_hoja, sql, encabezados, num_cols, &buf))
    {
        dynbuf_free(&buf);
        return 0;
    }

    // Write ZIP wrapper
    FILE *f = NULL;
    if (fopen_s(&f, path, "wb") != 0 || !f)
    {
        printf("Error al crear archivo: %s\n", path);
        dynbuf_free(&buf);
        return 0;
    }

    uint32_t crc = crc32_buf(buf.data, buf.len);
    uint16_t dtime = dos_time_now();
    uint16_t ddate = dos_date_now();

    // Write ZIP (1 entry: content.xml inside xl/ folder)
    const char *entry_name = "xl/content.xml";
    uint32_t local_offset;
    zip_write_local_header(f, entry_name, crc, (uint32_t)buf.len, dtime, ddate, &local_offset);
    fwrite(buf.data, 1, buf.len, f);

    // [Content_Types].xml entry
    const char *types_name = "[Content_Types].xml";
    char types_xml[512];
    snprintf(types_xml, sizeof(types_xml),
             "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
             "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
             "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
             "<Override PartName=\"/xl/content.xml\" ContentType=\"application/vnd.ms-excel.sheet.macroEnabled.main+xml\"/>"
             "</Types>");
    uint32_t types_crc = crc32_buf((const uint8_t*)types_xml, strnlen_s(types_xml, (size_t)-1));
    uint32_t types_offset;
    zip_write_local_header(f, types_name, types_crc, (uint32_t)strnlen_s(types_xml, (size_t)-1), dtime, ddate, &types_offset);
    fwrite(types_xml, 1, strnlen_s(types_xml, (size_t)-1), f);

    // Central directory
    uint32_t cd_offset = (uint32_t)ftell(f);
    uint32_t cd_size = 0;

    zip_write_central_dir(f, entry_name, crc, (uint32_t)buf.len, dtime, ddate, local_offset);
    cd_size += 46 + (uint16_t)strnlen_s(entry_name, (size_t)-1);
    zip_write_central_dir(f, types_name, types_crc, (uint32_t)strnlen_s(types_xml, (size_t)-1), dtime, ddate, types_offset);
    cd_size += 46 + (uint16_t)strnlen_s(types_name, (size_t)-1);

    zip_write_end(f, cd_offset, cd_size, 2);

    fclose(f);
    dynbuf_free(&buf);

    printf("Exportado a: %s\n", path);
    app_log_event("EXPORT_XLSX", "Exportacion XLSX generada");
    return 1;
}

static int exportar_partidos_xlsx_local(void)
{
    const char *headers[] = {"Fecha", "Goles", "Asistencias", "Resultado", "Clima", "Cancha"};
    return xlsx_exportar_tabla("Partidos",
                               "SELECT p.fecha_hora, p.goles, p.asistencias, p.resultado, p.clima, can.nombre "
                               "FROM partido p LEFT JOIN cancha can ON p.cancha_id=can.id WHERE p.resultado>0 ORDER BY p.fecha_hora DESC",
                               headers, 6);
}

static int exportar_equipos_xlsx_local(void)
{
    const char *headers[] = {"Nombre", "Pais", "Ciudad", "Estadio"};
    return xlsx_exportar_tabla("Equipos",
                               "SELECT nombre, pais, ciudad, estadio FROM equipo ORDER BY nombre", headers, 4);
}

static int exportar_canchas_xlsx_local(void)
{
    const char *headers[] = {"Nombre", "Ciudad", "Capacidad"};
    return xlsx_exportar_tabla("Canchas",
                               "SELECT nombre, ciudad, capacidad FROM cancha ORDER BY nombre", headers, 3);
}

static int exportar_camisetas_xlsx_local(void)
{
    const char *headers[] = {"Nombre", "Club", "Temporada", "Talle"};
    return xlsx_exportar_tabla("Camisetas",
                               "SELECT nombre, club, temporada, talle FROM camisetas ORDER BY nombre", headers, 4);
}

static int exportar_lesiones_xlsx_local(void)
{
    const char *headers[] = {"Jugador", "Descripcion", "Fecha", "Gravedad"};
    return xlsx_exportar_tabla("Lesiones",
                               "SELECT j.nombre, l.descripcion, l.fecha, l.gravedad "
                               "FROM lesion l LEFT JOIN jugador j ON l.jugador_id=j.id ORDER BY l.fecha DESC", headers, 4);
}

static int exportar_torneo_xlsx_local(void)
{
    const char *headers[] = {"Nombre", "Tipo", "Fecha Inicio", "Fecha Fin", "Estado"};
    return xlsx_exportar_tabla("Torneos",
                               "SELECT nombre, tipo, fecha_inicio, fecha_fin, estado FROM torneo ORDER BY nombre", headers, 5);
}

int exportar_partidos_xlsx(void)
{
    return exportar_partidos_xlsx_local();
}
int exportar_equipos_xlsx(void)
{
    return exportar_equipos_xlsx_local();
}
int exportar_canchas_xlsx(void)
{
    return exportar_canchas_xlsx_local();
}
int exportar_camisetas_xlsx(void)
{
    return exportar_camisetas_xlsx_local();
}
int exportar_lesiones_xlsx(void)
{
    return exportar_lesiones_xlsx_local();
}
int exportar_torneo_xlsx(void)
{
    return exportar_torneo_xlsx_local();
}

static void menu_exportar_partidos_xlsx_fn(void)
{
    exportar_partidos_xlsx_local();
}
static void menu_exportar_equipos_xlsx_fn(void)
{
    exportar_equipos_xlsx_local();
}
static void menu_exportar_canchas_xlsx_fn(void)
{
    exportar_canchas_xlsx_local();
}
static void menu_exportar_camisetas_xlsx_fn(void)
{
    exportar_camisetas_xlsx_local();
}
static void menu_exportar_lesiones_xlsx_fn(void)
{
    exportar_lesiones_xlsx_local();
}
static void menu_exportar_torneo_xlsx_fn(void)
{
    exportar_torneo_xlsx_local();
}

void menu_exportar_xlsx(void)
{
    MenuItem items[] =
    {
        {1, "Exportar Partidos a XLSX", &menu_exportar_partidos_xlsx_fn},
        {2, "Exportar Equipos a XLSX", &menu_exportar_equipos_xlsx_fn},
        {3, "Exportar Canchas a XLSX", &menu_exportar_canchas_xlsx_fn},
        {4, "Exportar Camisetas a XLSX", &menu_exportar_camisetas_xlsx_fn},
        {5, "Exportar Lesiones a XLSX", &menu_exportar_lesiones_xlsx_fn},
        {6, "Exportar Torneos a XLSX", &menu_exportar_torneo_xlsx_fn},
        {0, "Volver", NULL}
    };
    ejecutar_menu("EXPORTAR A XLSX", items, 7);
}
