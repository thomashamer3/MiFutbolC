/**
 * @file utils.c
 * @brief Funciones utilitarias para entrada/salida, manejo de fechas y
 * operaciones de base de datos.
 *
 * Este archivo contiene funciones auxiliares para interactuar con el usuario,
 * manejar fechas y horas, limpiar la pantalla, verificar existencia de IDs en
 * la base de datos, y gestionar directorios de exportación.
 */

#include "utils.h"
#include "ascii_art.h"
#include "db.h"
#include "menu.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>
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
    if (scanf_s("%d", &v) != 1) {
        v = 0;
        while (getchar() != '\n'); // Clear input buffer
    }
    getchar();
    return v;
}

/* Implementación portable de safe_strnlen */
size_t safe_strnlen(const char *s, size_t maxlen)
{
    if (s == NULL)
        return 0;
    size_t i;
    for (i = 0; i < maxlen; i++)
    {
        if (s[i] == '\0')
            break;
    }
    return i;
}

/**
 * Determina si un punto en la posición dada es un separador de miles o decimal.
 * Un punto se considera separador de miles si hay al menos 3 dígitos después de
 * él.
 */
static int is_thousands_separator(const char *buffer, int position)
{
    int remaining_digits = 0;

    // Contar dígitos después del punto actual
    for (int k = position + 1; buffer[k] != '\0'; k++)
    {
        if (isdigit(buffer[k]))
        {
            remaining_digits++;
        }
        else if (buffer[k] == ',' || buffer[k] == '.')
        {
            break;
        }
    }

    // Si hay al menos 3 dígitos después, es separador de miles
    return remaining_digits >= 3;
}

/**
 * Procesa un carácter individual y lo agrega al buffer de salida si es válido.
 * Devuelve 1 si se agregó un carácter, 0 si se omitió, -1 si se alcanzó el
 * límite.
 */
static int process_character(char c, char *output, size_t *j,
                             size_t output_size, int *has_decimal,
                             const char *input, int i)
{
    // Procesar coma como separador decimal
    if (c == ',' && !*has_decimal)
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar punto (puede ser separador de miles o decimal)
    if (c == '.' && !*has_decimal)
    {
        if (is_thousands_separator(input, i))
            return 0; // Omitir separador de miles
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar dígitos
    if (isdigit(c))
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = c;
        return 1;
    }

    // Ignorar otros caracteres
    return 0;
}

/**
 * Procesa un buffer de entrada para convertir separadores de miles y decimales
 * a formato estándar. Convierte comas a puntos decimales y elimina separadores
 * de miles.
 */
static void process_numeric_input(const char *input, char *output,
                                  size_t output_size)
{
    size_t j = 0;
    int has_decimal = 0;
    int ok = 1;

    if (input == NULL || output == NULL || output_size == 0)
    {
        if (output && output_size > 0)
            output[0] = '\0';
        return;
    }

    int i = 0;
    while (input[i] != '\0' && j < output_size - 1 && ok)
    {
        char c = input[i];

        ok = (process_character(c, output, &j, output_size, &has_decimal, input,
                                i) != -1);
        i++;
    }

    output[j] = '\0';
}

/**
 * Permite la entrada de valores de punto flotante por parte del usuario,
 * facilitando la configuración de parámetros decimales en el sistema.
 * Acepta tanto punto como coma como separador decimal, y maneja separadores de
 * miles.
 */
double input_double(const char *msg)
{
    char buffer[100];
    char processed[100];
    double v = 0.0;

    while (1)
    {
        printf("%s", msg);

        if (!fgets(buffer, sizeof(buffer), stdin))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        process_numeric_input(buffer, processed, sizeof(processed));

        if (sscanf_s(processed, "%lf", &v) == 1)
            return v;
        printf("Entrada inválida. Ingrese un número válido (ej: 250, 1.500, "
               "12.500, 250.000): ");
    }
}

/**
 * Valida la entrada de texto para asegurar la integridad de los datos y
 * prevenir errores en el procesamiento posterior, aceptando solo caracteres
 * alfanuméricos y espacios.
 */
void input_string(const char *msg, char *buffer, int size)
{
    while (1)
    {
        printf("%s", msg);

        if (!fgets(buffer, size, stdin))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        int valid = 1;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            if (!isalpha(buffer[i]) && !isspace(buffer[i]) && !isdigit(buffer[i]))
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            return;
        printf("Entrada inválida. Solo se permiten letras, espacios y números.\n");
    }
}

/**
 * Valida la entrada de fecha para asegurar el formato correcto,
 * aceptando solo dígitos, barras diagonales (/) y dos puntos (:).
 */
