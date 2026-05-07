#include "atajos.h"
#include "utils.h"
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#include <Windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

static int atajos_activos = 0;

#ifndef _WIN32
static struct termios old_term;

static void set_nonblocking_mode()
{
    struct termios new_term;

    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;

    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 0;
    new_term.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

static void restore_terminal_mode()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}
#endif

void inicializar_atajos()
{
#ifndef _WIN32
    set_nonblocking_mode();
#endif
    atajos_activos = 1;
}

void finalizar_atajos()
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

static int kbhit_portable()
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

static int getch_portable()
{
#ifdef _WIN32
    return _getch();
#else
    int ch = getchar();
    return ch;
#endif
}

int verificar_atajo()
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

    int ch = getch_portable();

    if (ch == -1 || ch == EOF)
    {
        return 0;
    }

    // Convertir a mayúscula
    ch = toupper(ch);

    return ch;
}

void mostrar_ayuda_atajos()
{
    clear_screen();
    print_header("ATAJOS DE TECLADO");

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                 ATAJOS RAPIDOS DISPONIBLES                   ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║  [D] - Mostrar Dashboard                                     ║\n");
    printf("║  [B] - Busqueda Global                                       ║\n");
    printf("║  [C] - Calendario del Mes                                    ║\n");
    printf("║  [N] - Crear Nuevo Partido                                   ║\n");
    printf("║  [S] - Ver Estadisticas                                      ║\n");
    printf("║  [H] - Mostrar esta Ayuda                                    ║\n");
    printf("║  [Q] - Salir de la Aplicacion                                ║\n");
    printf("║                                                              ║\n");
    printf("║  Nota: Los atajos funcionan desde el menu principal          ║\n");
    printf("║        Presione la tecla sin Enter                           ║\n");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    pause_console();
}
