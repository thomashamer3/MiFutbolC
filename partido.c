#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include "equipo.h"
#include "ascii_art.h"
#include "entrenador_ia.h"
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <stdlib.h>
#include <time.h>
#include <process.h>
#include <memory.h>

// Prototipos de funciones estáticas usadas antes de su definición
static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo);
static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo);
static void guardar_estadisticas_equipo(const Equipo *equipo, int const *estadisticas, int const *asistencias,
                                        int resultado, int cancha_id, char const *fecha_simulacion);

/**
 * @brief Estructura para agrupar los datos de un partido
 *
 * Esta estructura se utiliza para reducir el número de parámetros en funciones
 * y mejorar la organización del código.
 */
typedef struct
{
    int cancha_id;
    int goles;
    int asistencias;
    int camiseta;
    int resultado;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    char comentario_personal[256];
    int clima;
    int dia;
} DatosPartido;

/**
 * @brief Estructura para agrupar estadísticas de un partido
 *
 * Esta estructura se utiliza para reducir el número de parámetros en funciones
 * de simulación y resultados, agrupando las estadísticas de ambos equipos.
 */
typedef struct
{
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} EstadisticasPartido;

/**
 * @brief Estructura para agrupar datos de simulación de partido
 *
 * Esta estructura agrupa todos los datos necesarios para la simulación
 * y guardado de resultados de un partido, reduciendo la cantidad de
 * parámetros en las funciones relacionadas.
 */
typedef struct
{
    Equipo equipo_local;
    Equipo equipo_visitante;
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} DatosSimulacion;

/**
 * @brief Generates a random number using standard rand()
 *
 * This function provides a simple random number generator for non-critical uses.
 * For cryptographic applications, consider using a CSPRNG library.
 *
 * @param max Maximum value (exclusive)
 * @return Random number in range [0, max)
 */
static int secure_rand(int max)
{
    if (max <= 0)
        return 0;
    return (unsigned int)rand() % max;
}

/**
 * @brief Verifica que existan canchas y camisetas antes de crear un partido
 *
 * Para mantener la integridad de los datos, se asegura de que haya entidades relacionadas
 * disponibles antes de permitir la creación de un nuevo partido.
 *
 * @return 1 si hay entidades disponibles, 0 si no
 */
static int verificar_prerrequisitos_partido()
{
    sqlite3_stmt *stmt_count_canchas;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM cancha", -1, &stmt_count_canchas, NULL);
    sqlite3_step(stmt_count_canchas);
    int count_canchas = sqlite3_column_int(stmt_count_canchas, 0);
    sqlite3_finalize(stmt_count_canchas);

    sqlite3_stmt *stmt_count_camisetas;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM camiseta", -1, &stmt_count_camisetas, NULL);
    sqlite3_step(stmt_count_camisetas);
    int count_camisetas = sqlite3_column_int(stmt_count_camisetas, 0);
    sqlite3_finalize(stmt_count_camisetas);

    if (count_canchas == 0 && count_camisetas == 0)
    {
        printf("No se puede crear un partido porque no hay canchas ni camisetas registradas.\n");
        pause_console();
        return 0;
    }
    return 1;
}

/**
 * @brief Muestra la lista de canchas disponibles para selección
 *
 * Facilita la selección de cancha al usuario mostrando las opciones disponibles.
 */
static void listar_canchas_disponibles()
{
    printf("Canchas disponibles:\n");
    sqlite3_stmt *stmt_canchas;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM cancha ORDER BY id", -1, &stmt_canchas, NULL);
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        printf("%d | %s\n", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);
}

/**
 * @brief Recopila todos los datos necesarios para un partido desde el usuario
 *
 * Valida cada entrada para asegurar que los datos sean correctos antes de proceder.
 * Utiliza bucles para reintentar entradas inválidas, mejorando la experiencia del usuario.
 *
 * @param datos Puntero a la estructura DatosPartido que contendrá los datos recopilados
 */
