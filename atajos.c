#include "atajos.h"
#include "settings.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#endif

static int atajos_activos = 0;

#ifndef _WIN32
static struct termios old_term;

static void set_nonblocking_mode(void)
{
    tcgetattr(STDIN_FILENO, &old_term);
}

static void restore_terminal_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}
#endif

void inicializar_atajos(void)
{
#ifndef _WIN32
    set_nonblocking_mode();
#endif
    atajos_activos = 1;
}

void finalizar_atajos(void)
{
    if (!atajos_activos)
    {
        return;
    }

#ifndef _WIN32
    restore_terminal_mode();
#endif
    atajos_activos = 0;
}

static int kbhit_portable(void)
{
#ifdef _WIN32
    return _kbhit();
#else
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0;
#endif
}

static int getch_portable(void)
{
#ifdef _WIN32
    return _getch();
#else
    int ch = getchar();
    return ch;
#endif
}

int verificar_atajo(void)
{
    if (!atajos_activos)
    {
        return 0;
    }

    // Solo verificar si no estamos en modo interactivo bloqueante
    if (!kbhit_portable())
    {
        return 0;
    }

    int check = getch_portable();

    if (check == -1 || check == EOF)
    {
        return 0;
    }

    // Convertir a mayúscula
    check = toupper(check);

    return check;
}

void mostrar_ayuda_atajos(void)
{
    clear_screen();
    print_header(get_text("header_atajos"));

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                 %-47s║\n", get_text("atajos_title"));
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║  [D] - %-47s║\n", get_text("atajos_dashboard"));
    printf("║  [B] - %-47s║\n", get_text("atajos_busqueda"));
    printf("║  [C] - %-47s║\n", get_text("atajos_calendario"));
    printf("║  [N] - %-47s║\n", get_text("atajos_nuevo_partido"));
    printf("║  [S] - %-47s║\n", get_text("atajos_estadisticas"));
    printf("║  [H] - %-47s║\n", get_text("atajos_ayuda"));
    printf("║  [Q] - %-47s║\n", get_text("atajos_salir"));
    printf("║                                                              ║\n");
    printf("║  %-53s║\n", get_text("atajos_nota"));
    printf("║  %-53s║\n", get_text("atajos_presione_tecla"));
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    pause_console();
}
