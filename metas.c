#include "metas.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define META_TIPOS 5

static const char *g_tipos[META_TIPOS] = {"Partidos jugados", "Goles totales", "Asistencias",
                                          "Victorias", "Rendimiento promedio"
                                         };

static const char *g_tipo_col[META_TIPOS] =
{
    "COUNT(*)", "COALESCE(SUM(goles),0)", "COALESCE(SUM(asistencias),0)",
    "COALESCE(SUM(CASE WHEN resultado=1 THEN 1 ELSE 0 END),0)",
    "COALESCE(AVG(rendimiento_general),0)"
};

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static const char *tipo_to_text(int tipo)
{
    if (tipo >= 1 && tipo <= META_TIPOS)
    {
        return g_tipos[tipo - 1];
    }
    return "Desconocido";
}

static double obtener_valor_actual_meta(int tipo)
{
    sqlite3_stmt *stmt = NULL;
    char sql[256];

    if (tipo < 1 || tipo > META_TIPOS)
    {
        return 0.0;
    }

    snprintf(sql, sizeof(sql), "SELECT %s FROM partido WHERE resultado > 0", g_tipo_col[tipo - 1]);

    if (!preparar_stmt(&stmt, sql))
    {
        return 0.0;
    }

    double valor = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (tipo == 5)
        {
            valor = sqlite3_column_double(stmt, 0);
        }
        else
        {
            valor = (double)sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return valor;
}

void metas_crear(void)
{
    clear_screen();
    print_header("NUEVA META");

    char nombre[256] = {0};
    input_string("Nombre de la meta: ", nombre, (int)sizeof(nombre));
    if (nombre[0] == '\0')
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    printf("\nTipo de meta:\n");
    for (int i = 0; i < META_TIPOS; i++)
    {
        double actual = obtener_valor_actual_meta(i + 1);
        printf("  %d - %s (actual: %.0f)\n", i + 1, g_tipos[i], actual);
    }

    int tipo = input_int_rango("Seleccione tipo", 1, META_TIPOS);

    double valor_actual = obtener_valor_actual_meta(tipo);
    printf("Valor actual: %.0f\n", valor_actual);

    double valor_objetivo = input_double("Valor objetivo: ");
    if (valor_objetivo <= valor_actual)
    {
        printf("El valor objetivo debe ser mayor al actual (%.0f).\n", valor_actual);
        pause_console();
        return;
    }

    char fecha_inicio[64] = {0};
    char fecha_fin[64] = {0};
    input_date("Fecha inicio (dd/mm/aaaa): ", fecha_inicio, (int)sizeof(fecha_inicio));
    input_date("Fecha fin (dd/mm/aaaa): ", fecha_fin, (int)sizeof(fecha_fin));

    long long id = obtener_siguiente_id("meta");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "INSERT INTO meta(id, nombre, tipo, valor_objetivo, "
                       "valor_inicial, fecha_inicio, fecha_fin, estado) "
                       "VALUES (?,?,?,?,?,?,?,'Activa')"))
    {
        mostrar_error_operacion("Meta", "crear");
        return;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 3, tipo);
    sqlite3_bind_double(stmt, 4, valor_objetivo);
    sqlite3_bind_double(stmt, 5, valor_actual);
    sqlite3_bind_text(stmt, 6, fecha_inicio[0] ? fecha_inicio : "", -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 7, fecha_fin[0] ? fecha_fin : "", -1, DB_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Meta", "Creada", nombre);
    }
    else
    {
        mostrar_error_operacion("Meta", "crear");
    }
    pause_console();
}

void metas_listar(void)
{
    if (!hay_registros("meta"))
    {
        mostrar_no_hay_registros("metas");
        pause_console();
        return;
    }

    clear_screen();
    print_header("METAS PERSONALES");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT id, nombre, tipo, valor_objetivo, valor_inicial, "
                       "fecha_inicio, fecha_fin, estado FROM meta ORDER BY estado, id"))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        int tipo = sqlite3_column_int(stmt, 2);
        double objetivo = sqlite3_column_double(stmt, 3);
        double inicial = sqlite3_column_double(stmt, 4);
        const char *f_ini = (const char *)sqlite3_column_text(stmt, 5);
        const char *f_fin = (const char *)sqlite3_column_text(stmt, 6);
        const char *estado = (const char *)sqlite3_column_text(stmt, 7);

        double actual = obtener_valor_actual_meta(tipo);
        double progreso = 0.0;
        if (objetivo > inicial)
        {
            progreso = ((actual - inicial) / (objetivo - inicial)) * 100.0;
            if (progreso > 100.0)
            {
                progreso = 100.0;
            }
            if (progreso < 0.0)
            {
                progreso = 0.0;
            }
        }

        printf("  %d. %s [%s]\n", id, nombre ? nombre : "", estado ? estado : "");
        printf("     Tipo: %s\n", tipo_to_text(tipo));
        printf("     Progreso: %.0f / %.0f (%.1f%%)\n", actual, objetivo, progreso);

        int barras = (int)(progreso / 5.0);
        if (barras > 20)
        {
            barras = 20;
        }
        printf("     [");
        for (int i = 0; i < barras; i++)
        {
            printf("#");
        }
        for (int i = barras; i < 20; i++)
        {
            printf(".");
        }
        printf("]\n");

        if (f_ini && f_ini[0])
        {
            printf("     Inicio: %s", f_ini);
        }
        if (f_fin && f_fin[0])
        {
            printf(" - Fin: %s", f_fin);
        }
        printf("\n\n");
    }
    sqlite3_finalize(stmt);
    pause_console();
}

