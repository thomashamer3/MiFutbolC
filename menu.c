#include "menu.h"
#include "utils.h"
#include <stdio.h>

/**
 * @brief Ejecuta un menú interactivo en la consola.
 *
 * Esta función muestra un menú con el título proporcionado y una lista de opciones.
 * Permite al usuario seleccionar una opción y ejecuta la acción correspondiente.
 * Si la acción es NULL, sale del menú.
 *
 * @param titulo El título del menú a mostrar.
 * @param items Arreglo de elementos del menú.
 * @param cantidad Número de elementos en el arreglo.
 */
void ejecutar_menu(const char *titulo, MenuItem *items, int cantidad)
{
    int opcion;

    while (1)
    {
        clear_screen();
        print_header(titulo);

        for (int i = 0; i < cantidad; i++)
            printf("%d.%s\n", items[i].opcion, items[i].texto);

        opcion = input_int(">");

        for (int i = 0; i < cantidad; i++)
        {
            if (items[i].opcion == opcion)
            {
                if (!items[i].accion)
                    return;

                items[i].accion();
            }
        }
    }
}
