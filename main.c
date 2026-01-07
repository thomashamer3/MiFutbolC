#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main()
{
    if (!db_init())
        return 1;

    // Verificar si existe nombre de usuario
    char *nombre_usuario = get_user_name();
    if (!nombre_usuario)
    {
        pedir_nombre_usuario();
    }
    else
    {
        printf("Bienvenido De Vuelta, %s\n", nombre_usuario);
        free(nombre_usuario);
        pause_console();
    }

    MenuItem items[] =
    {
        {1, "Camisetas", menu_camisetas},
        {2, "Canchas", menu_canchas},
        {3, "Partidos", menu_partidos},
        {4, "Equipos", menu_equipos},
        {5, "Estadisticas", menu_estadisticas},
        {6, "Logros", menu_logros},
        {7, "Analisis", mostrar_analisis},
        {8, "Lesiones", menu_lesiones},
        {9, "Exportar", menu_exportar},
        {10, "Importar", menu_importar},
        {11, "Usuario", menu_usuario},
        {12, "Torneos", menu_torneos},
        {0, "Salir", NULL}
    };

    ejecutar_menu("MI FUTBOL C", items, 13);
    db_close();
    return 0;
}