void metas_editar(void)
{
    if (!hay_registros("meta"))
    {
        mostrar_no_hay_registros("metas");
        pause_console();
        return;
    }

    listar_entidades("meta", "EDITAR META", "No hay metas");

    int id = input_int("ID de la meta a editar (0=cancelar): ");
    if (id <= 0 || !existe_id("meta", id))
    {
        if (id > 0)
        {
            mostrar_no_existe("Meta");
        }

        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT nombre, tipo, valor_objetivo, "
                       "fecha_inicio, fecha_fin, estado FROM meta WHERE id=?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    char nombre_actual[256] = {0};
    int tipo_actual = 0;
    double objetivo_actual = 0;
    char f_ini_actual[64] = {0};
    char f_fin_actual[64] = {0};
    char estado_actual[32] = {0};

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strncpy_s(nombre_actual, sizeof(nombre_actual), (const char *)sqlite3_column_text(stmt, 0),
                  _TRUNCATE);
        tipo_actual = sqlite3_column_int(stmt, 1);
        objetivo_actual = sqlite3_column_double(stmt, 2);
        const char *p = (const char *)sqlite3_column_text(stmt, 3);
        if (p)
        {
            strncpy_s(f_ini_actual, sizeof(f_ini_actual), p, _TRUNCATE);
        }
        p = (const char *)sqlite3_column_text(stmt, 4);
        if (p)
        {
            strncpy_s(f_fin_actual, sizeof(f_fin_actual), p, _TRUNCATE);
        }
        p = (const char *)sqlite3_column_text(stmt, 5);
        if (p)
        {
            strncpy_s(estado_actual, sizeof(estado_actual), p, _TRUNCATE);
        }
    }
    sqlite3_finalize(stmt);

    char nombre[256] = {0};
    char f_ini[64] = {0};
    char f_fin[64] = {0};
    char estado[32] = {0};

    printf("Editando: %s (tipo: %s, objetivo: %.0f, estado: %s)\n\n", nombre_actual,
           tipo_to_text(tipo_actual), objetivo_actual, estado_actual);

    printf("Nombre [%s]: ", nombre_actual);
    input_string("", nombre, (int)sizeof(nombre));
    if (nombre[0] == '\0')
    {
        strncpy_s(nombre, sizeof(nombre), nombre_actual, _TRUNCATE);
    }

    printf("Valor objetivo [%.0f]: ", objetivo_actual);
    double objetivo = input_double("");
    if (objetivo <= 0)
    {
    }
    objetivo = objetivo_actual;

    printf("Estado [%s]: ", estado_actual);
    input_string("", estado, (int)sizeof(estado));
    if (estado[0] == '\0')
    {
        strncpy_s(estado, sizeof(estado), estado_actual, _TRUNCATE);
    }

    printf("Fecha inicio [%s]: ", f_ini_actual);
    input_date("", f_ini, (int)sizeof(f_ini));
    if (f_ini[0] == '\0')
    {
        strncpy_s(f_ini, sizeof(f_ini), f_ini_actual, _TRUNCATE);
    }

    printf("Fecha fin [%s]: ", f_fin_actual);
    input_date("", f_fin, (int)sizeof(f_fin));
    if (f_fin[0] == '\0')
    {
        strncpy_s(f_fin, sizeof(f_fin), f_fin_actual, _TRUNCATE);
    }

    if (!preparar_stmt(&stmt, "UPDATE meta SET nombre=?, valor_objetivo=?, "
                       "fecha_inicio=?, fecha_fin=?, estado=? WHERE id=?"))
    {
        mostrar_error_operacion("Meta", "actualizar");
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, DB_TRANSIENT);
    sqlite3_bind_double(stmt, 2, objetivo);
    sqlite3_bind_text(stmt, 3, f_ini, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 4, f_fin, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 5, estado, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 6, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Meta", "Editada", nombre);
    }
    else
    {
        mostrar_error_operacion("Meta", "editar");
    }
    pause_console();
}

