/**
 * @file utils.c
 * @brief Funciones utilitarias para entrada/salida, manejo de fechas y
 * operaciones de base de datos.
 *
 * Este archivo contiene funciones auxiliares para interactuar con el usuario,
 * manejar fechas y horas, limpiar la pantalla, verificar existencia de IDs en
 * la base de datos, y gestionar directorios de exportación.
 */

#include "utils.h"
#include "export.h"
#include "ascii_art.h"
#include "db.h"
#include "menu.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>
#include <stdarg.h>
#ifdef USE_NCURSES
#include <ncursesw/ncurses.h>
#endif
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#include <windows.h>
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#ifdef _WIN32
/* Función deshabilitada: evitar maximizar la consola automáticamente
 * Se deja como no-op para que el usuario pueda cerrar/reducir la ventana.
 */
void ensure_console_maximized_windows(void)
{
    /* Intencionalmente vacío */
}
#else
void ensure_console_maximized_windows(void)
{
    /* No-op en otros sistemas */
}
#endif

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    return 1;
}

static int ui_is_ncurses_active(void)
{
#ifdef USE_NCURSES
    return !isendwin();
#else
    return 0;
#endif
}

static void uppercase_ascii(const char *src, char *dst, size_t size)
{
    size_t i = 0;
    if (!dst || size == 0)
    {
        return;
    }
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < size)
    {
        unsigned char ch = (unsigned char)src[i];
        dst[i] = (char)toupper(ch);
        i++;
    }
    dst[i] = '\0';
}

#ifdef USE_NCURSES
static WINDOW *g_ui_output_win = NULL;

void ui_set_output_window(WINDOW *win)
{
    g_ui_output_win = win;
}

WINDOW *ui_get_output_window(void)
{
    return g_ui_output_win;
}
#endif

int ui_printf(const char *fmt, ...) // NOSONAR
{
    va_list args;
    va_start(args, fmt);
    char *formatted = sqlite3_vmprintf(fmt, args);
    va_end(args);

    if (!formatted)
    {
        return -1;
    }
#ifdef USE_NCURSES
    if (ui_is_ncurses_active())
    {
        WINDOW *target = g_ui_output_win ? g_ui_output_win : stdscr;
        int rc = wprintw(target, "%s", formatted);
        wrefresh(target);
        sqlite3_free(formatted);
        return rc;
    }
#endif
    int rc = printf("%s", formatted);
    sqlite3_free(formatted);
    return rc;
}

int ui_puts(const char *s)
{
#ifdef USE_NCURSES
    if (ui_is_ncurses_active())
    {
        WINDOW *target = g_ui_output_win ? g_ui_output_win : stdscr;
        int rc = wprintw(target, "%s\n", s ? s : "");
        wrefresh(target);
        return rc;
    }
#endif
    return puts(s ? s : "");
}

int ui_putchar(int c)
{
#ifdef USE_NCURSES
    if (ui_is_ncurses_active())
    {
        WINDOW *target = g_ui_output_win ? g_ui_output_win : stdscr;
        int rc = waddch(target, c);
        wrefresh(target);
        return rc;
    }
#endif
    return putchar(c);
}

int ui_printf_centered_line(const char *fmt, ...) // NOSONAR
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    char *formatted = sqlite3_vmprintf(fmt, args);
    va_end(args);

    if (!formatted)
    {
        return -1;
    }

    snprintf(buffer, sizeof(buffer), "%s", formatted);
    sqlite3_free(formatted);

#ifdef USE_NCURSES
    if (ui_is_ncurses_active())
    {
        WINDOW *target = g_ui_output_win ? g_ui_output_win : stdscr;
        int height;
        int width;
        int y;
        int x;
        getmaxyx(target, height, width);
        getyx(target, y, x);
        (void)height;
        (void)x;

        if (y >= height - 1 && wscrl(target, 1) == OK)
        {
            y = height - 1;
        }

        int len = (int)strlen(buffer);
        int start_x = (width - len) / 2;
        if (start_x < 0)
        {
            start_x = 0;
        }

        mvwprintw(target, y, start_x, "%s", buffer);
        wmove(target, y + 1, 0);
        wrefresh(target);
        return len;
    }
#endif

    return printf("%s\n", buffer);
}

static int ui_readline(char *buffer, int size)
{
    if (!buffer || size <= 0)
    {
        return 0;
    }

#ifdef USE_NCURSES
    if (ui_is_ncurses_active())
    {
        WINDOW *target = g_ui_output_win ? g_ui_output_win : stdscr;
        echo();
        int rc = wgetnstr(target, buffer, size - 1);
        noecho();
        if (rc == ERR)
        {
            buffer[0] = '\0';
            return 0;
        }
        return 1;
    }
#endif

    return fgets(buffer, size, stdin) != NULL;
}

/**
 * Permite la entrada de valores numéricos por parte del usuario,
 * facilitando la configuración de parámetros enteros en el sistema.
 */
int input_int(const char *msg)
{
    char buffer[64];
    int v = 0;
    int attempts = 0;

    while (attempts < 5)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, sizeof(buffer)))
        {
            attempts++;
            continue;
        }

        size_t len = strlen_s(buffer, sizeof(buffer));
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        {
            buffer[--len] = '\0';
        }

        char extra = '\0';
        if (sscanf_s(buffer, "%d %c", &v, &extra, (unsigned int)sizeof(extra)) == 1)
            return v;

        ui_printf("Entrada inválida. Intente nuevamente.\n");
        attempts++;
    }

    ui_printf("Se alcanzó el máximo de intentos.\n");
    return 0;
}

