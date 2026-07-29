/**
 * @file ranking_amigos.c
 * @brief Implementacion del modulo de ranking de amigos
 *
 * CRUD completo para registrar amigos del jugador, almacenar sus
 * estadisticas futbolisticas y generar rankings comparativos.
 * Permite comparar las estadisticas del usuario local con las
 * de cualquier amigo registrado.
 */

#include "ranking_amigos.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Estructura temporal para cargar datos de un amigo en memoria
 */
typedef struct
{
    long long id;
    char nombre[100];
    char posicion[50];
    int goles_total;
    int asistencias_total;
    int partidos_total;
    double mejor_rendimiento;
    char notas[300];
} AmigoTemp;

/**
 * @brief Carga los stats del usuario local desde la tabla partido
 *
 * Obtiene COUNT, SUM(goles), SUM(asistencias) y AVG(rendimiento_general)
 * de todos los partidos registrados.
 *
 * @param[out] partidos Total de partidos jugados
 * @param[out] goles Total de goles marcados
 * @param[out] asistencias Total de asistencias
 * @param[out] rendimiento_promedio Rendimiento promedio
 */
static void cargar_stats_local(int *partidos, int *goles, int *asistencias,
                               double *rendimiento_promedio)
{
    *partidos = 0;
    *goles = 0;
    *asistencias = 0;
    *rendimiento_promedio = 0.0;

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "SELECT COUNT(*), IFNULL(SUM(goles), 0), "
                         "IFNULL(SUM(asistencias), 0), "
                         "IFNULL(AVG(CASE WHEN rendimiento_general > 0 "
                         "THEN rendimiento_general END), 0) "
                         "FROM partido"))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *partidos = sqlite3_column_int(stmt, 0);
        *goles = sqlite3_column_int(stmt, 1);
        *asistencias = sqlite3_column_int(stmt, 2);
        *rendimiento_promedio = sqlite3_column_double(stmt, 3);
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Lista amigos en formato compacto (sin pausa)
 *
 * Muestra id, nombre y posicion de cada amigo registrado.
 */
static void listar_amigos_simple(void)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "SELECT id, nombre, IFNULL(posicion, '') "
                         "FROM amigo ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        const char *posicion = (const char *)sqlite3_column_text(stmt, 2);

        printf("%2d - %-20s %s\n", id,
               nombre ? nombre : "(sin nombre)",
               posicion && posicion[0] != '\0' ? posicion : "-");
        hay = 1;
    }

    if (!hay)
    {
        mostrar_no_hay_registros("amigos cargados");
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Agrega un amigo al ranking
 *
 * Solicita nombre (obligatorio), posicion, goles_total,
 * asistencias_total, partidos_total, mejor_rendimiento y notas.
 * Usa obtener_siguiente_id("amigo") para el ID.
 */
void agregar_amigo(void)
{
    clear_screen();
    print_header("AGREGAR AMIGO");

    char nombre[100];
    char posicion[50];
    char notas[300];

    while (1)
    {
        input_string("Nombre del amigo: ", nombre, sizeof(nombre));
        trim_whitespace(nombre);
        if (nombre[0] != '\0')
        {
            break;
        }
        printf("El nombre no puede estar vacio.\n");
    }

    input_string("Posicion (ej: Delantero, Mediocampista, Defensor, Portero): ",
                 posicion, sizeof(posicion));
    int goles_total = input_int("Goles totales: ");
    int asistencias_total = input_int("Asistencias totales: ");
    int partidos_total = input_int("Partidos totales: ");
    double mejor_rendimiento = input_double("Mejor rendimiento: ");
    input_string("Notas: ", notas, sizeof(notas));

    long long amigo_id = obtener_siguiente_id("amigo");

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "INSERT INTO amigo(id, nombre, posicion, "
                         "goles_total, asistencias_total, partidos_total, "
                         "mejor_rendimiento, notas) "
                         "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"))
    {
        printf("Error al crear el amigo.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int64(stmt, 1, amigo_id);
    sqlite3_bind_text(stmt, 2, nombre, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 3, posicion, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 4, goles_total);
    sqlite3_bind_int(stmt, 5, asistencias_total);
    sqlite3_bind_int(stmt, 6, partidos_total);
    sqlite3_bind_double(stmt, 7, mejor_rendimiento);
    sqlite3_bind_text(stmt, 8, notas, -1, DB_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creado amigo id=%lld nombre=%.180s",
                 amigo_id, nombre);
        app_log_event("RANKING_AMIGOS", log_msg);
        mostrar_alerta_operacion("Amigo", "Agregado", nombre);
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear amigo nombre=%.180s",
                 nombre);
        app_log_event("RANKING_AMIGOS", log_msg);
        printf("Error al agregar el amigo en la base de datos.\n");
        pause_console();
    }
}

