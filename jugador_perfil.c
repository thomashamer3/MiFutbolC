#include "jugador_perfil.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "ascii_charts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static void mostrar_datos_personales(int jugador_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT nombre, edad, posicion, fecha_nacimiento, nacionalidad FROM jugador WHERE id = ?", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("\n=== DATOS PERSONALES ===\n");
        printf("  Nombre: %s\n", sqlite3_column_text(stmt, 0));
        printf("  Edad: %d\n", sqlite3_column_int(stmt, 1));
        printf("  Posicion: %s\n", sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "N/A");
        printf("  Fecha Nac: %s\n", sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "N/A");
        printf("  Nacionalidad: %s\n", sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "N/A");
    }
    else
    {
        printf("\n  Jugador no encontrado (ID: %d).\n", jugador_id);
    }
    sqlite3_finalize(stmt);
}

static void mostrar_estadisticas_historicas(int jugador_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT COUNT(*), COALESCE(SUM(goles),0), COALESCE(SUM(asistencias),0), "
                       "COALESCE(AVG(rendimiento_general),0) "
                       "FROM partido WHERE equipo_id = ? AND resultado > 0", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    printf("\n=== ESTADISTICAS HISTORICAS ===\n");
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("  Partidos: %d\n", sqlite3_column_int(stmt, 0));
        printf("  Goles: %d\n", sqlite3_column_int(stmt, 1));
        printf("  Asistencias: %d\n", sqlite3_column_int(stmt, 2));
        printf("  Rendimiento Prom: %.1f\n", sqlite3_column_double(stmt, 3));
    }
    sqlite3_finalize(stmt);
}

static void mostrar_lesiones_recientes(int jugador_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT descripcion, fecha, gravedad FROM lesion WHERE jugador_id = ? ORDER BY fecha DESC LIMIT 5", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    printf("\n=== LESIONES RECIENTES ===\n");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %s - %s (Gravedad: %d)\n",
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 0),
               sqlite3_column_int(stmt, 2));
    }
    sqlite3_finalize(stmt);
    if (count == 0) printf("  Sin lesiones registradas.\n");
}

static void mostrar_partidos_recientes(int jugador_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT p.fecha_hora, p.goles, p.asistencias, p.resultado, p.rendimiento_general "
                       "FROM partido p WHERE p.equipo_id = ? AND p.resultado > 0 "
                       "ORDER BY p.fecha_hora DESC LIMIT 5", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    printf("\n=== PARTIDOS RECIENTES ===\n");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %s | G:%d A:%d | %s | Rend: %d\n",
               sqlite3_column_text(stmt, 0),
               sqlite3_column_int(stmt, 1),
               sqlite3_column_int(stmt, 2),
               resultado_to_text(sqlite3_column_int(stmt, 3)),
               sqlite3_column_int(stmt, 4));
    }
    sqlite3_finalize(stmt);
    if (count == 0) printf("  Sin partidos registrados.\n");
}

static void mostrar_progresion_atributos(int jugador_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT semana, ataque, defensa, resistencia, velocidad, tecnica "
                       "FROM progresion_jugador WHERE jugador_id = ? ORDER BY semana", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    printf("\n=== PROGRESION DE ATRIBUTOS ===\n");
    int count = 0;
    double prom_ataque = 0;
    double prom_defensa = 0;
    double prom_resistencia = 0;
    double prom_velocidad = 0;
    double prom_tecnica = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < 100)
    {
        prom_ataque += sqlite3_column_int(stmt, 1);
        prom_defensa += sqlite3_column_int(stmt, 2);
        prom_resistencia += sqlite3_column_int(stmt, 3);
        prom_velocidad += sqlite3_column_int(stmt, 4);
        prom_tecnica += sqlite3_column_int(stmt, 5);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
    {
        printf("  Sin datos de progresion.\n");
        return;
    }

    double promedios[5] =
    {
        prom_ataque / count, prom_defensa / count,
        prom_resistencia / count, prom_velocidad / count, prom_tecnica / count
    };
    const char *etiquetas[5] = {"Ataque", "Defensa", "Resist.", "Veloc.", "Tecnica"};
    dibujar_grafico_barras(promedios, etiquetas, 5, "Progresion Promedio", 40);
}

