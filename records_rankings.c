/**
 * @file records_rankings.c
 * @brief Implementación de récords y rankings en MiFutbolC
 */

#include "records_rankings.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * @brief Función auxiliar para ejecutar consultas SQL y mostrar resultados de récords
 */
static void mostrar_record(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
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
            printf("No hay datos disponibles.\n");
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Función auxiliar para mostrar combinaciones cancha + camiseta
 */
static void mostrar_combinacion(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("Cancha: %s\n", sqlite3_column_text(stmt, 0));
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
            printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 2));
            printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 3));
        }
        else
        {
            printf("No hay datos disponibles.\n");
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Función auxiliar para mostrar temporadas
 */
static void mostrar_temporada(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char* year = (const char*)sqlite3_column_text(stmt, 0);
            if (year)
            {
                printf("Anio: %s\n", year);
            }
            else
            {
                printf("Anio: Desconocido\n");
            }
            printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 1));
            printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 2));
        }
        else
        {
            printf("No hay datos disponibles.\n");
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Muestra el récord de goles en un partido
 */
void mostrar_record_goles_partido()
{
    clear_screen();
    print_header("RECORD DE GOLES EN UN PARTIDO");

    mostrar_record("Record de Goles en un Partido",
                   "SELECT p.goles, c.nombre, p.fecha_hora "
                   "FROM partido p "
                   "JOIN camiseta c ON p.camiseta_id = c.id "
                   "ORDER BY p.goles DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra el récord de asistencias en un partido
 */
void mostrar_record_asistencias_partido()
{
    clear_screen();
    print_header("RECORD DE ASISTENCIAS EN UN PARTIDO");

    mostrar_record("Record de Asistencias en un Partido",
                   "SELECT p.asistencias, c.nombre, p.fecha_hora "
                   "FROM partido p "
                   "JOIN camiseta c ON p.camiseta_id = c.id "
                   "ORDER BY p.asistencias DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la mejor combinación cancha + camiseta
 */
void mostrar_mejor_combinacion_cancha_camiseta()
{
    clear_screen();
    print_header("MEJOR COMBINACION CANCHA + CAMISETA");

    mostrar_combinacion("Mejor Combinacion Cancha + Camiseta",
                        "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                        "FROM partido p "
                        "JOIN cancha ca ON p.cancha_id = ca.id "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "GROUP BY p.cancha_id, p.camiseta_id "
                        "ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la peor combinación cancha + camiseta
 */
void mostrar_peor_combinacion_cancha_camiseta()
{
    clear_screen();
    print_header("PEOR COMBINACION CANCHA + CAMISETA");

    mostrar_combinacion("Peor Combinacion Cancha + Camiseta",
                        "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                        "FROM partido p "
                        "JOIN cancha ca ON p.cancha_id = ca.id "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "GROUP BY p.cancha_id, p.camiseta_id "
                        "ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la mejor temporada
 */
void mostrar_mejor_temporada()
{
    clear_screen();
    print_header("MEJOR TEMPORADA");

    mostrar_temporada("Mejor Temporada",
                      "SELECT substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL GROUP BY substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la peor temporada
 */
void mostrar_peor_temporada()
{
    clear_screen();
    print_header("PEOR TEMPORADA");

    mostrar_temporada("Peor Temporada",
                      "SELECT substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL GROUP BY substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra el partido con mejor rendimiento_general
 */
void mostrar_partido_mejor_rendimiento_general()
{
    clear_screen();
    print_header("PARTIDO CON MEJOR RENDIMIENTO GENERAL");

    sqlite3_stmt *stmt;

    printf("\nPartido con Mejor Rendimiento General\n");
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db,
                           "SELECT p.id, p.fecha_hora, c.nombre, p.rendimiento_general "
                           "FROM partido p "
                           "JOIN camiseta c ON p.camiseta_id = c.id "
                           "ORDER BY p.rendimiento_general DESC LIMIT 1",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d\n", sqlite3_column_int(stmt, 0));
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 1));
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 2));
            printf("Rendimiento General: %.2f\n", sqlite3_column_double(stmt, 3));
        }
        else
        {
            printf("No hay datos disponibles.\n");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra el partido con peor rendimiento_general
 */
void mostrar_partido_peor_rendimiento_general()
{
    clear_screen();
    print_header("PARTIDO CON PEOR RENDIMIENTO GENERAL");

    sqlite3_stmt *stmt;

    printf("\nPartido con Peor Rendimiento General\n");
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db,
                           "SELECT p.id, p.fecha_hora, c.nombre, p.rendimiento_general "
                           "FROM partido p "
                           "JOIN camiseta c ON p.camiseta_id = c.id "
                           "ORDER BY p.rendimiento_general ASC LIMIT 1",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d\n", sqlite3_column_int(stmt, 0));
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 1));
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 2));
            printf("Rendimiento General: %.2f\n", sqlite3_column_double(stmt, 3));
        }
        else
        {
            printf("No hay datos disponibles.\n");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra el partido con mejor combinación (goles + asistencias)
 */
void mostrar_partido_mejor_combinacion_goles_asistencias()
{
    clear_screen();
    print_header("PARTIDO CON MEJOR COMBINACION GOLES+ASISTENCIAS");

    sqlite3_stmt *stmt;

    printf("\nPartido con Mejor Combinacion Goles+Asistencias\n");
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db,
                           "SELECT p.id, p.fecha_hora, c.nombre, p.goles, p.asistencias, (p.goles + p.asistencias) AS combinacion "
                           "FROM partido p "
                           "JOIN camiseta c ON p.camiseta_id = c.id "
                           "ORDER BY combinacion DESC LIMIT 1",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d\n", sqlite3_column_int(stmt, 0));
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 1));
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 2));
            printf("Goles: %d\n", sqlite3_column_int(stmt, 3));
            printf("Asistencias: %d\n", sqlite3_column_int(stmt, 4));
            printf("Combinacion: %d\n", sqlite3_column_int(stmt, 5));
        }
        else
        {
            printf("No hay datos disponibles.\n");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra los partidos sin goles
 */
void mostrar_partidos_sin_goles()
{
    clear_screen();
    print_header("PARTIDOS SIN GOLES");

    sqlite3_stmt *stmt;

    printf("\nPartidos sin Goles\n");
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db,
                           "SELECT p.id, p.fecha_hora, c.nombre, p.goles, p.asistencias "
                           "FROM partido p "
                           "JOIN camiseta c ON p.camiseta_id = c.id "
                           "WHERE p.goles = 0 "
                           "ORDER BY p.fecha_hora DESC",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d | Fecha: %s | Camiseta: %s | Goles: %d | Asistencias: %d\n",
                   sqlite3_column_int(stmt, 0),
                   sqlite3_column_text(stmt, 1),
                   sqlite3_column_text(stmt, 2),
                   sqlite3_column_int(stmt, 3),
                   sqlite3_column_int(stmt, 4));
            count++;
        }

        if (count == 0)
        {
            printf("No hay partidos sin goles.\n");
        }
        else
        {
            printf("\nTotal: %d partidos\n", count);
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra los partidos sin asistencias
 */
void mostrar_partidos_sin_asistencias()
{
    clear_screen();
    print_header("PARTIDOS SIN ASISTENCIAS");

    sqlite3_stmt *stmt;

    printf("\nPartidos sin Asistencias\n");
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db,
                           "SELECT p.id, p.fecha_hora, c.nombre, p.goles, p.asistencias "
                           "FROM partido p "
                           "JOIN camiseta c ON p.camiseta_id = c.id "
                           "WHERE p.asistencias = 0 "
                           "ORDER BY p.fecha_hora DESC",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d | Fecha: %s | Camiseta: %s | Goles: %d | Asistencias: %d\n",
                   sqlite3_column_int(stmt, 0),
                   sqlite3_column_text(stmt, 1),
                   sqlite3_column_text(stmt, 2),
                   sqlite3_column_int(stmt, 3),
                   sqlite3_column_int(stmt, 4));
            count++;
        }

        if (count == 0)
        {
            printf("No hay partidos sin asistencias.\n");
        }
        else
        {
            printf("\nTotal: %d partidos\n", count);
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

/****
 * @brief Función auxiliar para calcular rachas
 */
static void calcular_racha(const char *titulo, int tipo_racha)
{
    sqlite3_stmt *stmt;
    int racha_actual = 0;
    int mejor_racha = 0;
    int inicio_racha = -1;
    int fin_racha = -1;
    int temp_inicio = -1;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_prepare_v2(db,
                           "SELECT id, goles FROM partido ORDER BY fecha_hora ASC",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int id = sqlite3_column_int(stmt, 0);
            int goles = sqlite3_column_int(stmt, 1);
            int condicion = (tipo_racha == 1) ? (goles > 0) : (goles == 0); // 1: scoring streak, 0: non-scoring streak

            if (condicion)
            {
                if (racha_actual == 0)
                {
                    temp_inicio = id;
                }
                racha_actual++;
                if (racha_actual > mejor_racha)
                {
                    mejor_racha = racha_actual;
                    inicio_racha = temp_inicio;
                    fin_racha = id;
                }
            }
            else
            {
                racha_actual = 0;
            }
        }
        sqlite3_finalize(stmt);
    }

    if (mejor_racha > 0)
    {
        printf("Mejor Racha: %d partidos\n", mejor_racha);
        printf("Desde partido ID %d hasta ID %d\n", inicio_racha, fin_racha);
    }
    else
    {
        printf("No hay rachas disponibles.\n");
    }
}

/**
 * @brief Muestra la mejor racha goleadora
 */
void mostrar_mejor_racha_goleadora()
{
    clear_screen();
    print_header("MEJOR RACHA GOLEADORA");

    calcular_racha("Mejor Racha Goleadora (partidos consecutivos con goles)",
                   1); // 1 para racha goleadora

    pause_console();
}

/**
 * @brief Muestra la peor racha
 */
void mostrar_peor_racha()
{
    clear_screen();
    print_header("PEOR RACHA");

    calcular_racha("Peor Racha (partidos consecutivos sin goles)",
                   0); // 0 para racha sin goles

    pause_console();
}

/**
 * @brief Muestra los partidos consecutivos anotando
 */
void mostrar_partidos_consecutivos_anotando()
{
    clear_screen();
    print_header("PARTIDOS CONSECUTIVOS ANOTANDO");

    calcular_racha("Partidos Consecutivos Anotando",
                   1); // Mismo que mejor racha goleadora

    pause_console();
}

/**
 * @brief Muestra el menú de récords y rankings
 */
void menu_records_rankings()
{
    MenuItem items[] =
    {
        {1, "Record de Goles en un Partido", mostrar_record_goles_partido},
        {2, "Record de Asistencias", mostrar_record_asistencias_partido},
        {3, "Mejor Combinacion Cancha + Camiseta", mostrar_mejor_combinacion_cancha_camiseta},
        {4, "Peor Combinacion Cancha + Camiseta", mostrar_peor_combinacion_cancha_camiseta},
        {5, "Mejor Temporada", mostrar_mejor_temporada},
        {6, "Peor Temporada", mostrar_peor_temporada},
        {7, "Partido con Mejor Rendimiento General", mostrar_partido_mejor_rendimiento_general},
        {8, "Partido con Peor Rendimiento General", mostrar_partido_peor_rendimiento_general},
        {9, "Partido con Mejor Combinacion Goles+Asistencias", mostrar_partido_mejor_combinacion_goles_asistencias},
        {10, "Partidos sin Goles", mostrar_partidos_sin_goles},
        {11, "Partidos sin Asistencias", mostrar_partidos_sin_asistencias},
        {12, "Mejor Racha Goleadora", mostrar_mejor_racha_goleadora},
        {13, "Peor Racha", mostrar_peor_racha},
        {14, "Partidos Consecutivos Anotando", mostrar_partidos_consecutivos_anotando},
        {0, "Volver", NULL}
    };

    ejecutar_menu("RECORDS & RANKINGS", items, 15);
}
