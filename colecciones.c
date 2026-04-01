#include "colecciones.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "export.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    INV_TIPO_CAMISETA = 1,
    INV_TIPO_BOTINES = 2,
    INV_TIPO_ACCESORIO = 3
};

enum
{
    INV_ESTADO_NUEVO = 0,
    INV_ESTADO_USADO = 1
};

#define COLECCIONES_BACKUP_DEFAULT "colecciones_inventario.json"
#define MAX_JSON_BYTES (10 * 1024 * 1024)

typedef struct
{
    int old_id;
    int new_id;
} IdMap;

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static FILE *abrir_archivo_lectura(const char *path)
{
    FILE *file = NULL;
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    return file;
}

static FILE *abrir_archivo_escritura(const char *path)
{
    FILE *file = NULL;
#ifdef _WIN32
    if (fopen_s(&file, path, "wb") != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    return file;
}

static char *leer_archivo_completo(const char *path)
{
    FILE *file = abrir_archivo_lectura(path);
    if (!file)
    {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length <= 0 || length > MAX_JSON_BYTES)
    {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    char *content = (char *)malloc((size_t)length + 1);
    if (!content)
    {
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(content, 1, (size_t)length, file);
    fclose(file);

    if (read_bytes != (size_t)length)
    {
        free(content);
        return NULL;
    }

    content[length] = '\0';
    return content;
}

static int map_get_new_id(const IdMap *map, int count, int old_id)
{
    if (!map)
    {
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        if (map[i].old_id == old_id)
        {
            return map[i].new_id;
        }
    }

    return 0;
}

static int cjson_obj_int_or(const cJSON *obj, const char *key, int default_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item))
    {
        return default_value;
    }
    return item->valueint;
}

static double cjson_obj_double_or(const cJSON *obj, const char *key, double default_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item))
    {
        return default_value;
    }
    return item->valuedouble;
}

static const char *cjson_obj_string_or(const cJSON *obj, const char *key, const char *default_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || !item->valuestring)
    {
        return default_value;
    }
    return item->valuestring;
}

