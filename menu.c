#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>
#ifdef USE_NCURSES
#include <ncursesw/ncurses.h>
#endif
#ifdef _WIN32
#include <Windows.h>
#endif
#include "db.h"
#include "camiseta.h"
#include "cancha.h"
#include "partido.h"
#include "lesion.h"
#include "estadisticas.h"
#include "analisis.h"
#include "logros.h"
#include "export.h"
#include "export_all.h"
#include "import.h"
#include "equipo.h"
#include "torneo.h"
#include "temporada.h"
#include "ascii_art.h"
#include "settings.h"
#include "financiamiento.h"
#include "entrenador_ia.h"
#include "bienestar.h"
#include "qr.h"

// Definir items del menú principal directamente con inicialización static
struct MenuItemDefinition
{
    int opcion;
    const char* texto;
    void (*accion)(void);
};

static const struct MenuItemDefinition MENU_ITEMS[] =
{
    {1, "Camisetas", &menu_camisetas},
    {2, "Canchas", &menu_canchas},
    {3, "Equipos", &menu_equipos},
    {4, "Partidos", &menu_partidos},
    {5, "Lesiones", &menu_lesiones},
    {6, "Estadisticas", &menu_estadisticas},
    {7, "QR", &menu_qr},
    {8, "Logros", &menu_logros},
    {9, "Financiamiento", &menu_financiamiento},
    {10, "Torneos", &menu_torneos},
    {11, "Temporada", &menu_temporadas},
    {12, "Analisis", &mostrar_analisis},
    {13, "Bienestar", &menu_bienestar},
    {14, "Ajustes", &menu_settings},
    {0, "Salir", NULL}
};

// Número de items en el menú principal
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
static const size_t MENU_ITEM_COUNT = ARRAY_SIZE(MENU_ITEMS);

static const MenuItem *buscar_item(const MenuItem *items, int cantidad, int opcion)
{
    for (int i = 0; i < cantidad; i++)
    {
        if (items[i].opcion == opcion)
        {
            return &items[i];
        }
    }
    return NULL;
}

#ifdef USE_NCURSES
#define HEADER_H 4
#define STATUS_H 1
#define MENU_W 26

/* forward declarations de funciones ncurses usadas antes de su definición */
static void ncurses_sync_screen_size(void);

#define TERM_RECOMMENDED_W 80
#define TERM_RECOMMENDED_H 24
#define TERM_TIGHT_W 70
#define TERM_TIGHT_H 20
#define TERM_MIN_W 60
#define TERM_MIN_H 15

static int ncurses_init(void)
{
    ensure_console_maximized_windows();
    if (initscr() == NULL)
    {
        return 0;
    }
#ifdef _WIN32
    /* Dar tiempo a Windows a aplicar la maximización de la consola */
    Sleep(150);
#endif
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors())
    {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_WHITE, -1);
        init_pair(2, COLOR_BLACK, COLOR_GREEN);
        init_pair(3, COLOR_BLACK, COLOR_WHITE);
        init_pair(4, COLOR_GREEN, -1);
        init_pair(5, COLOR_MAGENTA, -1);
    }

    /* Asegurarnos de sincronizar el tamaño del terminal inmediatamente al iniciar */
    ncurses_sync_screen_size();
    clear();
    refresh();

    return 1;
}


static int ncurses_count_lines(const char *text)
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

