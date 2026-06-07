/**
 * @file ascii_charts.c
 * @brief Implementación de funciones para dibujar gráficos ASCII en consola
 *
 * Este módulo proporciona visualización gráfica de datos directamente en la
 * terminal usando caracteres ASCII y Unicode, sin dependencias externas.
 */

#include "ascii_charts.h"
#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MAX_CHART_WIDTH 200
#define MAX_CHART_HEIGHT 50
#define MAX_BINS 40
#define MAX_SPARKLINE 200
#define MAX_LABEL 40

/* ---------------------------------------------------------------
 * Helpers internos
 * --------------------------------------------------------------- */

/**
 * @brief Obtiene el carácter de bloque Unicode para sparkline
 * @param level Nivel 0..7
 * @return Cadena con el carácter Unicode
 */
static const char *sparkline_char(int level)
{
    static const char *blocks[] = {"\xE2\x96\x81", "\xE2\x96\x82", "\xE2\x96\x83",
                                   "\xE2\x96\x84", "\xE2\x96\x85", "\xE2\x96\x86",
                                   "\xE2\x96\x87", "\xE2\x96\x88"
                                  };
    if (level < 0)
        level = 0;
    if (level > 7)
        level = 7;
    return blocks[level];
}

/**
 * @brief Redondea un double a entero (mitad hacia arriba)
 */
static int round_to_int(double x)
{
    return (int)(x + 0.5);
}

/**
 * @brief Imprime una línea horizontal de caracteres
 */
static void print_line_char(char ch, int length)
{
    for (int i = 0; i < length; i++)
        ui_putchar(ch);
}

/**
 * @brief Elige el carácter de conexión para un punto en un gráfico de líneas
 * @param rows  Arreglo con la fila de cada columna
 * @param c     Columna actual
 * @param width Ancho total
 * @return Carácter a dibujar
 */
static char caracter_conexion_medio(int cur, int left, int right)
{
    if (cur > left && cur > right) return '+';
    if (cur > left) return '\\';
    if (cur > right) return '/';
    if (cur < left && cur < right) return '+';
    if (cur < left) return '/';
    if (cur < right) return '\\';
    return '-';
}

static char elegir_caracter_linea(const int *rows, int c, int width)
{
    if (width <= 1)
        return '-';

    if (c > 0 && c < width - 1)
        return caracter_conexion_medio(rows[c], rows[c - 1], rows[c + 1]);

    if (c == 0)
    {
        if (rows[c] < rows[c + 1]) return '\\';
        if (rows[c] > rows[c + 1]) return '/';
        return '-';
    }

    if (rows[c] > rows[c - 1]) return '\\';
    if (rows[c] < rows[c - 1]) return '/';
    return '-';
}

/* ---------------------------------------------------------------
 * chart_min / chart_max
 * --------------------------------------------------------------- */

double chart_min(const double *values, int count)
{
    if (!values || count <= 0)
        return 0.0;
    double m = values[0];
    for (int i = 1; i < count; i++)
        if (values[i] < m)
            m = values[i];
    return m;
}

double chart_max(const double *values, int count)
{
    if (!values || count <= 0)
        return 0.0;
    double m = values[0];
    for (int i = 1; i < count; i++)
        if (values[i] > m)
            m = values[i];
    return m;
}

/* ---------------------------------------------------------------
 * Gráfico de barras horizontal
 * --------------------------------------------------------------- */

void dibujar_grafico_barras(const double *values, const char **labels,
                            int count, const char *title, int max_width)
{
    if (!values || !labels || count <= 0 || !title || max_width <= 0)
        return;
    if (max_width > MAX_CHART_WIDTH)
        max_width = MAX_CHART_WIDTH;

    int usar_unicode = consola_soporta_unicode();
    const char *bar_char = usar_unicode ? "\xE2\x96\x88" : "#";

    ui_printf("\n%s\n\n", title);

    double min_val = chart_min(values, count);
    double max_val = chart_max(values, count);
    double range = max_val - min_val;

    for (int i = 0; i < count; i++)
    {
        int bar_len = 0;
        if (range > 0.0)
        {
            double ratio = (values[i] - min_val) / range;
            bar_len = round_to_int(ratio * max_width);
        }
        else
        {
            bar_len = max_width / 2;
        }

        if (bar_len < 0)
            bar_len = 0;
        if (bar_len > max_width)
            bar_len = max_width;

        /* Etiqueta truncada */
        char label_buf[MAX_LABEL];
        snprintf(label_buf, sizeof(label_buf), "%s", labels[i] ? labels[i] : "");

        ui_printf(" %-*s | ", MAX_LABEL - 4, label_buf);

        for (int j = 0; j < bar_len; j++)
            ui_printf("%s", bar_char);

        ui_printf(" %.2f\n", values[i]);
    }

    ui_printf("\n");
}