/* Implementación portable de safe_strnlen */
size_t safe_strnlen(const char *s, size_t maxlen)
{
    if (s == NULL)
        return 0;
    size_t i;
    for (i = 0; i < maxlen; i++)
    {
        if (s[i] == '\0')
            break;
    }
    return i;
}

#if !defined(__STDC_LIB_EXT1__)
size_t strlen_s(const char *s, size_t maxlen)
{
    return safe_strnlen(s, maxlen);
}
#endif

/**
 * Determina si un punto en la posición dada es un separador de miles o decimal.
 * Un punto se considera separador de miles si hay al menos 3 dígitos después de
 * él.
 */
static int is_thousands_separator(const char *buffer, int position)
{
    int remaining_digits = 0;

    // Contar dígitos después del punto actual
    for (int k = position + 1; buffer[k] != '\0'; k++)
    {
        if (isdigit(buffer[k]))
        {
            remaining_digits++;
        }
        else if (buffer[k] == ',' || buffer[k] == '.')
        {
            break;
        }
    }

    // Si hay al menos 3 dígitos después, es separador de miles
    return remaining_digits >= 3;
}

/**
 * Procesa un carácter individual y lo agrega al buffer de salida si es válido.
 * Devuelve 1 si se agregó un carácter, 0 si se omitió, -1 si se alcanzó el
 * límite.
 */
static int process_character(char c, char *output, size_t *j,
                             size_t output_size, int *has_decimal,
                             const char *input, int i)
{
    // Procesar coma como separador decimal
    if (c == ',' && !*has_decimal)
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar punto (puede ser separador de miles o decimal)
    if (c == '.' && !*has_decimal)
    {
        if (is_thousands_separator(input, i))
            return 0; // Omitir separador de miles
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar dígitos
    if (isdigit(c))
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = c;
        return 1;
    }

    // Ignorar otros caracteres
    return 0;
}

/**
 * Procesa un buffer de entrada para convertir separadores de miles y decimales
 * a formato estándar. Convierte comas a puntos decimales y elimina separadores
 * de miles.
 */
static void process_numeric_input(const char *input, char *output,
                                  size_t output_size)
{
    size_t j = 0;
    int has_decimal = 0;
    int ok = 1;

    if (input == NULL || output == NULL || output_size == 0)
    {
        if (output && output_size > 0)
            output[0] = '\0';
        return;
    }

    int i = 0;
    while (input[i] != '\0' && j < output_size - 1 && ok)
    {
        char c = input[i];

        ok = (process_character(c, output, &j, output_size, &has_decimal, input,
                                i) != -1);
        i++;
    }

    output[j] = '\0';
}

/**
 * Permite la entrada de valores de punto flotante por parte del usuario,
 * facilitando la configuración de parámetros decimales en el sistema.
 * Acepta tanto punto como coma como separador decimal, y maneja separadores de
 * miles.
 */
double input_double(const char *msg)
{
    char buffer[100];
    char processed[100];
    double v = 0.0;

    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, sizeof(buffer)))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        process_numeric_input(buffer, processed, sizeof(processed));

        if (sscanf_s(processed, "%lf", &v) == 1)
            return v;
        ui_printf("Entrada inválida. Ingrese un número válido (ej: 250, 1.500, "
                  "12.500, 250.000): ");
    }
}

/**
 * Valida la entrada de texto para asegurar la integridad de los datos y
 * prevenir errores en el procesamiento posterior, aceptando solo caracteres
 * alfanuméricos y espacios.
 */
void input_string(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        int valid = 1;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            if (!isalpha(buffer[i]) && !isspace(buffer[i]) && !isdigit(buffer[i]))
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            return;
        ui_printf("Entrada inválida. Solo se permiten letras, espacios y números.\n");
    }
}

void convert_display_date_to_storage(const char *display_date,
                                     char *storage_buffer, int buffer_size);

static void get_current_date(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y", &tm_struct);
}

static void get_current_time(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%H:%M", &tm_struct);
}

static void get_current_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

static int es_hora_sola(const char *buffer)
{
    return strchr(buffer, ':') != NULL && strchr(buffer, '/') == NULL && strchr(buffer, '-') == NULL;
}

static int msg_pide_datetime(const char *msg)
{
    if (!msg)
        return 0;
    return (strstr(msg, "HH:MM") || strstr(msg, "hh:mm")) &&
           (strstr(msg, "DD") || strstr(msg, "dd") || strstr(msg, "YYYY") || strstr(msg, "AAAA") || strstr(msg, "/"));
}

static int msg_pide_hora(const char *msg)
{
    if (!msg)
        return 0;
    return (strstr(msg, "HH:MM") || strstr(msg, "hh:mm")) && !msg_pide_datetime(msg);
}

static void completar_fecha_por_defecto(const char *msg, char *buffer, int size)
{
    if (msg_pide_datetime(msg))
    {
        get_current_datetime(buffer, size);
        return;
    }

    if (msg_pide_hora(msg))
    {
        get_current_time(buffer, size);
        return;
    }

    get_current_date(buffer, size);
}