static void recopilar_datos_partido(DatosPartido *datos)
{
    // Initialize all fields to safe default values
    datos->cancha_id = 0;
    datos->goles = 0;
    datos->asistencias = 0;
    datos->camiseta = 0;
    datos->resultado = 0;
    datos->rendimiento_general = 0;
    datos->cansancio = 0;
    datos->estado_animo = 0;
    datos->clima = 0;
    datos->dia = 0;
    strcpy_s(datos->comentario_personal, sizeof(datos->comentario_personal), "");

    datos->cancha_id = input_int("ID Cancha, (0 para Cancelar): ");
    if (!existe_id("cancha", datos->cancha_id))
        return;
    datos->goles = input_int("Goles: ");
    datos->asistencias = input_int("Asistencias: ");
    datos->resultado = input_int("Resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (datos->resultado < 1 || datos->resultado > 3)
    {
        datos->resultado = input_int("Resultado invalido. (1=VICTORIA, 2=EMPATE, 3=DERROTA):");
    }
    listar_camisetas();
    datos->camiseta = input_int("ID Camiseta: ");
    if (!existe_id("camiseta", datos->camiseta))
        return;
    datos->rendimiento_general = input_int("Rendimiento general (1-10): ");
    while (datos->rendimiento_general < 1 || datos->rendimiento_general > 10)
    {
        datos->rendimiento_general = input_int("Rendimiento invalido. Ingrese entre 1 y 10: ");
    }
    datos->cansancio = input_int("Cansancio (1-10): ");
    while (datos->cansancio < 1 || datos->cansancio > 10)
    {
        datos->cansancio = input_int("Cansancio invalido. Ingrese entre 1 y 10:  ");
    }
    datos->estado_animo = input_int("Estado de Animo (1-10): ");
    while (datos->estado_animo < 1 || datos->estado_animo > 10)
    {
        datos->estado_animo = input_int("Estado de Animo invalido. Ingrese entre 1 y 10: ");
    }
    input_string("Comentario personal: ", datos->comentario_personal, 256);
    datos->clima = input_int("Clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio):");
    while (datos->clima < 1 || datos->clima > 6)
    {
        datos->clima = input_int("Clima invalido (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    }
    datos->dia = input_int("Dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (datos->dia < 1 || datos->dia > 3)
    {
        datos->dia = input_int("Dia invalido (1=Dia, 2=Tarde, 3=Noche): ");
    }
}

/**
 * @brief Inserta un nuevo partido en la base de datos
 *
 * Utiliza prepared statements para evitar inyección SQL y asegurar integridad de datos.
 * Maneja errores de SQLite para informar al usuario si la inserción falla.
 *
 * @param id ID del partido
 * @param datos Puntero a la estructura DatosPartido que contiene los datos del partido
 * @param fecha Fecha y hora
 */
static void insertar_partido(long long id, DatosPartido const *datos, char const *fecha)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "INSERT INTO partido(id, cancha_id,fecha_hora,goles,asistencias,camiseta_id,resultado,rendimiento_general,cansancio,estado_animo,comentario_personal,clima,dia)"
                       "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
                       -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, datos->cancha_id);
    sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, datos->goles);
    sqlite3_bind_int(stmt, 5, datos->asistencias);
    sqlite3_bind_int(stmt, 6, datos->camiseta);
    sqlite3_bind_int(stmt, 7, datos->resultado);
    sqlite3_bind_int(stmt, 8, datos->rendimiento_general);
    sqlite3_bind_int(stmt, 9, datos->cansancio);
    sqlite3_bind_int(stmt, 10, datos->estado_animo);
    sqlite3_bind_text(stmt, 11, datos->comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, datos->clima);
    sqlite3_bind_int(stmt, 13, datos->dia);
    int result = sqlite3_step(stmt);
    if (result == SQLITE_DONE)
    {
        printf("Partido creado correctamente con ID %lld\n", id);
    }
    else
    {
        printf("Error al crear el partido: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Crea un nuevo partido en la base de datos
 *
 * Coordina la verificación de prerrequisitos, recopilación de datos y inserción
 * para asegurar un proceso robusto y modular de creación de partidos.
 */
void crear_partido()
{
    // Activar IA antes de crear partido
    activar_ia_antes_partido();

    if (!verificar_prerrequisitos_partido())
        return;

    DatosPartido datos;
    listar_canchas_disponibles();
    recopilar_datos_partido(&datos);

    char fecha[20];
    get_datetime(fecha, sizeof(fecha));
    long long id = obtener_siguiente_id("partido");
    insertar_partido(id, &datos, fecha);
}

/**
 * @brief Muestra un listado de todos los partidos registrados
 *
 * Consulta la base de datos y muestra en pantalla todos los partidos
 * con sus respectivos datos: ID, cancha, fecha/hora, goles, asistencias
 * y nombre de la camiseta utilizada. Realiza un JOIN con la tabla camiseta
 * para obtener el nombre de la camiseta.
 *
 * @note Si no hay partidos registrados, muestra un mensaje informativo
 */
void listar_partidos()
{
    clear_screen();
    print_header("LISTADO DE PARTIDOS");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "JOIN cancha can ON p.cancha_id = can.id ORDER BY p.id DESC",
                       -1, &stmt, NULL);

    int hay = 0;
    char fecha_formateada[20];

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // Formatear la fecha para visualización
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2), fecha_formateada, sizeof(fecha_formateada));

        printf("%d |Cancha:%s |Fecha:%s | G:%d A:%d |Camiseta:%s | %s |Clima:%s |Dia:%s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               fecha_formateada,
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_text(stmt, 5),
               resultado_to_text(sqlite3_column_int(stmt, 6)),
               clima_to_text(sqlite3_column_int(stmt, 7)),
               dia_to_text(sqlite3_column_int(stmt, 8)));
        hay = 1;
    }

    if (!hay)
        printf("No hay partidos cargados.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Elimina un partido de la base de datos.
 *
 * Esta función permite al usuario eliminar un partido existente. Primero muestra
 * la lista de partidos disponibles, solicita el ID del partido a eliminar,
 * verifica que el partido exista, solicita confirmación al usuario y finalmente
 * elimina el registro de la base de datos si se confirma.
 *
 * @note Si el partido no existe, muestra un mensaje de error y no realiza la eliminación.
 * @note Si el usuario no confirma la eliminación, la operación se cancela.
 */
void eliminar_partido()
{
    print_header("ELIMINAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Eliminar (0 para cancelar): ");

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        return;
    }

    if (!confirmar("Seguro que desea eliminar este partido?"))
        return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "DELETE FROM partido WHERE id = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Partido Eliminado Correctamente\n");
    pause_console();
}