/**
 * @brief Lista todos los amigos registrados
 *
 * Muestra id, nombre, posicion, goles, asistencias y partidos
 * de cada amigo almacenado en la tabla amigo.
 */
void listar_amigos(void)
{
    clear_screen();
    print_header("LISTADO DE AMIGOS");
    app_log_event("RANKING_AMIGOS", "Listado de amigos consultado");

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "SELECT id, nombre, IFNULL(posicion, ''), "
                         "IFNULL(goles_total, 0), "
                         "IFNULL(asistencias_total, 0), "
                         "IFNULL(partidos_total, 0) "
                         "FROM amigo ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    int hay = 0;
    printf("%-4s %-20s %-15s %-7s %-7s %-7s\n",
           "ID", "Nombre", "Posicion", "Goles", "Asist.", "Partidos");
    printf("--------------------------------------------------------------\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        const char *posicion = (const char *)sqlite3_column_text(stmt, 2);
        int goles = sqlite3_column_int(stmt, 3);
        int asistencias = sqlite3_column_int(stmt, 4);
        int partidos = sqlite3_column_int(stmt, 5);

        printf("%-4d %-20s %-15s %-7d %-7d %-7d\n",
               id,
               nombre ? nombre : "-",
               posicion && posicion[0] != '\0' ? posicion : "-",
               goles, asistencias, partidos);
        hay = 1;
    }

    sqlite3_finalize(stmt);

    if (!hay)
    {
        mostrar_no_hay_registros("amigos registrados");
    }

    pause_console();
}