static void ncurses_print_centered_lines(int start_y, const char *text)
{
    int height, width;
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

static void ncurses_show_welcome(const char *ascii_art, const char *line1, const char *line2)
{
    int height;
    int width;
    getmaxyx(stdscr, height, width);

    int art_lines = ncurses_count_lines(ascii_art);
    int msg_lines = (line2 && line2[0] != '\0') ? 2 : 1;
    int total_lines = art_lines + msg_lines;

    int start_y = (height - total_lines) / 2;
    if (start_y < 0)
    {
        start_y = 0;
    }

    ncurses_print_centered_lines(start_y, ascii_art);

    int msg_y = start_y + art_lines;
    int len1 = (int)strlen(line1);
    int x1 = (width - len1) / 2;
    mvprintw(msg_y, x1 < 0 ? 0 : x1, "%s", line1);

    if (msg_lines == 2)
    {
        int len2 = (int)strlen(line2);
        int x2 = (width - len2) / 2;
        mvprintw(msg_y + 1, x2 < 0 ? 0 : x2, "%s", line2);
    }

    refresh();
    int ch;
    do
    {
        ch = getch();
    }
    while (ch != '\n' && ch != KEY_ENTER);
}

static void ncurses_draw_header(WINDOW *win, const char *titulo)
{
    int width = getmaxx(win);
    char fecha[20];
    char titulo_buf[128];
    const char *titulo_src = titulo ? titulo : "";
    size_t titulo_len = strlen(titulo_src);
    char *nombre_usuario_dyn = get_user_name();
    const char *nombre_usuario = nombre_usuario_dyn;
    if (!nombre_usuario_dyn)
    {
        nombre_usuario = "Usuario Desconocido";
    }
    get_datetime(fecha, sizeof(fecha));

    if (titulo_len >= sizeof(titulo_buf))
    {
        titulo_len = sizeof(titulo_buf) - 1;
    }
    for (size_t i = 0; i < titulo_len; ++i)
    {
        unsigned char ch = (unsigned char)titulo_src[i];
        titulo_buf[i] = (char)toupper(ch);
    }
    titulo_buf[titulo_len] = '\0';

    werase(win);
    wbkgd(win, COLOR_PAIR(3));
    wattron(win, A_BOLD);
    mvwprintw(win, 0, (width - (int)strlen(titulo_buf)) / 2, "%s", titulo_buf);
    wattroff(win, A_BOLD);
    {
        char usuario_line[256];
        char fecha_line[256];
        int usuario_len;
        int fecha_len;
        int usuario_x;
        int fecha_x;

        snprintf(usuario_line, sizeof(usuario_line), "Usuario: %s", nombre_usuario);
        snprintf(fecha_line, sizeof(fecha_line), "Fecha: %s", fecha);
        usuario_len = (int)strlen(usuario_line);
        fecha_len = (int)strlen(fecha_line);
        usuario_x = (width - usuario_len) / 2;
        fecha_x = (width - fecha_len) / 2;
        if (usuario_x < 0)
        {
            usuario_x = 0;
        }
        if (fecha_x < 0)
        {
            fecha_x = 0;
        }

        mvwprintw(win, 1, usuario_x, "%s", usuario_line);
        mvwprintw(win, 2, fecha_x, "%s", fecha_line);
    }
    wrefresh(win);

    if (nombre_usuario_dyn)
    {
        free(nombre_usuario_dyn);
    }
}

static int ncurses_get_chunk_size(const char *text, int offset, int prefix_len, int available)
{
    int remaining = prefix_len - offset;
    int chunk = remaining > available ? available : remaining;

    if (chunk == available && chunk > 1)
    {
        int k;
        for (k = chunk - 1; k > 0; --k)
        {
            if (text[offset + k] == ' ')
            {
                chunk = k;
                break;
            }
        }
    }

    return chunk;
}

typedef struct
{
    WINDOW *win;
    int start_x;
    int available;
    int content_bottom;
    int selected;
} NcursesWrapLineContext;

static int ncurses_draw_wrapped_option_line(const NcursesWrapLineContext *ctx,
        const char *text,
        int prefix_len,
        int current_row)
{
    int offset = 0;

    while (offset < prefix_len && current_row <= ctx->content_bottom)
    {
        int chunk = ncurses_get_chunk_size(text, offset, prefix_len, ctx->available);

        if (ctx->selected)
        {
            wattron(ctx->win, COLOR_PAIR(2) | A_BOLD);
        }
        else
        {
            wattron(ctx->win, COLOR_PAIR(1));
        }

        mvwprintw(ctx->win, current_row, ctx->start_x, "%.*s", chunk, text + offset);
        wmove(ctx->win, current_row, ctx->start_x + chunk);
        wclrtoeol(ctx->win);

        if (ctx->selected)
        {
            wattroff(ctx->win, COLOR_PAIR(2) | A_BOLD);
        }
        else
        {
            wattroff(ctx->win, COLOR_PAIR(1));
        }

        current_row++;
        offset += chunk;
        while (offset < prefix_len && text[offset] == ' ')
        {
            offset++;
        }
    }

    return current_row;
}

static void ncurses_draw_menu(WINDOW *win, const MenuItem *items, int cantidad, int selected)
{
    werase(win);
    box(win, 0, 0);
    wattron(win, A_BOLD);
    {
        int width = getmaxx(win);
        const char *label = " Menu ";
        int label_x = (width - (int)strlen(label)) / 2;
        if (label_x < 2)
        {
            label_x = 2;
        }
        mvwprintw(win, 0, label_x, "%s", label);
    }
    wattroff(win, A_BOLD);

    int width = getmaxx(win);
    int height = getmaxy(win);
    int max_len = 0;
    for (int i = 0; i < cantidad; ++i)
    {
        char line_buf[128];
        int len = snprintf(line_buf, sizeof(line_buf), "> %d. %s", items[i].opcion, items[i].texto);
        if (len > max_len)
        {
            max_len = len;
        }
    }

    int start_x = (width - max_len) / 2;
    if (start_x < 2)
    {
        start_x = 2;
    }

    int content_top = 1;
    int content_bottom = height - 2;
    int content_height = content_bottom - content_top + 1;
    int start_y = content_top + (content_height - cantidad) / 2;
    if (start_y < 2)
    {
        start_y = 2;
    }

    int available = width - start_x - 2;
    if (available < 1)
        available = 1;

    int current_row = start_y;
    for (int i = 0; i < cantidad && current_row <= content_bottom; ++i)
    {
        char full[512];
        int prefix_len = snprintf(full, sizeof(full), "%s %d. %s", (i == selected) ? ">" : " ", items[i].opcion, items[i].texto);
        NcursesWrapLineContext ctx =
        {
            win,
            start_x,
            available,
            content_bottom,
            i == selected
        };

        current_row = ncurses_draw_wrapped_option_line(&ctx, full, prefix_len, current_row);
    }
    wrefresh(win);
}

static void ncurses_draw_status(WINDOW *win, const char *msg)
{
    int width = getmaxx(win);
    werase(win);
    wattron(win, COLOR_PAIR(3));
    {
        int len = msg ? (int)strlen(msg) : 0;
        int start_x = (width - len) / 2;
        if (start_x < 1)
        {
            start_x = 1;
        }
        mvwprintw(win, 0, start_x, "%s", msg ? msg : "");
    }
    wattroff(win, COLOR_PAIR(3));
    wclrtoeol(win);
    wrefresh(win);
}

static void ncurses_draw_content(WINDOW *win, WINDOW *body_win, const char *title, const char *body)
{
    int width = getmaxx(win);
    werase(win);
    box(win, 0, 0);
    wattron(win, A_BOLD);
    {
        int title_x = (width - (int)strlen(title)) / 2;
        if (title_x < 2)
        {
            title_x = 2;
        }
        mvwprintw(win, 0, title_x, " %s ", title);
    }
    wattroff(win, A_BOLD);

    if (body_win)
    {
        werase(body_win);
        if (body && body[0] != '\0')
        {
            mvwprintw(body_win, 0, 0, "%s", body);
        }
        wrefresh(body_win);
    }
    wrefresh(win);
}

static int ncurses_is_too_small(int height, int width)
{
    return (height < TERM_MIN_H || width < TERM_MIN_W);
}

static const char *ncurses_get_size_notice(int height, int width, char *buffer, size_t size)
{
    if (height < TERM_MIN_H || width < TERM_MIN_W)
    {
        snprintf(buffer, size, "Terminal demasiado pequena (min %dx%d)", TERM_MIN_W, TERM_MIN_H);
        return buffer;
    }
    if (height < TERM_TIGHT_H || width < TERM_TIGHT_W)
    {
        snprintf(buffer, size, "Terminal muy ajustada (%dx%d)", TERM_TIGHT_W, TERM_TIGHT_H);
        return buffer;
    }
    if (height < TERM_RECOMMENDED_H || width < TERM_RECOMMENDED_W)
    {
        snprintf(buffer, size, "Terminal ajustada (rec %dx%d)", TERM_RECOMMENDED_W, TERM_RECOMMENDED_H);
        return buffer;
    }
    return NULL;
}

static void ncurses_draw_size_warning(int height, int width)
{
    const char *line1 = NULL;
    const char *line2 = NULL;
    const char *line3 = NULL;
    char detail[64];
    int y = height / 2 - 1;
    int x1 = 0;
    int x2 = 0;
    int x3 = 0;

    if (y < 0)
    {
        y = 0;
    }

    if (width < TERM_MIN_W || height < TERM_MIN_H)
    {
        line1 = "Terminal demasiado pequena";
        snprintf(detail, sizeof(detail), "Minimo absoluto: %dx%d", TERM_MIN_W, TERM_MIN_H);
        line2 = detail;
        line3 = "Ajusta el tamano para continuar";
    }
    else if (width < TERM_TIGHT_W || height < TERM_TIGHT_H)
    {
        line1 = "Terminal muy ajustada";
        snprintf(detail, sizeof(detail), "Segundo nivel: %dx%d", TERM_TIGHT_W, TERM_TIGHT_H);
        line2 = detail;
        line3 = "Puede verse apretado";
    }
    else if (width < TERM_RECOMMENDED_W || height < TERM_RECOMMENDED_H)
    {
        line1 = "Terminal ajustada";
        snprintf(detail, sizeof(detail), "Recomendado: %dx%d", TERM_RECOMMENDED_W, TERM_RECOMMENDED_H);
        line2 = detail;
        line3 = "Para mejor vista, agranda";
    }

    if (!line1)
    {
        return;
    }

    x1 = (width - (int)strlen(line1)) / 2;
    x2 = (width - (int)strlen(line2)) / 2;
    x3 = (width - (int)strlen(line3)) / 2;
    if (x1 < 0)
    {
        x1 = 0;
    }
    if (x2 < 0)
    {
        x2 = 0;
    }
    if (x3 < 0)
    {
        x3 = 0;
    }

    erase();
    mvprintw(y, x1, "%s", line1);
    mvprintw(y + 1, x2, "%s", line2);
    mvprintw(y + 2, x3, "%s", line3);
    refresh();
}

static int ncurses_is_main_menu(const char *titulo)
{
    (void)titulo;
    return 1;
}

static int ncurses_menu_persist(const char *titulo)
{
    (void)titulo;
    return 1;
}

static void ncurses_sync_screen_size(void)
{
    resize_term(0, 0);
    clearok(stdscr, TRUE);
    refresh();
}

typedef struct
{
    const char *titulo;
    const MenuItem *items;
    int cantidad;
    int selected;
    int header_h;
    int status_h;
    int is_main;
    WINDOW *header;
    WINDOW *status;
    WINDOW *menu;
    WINDOW *content;
    WINDOW *content_body;
    int height;
    int width;
    int body_h;
    int menu_w;
    int content_w;
    int too_small;
} NcursesMenuLayout;

static void ncurses_relayout(NcursesMenuLayout *layout)
{
    ncurses_sync_screen_size();
    getmaxyx(stdscr, layout->height, layout->width);
    layout->too_small = ncurses_is_too_small(layout->height, layout->width);
    if (layout->too_small)
    {
        ncurses_draw_size_warning(layout->height, layout->width);
        return;
    }
    layout->body_h = layout->height - layout->header_h - layout->status_h;
    layout->menu_w = layout->is_main ? MENU_W : layout->width;
    layout->content_w = layout->is_main ? (layout->width - layout->menu_w - 2) : 0;
    if (layout->content_w <= 10)
    {
        layout->menu_w = layout->width;
        layout->content_w = 0;
    }

    wresize(layout->header, layout->header_h, layout->width);
    wresize(layout->status, layout->status_h, layout->width);
    mvwin(layout->status, layout->height - 1, 0);

    wresize(layout->menu, layout->body_h, layout->is_main ? layout->menu_w : layout->width);
    mvwin(layout->menu, layout->header_h, 0);

    if (layout->content)
    {
        if (layout->content_w > 10)
        {
            wresize(layout->content, layout->body_h, layout->content_w);
            mvwin(layout->content, layout->header_h, layout->menu_w + 1);
            if (layout->content_body)
            {
                delwin(layout->content_body);
                layout->content_body = NULL;
            }
            if (layout->body_h > 2 && layout->content_w > 2)
            {
                layout->content_body = derwin(layout->content, layout->body_h - 2, layout->content_w - 2, 1, 1);
            }
        }
        else
        {
            if (layout->content_body)
            {
                delwin(layout->content_body);
                layout->content_body = NULL;
            }
            werase(layout->content);
            wrefresh(layout->content);
            delwin(layout->content);
            layout->content = NULL;
        }
    }
    else if (layout->is_main && layout->content_w > 10)
    {
        layout->content = newwin(layout->body_h, layout->content_w, layout->header_h, layout->menu_w + 1);
        if (layout->content && layout->body_h > 2 && layout->content_w > 2)
        {
            layout->content_body = derwin(layout->content, layout->body_h - 2, layout->content_w - 2, 1, 1);
        }
    }

    {
        char status_buf[128];
        const char *status_msg = ncurses_get_size_notice(layout->height, layout->width, status_buf, sizeof(status_buf));
        clear();
        ncurses_draw_header(layout->header, layout->titulo);
        ncurses_draw_status(layout->status, status_msg ? status_msg : "Flechas Navegar | Enter seleccionar");
    }
    ncurses_draw_menu(layout->menu, layout->items, layout->cantidad, layout->selected);
    if (layout->content)
    {
        ncurses_draw_content(layout->content, layout->content_body, "Contenido", layout->items[layout->selected].texto);
    }
    refresh();
}

static void ncurses_prepare_action_output(WINDOW *content_body)
{
    if (content_body)
    {
        werase(content_body);
        wmove(content_body, 0, 0);
        wrefresh(content_body);
        scrollok(content_body, TRUE);
        ui_set_output_window(content_body);
        return;
    }

    ui_set_output_window(NULL);
}

static void ncurses_finalize_action_output(WINDOW *content_body)
{
    ui_set_output_window(NULL);
    if (content_body)
    {
        scrollok(content_body, FALSE);
    }
}

static void ncurses_execute_selected_item(NcursesMenuLayout *layout, const MenuItem *selected_item)
{
    ncurses_prepare_action_output(layout->content_body);
    selected_item->accion();
    ncurses_finalize_action_output(layout->content_body);
    ncurses_relayout(layout);
}

static int ncurses_run_menu(const char *titulo, const MenuItem *items, int cantidad)
{
    NcursesMenuLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.titulo = titulo;
    layout.items = items;
    layout.cantidad = cantidad;
    layout.selected = 0;

    int ch;
    char input_buf[8] = {0};
    int input_len = 0;
    int pending_resize = 0;
    int pending_height = 0;
    int pending_width = 0;
    clock_t resize_start = 0;
    const int resize_debounce_ms = 250;
    layout.is_main = ncurses_is_main_menu(titulo);

    if (!ncurses_init())
    {
        return 0;
    }

    getmaxyx(stdscr, layout.height, layout.width);

    timeout(-1);

    layout.header_h = HEADER_H;
    layout.status_h = STATUS_H;
    layout.body_h = layout.height - layout.header_h - layout.status_h;
    layout.menu_w = layout.is_main ? MENU_W : layout.width;
    layout.content_w = layout.is_main ? (layout.width - layout.menu_w - 2) : 0;
    if (layout.content_w <= 10)
    {
        layout.menu_w = layout.width;
        layout.content_w = 0;
    }

    layout.header = newwin(layout.header_h, layout.width, 0, 0);
    layout.status = newwin(layout.status_h, layout.width, layout.height - 1, 0);
    layout.menu = newwin(layout.body_h, layout.is_main ? layout.menu_w : layout.width, layout.header_h, 0);

    if (layout.is_main && layout.content_w > 10)
    {
        layout.content = newwin(layout.body_h, layout.content_w, layout.header_h, layout.menu_w + 1);
        if (layout.content && layout.body_h > 2 && layout.content_w > 2)
        {
            layout.content_body = derwin(layout.content, layout.body_h - 2, layout.content_w - 2, 1, 1);
        }
    }

    layout.too_small = ncurses_is_too_small(layout.height, layout.width);
    if (layout.too_small)
    {
        ncurses_draw_size_warning(layout.height, layout.width);
    }
    else
    {
        char status_buf[128];
        const char *status_msg = ncurses_get_size_notice(layout.height, layout.width, status_buf, sizeof(status_buf));
        ncurses_draw_header(layout.header, titulo);
        ncurses_draw_menu(layout.menu, items, cantidad, layout.selected);
        if (layout.content)
        {
            ncurses_draw_content(layout.content, layout.content_body, "Contenido", "Seleccione una Opcion");
        }
        ncurses_draw_status(layout.status, status_msg ? status_msg : "Flechas Navegar | Enter seleccionar");
        refresh();
    }

    while (1)
    {
        ch = getch();
#ifdef KEY_RESIZE
        if (ch == KEY_RESIZE)
        {
            ncurses_sync_screen_size();
            getmaxyx(stdscr, layout.height, layout.width);
            pending_resize = 1;
            pending_height = layout.height;
            pending_width = layout.width;
            resize_start = clock();
        }
#endif

        if (pending_resize)
        {
            int current_h;
            int current_w;
            int elapsed_ms;
            ncurses_sync_screen_size();
            getmaxyx(stdscr, current_h, current_w);
            if (current_h != pending_height || current_w != pending_width)
            {
                pending_height = current_h;
                pending_width = current_w;
                resize_start = clock();
            }

            elapsed_ms = (int)((clock() - resize_start) * 1000 / CLOCKS_PER_SEC);
            if (elapsed_ms >= resize_debounce_ms && current_h == pending_height && current_w == pending_width)
            {
                pending_resize = 0;
                ncurses_relayout(&layout);
                continue;
            }
        }
        if (ch == KEY_UP)
        {
            layout.selected = (layout.selected - 1 + cantidad) % cantidad;
            input_len = 0;
            input_buf[0] = '\0';
        }
        else if (ch == KEY_DOWN)
        {
            layout.selected = (layout.selected + 1) % cantidad;
            input_len = 0;
            input_buf[0] = '\0';
        }
        else if (ch == 10 || ch == KEY_ENTER)
        {
            int target_index = layout.selected;
            if (input_len > 0)
            {
                int numero = atoi(input_buf);
                const MenuItem *by_number = buscar_item(items, cantidad, numero);
                if (by_number)
                {
                    target_index = (int)(by_number - items);
                }
                input_len = 0;
                input_buf[0] = '\0';
            }

            const MenuItem *selected_item = &items[target_index];
            if (!selected_item->accion)
            {
                break;
            }

            ncurses_execute_selected_item(&layout, selected_item);
        }
        else if (ch >= '0' && ch <= '9')
        {
            if (input_len < (int)(sizeof(input_buf) - 1))
            {
                input_buf[input_len++] = (char)ch;
                input_buf[input_len] = '\0';
            }
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
        {
            if (input_len > 0)
            {
                input_len--;
                input_buf[input_len] = '\0';
            }
        }

        if (input_len > 0)
        {
            char status_msg[128];
            snprintf(status_msg, sizeof(status_msg), "Numero: %s (Enter para ir)", input_buf);
            ncurses_draw_status(layout.status, status_msg);
        }

        if (layout.too_small)
        {
            ncurses_draw_size_warning(layout.height, layout.width);
            continue;
        }
        ncurses_draw_menu(layout.menu, items, cantidad, layout.selected);
        if (layout.content)
        {
            ncurses_draw_content(layout.content, layout.content_body, "Contenido", items[layout.selected].texto);
        }
    }

    delwin(layout.menu);
    delwin(layout.status);
    delwin(layout.header);
    if (layout.content_body)
    {
        delwin(layout.content_body);
    }
    if (layout.content)
    {
        delwin(layout.content);
    }
    endwin();
    ensure_console_maximized_windows();
    return 1;
}
#endif

#ifdef UNIT_TEST
static MenuTestCapture *g_menu_test_capture = NULL;

int menu_get_item_count(void)
{
    return (int)MENU_ITEM_COUNT;
}

const MenuItem *menu_get_items(void)
{
    return (const MenuItem *)MENU_ITEMS;
}

const MenuItem *menu_buscar_item(const MenuItem *items, int cantidad, int opcion)
{
    return buscar_item(items, cantidad, opcion);
}

void menu_test_set_capture(MenuTestCapture *capture)
{
    g_menu_test_capture = capture;
}
#endif

/**
 * @brief Inicializa la aplicación: consola, locale, base de datos y configuración
 */
void initialize_application()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    setlocale(LC_ALL, "");

    if (!db_init())
    {
        printf("Error inicializando base de datos\n");
        exit(1);
    }

    settings_init();

#ifdef USE_NCURSES
    /* Verificar tamaño de terminal al iniciar y ofrecer opción para agrandar */
    if (settings_get_use_ncurses() && ncurses_init())
    {
        int h, w;
        getmaxyx(stdscr, h, w);
        if (h < TERM_RECOMMENDED_H || w < TERM_RECOMMENDED_W)
        {
            int box_w = w - 10;
            if (box_w > 60) box_w = 60;
            if (box_w < 30) box_w = w - 4;
            int box_h = 6;
            int box_y = (h - box_h) / 2;
            int box_x = (w - box_w) / 2;
            WINDOW *dlg = newwin(box_h, box_w, box_y < 0 ? 0 : box_y, box_x < 0 ? 0 : box_x);
            box(dlg, 0, 0);
            mvwprintw(dlg, 1, 2, "Terminal: %dx%d", w, h);
            mvwprintw(dlg, 2, 2, "La terminal parece pequeña para una vista óptima.");
            mvwprintw(dlg, 3, 2, "¿Desea agrandar la ventana? (S/N)");
            wrefresh(dlg);

            int ch;
            while ((ch = wgetch(dlg)))
            {
                if (ch == 's' || ch == 'S' || ch == 'n' || ch == 'N')
                    break;
            }

            delwin(dlg);

            if (ch == 's' || ch == 'S')
            {
                WINDOW *msg = newwin(3, box_w, box_y < 0 ? 0 : box_y, box_x < 0 ? 0 : box_x);
                box(msg, 0, 0);
                mvwprintw(msg, 1, 2, "Ajuste el tamaño de la ventana y presione ENTER para continuar...");
                wrefresh(msg);
                while ((ch = wgetch(msg)) != 10 && ch != KEY_ENTER) {}
                delwin(msg);
                ncurses_sync_screen_size();
                clear();
                refresh();
            }
            else
            {
                /* El usuario eligió no agrandar; continuamos normalmente */
            }
        }
    }
    if (settings_get_use_ncurses())
    {
        endwin();
    }
#endif

}

/**
 * @brief Maneja la verificación y creación del nombre de usuario
 */
void handle_user_name()
{
    char* nombre_usuario;
    char buffer[256];

    nombre_usuario = get_user_name();

    if (!nombre_usuario)
    {
        pedir_nombre_usuario();
        nombre_usuario = get_user_name();
    }

    if (nombre_usuario)
    {
        snprintf(buffer, sizeof(buffer), "%s, %s\n", get_text("welcome_message"), nombre_usuario);
#ifdef USE_NCURSES
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
        {
            buffer[len - 1] = '\0';
        }
        if (settings_get_use_ncurses() && ncurses_init())
        {
            clear();
            ncurses_show_welcome(ASCII_BIENVENIDA, buffer, "Presione ENTER para continuar...");
            /* Mantener ncurses activo para que los menús y opciones sigan usando ncurses */
        }
        else
        {
            fputs(buffer, stdout);
            pause_console();
        }
#else
        fputs(buffer, stdout);
        pause_console();
#endif
        free(nombre_usuario);
    }
}

/**
 * @brief Crea el menú filtrado dinámicamente
 */
MenuItem* create_filtered_menu(int* count)
{
    *count = (int)MENU_ITEM_COUNT;

    MenuItem* filtered_items = (MenuItem*)malloc((size_t)(*count) * sizeof(MenuItem));
    if (!filtered_items)
    {
        printf("Error de memoria\n");
        db_close();
        exit(1);
    }

    for (int i = 0; i < *count; i++)
    {
        filtered_items[i].opcion = (MENU_ITEMS[i].opcion == 0) ? 0 : (i + 1);
        filtered_items[i].texto = MENU_ITEMS[i].texto;
        filtered_items[i].accion = MENU_ITEMS[i].accion;
    }

    return filtered_items;
}

/**
 * @brief Ejecuta el menú principal y libera recursos
 */
void run_menu(MenuItem* filtered_items, int count)
{
    ejecutar_menu(get_text("menu_title"), filtered_items, count);
    free(filtered_items);
    db_close();
#ifdef USE_NCURSES
    clear_screen();
#endif
}

/**
 * @brief Ejecuta un menú interactivo en la consola
 *
 * Esta función muestra un menú con el título proporcionado y una lista de opciones.
 * Permite al usuario seleccionar una opción y ejecuta la acción correspondiente.
 * Si la acción es NULL, sale del menú.
 */
void ejecutar_menu(const char *titulo, const MenuItem *items, int cantidad)
{
#ifdef UNIT_TEST
    if (g_menu_test_capture)
    {
        g_menu_test_capture->titulo = titulo;
        g_menu_test_capture->cantidad = cantidad;
        if (items && cantidad > 0)
        {
            g_menu_test_capture->last_item = items[cantidad - 1];
        }
        else
        {
            g_menu_test_capture->last_item.opcion = 0;
            g_menu_test_capture->last_item.texto = NULL;
            g_menu_test_capture->last_item.accion = NULL;
        }
        return;
    }
#endif
#ifdef USE_NCURSES
    if (settings_get_use_ncurses() && ncurses_run_menu(titulo, items, cantidad))
    {
        return;
    }
#endif
    int opcion;

    while (1)
    {
        clear_screen();
        print_header(titulo);

        for (int i = 0; i < cantidad; i++)
        {
            printf("%d.%s\n", items[i].opcion, items[i].texto);
        }

        opcion = input_int(">");

        const MenuItem *selected = buscar_item(items, cantidad, opcion);

        if (selected)
        {
            if (!selected->accion)
            {
                return;
            }

            selected->accion();
        }
    }
}