/**
 * @brief Variable global para almacenar el ID del partido actualmente siendo modificado
 *
 * Esta variable se utiliza en las funciones de modificación para identificar
 * qué partido se está editando en el menú de modificación.
 */
static int current_partido_id;

/**
 * @brief Función genérica para modificar un campo entero de un partido
 *
 * @param campo Nombre del campo en la base de datos
 * @param prompt Texto para solicitar el nuevo valor
 * @param mensaje_exito Mensaje de éxito
 * @param min_val Valor mínimo válido (0 si no aplica)
 * @param max_val Valor máximo válido (0 si no aplica)
 * @param mostrar_lista Función para mostrar lista de opciones (NULL si no aplica)
 */
static void modificar_campo_partido(const char *campo, const char *prompt, const char *mensaje_exito,
                                    int min_val, int max_val, void (*mostrar_lista)(void))
{
    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (min_val != 0 || max_val != 0)
    {
        while (valor < min_val || valor > max_val)
        {
            char prompt_error[256];
            snprintf(prompt_error, sizeof(prompt_error), "Valor invalido. Ingrese entre %d y %d: ", min_val, max_val);
            valor = input_int(prompt_error);
        }
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    pause_console();
}

/**
 * @brief Función genérica para modificar un campo de texto de un partido
 *
 * @param campo Nombre del campo en la base de datos
 * @param prompt Texto para solicitar el nuevo valor
 * @param mensaje_exito Mensaje de éxito
 * @param buffer_size Tamaño del buffer para input
 */
static void modificar_campo_texto_partido(const char *campo, const char *prompt, const char *mensaje_exito, int buffer_size)
{
    char valor[buffer_size];
    printf("%s", prompt);
    fgets(valor, sizeof(valor), stdin);
    valor[strcspn(valor, "\n")] = 0;

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    pause_console();
}

/**
 * @brief Función genérica para buscar partidos por un criterio
 *
 * @param header Título del header
 * @param campo Campo por el que buscar
 * @param prompt Texto para solicitar el valor de búsqueda
 * @param mostrar_lista Función para mostrar lista de opciones (NULL si no aplica)
 * @param validar_id Si debe validar que el ID existe
 */
static void buscar_partidos_generico(const char *header, const char *campo, const char *prompt,
                                     void (*mostrar_lista)(void), int validar_id)
{
    print_header(header);

    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (validar_id && !existe_id(campo, valor))
    {
        printf("El %s no existe.\n", campo);
        pause_console();
        return;
    }

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
             "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
             "JOIN cancha can ON p.cancha_id = can.id "
             "WHERE p.%s = ?",
             campo);

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, valor);

    int hay = 0;
    char fecha_formateada[20];

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // Formatear la fecha para visualización
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2), fecha_formateada, sizeof(fecha_formateada));

        printf("%d | %s | %s | G:%d A:%d | %s | %s | %s | %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               fecha_formateada,
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_text(stmt, 5),
               resultado_to_text(sqlite3_column_int(stmt, 6)),
               clima_to_text(sqlite3_column_int(stmt, 7)),
               dia_to_text(sqlite3_column_int(stmt, 8)));
        hay = 1;
    }

    if (!hay)
        printf("No se encontraron partidos con ese criterio.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Modifica la cancha de un partido existente
 */
static void modificar_cancha_partido()
{
    modificar_campo_partido("cancha_id", "Nuevo ID Cancha: ", "Cancha modificada correctamente", 0, 0, &listar_canchas_disponibles);
}

/**
 * @brief Modifica la fecha y hora de un partido existente
 *
 * Solicita al usuario la nueva fecha en formato dd/mm/yyyy y la nueva hora en formato hh:mm,
 * combina ambos en una cadena y actualiza el campo fecha_hora en la base de datos.
 */
static void modificar_fecha_hora_partido()
{
    char fecha[20];
    char hora[10];
    char fecha_hora[30];
    printf("Nueva fecha (dd/mm/yyyy): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;
    printf("Nueva hora (hh:mm): ");
    fgets(hora, sizeof(hora), stdin);
    hora[strcspn(hora, "\n")] = 0;
    snprintf(fecha_hora, sizeof(fecha_hora), "%s %s", fecha, hora);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET fecha_hora=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, fecha_hora, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Fecha y hora modificadas correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el número de goles de un partido existente
 */
static void modificar_goles_partido()
{
    modificar_campo_partido("goles", "Nuevos goles: ", "Goles modificados correctamente", 0, 0, NULL);
}

/**
 * @brief Modifica el número de asistencias de un partido existente
 */
static void modificar_asistencias_partido()
{
    modificar_campo_partido("asistencias", "Nuevas asistencias: ", "Asistencias modificadas correctamente", 0, 0, NULL);
}

/**
 * @brief Modifica el resultado de un partido existente
 */
static void modificar_resultado_partido()
{
    modificar_campo_partido("resultado", "Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ", "Resultado modificado correctamente", 1, 3, NULL);
}

/**
 * @brief Modifica la camiseta utilizada en un partido existente
 *
 * Muestra la lista de camisetas disponibles, solicita el nuevo ID de camiseta,
 * verifica que exista y actualiza el campo camiseta_id en la base de datos.
 */
static void modificar_camiseta_partido()
{
    listar_camisetas();
    int camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", camiseta))
    {
        printf("La camiseta no existe\n");
        return;
    }
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET camiseta_id=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, camiseta);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Camiseta modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el clima de un partido existente
 */
static void modificar_clima_partido()
{
    modificar_campo_partido("clima", "Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ", "Clima modificado correctamente", 1, 6, NULL);
}

/**
 * @brief Modifica el día de un partido existente
 */
static void modificar_dia_partido()
{
    modificar_campo_partido("dia", "Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ", "Dia modificado correctamente", 1, 3, NULL);
}

/**
 * @brief Modifica el comentario personal de un partido existente
 */
static void modificar_comentario_partido()
{
    modificar_campo_texto_partido("comentario_personal", "Nuevo comentario personal: ", "Comentario modificado correctamente", 256);
}

/**
 * @brief Recopila datos completos para modificar un partido
 *
 * Solicita al usuario todos los campos necesarios para actualizar un partido,
 * validando cada entrada para asegurar consistencia de datos.
 *
 * @param datos Puntero a la estructura DatosPartido donde almacenar los datos
 */
static void recopilar_datos_completos_partido(DatosPartido *datos)
{
    listar_canchas_disponibles();
    datos->cancha_id = input_int("Nuevo ID Cancha: ");
    if (!existe_id("cancha", datos->cancha_id))
        return;
    char fecha[20];
    char hora[10];
    input_date("Nueva fecha (dd/mm/yyyy): ", fecha, 20);
    input_date("Nueva hora (hh:mm): ", hora, 10);
    snprintf(datos->comentario_personal, sizeof(datos->comentario_personal), "%s %s", fecha, hora);
    datos->goles = input_int("Nuevos goles: ");
    datos->asistencias = input_int("Nuevas asistencias: ");
    datos->resultado = input_int("Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (datos->resultado < 1 || datos->resultado > 3)
    {
        datos->resultado = input_int("Resultado invalido. Ingrese 1, 2 o 3: ");
    }
    listar_camisetas();
    datos->camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", datos->camiseta))
    {
        printf("La camiseta no existe\n");
        return;
    }
    datos->clima = input_int("Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    while (datos->clima < 1 || datos->clima > 6)
    {
        datos->clima = input_int("Clima invalido. Ingrese entre 1 y 6: ");
    }
    datos->dia = input_int("Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (datos->dia < 1 || datos->dia > 3)
    {
        datos->dia = input_int("Dia invalido. Ingrese 1, 2 o 3: ");
    }
}

/**
 * @brief Actualiza todos los campos de un partido en la base de datos
 *
 * Realiza una actualización completa de un partido utilizando prepared statements
 * para prevenir inyección SQL y asegurar atomicidad de la operación.
 *
 * @param datos Puntero a la estructura DatosPartido con los datos a actualizar
 * @param fecha_hora Fecha y hora combinadas
 */
static void actualizar_partido_completo(DatosPartido const *datos, char const *fecha_hora)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "UPDATE partido "
                       "SET cancha_id=?, fecha_hora=?, goles=?, asistencias=?, camiseta_id=?, resultado=?, clima=?, dia=? "
                       "WHERE id=?",

                       -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, datos->cancha_id);
    sqlite3_bind_text(stmt, 2, fecha_hora, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, datos->goles);
    sqlite3_bind_int(stmt, 4, datos->asistencias);
    sqlite3_bind_int(stmt, 5, datos->camiseta);
    sqlite3_bind_int(stmt, 6, datos->resultado);
    sqlite3_bind_int(stmt, 7, datos->clima);
    sqlite3_bind_int(stmt, 8, datos->dia);
    sqlite3_bind_int(stmt, 9, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Partido Modificado Correctamente\n");
    pause_console();
}

/**
 * @brief Modifica todos los campos de un partido existente
 *
 * Coordina la recopilación de datos y actualización para simplificar
 * la modificación completa de un partido en una sola operación.
 */
static void modificar_todo_partido()
{
    DatosPartido datos;
    recopilar_datos_completos_partido(&datos);
    actualizar_partido_completo(&datos, datos.comentario_personal);
}
/**
 * @brief Permite modificar los datos de un partido existente
 *
 * Muestra la lista de partidos disponibles, solicita el ID a modificar,
 * verifica que exista y muestra un menú con opciones para modificar campos individuales o todos.
 */
void modificar_partido()
{
    print_header("MODIFICAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Modificar (0 para cancelar): ");

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        return;
    }

    current_partido_id = id;

    MenuItem items[] =
    {
        {1, "Cancha", modificar_cancha_partido},
        {2, "Fecha y Hora", modificar_fecha_hora_partido},
        {3, "Goles", modificar_goles_partido},
        {4, "Asistencias", modificar_asistencias_partido},
        {5, "Resultado", modificar_resultado_partido},
        {6, "Camiseta", modificar_camiseta_partido},
        {7, "Clima", modificar_clima_partido},
        {8, "Dia", modificar_dia_partido},
        {9, "Comentario", modificar_comentario_partido},
        {10, "Modificar Todo", modificar_todo_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR PARTIDO", items, 11);
}
/** @brief Busca partidos por camiseta utilizada */
static void buscar_por_camiseta()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CAMISETA", "camiseta_id", "ID de la camiseta: ", &listar_camisetas, 1);
}

/** @brief Busca partidos por número de goles */
static void buscar_por_goles()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR GOLES", "goles", "Número de goles: ", NULL, 0);
}

/** @brief Busca partidos por número de asistencias */
static void buscar_por_asistencias()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR ASISTENCIAS", "asistencias", "Número de asistencias: ", NULL, 0);
}

/** @brief Busca partidos por cancha */
static void buscar_por_cancha()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CANCHA", "cancha_id", "ID de la cancha: ", &listar_canchas_disponibles, 1);
}

/**
 * @brief Permite buscar partidos según diferentes criterios
 *
 * Presenta un submenú con opciones para buscar partidos por:
 * - Camiseta utilizada
 * - Número de goles
 * - Número de asistencias
 * - Cancha donde se jugó
 */
void buscar_partidos()
{
    MenuItem items[] =
    {
        {1, "Por Camiseta", buscar_por_camiseta},
        {2, "Por Goles", buscar_por_goles},
        {3, "Por Asistencias", buscar_por_asistencias},
        {4, "Por Cancha", buscar_por_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu("BUSQUEDA DE PARTIDOS", items, 5);
}

/**
 * @brief Maneja un gol marcado por el equipo local durante la simulación
 *
 * @param equipo_local Puntero al equipo local
 * @param minuto Minuto del partido
 * @param jugador_idx Índice del jugador que marcó
 * @param asistente_idx Índice del asistente
 * @param estadisticas_local Array de estadísticas locales
 * @param asistencias_local Array de asistencias locales
 * @param goles_local Puntero al contador de goles locales
 */
static void manejar_gol_local(Equipo const *equipo_local, int minuto, int jugador_idx, int asistente_idx,
                              int *estadisticas_local, int *asistencias_local, int *goles_local)
{
    if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
    }

    (*goles_local)++;
    estadisticas_local[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_local[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_local->jugadores[jugador_idx].nombre,
           equipo_local->jugadores[jugador_idx].numero,
           equipo_local->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_local->jugadores[asistente_idx].nombre,
               equipo_local->jugadores[asistente_idx].numero);
    }
}

/**
 * @brief Maneja un gol marcado por el equipo visitante durante la simulación
 *
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto Minuto del partido
 * @param jugador_idx Índice del jugador que marcó
 * @param asistente_idx Índice del asistente
 * @param estadisticas_visitante Array de estadísticas visitantes
 * @param asistencias_visitante Array de asistencias visitantes
 * @param goles_visitante Puntero al contador de goles visitantes
 */
static void manejar_gol_visitante(Equipo const *equipo_visitante, int minuto, int jugador_idx, int asistente_idx,
                                  int *estadisticas_visitante, int *asistencias_visitante, int *goles_visitante)
{
    if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
    }

    (*goles_visitante)++;
    estadisticas_visitante[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_visitante[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_visitante->jugadores[jugador_idx].nombre,
           equipo_visitante->jugadores[jugador_idx].numero,
           equipo_visitante->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_visitante->jugadores[asistente_idx].nombre,
               equipo_visitante->jugadores[asistente_idx].numero);
    }
}

/**
 * @brief Verifica si hay suficientes equipos para simular un partido
 *
 * @return 1 si hay al menos 2 equipos, 0 si no
 */
static int verificar_equipos_disponibles()
{
    sqlite3_stmt *stmt_count;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM equipo", -1, &stmt_count, NULL);
    sqlite3_step(stmt_count);
    int total_equipos = sqlite3_column_int(stmt_count, 0);
    sqlite3_finalize(stmt_count);

    if (total_equipos < 2)
    {
        printf("Se necesitan al menos 2 equipos guardados para simular un partido.\n");
        printf("Por favor, cree equipos primero.\n");
        pause_console();
        return 0;
    }
    return 1;
}

/**
 * @brief Muestra la lista de equipos disponibles
 */
static void mostrar_equipos_disponibles()
{
    printf("=== EQUIPOS DISPONIBLES ===\n\n");
    sqlite3_stmt *stmt_equipos;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM equipo ORDER BY id", -1, &stmt_equipos, NULL);

    while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
    {
        printf("%d. %s\n", sqlite3_column_int(stmt_equipos, 0),
               sqlite3_column_text(stmt_equipos, 1));
    }
    sqlite3_finalize(stmt_equipos);
}

/**
 * @brief Selecciona los equipos local y visitante
 *
 * @param equipo_local_id Puntero al ID del equipo local
 * @param equipo_visitante_id Puntero al ID del equipo visitante
 */
static void seleccionar_equipos(int *equipo_local_id, int *equipo_visitante_id)
{
    // Seleccionar equipo local
    do
    {
        *equipo_local_id = input_int("\nSeleccione el equipo LOCAL (ID): ");
        if (!existe_id("equipo", *equipo_local_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (!existe_id("equipo", *equipo_local_id));

    // Seleccionar equipo visitante (diferente al local)
    do
    {
        *equipo_visitante_id = input_int("Seleccione el equipo VISITANTE (ID): ");
        if (*equipo_visitante_id == *equipo_local_id)
        {
            printf("El equipo visitante debe ser diferente al local.\n");
        }
        else if (!existe_id("equipo", *equipo_visitante_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (*equipo_visitante_id == *equipo_local_id || !existe_id("equipo", *equipo_visitante_id));
}

/**
 * @brief Carga los equipos desde la base de datos
 *
 * @param equipo_local_id ID del equipo local
 * @param equipo_visitante_id ID del equipo visitante
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @return 1 si se cargaron exitosamente, 0 si hubo error
 */
static int cargar_equipos(int equipo_local_id, int equipo_visitante_id, Equipo *equipo_local, Equipo *equipo_visitante)
{
    memset(equipo_local, 0, sizeof(Equipo));
    memset(equipo_visitante, 0, sizeof(Equipo));

    if (!cargar_equipo_desde_bd(equipo_local_id, equipo_local))
    {
        printf("Error al cargar el equipo local.\n");
        pause_console();
        return 0;
    }

    if (!cargar_equipo_desde_bd(equipo_visitante_id, equipo_visitante))
    {
        printf("Error al cargar el equipo visitante.\n");
        pause_console();
        return 0;
    }
    return 1;
}

/**
 * @brief Muestra la información inicial del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
static void mostrar_inicio_partido(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    printf("\n*** INICIANDO SIMULACION ***\n");
    printf("EQUIPO LOCAL: %s\n", equipo_local->nombre);
    printf("EQUIPO VISITANTE: %s\n\n", equipo_visitante->nombre);
}

/**
 * @brief Muestra la alineación de los equipos
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
static void mostrar_alineacion(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    clear_screen();
    printf("%s\n", ASCII_SIMULACION);
    printf("                    SIMULACION DE PARTIDO\n\n");

    printf("=== %s VS %s ===\n\n", equipo_local->nombre, equipo_visitante->nombre);

    // Mostrar cancha inicial
    mostrar_cancha_animada(0, 0);

    // Mostrar equipos alineados
    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_local->jugadores[i].numero, equipo_local->jugadores[i].nombre);
        if (equipo_local->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_visitante->jugadores[i].numero, equipo_visitante->jugadores[i].nombre);
        if (equipo_visitante->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\n*** INICIO DEL PARTIDO ***\n");
    printf("La simulacion comenzara automaticamente en 3 segundos...\n");
    Sleep(3000);
}

/**
 * @brief Ejecuta la lógica de simulación del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param estadisticas Puntero a la estructura con todas las estadísticas del partido
 */
static void simular_partido_logica(Equipo const *equipo_local, Equipo const *equipo_visitante,
                                   EstadisticasPartido *estadisticas)
{
    for (int minuto = 1; minuto <= 60; minuto++)
    {
        clear_screen();
        print_header("SIMULACION DE PARTIDO");

        printf("=== %s %d - %d %s ===\n\n",
               equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);
        printf("MINUTO: %d\n\n", minuto);

        // Generar eventos aleatorios
        int evento = secure_rand(100);

        if (evento < 2) // Gol local
        {
            int jugador_idx = secure_rand(equipo_local->num_jugadores);
            int asistente_idx = secure_rand(equipo_local->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
            }

            manejar_gol_local(equipo_local, minuto, jugador_idx, asistente_idx,
                              estadisticas->estadisticas_local, estadisticas->asistencias_local, &estadisticas->goles_local);
        }
        else if (evento < 4) // Gol visitante
        {
            int jugador_idx = secure_rand(equipo_visitante->num_jugadores);
            int asistente_idx = secure_rand(equipo_visitante->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
            }

            manejar_gol_visitante(equipo_visitante, minuto, jugador_idx, asistente_idx,
                                  estadisticas->estadisticas_visitante, estadisticas->asistencias_visitante, &estadisticas->goles_visitante);
        }
        else if (evento < 10)
        {
            printf("*** Oportunidad de gol ***\n");
        }
        else
        {
            printf("*** El partido continúa... ***\n");
        }

        mostrar_cancha_animada(minuto, (evento < 4) ? 1 : 0);
        Sleep(1000);
    }
}

/**
 * @brief Muestra los resultados finales del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param estadisticas Puntero a la estructura con todas las estadísticas del partido
 */
static void mostrar_resultados(Equipo const *equipo_local, Equipo const *equipo_visitante,
                               EstadisticasPartido const *estadisticas)
{
    // Resultados finales
    clear_screen();
    print_header("FIN DEL PARTIDO");

    printf("*** RESULTADO FINAL ***\n\n");
    printf("*** 60 MINUTOS COMPLETADOS ***\n\n");

    printf("*** %s %d - %d %s ***\n\n",
           equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);

    if (estadisticas->goles_local > estadisticas->goles_visitante)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_local->nombre);
    }
    else if (estadisticas->goles_visitante > estadisticas->goles_local)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_visitante->nombre);
    }
    else
    {
        printf("*** ¡EMPATE! ***\n\n");
    }

    // Mostrar estadísticas
    printf("*** ESTADISTICAS DEL PARTIDO ***\n\n");

    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_local[i] > 0 || estadisticas->asistencias_local[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_local->jugadores[i].nombre,
                   equipo_local->jugadores[i].numero,
                   estadisticas->estadisticas_local[i], estadisticas->asistencias_local[i]);
        }
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_visitante[i] > 0 || estadisticas->asistencias_visitante[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_visitante->jugadores[i].nombre,
                   equipo_visitante->jugadores[i].numero,
                   estadisticas->estadisticas_visitante[i], estadisticas->asistencias_visitante[i]);
        }
    }
}

/**
 * @brief Carga un equipo desde la base de datos por su ID
 *
 * @param equipo_id ID del equipo a cargar
 * @param equipo Puntero al equipo donde cargar los datos
 * @return 1 si se cargó exitosamente, 0 si hubo error
 */
static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_equipo;
    const char *sql_equipo = "SELECT nombre, tipo, tipo_futbol, num_jugadores FROM equipo WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql_equipo, -1, &stmt_equipo, 0) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_int(stmt_equipo, 1, equipo_id);

    if (sqlite3_step(stmt_equipo) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt_equipo);
        return 0;
    }

    equipo->id = equipo_id;
    strncpy_s(equipo->nombre, sizeof(equipo->nombre), (const char *)sqlite3_column_text(stmt_equipo, 0), _TRUNCATE);
    equipo->tipo = sqlite3_column_int(stmt_equipo, 1);
    equipo->tipo_futbol = sqlite3_column_int(stmt_equipo, 2);
    equipo->num_jugadores = sqlite3_column_int(stmt_equipo, 3);
    equipo->partido_id = -1;

    sqlite3_finalize(stmt_equipo);

    // Cargar jugadores
    return cargar_jugadores_equipo(equipo_id, equipo);
}