void metas_eliminar(void)
{
    if (!hay_registros("meta"))
    {
        mostrar_no_hay_registros("metas");
        pause_console();
        return;
    }

    listar_entidades("meta", "ELIMINAR META", "No hay metas");

    int id = input_int("ID de la meta a eliminar (0=cancelar): ");
    if (id <= 0 || !existe_id("meta", id))
    {
        if (id > 0)
        {
            mostrar_no_existe("Meta");
        }

        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT nombre FROM meta WHERE id = ?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    char nombre[256] = {0};
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strncpy_s(nombre, sizeof(nombre), (const char *)sqlite3_column_text(stmt, 0), _TRUNCATE);
    }
    sqlite3_finalize(stmt);

    printf("Eliminar meta '%s'? (ID: %d)\n", nombre, id);
    if (!confirmar("Confirmar eliminacion"))
    {
        printf("Cancelado.\n");
        pause_console();
        return;
    }

    if (!preparar_stmt(&stmt, "DELETE FROM meta WHERE id = ?"))
    {
        mostrar_error_operacion("Meta", "eliminar");
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Meta", "Eliminada", nombre);
    }
    else
    {
        mostrar_error_operacion("Meta", "eliminar");
    }
    pause_console();
}

void metas_recalcular_progreso(void)
{
    if (!hay_registros("meta"))
    {
        mostrar_no_hay_registros("metas");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT id, tipo, valor_objetivo, valor_inicial FROM meta "
                       "WHERE estado = 'Activa'"))
    {
        return;
    }

    int actualizadas = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int tipo = sqlite3_column_int(stmt, 1);
        double objetivo = sqlite3_column_double(stmt, 2);
        double actual = obtener_valor_actual_meta(tipo);

        sqlite3_stmt *upd = NULL;
        if (preparar_stmt(&upd, "UPDATE meta SET valor_inicial = ? WHERE id = ?"))
        {
            sqlite3_bind_double(upd, 1, actual);
            sqlite3_bind_int(upd, 2, id);
            sqlite3_step(upd);
            sqlite3_finalize(upd);
        }

        if (actual >= objetivo)
        {
            sqlite3_stmt *upd2 = NULL;
            if (preparar_stmt(&upd2, "UPDATE meta SET estado = 'Completada' WHERE id = ?"))
            {
                sqlite3_bind_int(upd2, 1, id);
                sqlite3_step(upd2);
                sqlite3_finalize(upd2);
            }
        }

        actualizadas++;
    }
    sqlite3_finalize(stmt);

    printf("Progreso recalculado para %d meta(s).\n", actualizadas);
    pause_console();
}

void metas_dashboard(void)
{
    clear_screen();
    print_header("DASHBOARD DE METAS");

    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM meta WHERE estado = 'Activa'"))
    {
        return;
    }
    int activas = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activas = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM meta WHERE estado = 'Completada'"))
    {
        return;
    }
    int completadas = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        completadas = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM meta"))
    {
        return;
    }
    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    printf("  Metas activas:    %d\n", activas);
    printf("  Metas completadas: %d\n", completadas);
    printf("  Total metas:      %d\n", total);

    if (total > 0)
    {
        double pct = (double)completadas / (double)total * 100.0;
        printf("  Tasa exito:       %.1f%%\n", pct);
    }
    printf("\n");

    if (!preparar_stmt(&stmt, "SELECT id, nombre, tipo, valor_objetivo, valor_inicial "
                       "FROM meta WHERE estado = 'Activa' ORDER BY id"))
    {
        pause_console();
        return;
    }

    int hay_activas = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        hay_activas = 1;
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        int tipo = sqlite3_column_int(stmt, 2);
        double objetivo = sqlite3_column_double(stmt, 3);
        double actual = obtener_valor_actual_meta(tipo);

        double progreso = 0.0;
        if (objetivo > 0)
        {
            progreso = (actual / objetivo) * 100.0;
        }

        if (progreso > 100.0)
        {
            progreso = 100.0;
        }

        printf("  %s (%s): %.0f/%.0f (%.1f%%)\n", nombre ? nombre : "", tipo_to_text(tipo), actual,
               objetivo, progreso);

        int b = (int)(progreso / 5.0);
        if (b > 20)
        {
            b = 20;
            printf("  ");
        }

        for (int i = 0; i < b; i++)
        {
            printf("#");
        }

        for (int i = b; i < 20; i++)
        {
            printf(".");
        }

        printf("\n\n");
    }
    sqlite3_finalize(stmt);

    if (!hay_activas)
    {
        printf("  No hay metas activas. ¡Crea una!\n");
    }
    pause_console();
}

void menu_metas(void)
{
    MenuItem items[] = {{1, "Nueva meta", &metas_crear},
        {2, "Listar metas", &metas_listar},
        {3, "Editar meta", &metas_editar},
        {4, "Eliminar meta", &metas_eliminar},
        {5, "Recalcular progreso", &metas_recalcular_progreso},
        {6, "Dashboard de metas", &metas_dashboard},
        {0, "Volver", NULL}
    };
    ejecutar_menu("METAS PERSONALES", items, 7);
}
