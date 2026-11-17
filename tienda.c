#include "tienda.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdio.h>

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static void asegurar_tabla_tienda(void)
{
    const char *sql = "CREATE TABLE IF NOT EXISTS tienda ("
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

typedef struct
{
    char nombre[100];
    int tipo;
    char url[300];
    char direccion[300];
    char telefono[50];
    char whatsapp[50];
    char instagram[100];
    char email[100];
    int vende_botines;
    int vende_camisetas;
    int vende_pelotas;
    int vende_equipamiento;
    int vende_accesorios;
    int rango_precio;
    int tiene_envio;
    int mercadopago;
    int rating;
    int favorito;
    char notas[500];
} TiendaDatos;

/* Textos que cambian entre alta y modificacion; el resto del formulario es comun. */
typedef struct
{
    const char *nombre;
    const char *menu_tipo;
    const char *url;
    const char *direccion;
    const char *telefono;
    const char *whatsapp;
    const char *instagram;
    const char *email;
    const char *equipamiento;
    const char *accesorios;
    const char *menu_rango;
    const char *notas;
} TiendaPrompts;

static const TiendaPrompts TIENDA_PROMPTS_CREAR =
{
    "Nombre de la tienda: ",
    "\nTipo de tienda:\n1. Online\n2. Fisica\n3. Ambas\n",
    "URL: ",
    "Direccion: ",
    "Telefono: ",
    "WhatsApp: ",
    "Instagram: ",
    "Email: ",
    "  Equipamiento (canilleras, medias, etc): ",
    "  Accesorios (bolsos, porta-botines, etc): ",
    "\nRango de precio:\n0. Sin informacion\n1. Economico\n2. Medio\n3. Premium\n",
    "Notas (opcional): ",
};

static const TiendaPrompts TIENDA_PROMPTS_MODIFICAR =
{
    "Nuevo nombre: ",
    "\nNuevo tipo de tienda:\n1. Online\n2. Fisica\n3. Ambas\n",
    "Nueva URL: ",
    "Nueva direccion: ",
    "Nuevo telefono: ",
    "Nuevo WhatsApp: ",
    "Nuevo Instagram: ",
    "Nuevo Email: ",
    "  Equipamiento: ",
    "  Accesorios: ",
    "\nRango de precio (0=Sin info, 1=Economico, 2=Medio, 3=Premium):\n",
    "Nuevas notas: ",
};

/* Devuelve 0 si el nombre queda vacio (ya avisado al usuario). */
static int tienda_leer_datos(TiendaDatos *t, const TiendaPrompts *p)
{
    input_string(p->nombre, t->nombre, sizeof(t->nombre));
    if (t->nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return 0;
    }

    printf("%s", p->menu_tipo);
    t->tipo = input_int_rango("Seleccione (1-3): ", 1, 3);

    if (t->tipo == 1 || t->tipo == 3)
    {
        input_string(p->url, t->url, sizeof(t->url));
    }

    if (t->tipo == 2 || t->tipo == 3)
    {
        input_string(p->direccion, t->direccion, sizeof(t->direccion));
    }

    input_string(p->telefono, t->telefono, sizeof(t->telefono));
    input_string(p->whatsapp, t->whatsapp, sizeof(t->whatsapp));
    input_string(p->instagram, t->instagram, sizeof(t->instagram));
    input_string(p->email, t->email, sizeof(t->email));

    printf("\nProductos que vende (0=No, 1=Si):\n");
    t->vende_botines = input_int("  Botines: ");
    t->vende_camisetas = input_int("  Camisetas: ");
    t->vende_pelotas = input_int("  Pelotas: ");
    t->vende_equipamiento = input_int(p->equipamiento);
    t->vende_accesorios = input_int(p->accesorios);

    printf("%s", p->menu_rango);
    t->rango_precio = input_int_rango("Seleccione (0-3): ", 0, 3);

    t->tiene_envio = input_int("Tiene envio? (0=No, 1=Si): ");
    t->mercadopago = input_int("Acepta MercadoPago? (0=No, 1=Si): ");
    t->rating = input_int_rango("Rating (1-10): ", 1, 10);
    t->favorito = input_int("Es favorito? (0=No, 1=Si): ");

    input_string(p->notas, t->notas, sizeof(t->notas));
    return 1;
}

/* INSERT y UPDATE listan los campos en el mismo orden; solo cambia el primer indice. */
static void tienda_bind_datos(sqlite3_stmt *stmt, const TiendaDatos *t, int i)
{
    sqlite3_bind_text(stmt, i++, t->nombre, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, i++, t->tipo);
    sqlite3_bind_text(stmt, i++, t->url, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, i++, t->direccion, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, i++, t->telefono, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, i++, t->whatsapp, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, i++, t->instagram, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, i++, t->email, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, i++, t->vende_botines);
    sqlite3_bind_int(stmt, i++, t->vende_camisetas);
    sqlite3_bind_int(stmt, i++, t->vende_pelotas);
    sqlite3_bind_int(stmt, i++, t->vende_equipamiento);
    sqlite3_bind_int(stmt, i++, t->vende_accesorios);
    sqlite3_bind_int(stmt, i++, t->rango_precio);
    sqlite3_bind_int(stmt, i++, t->tiene_envio);
    sqlite3_bind_int(stmt, i++, t->mercadopago);
    sqlite3_bind_int(stmt, i++, t->rating);
    sqlite3_bind_int(stmt, i++, t->favorito);
    sqlite3_bind_text(stmt, i, t->notas, -1, SQLITE_STATIC);
}

void crear_tienda(void)
{
    clear_screen();
    print_header("CREAR TIENDA");

    int id = (int)obtener_siguiente_id("tienda");
    TiendaDatos t = {0};
    t.tipo = 1;

    if (!tienda_leer_datos(&t, &TIENDA_PROMPTS_CREAR))
    {
        return;
    }

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
        tienda_bind_datos(stmt, &t, 2);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            mostrar_alerta_operacion("Tienda", "Creada", t.nombre);
        }
        else
        {
            printf("Error al crear la tienda: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

static void imprimir_contacto_tienda(const char *url, const char *direccion, const char *telefono,
                                     const char *whatsapp, const char *instagram, const char *email)
{
    if (url && url[0])
    {
        printf("  URL: %s\n", url);
    }
    if (direccion && direccion[0])
        printf("  Direccion: %s\n", direccion);
    if (telefono && telefono[0])
        printf("  Telefono: %s\n", telefono);
    if (whatsapp && whatsapp[0])
        printf("  WhatsApp: %s\n", whatsapp);
    if (instagram && instagram[0])
        printf("  Instagram: %s\n", instagram);
    if (email && email[0])
        printf("  Email: %s\n", email);
}

static void imprimir_productos_tienda(int botines, int camisetas, int pelotas, int equipamiento,
                                      int accesorios)
{
    printf("  Vende: ");
    if (botines)
        printf("Botines ");
    if (camisetas)
        printf("Camisetas ");
    if (pelotas)
        printf("Pelotas ");
    if (equipamiento)
        printf("Equipamiento ");
    if (accesorios)
        printf("Accesorios ");
    printf("\n");
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
    imprimir_contacto_tienda(url, direccion, telefono, whatsapp, instagram, email);
    imprimir_productos_tienda(vende_botines, vende_camisetas, vende_pelotas, vende_equipamiento,
                              vende_accesorios);
    printf("  Rango precio: %s\n", rango_precio_str(rango_precio));
    printf("  Envio: %s | MercadoPago: %s | Rating: %d/10\n", tiene_envio ? "Si" : "No",
           mercadopago ? "Si" : "No", rating);
    if (notas && notas[0])
    {
        printf("  Notas: %s\n", notas);
    }

    printf("  Agregado: %s\n", fecha ? fecha : "");
    printf("----------------------------------------\n");
}

void listar_tiendas(void)
{
    clear_screen();
    print_header("LISTADO DE TIENDAS");

    asegurar_tabla_tienda();

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, nombre, tipo, url, direccion, telefono, whatsapp, instagram, "
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
        {
            mostrar_no_hay_registros("tiendas registradas");
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

void modificar_tienda(void)
{
    clear_screen();
    print_header("MODIFICAR TIENDA");

    asegurar_tabla_tienda();

    listar_tiendas();

    int id = input_int("\nID de la tienda a modificar (0 cancelar): ");
    if (id <= 0)
    {
        return;
    }

    if (!existe_id("tienda", id))
    {
        printf("ID invalido.\n");
        pause_console();
        return;
    }

    TiendaDatos t = {0};
    t.tipo = 1;

    if (!tienda_leer_datos(&t, &TIENDA_PROMPTS_MODIFICAR))
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "UPDATE tienda SET "
                      "nombre = ?, tipo = ?, url = ?, direccion = ?, telefono = ?, whatsapp = ?, "
                      "instagram = ?, email = ?, vende_botines = ?, vende_camisetas = ?, "
                      "vende_pelotas = ?, vende_equipamiento = ?, vende_accesorios = ?, "
                      "rango_precio = ?, tiene_envio = ?, mercadopago = ?, rating = ?, "
                      "favorito = ?, notas = ? WHERE id = ?";

    if (preparar_stmt(&stmt, sql))
    {
        tienda_bind_datos(stmt, &t, 1);
        sqlite3_bind_int(stmt, 20, id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            mostrar_alerta_operacion("Tienda", "Modificada", t.nombre);
        }
        else
        {
            printf("Error al modificar la tienda: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

void eliminar_tienda(void)
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

void menu_tiendas(void)
{
    asegurar_tabla_tienda();

    MenuItem items[] = {{1, "Crear Tienda", &crear_tienda},
        {2, "Listar Tiendas", &listar_tiendas},
        {3, "Modificar Tienda", &modificar_tienda},
        {4, "Eliminar Tienda", &eliminar_tienda},
        {0, "Volver", NULL}
    };

    ejecutar_menu("TIENDAS", items, 5);
}