/**
 * @brief Carga los jugadores de un equipo desde la base de datos
 *
 * @param equipo_id ID del equipo cuyos jugadores se cargarán
 * @param equipo Puntero al equipo donde cargar los jugadores
 * @return 1 si se cargaron exitosamente, 0 si hubo error
 */
static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero";

    if (sqlite3_prepare_v2(db, sql_jugadores, -1, &stmt_jugadores, 0) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

    int jugador_idx = 0;
    while (sqlite3_step(stmt_jugadores) == SQLITE_ROW && jugador_idx < 11)
    {
        strncpy_s(equipo->jugadores[jugador_idx].nombre,
                  sizeof(equipo->jugadores[jugador_idx].nombre),
                  (const char *)sqlite3_column_text(stmt_jugadores, 0),
                  _TRUNCATE);
        equipo->jugadores[jugador_idx].numero = sqlite3_column_int(stmt_jugadores, 1);
        equipo->jugadores[jugador_idx].posicion = sqlite3_column_int(stmt_jugadores, 2);
        equipo->jugadores[jugador_idx].es_capitan = sqlite3_column_int(stmt_jugadores, 3);
        jugador_idx++;
    }

    sqlite3_finalize(stmt_jugadores);
    return 1;
}

/**
 * @brief Determina el resultado de un partido basado en los goles
 *
 * @param goles_local Goles del equipo local
 * @param goles_visitante Goles del equipo visitante
 * @param resultado_local Puntero para almacenar el resultado del equipo local
 * @param resultado_visitante Puntero para almacenar el resultado del equipo visitante
 */