static int modificar_amigo_string(int id, const char *nombre_actual, int campo)
{
    sqlite3_stmt *stmt;
    const char *campo_sql = NULL;
    const char *prompt = NULL;

    switch (campo)
    {
    case 1:
        campo_sql = "nombre";
        prompt = "Nuevo nombre: ";
        break;
    case 2:
        campo_sql = "posicion";
        prompt = "Nueva posicion: ";
        break;
    default:
        return 0;
    }

    char nuevo_valor[300];
    input_string(prompt, nuevo_valor, sizeof(nuevo_valor));
    trim_whitespace(nuevo_valor);

    if (campo == 1 && nuevo_valor[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return 0;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE amigo SET %s = ? WHERE id = ?", campo_sql);

    if (!db_prepare_stmt(&stmt, sql))
    {
        printf("Error al actualizar el amigo.\n");
        pause_console();
        return 0;
    }

    sqlite3_bind_text(stmt, 1, nuevo_valor, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
                 "Modificado amigo id=%d campo=%s", id, campo_sql);
        app_log_event("RANKING_AMIGOS", log_msg);
        mostrar_alerta_operacion("Amigo", "Modificado", nombre_actual);
    }
    else
    {
        printf("Error al modificar el amigo.\n");
        pause_console();
        return 0;
    }
    return 1;
}

static int modificar_amigo_numerico(int id, const char *nombre_actual, int campo)
{
    sqlite3_stmt *stmt;

    if (campo == 7)
    {
        char nuevo_valor[300];
        input_string("Nuevas notas: ", nuevo_valor, sizeof(nuevo_valor));

        if (!db_prepare_stmt(&stmt,
                             "UPDATE amigo SET notas = ? WHERE id = ?"))
        {
            printf("Error al actualizar el amigo.\n");
            pause_console();
            return 0;
        }

        sqlite3_bind_text(stmt, 1, nuevo_valor, -1, DB_TRANSIENT);
        sqlite3_bind_int(stmt, 2, id);
        int result = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result == SQLITE_DONE)
        {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                     "Modificado amigo id=%d campo=notas", id);
            app_log_event("RANKING_AMIGOS", log_msg);
            mostrar_alerta_operacion("Amigo", "Modificado", nombre_actual);
        }
        else
        {
            printf("Error al modificar el amigo.\n");
            pause_console();
            return 0;
        }
        return 1;
    }

    int nuevo_int = 0;
    double nuevo_double = 0.0;
    const char *campo_sql = NULL;

    switch (campo)
    {
    case 3:
        campo_sql = "goles_total";
        nuevo_int = input_int("Nuevos goles totales: ");
        break;
    case 4:
        campo_sql = "asistencias_total";
        nuevo_int = input_int("Nuevas asistencias totales: ");
        break;
    case 5:
        campo_sql = "partidos_total";
        nuevo_int = input_int("Nuevos partidos totales: ");
        break;
    case 6:
        campo_sql = "mejor_rendimiento";
        nuevo_double = input_double("Nuevo mejor rendimiento: ");
        break;
    default:
        return 0;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE amigo SET %s = ? WHERE id = ?", campo_sql);

    if (!db_prepare_stmt(&stmt, sql))
    {
        printf("Error al actualizar el amigo.\n");
        pause_console();
        return 0;
    }

    if (campo == 6)
    {
        sqlite3_bind_double(stmt, 1, nuevo_double);
    }
    else
    {
        sqlite3_bind_int(stmt, 1, nuevo_int);
    }
    sqlite3_bind_int(stmt, 2, id);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
                 "Modificado amigo id=%d campo=%s", id, campo_sql);
        app_log_event("RANKING_AMIGOS", log_msg);
        mostrar_alerta_operacion("Amigo", "Modificado", nombre_actual);
    }
    else
    {
        printf("Error al modificar el amigo.\n");
        pause_console();
        return 0;
    }
    return 1;
}

/**
 * @brief Modifica un amigo existente en la base de datos
 *
 * Solicita el ID, muestra los valores actuales y permite
 * modificar uno de los 7 campos disponibles.
 */
