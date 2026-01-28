#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "db.h"
#include "menu.h"
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
#include "utils.h"
#include "equipo.h"
#include "torneo.h"
#include "temporada.h"
#include "ascii_art.h"
#include "settings.h"
#include "financiamiento.h"
#include "entrenador_ia.h"
#include "qr.h"

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    setlocale(LC_ALL, "");
    // Inicializar configuración
    settings_init();

    if (!db_init())
        return 1;

    // Verificar si existe nombre de usuario
    char *nombre_usuario = get_user_name();
    char buffer[256];
    if (!nombre_usuario)
    {
        pedir_nombre_usuario();
        // Verificar si el usuario fue creado exitosamente
        nombre_usuario = get_user_name();
        if (nombre_usuario)
        {
            snprintf(buffer, sizeof(buffer), "%s, %s\n", get_text("welcome_message"), nombre_usuario);
            fputs(buffer, stdout);
            free(nombre_usuario);
            pause_console();
        }
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%s, %s\n", get_text("welcome_message"), nombre_usuario);
        fputs(buffer, stdout);
        free(nombre_usuario);
        pause_console();
    }

    MenuItem items[] =
    {
        {1, get_text("menu_camisetas"), menu_camisetas},
        {2, get_text("menu_canchas"), menu_canchas},
        {3, get_text("menu_partidos"), menu_partidos},
        {4, get_text("menu_equipos"), menu_equipos},
        {5, get_text("menu_estadisticas"), menu_estadisticas},
        {6, get_text("menu_qr"), menu_qr},
        {7, get_text("menu_logros"), menu_logros},
        {8, get_text("menu_analisis"), mostrar_analisis},
        {9, get_text("menu_lesiones"), menu_lesiones},
        {10, get_text("menu_financiamiento"), menu_financiamiento},
        {11, get_text("menu_exportar"), menu_exportar},
        {12, get_text("menu_importar"), menu_importar},
        {13, get_text("menu_torneos"), menu_torneos},
        {14, get_text("menu_temporada"), menu_temporadas},
        {15, get_text("menu_entrenador_ia"), menu_entrenador_ia},
        {16, get_text("menu_settings"), menu_settings},
        {0, get_text("menu_exit"), NULL}
    };

    ejecutar_menu(get_text("menu_title"), items, 17);
    db_close();
    return 0;
}
