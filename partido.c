#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include "equipo.h"
#include "ascii_art.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>


/**
 * @brief Obtiene el siguiente ID disponible para un nuevo partido
 *
 * Busca el ID más pequeño disponible reutilizando espacios de IDs eliminados.
 * Utiliza una consulta SQL que encuentra el primer hueco en la secuencia de IDs.
 *
 * @return El ID disponible más pequeño (comenzando desde 1 si la tabla está vacía)
 */
static int obtener_siguiente_id_partido()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM partido WHERE id = 1) THEN 1 ELSE (SELECT MIN(t1.id + 1) FROM partido t1 WHERE NOT EXISTS (SELECT 1 FROM partido t2 WHERE t2.id = t1.id + 1)) END",
                       -1, &stmt, NULL);

    int id = 1; // Default si la tabla está vacía
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief Verifica si hay partidos registrados en la base de datos
 *
 * @return 1 si hay al menos un partido, 0 si no hay ninguno
 */
static int hay_partidos()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &stmt, NULL);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
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
 * @param cancha_id Puntero al ID de la cancha
 * @param goles Puntero a los goles
 * @param asistencias Puntero a las asistencias
 * @param camiseta Puntero al ID de la camiseta
 * @param resultado Puntero al resultado
 * @param rendimiento_general Puntero al rendimiento general
 * @param cansancio Puntero al cansancio
 * @param estado_animo Puntero al estado de ánimo
 * @param comentario_personal Cadena para el comentario personal
 * @param clima Puntero al clima
 * @param dia Puntero al día
 */