static void determinar_resultado_partido(int goles_local, int goles_visitante,
        int *resultado_local, int *resultado_visitante)
{
    if (goles_local > goles_visitante)
    {
        *resultado_local = 1;     // VICTORIA
        *resultado_visitante = 3; // DERROTA
    }
    else if (goles_visitante > goles_local)
    {
        *resultado_local = 3;     // DERROTA
        *resultado_visitante = 1; // VICTORIA
    }
    else
    {
        *resultado_local = 2;     // EMPATE
        *resultado_visitante = 2; // EMPATE
    }
}

/**
 * @brief Obtiene el ID de una cancha por defecto
 *
 * @return ID de la cancha por defecto
 */
static int obtener_cancha_defecto()
{
    int cancha_id = 1;
    sqlite3_stmt *stmt_cancha;
    sqlite3_prepare_v2(db, "SELECT id FROM cancha ORDER BY id LIMIT 1", -1, &stmt_cancha, NULL);
    if (sqlite3_step(stmt_cancha) == SQLITE_ROW)
    {
        cancha_id = sqlite3_column_int(stmt_cancha, 0);
    }
    sqlite3_finalize(stmt_cancha);
    return cancha_id;
}

/**
 * @brief Guarda los resultados de una simulación de partido en la base de datos
 *
 * @param datos_simulacion Puntero a la estructura con todos los datos de la simulación
 */
