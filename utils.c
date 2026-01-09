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
#include "ascii_art.h"
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
 * Permite la entrada de valores de punto flotante por parte del usuario,
 * facilitando la configuración de parámetros decimales en el sistema.
 * Acepta tanto punto como coma como separador decimal, y maneja separadores de miles.
 */
double input_double(const char *msg)
{
    char buffer[100];
    char processed[100];
    double v = 0.0;
    int valid = 0;

    while (!valid)
    {
        printf("%s", msg);

        if (fgets(buffer, sizeof(buffer), stdin))
        {
            // Remover newline
            buffer[strcspn(buffer, "\n")] = 0;

            // Procesar el buffer para manejar separadores de miles y decimales
            int j = 0;
            int has_decimal = 0;

            for (int i = 0; buffer[i] != '\0' && j < sizeof(processed) - 1; i++)
            {
                char c = buffer[i];

                if (c == ',' && !has_decimal)
                {
                    // Primera coma encontrada, tratar como separador decimal
                    processed[j++] = '.';
                    has_decimal = 1;
                }
                else if (c == '.' && !has_decimal)
                {
                    // Verificar si es un separador de miles o decimal
                    // Si hay dígitos después y estamos en una posición que sugiere separador de miles
                    int remaining_digits = 0;
                    for (int k = i + 1; buffer[k] != '\0'; k++)
                    {
                        if (isdigit(buffer[k])) remaining_digits++;
                        else if (buffer[k] == ',' || buffer[k] == '.') break;
                    }

                    // Si hay exactamente 3 dígitos antes del siguiente separador, es separador de miles
                    if (remaining_digits >= 3)
                    {
                        // Omitir este punto (separador de miles)
                        continue;
                    }
                    else
                    {
                        // Es separador decimal
                        processed[j++] = '.';
                        has_decimal = 1;
                    }
                }
                else if (isdigit(c))
                {
                    processed[j++] = c;
                }
                // Ignorar otros caracteres
            }
            processed[j] = '\0';

            // Intentar convertir a double
            if (sscanf(processed, "%lf", &v) == 1)
            {
                valid = 1;
            }
            else
            {
                printf("Entrada inválida. Ingrese un número válido (ej: 250, 1.500, 12.500, 250.000): ");
            }
        }
    }

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
 * Valida la entrada de fecha para asegurar el formato correcto,
 * aceptando solo dígitos, barras diagonales (/) y dos puntos (:).
 */
void input_date(const char *msg, char *buffer, int size)
{
    int valid = 0;

    while (!valid)
    {
        printf("%s", msg);

        if (fgets(buffer, size, stdin))
        {
            buffer[strcspn(buffer, "\n")] = 0;

            // Validar que contenga solo dígitos, barras diagonales y dos puntos
            valid = 1;
            for (int i = 0; buffer[i] != '\0'; i++)
            {
                if (!isdigit(buffer[i]) && buffer[i] != '/' && buffer[i] != ':')
                {
                    valid = 0;
                    break;
                }
            }

            if (!valid)
            {
                printf("Entrada inválida. Solo se permiten dígitos, barras diagonales (/) y dos puntos (:).\n");
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
 * y registrar el momento de las operaciones, incluyendo arte ASCII contextual.
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

    // Mostrar arte ASCII contextual según el título del menú
    if (strstr(titulo, "MI FUTBOL C"))
    {
        printf("%s\n", ASCII_BIENVENIDA);
    }
    else if (strstr(titulo, "CAMISETA") || strstr(titulo, "CAMISETAS"))
    {
        printf("%s\n", ASCII_CAMISETA);
    }
    else if (strstr(titulo, "CANCHAS"))
    {
        printf("%s\n", ASCII_CANCHA);
    }
    else if (strstr(titulo, "PARTIDO") || strstr(titulo, "PARTIDOS"))
    {
        printf("%s\n", ASCII_FUTBOL);
    }
    else if (strstr(titulo, "EQUIPOS"))
    {
        printf("%s\n", ASCII_EQUIPO);
    }
    else if (strstr(titulo, "ESTADISTICA") || strstr(titulo, "ESTADISTICAS"))
    {
        printf("%s\n", ASCII_ESTADISTICAS);
    }
    else if (strstr(titulo, "LOGROS"))
    {
        printf("%s\n", ASCII_LOGROS);
    }
    else if (strstr(titulo, "ANALISIS") || strstr(titulo, "EVOLUCION TEMPORAL"))
    {
        printf("%s\n", ASCII_ANALISIS);
    }
    else if (strstr(titulo, "LESIONES"))
    {
        printf("%s\n", ASCII_LESIONES);
    }
    else if (strstr(titulo, "FINANCIAMIENTO"))
    {
        printf("%s\n", ASCII_FINANCIAMIENTO);
    }
    else if (strstr(titulo, "EXPORTAR"))
    {
        printf("%s\n", ASCII_EXPORTAR);
    }
    else if (strstr(titulo, "IMPORTAR"))
    {
        printf("%s\n", ASCII_IMPORTAR);
    }
    else if (strstr(titulo, "TORNEOS"))
    {
        printf("%s\n", ASCII_TORNEOS);
    }
    else if (strstr(titulo, "AJUSTES") || strstr(titulo, "SETTINGS"))
    {
        printf("%s\n", ASCII_AJUSTES);
    }

    printf("========================================\n");
    printf(" Usuario: %s\n", nombre_usuario);
    printf(" Fecha  : %s\n", fecha);
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
    clear_screen();
    printf("%s\n", ASCII_BIENVENIDA);
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

/**
 * Convierte un valor de resultado a texto
 *
 * @param resultado El valor numérico del resultado
 * @return La representación textual del resultado
 */
const char *resultado_to_text(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "VICTORIA";
    case 2:
        return "EMPATE";
    case 3:
        return "DERROTA";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Convierte un valor de clima a texto
 *
 * @param clima El valor numérico del clima
 * @return La representación textual del clima
 */
const char *clima_to_text(int clima)
{
    switch (clima)
    {
    case 1:
        return "Despejado";
    case 2:
        return "Nublado";
    case 3:
        return "Lluvia";
    case 4:
        return "Ventoso";
    case 5:
        return "Mucho Calor";
    case 6:
        return "Mucho Frio";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Convierte un valor de día a texto
 *
 * @param dia El valor numérico del día
 * @return La representación textual del día
 */
const char *dia_to_text(int dia)
{
    switch (dia)
    {
    case 1:
        return "Dia";
    case 2:
        return "Tarde";
    case 3:
        return "Noche";
    default:
        return "DESCONOCIDO";
    }
}
