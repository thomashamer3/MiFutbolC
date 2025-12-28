#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include <stdio.h>

/**
 * @brief Convierte el número de resultado a texto
 *
 * @param resultado Número del resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA)
 * @return Cadena de texto correspondiente al resultado
 */
static const char *resultado_to_text(int resultado)
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
 * @brief Convierte el número de clima a texto
 *
 * @param clima Número del clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio)
 * @return Cadena de texto correspondiente al clima
 */
static const char *clima_to_text(int clima)
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
 * @brief Convierte el número de dia a texto
 *
 * @param dia Número del dia (1=Dia, 2=Tarde, 3=Noche)
 * @return Cadena de texto correspondiente al dia
 */
static const char *dia_to_text(int dia)
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
 * @brief Crea un nuevo partido en la base de datos
 *
 * Solicita al usuario los datos del partido (cancha_id, goles, asistencias, camiseta, resultado)
 * y lo inserta en la tabla 'partido'. Obtiene la fecha y hora actual automáticamente.
 * Verifica que la cancha y camiseta seleccionadas existan antes de insertar.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_partido()
{
    char fecha[20], comentario_personal[256];
    int cancha_id, goles, asistencias, camiseta, resultado, rendimiento_general, cansancio, estado_animo, clima, dia;

    // Verificar si hay canchas y camisetas registradas
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
        return;
    }

    // Listar canchas disponibles
    printf("Canchas disponibles:\n");
    sqlite3_stmt *stmt_canchas;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM cancha ORDER BY id", -1, &stmt_canchas, NULL);
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        printf("%d | %s\n", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);

    cancha_id = input_int("ID Cancha, (0 para Cancelar): ");
    if (!existe_id("cancha", cancha_id))
        return;

    goles = input_int("Goles: ");
    asistencias = input_int("Asistencias: ");
    resultado = input_int("Resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (resultado < 1 || resultado > 3)
    {
        resultado = input_int("Resultado invalido. (1=VICTORIA, 2=EMPATE, 3=DERROTA):");
    }
    listar_camisetas();
    camiseta = input_int("ID Camiseta: ");

    if (!existe_id("camiseta", camiseta))
        return;

    rendimiento_general = input_int("Rendimiento general (1-10): ");
    while (rendimiento_general < 1 || rendimiento_general > 10)
    {
        rendimiento_general = input_int("Rendimiento invalido. Ingrese entre 1 y 10: ");
    }

    cansancio = input_int("Cansancio (1-10): ");
    while (cansancio < 1 || cansancio > 10)
    {
        cansancio = input_int("Cansancio invalido. Ingrese entre 1 y 10:  ");
    }

    estado_animo = input_int("Estado de Animo (1-10): ");
    while (estado_animo < 1 || estado_animo > 10)
    {
        estado_animo = input_int("Estado de Animo invalido. Ingrese entre 1 y 10: ");
    }

    input_string("Comentario personal: ", comentario_personal, sizeof(comentario_personal));

    clima = input_int("Clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio):");
    while (clima < 1 || clima > 6)
    {
        clima = input_int("Clima invalido (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    }

    dia = input_int("Dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (dia < 1 || dia > 3)
    {
        dia = input_int("Dia invalido (1=Dia, 2=Tarde, 3=Noche): ");
    }

    get_datetime(fecha, sizeof(fecha));

    int id = obtener_siguiente_id_partido();
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
                       "JOIN cancha can ON p.cancha_id = can.id ORDER BY fecha_hora DESC",
                       -1, &stmt, NULL);

    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d |Cancha:%s |Fecha:%s | G:%d A:%d |Camiseta:%s | %s |Clima:%s |Dia:%s\n",
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

static int current_partido_id;

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

static void modificar_fecha_hora_partido()
{
    char fecha[20], hora[10], fecha_hora[30];
    input_string("Nueva fecha (dd/mm/yyyy): ", fecha, sizeof(fecha));
    input_string("Nueva hora (hh:mm): ", hora, sizeof(hora));
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

static void modificar_todo_partido()
{
    int cancha_id, goles, asistencias, camiseta, resultado, clima, dia;
    char fecha[20], hora[10], fecha_hora[30];
    // Listar canchas disponibles
    printf("Canchas disponibles:\n");
    sqlite3_stmt *stmt_canchas;
    sqlite3_prepare_v2(db, "SELECT id, nombre FROM cancha ORDER BY id", -1, &stmt_canchas, NULL);
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        printf("%d | %s\n", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);
    cancha_id = input_int("Nuevo ID Cancha: ");
    if (!existe_id("cancha", cancha_id))
        return;
    input_string("Nueva fecha (dd/mm/yyyy): ", fecha, sizeof(fecha));
    input_string("Nueva hora (hh:mm): ", hora, sizeof(hora));
    sprintf(fecha_hora, "%s %s", fecha, hora);
    goles = input_int("Nuevos goles: ");
    asistencias = input_int("Nuevas asistencias: ");
    printf("Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    resultado = input_int("Nuevo resultado: ");
    while (resultado < 1 || resultado > 3)
    {
        printf("Resultado invalido. Ingrese 1, 2 o 3: ");
        resultado = input_int("Nuevo resultado: ");
    }
    listar_camisetas();
    camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", camiseta))
    {
        printf("La camiseta no existe\n");
        return;
    }
    printf("Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    clima = input_int("Nuevo clima: ");
    while (clima < 1 || clima > 6)
    {
        printf("Clima invalido. Ingrese entre 1 y 6: ");
        clima = input_int("Nuevo clima: ");
    }
    printf("Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ");
    dia = input_int("Nuevo dia: ");
    while (dia < 1 || dia > 3)
    {
        printf("Dia invalido. Ingrese 1, 2 o 3: ");
        dia = input_int("Nuevo dia: ");
    }
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
        {9, "Modificar Todo", modificar_todo_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR PARTIDO", items, 10);
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
 * @brief Muestra el menú principal de gestión de partidos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar partidos. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_partidos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_partido},
        {2, "Listar", listar_partidos},
        {3, "Modificar", modificar_partido},
        {4, "Eliminar", eliminar_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("PARTIDOS", items, 5);
}
