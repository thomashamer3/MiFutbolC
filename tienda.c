#include "tienda.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static void asegurar_tabla_tienda(void)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS tienda ("
        " id                  INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre              TEXT    NOT NULL,"
        " tipo                INTEGER DEFAULT 1,"
        " url                 TEXT    DEFAULT '',"
        " direccion           TEXT    DEFAULT '',"
        " telefono            TEXT    DEFAULT '',"
        " whatsapp            TEXT    DEFAULT '',"
        " instagram           TEXT    DEFAULT '',"
        " email               TEXT    DEFAULT '',"
        " vende_botines       INTEGER DEFAULT 0,"
        " vende_camisetas     INTEGER DEFAULT 0,"
        " vende_pelotas       INTEGER DEFAULT 0,"
        " vende_equipamiento  INTEGER DEFAULT 0,"
        " vende_accesorios    INTEGER DEFAULT 0,"
        " rango_precio        INTEGER DEFAULT 0,"
        " tiene_envio         INTEGER DEFAULT 0,"
        " mercadopago         INTEGER DEFAULT 0,"
        " rating              INTEGER DEFAULT 0,"
        " favorito            INTEGER DEFAULT 0,"
        " notas               TEXT    DEFAULT '',"
        " fecha_agregado      TEXT    DEFAULT (datetime('now', 'localtime'))"
        ");";

    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
    {
        printf("Error al crear tabla tienda: %s\n", sqlite3_errmsg(db));
    }
}

static const char *tipo_tienda_str(int tipo)
{
    switch (tipo)
    {
    case 1:
        return "Online";
    case 2:
        return "Fisica";
    case 3:
        return "Ambas";
    default:
        return "Desconocido";
    }
}

static const char *rango_precio_str(int rango)
{
    switch (rango)
    {
    case 0:
        return "Sin info";
    case 1:
        return "Economico";
    case 2:
        return "Medio";
    case 3:
        return "Premium";
    default:
        return "Desconocido";
    }
}

