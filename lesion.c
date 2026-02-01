#include "lesion.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "estadisticas_lesiones.h"
#include "camiseta.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int current_lesion_id;

/**
 * @brief Preparar statement y reportar errores
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

/**
 * @brief Obtener estado por opción
 */
static const char *estado_por_opcion(int opcion)
{
    switch (opcion)
    {
    case 1:
        return "ACTIVA";
    case 2:
        return "EN_TRATAMIENTO";
    case 3:
        return "MEJORANDO";
    case 4:
        return "RECUPERADO";
    case 5:
        return "RECAÍDA";
    default:
        return NULL;
    }
}

/**
 * @brief Ejecutar update simple con texto
 */
static void ejecutar_update_texto(const char *sql, const char *valor, int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Ejecutar update simple con entero
 */
static void ejecutar_update_int(const char *sql, int valor, int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Crea una nueva lesión en la base de datos
 *
 * Solicita al usuario el tipo, descripción de la lesión, el ID de la camiseta asociada
 * y el estado inicial, y la inserta en la tabla 'lesion'. El nombre del jugador se obtiene del usuario actual.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_lesion()
{
    clear_screen();
    char tipo[100];
    char descripcion[200];
    char fecha[20];
    char estado[50];
    int camiseta_id;

    input_string("Tipo de lesion: ", tipo, sizeof(tipo));
    input_string("Descripcion: ", descripcion, sizeof(descripcion));
    camiseta_id = input_int("ID de la Camiseta Asociada: ");

    // Mostrar opciones de estado
    printf("\nEstados disponibles:\n");
    printf("1. ACTIVA - Lesión reciente, jugador NO apto\n");
    printf("2. EN_TRATAMIENTO - Está en rehabilitación\n");
    printf("3. MEJORANDO - Evolución positiva\n");
    printf("4. RECUPERADO - Alta médica\n");
    printf("5. RECAÍDA - Vuelve la lesión\n");

    int opcion_estado = input_int("Seleccione estado inicial (1-5): ");
    const char *estado_sel = estado_por_opcion(opcion_estado);
    if (!estado_sel)
    {
        estado_sel = "ACTIVA";
    }
    strcpy_s(estado, sizeof(estado), estado_sel);

    get_datetime(fecha, sizeof(fecha));

    char *jugador = get_user_name();
    if (!jugador)
    {
        jugador = "Usuario Desconocido";
    }

    long long id = obtener_siguiente_id("lesion");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO lesion(id, jugador, tipo, descripcion, fecha, camiseta_id, estado) VALUES(?,?,?,?,?,?,?)",
                &stmt))
    {
        if (strcmp(jugador, "Usuario Desconocido") != 0)
        {
            free(jugador);
        }
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, jugador, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, camiseta_id);
    sqlite3_bind_text(stmt, 7, estado, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (strcmp(jugador, "Usuario Desconocido") != 0)
    {
        free(jugador);
    }

    printf("\nLesion creada correctamente con estado: %s\n", estado);
    pause_console();
}

/**
 * @brief Muestra un listado de todas las lesiones registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las lesiones
 * con sus respectivos datos: ID, tipo, descripción, fecha y estado.
 * Si no hay lesiones registradas, muestra un mensaje informativo.
 */
void listar_lesiones()
{
    mostrar_pantalla("LISTADO DE LESIONES");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "SELECT id, jugador, tipo, descripcion, fecha, camiseta_id, estado, partido_id FROM lesion ORDER BY fecha DESC",
                &stmt))
    {
        pause_console();
        return;
    }

    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *estado = (const char *)sqlite3_column_text(stmt, 6);
        const char *estado_display = estado ? estado : "ACTIVA"; // Default si es NULL
        int partido_id = sqlite3_column_int(stmt, 7);

        printf("ID: %d | Jugador: %s | Tipo: %s | Descripcion: %s | Fecha: %s | Camiseta ID: %d | Estado: %s | Partido ID: %d\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_text(stmt, 3),
               sqlite3_column_text(stmt, 4),
               sqlite3_column_int(stmt, 5),
               estado_display,
               partido_id);
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("lesiones");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Modifica el tipo de una lesión existente
 */
