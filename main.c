#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
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
#include "ascii_art.h"
#include "settings.h"
#include "financiamiento.h"

int main()
{
    if (!db_init())
        return 1;

    setlocale(LC_ALL, "");
    // Inicializar configuración
    settings_init();

    // Verificar si existe nombre de usuario
    char *nombre_usuario = get_user_name();
    if (!nombre_usuario)
    {
        pedir_nombre_usuario();
    }
    else
    {
        printf(get_text("welcome_message"), nombre_usuario);
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
        {6, get_text("menu_logros"), menu_logros},
        {7, get_text("menu_analisis"), mostrar_analisis},
        {8, get_text("menu_lesiones"), menu_lesiones},
        {9, get_text("menu_financiamiento"), menu_financiamiento},
        {10, get_text("menu_exportar"), menu_exportar},
        {11, get_text("menu_importar"), menu_importar},
        {12, get_text("menu_torneos"), menu_torneos},
        {13, get_text("menu_settings"), menu_settings},
        {0, get_text("menu_exit"), NULL}
    };

    ejecutar_menu(get_text("menu_title"), items, 14);
    db_close();
    return 0;
}
