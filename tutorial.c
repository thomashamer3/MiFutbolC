/**
 * @file tutorial.c
 * @brief Modulo de tutorial/onboarding para nuevos usuarios
 *
 * Implementa una guia interactiva paso a paso que recorre las principales
 * funcionalidades de MiFutbolC. Utiliza la tabla settings con una columna
 * adicional 'tutorial_completed' para persistir el estado de completado.
 */

#include "tutorial.h"
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
 * @brief Cantidad total de pasos del tutorial
 */
#define TUTORIAL_TOTAL_PASOS 8

/**
 * @brief Asegura que la columna tutorial_completed exista en settings
 */
static void tutorial_ensure_schema(void)
{
    char *err = NULL;
    sqlite3_exec(db,
                 "ALTER TABLE settings ADD COLUMN tutorial_completed INTEGER DEFAULT 0;",
                 NULL, NULL, &err);
    if (err)
    {
        sqlite3_free(err);
    }
}

/**
 * @brief Muestra el contenido textual de un paso del tutorial
 *
 * Imprime el titulo y descripcion del paso indicado, luego pausa
 * para que el usuario lea antes de continuar.
 *
 * @param paso Numero del paso (1-based)
 */
static void mostrar_contenido_paso(int paso)
{
    clear_screen();

    switch (paso)
    {
    case 1:
        print_header("Paso 1/8: Bienvenido a MiFutbolC");
        ui_printf("\n");
        ui_printf("MiFutbolC es tu historial personal de futbol.\n");
        ui_printf("\n");
        ui_printf("Registra partidos, estadisticas, lesiones y mucho mas.\n");
        ui_printf("Todo queda guardado para que puedas analizar tu\n");
        ui_printf("progreso a lo largo del tiempo.\n");
        ui_printf("\n");
        ui_printf("Este tutorial te guiara por las funciones principales.\n");
        break;

    case 2:
        print_header("Paso 2/8: Dashboard");
        ui_printf("\n");
        ui_printf("El Dashboard es lo primero que ves al iniciar la app.\n");
        ui_printf("\n");
        ui_printf("Muestra un resumen rapido de tu actividad reciente:\n");
        ui_printf("  - Partidos jugados este mes\n");
        ui_printf("  - Goles y asistencias acumulados\n");
        ui_printf("  - Racha de victorias actual\n");
        ui_printf("  - Lesiones activas\n");
        ui_printf("\n");
        ui_printf("Puedes habilitar o deshabilitar el Dashboard desde\n");
        ui_printf("Ajustes > Configuracion.\n");
        break;

    case 3:
        print_header("Paso 3/8: Partidos");
        ui_printf("\n");
        ui_printf("Desde el menu Partidos puedes registrar cada partido\n");
        ui_printf("que juegues con toda la informacion relevante:\n");
        ui_printf("\n");
        ui_printf("  - Fecha, rival, resultado (G/E/P)\n");
        ui_printf("  - Goles, asistencias y minutos jugados\n");
        ui_printf("  - Calificacion de rendimiento\n");
        ui_printf("  - Cancha, clima y formacion utilizada\n");
        ui_printf("\n");
        ui_printf("Cada partido registrado alimenta tus estadisticas.\n");
        break;

    case 4:
        print_header("Paso 4/8: Estadisticas");
        ui_printf("\n");
        ui_printf("El modulo de Estadisticas analiza tu desempeno\n");
        ui_printf("automaticamente a partir de los partidos registrados.\n");
        ui_printf("\n");
        ui_printf("Puedes consultar:\n");
        ui_printf("  - Estadisticas generales (goles, asistencias, promedios)\n");
        ui_printf("  - Estadisticas por mes o por anio\n");
        ui_printf("  - Estadisticas por lesiones\n");
        ui_printf("  - Metas personales y progresion\n");
        ui_printf("\n");
        ui_printf("Mientras mas partidos registres, mas precisas seran\n");
        ui_printf("las graficas y comparativas.\n");
        break;

    case 5:
        print_header("Paso 5/8: Bienestar");
        ui_printf("\n");
        ui_printf("El modulo de Bienestar te permite llevar un registro\n");
        ui_printf("integral de tu condicion fisica y mental:\n");
        ui_printf("\n");
        ui_printf("  - Estado de animo y cansancio\n");
        ui_printf("  - Dolor fisico y lesiones menores\n");
        ui_printf("  - Horas de sueno\n");
        ui_printf("  - Estado general de salud\n");
        ui_printf("\n");
        ui_printf("Registrar estos datos te ayuda a detectar patrones\n");
        ui_printf("y optimizar tu rendimiento.\n");
        break;

    case 6:
        print_header("Paso 6/8: Carrera");
        ui_printf("\n");
        ui_printf("La Carrera es una linea de tiempo de tu trayectoria\n");
        ui_printf("en el futbol:\n");
        ui_printf("\n");
        ui_printf("  - Equipos por los que has pasado\n");
        ui_printf("  - Torneos y temporadas disputadas\n");
        ui_printf("  - Logros y reconocimientos\n");
        ui_printf("  - Estadisticas acumuladas por temporada\n");
        ui_printf("\n");
        ui_printf("Construye tu historial completo paso a paso.\n");
        break;

    case 7:
        print_header("Paso 7/8: Exportar");
        ui_printf("\n");
        ui_printf("MiFutbolC te permite exportar todos tus datos en\n");
        ui_printf("diferentes formatos:\n");
        ui_printf("\n");
        ui_printf("  - CSV  (para abrir en Excel o Google Sheets)\n");
        ui_printf("  - JSON (para integrar con otras aplicaciones)\n");
        ui_printf("  - HTML (para visualizar en un navegador)\n");
        ui_printf("  - TXT  (reporte de texto plano)\n");
        ui_printf("  - PDF  (documento formal para imprimir)\n");
        ui_printf("\n");
        ui_printf("Tambien puedes exportar la base de datos completa\n");
        ui_printf("para hacer copias de seguridad.\n");
        break;

    case 8:
        print_header("Paso 8/8: Atajos de Teclado");
        ui_printf("\n");
        ui_printf("MiFutbolC incluye atajos de teclado para\n");
        ui_printf("agilizar tu navegacion:\n");
        ui_printf("\n");
        ui_printf("  Ctrl+D  - Abrir Dashboard rapidamente\n");
        ui_printf("  Ctrl+B  - Busqueda global\n");
        ui_printf("  Ctrl+E  - Exportar datos\n");
        ui_printf("  Ctrl+S  - Guardar configuracion\n");
        ui_printf("  Ctrl+Z  - Deshacer ultima accion\n");
        ui_printf("\n");
        ui_printf("Puedes personalizar los atajos desde\n");
        ui_printf("Ajustes > Atajos de Teclado.\n");
        break;

    default:
        ui_printf("Paso no valido.\n");
        return;
    }

    ui_printf("\n");
    pause_console();
}