static int obtener_id_coleccion_por_nombre(const char *nombre)
{
    sqlite3_stmt *stmt;
    int id = 0;

    if (!preparar_stmt(&stmt, "SELECT id FROM coleccion WHERE nombre = ? LIMIT 1"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

static int obtener_id_inventario_por_camiseta(int camiseta_id)
{
    sqlite3_stmt *stmt;
    int id = 0;

    if (!preparar_stmt(&stmt, "SELECT id FROM inventario_item WHERE camiseta_id = ? LIMIT 1"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, camiseta_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

static int obtener_item_inventario_equivalente(int tipo, const char *nombre, int estado,
                                               double valor, const char *fecha_compra)
{
    sqlite3_stmt *stmt;
    int id = 0;

    if (!preparar_stmt(&stmt,
                       "SELECT id FROM inventario_item "
                       "WHERE tipo = ? AND COALESCE(nombre, '') = ? AND estado = ? "
                       "AND ABS(COALESCE(valor, 0) - ?) < 0.000001 "
                       "AND COALESCE(fecha_compra, '') = ? AND camiseta_id IS NULL "
                       "LIMIT 1"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, tipo);
    sqlite3_bind_text(stmt, 2, nombre ? nombre : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, estado);
    sqlite3_bind_double(stmt, 4, valor);
    sqlite3_bind_text(stmt, 5, fecha_compra ? fecha_compra : "", -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return id;
}

static int guardar_coleccion_importada(const char *nombre, const char *descripcion, const char *fecha)
{
    sqlite3_stmt *stmt_insert;
    sqlite3_stmt *stmt_update;

    if (!preparar_stmt(&stmt_insert,
                       "INSERT OR IGNORE INTO coleccion(nombre, descripcion, fecha_creacion) "
                       "VALUES(?, ?, COALESCE(NULLIF(?, ''), CURRENT_TIMESTAMP))"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt_insert, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_insert, 2, descripcion ? descripcion : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_insert, 3, fecha ? fecha : "", -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt_insert);
    sqlite3_finalize(stmt_insert);

    if (!preparar_stmt(&stmt_update, "UPDATE coleccion SET descripcion = ? WHERE nombre = ?"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt_update, 1, descripcion ? descripcion : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_update, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt_update);
    sqlite3_finalize(stmt_update);

    return obtener_id_coleccion_por_nombre(nombre);
}

static const char *tipo_inventario_a_texto(int tipo)
{
    switch (tipo)
    {
    case INV_TIPO_CAMISETA:
        return "Camiseta";
    case INV_TIPO_BOTINES:
        return "Botines";
    case INV_TIPO_ACCESORIO:
        return "Accesorio";
    default:
        return "Desconocido";
    }
}

static const char *estado_inventario_a_texto(int estado)
{
    return estado == INV_ESTADO_USADO ? "Usado" : "Nuevo";
}

static void leer_texto_obligatorio(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string(prompt, buffer, size);
        trim_whitespace(buffer);
        if (buffer[0] != '\0')
        {
            return;
        }
        printf("El texto no puede estar vacio.\n");
    }
}

static int existe_item_inventario_para_camiseta(int camiseta_id)
{
    sqlite3_stmt *stmt;
    int existe = 0;

    if (!preparar_stmt(&stmt, "SELECT 1 FROM inventario_item WHERE camiseta_id = ? LIMIT 1"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, camiseta_id);
    existe = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return existe;
}

static void listar_camisetas_para_inventario(void)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt,
                       "SELECT c.id, c.nombre, CASE WHEN i.id IS NULL THEN 0 ELSE 1 END "
                       "FROM camiseta c "
                       "LEFT JOIN inventario_item i ON i.camiseta_id = c.id "
                       "ORDER BY c.id"))
    {
        printf("Error al listar camisetas.\n");
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *nombre = sqlite3_column_text(stmt, 1);
        int ya_vinculada = sqlite3_column_int(stmt, 2);

        printf("%d - %s%s\n",
               id,
               nombre ? (const char *)nombre : "(Sin nombre)",
               ya_vinculada ? " [YA EN INVENTARIO]" : "");
    }

    sqlite3_finalize(stmt);
}

static void listar_colecciones_simple(void)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt,
                       "SELECT c.id, c.nombre, COUNT(ci.inventario_id) "
                       "FROM coleccion c "
                       "LEFT JOIN coleccion_inventario ci ON ci.coleccion_id = c.id "
                       "GROUP BY c.id, c.nombre "
                       "ORDER BY c.nombre"))
    {
        printf("Error al listar colecciones.\n");
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("%d - %s (items: %d)\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2));
    }

    sqlite3_finalize(stmt);
}

static void listar_inventario_simple(void)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt,
                       "SELECT i.id, i.tipo, "
                       "CASE WHEN i.tipo = 1 THEN COALESCE(c.nombre, '(Camiseta eliminada)') ELSE i.nombre END, "
                       "i.estado "
                       "FROM inventario_item i "
                       "LEFT JOIN camiseta c ON c.id = i.camiseta_id "
                       "ORDER BY i.id"))
    {
        printf("Error al listar inventario.\n");
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int tipo = sqlite3_column_int(stmt, 1);
        int estado = sqlite3_column_int(stmt, 3);
        printf("%d - %s | %s | %s\n",
               sqlite3_column_int(stmt, 0),
               tipo_inventario_a_texto(tipo),
               sqlite3_column_text(stmt, 2),
               estado_inventario_a_texto(estado));
    }

    sqlite3_finalize(stmt);
}

static void crear_item_inventario(void)
{
    clear_screen();
    print_header("NUEVO ITEM DE INVENTARIO");

    int tipo = input_int_rango("Tipo (1=Camiseta, 2=Botines, 3=Accesorio): ", 1, 3);
    int estado = input_int_rango("Estado (0=Nuevo, 1=Usado): ", 0, 1);
    double valor = input_double("Valor estimado: ");

    char fecha_compra[24] = {0};
    input_string("Fecha compra (YYYY-MM-DD, opcional): ", fecha_compra, (int)sizeof(fecha_compra));
    trim_whitespace(fecha_compra);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "INSERT INTO inventario_item(tipo, nombre, estado, valor, fecha_compra, camiseta_id) "
                       "VALUES(?, ?, ?, ?, ?, ?)"))
    {
        printf("No se pudo preparar el alta de inventario.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, tipo);
    sqlite3_bind_int(stmt, 3, estado);
    sqlite3_bind_double(stmt, 4, valor);
    sqlite3_bind_text(stmt, 5, fecha_compra, -1, SQLITE_TRANSIENT);

    if (tipo == INV_TIPO_CAMISETA)
    {
        if (!hay_registros("camiseta"))
        {
            sqlite3_finalize(stmt);
            mostrar_no_hay_registros("camisetas para vincular");
            pause_console();
            return;
        }

        printf("\nCamisetas disponibles:\n");
        listar_camisetas_para_inventario();

        int camiseta_id = input_int("\nID de camiseta a vincular (0 para cancelar): ");
        if (camiseta_id == 0)
        {
            sqlite3_finalize(stmt);
            return;
        }

        if (!existe_id("camiseta", camiseta_id))
        {
            sqlite3_finalize(stmt);
            printf("ID de camiseta inexistente.\n");
            pause_console();
            return;
        }

        if (existe_item_inventario_para_camiseta(camiseta_id))
        {
            sqlite3_finalize(stmt);
            printf("Esa camiseta ya tiene un item de inventario asociado.\n");
            pause_console();
            return;
        }

        sqlite3_bind_text(stmt, 2, "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 6, camiseta_id);
    }
    else
    {
        char nombre_item[120];
        leer_texto_obligatorio("Nombre del item: ", nombre_item, (int)sizeof(nombre_item));
        sqlite3_bind_text(stmt, 2, nombre_item, -1, SQLITE_TRANSIENT);
        sqlite3_bind_null(stmt, 6);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("No se pudo crear el item de inventario: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    sqlite3_finalize(stmt);
    printf("Item de inventario creado correctamente.\n");
    pause_console();
}

static void listar_inventario_completo(void)
{
    clear_screen();
    print_header("INVENTARIO PERSONAL");

    if (!hay_registros("inventario_item"))
    {
        mostrar_no_hay_registros("items de inventario");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "SELECT i.id, i.tipo, "
                       "CASE WHEN i.tipo = 1 THEN COALESCE(c.nombre, '(Camiseta eliminada)') ELSE i.nombre END AS nombre_item, "
                       "i.estado, i.valor, COALESCE(i.fecha_compra, ''), COUNT(ci.coleccion_id) "
                       "FROM inventario_item i "
                       "LEFT JOIN camiseta c ON c.id = i.camiseta_id "
                       "LEFT JOIN coleccion_inventario ci ON ci.inventario_id = i.id "
                       "GROUP BY i.id, i.tipo, i.nombre, i.estado, i.valor, i.fecha_compra, c.nombre "
                       "ORDER BY i.id"))
    {
        printf("Error al consultar inventario.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int tipo = sqlite3_column_int(stmt, 1);
        int estado = sqlite3_column_int(stmt, 3);
        printf("ID:%d | %s | %s | Estado:%s | Valor:%.2f | Compra:%s | Colecciones:%d\n",
               sqlite3_column_int(stmt, 0),
               tipo_inventario_a_texto(tipo),
               sqlite3_column_text(stmt, 2),
               estado_inventario_a_texto(estado),
               sqlite3_column_double(stmt, 4),
               sqlite3_column_text(stmt, 5),
               sqlite3_column_int(stmt, 6));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void sincronizar_camisetas_a_inventario(void)
{
    clear_screen();
    print_header("SINCRONIZAR CAMISETAS");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        pause_console();
        return;
    }

    const char *sql =
        "INSERT INTO inventario_item(tipo, nombre, estado, valor, fecha_compra, camiseta_id) "
        "SELECT 1, '', 0, 0, '', c.id "
        "FROM camiseta c "
        "LEFT JOIN inventario_item i ON i.camiseta_id = c.id "
        "WHERE i.camiseta_id IS NULL;";

    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
    {
        printf("No se pudo sincronizar camisetas: %s\n", sqlite3_errmsg(db));
        pause_console();
        return;
    }

    printf("Sincronizacion completada. Nuevos items creados: %d\n", sqlite3_changes(db));
    pause_console();
}

static void crear_coleccion(void)
{
    clear_screen();
    print_header("NUEVA COLECCION");

    char nombre[120];
    char descripcion[220];

    leer_texto_obligatorio("Nombre de coleccion: ", nombre, (int)sizeof(nombre));
    input_string("Descripcion (opcional): ", descripcion, (int)sizeof(descripcion));
    trim_whitespace(descripcion);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "INSERT INTO coleccion(nombre, descripcion) VALUES(?, ?)"))
    {
        printf("No se pudo preparar el alta de coleccion.\n");
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, descripcion, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("No se pudo crear la coleccion: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    sqlite3_finalize(stmt);
    printf("Coleccion creada correctamente.\n");
    pause_console();
}

static void listar_colecciones_completo(void)
{
    clear_screen();
    print_header("COLECCIONES");

    if (!hay_registros("coleccion"))
    {
        mostrar_no_hay_registros("colecciones");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "SELECT c.id, c.nombre, COALESCE(c.descripcion, ''), COUNT(ci.inventario_id) "
                       "FROM coleccion c "
                       "LEFT JOIN coleccion_inventario ci ON ci.coleccion_id = c.id "
                       "GROUP BY c.id, c.nombre, c.descripcion "
                       "ORDER BY c.nombre"))
    {
        printf("Error al consultar colecciones.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("ID:%d | %s | Items:%d\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 3));

        const unsigned char *descripcion = sqlite3_column_text(stmt, 2);
        if (descripcion && descripcion[0] != '\0')
        {
            printf("  %s\n", descripcion);
        }
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void agregar_item_a_coleccion(void)
{
    clear_screen();
    print_header("AGREGAR ITEM A COLECCION");

    if (!hay_registros("coleccion"))
    {
        mostrar_no_hay_registros("colecciones");
        pause_console();
        return;
    }

    if (!hay_registros("inventario_item"))
    {
        mostrar_no_hay_registros("items de inventario");
        pause_console();
        return;
    }

    printf("Colecciones disponibles:\n");
    listar_colecciones_simple();
    int coleccion_id = input_int("\nID de coleccion (0 para cancelar): ");
    if (coleccion_id == 0)
    {
        return;
    }

    if (!existe_id("coleccion", coleccion_id))
    {
        printf("ID de coleccion inexistente.\n");
        pause_console();
        return;
    }

    printf("\nItems disponibles:\n");
    listar_inventario_simple();
    int inventario_id = input_int("\nID de item (0 para cancelar): ");
    if (inventario_id == 0)
    {
        return;
    }

    if (!existe_id("inventario_item", inventario_id))
    {
        printf("ID de item inexistente.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "INSERT OR IGNORE INTO coleccion_inventario(coleccion_id, inventario_id) VALUES(?, ?)"))
    {
        printf("No se pudo preparar la vinculacion.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, coleccion_id);
    sqlite3_bind_int(stmt, 2, inventario_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("No se pudo agregar item a coleccion: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int cambios = sqlite3_changes(db);
    sqlite3_finalize(stmt);

    if (cambios == 0)
    {
        printf("Ese item ya estaba dentro de la coleccion.\n");
    }
    else
    {
        printf("Item agregado a la coleccion correctamente.\n");
    }

    pause_console();
}

static void quitar_item_de_coleccion(void)
{
    clear_screen();
    print_header("QUITAR ITEM DE COLECCION");

    if (!hay_registros("coleccion_inventario"))
    {
        mostrar_no_hay_registros("asociaciones en colecciones");
        pause_console();
        return;
    }

    printf("Colecciones disponibles:\n");
    listar_colecciones_simple();
    int coleccion_id = input_int("\nID de coleccion (0 para cancelar): ");
    if (coleccion_id == 0)
    {
        return;
    }

    if (!existe_id("coleccion", coleccion_id))
    {
        printf("ID de coleccion inexistente.\n");
        pause_console();
        return;
    }

    char nombre_coleccion[128] = {0};
    obtener_nombre_entidad("coleccion", coleccion_id, nombre_coleccion, sizeof(nombre_coleccion));
    printf("\nItems de la coleccion '%s':\n", nombre_coleccion[0] ? nombre_coleccion : "(sin nombre)");

    sqlite3_stmt *stmt_list;
    if (!preparar_stmt(&stmt_list,
                       "SELECT i.id, i.tipo, "
                       "CASE WHEN i.tipo = 1 THEN COALESCE(c.nombre, '(Camiseta eliminada)') ELSE i.nombre END "
                       "FROM coleccion_inventario ci "
                       "JOIN inventario_item i ON i.id = ci.inventario_id "
                       "LEFT JOIN camiseta c ON c.id = i.camiseta_id "
                       "WHERE ci.coleccion_id = ? "
                       "ORDER BY i.id"))
    {
        printf("No se pudo listar items de la coleccion.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt_list, 1, coleccion_id);
    int hay_items = 0;
    while (sqlite3_step(stmt_list) == SQLITE_ROW)
    {
        hay_items = 1;
        printf("%d - %s | %s\n",
               sqlite3_column_int(stmt_list, 0),
               tipo_inventario_a_texto(sqlite3_column_int(stmt_list, 1)),
               sqlite3_column_text(stmt_list, 2));
    }
    sqlite3_finalize(stmt_list);

    if (!hay_items)
    {
        printf("La coleccion esta vacia.\n");
        pause_console();
        return;
    }

    int inventario_id = input_int("\nID de item a quitar (0 para cancelar): ");
    if (inventario_id == 0)
    {
        return;
    }

    sqlite3_stmt *stmt_del;
    if (!preparar_stmt(&stmt_del,
                       "DELETE FROM coleccion_inventario WHERE coleccion_id = ? AND inventario_id = ?"))
    {
        printf("No se pudo preparar la eliminacion.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt_del, 1, coleccion_id);
    sqlite3_bind_int(stmt_del, 2, inventario_id);

    if (sqlite3_step(stmt_del) != SQLITE_DONE)
    {
        printf("No se pudo quitar el item: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt_del);
        pause_console();
        return;
    }

    int cambios = sqlite3_changes(db);
    sqlite3_finalize(stmt_del);

    if (cambios == 0)
    {
        printf("No existia esa relacion en la coleccion.\n");
    }
    else
    {
        printf("Item quitado de la coleccion correctamente.\n");
    }

    pause_console();
}

static void ver_items_por_coleccion(void)
{
    clear_screen();
    print_header("ITEMS POR COLECCION");

    if (!hay_registros("coleccion"))
    {
        mostrar_no_hay_registros("colecciones");
        pause_console();
        return;
    }

    printf("Colecciones disponibles:\n");
    listar_colecciones_simple();

    int coleccion_id = input_int("\nID de coleccion (0 para cancelar): ");
    if (coleccion_id == 0)
    {
        return;
    }

    if (!existe_id("coleccion", coleccion_id))
    {
        printf("ID de coleccion inexistente.\n");
        pause_console();
        return;
    }

    char nombre_coleccion[128] = {0};
    obtener_nombre_entidad("coleccion", coleccion_id, nombre_coleccion, sizeof(nombre_coleccion));

    printf("\nColeccion: %s\n", nombre_coleccion[0] ? nombre_coleccion : "(sin nombre)");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "SELECT i.id, i.tipo, "
                       "CASE WHEN i.tipo = 1 THEN COALESCE(c.nombre, '(Camiseta eliminada)') ELSE i.nombre END, "
                       "i.estado, i.valor, COALESCE(i.fecha_compra, '') "
                       "FROM coleccion_inventario ci "
                       "JOIN inventario_item i ON i.id = ci.inventario_id "
                       "LEFT JOIN camiseta c ON c.id = i.camiseta_id "
                       "WHERE ci.coleccion_id = ? "
                       "ORDER BY i.id"))
    {
        printf("Error al consultar items de la coleccion.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, coleccion_id);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        hay = 1;
        printf("ID:%d | %s | %s | Estado:%s | Valor:%.2f | Compra:%s\n",
               sqlite3_column_int(stmt, 0),
               tipo_inventario_a_texto(sqlite3_column_int(stmt, 1)),
               sqlite3_column_text(stmt, 2),
               estado_inventario_a_texto(sqlite3_column_int(stmt, 3)),
               sqlite3_column_double(stmt, 4),
               sqlite3_column_text(stmt, 5));
    }

    sqlite3_finalize(stmt);

    if (!hay)
    {
        printf("La coleccion no tiene items asociados.\n");
    }

    pause_console();
}

static void filtrar_buscar_inventario(void)
{
    clear_screen();
    print_header("FILTRAR Y BUSCAR INVENTARIO");

    if (!hay_registros("inventario_item"))
    {
        mostrar_no_hay_registros("items de inventario");
        pause_console();
        return;
    }

    int tipo = input_int_rango("Tipo (0=Todos, 1=Camiseta, 2=Botines, 3=Accesorio): ", 0, 3);
    int estado = input_int_rango("Estado (-1=Todos, 0=Nuevo, 1=Usado): ", -1, 1);

    int coleccion_id = 0;
    if (confirmar("Deseas filtrar por coleccion especifica?"))
    {
        printf("\nColecciones disponibles:\n");
        listar_colecciones_simple();
        coleccion_id = input_int("\nID de coleccion (0 para todas): ");
        if (coleccion_id < 0)
        {
            coleccion_id = 0;
        }
        if (coleccion_id > 0 && !existe_id("coleccion", coleccion_id))
        {
            printf("ID de coleccion inexistente.\n");
            pause_console();
            return;
        }
    }

    char termino[128] = {0};
    input_string("Texto a buscar en nombre (opcional): ", termino, (int)sizeof(termino));
    trim_whitespace(termino);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "SELECT i.id, i.tipo, "
                       "CASE WHEN i.tipo = 1 THEN COALESCE(c.nombre, '(Camiseta eliminada)') ELSE i.nombre END AS nombre_item, "
                       "i.estado, i.valor, COALESCE(i.fecha_compra, ''), COUNT(ci.coleccion_id) "
                       "FROM inventario_item i "
                       "LEFT JOIN camiseta c ON c.id = i.camiseta_id "
                       "LEFT JOIN coleccion_inventario ci ON ci.inventario_id = i.id "
                       "WHERE (? = 0 OR i.tipo = ?) "
                       "AND (? = -1 OR i.estado = ?) "
                       "AND (? = 0 OR EXISTS(SELECT 1 FROM coleccion_inventario ci2 "
                       "                    WHERE ci2.inventario_id = i.id AND ci2.coleccion_id = ?)) "
                       "AND (? = '' OR lower(CASE WHEN i.tipo = 1 THEN COALESCE(c.nombre, '') ELSE COALESCE(i.nombre, '') END) "
                       "               LIKE '%' || lower(?) || '%') "
                       "GROUP BY i.id, i.tipo, i.nombre, i.estado, i.valor, i.fecha_compra, c.nombre "
                       "ORDER BY i.id"))
    {
        printf("Error al preparar consulta de filtros.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, tipo);
    sqlite3_bind_int(stmt, 2, tipo);
    sqlite3_bind_int(stmt, 3, estado);
    sqlite3_bind_int(stmt, 4, estado);
    sqlite3_bind_int(stmt, 5, coleccion_id);
    sqlite3_bind_int(stmt, 6, coleccion_id);
    sqlite3_bind_text(stmt, 7, termino, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, termino, -1, SQLITE_TRANSIENT);

    int encontrados = 0;
    printf("\nResultados:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        encontrados++;
        printf("ID:%d | %s | %s | Estado:%s | Valor:%.2f | Compra:%s | Colecciones:%d\n",
               sqlite3_column_int(stmt, 0),
               tipo_inventario_a_texto(sqlite3_column_int(stmt, 1)),
               sqlite3_column_text(stmt, 2),
               estado_inventario_a_texto(sqlite3_column_int(stmt, 3)),
               sqlite3_column_double(stmt, 4),
               sqlite3_column_text(stmt, 5),
               sqlite3_column_int(stmt, 6));
    }
    sqlite3_finalize(stmt);

    if (encontrados == 0)
    {
        printf("No se encontraron items con esos filtros.\n");
    }

    pause_console();
}

static void exportar_backup_colecciones_json(void)
{
    clear_screen();
    print_header("EXPORTAR BACKUP COLECCIONES");

    char filename[128] = {0};
    input_string("Nombre de archivo (Enter para colecciones_inventario.json): ",
                 filename,
                 (int)sizeof(filename));
    trim_whitespace(filename);
    if (filename[0] == '\0')
    {
        strcpy_s(filename, sizeof(filename), COLECCIONES_BACKUP_DEFAULT);
    }

    const char *path = get_export_path(filename);
    FILE *file = abrir_archivo_escritura(path);
    if (!file)
    {
        printf("No se pudo crear el archivo de backup.\n");
        pause_console();
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "tipo", "colecciones_inventario_backup");
    cJSON_AddNumberToObject(root, "version", 1);

    cJSON *arr_colecciones = cJSON_CreateArray();
    cJSON *arr_inventario = cJSON_CreateArray();
    cJSON *arr_relaciones = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "colecciones", arr_colecciones);
    cJSON_AddItemToObject(root, "inventario", arr_inventario);
    cJSON_AddItemToObject(root, "relaciones", arr_relaciones);

    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt,
                      "SELECT id, nombre, COALESCE(descripcion, ''), COALESCE(fecha_creacion, '') "
                      "FROM coleccion ORDER BY id"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(obj, "nombre", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddStringToObject(obj, "descripcion", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(obj, "fecha_creacion", (const char *)sqlite3_column_text(stmt, 3));
            cJSON_AddItemToArray(arr_colecciones, obj);
        }
        sqlite3_finalize(stmt);
    }

    if (preparar_stmt(&stmt,
                      "SELECT id, tipo, COALESCE(nombre, ''), estado, COALESCE(valor, 0), "
                      "COALESCE(fecha_compra, ''), COALESCE(camiseta_id, 0) "
                      "FROM inventario_item ORDER BY id"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddNumberToObject(obj, "tipo", sqlite3_column_int(stmt, 1));
            cJSON_AddStringToObject(obj, "nombre", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddNumberToObject(obj, "estado", sqlite3_column_int(stmt, 3));
            cJSON_AddNumberToObject(obj, "valor", sqlite3_column_double(stmt, 4));
            cJSON_AddStringToObject(obj, "fecha_compra", (const char *)sqlite3_column_text(stmt, 5));
            cJSON_AddNumberToObject(obj, "camiseta_id", sqlite3_column_int(stmt, 6));
            cJSON_AddItemToArray(arr_inventario, obj);
        }
        sqlite3_finalize(stmt);
    }

    if (preparar_stmt(&stmt,
                      "SELECT coleccion_id, inventario_id, COALESCE(fecha_asociacion, '') "
                      "FROM coleccion_inventario ORDER BY coleccion_id, inventario_id"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "coleccion_id", sqlite3_column_int(stmt, 0));
            cJSON_AddNumberToObject(obj, "inventario_id", sqlite3_column_int(stmt, 1));
            cJSON_AddStringToObject(obj, "fecha_asociacion", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddItemToArray(arr_relaciones, obj);
        }
        sqlite3_finalize(stmt);
    }

    char *json = cJSON_Print(root);
    if (!json)
    {
        cJSON_Delete(root);
        fclose(file);
        printf("No se pudo serializar el backup JSON.\n");
        pause_console();
        return;
    }

    fwrite(json, 1, strlen_s(json, SIZE_MAX), file);
    fclose(file);
    free(json);
    cJSON_Delete(root);

    printf("Backup exportado en: %s\n", path);
    pause_console();
}

static int guardar_item_inventario_importado(int tipo, const char *nombre, int estado,
                                             double valor, const char *fecha_compra, int camiseta_id)
{
    if (estado != INV_ESTADO_NUEVO && estado != INV_ESTADO_USADO)
    {
        estado = INV_ESTADO_NUEVO;
    }

    if (tipo == INV_TIPO_CAMISETA)
    {
        if (camiseta_id <= 0 || !existe_id("camiseta", camiseta_id))
        {
            return 0;
        }

        sqlite3_stmt *stmt;
        if (!preparar_stmt(&stmt,
                           "INSERT INTO inventario_item(tipo, nombre, estado, valor, fecha_compra, camiseta_id) "
                           "VALUES(1, '', ?, ?, ?, ?) "
                           "ON CONFLICT(camiseta_id) DO UPDATE SET "
                           "estado = excluded.estado, valor = excluded.valor, fecha_compra = excluded.fecha_compra"))
        {
            return 0;
        }

        sqlite3_bind_int(stmt, 1, estado);
        sqlite3_bind_double(stmt, 2, valor);
        sqlite3_bind_text(stmt, 3, fecha_compra ? fecha_compra : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, camiseta_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return obtener_id_inventario_por_camiseta(camiseta_id);
    }

    if (tipo != INV_TIPO_BOTINES && tipo != INV_TIPO_ACCESORIO)
    {
        return 0;
    }

    const char *nombre_final = (nombre && nombre[0] != '\0') ? nombre : "Item importado";
    int existente_id = obtener_item_inventario_equivalente(tipo, nombre_final, estado, valor,
                                                            fecha_compra ? fecha_compra : "");
    if (existente_id > 0)
    {
        return existente_id;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "INSERT INTO inventario_item(tipo, nombre, estado, valor, fecha_compra, camiseta_id) "
                       "VALUES(?, ?, ?, ?, ?, NULL)"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, tipo);
    sqlite3_bind_text(stmt, 2, nombre_final, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, estado);
    sqlite3_bind_double(stmt, 4, valor);
    sqlite3_bind_text(stmt, 5, fecha_compra ? fecha_compra : "", -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db);
}

static int importar_colecciones_desde_json(cJSON *colecciones, IdMap *coleccion_map, int map_capacity)
{
    int map_count = 0;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, colecciones)
    {
        if (!cJSON_IsObject(item) || map_count >= map_capacity)
        {
            continue;
        }

        int old_id = cjson_obj_int_or(item, "id", 0);
        const char *nombre = cjson_obj_string_or(item, "nombre", "");
        const char *descripcion = cjson_obj_string_or(item, "descripcion", "");
        const char *fecha_creacion = cjson_obj_string_or(item, "fecha_creacion", "");

        if (nombre[0] == '\0')
        {
            continue;
        }

        int new_id = guardar_coleccion_importada(nombre, descripcion, fecha_creacion);
        if (new_id > 0)
        {
            coleccion_map[map_count].old_id = old_id;
            coleccion_map[map_count].new_id = new_id;
            map_count++;
        }
    }

    return map_count;
}

static int importar_inventario_desde_json(cJSON *inventario, IdMap *inventario_map, int map_capacity)
{
    int map_count = 0;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, inventario)
    {
        if (!cJSON_IsObject(item) || map_count >= map_capacity)
        {
            continue;
        }

        int old_id = cjson_obj_int_or(item, "id", 0);
        int tipo = cjson_obj_int_or(item, "tipo", 0);
        const char *nombre = cjson_obj_string_or(item, "nombre", "");
        int estado = cjson_obj_int_or(item, "estado", 0);
        double valor = cjson_obj_double_or(item, "valor", 0.0);
        const char *fecha_compra = cjson_obj_string_or(item, "fecha_compra", "");
        int camiseta_id = cjson_obj_int_or(item, "camiseta_id", 0);

        int new_id = guardar_item_inventario_importado(tipo, nombre, estado, valor, fecha_compra, camiseta_id);
        if (new_id > 0)
        {
            inventario_map[map_count].old_id = old_id;
            inventario_map[map_count].new_id = new_id;
            map_count++;
        }
    }

    return map_count;
}

static int resolver_id_map_o_existente(const IdMap *map, int map_count, const char *tabla, int old_id)
{
    int new_id = map_get_new_id(map, map_count, old_id);
    if (new_id > 0)
    {
        return new_id;
    }

    if (old_id > 0 && existe_id(tabla, old_id))
    {
        return old_id;
    }

    return 0;
}

static int importar_relaciones_desde_json(cJSON *relaciones,
                                          const IdMap *coleccion_map,
                                          int coleccion_map_count,
                                          const IdMap *inventario_map,
                                          int inventario_map_count)
{
    int relaciones_agregadas = 0;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, relaciones)
    {
        if (!cJSON_IsObject(item))
        {
            continue;
        }

        int old_coleccion_id = cjson_obj_int_or(item, "coleccion_id", 0);
        int old_inventario_id = cjson_obj_int_or(item, "inventario_id", 0);

        int new_coleccion_id = resolver_id_map_o_existente(coleccion_map,
                                                            coleccion_map_count,
                                                            "coleccion",
                                                            old_coleccion_id);

        int new_inventario_id = resolver_id_map_o_existente(inventario_map,
                                                            inventario_map_count,
                                                            "inventario_item",
                                                            old_inventario_id);

        if (new_coleccion_id <= 0 || new_inventario_id <= 0)
        {
            continue;
        }

        sqlite3_stmt *stmt_rel;
        if (!preparar_stmt(&stmt_rel,
                           "INSERT OR IGNORE INTO coleccion_inventario(coleccion_id, inventario_id) "
                           "VALUES(?, ?)"))
        {
            continue;
        }

        sqlite3_bind_int(stmt_rel, 1, new_coleccion_id);
        sqlite3_bind_int(stmt_rel, 2, new_inventario_id);
        if (sqlite3_step(stmt_rel) == SQLITE_DONE)
        {
            relaciones_agregadas += sqlite3_changes(db);
        }
        sqlite3_finalize(stmt_rel);
    }

    return relaciones_agregadas;
}

static void importar_backup_colecciones_json(void)
{
    clear_screen();
    print_header("IMPORTAR BACKUP COLECCIONES");

    char filename[128] = {0};
    input_string("Archivo a importar desde carpeta Importaciones (Enter para colecciones_inventario.json): ",
                 filename,
                 (int)sizeof(filename));
    trim_whitespace(filename);
    if (filename[0] == '\0')
    {
        strcpy_s(filename, sizeof(filename), COLECCIONES_BACKUP_DEFAULT);
    }

    const char *import_dir = get_import_dir();
    if (!import_dir)
    {
        printf("No se pudo resolver la carpeta de importaciones.\n");
        pause_console();
        return;
    }

    char full_path[1024] = {0};
    app_build_path(full_path, sizeof(full_path), import_dir, filename);

    char *json_text = leer_archivo_completo(full_path);
    if (!json_text)
    {
        printf("No se pudo leer el archivo: %s\n", full_path);
        pause_console();
        return;
    }

    cJSON *root = cJSON_Parse(json_text);
    free(json_text);
    if (!root)
    {
        printf("JSON invalido o corrupto.\n");
        pause_console();
        return;
    }

    cJSON *colecciones = cJSON_GetObjectItemCaseSensitive(root, "colecciones");
    cJSON *inventario = cJSON_GetObjectItemCaseSensitive(root, "inventario");
    cJSON *relaciones = cJSON_GetObjectItemCaseSensitive(root, "relaciones");

    if (!cJSON_IsArray(colecciones) || !cJSON_IsArray(inventario) || !cJSON_IsArray(relaciones))
    {
        cJSON_Delete(root);
        printf("Formato de backup no compatible.\n");
        pause_console();
        return;
    }

    int colecciones_size = cJSON_GetArraySize(colecciones);
    int inventario_size = cJSON_GetArraySize(inventario);

    IdMap *coleccion_map = (IdMap *)calloc((size_t)(colecciones_size > 0 ? colecciones_size : 1), sizeof(IdMap));
    IdMap *inventario_map = (IdMap *)calloc((size_t)(inventario_size > 0 ? inventario_size : 1), sizeof(IdMap));
    if (!coleccion_map || !inventario_map)
    {
        free(coleccion_map);
        free(inventario_map);
        cJSON_Delete(root);
        printf("Memoria insuficiente para importar backup.\n");
        pause_console();
        return;
    }

    sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", NULL, NULL, NULL);

    int coleccion_map_count = importar_colecciones_desde_json(colecciones, coleccion_map, colecciones_size);
    int inventario_map_count = importar_inventario_desde_json(inventario, inventario_map, inventario_size);
    int relaciones_agregadas = importar_relaciones_desde_json(relaciones,
                                                              coleccion_map,
                                                              coleccion_map_count,
                                                              inventario_map,
                                                              inventario_map_count);

    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

    free(coleccion_map);
    free(inventario_map);
    cJSON_Delete(root);

    printf("Importacion completada. Colecciones procesadas: %d | Items procesados: %d | Relaciones nuevas: %d\n",
           coleccion_map_count,
           inventario_map_count,
           relaciones_agregadas);
    pause_console();
}

void menu_colecciones_inventario(void)
{
    MenuItem items[] =
    {
        {1, "Crear item de inventario", crear_item_inventario},
        {2, "Listar inventario", listar_inventario_completo},
        {3, "Sincronizar camisetas al inventario", sincronizar_camisetas_a_inventario},
        {4, "Crear coleccion", crear_coleccion},
        {5, "Listar colecciones", listar_colecciones_completo},
        {6, "Agregar item a coleccion", agregar_item_a_coleccion},
        {7, "Quitar item de coleccion", quitar_item_de_coleccion},
        {8, "Ver items por coleccion", ver_items_por_coleccion},
        {9, "Filtrar y buscar inventario", filtrar_buscar_inventario},
        {10, "Exportar backup JSON", exportar_backup_colecciones_json},
        {11, "Importar backup JSON", importar_backup_colecciones_json},
        {0, "Volver", NULL}
    };

    ejecutar_menu("COLECCIONES E INVENTARIO", items, 12);
}