static int validar_fecha_chars(const char *buffer)
{
    for (int i = 0; buffer[i] != '\0'; i++)
    {
        if (!isdigit(buffer[i]) && buffer[i] != '/' && buffer[i] != ':' && buffer[i] != ' ' && buffer[i] != '-')
        {
            return 0;
        }
    }
    return 1;
}

static int procesar_input_date(const char *msg, char *buffer, int size)
{
    if (safe_strnlen(buffer, (size_t)size) == 0)
    {
        completar_fecha_por_defecto(msg, buffer, size);
    }

    if (!validar_fecha_chars(buffer))
    {
        ui_printf("Entrada inválida. Solo se permiten dígitos, barras diagonales (/), "
                  "guiones (-) y dos puntos (:).\n");
        return 0;
    }

    if (!es_hora_sola(buffer) && strchr(buffer, '/') != NULL)
    {
        char storage[64];
        convert_display_date_to_storage(buffer, storage, sizeof(storage));
        strncpy_s(buffer, (size_t)size, storage, (size_t)size - 1);
    }

    return 1;
}

/**
 * Valida la entrada de fecha para asegurar el formato correcto,
 * aceptando solo dígitos, barras diagonales (/), guiones (-) y dos puntos (:).
 */
void input_date(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        if (procesar_input_date(msg, buffer, size))
        {
            return;
        }
    }
}

/**
 * Proporciona una representación legible de la fecha y hora actual para mostrar
 * en interfaces de usuario, mejorando la experiencia al contextualizar acciones
 * con el tiempo.
 */
void get_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

/**
 * Genera un identificador temporal compacto para usar en nombres de archivos,
 * asegurando unicidad y orden cronológico en exportaciones y backups.
 */
void get_timestamp(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%Y%m%d_%H%M", &tm_struct);
}

/**
 * Verifica la existencia de registros en la base de datos para mantener la
 * integridad referencial y evitar operaciones inválidas que puedan corromper
 * los datos.
 */
int existe_id(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];

    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s WHERE id=?", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, id);

    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

/**
 * Limpia la pantalla de la consola para proporcionar una interfaz limpia y
 * organizada, mejorando la legibilidad de la información mostrada.
 */
void clear_screen()
{
#ifdef USE_NCURSES
    if (!isendwin())
    {
        if (g_ui_output_win)
        {
            werase(g_ui_output_win);
            wmove(g_ui_output_win, 0, 0);
            wrefresh(g_ui_output_win);
        }
        else
        {
            clear();
            refresh();
        }
        return;
    }
#endif
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/**
 * Muestra información contextual del usuario y fecha para personalizar la
 * experiencia y registrar el momento de las operaciones, incluyendo arte ASCII
 * contextual.
 */
static const char *obtener_ascii_por_titulo(const char *titulo)
{
    if (strstr(titulo, "MI FUTBOL C"))
        return ASCII_BIENVENIDA;
    if (strstr(titulo, "CAMISETA") || strstr(titulo, "CAMISETAS"))
        return ASCII_CAMISETA;
    if (strstr(titulo, "CANCHAS"))
        return ASCII_CANCHA;
    if (strstr(titulo, "PARTIDO") || strstr(titulo, "PARTIDOS"))
        return ASCII_FUTBOL;
    if (strstr(titulo, "EQUIPOS"))
        return ASCII_EQUIPO;
    if (strstr(titulo, "ESTADISTICA") || strstr(titulo, "ESTADISTICAS"))
        return ASCII_ESTADISTICAS;
    if (strstr(titulo, "LOGROS"))
        return ASCII_LOGROS;
    if (strstr(titulo, "ANALISIS") || strstr(titulo, "EVOLUCION TEMPORAL"))
        return ASCII_ANALISIS;
    if (strstr(titulo, "LESIONES"))
        return ASCII_LESIONES;
    if (strstr(titulo, "FINANCIAMIENTO"))
        return ASCII_FINANCIAMIENTO;
    if (strstr(titulo, "EXPORTAR"))
        return ASCII_EXPORTAR;
    if (strstr(titulo, "IMPORTAR"))
        return ASCII_IMPORTAR;
    if (strstr(titulo, "TORNEOS"))
        return ASCII_TORNEOS;
    if (strstr(titulo, "AJUSTES") || strstr(titulo, "SETTINGS"))
        return ASCII_AJUSTES;
    if (strstr(titulo, "QR"))
        return ASCII_QR;
    if (strstr(titulo, "TEMPORADA") || strstr(titulo, "SEASON"))
        return ASCII_TEMPORADA;
    if (strstr(titulo, "ENTRENADOR IA"))
        return ASCII_ENTRENADOR_IA;
    return NULL;
}

#ifdef USE_NCURSES
static int ncurses_count_lines_util(const char *text)
{
    int lines = 1;
    for (const char *p = text; *p; ++p)
    {
        if (*p == '\n')
        {
            lines++;
        }
    }
    return lines;
}

static void ncurses_print_centered_lines_util(int start_y, const char *text)
{
    int height;
    int width;
    getmaxyx(stdscr, height, width);

    const char *line_start = text;
    int row = start_y;
    while (*line_start && row < height)
    {
        const char *line_end = strchr(line_start, '\n');
        int len = line_end ? (int)(line_end - line_start) : (int)strlen(line_start);
        int x = (width - len) / 2;
        if (x < 0)
        {
            x = 0;
        }
        mvprintw(row, x, "%.*s", len, line_start);

        row++;
        if (!line_end)
        {
            break;
        }
        line_start = line_end + 1;
    }
}

static void free_nombre_usuario_if_needed(char *nombre_usuario)
{
    if (nombre_usuario && strcmp(nombre_usuario, "Usuario Desconocido") != 0)
    {
        free(nombre_usuario);
    }
}

static void ncurses_print_header_window(WINDOW *target, const char *titulo_display,
                                        const char *nombre_usuario,
                                        const char *fecha, int mostrar_datos)
{
    int width = getmaxx(target);
    int height = getmaxy(target);
    int row = 0;

    werase(target);
    if (titulo_display && row < height)
    {
        int x = (width - (int)strlen(titulo_display)) / 2;
        if (x < 0)
        {
            x = 0;
        }
        mvwprintw(target, row, x, "%s", titulo_display);
    }

    if (mostrar_datos)
    {
        row++;
        if (row < height)
        {
            mvwprintw(target, row, 0, " Usuario: %s", nombre_usuario);
        }
        row++;
        if (row < height)
        {
            mvwprintw(target, row, 0, " Fecha  : %s", fecha);
        }
    }

    row++;
    if (row < height)
    {
        mvwhline(target, row, 0, '=', width > 0 ? width : 0);
    }
    row++;
    if (row < height)
    {
        wmove(target, row, 0);
    }

    wrefresh(target);
}

static int ncurses_setup_temp_screen(void)
{
    if (!isendwin())
    {
        return 0;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors())
    {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_WHITE, -1);
        init_pair(3, COLOR_YELLOW, COLOR_BLUE);
    }

    return 1;
}