static void guardar_resultados_simulacion(DatosSimulacion const *datos_simulacion)
{
    char fecha_simulacion[20] = "2023-01-01 00:00";
    get_datetime(fecha_simulacion, sizeof(fecha_simulacion));

    // Determinar resultados
    int resultado_local;
    int resultado_visitante;
    determinar_resultado_partido(datos_simulacion->goles_local, datos_simulacion->goles_visitante,
                                 &resultado_local, &resultado_visitante);

    // Obtener cancha por defecto
    int cancha_id = obtener_cancha_defecto();

    // Guardar estadísticas para cada jugador del equipo local
    guardar_estadisticas_equipo(&datos_simulacion->equipo_local, datos_simulacion->estadisticas_local,
                                datos_simulacion->asistencias_local, resultado_local, cancha_id, fecha_simulacion);

    // Guardar estadísticas para cada jugador del equipo visitante
    guardar_estadisticas_equipo(&datos_simulacion->equipo_visitante, datos_simulacion->estadisticas_visitante,
                                datos_simulacion->asistencias_visitante, resultado_visitante, cancha_id, fecha_simulacion);

    printf("*** RESULTADOS GUARDADOS EN LA BASE DE DATOS ***\n");
}

/**
 * @brief Guarda las estadísticas de un equipo en la base de datos
 *
 * @param equipo Puntero al equipo
 * @param estadisticas Array con estadísticas de goles por jugador
 * @param asistencias Array con estadísticas de asistencias por jugador
 * @param resultado Resultado del equipo
 * @param cancha_id ID de la cancha
 * @param fecha_simulacion Fecha de la simulación
 */
