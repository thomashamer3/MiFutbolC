/**
 * @file utils.c
 * @brief Funciones utilitarias para entrada/salida, manejo de fechas y operaciones de base de datos.
 *
 * Este archivo contiene funciones auxiliares para interactuar con el usuario,
 * manejar fechas y horas, limpiar la pantalla, verificar existencia de IDs en la base de datos,
 * y gestionar directorios de exportación.
 */

#include "utils.h"
#include "db.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

/**
 * Permite la entrada de valores numéricos por parte del usuario,
 * facilitando la configuración de parámetros enteros en el sistema.
 */
int input_int(const char *msg)
{
    int v;
    printf("%s", msg);
    scanf("%d", &v);
    getchar();
    return v;
}

/**
 * Valida la entrada de texto para asegurar la integridad de los datos y prevenir errores en el procesamiento posterior,
 * aceptando solo caracteres alfanuméricos y espacios.
 */
void input_string(const char *msg, char *buffer, int size)
{
    int valid = 0;

    while (!valid)
    {
        printf("%s", msg);

        if (fgets(buffer, size, stdin))
        {
            buffer[strcspn(buffer, "\n")] = 0;

            // Validar que contenga solo letras, espacios y números
            valid = 1;
            for (int i = 0; buffer[i] != '\0'; i++)
            {
                if (!isalpha(buffer[i]) && !isspace(buffer[i]) && !isdigit(buffer[i]))
                {
                    valid = 0;
                    break;
                }
            }

            if (!valid)
            {
                printf("Entrada inválida. Solo se permiten letras, espacios y números.\n");
            }
        }
    }
}

/**
 * Proporciona una representación legible de la fecha y hora actual para mostrar en interfaces de usuario,
 * mejorando la experiencia al contextualizar acciones con el tiempo.
 */
void get_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    strftime(buffer, size, "%d/%m/%Y %H:%M", localtime(&t));
}

/**
 * Genera un identificador temporal compacto para usar en nombres de archivos,
 * asegurando unicidad y orden cronológico en exportaciones y backups.
 */
void get_timestamp(char *buffer, int size)
{
    time_t t = time(NULL);
    strftime(buffer, size, "%Y%m%d_%H%M", localtime(&t));
}

/**
 * Verifica la existencia de registros en la base de datos para mantener la integridad referencial
 * y evitar operaciones inválidas que puedan corromper los datos.
 */
int existe_id(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];

    sprintf(sql, "SELECT 1 FROM %s WHERE id=?", tabla);
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);

    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

/**
 * Limpia la pantalla de la consola para proporcionar una interfaz limpia y organizada,
 * mejorando la legibilidad de la información mostrada.
 */
void clear_screen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/**
 * Muestra información contextual del usuario y fecha para personalizar la experiencia
 * y registrar el momento de las operaciones.
 */
void print_header(const char *titulo)
{
    char fecha[20];
    get_datetime(fecha, sizeof(fecha));

    char *nombre_usuario = get_user_name();
    if (!nombre_usuario)
    {
        nombre_usuario = "Usuario Desconocido";
    }

    printf("========================================\n");
    printf(" Usuario: %s\n", nombre_usuario);
    printf(" Fecha  : %s\n", fecha);
    printf(" %s\n", titulo);
    printf("========================================\n\n");

    if (strcmp(nombre_usuario, "Usuario Desconocido") != 0)
    {
        free(nombre_usuario);
    }
}

/**
 * Pausa la ejecución para permitir al usuario revisar información antes de continuar,
 * mejorando la interacción controlada.
 */
void pause_console()
{
    printf("\nPresione ENTER para continuar...");
    getchar();
}

/**
 * Solicita confirmación binaria del usuario para operaciones críticas,
 * previniendo acciones accidentales que puedan afectar datos.
 */
int confirmar(const char *msg)
{
    char c;
    printf("%s (S/N): ", msg);
    c = getchar();
    getchar();

    return (c == 's' || c == 'S');
}

/**
 * Recopila la identidad del usuario en el inicio para personalizar la aplicación
 * y mantener un registro de uso.
 */
