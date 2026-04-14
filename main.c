#include "menu.h"
#include "utils.h"
#include "settings.h"
#include "dashboard.h"
#include "atajos.h"
#include "musica.h"

#ifndef _WIN32
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int g_lock_fd = -1;
static char g_lock_path[1024];

static void release_single_instance_lock(void)
{
    if (g_lock_fd >= 0)
    {
        flock(g_lock_fd, LOCK_UN);
        close(g_lock_fd);
        g_lock_fd = -1;
    }

    if (g_lock_path[0] != '\0')
    {
        unlink(g_lock_path);
        g_lock_path[0] = '\0';
    }
}

static int acquire_single_instance_lock(void)
{
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir || runtime_dir[0] == '\0')
    {
        runtime_dir = "/tmp";
    }

    snprintf(g_lock_path, sizeof(g_lock_path), "%s/%s", runtime_dir,
             "mifutbolc.lock");

    g_lock_fd = open(g_lock_path, O_CREAT | O_RDWR, 0644);
    if (g_lock_fd < 0)
    {
        fprintf(stderr, "No se pudo crear lockfile (%s): %s\n", g_lock_path,
                strerror(errno));
        return 0;
    }

    if (flock(g_lock_fd, LOCK_EX | LOCK_NB) != 0)
    {
        if (errno == EWOULDBLOCK)
        {
            fprintf(stderr,
                    "MiFutbolC ya esta en ejecucion. Cierra la otra instancia e intenta nuevamente.\n");
        }
        else
        {
            fprintf(stderr, "No se pudo bloquear lockfile (%s): %s\n",
                    g_lock_path, strerror(errno));
        }

        close(g_lock_fd);
        g_lock_fd = -1;
        g_lock_path[0] = '\0';
        return 0;
    }

    atexit(release_single_instance_lock);
    return 1;
}
#endif

int main()
{
#ifndef _WIN32
    if (!acquire_single_instance_lock())
    {
        return 1;
    }
#endif

    if (!iniciar_sesion_multiusuario_local())
    {
        return 0;
    }

    initialize_application();
    if (settings_get_music_autoplay())
    {
        musica_iniciar_automatica();
    }
    handle_user_name();

    // Verificar actualizaciones al inicio (modo silencioso - solo muestra si hay actualización)
    verificar_actualizacion_disponible(0);
    
    // Mostrar Dashboard inicial con resumen de actividades
    mostrar_dashboard();
    
    // Inicializar sistema de atajos de teclado
    inicializar_atajos();

    int count;
    MenuItem* filtered_items = create_filtered_menu(&count);

    run_menu(filtered_items, count);
    
    // Finalizar sistema de atajos de teclado
    finalizar_atajos();

    return 0;
}
