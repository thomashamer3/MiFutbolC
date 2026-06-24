#include "equipamiento_historial.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static const char *item_tipo_texto(int tipo)
{
    switch (tipo)
    {
    case 1:
        return "Botines";
    case 2:
        return "Camiseta";
    case 3:
        return "Short";
    case 4:
        return "Canilleras";
    case 5:
        return "Guantes";
    case 6:
        return "Otro";
    default:
        return "Desconocido";
    }
}

static const char *estado_fisico_texto(int estado)
{
    switch (estado)
    {
    case 1:
        return "Nuevo";
    case 2:
        return "Bueno";
    case 3:
        return "Usado";
    case 4:
        return "Gastado";
    case 5:
        return "Descarte";
    default:
        return "N/A";
    }
}

void equipamiento_historial_crear(void)
{
    clear_screen();
    print_header("NUEVO EQUIPAMIENTO");

    printf("Tipo de item:\n");
    printf("  1 - Botines\n  2 - Camiseta\n  3 - Short\n");
    printf("  4 - Canilleras\n  5 - Guantes\n  6 - Otro\n");
    int tipo = input_int_rango("Seleccione tipo", 1, 6);

    char marca[128] = {0};
    char modelo[128] = {0};
    char fecha_compra[64] = {0};
    char notas[1024] = {0};

    input_string("Marca: ", marca, (int)sizeof(marca));
    input_string("Modelo: ", modelo, (int)sizeof(modelo));
    input_date("Fecha compra (dd/mm/aaaa): ", fecha_compra, (int)sizeof(fecha_compra));

    double precio = input_double("Precio ($): ");
    if (precio < 0)
    {
        precio = 0;
    }

    printf("Estado fisico (1=Nuevo, 2=Bueno, 3=Usado, 4=Gastado, 5=Descarte): ");
    int estado = input_int_rango("", 1, 5);

    int rating = input_int_rango("Rating personal (1-10): ", 1, 10);

    input_string("Notas: ", notas, (int)sizeof(notas));

    long long id = obtener_siguiente_id("equipamiento_historial");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "INSERT INTO equipamiento_historial "
                       "(id, tipo, marca, modelo, fecha_compra, precio, "
                       "partidos_usados, estado_fisico, rating, notas, activo) "
                       "VALUES (?,?,?,?,?,?,0,?,?,?,1)"))
    {
        mostrar_error_operacion("Equipamiento", "crear");
        return;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, tipo);
    sqlite3_bind_text(stmt, 3, marca[0] ? marca : "", -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 4, modelo[0] ? modelo : "", -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 5, fecha_compra[0] ? fecha_compra : "", -1, DB_TRANSIENT);
    sqlite3_bind_double(stmt, 6, precio);
    sqlite3_bind_int(stmt, 7, estado);
    sqlite3_bind_int(stmt, 8, rating);
    sqlite3_bind_text(stmt, 9, notas[0] ? notas : "", -1, DB_TRANSIENT);

    int flag = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (flag == SQLITE_DONE)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %s", marca, modelo);
        mostrar_alerta_operacion("Equipamiento", "Agregado", buf);
    }
    else
    {
        mostrar_error_operacion("Equipamiento", "guardar");
    }
    pause_console();
}

void equipamiento_historial_listar(void)
{
    if (!hay_registros("equipamiento_historial"))
    {
        mostrar_no_hay_registros("equipamiento");
        pause_console();
        return;
    }

    clear_screen();
    print_header("HISTORIAL DE EQUIPAMIENTO");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt,
                       "SELECT id, tipo, marca, modelo, fecha_compra, precio, "
                       "partidos_usados, estado_fisico, rating, notas, activo "
                       "FROM equipamiento_historial ORDER BY activo DESC, fecha_compra DESC"))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int tipo = sqlite3_column_int(stmt, 1);
        const char *marca = (const char *)sqlite3_column_text(stmt, 2);
        const char *modelo = (const char *)sqlite3_column_text(stmt, 3);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 4);
        double precio = sqlite3_column_double(stmt, 5);
        int partidos = sqlite3_column_int(stmt, 6);
        int estado_f = sqlite3_column_int(stmt, 7);
        int rating = sqlite3_column_int(stmt, 8);
        const char *notas = (const char *)sqlite3_column_text(stmt, 9);
        int activo = sqlite3_column_int(stmt, 10);

        printf("  %d. [%s] %s %s\n", id, activo ? "ACTIVO" : "INACTIVO", marca ? marca : "",
               modelo ? modelo : "");
        printf("     Tipo: %s\n", item_tipo_texto(tipo));
        if (fecha && fecha[0])
        {
            printf("     Compra: %s | $%.2f\n", fecha, precio);
        }
        printf("     Partidos: %d | Estado: %s | Rating: %d/10\n", partidos,
               estado_fisico_texto(estado_f), rating);
        if (notas && notas[0])
        {
            printf("     Notas: %s\n", notas);
        }
        printf("\n");
    }
    sqlite3_finalize(stmt);
    pause_console();
}