static void ncurses_print_header_stdscr(const char *ascii, const char *titulo_display,
                                        const char *nombre_usuario,
                                        const char *fecha, int mostrar_datos)
{
    clear();
    if (ascii)
    {
        int art_lines = ncurses_count_lines_util(ascii);
        ncurses_print_centered_lines_util(0, ascii);
        move(art_lines + 1, 0);
    }

    printw("%s\n", titulo_display);
    if (mostrar_datos)
    {
        printw(" Usuario: %s\n", nombre_usuario);
        printw(" Fecha  : %s\n", fecha);
    }
    if (has_colors())
    {
        attron(COLOR_PAIR(3));
    }
    printw("========================================\n\n");
    if (has_colors())
    {
        attroff(COLOR_PAIR(3));
    }

    refresh();
}
#endif

static void print_header_stdout(const char *ascii, const char *titulo_display,
                                const char *nombre_usuario, const char *fecha,
                                int mostrar_datos)
{
    if (ascii)
    {
        printf("%s\n", ascii);
    }

    printf("%s\n", titulo_display);
    if (mostrar_datos)
    {
        printf(" Usuario: %s\n", nombre_usuario);
        printf(" Fecha  : %s\n", fecha);
    }
    printf("========================================\n\n");
}

void print_header(const char *titulo)
{
    char fecha[20];
    get_datetime(fecha, sizeof(fecha));
    char titulo_buf[128];
    const char *titulo_display;

    char *nombre_usuario = get_user_name();
    if (!nombre_usuario)
    {
        nombre_usuario = "Usuario Desconocido";
    }

    uppercase_ascii(titulo, titulo_buf, sizeof(titulo_buf));
    titulo_display = titulo ? titulo_buf : "";
    const char *ascii = obtener_ascii_por_titulo(titulo);
    int mostrar_datos = 1;
    if (titulo && strstr(titulo, "LISTADO") != NULL)
    {
        mostrar_datos = 0;
    }

#ifdef USE_NCURSES
    if (ui_is_ncurses_active() && g_ui_output_win)
    {
        ncurses_print_header_window(g_ui_output_win, titulo_display, nombre_usuario,
                                    fecha, mostrar_datos);
        free_nombre_usuario_if_needed(nombre_usuario);
        return;
    }

    int temp_initscr = ncurses_setup_temp_screen();

    if (!isendwin())
    {
        ncurses_print_header_stdscr(ascii, titulo_display, nombre_usuario, fecha,
                                    mostrar_datos);

        if (temp_initscr)
        {
            endwin();
        }

        free_nombre_usuario_if_needed(nombre_usuario);
        return;
    }
#endif

    print_header_stdout(ascii, titulo_display, nombre_usuario, fecha,
                        mostrar_datos);
    free_nombre_usuario_if_needed(nombre_usuario);
}

/**
 * Pausa la ejecución para permitir al usuario revisar información antes de
 * continuar, mejorando la interacción controlada.
 */
void pause_console()
{
    ui_printf("\nPresione ENTER para continuar...");
#ifdef USE_NCURSES
    if (ui_is_ncurses_active())
    {
        int ch;
        do
        {
            ch = getch();
        }
        while (ch != '\n' && ch != KEY_ENTER);
        return;
    }
#endif
    getchar();
}

/**
 * Solicita confirmación binaria del usuario para operaciones críticas,
 * previniendo acciones accidentales que puedan afectar datos.
 */
