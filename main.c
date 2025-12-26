#include <stddef.h>
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

int main()
{
    if (!db_init())
        return 1;

    MenuItem items[] =
    {
        {1, "Camisetas", menu_camisetas},
        {2, "Canchas", menu_canchas},
        {3, "Partidos", menu_partidos},
        {4, "Estadisticas", menu_estadisticas},
        {5, "Logros", menu_logros},
        {6, "Analisis", mostrar_analisis},
        {7, "Lesiones", menu_lesiones},
        {8, "Exportar Todo", exportar_todo},
        {9, "Importar Todo", importar_todo_json},
        {0, "Salir", NULL}
    };

    ejecutar_menu("MI FUTBOL C", items, 10);
    db_close();
    return 0;
}