void input_date(const char *msg, char *buffer, int size)
{
    while (1)
    {
        printf("%s", msg);

        if (!fgets(buffer, size, stdin))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        int valid = 1;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            if (!isdigit(buffer[i]) && buffer[i] != '/' && buffer[i] != ':')
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            return;
        printf("Entrada inválida. Solo se permiten dígitos, barras diagonales (/) "
               "y dos puntos (:).\n");
    }
}

/**
 * Proporciona una representación legible de la fecha y hora actual para mostrar
 * en interfaces de usuario, mejorando la experiencia al contextualizar acciones
 * con el tiempo.
 */
void get_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

/**
 * Genera un identificador temporal compacto para usar en nombres de archivos,
 * asegurando unicidad y orden cronológico en exportaciones y backups.
 */
void get_timestamp(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%Y%m%d_%H%M", &tm_struct);
}

/**
 * Verifica la existencia de registros en la base de datos para mantener la
 * integridad referencial y evitar operaciones inválidas que puedan corromper
 * los datos.
 */
int existe_id(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];

    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s WHERE id=?", tabla);
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);

    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

/**
 * Limpia la pantalla de la consola para proporcionar una interfaz limpia y
 * organizada, mejorando la legibilidad de la información mostrada.
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
 * Muestra información contextual del usuario y fecha para personalizar la
 * experiencia y registrar el momento de las operaciones, incluyendo arte ASCII
 * contextual.
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
    else if (strstr(titulo, "ANALISIS") ||
             strstr(titulo, "EVOLUCION TEMPORAL"))
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
 * Pausa la ejecución para permitir al usuario revisar información antes de
 * continuar, mejorando la interacción controlada.
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
    int c;
    printf("%s (S/N): ", msg);
    c = getchar();
    getchar();

    return (c == 's' || c == 'S');
}

/**
 * Recopila la identidad del usuario en el inicio para personalizar la
 * aplicación y mantener un registro de uso.
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
    while (safe_strnlen(nombre, sizeof(nombre)) == 0)
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
 * Habilita la actualización de la identidad del usuario para mantener la
 * información actualizada y personalizada.
 */