/**
 * @brief Interpola valores en columnas para el gráfico de líneas
 * @param rows    Arreglo de salida (filas para cada columna)
 * @param values  Datos originales
 * @param count   Número de datos originales
 * @param width   Ancho del gráfico en columnas
 * @param height  Alto del gráfico en filas
 * @param max_val Valor máximo del rango
 * @param range   Rango (max_val - min_val)
 */
static void calcular_rows_grafico_lineas(int *rows, const double *values,
        int count, int width, int height,
        double max_val, double range)
{
    for (int c = 0; c < width; c++)
    {
        double t = (count > 1) ? (double)c / (width - 1) * (count - 1) : 0.0;
        int idx = (int)t;
        double frac = t - idx;

        if (idx >= count - 1)
        {
            idx = count - 1;
            frac = 0.0;
        }

        double val = values[idx];
        if (count > 1 && idx < count - 1)
            val = values[idx] * (1.0 - frac) + values[idx + 1] * frac;

        int r = round_to_int((max_val - val) / range * (height - 1));
        if (r < 0)
            r = 0;
        if (r >= height)
            r = height - 1;
        rows[c] = r;
    }
}

/**
 * @brief Configuración de fila para gráfico de líneas
 */
typedef struct
{
    const int *rows;
    int width;
    int height;
    int count;
    int label_w;
    const char *label_max;
    const char *label_min;
} LineChartRowConfig;

/**
 * @brief Imprime una fila del gráfico de líneas
 */
static void imprimir_fila_grafico_lineas(int r, const LineChartRowConfig *cfg)
{
    char line[MAX_CHART_WIDTH + 1];
    memset(line, ' ', cfg->width);
    line[cfg->width] = '\0';

    for (int c = 0; c < cfg->width; c++)
    {
        if (cfg->rows[c] == r)
            line[c] = elegir_caracter_linea(cfg->rows, c, cfg->width);
    }

    for (int i = 0; i < cfg->count; i++)
    {
        int c;
        if (cfg->count > 1)
            c = round_to_int((double)i / (cfg->count - 1) * (cfg->width - 1));
        else
            c = 0;

        if (c >= 0 && c < cfg->width && cfg->rows[c] == r)
            line[c] = '*';
    }

    const char *yl = "";
    if (r == 0)
        yl = cfg->label_max;
    else if (r == cfg->height - 1)
        yl = cfg->label_min;

    ui_printf(" %*s | ", cfg->label_w, yl);
    for (int c = 0; c < cfg->width; c++)
        ui_putchar(line[c]);
    ui_printf("\n");
}

/* ---------------------------------------------------------------
 * Gráfico de líneas
 * --------------------------------------------------------------- */

void dibujar_grafico_lineas(const double *values, int count, const char *title,
                            int width, int height)
{
    if (!values || count <= 0 || !title || width <= 0 || height <= 0)
        return;
    if (width > MAX_CHART_WIDTH)
        width = MAX_CHART_WIDTH;
    if (height > MAX_CHART_HEIGHT)
        height = MAX_CHART_HEIGHT;

    /* Escalar a 1 columna si hay un solo punto */
    if (count == 1)
        width = 1;

    double min_val = chart_min(values, count);
    double max_val = chart_max(values, count);

    if (min_val == max_val)
    {
        if (max_val == 0.0)
        {
            min_val = -1.0;
            max_val = 1.0;
        }
        else
        {
            min_val *= 0.9;
            max_val *= 1.1;
        }
    }
    double range = max_val - min_val;

    /* Interpolar valores para cada columna */
    int rows[MAX_CHART_WIDTH];

    calcular_rows_grafico_lineas(rows, values, count, width, height, max_val, range);

    ui_printf("\n%s\n\n", title);

    /* Etiqueta del eje Y (máximo) */
    char label_max[16];
    char label_min[16];
    snprintf(label_max, sizeof(label_max), "%.1f", max_val);
    snprintf(label_min, sizeof(label_min), "%.1f", min_val);
    int label_w = (int)strnlen_s(label_max, (size_t)-1);
    if ((int)strnlen_s(label_min, (size_t)-1) > label_w)
        label_w = (int)strnlen_s(label_min, (size_t)-1);

    /* Imprimir fila por fila, de arriba a abajo */
    LineChartRowConfig lcfg =
    {
        .rows = rows,
        .width = width,
        .height = height,
        .count = count,
        .label_w = label_w,
        .label_max = label_max,
        .label_min = label_min
    };
    for (int r = 0; r < height; r++)
        imprimir_fila_grafico_lineas(r, &lcfg);

    /* Eje X */
    ui_printf(" %*s +", label_w, "");
    print_line_char('-', width);
    ui_printf("\n");
    ui_printf(" %*s   0", label_w, "");
    if (count > 1)
        ui_printf("%*d", width - 1, count - 1);
    ui_printf("\n\n");
}

