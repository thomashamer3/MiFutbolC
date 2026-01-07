#include "menu.h"
#include "utils.h"
#include <stdio.h>

// Use an infinite loop to maintain menu interaction until the user explicitly chooses to exit,
// ensuring the interface remains responsive and allows repeated selections without restarting the program.
void ejecutar_menu(const char *titulo, MenuItem *items, int cantidad)
{
    int opcion;

    while (1)
    {
        // Clear screen each iteration to provide a clean, focused interface and prevent menu clutter from previous interactions.
        clear_screen();

        print_header(titulo);

        // Display all menu options to give user clear visibility of available actions.
        for (int i = 0; i < cantidad; i++)
            printf("%d.%s\n", items[i].opcion, items[i].texto);

        opcion = input_int(">");

        // Iterate through items to find matching option, allowing flexible menu configuration without hardcoded logic.
        for (int i = 0; i < cantidad; i++)
        {
            if (items[i].opcion == opcion)
            {
                // If action is NULL, treat as exit option to break the menu loop cleanly.
                if (!items[i].accion)
                    return;

                // Execute the selected action, delegating control to maintain modular design.
                items[i].accion();
            }
        }
    }
}