static void guardar_estadisticas_equipo(Equipo const *equipo, int const *estadisticas, int const *asistencias,
                                        int resultado, int cancha_id, char const *fecha_simulacion)
{
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        if (estadisticas[i] > 0 || asistencias[i] > 0)
        {
            // Buscar o crear camiseta para este jugador
            int camiseta_id = 1; // Usar camiseta por defecto

            DatosPartido datos = {0};
            datos.cancha_id = cancha_id;
            datos.goles = estadisticas[i];
            datos.asistencias = asistencias[i];
            datos.camiseta = camiseta_id;
            datos.resultado = resultado;
            datos.rendimiento_general = 8;
            datos.cansancio = 5;
            datos.estado_animo = 7;
            strncpy_s(datos.comentario_personal, sizeof(datos.comentario_personal), "Partido simulado", _TRUNCATE);
            datos.clima = 1;
            datos.dia = 1;

            long long partido_id = obtener_siguiente_id("partido");
            insertar_partido(partido_id, &datos, fecha_simulacion);
        }
    }
}

/**
 * @brief Crea la estructura de datos de simulación a partir de las estadísticas
 *
 * @param equipo_local Equipo local
 * @param equipo_visitante Equipo visitante
 * @param estadisticas Estadísticas del partido
 * @return Estructura de datos de simulación completa
 */