void crear_tienda()
{
    clear_screen();
    print_header("CREAR TIENDA");

    int id = (int)obtener_siguiente_id("tienda");
    char nombre[100] = {0};
    int tipo = 1;
    char url[300] = {0};
    char direccion[300] = {0};
    char telefono[50] = {0};
    char whatsapp[50] = {0};
    char instagram[100] = {0};
    char email[100] = {0};
    int vende_botines = 0;
    int vende_camisetas = 0;
    int vende_pelotas = 0;
    int vende_equipamiento = 0;
    int vende_accesorios = 0;
    int rango_precio = 0;
    int tiene_envio = 0;
    int mercadopago = 0;
    int rating = 0;
    int favorito = 0;
    char notas[500] = {0};

    input_string("Nombre de la tienda: ", nombre, sizeof(nombre));
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return;
    }

    printf("\nTipo de tienda:\n");
    printf("1. Online\n");
    printf("2. Fisica\n");
    printf("3. Ambas\n");
    tipo = input_int_rango("Seleccione (1-3): ", 1, 3);

    if (tipo == 1 || tipo == 3)
    {
        input_string("URL: ", url, sizeof(url));
    }

    if (tipo == 2 || tipo == 3)
    {
        input_string("Direccion: ", direccion, sizeof(direccion));
    }

    input_string("Telefono: ", telefono, sizeof(telefono));
    input_string("WhatsApp: ", whatsapp, sizeof(whatsapp));
    input_string("Instagram: ", instagram, sizeof(instagram));
    input_string("Email: ", email, sizeof(email));

    printf("\nProductos que vende (0=No, 1=Si):\n");
    vende_botines = input_int("  Botines: ");
    vende_camisetas = input_int("  Camisetas: ");
    vende_pelotas = input_int("  Pelotas: ");
    vende_equipamiento = input_int("  Equipamiento (canilleras, medias, etc): ");
    vende_accesorios = input_int("  Accesorios (bolsos, porta-botines, etc): ");

    printf("\nRango de precio:\n");
    printf("0. Sin informacion\n");
    printf("1. Economico\n");
    printf("2. Medio\n");
    printf("3. Premium\n");
    rango_precio = input_int_rango("Seleccione (0-3): ", 0, 3);

    tiene_envio = input_int("Tiene envio? (0=No, 1=Si): ");
    mercadopago = input_int("Acepta MercadoPago? (0=No, 1=Si): ");
    rating = input_int_rango("Rating (1-10): ", 1, 10);
    favorito = input_int("Es favorito? (0=No, 1=Si): ");

    input_string("Notas (opcional): ", notas, sizeof(notas));

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO tienda "
        "(id, nombre, tipo, url, direccion, telefono, whatsapp, instagram, email, "
        "vende_botines, vende_camisetas, vende_pelotas, vende_equipamiento, vende_accesorios, "
        "rango_precio, tiene_envio, mercadopago, rating, favorito, notas) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (preparar_stmt(&stmt, sql))
    {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, tipo);
        sqlite3_bind_text(stmt, 4, url, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, direccion, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, telefono, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, whatsapp, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, instagram, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, email, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 10, vende_botines);
        sqlite3_bind_int(stmt, 11, vende_camisetas);
        sqlite3_bind_int(stmt, 12, vende_pelotas);
        sqlite3_bind_int(stmt, 13, vende_equipamiento);
        sqlite3_bind_int(stmt, 14, vende_accesorios);
        sqlite3_bind_int(stmt, 15, rango_precio);
        sqlite3_bind_int(stmt, 16, tiene_envio);
        sqlite3_bind_int(stmt, 17, mercadopago);
        sqlite3_bind_int(stmt, 18, rating);
        sqlite3_bind_int(stmt, 19, favorito);
        sqlite3_bind_text(stmt, 20, notas, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            mostrar_alerta_operacion("Tienda", "Creada", nombre);
        }
        else
        {
            printf("Error al crear la tienda: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

static void imprimir_fila_tienda(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
    int tipo = sqlite3_column_int(stmt, 2);
    const char *url = (const char *)sqlite3_column_text(stmt, 3);
    const char *direccion = (const char *)sqlite3_column_text(stmt, 4);
    const char *telefono = (const char *)sqlite3_column_text(stmt, 5);
    const char *whatsapp = (const char *)sqlite3_column_text(stmt, 6);
    const char *instagram = (const char *)sqlite3_column_text(stmt, 7);
    const char *email = (const char *)sqlite3_column_text(stmt, 8);
    int vende_botines = sqlite3_column_int(stmt, 9);
    int vende_camisetas = sqlite3_column_int(stmt, 10);
    int vende_pelotas = sqlite3_column_int(stmt, 11);
    int vende_equipamiento = sqlite3_column_int(stmt, 12);
    int vende_accesorios = sqlite3_column_int(stmt, 13);
    int rango_precio = sqlite3_column_int(stmt, 14);
    int tiene_envio = sqlite3_column_int(stmt, 15);
    int mercadopago = sqlite3_column_int(stmt, 16);
    int rating = sqlite3_column_int(stmt, 17);
    int favorito = sqlite3_column_int(stmt, 18);
    const char *notas = (const char *)sqlite3_column_text(stmt, 19);
    const char *fecha = (const char *)sqlite3_column_text(stmt, 20);

    printf("ID: %d%s\n", id, favorito ? " [FAVORITO]" : "");
    printf("  Nombre: %s\n", nombre ? nombre : "");
    printf("  Tipo: %s\n", tipo_tienda_str(tipo));
    if (url && url[0]) printf("  URL: %s\n", url);
    if (direccion && direccion[0]) printf("  Direccion: %s\n", direccion);
    if (telefono && telefono[0]) printf("  Telefono: %s\n", telefono);
    if (whatsapp && whatsapp[0]) printf("  WhatsApp: %s\n", whatsapp);
    if (instagram && instagram[0]) printf("  Instagram: %s\n", instagram);
    if (email && email[0]) printf("  Email: %s\n", email);

    printf("  Vende: ");
    if (vende_botines) printf("Botines ");
    if (vende_camisetas) printf("Camisetas ");
    if (vende_pelotas) printf("Pelotas ");
    if (vende_equipamiento) printf("Equipamiento ");
    if (vende_accesorios) printf("Accesorios ");
    printf("\n");

    printf("  Rango precio: %s\n", rango_precio_str(rango_precio));
    printf("  Envio: %s | MercadoPago: %s | Rating: %d/10\n",
           tiene_envio ? "Si" : "No",
           mercadopago ? "Si" : "No",
           rating);
    if (notas && notas[0]) printf("  Notas: %s\n", notas);
    printf("  Agregado: %s\n", fecha ? fecha : "");
    printf("----------------------------------------\n");
}

void listar_tiendas()
{
    clear_screen();
    print_header("LISTADO DE TIENDAS");

    asegurar_tabla_tienda();

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, nombre, tipo, url, direccion, telefono, whatsapp, instagram, "
        "email, vende_botines, vende_camisetas, vende_pelotas, vende_equipamiento, "
        "vende_accesorios, rango_precio, tiene_envio, mercadopago, rating, favorito, "
        "notas, fecha_agregado FROM tienda ORDER BY favorito DESC, nombre ASC";

    if (preparar_stmt(&stmt, sql))
    {
        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            imprimir_fila_tienda(stmt);
        }

        if (!found)
            mostrar_no_hay_registros("tiendas registradas");

        sqlite3_finalize(stmt);
    }

    pause_console();
}

void modificar_tienda()
{
    clear_screen();
    print_header("MODIFICAR TIENDA");

    asegurar_tabla_tienda();

    listar_tiendas();

    int id = input_int("\nID de la tienda a modificar (0 cancelar): ");
    if (id <= 0)
        return;

    if (!existe_id("tienda", id))
    {
        printf("ID invalido.\n");
        pause_console();
        return;
    }

    char nombre[100] = {0};
    int tipo = 1;
    char url[300] = {0};
    char direccion[300] = {0};
    char telefono[50] = {0};
    char whatsapp[50] = {0};
    char instagram[100] = {0};
    char email[100] = {0};
    int vende_botines = 0;
    int vende_camisetas = 0;
    int vende_pelotas = 0;
    int vende_equipamiento = 0;
    int vende_accesorios = 0;
    int rango_precio = 0;
    int tiene_envio = 0;
    int mercadopago = 0;
    int rating = 0;
    int favorito = 0;
    char notas[500] = {0};

    input_string("Nuevo nombre: ", nombre, sizeof(nombre));
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return;
    }

    printf("\nNuevo tipo de tienda:\n");
    printf("1. Online\n2. Fisica\n3. Ambas\n");
    tipo = input_int_rango("Seleccione (1-3): ", 1, 3);

    if (tipo == 1 || tipo == 3)
        input_string("Nueva URL: ", url, sizeof(url));
    if (tipo == 2 || tipo == 3)
        input_string("Nueva direccion: ", direccion, sizeof(direccion));

    input_string("Nuevo telefono: ", telefono, sizeof(telefono));
    input_string("Nuevo WhatsApp: ", whatsapp, sizeof(whatsapp));
    input_string("Nuevo Instagram: ", instagram, sizeof(instagram));
    input_string("Nuevo Email: ", email, sizeof(email));

    printf("\nProductos que vende (0=No, 1=Si):\n");
    vende_botines = input_int("  Botines: ");
    vende_camisetas = input_int("  Camisetas: ");
    vende_pelotas = input_int("  Pelotas: ");
    vende_equipamiento = input_int("  Equipamiento: ");
    vende_accesorios = input_int("  Accesorios: ");

    printf("\nRango de precio (0=Sin info, 1=Economico, 2=Medio, 3=Premium):\n");
    rango_precio = input_int_rango("Seleccione (0-3): ", 0, 3);

    tiene_envio = input_int("Tiene envio? (0=No, 1=Si): ");
    mercadopago = input_int("Acepta MercadoPago? (0=No, 1=Si): ");
    rating = input_int_rango("Rating (1-10): ", 1, 10);
    favorito = input_int("Es favorito? (0=No, 1=Si): ");
    input_string("Nuevas notas: ", notas, sizeof(notas));

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "UPDATE tienda SET "
        "nombre = ?, tipo = ?, url = ?, direccion = ?, telefono = ?, whatsapp = ?, "
        "instagram = ?, email = ?, vende_botines = ?, vende_camisetas = ?, "
        "vende_pelotas = ?, vende_equipamiento = ?, vende_accesorios = ?, "
        "rango_precio = ?, tiene_envio = ?, mercadopago = ?, rating = ?, "
        "favorito = ?, notas = ? WHERE id = ?";

    if (preparar_stmt(&stmt, sql))
    {
        sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, tipo);
        sqlite3_bind_text(stmt, 3, url, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, direccion, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, telefono, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, whatsapp, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, instagram, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, email, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 9, vende_botines);
        sqlite3_bind_int(stmt, 10, vende_camisetas);
        sqlite3_bind_int(stmt, 11, vende_pelotas);
        sqlite3_bind_int(stmt, 12, vende_equipamiento);
        sqlite3_bind_int(stmt, 13, vende_accesorios);
        sqlite3_bind_int(stmt, 14, rango_precio);
        sqlite3_bind_int(stmt, 15, tiene_envio);
        sqlite3_bind_int(stmt, 16, mercadopago);
        sqlite3_bind_int(stmt, 17, rating);
        sqlite3_bind_int(stmt, 18, favorito);
        sqlite3_bind_text(stmt, 19, notas, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 20, id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            mostrar_alerta_operacion("Tienda", "Modificada", nombre);
        }
        else
        {
            printf("Error al modificar la tienda: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

void eliminar_tienda()
{
    clear_screen();
    print_header("ELIMINAR TIENDA");

    asegurar_tabla_tienda();

    listar_tiendas();

    int id = input_int("\nID de la tienda a eliminar (0 cancelar): ");
    if (id <= 0)
        return;

    if (!existe_id("tienda", id))
    {
        printf("ID invalido.\n");
        pause_console();
        return;
    }

    if (!confirmar("Seguro que deseas eliminar esta tienda?"))
        return;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM tienda WHERE id = ?";

    if (preparar_stmt(&stmt, sql))
    {
        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Tienda eliminada correctamente.\n");
        }
        else
        {
            printf("Error al eliminar la tienda: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

void menu_tiendas()
{
    asegurar_tabla_tienda();

    MenuItem items[] =
    {
        {1, "Crear Tienda", crear_tienda},
        {2, "Listar Tiendas", listar_tiendas},
        {3, "Modificar Tienda", modificar_tienda},
        {4, "Eliminar Tienda", eliminar_tienda},
        {0, "Volver", NULL}
    };

    ejecutar_menu("TIENDAS", items, 5);
}