void iniciar_tutorial(void)
{
    app_log_event("TUTORIAL", "Iniciando tutorial completo");

    for (int i = 1; i <= TUTORIAL_TOTAL_PASOS; i++)
    {
        mostrar_contenido_paso(i);
    }

    tutorial_ensure_schema();

    sqlite3_stmt *stmt;
    if (db_prepare_stmt(&stmt, "UPDATE settings SET tutorial_completed = 1 WHERE id = 1;"))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    clear_screen();
    print_header("Tutorial Completado");
    ui_printf("\n");
    ui_printf("Felicidades, has completado el tutorial de MiFutbolC.\n");
    ui_printf("\n");
    ui_printf("Ya estas listo para empezar a registrar tus partidos\n");
    ui_printf("y construir tu historial futbolistico.\n");
    ui_printf("\n");
    ui_printf("Si necesitas revisar algo, puedes acceder al tutorial\n");
    ui_printf("desde el menu de Ajustes en cualquier momento.\n");
    ui_printf("\n");
    pause_console();

    app_log_event("TUTORIAL", "Tutorial completado exitosamente");
}

void mostrar_paso_tutorial(int paso)
{
    if (paso < 1 || paso > TUTORIAL_TOTAL_PASOS)
    {
        clear_screen();
        print_header("Error");
        ui_printf("Paso no valido. Debe ser un numero entre 1 y %d.\n",
                  TUTORIAL_TOTAL_PASOS);
        pause_console();
        return;
    }

    mostrar_contenido_paso(paso);
}

int tutorial_completado(void)
{
    tutorial_ensure_schema();

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "SELECT tutorial_completed FROM settings WHERE id = 1;"))
    {
        return 0;
    }

    int completed = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        completed = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return completed;
}

void reiniciar_tutorial(void)
{
    app_log_event("TUTORIAL", "Reiniciando tutorial");

    clear_screen();
    print_header("Reiniciar Tutorial");

    if (!confirmar("Deseas reiniciar el tutorial? (S/N): "))
    {
        ui_printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    tutorial_ensure_schema();

    sqlite3_stmt *stmt;
    if (db_prepare_stmt(&stmt, "UPDATE settings SET tutorial_completed = 0 WHERE id = 1;"))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    ui_printf("Tutorial reiniciado correctamente.\n");
    ui_printf("Podras volver a recorrerlo cuando lo desees.\n");
    pause_console();

    app_log_event("TUTORIAL", "Tutorial reiniciado por el usuario");
}

static void accion_ver_paso_especifico(void)
{
    clear_screen();
    print_header("Ver Paso del Tutorial");
    ui_printf("Ingresa el numero de paso (1-%d): ", TUTORIAL_TOTAL_PASOS);
    int paso = input_int("> ");
    mostrar_paso_tutorial(paso);
}

static void accion_iniciar_tutorial(void)
{
    iniciar_tutorial();
}

static void accion_reiniciar_tutorial(void)
{
    reiniciar_tutorial();
}

void menu_tutorial(void)
{
    MenuItem items[] =
    {
        {1, "Iniciar tutorial", &accion_iniciar_tutorial},
        {2, "Ver paso especifico", &accion_ver_paso_especifico},
        {3, "Reiniciar tutorial", &accion_reiniciar_tutorial},
        {0, "Volver", NULL}
    };

    ejecutar_menu("Tutorial", items, (int)(sizeof(items) / sizeof(items[0])));
}