static DatosSimulacion crear_datos_simulacion(Equipo equipo_local, Equipo equipo_visitante,
        EstadisticasPartido const *estadisticas)
{
    DatosSimulacion datos_simulacion = {0};
    datos_simulacion.equipo_local = equipo_local;
    datos_simulacion.equipo_visitante = equipo_visitante;
    memcpy(datos_simulacion.estadisticas_local, estadisticas->estadisticas_local, sizeof(estadisticas->estadisticas_local));
    memcpy(datos_simulacion.estadisticas_visitante, estadisticas->estadisticas_visitante, sizeof(estadisticas->estadisticas_visitante));
    memcpy(datos_simulacion.asistencias_local, estadisticas->asistencias_local, sizeof(estadisticas->asistencias_local));
    memcpy(datos_simulacion.asistencias_visitante, estadisticas->asistencias_visitante, sizeof(estadisticas->asistencias_visitante));
    datos_simulacion.goles_local = estadisticas->goles_local;
    datos_simulacion.goles_visitante = estadisticas->goles_visitante;
    return datos_simulacion;
}

/**
 * @brief Simula un partido entre dos equipos guardados en la base de datos
 *
 * Permite al usuario seleccionar dos equipos existentes de la base de datos
 * y simular un partido entre ellos. Los resultados se guardan automáticamente
 * en la base de datos incluyendo estadísticas de goles y asistencias.
 */
void simular_partido_guardados()
{
    clear_screen();
    print_header("SIMULAR PARTIDO CON EQUIPOS GUARDADOS");

    // Verificar que hay al menos 2 equipos disponibles
    if (!verificar_equipos_disponibles())
    {
        return;
    }

    // Mostrar equipos disponibles
    mostrar_equipos_disponibles();

    // Seleccionar equipos
    int equipo_local_id;
    int equipo_visitante_id;
    seleccionar_equipos(&equipo_local_id, &equipo_visitante_id);

    // Cargar equipos desde la base de datos
    Equipo equipo_local;
    Equipo equipo_visitante;
    if (!cargar_equipos(equipo_local_id, equipo_visitante_id, &equipo_local, &equipo_visitante))
    {
        return;
    }

    // Mostrar información inicial
    mostrar_inicio_partido(&equipo_local, &equipo_visitante);

    // Mostrar alineación y comenzar partido
    mostrar_alineacion(&equipo_local, &equipo_visitante);

    // Preparar arrays para estadísticas
    EstadisticasPartido estadisticas = {0};

    // Ejecutar simulación del partido
    simular_partido_logica(&equipo_local, &equipo_visitante, &estadisticas);

    // Mostrar resultados finales
    mostrar_resultados(&equipo_local, &equipo_visitante, &estadisticas);

    // Crear estructura de datos de simulación
    DatosSimulacion datos_simulacion = crear_datos_simulacion(equipo_local, equipo_visitante, &estadisticas);

    // Guardar resultados en la base de datos
    guardar_resultados_simulacion(&datos_simulacion);

    printf("\nPresione Enter para volver al menu...");
    getchar();
}

/**
 * @brief Muestra el menú principal de gestión de partidos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar,
 * eliminar partidos y simular partidos con equipos guardados.
 * Utiliza la función ejecutar_menu para manejar la navegación del menú
 * y delega las operaciones a las funciones correspondientes.
 */
void menu_partidos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_partido},
        {2, "Listar", listar_partidos},
        {3, "Modificar", modificar_partido},
        {4, "Eliminar", eliminar_partido},
        {5, "Simular con Equipos Guardados", simular_partido_guardados},
        {0, "Volver", NULL}
    };

    ejecutar_menu("PARTIDOS", items, 6);
}
