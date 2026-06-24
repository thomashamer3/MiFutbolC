/**
 * @file db_integridad.c
 * @brief Implementacion de verificacion y reparacion de integridad de BD
 */

#include "db_integridad.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

/*
 * Callbacks para sqlite3_exec
 */

static int callback_integridad(void *data, int argc, char **argv, char **col_name)
{
    (void)col_name;
    int *errores = (int *)data;
    for (int i = 0; i < argc; i++)
    {
        if (!argv[i])
        {
            continue;
        }

        if (strcmp(argv[i], "ok") == 0)
        {
            ui_printf("  [OK] Base de datos integra\n");
        }
        else
        {
            ui_printf("  [ERROR] %s\n", argv[i]);
            if (errores)
            {
                (*errores)++;
            }
        }
    }
    return 0;
}

static int callback_info(void *data, int argc, char **argv, char **col_name)
{
    (void)data;
    for (int i = 0; i < argc; i++)
    {
        if (col_name[i] && argv[i])
        {
            ui_printf("  %s: %s\n", col_name[i], argv[i]);
        }
    }
    return 0;
}

/*
 * Ejecuta un PRAGMA con callback y muestra el resultado
 */
static int ejecutar_pragma(const char *pragma, const char *titulo,
                           int (*callback)(void *, int, char **, char **), void *data)
{
    char *errmsg = NULL;

    if (titulo)
    {
        mostrar_pantalla(titulo);
        ui_printf("\n");
    }

    ui_printf("  Ejecutando: %s\n\n", pragma);

    int flag = sqlite3_exec(db, pragma, callback, data, &errmsg);
    if (flag != SQLITE_OK)
    {
        ui_printf("  Error: %s\n", errmsg ? errmsg : "desconocido");
        app_log_event("INTEGRIDAD", errmsg ? errmsg : "Error al ejecutar PRAGMA");
        sqlite3_free(errmsg);
        return 0;
    }

    return 1;
}

void verificar_integridad_bd(void)
{
    int errores = 0;
    app_log_event("INTEGRIDAD", "Iniciando integrity_check");

    ejecutar_pragma("PRAGMA integrity_check;", "VERIFICACION DE INTEGRIDAD", callback_integridad,
                    &errores);

    if (errores > 0)
    {
        ui_printf("\n  SE ENCONTRARON %d ERROR(ES) DE INTEGRIDAD.\n", errores);
        ui_printf("  Se recomienda ejecutar 'Reparar base de datos'.\n");
    }
    else
    {
        ui_printf("\n  No se encontraron errores de integridad.\n");
    }

    app_log_event("INTEGRIDAD", errores > 0 ? "Integrity check FAILED" : "Integrity check OK");
    ui_printf("\n");
    pause_console();
}

void verificar_integridad_rapida(void)
{
    int errores = 0;
    app_log_event("INTEGRIDAD", "Iniciando quick_check");

    ejecutar_pragma("PRAGMA quick_check;", "VERIFICACION RAPIDA DE INTEGRIDAD", callback_integridad,
                    &errores);

    if (errores > 0)
    {
        ui_printf("\n  SE ENCONTRARON %d ERROR(ES) EN LA VERIFICACION RAPIDA.\n", errores);
        ui_printf("  Se recomienda ejecutar 'Verificar integridad' completa.\n");
    }
    else
    {
        ui_printf("\n  Verificacion rapida completada sin errores.\n");
    }

    app_log_event("INTEGRIDAD", errores > 0 ? "Quick check FAILED" : "Quick check OK");
    ui_printf("\n");
    pause_console();
}

void reconstruir_bd(void)
{
    mostrar_pantalla("RECONSTRUIR BASE DE DATOS");
    ui_printf("\n");
    ui_printf("  Esta operacion reconstruye completamente el archivo de base de\n");
    ui_printf("  datos para recuperar espacio no utilizado. Puede ser lenta en\n");
    ui_printf("  bases de datos grandes.\n\n");

    if (confirmar("  Desea realizar un backup antes de continuar"))
    {
        ui_printf("  Creando backup...\n");
        if (backup_base_datos_automatico("antes de VACUUM"))
        {
            ui_printf("  Backup creado exitosamente.\n");
        }
        else
        {
            ui_printf("  No se pudo crear el backup.\n");
        }
        ui_printf("\n");
    }

    if (!confirmar("  Confirmar VACUUM"))
    {
        ui_printf("  Operacion cancelada.\n\n");
        app_log_event("INTEGRIDAD", "VACUUM cancelado por el usuario");
        pause_console();
        return;
    }

    ui_printf("\n  Reconstruyendo base de datos... (puede tomar varios segundos)\n");
    ui_printf("  Por favor espere...\n\n");

    char *errmsg = NULL;
    int flag = sqlite3_exec(db, "VACUUM;", NULL, NULL, &errmsg);

    if (flag == SQLITE_OK)
    {
        ui_printf("  [OK] Base de datos reconstruida exitosamente.\n");
        app_log_event("INTEGRIDAD", "VACUUM completado exitosamente");
    }
    else
    {
        ui_printf("  [ERROR] VACUUM fallo: %s\n", errmsg ? errmsg : "desconocido");
        app_log_event("INTEGRIDAD", errmsg ? errmsg : "VACUUM fallo");
        sqlite3_free(errmsg);
    }

    ui_printf("\n");
    pause_console();
}