void modificar_amigo(void)
{
    clear_screen();
    print_header("MODIFICAR AMIGO");

    if (!hay_registros("amigo"))
    {
        mostrar_no_hay_registros("amigos para modificar");
        pause_console();
        return;
    }

    listar_amigos_simple();

    int id = input_int("\nID del amigo a modificar (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("amigo", id))
    {
        mostrar_no_existe("Amigo");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "SELECT nombre, IFNULL(posicion, ''), "
                         "IFNULL(goles_total, 0), "
                         "IFNULL(asistencias_total, 0), "
                         "IFNULL(partidos_total, 0), "
                         "IFNULL(mejor_rendimiento, 0), "
                         "IFNULL(notas, '') "
                         "FROM amigo WHERE id = ?"))
    {
        printf("Error al consultar el amigo.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        mostrar_no_existe("Amigo");
        pause_console();
        return;
    }

    char actual_nombre[100];
    char actual_posicion[50];
    int actual_goles = sqlite3_column_int(stmt, 2);
    int actual_asistencias = sqlite3_column_int(stmt, 3);
    int actual_partidos = sqlite3_column_int(stmt, 4);
    double actual_mejor_rend = sqlite3_column_double(stmt, 5);
    char actual_notas[300];

    snprintf(actual_nombre, sizeof(actual_nombre), "%s",
             (const char *)sqlite3_column_text(stmt, 0));
    snprintf(actual_posicion, sizeof(actual_posicion), "%s",
             (const char *)sqlite3_column_text(stmt, 1));
    snprintf(actual_notas, sizeof(actual_notas), "%s",
             (const char *)sqlite3_column_text(stmt, 6));
    sqlite3_finalize(stmt);

    printf("\nValores actuales:\n");
    printf("1) Nombre          : %s\n", actual_nombre);
    printf("2) Posicion        : %s\n", actual_posicion);
    printf("3) Goles totales   : %d\n", actual_goles);
    printf("4) Asistencias     : %d\n", actual_asistencias);
    printf("5) Partidos        : %d\n", actual_partidos);
    printf("6) Mejor rendim.   : %.1f\n", actual_mejor_rend);
    printf("7) Notas           : %s\n", actual_notas);
    printf("0) Volver\n");

    int campo = input_int("\nQue campo desea modificar? ");

    if (campo < 1 || campo > 7)
    {
        return;
    }

    if (campo >= 1 && campo <= 2)
    {
        modificar_amigo_string(id, actual_nombre, campo);
    }
    else
    {
        modificar_amigo_numerico(id, actual_nombre, campo);
    }
}

/**
 * @brief Elimina un amigo del ranking
 *
 * Solicita el ID, muestra listado y pide confirmacion antes de eliminar.
 */
void eliminar_amigo(void)
{
    clear_screen();
    print_header("ELIMINAR AMIGO");

    if (!hay_registros("amigo"))
    {
        mostrar_no_hay_registros("amigos para eliminar");
        pause_console();
        return;
    }

    listar_amigos_simple();

    int id = input_int("\nID del amigo a eliminar (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("amigo", id))
    {
        mostrar_no_existe("Amigo");
        pause_console();
        return;
    }

    if (!confirmar("Esta seguro de eliminar este amigo?"))
    {
        return;
    }

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "DELETE FROM amigo WHERE id = ?"))
    {
        printf("Error al eliminar el amigo.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Eliminado amigo id=%d", id);
        app_log_event("RANKING_AMIGOS", log_msg);
        mostrar_alerta_operacion("Amigo", "Eliminado", NULL);
    }
    else
    {
        printf("Error al eliminar el amigo.\n");
        pause_console();
    }
}

/**
 * @brief Carga todos los amigos en un arreglo temporal
 *
 * @param[out] amigos Arreglo donde almacenar los amigos cargados
 * @param[out] total Cantidad de amigos cargados
 * @param max Maximo de amigos a cargar
 */
