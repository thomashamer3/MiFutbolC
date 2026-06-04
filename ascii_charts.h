/**
 * @file ascii_charts.h
 * @brief Funciones para dibujar gráficos ASCII en la consola de MiFutbolC
 *
 * Proporciona funciones para visualizar datos mediante gráficos de barras,
 * líneas, histogramas, gráficos de pastel y minigráficos (sparklines)
 * utilizando caracteres ASCII y Unicode.
 */

#ifndef ASCII_CHARTS_H
#define ASCII_CHARTS_H

/**
 * @brief Dibuja un gráfico de barras horizontal
 *
 * Muestra un título seguido de barras proporcionales al valor de cada elemento.
 * Cada barra se dibuja con el carácter '#' o bloque Unicode si la consola lo soporta.
 *
 * @param values Arreglo de valores numéricos
 * @param labels Arreglo de etiquetas para cada valor
 * @param count Número de elementos en los arreglos
 * @param title Título del gráfico
 * @param max_width Ancho máximo de cada barra en caracteres
 */
void dibujar_grafico_barras(const double *values, const char **labels,
                            int count, const char *title, int max_width);

/**
 * @brief Dibuja un gráfico de líneas ASCII
 *
 * Traza puntos con '*' y los conecta con líneas ('-', '/', '\') según la pendiente.
 * Incluye etiquetas en el eje Y con los valores mínimo y máximo.
 *
 * @param values Arreglo de valores numéricos
 * @param count Número de puntos
 * @param title Título del gráfico
 * @param width Ancho del área del gráfico en caracteres
 * @param height Alto del área del gráfico en caracteres
 */
void dibujar_grafico_lineas(const double *values, int count, const char *title,
                            int width, int height);

/**
 * @brief Dibuja un histograma de distribución de frecuencias
 *
 * Agrupa los valores en el número de intervalos (bins) especificado y
 * muestra una barra para cada intervalo indicando la frecuencia.
 *
 * @param values Arreglo de valores numéricos
 * @param count Número de valores
 * @param bins Número de intervalos (bins)
 * @param title Título del histograma
 */
void dibujar_histograma(const double *values, int count, int bins,
                        const char *title);

/**
 * @brief Dibuja un gráfico de pastel ASCII
 *
 * Representa cada valor como una porción proporcional del total usando
 * barras horizontales con caracteres de bloque. Muestra etiqueta y porcentaje.
 *
 * @param values Arreglo de valores numéricos
 * @param labels Arreglo de etiquetas para cada porción
 * @param count Número de porciones
 * @param title Título del gráfico
 */
void dibujar_grafico_pastel(const double *values, const char **labels,
                            int count, const char *title);

/**
 * @brief Dibuja un minigráfico (sparkline) en una línea
 *
 * Representa la tendencia de los valores usando caracteres de bloque Unicode
 * (▁▂▃▄▅▆▇█) en una sola línea de texto. Utiliza '_' si no hay soporte Unicode.
 *
 * @param values Arreglo de valores numéricos
 * @param count Número de valores
 * @param width Ancho del minigráfico en caracteres
 */
void dibujar_minigrafico(const double *values, int count, int width);

/**
 * @brief Encuentra el valor mínimo en un arreglo de doubles
 *
 * @param values Arreglo de valores numéricos
 * @param count Número de elementos
 * @return El valor mínimo, o 0.0 si count <= 0
 */
double chart_min(const double *values, int count);

/**
 * @brief Encuentra el valor máximo en un arreglo de doubles
 *
 * @param values Arreglo de valores numéricos
 * @param count Número de elementos
 * @return El valor máximo, o 0.0 si count <= 0
 */
double chart_max(const double *values, int count);

#endif /* ASCII_CHARTS_H */
