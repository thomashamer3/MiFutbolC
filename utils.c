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
 * @brief Solicita al usuario un número entero.
 *
 * Muestra el mensaje proporcionado y lee un entero desde la entrada estándar.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @return El número entero ingresado por el usuario.
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
 * @brief Solicita al usuario una cadena de texto.
 *
 * Muestra el mensaje proporcionado y lee una cadena desde la entrada estándar,
 * eliminando el carácter de nueva línea al final.
 * Valida que la entrada contenga solo letras y espacios.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @param buffer El buffer donde se almacenará la cadena leída.
 * @param size El tamaño máximo del buffer.
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
 * @brief Obtiene la fecha y hora actual en formato legible.
 *
 * Formatea la fecha y hora actual en el formato "dd/mm/yyyy hh:mm".
 *
 * @param buffer El buffer donde se almacenará la cadena formateada.
 * @param size El tamaño máximo del buffer.
 */
void get_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    strftime(buffer, size, "%d/%m/%Y %H:%M", localtime(&t));
}

/**
 * @brief Obtiene un timestamp actual en formato compacto.
 *
 * Formatea la fecha y hora actual en el formato "yyyymmdd_hhmm" para usar en nombres de archivos.
 *
 * @param buffer El buffer donde se almacenará la cadena formateada.
 * @param size El tamaño máximo del buffer.
 */
void get_timestamp(char *buffer, int size)
{
    time_t t = time(NULL);
    strftime(buffer, size, "%Y%m%d_%H%M", localtime(&t));
}

/**
 * @brief Verifica si existe un ID en una tabla de la base de datos.
 *
 * Ejecuta una consulta SQL para comprobar si un ID específico existe en la tabla indicada.
 *
 * @param tabla El nombre de la tabla a consultar.
 * @param id El ID a verificar.
 * @return 1 si el ID existe, 0 en caso contrario.
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
 * @brief Limpia la pantalla de la consola.
 *
 * Ejecuta el comando apropiado para limpiar la pantalla dependiendo del sistema operativo.
 * En Windows usa "cls", en sistemas Unix/Linux usa "clear".
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
 * @brief Imprime un encabezado formateado en la consola.
 *
 * Muestra un encabezado con el nombre del usuario, la fecha actual y el título proporcionado,
 * rodeado de líneas decorativas.
 *
 * @param titulo El título a mostrar en el encabezado.
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

void pause_console()
{
    printf("\nPresione ENTER para continuar...");
    getchar();
}

/**
 * @brief Solicita confirmación al usuario (Sí/No).
 *
 * Muestra el mensaje proporcionado seguido de "(S/N):" y lee la respuesta del usuario.
 * Acepta 's' o 'S' como afirmativo.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @return 1 si el usuario confirma (sí), 0 en caso contrario.
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
 * @brief Pide el nombre del usuario en la primera ejecución
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
 * @brief Muestra el nombre actual del usuario
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
 * @brief Permite editar el nombre del usuario
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
 * @brief Menú de gestión de usuario
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