int confirmar(const char *msg)
{
    int c;
    ui_printf("%s (S/N): ", msg);
#ifdef USE_NCURSES
    if (ui_is_ncurses_active())
    {
        c = getch();
        return (c == 's' || c == 'S');
    }
#endif
    c = getchar();
    getchar();

    return (c == 's' || c == 'S');
}

static void leer_nombre_no_vacio(const char *prompt, const char *prompt_vacio,
                                 char *nombre, int size)
{
    ui_printf("%s", prompt);

    ui_readline(nombre, size);
    nombre[strcspn(nombre, "\n")] = 0;

    while (safe_strnlen(nombre, (size_t)size) == 0)
    {
        ui_printf("%s", prompt_vacio);
        ui_readline(nombre, size);
        nombre[strcspn(nombre, "\n")] = 0;
    }
}

/**
 * Recopila la identidad del usuario en el inicio para personalizar la
 * aplicación y mantener un registro de uso.
 */
void pedir_nombre_usuario()
{
    char nombre[100];
    clear_screen();
    ui_printf("%s\n", ASCII_BIENVENIDA);
    leer_nombre_no_vacio("Por favor, ingresa tu Nombre: ",
                         "El nombre no puede estar vacio. Ingresa tu nombre: ",
                         nombre, (int)sizeof(nombre));

    if (set_user_name(nombre))
    {
        ui_printf("!Bienvenido, %s!\n", nombre);
    }
    else
    {
        ui_printf("Error al guardar el nombre. Intenta nuevamente.\n");
    }
    pause_console();
}

/**
 * Permite al usuario verificar su identidad actual almacenada,
 * facilitando la gestión de su perfil.
 */
void mostrar_nombre_usuario()
{
    char *nombre = get_user_name();
    if (nombre)
    {
        ui_printf("Tu nombre actual es: %s\n", nombre);
        free(nombre);
    }
    else
    {
        ui_printf("No se pudo obtener el nombre del usuario.\n");
    }
    pause_console();
}

/**
 * Habilita la actualización de la identidad del usuario para mantener la
 * información actualizada y personalizada.
 */
void editar_nombre_usuario()
{
    char nombre[100];
    leer_nombre_no_vacio("Ingresa tu nuevo nombre: ",
                         "El nombre no puede estar vacio. Ingresa tu nuevo nombre: ",
                         nombre, (int)sizeof(nombre));

    if (set_user_name(nombre))
    {
        ui_printf("Nombre actualizado exitosamente a: %s\n", nombre);
    }
    else
    {
        ui_printf("Error al actualizar el nombre.\n");
    }
    pause_console();
}

/**
 * Proporciona una interfaz estructurada para gestionar opciones relacionadas
 * con el perfil del usuario.
 */
void menu_usuario()
{
    MenuItem items[] = {{1, "Mostrar Nombre", mostrar_nombre_usuario},
        {2, "Editar Nombre", editar_nombre_usuario},
        {0, "Volver", NULL}
    };

    ejecutar_menu("USUARIO", items, 3);
}

/**
 * Adapta fechas del almacenamiento interno a un formato amigable para la
 * visualización, permitiendo flexibilidad en formatos futuros.
 */
void format_date_for_display(const char *input_date, char *output_buffer,
                             int buffer_size)
{
    if (!input_date || buffer_size <= 0)
        return;

    if (safe_strnlen(input_date, (size_t)buffer_size) >= 10 && input_date[4] == '-' && input_date[7] == '-')
    {
        char fecha[16];
        fecha[0] = input_date[8];
        fecha[1] = input_date[9];
        fecha[2] = '/';
        fecha[3] = input_date[5];
        fecha[4] = input_date[6];
        fecha[5] = '/';
        fecha[6] = input_date[0];
        fecha[7] = input_date[1];
        fecha[8] = input_date[2];
        fecha[9] = input_date[3];
        fecha[10] = '\0';

        if (input_date[10] == ' ')
        {
            snprintf(output_buffer, (size_t)buffer_size, "%s%s", fecha, input_date + 10);
        }
        else
        {
            strncpy_s(output_buffer, buffer_size, fecha, buffer_size - 1);
        }
        return;
    }

    strncpy_s(output_buffer, buffer_size, input_date, buffer_size - 1);
}

/**
 * Convierte fechas ingresadas por el usuario a un formato interno consistente,
 * facilitando el almacenamiento y procesamiento uniforme.
 */
void convert_display_date_to_storage(const char *display_date,
                                     char *storage_buffer, int buffer_size)
{
    if (!display_date || buffer_size <= 0)
        return;

    if (strchr(display_date, '/') != NULL && safe_strnlen(display_date, (size_t)buffer_size) >= 10)
    {
        char yyyy[5] = {display_date[6], display_date[7], display_date[8], display_date[9], '\0'};
        char mm[3] = {display_date[3], display_date[4], '\0'};
        char dd[3] = {display_date[0], display_date[1], '\0'};

        if (display_date[10] == ' ')
        {
            snprintf(storage_buffer, (size_t)buffer_size, "%s-%s-%s%s", yyyy, mm, dd, display_date + 10);
        }
        else
        {
            snprintf(storage_buffer, (size_t)buffer_size, "%s-%s-%s", yyyy, mm, dd);
        }
        return;
    }

    strncpy_s(storage_buffer, buffer_size, display_date, buffer_size - 1);
}