void editar_nombre_usuario()
{
    char nombre[100];
    printf("Ingresa tu nuevo nombre: ");

    fgets(nombre, sizeof(nombre), stdin);
    nombre[strcspn(nombre, "\n")] = 0; // Remover newline

    // Validar que no esté vacío
    while (safe_strnlen(nombre, sizeof(nombre)) == 0)
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
 * Proporciona una interfaz estructurada para gestionar opciones relacionadas
 * con el perfil del usuario.
 */
void menu_usuario()
{
    MenuItem items[] = {{1, "Mostrar Nombre", mostrar_nombre_usuario},
        {2, "Editar Nombre", editar_nombre_usuario},
        {0, "Volver", NULL}
    };

    ejecutar_menu("USUARIO", items, 3);
}

/**
 * Adapta fechas del almacenamiento interno a un formato amigable para la
 * visualización, permitiendo flexibilidad en formatos futuros.
 */
void format_date_for_display(const char *input_date, char *output_buffer,
                             int buffer_size)
{
    // Actualmente el formato de almacenamiento y visualización son iguales
    // Copiar directamente la fecha de entrada a la salida
    strncpy_s(output_buffer, buffer_size, input_date, buffer_size - 1);
}

/**
 * Convierte fechas ingresadas por el usuario a un formato interno consistente,
 * facilitando el almacenamiento y procesamiento uniforme.
 */
void convert_display_date_to_storage(const char *display_date,
                                     char *storage_buffer, int buffer_size)
{
    // Actualmente el formato de almacenamiento y visualización son iguales
    // Copiar directamente la fecha de visualización al almacenamiento
    strncpy_s(storage_buffer, buffer_size, display_date, buffer_size - 1);
}

/**
 * Normaliza cadenas de texto removiendo caracteres acentuados para asegurar
 * compatibilidad con sistemas que no los soportan y mejorar la consistencia en
 * búsquedas.
 */
char *remover_tildes(const char *str)
{
    static char buffer[256];
    size_t j = 0;

    const char *p = str;
    while (*p != '\0' && j < sizeof(buffer) - 1)
    {
        unsigned char c = (unsigned char)*p++;
        if (c == 0xE1 || c == 0xC1)
            buffer[j++] = 'a'; // á, Á
        else if (c == 0xE9 || c == 0xC9)
            buffer[j++] = 'e'; // é, É
        else if (c == 0xED || c == 0xCD)
            buffer[j++] = 'i'; // í, Í
        else if (c == 0xF3 || c == 0xD3)
            buffer[j++] = 'o'; // ó, Ó
        else if (c == 0xFA || c == 0xDA)
            buffer[j++] = 'u'; // ú, Ú
        else if (c == 0xF1 || c == 0xD1)
            buffer[j++] = 'n'; // ñ, Ñ
        else if (c == 0xFC || c == 0xDC)
            buffer[j++] = 'u'; // ü, Ü
        else
            buffer[j++] = (char)c;
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

/**
 * Obtiene el nombre de una entidad por su ID desde la base de datos.
 * Función genérica para evitar duplicación de código en consultas SQL comunes.
 *
 * @param tabla Nombre de la tabla (ej: "camiseta", "torneo", "cancha")
 * @param id ID de la entidad a buscar
 * @param buffer Buffer donde se almacenará el nombre encontrado
 * @param size Tamaño máximo del buffer
 * @return 1 si se encontró la entidad, 0 si no se encontró
 */
int obtener_nombre_entidad(const char *tabla, int id, char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !buffer || size == 0)
    {
        return 0;
    }

    snprintf(sql, sizeof(sql), "SELECT nombre FROM %s WHERE id = ?", tabla);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        if (nombre)
        {
            snprintf(buffer, size, "%s", nombre);
            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

/**
 * @brief Obtiene el siguiente ID disponible para una tabla
 * Implementa el patrón usado en camiseta, cancha, lesion, etc.
 */
int obtener_siguiente_id(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[512];

    if (!tabla) return 1;

    snprintf(sql, sizeof(sql),
             "WITH RECURSIVE seq(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM seq WHERE id < (SELECT COALESCE(MAX(id),0)+1 FROM %s)) "
             "SELECT MIN(id) FROM seq WHERE id NOT IN (SELECT id FROM %s)",
             tabla, tabla);

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    int id = 1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief Verifica si hay registros en una tabla
 */
int hay_registros(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla) return 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", tabla);
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return count > 0;
}

/**
 * @brief Obtiene el ID de una entidad por nombre
 */
int obtener_id_por_nombre(const char *tabla, const char *nombre)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !nombre) return -1;

    snprintf(sql, sizeof(sql), "SELECT id FROM %s WHERE nombre = ?", tabla);
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return id;
}

/**
 * @brief Lista todas las entidades de una tabla
 */
void listar_entidades(const char *tabla, const char *titulo, const char *mensaje_vacio)
{
    if (!tabla || !titulo || !mensaje_vacio) return;

    clear_screen();
    print_header(titulo);

    sqlite3_stmt *stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, nombre FROM %s ORDER BY id", tabla);
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d - %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        printf("%s\n", mensaje_vacio);

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Solicita un entero validado en rango
 */
int input_int_rango(const char *msg, int min, int max)
{
    int valor;
    do
    {
        valor = input_int(msg);
        if (valor < min || valor > max)
            printf("Ingrese un valor entre %d y %d.\n", min, max);
    }
    while (valor < min || valor > max);
    return valor;
}

/**
 * @brief Muestra "no hay registros"
 */
void mostrar_no_hay_registros(const char *entidad)
{
    if (!entidad) return;
    size_t len = safe_strnlen(entidad, SIZE_MAX);
    printf("No hay %s registrad%s.\n", entidad,
           (entidad[len-1] == 'a' || entidad[len-1] == 'o') ? "o" : "os");
}

/**
 * @brief Muestra "entidad no existe"
 */
void mostrar_no_existe(const char *entidad)
{
    if (!entidad) return;
    printf("El %s no existe.\n", entidad);
}

/**
 * @brief Muestra error de operación
 */
void mostrar_error_operacion(const char *entidad, const char *operacion)
{
    if (!entidad || !operacion) return;
    printf("Error al %s el %s.\n", operacion, entidad);
}

/**
 * @brief Atajo para clear_screen + print_header
 */
void mostrar_pantalla(const char *titulo)
{
    clear_screen();
    print_header(titulo);
}

/**
 * @brief Elimina espacios en blanco al final de una cadena
 */
char* trim_trailing_spaces(char *str)
{
    if (!str) return str;

    size_t len = safe_strnlen(str, SIZE_MAX);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n' || str[len-1] == '\r'))
    {
        str[--len] = '\0';
    }
    return str;
}
/**
 * @brief Verifica si hay registros en una tabla específica
 * Función común para evitar duplicación de código en funciones de exportación
 */
int has_records(const char *table)
{
    sqlite3_stmt *check_stmt;
    char sql[128];
    int count = 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table);
    sqlite3_prepare_v2(db, sql, -1, &check_stmt, NULL);
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    return count > 0;
}

/**
 * @brief Ejecuta una consulta SQL y devuelve el statement preparado
 * Función común para centralizar la ejecución de consultas
 */
sqlite3_stmt* execute_query(const char *sql)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }
    return stmt;
}

/**
 * @brief Lista equipos disponibles para selección
 * Función común usada en múltiples módulos para mostrar equipos
 */
