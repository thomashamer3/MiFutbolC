#include "equipamiento_historial.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
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
        if (id > 0)
        {
            mostrar_no_existe("Item");
        }
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT tipo, marca, modelo, fecha_compra, precio, "
                       "partidos_usados, estado_fisico, rating, notas, activo "
                       "FROM equipamiento_historial WHERE id=?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    char marca_actual[128] = {0};
    char modelo_actual[128] = {0};
    char fecha_actual[64] = {0};
    double precio_actual = 0;
    int partidos_actual = 0;
    int estado_f_actual = 0;
    int rating_actual = 0;
    char notas_actual[1024] = {0};
    int activo_actual = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *flag = (const char *)sqlite3_column_text(stmt, 1);
        if (flag)
        {
            strncpy_s(marca_actual, sizeof(marca_actual), flag, _TRUNCATE);
        }
        const char *flag2 = (const char *)sqlite3_column_text(stmt, 2);
        if (flag2)
        {
            strncpy_s(modelo_actual, sizeof(modelo_actual), flag2, _TRUNCATE);
        }
        const char *flag3 = (const char *)sqlite3_column_text(stmt, 3);
        if (flag3)
        {
            strncpy_s(fecha_actual, sizeof(fecha_actual), flag3, _TRUNCATE);
        }
        precio_actual = sqlite3_column_double(stmt, 4);
        partidos_actual = sqlite3_column_int(stmt, 5);
        estado_f_actual = sqlite3_column_int(stmt, 6);
        rating_actual = sqlite3_column_int(stmt, 7);
        const char *flag4 = (const char *)sqlite3_column_text(stmt, 8);
        if (flag4)
        {
            strncpy_s(notas_actual, sizeof(notas_actual), flag4, _TRUNCATE);
        }
        activo_actual = sqlite3_column_int(stmt, 9);
    }
    sqlite3_finalize(stmt);

    char marca[128] = {0};
    char modelo[128] = {0};
    char fecha[64] = {0};
    char notas[1024] = {0};

    printf("Editando: %s %s\n\n", marca_actual, modelo_actual);

    printf("Marca [%s]: ", marca_actual);
    input_string("", marca, (int)sizeof(marca));
    if (marca[0] == '\0')
    {
        strncpy_s(marca, sizeof(marca), marca_actual, _TRUNCATE);
    }

    printf("Modelo [%s]: ", modelo_actual);
    input_string("", modelo, (int)sizeof(modelo));
    if (modelo[0] == '\0')
    {
        strncpy_s(modelo, sizeof(modelo), modelo_actual, _TRUNCATE);
    }

    printf("Fecha compra [%s]: ", fecha_actual);
    input_date("", fecha, (int)sizeof(fecha));
    if (fecha[0] == '\0')
    {
        strncpy_s(fecha, sizeof(fecha), fecha_actual, _TRUNCATE);
    }

    printf("Precio [%.2f]: ", precio_actual);
    double precio = input_double("");
    if (precio < 0)
    {
        precio = precio_actual;
    }

    printf("Partidos usados [%d]: ", partidos_actual);
    int partidos = input_int("");
    if (partidos < 0)
    {
        partidos = partidos_actual;
    }

    printf("Estado fisico (1-5) [%d]: ", estado_f_actual);
    int estado_f = input_int("");
    if (estado_f < 1 || estado_f > 5)
    {
        estado_f = estado_f_actual;
    }

    printf("Rating (1-10) [%d]: ", rating_actual);
    int rating = input_int("");
    if (rating < 1 || rating > 10)
    {
        rating = rating_actual;
    }

    printf("Activo (1=Si, 0=No) [%d]: ", activo_actual);
    int activo = input_int("");
    if (activo != 0 && activo != 1)
    {
        activo = activo_actual;
    }

    printf("Notas [%s]: ", notas_actual);
    input_string("", notas, (int)sizeof(notas));
    if (notas[0] == '\0')
    {
        strncpy_s(notas, sizeof(notas), notas_actual, _TRUNCATE);
    }

    if (!preparar_stmt(&stmt, "UPDATE equipamiento_historial SET marca=?, modelo=?, "
                       "fecha_compra=?, precio=?, partidos_usados=?, estado_fisico=?, "
                       "rating=?, notas=?, activo=? WHERE id=?"))
    {
        mostrar_error_operacion("Equipamiento", "actualizar");
        return;
    }

    sqlite3_bind_text(stmt, 1, marca, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 2, modelo, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fecha, -1, DB_TRANSIENT);
    sqlite3_bind_double(stmt, 4, precio);
    sqlite3_bind_int(stmt, 5, partidos);
    sqlite3_bind_int(stmt, 6, estado_f);
    sqlite3_bind_int(stmt, 7, rating);
    sqlite3_bind_text(stmt, 8, notas, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 9, activo);
    sqlite3_bind_int(stmt, 10, id);

    int flag = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (flag == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Equipamiento", "Editado", marca);
    }
    else
    {
        mostrar_error_operacion("Equipamiento", "editar");
    }
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

    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM equipamiento_historial"))
    {
        return;
    }
    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM equipamiento_historial WHERE activo=1"))
    {
        return;
    }
    int activos = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activos = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt, "SELECT SUM(partidos_usados) FROM equipamiento_historial"))
    {
        return;
    }
    int total_partidos = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total_partidos = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt, "SELECT SUM(precio) FROM equipamiento_historial"))
    {
        return;
    }
    double total_gastado = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total_gastado = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt, "SELECT AVG(rating) FROM equipamiento_historial"))
    {
        return;
    }
    double avg_rating = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        avg_rating = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);

    printf("  Total items:        %d\n", total);
    printf("  Items activos:      %d\n", activos);
    printf("  Partidos totales:   %d\n", total_partidos);
    printf("  Total gastado:      $%.2f\n", total_gastado);
    printf("  Rating promedio:    %.1f/10\n", avg_rating);

    printf("\n  Por tipo:\n");
    for (int tipo = 1; tipo <= 6; tipo++)
    {
        if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM equipamiento_historial WHERE tipo=?"))
        {
            continue;
        }
        sqlite3_bind_int(stmt, 1, tipo);
        int cnt = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            cnt = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        if (cnt > 0)
        {
            printf("    %s: %d\n", item_tipo_texto(tipo), cnt);
        }
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