/**
 * Normaliza cadenas de texto removiendo caracteres acentuados para asegurar
 * compatibilidad con sistemas que no los soportan y mejorar la consistencia en
 * búsquedas.
 */
char *remover_tildes(const char *str)
{
    static char buffer[256];
    size_t j = 0;

    const char *p = str;
    while (*p != '\0' && j < sizeof(buffer) - 1)
    {
        unsigned char c = (unsigned char)*p++;
        if (c == 0xE1 || c == 0xC1)
            buffer[j++] = 'a'; // á, Á
        else if (c == 0xE9 || c == 0xC9)
            buffer[j++] = 'e'; // é, É
        else if (c == 0xED || c == 0xCD)
            buffer[j++] = 'i'; // í, Í
        else if (c == 0xF3 || c == 0xD3)
            buffer[j++] = 'o'; // ó, Ó
        else if (c == 0xFA || c == 0xDA)
            buffer[j++] = 'u'; // ú, Ú
        else if (c == 0xF1 || c == 0xD1)
            buffer[j++] = 'n'; // ñ, Ñ
        else if (c == 0xFC || c == 0xDC)
            buffer[j++] = 'u'; // ü, Ü
        else
            buffer[j++] = (char)c;
    }
    buffer[j] = '\0';
    return buffer;
}

/**
 * Convierte un valor de resultado a texto
 *
 * @param resultado El valor numérico del resultado
 * @return La representación textual del resultado
 */