void list_available_teams()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== EQUIPOS DISPONIBLES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            printf("%d. %s\n", id, nombre);
        }

        if (!found)
        {
            mostrar_no_hay_registros("equipos registrados");
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Obtiene el ID de un equipo seleccionado por el usuario
 * Función común para selección de equipos con validación
 */
int select_team_id(const char *prompt)
{
    list_available_teams();
    int equipo_id = input_int(prompt);
    if (equipo_id == 0 || !existe_id("equipo", equipo_id))
    {
        printf("ID de equipo inválido.\n");
        return 0;
    }
    return equipo_id;
}

/**
 * @brief Escribe encabezado CSV para exportaciones
 * Función común para formato consistente en exportaciones CSV
 */
void write_csv_header(FILE *f, const char *header)
{
    fprintf(f, "%s\n", header);
}

/**
 * @brief Escribe fila CSV con datos de partido
 * Función común para exportar datos de partidos a CSV
 */
void write_partido_csv_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
    trim_trailing_spaces(cancha_trimmed);
    fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Escribe fila TXT con datos de partido
 * Función común para exportar datos de partidos a TXT
 */
void write_partido_txt_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
    trim_trailing_spaces(cancha_trimmed);
    fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Escribe objeto JSON con datos de partido
 * Función común para exportar datos de partidos a JSON
 */
void write_partido_json_object(cJSON *item, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
    trim_trailing_spaces(cancha_trimmed);

    cJSON_AddStringToObject(item, "cancha", cancha_trimmed);
    cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 3));
    cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
    cJSON_AddStringToObject(item, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
    cJSON_AddStringToObject(item, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
    cJSON_AddStringToObject(item, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
    cJSON_AddNumberToObject(item, "rendimiento_general", sqlite3_column_int(stmt, 8));
    cJSON_AddNumberToObject(item, "cansancio", sqlite3_column_int(stmt, 9));
    cJSON_AddNumberToObject(item, "estado_animo", sqlite3_column_int(stmt, 10));
    cJSON_AddStringToObject(item, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));

    free(cancha_trimmed);
}

/**
 * @brief Escribe fila HTML con datos de partido
 * Función común para exportar datos de partidos a HTML
 */
void write_partido_html_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
    trim_trailing_spaces(cancha_trimmed);
    fprintf(f,
            "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Función común para mostrar récords simples
 * Centraliza la lógica de mostrar récords con formato consistente
 */
void mostrar_record_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Valor: %d\n", sqlite3_column_int(stmt, 0));
        if (sqlite3_column_count(stmt) > 1)
        {
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        }
        if (sqlite3_column_count(stmt) > 2)
        {
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 2));
        }
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Función común para mostrar combinaciones cancha-camiseta
 * Centraliza la lógica de mostrar combinaciones con formato consistente
 */
void mostrar_combinacion_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Cancha: %s\n", sqlite3_column_text(stmt, 0));
        printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 2));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 3));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Función común para mostrar temporadas
 * Centraliza la lógica de mostrar temporadas con formato consistente
 */
void mostrar_temporada_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* year = (const char*)sqlite3_column_text(stmt, 0);
        if (year)
        {
            printf("Año: %s\n", year);
        }
        else
        {
            printf("Año: Desconocido\n");
        }
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 1));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 2));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Función genérica para mostrar récords simples
 * Centraliza la lógica de mostrar récords con formato consistente
 */
void mostrar_record_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Valor: %d\n", sqlite3_column_int(stmt, 0));
        if (sqlite3_column_count(stmt) > 1)
        {
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        }
        if (sqlite3_column_count(stmt) > 2)
        {
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 2));
        }
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Función genérica para exportar récords a CSV
 * Centraliza la lógica de exportación CSV para récords individuales
 */
void exportar_record_simple_csv(const char *titulo, const char *sql, const char *filename)
{
    FILE *file = NULL;
    errno_t err = fopen_s(&file, get_export_path(filename), "w");
    if (err != 0 || !file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Valor,Camiseta,Fecha\n");

    sqlite3_stmt *stmt = execute_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2));
    }

    if (stmt) sqlite3_finalize(stmt);
    printf("Exportado a %s\n", get_export_path(filename));
    fclose(file);
}

/**
 * @brief Función genérica para exportar un partido específico a CSV
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_csv(const char *order_by, const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    write_csv_header(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal");

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_csv_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Función genérica para exportar un partido específico a TXT
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_txt(const char *order_by, const char *filename, const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    fprintf(f, "%s\n\n", title);

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_txt_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Función genérica para exportar un partido específico a JSON
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_json(const char *order_by, const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_json_object(root, stmt);
        }
        sqlite3_finalize(stmt);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Función genérica para exportar un partido específico a HTML
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_html(const char *order_by, const char *filename, const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    fprintf(f, "<html><body><h1>%s</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>",
            title);

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_html_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(f, "</table></body></html>");
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}