static void mostrar_mejor_rendimiento(int jugador_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT fecha_hora, goles, asistencias, rendimiento_general, resultado "
                       "FROM partido WHERE equipo_id = ? AND resultado > 0 "
                       "ORDER BY rendimiento_general DESC LIMIT 1", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    printf("\n=== MEJOR RENDIMIENTO ===\n");
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("  Fecha: %s\n", sqlite3_column_text(stmt, 0));
        printf("  Goles: %d | Asistencias: %d\n", sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2));
        printf("  Rendimiento: %d\n", sqlite3_column_int(stmt, 3));
        printf("  Resultado: %s\n", resultado_to_text(sqlite3_column_int(stmt, 4)));
    }
    else
    {
        printf("  Sin datos de rendimiento.\n");
    }
    sqlite3_finalize(stmt);
}

static void mostrar_grafico_radar(int jugador_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT AVG(ataque), AVG(defensa), AVG(resistencia), AVG(velocidad), AVG(tecnica) "
                       "FROM progresion_jugador WHERE jugador_id = ?", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, jugador_id);

    printf("\n=== GRAFICO RADAR DE ATRIBUTOS ===\n");
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        double vals[5] =
        {
            sqlite3_column_double(stmt, 0),
            sqlite3_column_double(stmt, 1),
            sqlite3_column_double(stmt, 2),
            sqlite3_column_double(stmt, 3),
            sqlite3_column_double(stmt, 4)
        };
        const char *etiquetas[5] = {"Ataque", "Defensa", "Resist.", "Veloc.", "Tecnica"};

        int all_zero = 1;
        for (int i = 0; i < 5; i++)
            if (vals[i] > 0)
            {
                all_zero = 0;
                break;
            }

        if (all_zero)
        {
            printf("  Sin datos de atributos.\n");
        }
        else
        {
            for (int i = 0; i < 5; i++)
            {
                int barra = (int)(vals[i] / 5.0);
                if (barra > 40) barra = 40;
                printf("  %-10s |", etiquetas[i]);
                for (int j = 0; j < barra; j++) printf("#");
                printf(" %.0f\n", vals[i]);
            }
        }
    }
    sqlite3_finalize(stmt);
}

static void listar_jugadores(void)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, "SELECT id, nombre, posicion, equipo_id FROM jugador ORDER BY id", -1, &stmt, NULL) != SQLITE_OK)
        return;

    printf("\nJugadores disponibles:\n");
    printf("----------------------------------------\n");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d - %s (Pos: %d, Equipo ID: %d)\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3));
    }
    sqlite3_finalize(stmt);
    if (!count) printf("  No hay jugadores registrados.\n");
    printf("\n");
}

void menu_perfil_jugador(void)
{
    if (!hay_registros("jugador"))
    {
        mostrar_no_hay_registros("jugadores");
        pause_console();
        return;
    }

    listar_jugadores();
    int jugador_id = input_int("ID del jugador (0 para cancelar): ");
    if (jugador_id <= 0)
    {
        printf("ID invalido.\n");
        return;
    }

    if (!existe_id("jugador", jugador_id))
    {
        mostrar_no_existe("jugador");
        pause_console();
        return;
    }

    mostrar_pantalla("PERFIL DEL JUGADOR");
    mostrar_datos_personales(jugador_id);
    mostrar_estadisticas_historicas(jugador_id);
    mostrar_lesiones_recientes(jugador_id);
    mostrar_partidos_recientes(jugador_id);
    mostrar_progresion_atributos(jugador_id);
    mostrar_mejor_rendimiento(jugador_id);
    mostrar_grafico_radar(jugador_id);
    printf("\n");
    pause_console();
}