static void recopilar_datos_partido(int *cancha_id, int *goles, int *asistencias, int *camiseta, int *resultado, int *rendimiento_general, int *cansancio, int *estado_animo, char *comentario_personal, int *clima, int *dia)
{
    *cancha_id = input_int("ID Cancha, (0 para Cancelar): ");
    if (!existe_id("cancha", *cancha_id))
        return;
    *goles = input_int("Goles: ");
    *asistencias = input_int("Asistencias: ");
    *resultado = input_int("Resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (*resultado < 1 || *resultado > 3)
    {
        *resultado = input_int("Resultado invalido. (1=VICTORIA, 2=EMPATE, 3=DERROTA):");
    }
    listar_camisetas();
    *camiseta = input_int("ID Camiseta: ");
    if (!existe_id("camiseta", *camiseta))
        return;
    *rendimiento_general = input_int("Rendimiento general (1-10): ");
    while (*rendimiento_general < 1 || *rendimiento_general > 10)
    {
        *rendimiento_general = input_int("Rendimiento invalido. Ingrese entre 1 y 10: ");
    }
    *cansancio = input_int("Cansancio (1-10): ");
    while (*cansancio < 1 || *cansancio > 10)
    {
        *cansancio = input_int("Cansancio invalido. Ingrese entre 1 y 10:  ");
    }
    *estado_animo = input_int("Estado de Animo (1-10): ");
    while (*estado_animo < 1 || *estado_animo > 10)
    {
        *estado_animo = input_int("Estado de Animo invalido. Ingrese entre 1 y 10: ");
    }
    input_string("Comentario personal: ", comentario_personal, 256);
    *clima = input_int("Clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio):");
    while (*clima < 1 || *clima > 6)
    {
        *clima = input_int("Clima invalido (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    }
    *dia = input_int("Dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (*dia < 1 || *dia > 3)
    {
        *dia = input_int("Dia invalido (1=Dia, 2=Tarde, 3=Noche): ");
    }
}

/**
 * @brief Inserta un nuevo partido en la base de datos
 *
 * Utiliza prepared statements para evitar inyección SQL y asegurar integridad de datos.
 * Maneja errores de SQLite para informar al usuario si la inserción falla.
 *
 * @param id ID del partido
 * @param cancha_id ID de la cancha
 * @param fecha Fecha y hora
 * @param goles Número de goles
 * @param asistencias Número de asistencias
 * @param camiseta ID de la camiseta
 * @param resultado Resultado del partido
 * @param rendimiento_general Rendimiento general
 * @param cansancio Nivel de cansancio
 * @param estado_animo Estado de ánimo
 * @param comentario_personal Comentario personal
 * @param clima Condición climática
 * @param dia Momento del día
 */
static void insertar_partido(int id, int cancha_id, char *fecha, int goles, int asistencias, int camiseta, int resultado, int rendimiento_general, int cansancio, int estado_animo, char *comentario_personal, int clima, int dia)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "INSERT INTO partido(id, cancha_id,fecha_hora,goles,asistencias,camiseta_id,resultado,rendimiento_general,cansancio,estado_animo,comentario_personal,clima,dia)"
                       "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
                       -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, cancha_id);
    sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, goles);
    sqlite3_bind_int(stmt, 5, asistencias);
    sqlite3_bind_int(stmt, 6, camiseta);
    sqlite3_bind_int(stmt, 7, resultado);
    sqlite3_bind_int(stmt, 8, rendimiento_general);
    sqlite3_bind_int(stmt, 9, cansancio);
    sqlite3_bind_int(stmt, 10, estado_animo);
    sqlite3_bind_text(stmt, 11, comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, clima);
    sqlite3_bind_int(stmt, 13, dia);
    int result = sqlite3_step(stmt);
    if (result == SQLITE_DONE)
    {
        printf("Partido creado correctamente con ID %d\n", id);
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
    if (!verificar_prerrequisitos_partido()) return;
    int cancha_id, goles, asistencias, camiseta, resultado, rendimiento_general, cansancio, estado_animo, clima, dia;
    char comentario_personal[256];
    listar_canchas_disponibles();
    recopilar_datos_partido(&cancha_id, &goles, &asistencias, &camiseta, &resultado, &rendimiento_general, &cansancio, &estado_animo, comentario_personal, &clima, &dia);
    char fecha[20];
    get_datetime(fecha, sizeof(fecha));
    int id = obtener_siguiente_id_partido();
    insertar_partido(id, cancha_id, fecha, goles, asistencias, camiseta, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia);
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

    if (!hay_partidos())
    {
        printf("No hay partidos para eliminar.\n");
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
 * @brief Modifica la cancha de un partido existente
 *
 * Muestra la lista de canchas disponibles, solicita el nuevo ID de cancha,
 * verifica que exista y actualiza el campo cancha_id en la base de datos.
 */
static void modificar_cancha_partido()
{
    printf("Canchas disponibles:\n");
    sqlite3_stmt *stmt_canchas;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM cancha ORDER BY id", -1, &stmt_canchas, NULL);
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        printf("%d | %s\n", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);
    int cancha_id = input_int("Nuevo ID Cancha: ");
    if (!existe_id("cancha", cancha_id))
        return;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET cancha_id=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, cancha_id);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Cancha modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la fecha y hora de un partido existente
 *
 * Solicita al usuario la nueva fecha en formato dd/mm/yyyy y la nueva hora en formato hh:mm,
 * combina ambos en una cadena y actualiza el campo fecha_hora en la base de datos.
 */
static void modificar_fecha_hora_partido()
{
    char fecha[20], hora[10], fecha_hora[30];
    printf("Nueva fecha (dd/mm/yyyy): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;
    printf("Nueva hora (hh:mm): ");
    fgets(hora, sizeof(hora), stdin);
    hora[strcspn(hora, "\n")] = 0;
    sprintf(fecha_hora, "%s %s", fecha, hora);
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
 *
 * Solicita al usuario el nuevo número de goles y actualiza el campo goles en la base de datos.
 */
static void modificar_goles_partido()
{
    int goles = input_int("Nuevos goles: ");
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET goles=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, goles);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Goles modificados correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el número de asistencias de un partido existente
 *
 * Solicita al usuario el nuevo número de asistencias y actualiza el campo asistencias en la base de datos.
 */
static void modificar_asistencias_partido()
{
    int asistencias = input_int("Nuevas asistencias: ");
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET asistencias=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, asistencias);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Asistencias modificadas correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el resultado de un partido existente
 *
 * Solicita al usuario el nuevo resultado (VICTORIA, EMPATE, DERROTA) y actualiza el campo resultado en la base de datos.
 */
static void modificar_resultado_partido()
{
    int resultado = input_int("Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (resultado < 1 || resultado > 3)
    {
        resultado = input_int("Resultado invalido. Ingrese 1, 2 o 3: ");
    }
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET resultado=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, resultado);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Resultado modificado correctamente\n");
    pause_console();
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
 *
 * Solicita al usuario el nuevo clima (Despejado, Nublado, Lluvia, Ventoso, Mucho Calor, Mucho Frio)
 * y actualiza el campo clima en la base de datos.
 */
static void modificar_clima_partido()
{
    int clima = input_int("Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    while (clima < 1 || clima > 6)
    {
        clima = input_int("Clima invalido. Ingrese entre 1 y 6: ");
    }
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET clima=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, clima);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Clima modificado correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el día de un partido existente
 *
 * Solicita al usuario el nuevo día (Dia, Tarde, Noche) y actualiza el campo dia en la base de datos.
 */
static void modificar_dia_partido()
{
    int dia = input_int("Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (dia < 1 || dia > 3)
    {
        dia = input_int("Dia invalido. Ingrese 1, 2 o 3: ");
    }
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET dia=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, dia);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Dia modificado correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el comentario personal de un partido existente
 *
 * Solicita al usuario el nuevo comentario personal y actualiza el campo comentario_personal en la base de datos.
 */
static void modificar_comentario_partido()
{
    char comentario[256];
    printf("Nuevo comentario personal: ");
    fgets(comentario, sizeof(comentario), stdin);
    comentario[strcspn(comentario, "\n")] = 0;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE partido SET comentario_personal=? WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, comentario, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Comentario modificado correctamente\n");
    pause_console();
}

/**
 * @brief Recopila datos completos para modificar un partido
 *
 * Solicita al usuario todos los campos necesarios para actualizar un partido,
 * validando cada entrada para asegurar consistencia de datos.
 *
 * @param cancha_id Puntero al ID de cancha
 * @param fecha Cadena para fecha
 * @param hora Cadena para hora
 * @param goles Puntero a goles
 * @param asistencias Puntero a asistencias
 * @param camiseta Puntero a ID camiseta
 * @param resultado Puntero a resultado
 * @param clima Puntero a clima
 * @param dia Puntero a día
 */
static void recopilar_datos_completos_partido(int *cancha_id, char *fecha, char *hora, int *goles, int *asistencias, int *camiseta, int *resultado, int *clima, int *dia)
{
    listar_canchas_disponibles();
    *cancha_id = input_int("Nuevo ID Cancha: ");
    if (!existe_id("cancha", *cancha_id))
        return;
    input_date("Nueva fecha (dd/mm/yyyy): ", fecha, 20);
    input_date("Nueva hora (hh:mm): ", hora, 10);
    *goles = input_int("Nuevos goles: ");
    *asistencias = input_int("Nuevas asistencias: ");
    *resultado = input_int("Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (*resultado < 1 || *resultado > 3)
    {
        *resultado = input_int("Resultado invalido. Ingrese 1, 2 o 3: ");
    }
    listar_camisetas();
    *camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", *camiseta))
    {
        printf("La camiseta no existe\n");
        return;
    }
    *clima = input_int("Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    while (*clima < 1 || *clima > 6)
    {
        *clima = input_int("Clima invalido. Ingrese entre 1 y 6: ");
    }
    *dia = input_int("Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (*dia < 1 || *dia > 3)
    {
        *dia = input_int("Dia invalido. Ingrese 1, 2 o 3: ");
    }
}

/**
 * @brief Actualiza todos los campos de un partido en la base de datos
 *
 * Realiza una actualización completa de un partido utilizando prepared statements
 * para prevenir inyección SQL y asegurar atomicidad de la operación.
 *
 * @param cancha_id ID de la cancha
 * @param fecha_hora Fecha y hora combinadas
 * @param goles Número de goles
 * @param asistencias Número de asistencias
 * @param camiseta ID de la camiseta
 * @param resultado Resultado del partido
 * @param clima Condición climática
 * @param dia Momento del día
 */
static void actualizar_partido_completo(int cancha_id, char *fecha_hora, int goles, int asistencias, int camiseta, int resultado, int clima, int dia)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "UPDATE partido "
                       "SET cancha_id=?, fecha_hora=?, goles=?, asistencias=?, camiseta_id=?, resultado=?, clima=?, dia=? "
                       "WHERE id=?",

                       -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, cancha_id);
    sqlite3_bind_text(stmt, 2, fecha_hora, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, goles);
    sqlite3_bind_int(stmt, 4, asistencias);
    sqlite3_bind_int(stmt, 5, camiseta);
    sqlite3_bind_int(stmt, 6, resultado);
    sqlite3_bind_int(stmt, 7, clima);
    sqlite3_bind_int(stmt, 8, dia);
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
    int cancha_id, goles, asistencias, camiseta, resultado, clima, dia;
    char fecha[20], hora[10];
    recopilar_datos_completos_partido(&cancha_id, fecha, hora, &goles, &asistencias, &camiseta, &resultado, &clima, &dia);
    char fecha_hora[30];
    sprintf(fecha_hora, "%s %s", fecha, hora);
    actualizar_partido_completo(cancha_id, fecha_hora, goles, asistencias, camiseta, resultado, clima, dia);
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

    if (!hay_partidos())
    {
        printf("No hay partidos para modificar.\n");
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
/**
 * @brief Busca partidos por camiseta utilizada
 *
 * Solicita el ID de la camiseta y muestra todos los partidos donde se utilizó esa camiseta.
 */
static void buscar_por_camiseta()
{
    print_header("BUSCAR PARTIDOS POR CAMISETA");

    listar_camisetas();
    int camiseta_id = input_int("ID de la camiseta: ");

    if (!existe_id("camiseta", camiseta_id))
    {
        printf("La camiseta no existe.\n");
        return;
    }

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "WHERE p.camiseta_id = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, camiseta_id);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d | %s | %s | G:%d A:%d | %s | %s | %s | %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_text(stmt, 5),
               resultado_to_text(sqlite3_column_int(stmt, 6)),
               clima_to_text(sqlite3_column_int(stmt, 7)),
               dia_to_text(sqlite3_column_int(stmt, 8)));
        hay = 1;
    }

    if (!hay)
        printf("No se encontraron partidos con esa camiseta.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Busca partidos por número de goles
 *
 * Solicita el número de goles y muestra todos los partidos con ese número de goles.
 */
static void buscar_por_goles()
{
    print_header("BUSCAR PARTIDOS POR GOLES");

    int goles = input_int("Número de goles: ");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "WHERE p.goles = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, goles);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d | %s | %s | G:%d A:%d | %s | %s | %s | %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_text(stmt, 5),
               resultado_to_text(sqlite3_column_int(stmt, 6)),
               clima_to_text(sqlite3_column_int(stmt, 7)),
               dia_to_text(sqlite3_column_int(stmt, 8)));
        hay = 1;
    }

    if (!hay)
        printf("No se encontraron partidos con %d goles.\n", goles);

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Busca partidos por número de asistencias
 *
 * Solicita el número de asistencias y muestra todos los partidos con ese número de asistencias.
 */
static void buscar_por_asistencias()
{
    print_header("BUSCAR PARTIDOS POR ASISTENCIAS");

    int asistencias = input_int("Número de asistencias: ");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "WHERE p.asistencias = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, asistencias);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d | %s | %s | G:%d A:%d | %s | %s | %s | %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_text(stmt, 5),
               resultado_to_text(sqlite3_column_int(stmt, 6)),
               clima_to_text(sqlite3_column_int(stmt, 7)),
               dia_to_text(sqlite3_column_int(stmt, 8)));
        hay = 1;
    }

    if (!hay)
        printf("No se encontraron partidos con %d asistencias.\n", asistencias);

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Busca partidos por cancha
 *
 * Solicita el ID de la cancha y muestra todos los partidos jugados en esa cancha.
 */
static void buscar_por_cancha()
{
    print_header("BUSCAR PARTIDOS POR CANCHA");

    // Listar canchas disponibles
    printf("Canchas disponibles:\n");
    sqlite3_stmt *stmt_canchas;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM cancha ORDER BY id", -1, &stmt_canchas, NULL);
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        printf("%d | %s\n", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);

    int cancha_id = input_int("ID de la cancha: ");

    if (!existe_id("cancha", cancha_id))
    {
        printf("La cancha no existe.\n");
        return;
    }

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                       "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                       "JOIN cancha can ON p.cancha_id = can.id "
                       "WHERE p.cancha_id = ?",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, cancha_id);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d | %s | %s | G:%d A:%d | %s | %s | %s | %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_text(stmt, 5),
               resultado_to_text(sqlite3_column_int(stmt, 6)),
               clima_to_text(sqlite3_column_int(stmt, 7)),
               dia_to_text(sqlite3_column_int(stmt, 8)));
        hay = 1;
    }

    if (!hay)
        printf("No se encontraron partidos en esa cancha.\n");

    sqlite3_finalize(stmt);
    pause_console();
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
 * @brief Simula un partido entre dos equipos guardados en la base de datos
 *
 * Permite al usuario seleccionar dos equipos existentes de la base de datos
 * y simular un partido entre ellos. Los resultados se guardan automáticamente
 * en la base de datos incluyendo estadísticas de goles y asistencias.
 */
void simular_partido_guardados();

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

    if (sqlite3_prepare_v2(db, sql_equipo, -1, &stmt_equipo, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt_equipo, 1, equipo_id);

        if (sqlite3_step(stmt_equipo) == SQLITE_ROW)
        {
            equipo->id = equipo_id;
            strncpy(equipo->nombre, (const char*)sqlite3_column_text(stmt_equipo, 0), sizeof(equipo->nombre));
            equipo->tipo = sqlite3_column_int(stmt_equipo, 1);
            equipo->tipo_futbol = sqlite3_column_int(stmt_equipo, 2);
            equipo->num_jugadores = sqlite3_column_int(stmt_equipo, 3);
            equipo->partido_id = -1;

            sqlite3_finalize(stmt_equipo);

            // Cargar jugadores
            sqlite3_stmt *stmt_jugadores;
            const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero";

            if (sqlite3_prepare_v2(db, sql_jugadores, -1, &stmt_jugadores, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

                int jugador_idx = 0;
                while (sqlite3_step(stmt_jugadores) == SQLITE_ROW && jugador_idx < 11)
                {
                    strncpy(equipo->jugadores[jugador_idx].nombre,
                            (const char*)sqlite3_column_text(stmt_jugadores, 0),
                            sizeof(equipo->jugadores[jugador_idx].nombre));
                    equipo->jugadores[jugador_idx].numero = sqlite3_column_int(stmt_jugadores, 1);
                    equipo->jugadores[jugador_idx].posicion = sqlite3_column_int(stmt_jugadores, 2);
                    equipo->jugadores[jugador_idx].es_capitan = sqlite3_column_int(stmt_jugadores, 3);
                    jugador_idx++;
                }

                sqlite3_finalize(stmt_jugadores);
                return 1;
            }
        }
        sqlite3_finalize(stmt_equipo);
    }
    return 0;
}

/**
 * @brief Guarda los resultados de una simulación de partido en la base de datos
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param goles_local Goles marcados por el equipo local
 * @param goles_visitante Goles marcados por el equipo visitante
 * @param estadisticas_local Array con estadísticas de goles por jugador local
 * @param estadisticas_visitante Array con estadísticas de goles por jugador visitante
 * @param asistencias_local Array con estadísticas de asistencias por jugador local
 * @param asistencias_visitante Array con estadísticas de asistencias por jugador visitante
 */
static void guardar_resultados_simulacion(Equipo *equipo_local, Equipo *equipo_visitante,
        int goles_local, int goles_visitante,
        int *estadisticas_local, int *estadisticas_visitante,
        int *asistencias_local, int *asistencias_visitante)
{
    char fecha_simulacion[20];
    get_datetime(fecha_simulacion, sizeof(fecha_simulacion));

    // Determinar resultado
    int resultado_local, resultado_visitante;
    if (goles_local > goles_visitante)
    {
        resultado_local = 1; // VICTORIA
        resultado_visitante = 3; // DERROTA
    }
    else if (goles_visitante > goles_local)
    {
        resultado_local = 3; // DERROTA
        resultado_visitante = 1; // VICTORIA
    }
    else
    {
        resultado_local = 2; // EMPATE
        resultado_visitante = 2; // EMPATE
    }

    // Obtener una cancha por defecto (la primera disponible)
    int cancha_id = 1;
    sqlite3_stmt *stmt_cancha;
    sqlite3_prepare_v2(db, "SELECT id FROM cancha ORDER BY id LIMIT 1", -1, &stmt_cancha, NULL);
    if (sqlite3_step(stmt_cancha) == SQLITE_ROW)
    {
        cancha_id = sqlite3_column_int(stmt_cancha, 0);
    }
    sqlite3_finalize(stmt_cancha);

    // Guardar estadísticas para cada jugador del equipo local
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        if (estadisticas_local[i] > 0 || asistencias_local[i] > 0)
        {
            // Buscar o crear camiseta para este jugador
            int camiseta_id = 1; // Usar camiseta por defecto

            int partido_id = obtener_siguiente_id_partido();
            insertar_partido(partido_id, cancha_id, fecha_simulacion,
                             estadisticas_local[i], asistencias_local[i],
                             camiseta_id, resultado_local, 8, 5, 7,
                             "Partido simulado", 1, 1);
        }
    }

    // Guardar estadísticas para cada jugador del equipo visitante
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        if (estadisticas_visitante[i] > 0 || asistencias_visitante[i] > 0)
        {
            // Buscar o crear camiseta para este jugador
            int camiseta_id = 1; // Usar camiseta por defecto

            int partido_id = obtener_siguiente_id_partido();
            insertar_partido(partido_id, cancha_id, fecha_simulacion,
                             estadisticas_visitante[i], asistencias_visitante[i],
                             camiseta_id, resultado_visitante, 8, 5, 7,
                             "Partido simulado", 1, 1);
        }
    }

    printf("*** RESULTADOS GUARDADOS EN LA BASE DE DATOS ***\n");
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
        return;
    }

    // Mostrar equipos disponibles
    printf("=== EQUIPOS DISPONIBLES ===\n\n");
    sqlite3_stmt *stmt_equipos;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM equipo ORDER BY id", -1, &stmt_equipos, NULL);

    while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
    {
        printf("%d. %s\n", sqlite3_column_int(stmt_equipos, 0),
               sqlite3_column_text(stmt_equipos, 1));
    }
    sqlite3_finalize(stmt_equipos);

    // Seleccionar equipo local
    int equipo_local_id;
    do
    {
        equipo_local_id = input_int("\nSeleccione el equipo LOCAL (ID): ");
        if (!existe_id("equipo", equipo_local_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (!existe_id("equipo", equipo_local_id));

    // Seleccionar equipo visitante (diferente al local)
    int equipo_visitante_id;
    do
    {
        equipo_visitante_id = input_int("Seleccione el equipo VISITANTE (ID): ");
        if (equipo_visitante_id == equipo_local_id)
        {
            printf("El equipo visitante debe ser diferente al local.\n");
        }
        else if (!existe_id("equipo", equipo_visitante_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (equipo_visitante_id == equipo_local_id || !existe_id("equipo", equipo_visitante_id));

    // Cargar equipos desde la base de datos
    Equipo equipo_local, equipo_visitante;

    if (!cargar_equipo_desde_bd(equipo_local_id, &equipo_local))
    {
        printf("Error al cargar el equipo local.\n");
        pause_console();
        return;
    }

    if (!cargar_equipo_desde_bd(equipo_visitante_id, &equipo_visitante))
    {
        printf("Error al cargar el equipo visitante.\n");
        pause_console();
        return;
    }

    // Simular el partido
    printf("\n*** INICIANDO SIMULACION ***\n");
    printf("EQUIPO LOCAL: %s\n", equipo_local.nombre);
    printf("EQUIPO VISITANTE: %s\n\n", equipo_visitante.nombre);

    // Preparar arrays para estadísticas
    int estadisticas_local[11] = {0};
    int estadisticas_visitante[11] = {0};
    int asistencias_local[11] = {0};
    int asistencias_visitante[11] = {0};

    // Simular partido (código simplificado basado en simular_partido existente)
    clear_screen();
    printf("%s\n", ASCII_SIMULACION);
    printf("                    SIMULACION DE PARTIDO\n\n");

    printf("=== %s VS %s ===\n\n", equipo_local.nombre, equipo_visitante.nombre);

    // Mostrar cancha inicial
    mostrar_cancha_animada(0, 0);

    // Mostrar equipos alineados
    printf("EQUIPO LOCAL (%s):\n", equipo_local.nombre);
    for (int i = 0; i < equipo_local.num_jugadores; i++)
    {
        printf("  %d. %s", equipo_local.jugadores[i].numero, equipo_local.jugadores[i].nombre);
        if (equipo_local.jugadores[i].es_capitan) printf(" (C)");
        printf("\n");
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante.nombre);
    for (int i = 0; i < equipo_visitante.num_jugadores; i++)
    {
        printf("  %d. %s", equipo_visitante.jugadores[i].numero, equipo_visitante.jugadores[i].nombre);
        if (equipo_visitante.jugadores[i].es_capitan) printf(" (C)");
        printf("\n");
    }

    printf("\n*** INICIO DEL PARTIDO ***\n");
    printf("La simulacion comenzara automaticamente en 3 segundos...\n");
    Sleep(3000);

    // Simulación simplificada de 60 minutos
    int goles_local = 0, goles_visitante = 0;
    for (int minuto = 1; minuto <= 60; minuto++)
    {
        clear_screen();
        print_header("SIMULACION DE PARTIDO");

        printf("=== %s %d - %d %s ===\n\n",
               equipo_local.nombre, goles_local, goles_visitante, equipo_visitante.nombre);
        printf("MINUTO: %d\n\n", minuto);

        // Generar eventos aleatorios
        int evento = rand() % 100;

        if (evento < 2)   // Gol local
        {
            int jugador_idx = rand() % equipo_local.num_jugadores;
            int asistente_idx = rand() % equipo_local.num_jugadores;
            if (asistente_idx == jugador_idx && equipo_local.num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_local.num_jugadores;
            }

            goles_local++;
            estadisticas_local[jugador_idx]++;
            if (asistente_idx != jugador_idx)
            {
                asistencias_local[asistente_idx]++;
            }

            printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
            printf("   Gol de %s (%d) para %s\n",
                   equipo_local.jugadores[jugador_idx].nombre,
                   equipo_local.jugadores[jugador_idx].numero,
                   equipo_local.nombre);
            if (asistente_idx != jugador_idx)
            {
                printf("   Asistencia de %s (%d)\n",
                       equipo_local.jugadores[asistente_idx].nombre,
                       equipo_local.jugadores[asistente_idx].numero);
            }
        }
        else if (evento < 4)   // Gol visitante
        {
            int jugador_idx = rand() % equipo_visitante.num_jugadores;
            int asistente_idx = rand() % equipo_visitante.num_jugadores;
            if (asistente_idx == jugador_idx && equipo_visitante.num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_visitante.num_jugadores;
            }

            goles_visitante++;
            estadisticas_visitante[jugador_idx]++;
            if (asistente_idx != jugador_idx)
            {
                asistencias_visitante[asistente_idx]++;
            }

            printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
            printf("   Gol de %s (%d) para %s\n",
                   equipo_visitante.jugadores[jugador_idx].nombre,
                   equipo_visitante.jugadores[jugador_idx].numero,
                   equipo_visitante.nombre);
            if (asistente_idx != jugador_idx)
            {
                printf("   Asistencia de %s (%d)\n",
                       equipo_visitante.jugadores[asistente_idx].nombre,
                       equipo_visitante.jugadores[asistente_idx].numero);
            }
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

    // Resultados finales
    clear_screen();
    print_header("FIN DEL PARTIDO");

    printf("*** RESULTADO FINAL ***\n\n");
    printf("*** 60 MINUTOS COMPLETADOS ***\n\n");

    printf("*** %s %d - %d %s ***\n\n",
           equipo_local.nombre, goles_local, goles_visitante, equipo_visitante.nombre);

    if (goles_local > goles_visitante)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_local.nombre);
    }
    else if (goles_visitante > goles_local)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_visitante.nombre);
    }
    else
    {
        printf("*** ¡EMPATE! ***\n\n");
    }

    // Mostrar estadísticas
    printf("*** ESTADISTICAS DEL PARTIDO ***\n\n");

    printf("EQUIPO LOCAL (%s):\n", equipo_local.nombre);
    for (int i = 0; i < equipo_local.num_jugadores; i++)
    {
        if (estadisticas_local[i] > 0 || asistencias_local[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_local.jugadores[i].nombre,
                   equipo_local.jugadores[i].numero,
                   estadisticas_local[i], asistencias_local[i]);
        }
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante.nombre);
    for (int i = 0; i < equipo_visitante.num_jugadores; i++)
    {
        if (estadisticas_visitante[i] > 0 || asistencias_visitante[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_visitante.jugadores[i].nombre,
                   equipo_visitante.jugadores[i].numero,
                   estadisticas_visitante[i], asistencias_visitante[i]);
        }
    }

    // Guardar resultados en la base de datos
    guardar_resultados_simulacion(&equipo_local, &equipo_visitante,
                                  goles_local, goles_visitante,
                                  estadisticas_local, estadisticas_visitante,
                                  asistencias_local, asistencias_visitante);

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