/**
 * @brief Configuración de fila para histograma
 */
typedef struct
{
    int bins;
    double min_val;
    double bin_width;
    const int *freq;
    int max_freq;
    int bar_max;
    const char *bar_char;
} HistogramRowConfig;

/**
 * @brief Imprime una fila del histograma
 */
static void imprimir_fila_histograma(int i, const HistogramRowConfig *cfg)
{
    double lo = cfg->min_val + i * cfg->bin_width;
    double hi = lo + cfg->bin_width;

    char range_label[32];
    if (i == cfg->bins - 1)
        snprintf(range_label, sizeof(range_label), "[%.2f, %.2f]", lo, hi);
    else
        snprintf(range_label, sizeof(range_label), "[%.2f, %.2f)", lo, hi);

    int bar_len = round_to_int((double)cfg->freq[i] / cfg->max_freq * cfg->bar_max);
    if (bar_len < 0)
        bar_len = 0;

    ui_printf(" %-26s | %4d ", range_label, cfg->freq[i]);
    for (int j = 0; j < bar_len; j++)
        ui_printf("%s", cfg->bar_char);
    ui_printf("\n");
}

/* ---------------------------------------------------------------
 * Histograma
 * --------------------------------------------------------------- */

static void calcular_bins_histograma(const double *values, int count,
                                     double min_val, double max_val,
                                     int bins, int *freq, int *max_freq)
{
    double bin_width = (max_val - min_val) / bins;

    memset(freq, 0, sizeof(int) * bins);
    for (int i = 0; i < count; i++)
    {
        int b = (int)((values[i] - min_val) / bin_width);
        if (b >= bins) b = bins - 1;
        if (b < 0)     b = 0;
        freq[b]++;
    }

    *max_freq = 0;
    for (int i = 0; i < bins; i++)
        if (freq[i] > *max_freq)
            *max_freq = freq[i];

    if (*max_freq == 0)
        *max_freq = 1;
}

void dibujar_histograma(const double *values, int count, int bins,
                        const char *title)
{
    if (!values || count <= 0 || !title || bins <= 0)
        return;
    if (bins > MAX_BINS)
        bins = MAX_BINS;

    int usar_unicode = consola_soporta_unicode();
    const char *bar_char = usar_unicode ? "\xE2\x96\x88" : "#";

    double min_val = chart_min(values, count);
    double max_val = chart_max(values, count);

    if (min_val == max_val)
    {
        if (max_val == 0.0)
        {
            min_val = -1.0;
            max_val = 1.0;
        }
        else
        {
            min_val *= 0.9;
            max_val *= 1.1;
        }
    }

    int freq[MAX_BINS];
    int max_freq;
    calcular_bins_histograma(values, count, min_val, max_val, bins, freq, &max_freq);

    int bar_max = MAX_CHART_WIDTH / 2;
    if (bar_max > 60)
        bar_max = 60;

    ui_printf("\n%s\n\n", title);
    ui_printf(" Rango                      | Frecuencia\n");
    ui_printf("----------------------------+");
    print_line_char('-', bar_max + 10);
    ui_printf("\n");

    HistogramRowConfig hcfg =
    {
        .bins = bins,
        .min_val = min_val,
        .bin_width = (max_val - min_val) / bins,
        .freq = freq,
        .max_freq = max_freq,
        .bar_max = bar_max,
        .bar_char = bar_char
    };
    for (int i = 0; i < bins; i++)
        imprimir_fila_histograma(i, &hcfg);

    ui_printf("\n");
}