typedef struct
{
    int tipo;
    char marca[128];
    char modelo[128];
    char fecha[64];
    double precio;
    int partidos_usados;
    int estado_fisico;
    int rating;
    char notas[1024];
    int activo;
} EquipamientoActual;

static bool leer_equipamiento_actual(int id, EquipamientoActual *out)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT tipo, marca, modelo, fecha_compra, precio, "
                       "partidos_usados, estado_fisico, rating, notas, activo "
                       "FROM equipamiento_historial WHERE id=?"))
    {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    memset(out, 0, sizeof(*out));
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        out->tipo = sqlite3_column_int(stmt, 0);
        const char *s;
        s = (const char *)sqlite3_column_text(stmt, 1);
        if (s) strncpy_s(out->marca, sizeof(out->marca), s, _TRUNCATE);
        s = (const char *)sqlite3_column_text(stmt, 2);
        if (s) strncpy_s(out->modelo, sizeof(out->modelo), s, _TRUNCATE);
        s = (const char *)sqlite3_column_text(stmt, 3);
        if (s) strncpy_s(out->fecha, sizeof(out->fecha), s, _TRUNCATE);
        out->precio = sqlite3_column_double(stmt, 4);
        out->partidos_usados = sqlite3_column_int(stmt, 5);
        out->estado_fisico = sqlite3_column_int(stmt, 6);
        out->rating = sqlite3_column_int(stmt, 7);
        s = (const char *)sqlite3_column_text(stmt, 8);
        if (s) strncpy_s(out->notas, sizeof(out->notas), s, _TRUNCATE);
        out->activo = sqlite3_column_int(stmt, 9);
    }
    sqlite3_finalize(stmt);
    return true;
}

typedef struct
{
    char marca[128];
    char modelo[128];
    char fecha[64];
    char notas[1024];
    double precio;
    int partidos_usados;
    int estado_fisico;
    int rating;
    int activo;
} EquipamientoInput;

static EquipamientoInput pedir_datos_equipamiento(const EquipamientoActual *actual)
{
    EquipamientoInput in;
    memset(&in, 0, sizeof(in));

    printf("Editando: %s %s\n\n", actual->marca, actual->modelo);

    input_string_default("Marca", actual->marca, in.marca, (int)sizeof(in.marca));
    input_string_default("Modelo", actual->modelo, in.modelo, (int)sizeof(in.modelo));
    input_string_default("Fecha compra", actual->fecha, in.fecha, (int)sizeof(in.fecha));
    input_string_default("Notas", actual->notas, in.notas, (int)sizeof(in.notas));

    in.precio = input_double_default("Precio", actual->precio, 0);
    printf("Partidos usados [%d]: ", actual->partidos_usados);
    in.partidos_usados = input_int("");
    if (in.partidos_usados < 0) in.partidos_usados = actual->partidos_usados;

    printf("Estado fisico (1-5) [%d]: ", actual->estado_fisico);
    in.estado_fisico = input_int("");
    if (in.estado_fisico < 1 || in.estado_fisico > 5) in.estado_fisico = actual->estado_fisico;

    printf("Rating (1-10) [%d]: ", actual->rating);
    in.rating = input_int("");
    if (in.rating < 1 || in.rating > 10) in.rating = actual->rating;

    printf("Activo (1=Si, 0=No) [%d]: ", actual->activo);
    in.activo = input_int("");
    if (in.activo < 0 || in.activo > 1) in.activo = actual->activo;

    return in;
}

static void actualizar_equipamiento_en_db(int id, const EquipamientoInput *in)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "UPDATE equipamiento_historial SET marca=?, modelo=?, "
                       "fecha_compra=?, precio=?, partidos_usados=?, estado_fisico=?, "
                       "rating=?, notas=?, activo=? WHERE id=?"))
    {
        mostrar_error_operacion("Equipamiento", "actualizar");
        return;
    }

    sqlite3_bind_text(stmt, 1, in->marca, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 2, in->modelo, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 3, in->fecha, -1, DB_TRANSIENT);
    sqlite3_bind_double(stmt, 4, in->precio);
    sqlite3_bind_int(stmt, 5, in->partidos_usados);
    sqlite3_bind_int(stmt, 6, in->estado_fisico);
    sqlite3_bind_int(stmt, 7, in->rating);
    sqlite3_bind_text(stmt, 8, in->notas, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 9, in->activo);
    sqlite3_bind_int(stmt, 10, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Equipamiento", "Editado", in->marca);
    }
    else
    {
        mostrar_error_operacion("Equipamiento", "editar");
    }
}