void analizar_bd(void)
{
    mostrar_pantalla("ANALIZAR BASE DE DATOS");
    ui_printf("\n");
    ui_printf("  Ejecutando ANALYZE para actualizar estadisticas del optimizador...\n\n");

    char *errmsg = NULL;
    int flag = sqlite3_exec(db, "ANALYZE;", NULL, NULL, &errmsg);

    if (flag == SQLITE_OK)
    {
        ui_printf("  [OK] Tablas analizadas exitosamente.\n");
        app_log_event("INTEGRIDAD", "ANALYZE completado");
    }
    else
    {
        ui_printf("  [ERROR] ANALYZE fallo: %s\n", errmsg ? errmsg : "desconocido");
        app_log_event("INTEGRIDAD", errmsg ? errmsg : "ANALYZE fallo");
        sqlite3_free(errmsg);
    }

    ui_printf("\n");
    pause_console();
}

void reparar_bd(void)
{
    mostrar_pantalla("REPARAR BASE DE DATOS");
    ui_printf("\n");
    ui_printf("  PASO 1: Verificando integridad...\n\n");

    int errores = 0;
    char *errmsg = NULL;
    int flag = sqlite3_exec(db, "PRAGMA integrity_check;", callback_integridad, &errores, &errmsg);

    if (flag != SQLITE_OK)
    {
        ui_printf("  Error al verificar integridad: %s\n", errmsg ? errmsg : "desconocido");
        sqlite3_free(errmsg);
        pause_console();
        return;
    }

    if (errores == 0)
    {
        ui_printf("\n  No se encontraron errores de integridad.\n");
        app_log_event("INTEGRIDAD", "Reparar: sin errores");
        pause_console();
        return;
    }

    ui_printf("\n  PASO 2: Reconstruyendo indices (REINDEX)...\n\n");
    app_log_event("INTEGRIDAD", "Iniciando REINDEX por errores detectados");

    flag = sqlite3_exec(db, "REINDEX;", NULL, NULL, &errmsg);
    if (flag == SQLITE_OK)
    {
        ui_printf("  [OK] Indices reconstruidos.\n");
        app_log_event("INTEGRIDAD", "REINDEX completado");
    }
    else
    {
        ui_printf("  [ERROR] REINDEX fallo: %s\n", errmsg ? errmsg : "desconocido");
        app_log_event("INTEGRIDAD", errmsg ? errmsg : "REINDEX fallo");
        sqlite3_free(errmsg);
        errmsg = NULL;
    }

    ui_printf("\n  PASO 3: Verificando integridad despues de REINDEX...\n\n");

    errores = 0;
    flag = sqlite3_exec(db, "PRAGMA integrity_check;", callback_integridad, &errores, &errmsg);
    if (flag != SQLITE_OK)
    {
        ui_printf("  Error al verificar integridad: %s\n", errmsg ? errmsg : "desconocido");
        sqlite3_free(errmsg);
        pause_console();
        return;
    }

    if (errores == 0)
    {
        ui_printf("\n  REINDEX soluciono los errores de integridad.\n");
        app_log_event("INTEGRIDAD", "Reparar completado con REINDEX");
    }
    else
    {
        ui_printf("\n  Aun hay %d error(es). Se recomienda restaurar desde un backup.\n", errores);
        app_log_event("INTEGRIDAD", "Reparar: errores persistentes, recomendar backup");
    }

    pause_console();
}

void mostrar_info_bd(void)
{
    mostrar_pantalla("INFORMACION DE LA BASE DE DATOS");
    ui_printf("\n");

    const char *pragmas[] = {"PRAGMA page_count;", "PRAGMA page_size;", "PRAGMA freelist_count;",
                             "PRAGMA schema_version;", "PRAGMA user_version;"
                            };
    int num_pragmas = sizeof(pragmas) / sizeof(pragmas[0]);

    for (int i = 0; i < num_pragmas; i++)
    {
        char *errmsg = NULL;
        int flag = sqlite3_exec(db, pragmas[i], callback_info, NULL, &errmsg);
        if (flag != SQLITE_OK)
        {
            ui_printf("  PRAGMA %d - Error: %s\n", i + 1, errmsg ? errmsg : "desconocido");
            sqlite3_free(errmsg);
        }
    }

    ui_printf("\n");
    app_log_event("INTEGRIDAD", "Informacion de BD mostrada");
    pause_console();
}

void menu_integridad_bd(void)
{
    MenuItem items[] = {{1, "Verificar integridad", &verificar_integridad_bd},
        {2, "Verificacion rapida", &verificar_integridad_rapida},
        {3, "Reconstruir base de datos (VACUUM)", &reconstruir_bd},
        {4, "Analizar tablas (ANALYZE)", &analizar_bd},
        {5, "Reparar base de datos", &reparar_bd},
        {6, "Mostrar informacion de la BD", &mostrar_info_bd},
        {0, "Volver", NULL}
    };
    ejecutar_menu("INTEGRIDAD DE BASE DE DATOS", items, 7);
}