const char *resultado_to_text(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "VICTORIA";
    case 2:
        return "EMPATE";
    case 3:
        return "DERROTA";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Convierte un valor de clima a texto
 *
 * @param clima El valor numérico del clima
 * @return La representación textual del clima
 */
const char *clima_to_text(int clima)
{
    switch (clima)
    {
    case 1:
        return "Despejado";
    case 2:
        return "Nublado";
    case 3:
        return "Lluvia";
    case 4:
        return "Ventoso";
    case 5:
        return "Mucho Calor";
    case 6:
        return "Mucho Frio";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Convierte un valor de día a texto
 *
 * @param dia El valor numérico del día
 * @return La representación textual del día
 */
const char *dia_to_text(int dia)
{
    switch (dia)
    {
    case 1:
        return "Dia";
    case 2:
        return "Tarde";
    case 3:
        return "Noche";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Obtiene el nombre de una entidad por su ID desde la base de datos.
 * Función genérica para evitar duplicación de código en consultas SQL comunes.
 *
 * @param tabla Nombre de la tabla (ej: "camiseta", "torneo", "cancha")
 * @param id ID de la entidad a buscar
 * @param buffer Buffer donde se almacenará el nombre encontrado
 * @param size Tamaño máximo del buffer
 * @return 1 si se encontró la entidad, 0 si no se encontró
 */
int obtener_nombre_entidad(const char *tabla, int id, char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !buffer || size == 0)
    {
        return 0;
    }

    snprintf(sql, sizeof(sql), "SELECT nombre FROM %s WHERE id = ?", tabla);

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        if (nombre)
        {
            snprintf(buffer, size, "%s", nombre);
            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

/**
 * @brief Obtiene el siguiente ID disponible para una tabla
 * Implementa el patrón usado en camiseta, cancha, lesion, etc.
 */
long long obtener_siguiente_id(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[512];

    if (!tabla) return 1;

    /*
     * Esta consulta utiliza una CTE (Common Table Expression) recursiva para generar
     * una secuencia de números y encontrar el primer "hueco" (ID faltante) en la tabla.
     * Esto permite reutilizar IDs de registros eliminados manteniendo la secuencia compacta.
     */
    snprintf(sql, sizeof(sql),
             "WITH RECURSIVE seq(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM seq WHERE id < (SELECT COALESCE(MAX(id),0)+1 FROM %s)) "
             "SELECT MIN(id) FROM seq WHERE id NOT IN (SELECT id FROM %s)",
             tabla, tabla);

    if (!preparar_stmt(sql, &stmt))
    {
        return 1;
    }

    int id = 1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief Verifica si hay registros en una tabla
 */
int hay_registros(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla) return 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return count > 0;
}

/**
 * @brief Obtiene el ID de una entidad por nombre
 */
int obtener_id_por_nombre(const char *tabla, const char *nombre)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !nombre) return -1;

    snprintf(sql, sizeof(sql), "SELECT id FROM %s WHERE nombre = ?", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return id;
}

/**
 * @brief Lista todas las entidades de una tabla
 */
void listar_entidades(const char *tabla, const char *titulo, const char *mensaje_vacio)
{
    if (!tabla || !titulo || !mensaje_vacio) return;

    clear_screen();
    print_header(titulo);

    sqlite3_stmt *stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, nombre FROM %s ORDER BY id", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d - %s",
                                sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        ui_printf_centered_line("%s", mensaje_vacio);

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Solicita un entero validado en rango
 */
int input_int_rango(const char *msg, int min, int max)
{
    int valor;
    do
    {
        valor = input_int(msg);
        if (valor < min || valor > max)
            printf("Ingrese un valor entre %d y %d.\n", min, max);
    }
    while (valor < min || valor > max);
    return valor;
}

/**
 * @brief Muestra "no hay registros"
 */
void mostrar_no_hay_registros(const char *entidad)
{
    if (!entidad) return;
    size_t len = safe_strnlen(entidad, SIZE_MAX);
    printf("No hay %s registrad%s.\n", entidad,
           (entidad[len-1] == 'a' || entidad[len-1] == 'o') ? "o" : "os");
}

/**
 * @brief Muestra "entidad no existe"
 */
void mostrar_no_existe(const char *entidad)
{
    if (!entidad) return;
    printf("El %s no existe.\n", entidad);
}

/**
 * @brief Muestra error de operación
 */
void mostrar_error_operacion(const char *entidad, const char *operacion)
{
    if (!entidad || !operacion) return;
    printf("Error al %s el %s.\n", operacion, entidad);
}

/**
 * @brief Atajo para clear_screen + print_header
 */
void mostrar_pantalla(const char *titulo)
{
    clear_screen();
    print_header(titulo);
}

/**
 * @brief Elimina espacios en blanco al final de una cadena
 */
char* trim_trailing_spaces(char *str)
{
    if (!str) return str;

    size_t len = safe_strnlen(str, SIZE_MAX);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n' || str[len-1] == '\r'))
    {
        str[--len] = '\0';
    }
    return str;
}

/**
 * @brief Ejecuta una consulta SQL y devuelve el statement preparado
 * Función común para centralizar la ejecución de consultas
 */
sqlite3_stmt* execute_query(const char *sql)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        return NULL;
    }
    return stmt;
}

/**
 * @brief Lista equipos disponibles para selección
 * Función común usada en múltiples módulos para mostrar equipos
 */
int list_available_teams(const char *no_records_msg, int pause_on_empty)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (preparar_stmt(sql, &stmt))
    {
        ui_printf_centered_line("=== EQUIPOS DISPONIBLES ===");
        ui_printf("\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            ui_printf_centered_line("%d. %s", id, nombre);
        }

        if (!found)
        {
            mostrar_no_hay_registros(no_records_msg);
            sqlite3_finalize(stmt);
            if (pause_on_empty)
            {
                pause_console();
            }
            return 0;
        }
        sqlite3_finalize(stmt);
        return 1;
    }

    if (pause_on_empty)
    {
        pause_console();
    }
    return 0;
}

/**
 * @brief Obtiene el ID de un equipo seleccionado por el usuario
 * Función común para selección de equipos con validación
 */
int select_team_id(const char *prompt, const char *no_records_msg, int pause_on_error)
{
    if (!list_available_teams(no_records_msg, pause_on_error))
    {
        return 0;
    }
    int equipo_id = input_int(prompt);
    if (equipo_id == 0)
    {
        return 0;
    }
    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        if (pause_on_error)
        {
            pause_console();
        }
        return 0;
    }
    return equipo_id;
}

/**
 * @brief Escribe encabezado CSV para exportaciones
 * Función común para formato consistente en exportaciones CSV
 */
void write_csv_header(FILE *f, const char *header)
{
    fprintf(f, "%s\n", header);
}

static char *get_trimmed_cancha_from_stmt(sqlite3_stmt *stmt)
{
    char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
    trim_trailing_spaces(cancha_trimmed);
    return cancha_trimmed;
}

/**
 * @brief Escribe fila CSV con datos de partido
 * Función común para exportar datos de partidos a CSV
 */
void write_partido_csv_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Escribe fila TXT con datos de partido
 * Función común para exportar datos de partidos a TXT
 */
void write_partido_txt_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Escribe objeto JSON con datos de partido
 * Función común para exportar datos de partidos a JSON
 */
void write_partido_json_object(cJSON *item, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);

    cJSON_AddStringToObject(item, "cancha", cancha_trimmed);
    cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 3));
    cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
    cJSON_AddStringToObject(item, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
    cJSON_AddStringToObject(item, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
    cJSON_AddStringToObject(item, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
    cJSON_AddNumberToObject(item, "rendimiento_general", sqlite3_column_int(stmt, 8));
    cJSON_AddNumberToObject(item, "cansancio", sqlite3_column_int(stmt, 9));
    cJSON_AddNumberToObject(item, "estado_animo", sqlite3_column_int(stmt, 10));
    cJSON_AddStringToObject(item, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));

    free(cancha_trimmed);
}

/**
 * @brief Escribe fila HTML con datos de partido
 * Función común para exportar datos de partidos a HTML
 */
void write_partido_html_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    fprintf(f,
            "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Función común para mostrar récords simples
 * Centraliza la lógica de mostrar récords con formato consistente
 */
void mostrar_record_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Valor: %d\n", sqlite3_column_int(stmt, 0));
        if (sqlite3_column_count(stmt) > 1)
        {
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        }
        if (sqlite3_column_count(stmt) > 2)
        {
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 2));
        }
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Función común para mostrar combinaciones cancha-camiseta
 * Centraliza la lógica de mostrar combinaciones con formato consistente
 */
void mostrar_combinacion_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Cancha: %s\n", sqlite3_column_text(stmt, 0));
        printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 2));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 3));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Función común para mostrar temporadas
 * Centraliza la lógica de mostrar temporadas con formato consistente
 */