void equipamiento_historial_editar(void)
{
    if (!hay_registros("equipamiento_historial"))
    {
        mostrar_no_hay_registros("equipamiento");
        pause_console();
        return;
    }

    listar_entidades("equipamiento_historial", "EDITAR EQUIPAMIENTO", "No hay items");

    int id = input_int("ID del item a editar (0=cancelar): ");
    if (id <= 0 || !existe_id("equipamiento_historial", id))
    {
        if (id > 0) mostrar_no_existe("Item");
        pause_console();
        return;
    }

    EquipamientoActual actual;
    if (!leer_equipamiento_actual(id, &actual))
    {
        mostrar_no_existe("Item");
        pause_console();
        return;
    }

    EquipamientoInput in = pedir_datos_equipamiento(&actual);
    actualizar_equipamiento_en_db(id, &in);
    pause_console();
}

void equipamiento_historial_eliminar(void)
{
    if (!hay_registros("equipamiento_historial"))
    {
        mostrar_no_hay_registros("equipamiento");
        pause_console();
        return;
    }

    listar_entidades("equipamiento_historial", "ELIMINAR EQUIPAMIENTO", "No hay items");

    int id = input_int("ID del item a eliminar (0=cancelar): ");
    if (id <= 0 || !existe_id("equipamiento_historial", id))
    {
        if (id > 0)
        {
            mostrar_no_existe("Item");
        }
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT marca, modelo FROM equipamiento_historial WHERE id = ?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    char marca[128] = {0};
    char modelo[128] = {0};
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *pumtero = (const char *)sqlite3_column_text(stmt, 0);
        if (pumtero)
        {
            strncpy_s(marca, sizeof(marca), pumtero, _TRUNCATE);
        }
        const char *puntero = (const char *)sqlite3_column_text(stmt, 1);
        if (puntero)
        {
            strncpy_s(modelo, sizeof(modelo), puntero, _TRUNCATE);
        }
    }
    sqlite3_finalize(stmt);

    char nombre[256];
    snprintf(nombre, sizeof(nombre), "%s %s", marca, modelo);

    printf("Eliminar '%s'? (ID: %d)\n", nombre, id);
    if (!confirmar("Confirmar eliminacion"))
    {
        printf("Cancelado.\n");
        pause_console();
        return;
    }

    if (!preparar_stmt(&stmt, "DELETE FROM equipamiento_historial WHERE id = ?"))
    {
        mostrar_error_operacion("Equipamiento", "eliminar");
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    int flag = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (flag == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Equipamiento", "Eliminado", nombre);
    }
    else
    {
        mostrar_error_operacion("Equipamiento", "eliminar");
    }
    pause_console();
}

static int contar_equipamiento(const char *where_clause)
{
    char sql[256];
    if (where_clause)
    {
        snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM equipamiento_historial WHERE %s", where_clause);
    }
    else
    {
        snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM equipamiento_historial");
    }
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, sql)) return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

static double sumar_equipamiento(const char *col)
{
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT COALESCE(SUM(%s), 0) FROM equipamiento_historial", col);
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, sql)) return 0.0;
    double val = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) val = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return val;
}

void equipamiento_historial_estadisticas(void)
{
    if (!hay_registros("equipamiento_historial"))
    {
        mostrar_no_hay_registros("equipamiento");
        pause_console();
        return;
    }

    clear_screen();
    print_header("ESTADISTICAS DE EQUIPAMIENTO");

    int total = contar_equipamiento(NULL);
    int activos = contar_equipamiento("activo=1");
    int total_partidos = (int)sumar_equipamiento("partidos_usados");
    double total_gastado = sumar_equipamiento("precio");
    double avg_rating = sumar_equipamiento("rating") / (total > 0 ? (double)total : 1.0);

    printf("  Total items:        %d\n", total);
    printf("  Items activos:      %d\n", activos);
    printf("  Partidos totales:   %d\n", total_partidos);
    printf("  Total gastado:      $%.2f\n", total_gastado);
    printf("  Rating promedio:    %.1f/10\n", avg_rating);

    printf("\n  Por tipo:\n");
    for (int tipo = 1; tipo <= 6; tipo++)
    {
        sqlite3_stmt *stmt = NULL;
        if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM equipamiento_historial WHERE tipo=?"))
            continue;
        sqlite3_bind_int(stmt, 1, tipo);
        int cnt = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) cnt = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        if (cnt > 0)
            printf("    %s: %d\n", item_tipo_texto(tipo), cnt);
    }

    pause_console();
}

void menu_equipamiento_historial(void)
{
    MenuItem items[] = {{1, "Nuevo item", &equipamiento_historial_crear},
        {2, "Listar items", &equipamiento_historial_listar},
        {3, "Editar item", &equipamiento_historial_editar},
        {4, "Eliminar item", &equipamiento_historial_eliminar},
        {5, "Estadisticas", &equipamiento_historial_estadisticas},
        {0, "Volver", NULL}
    };
    ejecutar_menu("HISTORIAL DE EQUIPAMIENTO", items, 6);
}
