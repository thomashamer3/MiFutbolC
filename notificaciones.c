#include "notificaciones.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <time.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static void agregar_notificacion(const char *tipo, const char *mensaje)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("INSERT INTO notificacion (tipo, mensaje, leida) VALUES (?, ?, 0)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, mensaje, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void crear_notificacion(void)
{
    char tipo[64];
    char mensaje[1024];
    input_string("Tipo (lesion/finanza/torneo/cumpleanos/general): ", tipo, sizeof(tipo));
    input_string("Mensaje: ", mensaje, sizeof(mensaje));
    agregar_notificacion(tipo, mensaje);
    printf("Notificacion creada.\n");
    app_log_event("NOTIFICACIONES", "Notificacion manual creada");
    pause_console();
}

static void listar_todas(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, tipo, mensaje, leida, fecha FROM notificacion ORDER BY id DESC LIMIT 50", &stmt))
    {
        mostrar_no_hay_registros("notificaciones");
        return;
    }

    mostrar_pantalla("TODAS LAS NOTIFICACIONES");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. [%s] %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 3) ? "LEIDA" : "NO LEIDA");
        printf("     %s\n", sqlite3_column_text(stmt, 2));
        printf("     Fecha: %s\n", sqlite3_column_text(stmt, 4));
        printf("     ------------------------------\n");
    }
    sqlite3_finalize(stmt);
    if (count == 0) mostrar_no_hay_registros("notificaciones");
    pause_console();
}

static void listar_no_leidas(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, tipo, mensaje, fecha FROM notificacion WHERE leida = 0 ORDER BY id DESC", &stmt))
    {
        mostrar_no_hay_registros("notificaciones no leidas");
        return;
    }

    mostrar_pantalla("NOTIFICACIONES NO LEIDAS");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. [%s] %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2));
        printf("     Fecha: %s\n", sqlite3_column_text(stmt, 3));
        printf("     ------------------------------\n");
    }
    sqlite3_finalize(stmt);
    if (count == 0) mostrar_no_hay_registros("notificaciones no leidas");
    pause_console();
}

static void marcar_leida(void)
{
    int id = input_int("ID de la notificacion: ");
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE notificacion SET leida = 1 WHERE id = ?", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Notificacion marcada como leida.\n");
    else
        printf("Error: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
}

static void marcar_todas_leidas(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE notificacion SET leida = 1 WHERE leida = 0", &stmt))
        return;
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Todas las notificaciones marcadas como leidas.\n");
}

static void eliminar_notificacion(void)
{
    int id = input_int("ID de la notificacion a eliminar: ");
    sqlite3_stmt *stmt;
    if (!preparar_stmt("DELETE FROM notificacion WHERE id = ?", &stmt))
        return;
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Notificacion eliminada.\n");
}

static void alertas_lesiones(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT j.nombre, l.descripcion, l.fecha "
                       "FROM lesion l LEFT JOIN jugador j ON l.jugador_id = j.id "
                       "WHERE l.fecha >= date('now', '-7 days') ORDER BY l.fecha DESC", &stmt))
    {
        mostrar_no_hay_registros("alertas de lesiones recientes");
        return;
    }

    mostrar_pantalla("ALERTAS DE LESIONES (ultimos 7 dias)");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %s: %s (%s)\n",
               sqlite3_column_text(stmt, 0) ? (const char*)sqlite3_column_text(stmt, 0) : "?",
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2));
    }
    sqlite3_finalize(stmt);
    if (count == 0) mostrar_no_hay_registros("lesiones recientes");
    pause_console();
}

static void alertas_financieras(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, monto, tipo, descripcion, fecha "
                       "FROM financiamiento ORDER BY id DESC LIMIT 10", &stmt))
    {
        mostrar_no_hay_registros("movimientos financieros");
        return;
    }

    mostrar_pantalla("ALERTAS FINANCIERAS (ultimos movimientos)");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. %s $%.2f - %s (%s)\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_double(stmt, 1),
               sqlite3_column_text(stmt, 3),
               sqlite3_column_text(stmt, 4));
    }
    sqlite3_finalize(stmt);
    if (count == 0) mostrar_no_hay_registros("movimientos financieros");
    pause_console();
}

static void torneos_por_vencer(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT nombre, fecha_fin FROM torneo "
                       "WHERE fecha_fin >= date('now') AND fecha_fin <= date('now', '+30 days') "
                       "ORDER BY fecha_fin", &stmt))
    {
        mostrar_no_hay_registros("torneos por vencer");
        return;
    }

    mostrar_pantalla("TORNEOS POR VENCER (proximos 30 dias)");
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %s - Vence: %s\n",
               sqlite3_column_text(stmt, 0),
               sqlite3_column_text(stmt, 1));
    }
    sqlite3_finalize(stmt);
    if (count == 0) mostrar_no_hay_registros("torneos por vencer");
    pause_console();
}

static void cumpleanos_jugadores(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT nombre, fecha_nacimiento FROM jugador "
                       "WHERE fecha_nacimiento IS NOT NULL AND fecha_nacimiento != ''", &stmt))
    {
        mostrar_no_hay_registros("jugadores con fecha de nacimiento");
        return;
    }

    mostrar_pantalla("CUMPLEANOS DE JUGADORES");

    time_t t = time(NULL);
    struct tm tm_struct;
    localtime_s(&tm_struct, &t);
    int mes_actual = tm_struct.tm_mon + 1;
    int dia_actual = tm_struct.tm_mday;

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fnac = (const char*)sqlite3_column_text(stmt, 1);
        if (!fnac) continue;
        int anio;
        int mes;
        int dia;
        if (sscanf_s(fnac, "%d-%d-%d", &anio, &mes, &dia) >= 2 && mes == mes_actual)
        {
            count++;
            int dias = dia - dia_actual;
            const char *estado;
            if (dias == 0)
                estado = "HOY!";
            else if (dias > 0)
                estado = "En %d dias";
            else
                estado = "Pasado";
            printf("  %s: %s (%s)\n",
                   sqlite3_column_text(stmt, 0), fnac, estado);
        }
    }
    sqlite3_finalize(stmt);
    if (count == 0) printf("  No hay cumpleanos este mes.\n");
    pause_console();
}

void menu_notificaciones(void)
{
    MenuItem items[] =
    {
        {1, "Todas las Notificaciones", &listar_todas},
        {2, "No Leidas", &listar_no_leidas},
        {3, "Crear Notificacion", &crear_notificacion},
        {4, "Marcar como Leida", &marcar_leida},
        {5, "Marcar Todas como Leidas", &marcar_todas_leidas},
        {6, "Eliminar Notificacion", &eliminar_notificacion},
        {7, "Alertas de Lesiones", &alertas_lesiones},
        {8, "Alertas Financieras", &alertas_financieras},
        {9, "Torneos por Vencer", &torneos_por_vencer},
        {10, "Cumpleanos de Jugadores", &cumpleanos_jugadores},
        {0, "Volver", NULL}
    };
    ejecutar_menu("NOTIFICACIONES", items, 11);
}