static void modificar_tipo_lesion()
{
    char tipo[100];
    input_string("Nuevo tipo de lesion: ", tipo, sizeof(tipo));
    ejecutar_update_texto("UPDATE lesion SET tipo=? WHERE id=?", tipo, current_lesion_id);
    printf("Tipo modificado correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la descripción de una lesión existente
 */
static void modificar_descripcion_lesion()
{
    char descripcion[200];
    input_string("Nueva descripcion: ", descripcion, sizeof(descripcion));
    ejecutar_update_texto("UPDATE lesion SET descripcion=? WHERE id=?", descripcion, current_lesion_id);
    printf("Descripcion modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la fecha de una lesión existente
 */
static void modificar_fecha_lesion()
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
    ejecutar_update_texto("UPDATE lesion SET fecha=? WHERE id=?", fecha_hora, current_lesion_id);
    printf("Fecha modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la camiseta de una lesión existente
 */
static void modificar_camiseta_lesion()
{
    listar_camisetas();
    int camiseta_id = input_int("Nuevo ID de la Camiseta Asociada: ");
    ejecutar_update_int("UPDATE lesion SET camiseta_id=? WHERE id=?", camiseta_id, current_lesion_id);
    printf("Camiseta modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el estado de una lesión existente
 */
static void modificar_estado_lesion()
{
    printf("\nEstados disponibles:\n");
    printf("1. ACTIVA - Lesión reciente, jugador NO apto\n");
    printf("2. EN_TRATAMIENTO - Está en rehabilitación\n");
    printf("3. MEJORANDO - Evolución positiva\n");
    printf("4. RECUPERADO - Alta médica\n");
    printf("5. RECAÍDA - Vuelve la lesión\n");

    int opcion_estado = input_int("Seleccione nuevo estado (1-5): ");
    const char *estado = estado_por_opcion(opcion_estado);
    if (!estado)
    {
        printf("Opción inválida\n");
        pause_console();
        return;
    }

    ejecutar_update_texto("UPDATE lesion SET estado=? WHERE id=?", estado, current_lesion_id);
    printf("Estado modificado correctamente a: %s\n", estado);
    pause_console();
}

/**
 * @brief Modifica todos los campos de una lesión existente
 */
static void modificar_todo_lesion()
{
    char tipo[100];
    char descripcion[200];
    char fecha[20];
    char estado[50];
    int camiseta_id;

    input_string("Nuevo tipo de lesion: ", tipo, sizeof(tipo));
    input_string("Nueva descripcion: ", descripcion, sizeof(descripcion));
    input_date("Nueva fecha (DD/MM/YYYY HH:MM): ", fecha, sizeof(fecha));
    camiseta_id = input_int("Nuevo ID de la Camiseta Asociada: ");

    // Mostrar opciones de estado
    printf("\nEstados disponibles:\n");
    printf("1. ACTIVA - Lesión reciente, jugador NO apto\n");
    printf("2. EN_TRATAMIENTO - Está en rehabilitación\n");
    printf("3. MEJORANDO - Evolución positiva\n");
    printf("4. RECUPERADO - Alta médica\n");
    printf("5. RECAÍDA - Vuelve la lesión\n");

    int opcion_estado = input_int("Seleccione estado (1-5): ");
    const char *estado_sel = estado_por_opcion(opcion_estado);
    if (!estado_sel)
    {
        estado_sel = "ACTIVA";
    }
    strcpy_s(estado, sizeof(estado), estado_sel);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "UPDATE lesion SET tipo=?, descripcion=?, fecha=?, camiseta_id=?, estado=? WHERE id=?",
                &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, camiseta_id);
    sqlite3_bind_text(stmt, 5, estado, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, current_lesion_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Lesion modificada correctamente\n");
    pause_console();
}

/**
 * @brief Permite modificar una lesión existente
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a modificar,
 * verifica que exista y muestra un menú con opciones para modificar campos individuales o todos.
 */
void modificar_lesion()
{
    mostrar_pantalla("MODIFICAR LESION");

    if (!hay_registros("lesion"))
    {
        mostrar_no_hay_registros("lesion");
        pause_console();
        return;
    }

    printf("Lesiones disponibles:\n\n");
    listar_lesiones();

    int id = input_int("\nID Lesion a Modificar (0 para cancelar): ");

    if (!existe_id("lesion", id))
    {
        mostrar_no_existe("lesion");
        return;
    }

    current_lesion_id = id;

    MenuItem items[] =
    {
        {1, "Tipo", modificar_tipo_lesion},
        {2, "Descripcion", modificar_descripcion_lesion},
        {3, "Fecha", modificar_fecha_lesion},
        {4, "Camiseta", modificar_camiseta_lesion},
        {5, "Estado", modificar_estado_lesion},
        {6, "Modificar Todo", modificar_todo_lesion},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR LESION", items, 7);
}

/**
 * @brief Elimina una lesión de la base de datos
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de eliminar.
 * Una vez eliminada, el ID queda disponible para reutilización.
 */
void eliminar_lesion()
{
    mostrar_pantalla("ELIMINAR LESION");

    if (!hay_registros("lesion"))
    {
        mostrar_no_hay_registros("lesiones");
        pause_console();
        return;
    }

    printf("Lesiones disponibles:\n\n");
    listar_lesiones();

    int id = input_int("\nID a eliminar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("lesion", id))
    {
        mostrar_no_existe("lesion");
        pause_console();
        return;
    }

    if (!confirmar("¿Seguro que desea eliminar esta lesion?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt("DELETE FROM lesion WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("\nLesion eliminada correctamente\n");
    pause_console();
}

/**
 * @brief Calcula la diferencia en días entre dos fechas de lesiones
 *
 * @param fecha1 Primera fecha en formato string
 * @param fecha2 Segunda fecha en formato string
 * @return Diferencia en días (positivo si fecha2 es posterior a fecha1)
 */
static int calcular_diferencia_dias(const char *fecha1, const char *fecha2)
{
    // Para simplificar, usaremos una conversión básica
    // En un sistema real, se debería usar una librería de fechas más robusta

    int dia1;
    int mes1;
    int ano1;
    int hora1;
    int min1;
    int dia2;
    int mes2;
    int ano2;
    int hora2;
    int min2;

    // Parsear fecha1 (formato esperado: "DD/MM/YYYY HH:MM" o similar)
    sscanf_s(fecha1, "%d/%d/%d %d:%d", &dia1, &mes1, &ano1, &hora1, &min1);

    // Parsear fecha2
    sscanf_s(fecha2, "%d/%d/%d %d:%d", &dia2, &mes2, &ano2, &hora2, &min2);

    // Convertir a días desde una fecha base (simplificación)
    int dias1 = ano1 * 365 + mes1 * 30 + dia1;
    int dias2 = ano2 * 365 + mes2 * 30 + dia2;

    return dias2 - dias1;
}

/**
 * @brief Muestra diferencias de días entre lesiones consecutivas
 */
void mostrar_diferencias_lesiones()
{
    clear_screen();
    print_header("DIFERENCIAS ENTRE LESIONES");

    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, fecha, tipo FROM lesion ORDER BY fecha ASC", &stmt))
    {
        pause_console();
        return;
    }

    char fecha_anterior[50] = "";
    int primera_lesion = 1;

    printf("Diferencias de días entre lesiones consecutivas:\n\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha_actual = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);

        if (!primera_lesion)
        {
            int dias_diferencia = calcular_diferencia_dias(fecha_anterior, fecha_actual);
            printf("Lesion ID %d (%s) - %d días después\n", id, tipo, dias_diferencia);
        }
        else
        {
            printf("Lesion ID %d (%s) - Primera lesión\n", id, tipo);
            primera_lesion = 0;
        }

        strcpy_s(fecha_anterior, sizeof(fecha_anterior), fecha_actual);
    }

    sqlite3_finalize(stmt);

    if (primera_lesion)
    {
        printf("No hay lesiones registradas.\n");
    }

    pause_console();
}

/**
 * @brief Pregunta al usuario si desea actualizar el estado de las lesiones activas
 */
void actualizar_estados_lesiones()
{
    mostrar_pantalla("ACTUALIZAR ESTADOS DE LESIONES");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "SELECT id, tipo, descripcion, fecha, estado FROM lesion WHERE estado != 'RECUPERADO' ORDER BY fecha DESC",
                &stmt))
    {
        pause_console();
        return;
    }

    int lesiones_activas = 0;

    printf("Lesiones que pueden requerir actualización de estado:\n\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 1);
        const char *descripcion = (const char *)sqlite3_column_text(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);
        const char *estado = (const char *)sqlite3_column_text(stmt, 4);

        printf("ID: %d | Tipo: %s | Estado actual: %s | Fecha: %s\n", id, tipo, estado, fecha);
        printf("  Descripción: %s\n\n", descripcion);
        lesiones_activas++;
    }

    sqlite3_finalize(stmt);

    if (lesiones_activas == 0)
    {
        mostrar_no_hay_registros("lesiones activas");
        pause_console();
        return;
    }

    if (confirmar("¿Desea actualizar el estado de alguna lesión?"))
    {
        int id_lesion = input_int("Ingrese el ID de la lesión a actualizar (0 para cancelar): ");

        if (id_lesion != 0 && existe_id("lesion", id_lesion))
        {
            current_lesion_id = id_lesion;
            modificar_estado_lesion();
        }
    }

    pause_console();
}

/**
 * @brief Muestra el menú principal de gestión de lesiones
 *
 * Presenta un menú interactivo con opciones para crear, listar, editar
 * y eliminar lesiones. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_lesiones()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_lesion},
        {2, "Listar", listar_lesiones},
        {3, "Modificar", modificar_lesion},
        {4, "Eliminar", eliminar_lesion},
        {5, "Estadisticas", mostrar_estadisticas_lesiones},
        {6, "Diferencias entre Lesiones", mostrar_diferencias_lesiones},
        {7, "Actualizar Estados", actualizar_estados_lesiones},
        {0, "Volver", NULL}
    };
    ejecutar_menu("LESIONES", items, 8);
}
