#include <stddef.h>
#include "db.h"
#include "menu.h"
#include "camiseta.h"
#include "partido.h"
#include "lesion.h"
#include "estadisticas.h"
#include "export.h"
#include "export_all.h"

int main()
{
    if (!db_init())
        return 1;

    MenuItem items[] =
    {
        {1,"Camisetas",menu_camisetas},
        {2,"Partidos",menu_partidos},
        {3,"Estadisticas",mostrar_estadisticas},
        {4,"Lesiones",menu_lesiones},
        {5,"Exportar Todo",exportar_todo},
        {0,"Salir",NULL}
    };

    ejecutar_menu("MI FUTBOL C", items, 6);

    db_close();
    return 0;
}