void mostrar_temporada_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* year = (const char*)sqlite3_column_text(stmt, 0);
        if (year)
        {
            printf("Año: %s\n", year);
        }
        else
        {
            printf("Año: Desconocido\n");
        }
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 1));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 2));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}



/**
 * @brief Función genérica para exportar récords a CSV
 * Centraliza la lógica de exportación CSV para récords individuales
 */
void exportar_record_simple_csv(const char *titulo, const char *sql, const char *filename)
{
    FILE *file = NULL;
    errno_t err = fopen_s(&file, get_export_path(filename), "w");
    if (err != 0 || !file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Valor,Camiseta,Fecha\n");

    sqlite3_stmt *stmt = execute_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2));
    }

    if (stmt) sqlite3_finalize(stmt);
    printf("Exportado a %s\n", get_export_path(filename));
    fclose(file);
}

/**
 * @brief Función genérica para exportar un partido específico a CSV
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_csv(const char *order_by, const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    write_csv_header(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal");

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_csv_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Función genérica para exportar un partido específico a TXT
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_txt(const char *order_by, const char *filename, const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    fprintf(f, "%s\n\n", title);

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_txt_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Función genérica para exportar un partido específico a JSON
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_json(const char *order_by, const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_json_object(root, stmt);
        }
        sqlite3_finalize(stmt);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Función genérica para exportar un partido específico a HTML
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_html(const char *order_by, const char *filename, const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    fprintf(f, "<html><body><h1>%s</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>",
            title);

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_html_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(f, "</table></body></html>");
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/* ===================== FUNCIONES DE ESTADÍSTICAS COMPARTIDAS ===================== */

void reset_estadisticas(Estadisticas *stats)
{
    memset(stats, 0, sizeof(*stats));
}

void calcular_estadisticas(Estadisticas *stats, const char *sql)
{
    sqlite3_stmt *stmt;
    reset_estadisticas(stats);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->avg_goles = sqlite3_column_double(stmt, 1);
        stats->avg_asistencias = sqlite3_column_double(stmt, 2);
        stats->avg_rendimiento = sqlite3_column_double(stmt, 3);
        stats->avg_cansancio = sqlite3_column_double(stmt, 4);
        stats->avg_animo = sqlite3_column_double(stmt, 5);
    }
    sqlite3_finalize(stmt);
}

void actualizar_rachas(int resultado, int *racha_actual_v, int *max_racha_v,
                       int *racha_actual_d, int *max_racha_d)
{
    if (resultado == 1)
    {
        (*racha_actual_v)++;
        if (*racha_actual_v > *max_racha_v)
            *max_racha_v = *racha_actual_v;
        *racha_actual_d = 0;
        return;
    }

    if (resultado == 3)
    {
        (*racha_actual_d)++;
        if (*racha_actual_d > *max_racha_d)
            *max_racha_d = *racha_actual_d;
        *racha_actual_v = 0;
        return;
    }

    *racha_actual_v = 0;
    *racha_actual_d = 0;
}

int preparar_stmt_export(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

int preparar_consulta_con_verificacion(sqlite3_stmt **stmt, const char *tabla,
                                       const char *mensaje, const char *sql, int *count)
{
    sqlite3_stmt *check_stmt;
    *count = 0;

    // Construir consulta de conteo
    char count_sql[256];
    snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) FROM %s", tabla);

    // Verificar si hay registros
    if (!preparar_stmt_export(&check_stmt, count_sql))
    {
        return 0;
    }

    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        *count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);

    if (*count == 0)
    {
        mostrar_no_hay_registros(mensaje);
        return 0;
    }

    // Preparar la consulta principal
    if (!preparar_stmt_export(stmt, sql))
    {
        return 0;
    }

    return 1;
}

FILE *abrir_archivo_exportacion(const char *filename, const char *error_msg)
{
    FILE *file;
    const char *path = get_export_path(filename);
    errno_t err = fopen_s(&file, path, "w");
    if (err != 0 || file == NULL)
    {
        printf("%s\n", error_msg);
        return NULL;
    }
    return file;
}

int has_records(const char *table_name)
{
    sqlite3_stmt *stmt;
    char sql[256];
    int count = 0;
    int result = 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table_name);

    if (preparar_stmt_export(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        result = count > 0;
    }

    return result;
}

void trim_whitespace(char *str)
{
    if (!str)
        return;

    size_t total = strlen_s(str, SIZE_MAX);
    if (total == 0)
        return;

    char const *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    char const *end = start + strlen_s(start, SIZE_MAX);
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;

    size_t len = (size_t)(end - start);
    if (start != str)
        memmove(str, start, len + 1);
    else
        str[len] = '\0';
}

void extraer_estadistica_anio(sqlite3_stmt *stmt, EstadisticaAnio *stats)
{
    stats->anio = (const char *)sqlite3_column_text(stmt, 0);
    stats->camiseta = (const char *)sqlite3_column_text(stmt, 1);
    stats->partidos = sqlite3_column_int(stmt, 2);
    stats->total_goles = sqlite3_column_int(stmt, 3);
    stats->total_asistencias = sqlite3_column_int(stmt, 4);
    stats->avg_goles = sqlite3_column_double(stmt, 5);
    stats->avg_asistencias = sqlite3_column_double(stmt, 6);
}
