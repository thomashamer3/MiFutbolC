/**
 * @file ascii_charts.c
 * @brief Implementación de funciones para dibujar gráficos ASCII en consola
 *
 * Este módulo proporciona visualización gráfica de datos directamente en la
 * terminal usando caracteres ASCII y Unicode, sin dependencias externas.
 */

#include "ascii_charts.h"
#include "utils.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHART_WIDTH  200
#define MAX_CHART_HEIGHT 50
#define MAX_BINS         40
#define MAX_SPARKLINE    200
#define MAX_LABEL        40

/* ---------------------------------------------------------------
 * Helpers internos
 * --------------------------------------------------------------- */

/**
 * @brief Obtiene el carácter de bloque Unicode para sparkline
 * @param level Nivel 0..7
 * @return Cadena con el carácter Unicode
 */
static const char* sparkline_char(int level)
{
    static const char *blocks[] =
    {
        "\xE2\x96\x81", "\xE2\x96\x82", "\xE2\x96\x83", "\xE2\x96\x84",
        "\xE2\x96\x85", "\xE2\x96\x86", "\xE2\x96\x87", "\xE2\x96\x88"
    };
    if (level < 0) level = 0;
    if (level > 7) level = 7;
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

/* ---------------------------------------------------------------
 * chart_min / chart_max
 * --------------------------------------------------------------- */

double chart_min(const double *values, int count)
{
    if (!values || count <= 0) return 0.0;
    double m = values[0];
    for (int i = 1; i < count; i++)
        if (values[i] < m) m = values[i];
    return m;
}

double chart_max(const double *values, int count)
{
    if (!values || count <= 0) return 0.0;
    double m = values[0];
    for (int i = 1; i < count; i++)
        if (values[i] > m) m = values[i];
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

        if (bar_len < 0) bar_len = 0;
        if (bar_len > max_width) bar_len = max_width;

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

/* ---------------------------------------------------------------
 * Gráfico de líneas
 * --------------------------------------------------------------- */

void dibujar_grafico_lineas(const double *values, int count, const char *title,
                            int width, int height)
{
    if (!values || count <= 0 || !title || width <= 0 || height <= 0)
        return;
    if (width > MAX_CHART_WIDTH)  width = MAX_CHART_WIDTH;
    if (height > MAX_CHART_HEIGHT) height = MAX_CHART_HEIGHT;

    /* Escalar a 1 columna si hay un solo punto */
    if (count == 1) width = 1;

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

        /* Mapear: max_val → fila 0 (arriba), min_val → fila height-1 (abajo) */
        int r = round_to_int((max_val - val) / range * (height - 1));
        if (r < 0) r = 0;
        if (r >= height) r = height - 1;
        rows[c] = r;
    }

    ui_printf("\n%s\n\n", title);

    /* Etiqueta del eje Y (máximo) */
    char label_max[16], label_min[16];
    snprintf(label_max, sizeof(label_max), "%.1f", max_val);
    snprintf(label_min, sizeof(label_min), "%.1f", min_val);
    int label_w = (int)strlen(label_max);
    if ((int)strlen(label_min) > label_w)
        label_w = (int)strlen(label_min);

    /* Imprimir fila por fila, de arriba a abajo */
    for (int r = 0; r < height; r++)
    {
        char line[MAX_CHART_WIDTH + 1];
        memset(line, ' ', width);
        line[width] = '\0';

        for (int c = 0; c < width; c++)
        {
            if (rows[c] == r)
            {
                /* Elegir carácter de conexión según la dirección */
                if (c > 0 && c < width - 1)
                {
                    if (rows[c] > rows[c - 1] && rows[c] > rows[c + 1])
                        line[c] = '+';
                    else if (rows[c] > rows[c - 1])
                        line[c] = '\\';
                    else if (rows[c] > rows[c + 1])
                        line[c] = '/';
                    else if (rows[c] < rows[c - 1] && rows[c] < rows[c + 1])
                        line[c] = '+';
                    else if (rows[c] < rows[c - 1])
                        line[c] = '/';
                    else if (rows[c] < rows[c + 1])
                        line[c] = '\\';
                    else
                        line[c] = '-';
                }
                else if (c == 0)
                {
                    if (width > 1 && rows[c] < rows[c + 1])
                        line[c] = '\\';
                    else if (width > 1 && rows[c] > rows[c + 1])
                        line[c] = '/';
                    else
                        line[c] = '-';
                }
                else
                {
                    if (width > 1 && rows[c] > rows[c - 1])
                        line[c] = '\\';
                    else if (width > 1 && rows[c] < rows[c - 1])
                        line[c] = '/';
                    else
                        line[c] = '-';
                }
            }
        }

        /* Sobrescribir con '*' en las columnas que corresponden a datos originales */
        for (int i = 0; i < count; i++)
        {
            int c;
            if (count > 1)
                c = round_to_int((double)i / (count - 1) * (width - 1));
            else
                c = 0;

            if (c >= 0 && c < width && rows[c] == r)
                line[c] = '*';
        }

        /* Etiqueta Y */
        const char *yl = "";
        if (r == 0) yl = label_max;
        else if (r == height - 1) yl = label_min;

        ui_printf(" %*s | ", label_w, yl);
        for (int c = 0; c < width; c++)
            ui_putchar(line[c]);
        ui_printf("\n");
    }

    /* Eje X */
    ui_printf(" %*s +", label_w, "");
    print_line_char('-', width);
    ui_printf("\n");
    ui_printf(" %*s   0", label_w, "");
    if (count > 1)
        ui_printf("%*d", width - 1, count - 1);
    ui_printf("\n\n");
}

/* ---------------------------------------------------------------
 * Histograma
 * --------------------------------------------------------------- */

void dibujar_histograma(const double *values, int count, int bins,
                        const char *title)
{
    if (!values || count <= 0 || !title || bins <= 0)
        return;
    if (bins > MAX_BINS) bins = MAX_BINS;

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

    double bin_width = (max_val - min_val) / bins;

    /* Contar frecuencias */
    int freq[MAX_BINS];
    memset(freq, 0, sizeof(freq));

    for (int i = 0; i < count; i++)
    {
        int b = (int)((values[i] - min_val) / bin_width);
        if (b >= bins) b = bins - 1;
        if (b < 0) b = 0;
        freq[b]++;
    }

    int max_freq = 0;
    for (int i = 0; i < bins; i++)
        if (freq[i] > max_freq) max_freq = freq[i];

    if (max_freq == 0) max_freq = 1;

    int bar_max = MAX_CHART_WIDTH / 2;
    if (bar_max > 60) bar_max = 60;

    ui_printf("\n%s\n\n", title);
    ui_printf(" Rango                      | Frecuencia\n");
    ui_printf("----------------------------+");
    print_line_char('-', bar_max + 10);
    ui_printf("\n");

    for (int i = 0; i < bins; i++)
    {
        double lo = min_val + i * bin_width;
        double hi = lo + bin_width;

        char range_label[32];
        if (i == bins - 1)
            snprintf(range_label, sizeof(range_label), "[%.2f, %.2f]", lo, hi);
        else
            snprintf(range_label, sizeof(range_label), "[%.2f, %.2f)", lo, hi);

        int bar_len = round_to_int((double)freq[i] / max_freq * bar_max);
        if (bar_len < 0) bar_len = 0;

        ui_printf(" %-26s | %4d ", range_label, freq[i]);
        for (int j = 0; j < bar_len; j++)
            ui_printf("%s", bar_char);
        ui_printf("\n");
    }

    ui_printf("\n");
}

/* ---------------------------------------------------------------
 * Gráfico de pastel
 * --------------------------------------------------------------- */

void dibujar_grafico_pastel(const double *values, const char **labels,
                            int count, const char *title)
{
    if (!values || !labels || count <= 0 || !title)
        return;

    int usar_unicode = consola_soporta_unicode();
    const char *bar_char = usar_unicode ? "\xE2\x96\x88" : "#";
    const char *empty_char = " ";

    /* Calcular total */
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
    if (bar_max > 40) bar_max = 40;

    ui_printf("\n%s\n\n", title);

    for (int i = 0; i < count; i++)
    {
        if (values[i] <= 0.0) continue;

        double pct = (values[i] / total) * 100.0;
        int bar_len = round_to_int(values[i] / total * bar_max);
        if (bar_len < 0) bar_len = 0;
        if (bar_len > bar_max) bar_len = bar_max;

        char label_buf[MAX_LABEL];
        snprintf(label_buf, sizeof(label_buf), "%s", labels[i] ? labels[i] : "");

        ui_printf(" %-*s |", MAX_LABEL - 4, label_buf);

        for (int j = 0; j < bar_len; j++)
            ui_printf("%s", bar_char);
        for (int j = bar_len; j < bar_max; j++)
            ui_printf("%s", empty_char);

        ui_printf("| %5.1f%% (%.2f)\n", pct, values[i]);
    }

    ui_printf(" %-*s +", MAX_LABEL - 4, "");
    print_line_char('-', bar_max);
    ui_printf("+------\n");
    ui_printf(" %-*s  Total: %.2f\n\n", MAX_LABEL - 4, "", total);
}

/* ---------------------------------------------------------------
 * Minigráfico (sparkline)
 * --------------------------------------------------------------- */

void dibujar_minigrafico(const double *values, int count, int width)
{
    if (!values || count <= 0 || width <= 0) return;
    if (width > MAX_SPARKLINE) width = MAX_SPARKLINE;

    int usar_unicode = consola_soporta_unicode();

    double min_val = chart_min(values, count);
    double max_val = chart_max(values, count);

    /* Sin variación: nivel medio */
    double range = max_val - min_val;
    if (range == 0.0)
    {
        int lvl = usar_unicode ? 4 : 4;
        for (int i = 0; i < width; i++)
        {
            if (usar_unicode)
                ui_printf("%s", sparkline_char(lvl));
            else
                ui_putchar('~');
        }
        ui_printf("\n");
        return;
    }

    /* Muestrear/interpolar valores a las posiciones del sparkline */
    for (int c = 0; c < width; c++)
    {
        double t = (count > 1) ? (double)c / (width - 1) * (count - 1) : 0.0;
        int idx = (int)t;
        double frac = t - idx;

        double val;
        if (idx >= count - 1)
        {
            val = values[count - 1];
        }
        else
        {
            val = values[idx] * (1.0 - frac) + values[idx + 1] * frac;
        }

        /* Mapear a 8 niveles (0..7) */
        int lvl = round_to_int((val - min_val) / range * 7.0);
        if (lvl < 0) lvl = 0;
        if (lvl > 7) lvl = 7;

        if (usar_unicode)
            ui_printf("%s", sparkline_char(lvl));
        else
        {
            /* Alternativa ASCII con caracteres simples */
            static const char ascii_levels[] = "_.,-=+#";
            char ch = ascii_levels[lvl];
            ui_putchar(ch);
        }
    }
    ui_printf("\n");
}