/* ---------------------------------------------------------------
 * Gráfico de pastel
 * --------------------------------------------------------------- */

static void imprimir_fila_pastel(double valor, double total,
                                 const char *label, int bar_max,
                                 const char *bar_char, const char *empty_char)
{
    double pct = (valor / total) * 100.0;
    int bar_len = round_to_int(valor / total * bar_max);
    if (bar_len < 0) bar_len = 0;
    if (bar_len > bar_max) bar_len = bar_max;

    char label_buf[MAX_LABEL];
    snprintf(label_buf, sizeof(label_buf), "%s", label ? label : "");

    ui_printf(" %-*s |", MAX_LABEL - 4, label_buf);
    for (int j = 0; j < bar_len; j++)
        ui_printf("%s", bar_char);
    for (int j = bar_len; j < bar_max; j++)
        ui_printf("%s", empty_char);
    ui_printf("| %5.1f%% (%.2f)\n", pct, valor);
}

void dibujar_grafico_pastel(const double *values, const char **labels,
                            int count, const char *title)
{
    if (!values || !labels || count <= 0 || !title)
        return;

    int usar_unicode = consola_soporta_unicode();
    const char *bar_char = usar_unicode ? "\xE2\x96\x88" : "#";
    const char *empty_char = " ";

    double total = 0.0;
    for (int i = 0; i < count; i++)
    {
        if (values[i] > 0.0)
            total += values[i];
    }

    if (total == 0.0)
    {
        ui_printf("\n%s\n  (sin datos)\n\n", title);
        return;
    }

    int bar_max = MAX_CHART_WIDTH / 3;
    if (bar_max > 40)
        bar_max = 40;

    ui_printf("\n%s\n\n", title);

    for (int i = 0; i < count; i++)
    {
        if (values[i] <= 0.0)
            continue;
        imprimir_fila_pastel(values[i], total, labels[i], bar_max,
                             bar_char, empty_char);
    }

    ui_printf(" %-*s +", MAX_LABEL - 4, "");
    print_line_char('-', bar_max);
    ui_printf("+------\n");
    ui_printf(" %-*s  Total: %.2f\n\n", MAX_LABEL - 4, "", total);
}

/* ---------------------------------------------------------------
 * Minigráfico (sparkline)
 * --------------------------------------------------------------- */

static void dibujar_sparkline_sin_variacion(int width, int usar_unicode)
{
    for (int i = 0; i < width; i++)
    {
        if (usar_unicode)
            ui_printf("%s", sparkline_char(4));
        else
            ui_putchar('~');
    }
    ui_printf("\n");
}

static void dibujar_sparkline_interpolar(const double *values, int count,
        int width, int usar_unicode)
{
    double min_val = chart_min(values, count);
    double max_val = chart_max(values, count);
    double range = max_val - min_val;

    for (int c = 0; c < width; c++)
    {
        double t = (count > 1) ? (double)c / (width - 1) * (count - 1) : 0.0;
        int idx = (int)t;
        double frac = t - idx;

        double val;
        if (idx >= count - 1)
            val = values[count - 1];
        else
            val = values[idx] * (1.0 - frac) + values[idx + 1] * frac;

        int lvl = round_to_int((val - min_val) / range * 7.0);
        if (lvl < 0) lvl = 0;
        if (lvl > 7) lvl = 7;

        if (usar_unicode)
            ui_printf("%s", sparkline_char(lvl));
        else
        {
            static const char ascii_levels[] = "_.,-=+#";
            ui_putchar(ascii_levels[lvl]);
        }
    }
    ui_printf("\n");
}

void dibujar_minigrafico(const double *values, int count, int width)
{
    if (!values || count <= 0 || width <= 0)
        return;
    if (width > MAX_SPARKLINE)
        width = MAX_SPARKLINE;

    int usar_unicode = consola_soporta_unicode();

    double min_val = chart_min(values, count);
    double max_val = chart_max(values, count);

    if (max_val - min_val == 0.0)
    {
        dibujar_sparkline_sin_variacion(width, usar_unicode);
        return;
    }

    dibujar_sparkline_interpolar(values, count, width, usar_unicode);
}