void pedir_nombre_usuario()
{
    char nombre[100];
    printf("!Bienvenido a MiFutbolC!\n");
    printf("Por favor, ingresa tu Nombre: ");

    fgets(nombre, sizeof(nombre), stdin);
    nombre[strcspn(nombre, "\n")] = 0;

    // Validar que no esté vacío
    while (strlen(nombre) == 0)
    {
        printf("El nombre no puede estar vacio. Ingresa tu nombre: ");
        fgets(nombre, sizeof(nombre), stdin);
        nombre[strcspn(nombre, "\n")] = 0;
    }

    if (set_user_name(nombre))
    {
        printf("!Bienvenido, %s!\n", nombre);
    }
    else
    {
        printf("Error al guardar el nombre. Intenta nuevamente.\n");
    }
    pause_console();
}

/**
 * Permite al usuario verificar su identidad actual almacenada,
 * facilitando la gestión de su perfil.
 */
void mostrar_nombre_usuario()
{
    char *nombre = get_user_name();
    if (nombre)
    {
        printf("Tu nombre actual es: %s\n", nombre);
        free(nombre);
    }
    else
    {
        printf("No se pudo obtener el nombre del usuario.\n");
    }
    pause_console();
}

/**
 * Habilita la actualización de la identidad del usuario para mantener la información actualizada y personalizada.
 */
void editar_nombre_usuario()
{
    char nombre[100];
    printf("Ingresa tu nuevo nombre: ");

    fgets(nombre, sizeof(nombre), stdin);
    nombre[strcspn(nombre, "\n")] = 0; // Remover newline

    // Validar que no esté vacío
    while (strlen(nombre) == 0)
    {
        printf("El nombre no puede estar vacio. Ingresa tu nuevo nombre: ");
        fgets(nombre, sizeof(nombre), stdin);
        nombre[strcspn(nombre, "\n")] = 0;
    }

    if (set_user_name(nombre))
    {
        printf("Nombre actualizado exitosamente a: %s\n", nombre);
    }
    else
    {
        printf("Error al actualizar el nombre.\n");
    }
    pause_console();
}

/**
 * Proporciona una interfaz estructurada para gestionar opciones relacionadas con el perfil del usuario.
 */
void menu_usuario()
{
    MenuItem items[] =
    {
        {1, "Mostrar Nombre", mostrar_nombre_usuario},
        {2, "Editar Nombre", editar_nombre_usuario},
        {0, "Volver", NULL}
    };

    ejecutar_menu("USUARIO", items, 3);
}

/**
 * Adapta fechas del almacenamiento interno a un formato amigable para la visualización,
 * permitiendo flexibilidad en formatos futuros.
 */
void format_date_for_display(const char *input_date, char *output_buffer, int buffer_size)
{
    // Actualmente el formato de almacenamiento y visualización son iguales
    // Copiar directamente la fecha de entrada a la salida
    strncpy(output_buffer, input_date, buffer_size - 1);
    output_buffer[buffer_size - 1] = '\0';
}

/**
 * Convierte fechas ingresadas por el usuario a un formato interno consistente,
 * facilitando el almacenamiento y procesamiento uniforme.
 */
void convert_display_date_to_storage(const char *display_date, char *storage_buffer, int buffer_size)
{
    // Actualmente el formato de almacenamiento y visualización son iguales
    // Copiar directamente la fecha de visualización al almacenamiento
    strncpy(storage_buffer, display_date, buffer_size - 1);
    storage_buffer[buffer_size - 1] = '\0';
}

/**
 * Normaliza cadenas de texto removiendo caracteres acentuados para asegurar compatibilidad con sistemas que no los soportan
 * y mejorar la consistencia en búsquedas.
 */
char* remover_tildes(const char *str)
{
    static char buffer[256];
    int i, j = 0;

    for (i = 0; str[i] != '\0' && i < sizeof(buffer) - 1; i++)
    {
        unsigned char c = str[i];
        if (c == 0xE1 || c == 0xC1) buffer[j++] = 'a'; // á, Á
        else if (c == 0xE9 || c == 0xC9) buffer[j++] = 'e'; // é, É
        else if (c == 0xED || c == 0xCD) buffer[j++] = 'i'; // í, Í
        else if (c == 0xF3 || c == 0xD3) buffer[j++] = 'o'; // ó, Ó
        else if (c == 0xFA || c == 0xDA) buffer[j++] = 'u'; // ú, Ú
        else if (c == 0xF1 || c == 0xD1) buffer[j++] = 'n'; // ñ, Ñ
        else if (c == 0xFC || c == 0xDC) buffer[j++] = 'u'; // ü, Ü
        else buffer[j++] = str[i];
    }
    buffer[j] = '\0';
    return buffer;
}