static void cargar_amigos(AmigoTemp *amigos, int *total, int max)
{
    *total = 0;

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "SELECT id, nombre, IFNULL(posicion, ''), "
                         "IFNULL(goles_total, 0), "
                         "IFNULL(asistencias_total, 0), "
                         "IFNULL(partidos_total, 0), "
                         "IFNULL(mejor_rendimiento, 0), "
                         "IFNULL(notas, '') "
                         "FROM amigo ORDER BY goles_total DESC"))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && *total < max)
    {
        amigos[*total].id = sqlite3_column_int64(stmt, 0);
        snprintf(amigos[*total].nombre, sizeof(amigos[*total].nombre), "%s",
                 (const char *)sqlite3_column_text(stmt, 1));
        snprintf(amigos[*total].posicion, sizeof(amigos[*total].posicion), "%s",
                 (const char *)sqlite3_column_text(stmt, 2));
        amigos[*total].goles_total = sqlite3_column_int(stmt, 3);
        amigos[*total].asistencias_total = sqlite3_column_int(stmt, 4);
        amigos[*total].partidos_total = sqlite3_column_int(stmt, 5);
        amigos[*total].mejor_rendimiento = sqlite3_column_double(stmt, 6);
        snprintf(amigos[*total].notas, sizeof(amigos[*total].notas), "%s",
                 (const char *)sqlite3_column_text(stmt, 7));
        (*total)++;
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra el ranking completo de amigos
 *
 * Carga todos los amigos y las estadisticas del usuario local
 * (partidos, goles, asistencias, rendimiento promedio).
 * Ordena por goles_total de mayor a menor y muestra una tabla
 * numerada. El usuario local aparece en la lista con prefijo *.
 */
void mostrar_ranking(void)
{
    clear_screen();
    print_header("RANKING DE AMIGOS");
    app_log_event("RANKING_AMIGOS", "Ranking consultado");

    AmigoTemp amigos[100];
    int total_amigos = 0;
    cargar_amigos(amigos, &total_amigos, 100);

    int local_partidos = 0;
    int local_goles = 0;
    int local_asistencias = 0;
    double local_rendimiento = 0.0;
    cargar_stats_local(&local_partidos, &local_goles, &local_asistencias,
                       &local_rendimiento);

    if (total_amigos == 0 && local_goles == 0)
    {
        mostrar_no_hay_registros("amigos ni partidos para rankear");
        pause_console();
        return;
    }

    int total_entries = total_amigos + (local_goles > 0 || local_partidos > 0 ? 1 : 0);

    if (total_entries <= 0)
    {
        mostrar_no_hay_registros("datos para rankear");
        pause_console();
        return;
    }

    typedef struct
    {
        const char *nombre;
        int goles;
        int asistencias;
        int partidos;
        double mejor_rendimiento;
        int es_local;
    } RankingEntry;

    RankingEntry *entries = (RankingEntry *)calloc((size_t)total_entries, sizeof(RankingEntry));
    if (!entries)
    {
        printf("Error de memoria.\n");
        pause_console();
        return;
    }

    int idx = 0;
    for (int i = 0; i < total_amigos; i++)
    {
        entries[idx].nombre = amigos[i].nombre;
        entries[idx].goles = amigos[i].goles_total;
        entries[idx].asistencias = amigos[i].asistencias_total;
        entries[idx].partidos = amigos[i].partidos_total;
        entries[idx].mejor_rendimiento = amigos[i].mejor_rendimiento;
        entries[idx].es_local = 0;
        idx++;
    }

    if (local_goles > 0 || local_partidos > 0)
    {
        entries[idx].nombre = "Yo (usuario local)";
        entries[idx].goles = local_goles;
        entries[idx].asistencias = local_asistencias;
        entries[idx].partidos = local_partidos;
        entries[idx].mejor_rendimiento = local_rendimiento;
        entries[idx].es_local = 1;
        idx++;
    }

    for (int i = 0; i < total_entries - 1; i++)
    {
        for (int j = 0; j < total_entries - i - 1; j++)
        {
            if (entries[j].goles < entries[j + 1].goles)
            {
                RankingEntry tmp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = tmp;
            }
        }
    }

    printf("\n%-4s %-22s %-7s %-7s %-7s %-10s\n",
           "#", "Nombre", "Goles", "Asist.", "Partidos", "Mej.Rend.");
    printf("--------------------------------------------------------------\n");

    for (int i = 0; i < total_entries; i++)
    {
        const char *prefijo = entries[i].es_local ? "* " : "  ";
        printf("%-2s%-2d %-20s %-7d %-7d %-7d %-10.1f\n",
               prefijo,
               i + 1,
               entries[i].nombre,
               entries[i].goles,
               entries[i].asistencias,
               entries[i].partidos,
               entries[i].mejor_rendimiento);
    }

    printf("\n* Tu estadistica personal (usuario local)\n");

    free(entries);
    pause_console();
}

/**
 * @brief Compara las estadisticas del usuario local con las de un amigo
 *
 * Solicita el ID del amigo, carga sus estadisticas y las del usuario
 * local, y muestra una comparacion lado a lado: goles, asistencias,
 * partidos, mejor_rendimiento y rendimiento_promedio.
 */
void comparar_con_amigo(void)
{
    clear_screen();
    print_header("COMPARAR CON AMIGO");

    if (!hay_registros("amigo"))
    {
        mostrar_no_hay_registros("amigos para comparar");
        pause_console();
        return;
    }

    listar_amigos_simple();

    int id = input_int("\nID del amigo a comparar (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("amigo", id))
    {
        mostrar_no_existe("Amigo");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "SELECT nombre, IFNULL(posicion, ''), "
                         "IFNULL(goles_total, 0), "
                         "IFNULL(asistencias_total, 0), "
                         "IFNULL(partidos_total, 0), "
                         "IFNULL(mejor_rendimiento, 0) "
                         "FROM amigo WHERE id = ?"))
    {
        printf("Error al consultar el amigo.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        mostrar_no_existe("Amigo");
        pause_console();
        return;
    }

    char ami_nombre[100];
    char ami_posicion[50];
    int ami_goles = sqlite3_column_int(stmt, 2);
    int ami_asistencias = sqlite3_column_int(stmt, 3);
    int ami_partidos = sqlite3_column_int(stmt, 4);
    double ami_mejor_rend = sqlite3_column_double(stmt, 5);

    snprintf(ami_nombre, sizeof(ami_nombre), "%s",
             (const char *)sqlite3_column_text(stmt, 0));
    snprintf(ami_posicion, sizeof(ami_posicion), "%s",
             (const char *)sqlite3_column_text(stmt, 1));
    sqlite3_finalize(stmt);

    int local_partidos = 0;
    int local_goles = 0;
    int local_asistencias = 0;
    double local_rendimiento = 0.0;
    cargar_stats_local(&local_partidos, &local_goles, &local_asistencias,
                       &local_rendimiento);

    double ami_rend_prom = 0.0;
    if (ami_partidos > 0)
    {
        ami_rend_prom = (double)ami_goles / ami_partidos;
    }

    double local_rend_prom = 0.0;
    if (local_partidos > 0)
    {
        local_rend_prom = (double)local_goles / local_partidos;
    }

    printf("\n%-22s %-15s %-15s\n", "Estadistica", "Yo", ami_nombre);
    printf("------------------------------------------------------\n");
    printf("%-22s %-15d %-15d\n", "Goles totales", local_goles, ami_goles);
    printf("%-22s %-15d %-15d\n", "Asistencias totales", local_asistencias,
           ami_asistencias);
    printf("%-22s %-15d %-15d\n", "Partidos jugados", local_partidos,
           ami_partidos);
    printf("%-22s %-15.1f %-15.1f\n", "Mejor rendimiento", local_rendimiento,
           ami_mejor_rend);
    printf("%-22s %-15.1f %-15.1f\n", "Rendimiento promedio", local_rend_prom,
           ami_rend_prom);

    printf("\nPosicion del amigo: %s\n",
           ami_posicion[0] != '\0' ? ami_posicion : "No registrada");

    printf("\nDiferencia de goles: %+d\n", local_goles - ami_goles);
    printf("Diferencia de asistencias: %+d\n", local_asistencias - ami_asistencias);

    pause_console();
}

/**
 * @brief Muestra el menu de ranking de amigos
 *
 * Ofrece opciones: Agregar amigo, Listar, Modificar, Eliminar,
 * Ver ranking, Comparar conmigo y Volver.
 */
void menu_ranking_amigos(void)
{
    MenuItem items[] =
    {
        {1, "Agregar amigo", &agregar_amigo},
        {2, "Listar amigos", &listar_amigos},
        {3, "Modificar amigo", &modificar_amigo},
        {4, "Eliminar amigo", &eliminar_amigo},
        {5, "Ver ranking", &mostrar_ranking},
        {6, "Comparar conmigo", &comparar_con_amigo},
        {0, "Volver", NULL}
    };
    ejecutar_menu("RANKING DE AMIGOS", items, 7);
}
