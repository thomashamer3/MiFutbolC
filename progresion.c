#include "progresion.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "ascii_charts.h"
#include <stdio.h>
#include <time.h>


static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return db_prepare_stmt(stmt, sql);
}

static void crear_plan_entrenamiento(void)
{
    char nombre[256];
    char desc[1024];
    int duracion_semanas;
    int sesiones_por_semana;

    input_string("Nombre del plan: ", nombre, sizeof(nombre));
    input_string("Descripcion: ", desc, sizeof(desc));
    duracion_semanas = input_int("Duracion (semanas): ");
    sesiones_por_semana = input_int("Sesiones por semana: ");

    sqlite3_stmt *stmt;
    if (!preparar_stmt("INSERT INTO entrenamiento_plan (nombre, descripcion, duracion_semanas, sesiones_por_semana) VALUES (?,?,?,?)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, duracion_semanas);
    sqlite3_bind_int(stmt, 4, sesiones_por_semana);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Plan de entrenamiento creado.\n");
    else
        printf("Error al crear plan: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
}

static void listar_planes_entrenamiento(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, nombre, descripcion, duracion_semanas, sesiones_por_semana FROM entrenamiento_plan ORDER BY id", &stmt))
    {
        mostrar_no_hay_registros("planes de entrenamiento");
        return;
    }

    mostrar_pantalla("PLANES DE ENTRENAMIENTO");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. %s\n", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1));
        printf("     %s\n", sqlite3_column_text(stmt, 2));
        printf("     Duracion: %d semanas | Sesiones/semana: %d\n",
               sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
        printf("     ------------------------------\n");
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        mostrar_no_hay_registros("planes de entrenamiento");
    pause_console();
}

static void eliminar_plan_entrenamiento(void)
{
    int id = input_int("ID del plan a eliminar: ");
    sqlite3_stmt *stmt;
    if (!preparar_stmt("DELETE FROM entrenamiento_plan WHERE id = ?", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Plan eliminado.\n");
    else
        printf("Error al eliminar plan: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
}

void menu_entrenamiento_plan(void)
{
    MenuItem items[] =
    {
        {1, "Crear Plan", &crear_plan_entrenamiento},
        {2, "Listar Planes", &listar_planes_entrenamiento},
        {3, "Eliminar Plan", &eliminar_plan_entrenamiento},
        {0, "Volver", NULL}
    };
    ejecutar_menu("PLANES DE ENTRENAMIENTO", items, 4);
}

static void registrar_progresion(void)
{
    int jugador_id;
    int plan_id;
    int semana;
    int ataque;
    int defensa;
    int resistencia;
    int velocidad;
    int tecnica;

    jugador_id = input_int("ID del jugador: ");
    plan_id = input_int("ID del plan: ");
    semana = input_int("Semana: ");
    ataque = input_int_rango("Progresion Ataque (0-100): ", 0, 100);
    defensa = input_int_rango("Progresion Defensa (0-100): ", 0, 100);
    resistencia = input_int_rango("Progresion Resistencia (0-100): ", 0, 100);
    velocidad = input_int_rango("Progresion Velocidad (0-100): ", 0, 100);
    tecnica = input_int_rango("Progresion Tecnica (0-100): ", 0, 100);

    sqlite3_stmt *stmt;
    if (!preparar_stmt("INSERT INTO progresion_jugador (jugador_id, plan_id, semana, ataque, defensa, resistencia, velocidad, tecnica) VALUES (?,?,?,?,?,?,?,?)", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);
    sqlite3_bind_int(stmt, 2, plan_id);
    sqlite3_bind_int(stmt, 3, semana);
    sqlite3_bind_int(stmt, 4, ataque);
    sqlite3_bind_int(stmt, 5, defensa);
    sqlite3_bind_int(stmt, 6, resistencia);
    sqlite3_bind_int(stmt, 7, velocidad);
    sqlite3_bind_int(stmt, 8, tecnica);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Progresion registrada.\n");
    else
        printf("Error al registrar progresion: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
}

static void ver_progresion_jugador(void)
{
    int jugador_id = input_int("ID del jugador: ");

    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT p.semana, p.ataque, p.defensa, p.resistencia, p.velocidad, p.tecnica, pl.nombre "
                       "FROM progresion_jugador p LEFT JOIN entrenamiento_plan pl ON p.plan_id = pl.id "
                       "WHERE p.jugador_id = ? ORDER BY p.semana", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    mostrar_pantalla("PROGRESION DEL JUGADOR");

    int count = 0;
    double valores_ataque[256];
    double valores_defensa[256];
    double valores_resistencia[256];
    double valores_velocidad[256];
    double valores_tecnica[256];
    int semanas_vals[256];

    while (sqlite3_step(stmt) == SQLITE_ROW && count < 256)
    {
        semanas_vals[count] = sqlite3_column_int(stmt, 0);
        valores_ataque[count] = (double)sqlite3_column_int(stmt, 1);
        valores_defensa[count] = (double)sqlite3_column_int(stmt, 2);
        valores_resistencia[count] = (double)sqlite3_column_int(stmt, 3);
        valores_velocidad[count] = (double)sqlite3_column_int(stmt, 4);
        valores_tecnica[count] = (double)sqlite3_column_int(stmt, 5);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
    {
        mostrar_no_hay_registros("progresion para este jugador");
        pause_console();
        return;
    }

    for (int i = 0; i < count; i++)
    {
        printf("Semana %d:\n", semanas_vals[i]);
        printf("  Ataque: %.0f  Defensa: %.0f  Resistencia: %.0f  Velocidad: %.0f  Tecnica: %.0f\n",
               valores_ataque[i], valores_defensa[i], valores_resistencia[i],
               valores_velocidad[i], valores_tecnica[i]);
    }

    {
        double promedios[5] = {0};
        const char *etiquetas[5] = {"Ataque", "Defensa", "Resist.", "Veloc.", "Tecnica"};
        for (int i = 0; i < count; i++)
        {
            promedios[0] += valores_ataque[i];
            promedios[1] += valores_defensa[i];
            promedios[2] += valores_resistencia[i];
            promedios[3] += valores_velocidad[i];
            promedios[4] += valores_tecnica[i];
        }
        for (int i = 0; i < 5; i++)
            promedios[i] = (count > 0) ? promedios[i] / count : 0;

        printf("\nPromedio por atributo:\n");
        dibujar_grafico_barras(promedios, etiquetas, 5, "Progresion Promedio", 40);
    }

    if (count >= 2)
    {
        printf("\n--- Evolucion Ataque ---\n");
        dibujar_grafico_lineas(valores_ataque, count, "Ataque", 40, 10);
    }

    pause_console();
}

static void listar_progresiones(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT p.id, j.nombre, pl.nombre, p.semana, p.ataque "
                       "FROM progresion_jugador p "
                       "LEFT JOIN jugador j ON p.jugador_id = j.id "
                       "LEFT JOIN entrenamiento_plan pl ON p.plan_id = pl.id "
                       "ORDER BY p.jugador_id, p.semana", &stmt))
    {
        mostrar_no_hay_registros("progresiones");
        return;
    }

    mostrar_pantalla("PROGRESIONES REGISTRADAS");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  ID %d | Jugador: %s | Plan: %s | Semana: %d | Ataque: %d\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "?",
               sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "?",
               sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
    }
    sqlite3_finalize(stmt);

    if (count == 0) mostrar_no_hay_registros("progresiones");
    pause_console();
}

void menu_progresion_jugador(void)
{
    MenuItem items[] =
    {
        {1, "Registrar Progresion", &registrar_progresion},
        {2, "Ver Progresion de Jugador", &ver_progresion_jugador},
        {3, "Listar Progresiones", &listar_progresiones},
        {0, "Volver", NULL}
    };
    ejecutar_menu("PROGRESION DE JUGADOR", items, 4);
}

void menu_progresion(void)
{
    MenuItem items[] =
    {
        {1, "Planes de Entrenamiento", &menu_entrenamiento_plan},
        {2, "Progresion de Jugador", &menu_progresion_jugador},
        {0, "Volver", NULL}
    };
    ejecutar_menu("ENTRENAMIENTO Y PROGRESION", items, 3);
}
