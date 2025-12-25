#ifndef MENU_H
#define MENU_H

typedef struct
{
    int opcion;
    const char *texto;
    void (*accion)();
} MenuItem;

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
void ejecutar_menu(const char *titulo, MenuItem *items, int cantidad);

#endif
