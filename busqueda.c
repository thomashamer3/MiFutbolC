#include "busqueda.h"
#include "db.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, 0) == SQLITE_OK;
}

static int copiar_cadena_segura(char *dest, size_t dest_size, const char *src)
{
    if (!dest || !src || dest_size == 0)
    {
        return 1;
    }

#if defined(__STDC_LIB_EXT1__)
    return strncpy_s(dest, dest_size, src, dest_size - 1);
#elif defined(_MSC_VER)
    return strncpy_s(dest, dest_size, src, _TRUNCATE);
#else
    snprintf(dest, dest_size, "%s", src);
    return 0;
#endif
}

static size_t longitud_segura(const char *texto, size_t max_len)
{
    if (!texto)
    {
        return 0;
    }

#if defined(__STDC_LIB_EXT1__)
    return strlen_s(texto, max_len);
#elif defined(_MSC_VER)
    return strnlen_s(texto, max_len);
#else
    size_t i = 0;
    while (i < max_len && texto[i] != '\0')
    {
        i++;
    }
    return i;
#endif
}

static void to_lowercase(char *str)
{
    for (int i = 0; str[i]; i++)
    {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

static const char *linea_separadora_busqueda(void)
{
    if (consola_soporta_unicode())
    {
        return "────────────────────────────────────────────────────────";
    }
    return "--------------------------------------------------------";
}

static int construir_patron_busqueda(const char *termino, char *patron_lower,
                                     size_t patron_size)
{
    char patron[256];
    snprintf(patron, sizeof(patron), "%%%s%%", termino);

    if (copiar_cadena_segura(patron_lower, patron_size, patron) != 0)
    {
        patron_lower[0] = '\0';
        return 0;
    }

    to_lowercase(patron_lower);
    return 1;
}

static const char *texto_sqlite_o_default(const unsigned char *texto,
        const char *defecto)
{
    return texto ? (const char *)texto : defecto;
}

typedef void (*imprimir_fila_fn)(sqlite3_stmt *stmt);

#define BUSQUEDA_POR_PAGINA 20

static void busqueda_imprimir_titulo(const char *titulo_unicode,
                                     const char *titulo_ascii)
{
    printf("\n  %s\n", consola_soporta_unicode() ? titulo_unicode : titulo_ascii);
    printf("  %s\n", linea_separadora_busqueda());
}

static int busqueda_contar_total(const char *sql_count, const char *patron,
                                 int cantidad_binds)
{
    int total = 0;
    sqlite3_stmt *stmt_count;

    if (!preparar_stmt(sql_count, &stmt_count))
    {
        return 0;
    }

    for (int i = 1; i <= cantidad_binds; i++)
    {
        sqlite3_bind_text(stmt_count, i, patron, -1, SQLITE_TRANSIENT);
    }

    if (sqlite3_step(stmt_count) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt_count, 0);
    }

    sqlite3_finalize(stmt_count);
    return total;
}

static int busqueda_mostrar_pagina(const char *sql_data, const char *patron,
                                   int cantidad_binds, int pagina, int paginas,
                                   int total, imprimir_fila_fn imprimir_fila)
{
    int offset = (pagina - 1) * BUSQUEDA_POR_PAGINA;
    int hasta = offset + BUSQUEDA_POR_PAGINA;
    sqlite3_stmt *stmt;

    if (!preparar_stmt(sql_data, &stmt))
    {
        return 0;
    }

    for (int i = 1; i <= cantidad_binds; i++)
    {
        sqlite3_bind_text(stmt, i, patron, -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, cantidad_binds + 1, BUSQUEDA_POR_PAGINA);
    sqlite3_bind_int(stmt, cantidad_binds + 2, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        imprimir_fila(stmt);
    }

    sqlite3_finalize(stmt);

    if (hasta > total)
    {
        hasta = total;
    }

    printf("\n  Pagina %d/%d  (%d-%d de %d)\n", pagina, paginas, offset + 1,
           hasta, total);
    return 1;
}

static int busqueda_leer_movimiento(int pagina, int paginas)
{
    char op[8] = "";

    printf("  [");
    if (pagina < paginas)
    {
        printf("N=siguiente  ");
    }
    if (pagina > 1)
    {
        printf("A=anterior  ");
    }
    printf("0=salir]: ");

    if (!fgets(op, sizeof(op), stdin))
    {
        return 0;
    }

    to_lowercase(op);

    if (op[0] == 'n' && pagina < paginas)
    {
        return 1;
    }
    if (op[0] == 'a' && pagina > 1)
    {
        return -1;
    }
    return 0;
}

static int ejecutar_busqueda_generica(const char *sql_count,
                                      const char *sql_data,
                                      const char *titulo_unicode,
                                      const char *titulo_ascii,
                                      const char *patron, int cantidad_binds,
                                      imprimir_fila_fn imprimir_fila)
{
    int total = busqueda_contar_total(sql_count, patron, cantidad_binds);

    busqueda_imprimir_titulo(titulo_unicode, titulo_ascii);

    if (total == 0)
    {
        printf("  (No se encontraron resultados)\n");
        return 0;
    }

    int paginas = (total + BUSQUEDA_POR_PAGINA - 1) / BUSQUEDA_POR_PAGINA;
    int pagina = 1;

    int continuar = 1;
    while (continuar)
    {
        int movimiento;

        if (!busqueda_mostrar_pagina(sql_data, patron, cantidad_binds, pagina,
                                     paginas, total, imprimir_fila))
        {
            return total;
        }

        if (paginas <= 1)
        {
            continuar = 0;
            continue;
        }

        movimiento = busqueda_leer_movimiento(pagina, paginas);

        if (movimiento > 0)
        {
            pagina++;
            busqueda_imprimir_titulo(titulo_unicode, titulo_ascii);
            continue;
        }

        if (movimiento < 0)
        {
            pagina--;
            busqueda_imprimir_titulo(titulo_unicode, titulo_ascii);
            continue;
        }

        continuar = 0;
    }

    return total;
}

static const char *modalidad_to_texto(int modalidad)
{
    switch (modalidad)
    {
    case 5:
        return "Futbol 5";
    case 7:
        return "Futbol 7";
    case 8:
        return "Futbol 8";
    case 11:
        return "Futbol 11";
    default:
        return "Desconocido";
    }
}

static void imprimir_fila_partido(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *cancha = sqlite3_column_text(stmt, 1);
    const unsigned char *fecha = sqlite3_column_text(stmt, 2);
    const unsigned char *hora = sqlite3_column_text(stmt, 3);
    int goles = sqlite3_column_int(stmt, 4);
    int asistencias = sqlite3_column_int(stmt, 5);
    const unsigned char *camiseta = sqlite3_column_text(stmt, 6);

    if (consola_soporta_unicode())
    {
        printf("  ID %d: %s | %s %s | ⚽%d 🎯%d | 👕%s\n", id,
               texto_sqlite_o_default(cancha, "Sin cancha"),
               texto_sqlite_o_default(fecha, "Sin fecha"),
               texto_sqlite_o_default(hora, "Sin hora"), goles, asistencias,
               texto_sqlite_o_default(camiseta, "Sin camiseta"));
        return;
    }

    printf("  ID %d: %s | %s %s | G:%d A:%d | Camiseta:%s\n", id,
           texto_sqlite_o_default(cancha, "Sin cancha"),
           texto_sqlite_o_default(fecha, "Sin fecha"),
           texto_sqlite_o_default(hora, "Sin hora"), goles, asistencias,
           texto_sqlite_o_default(camiseta, "Sin camiseta"));
}

static void imprimir_fila_equipo(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *nombre = sqlite3_column_text(stmt, 1);
    int modalidad = sqlite3_column_int(stmt, 2);

    printf("  ID %d: %s (%s)\n", id, texto_sqlite_o_default(nombre, "Sin nombre"),
           modalidad_to_texto(modalidad));
}

static void imprimir_fila_camiseta(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *nombre = sqlite3_column_text(stmt, 1);
    const unsigned char *color = sqlite3_column_text(stmt, 2);

    printf("  ID %d: %s (%s)\n", id, texto_sqlite_o_default(nombre, "Sin nombre"),
           texto_sqlite_o_default(color, "Sin color"));
}

static void imprimir_fila_cancha(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *nombre = sqlite3_column_text(stmt, 1);

    printf("  ID %d: %s\n", id, texto_sqlite_o_default(nombre, "Sin nombre"));
}

static void imprimir_fila_jugador(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *nombre = sqlite3_column_text(stmt, 1);
    int num = sqlite3_column_int(stmt, 2);
    int pos = sqlite3_column_int(stmt, 3);

    printf("  ID %d: %s (#%d, Pos: %d)\n", id,
           texto_sqlite_o_default(nombre, "Sin nombre"), num, pos);
}

static void imprimir_fila_lesion(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *jugador = sqlite3_column_text(stmt, 1);
    const unsigned char *tipo = sqlite3_column_text(stmt, 2);
    const unsigned char *desc = sqlite3_column_text(stmt, 3);

    printf("  ID %d: %s - %s (%s)\n", id, texto_sqlite_o_default(jugador, "?"),
           texto_sqlite_o_default(tipo, "?"), texto_sqlite_o_default(desc, "?"));
}

static void imprimir_fila_torneo(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *nombre = sqlite3_column_text(stmt, 1);
    int tipo = sqlite3_column_int(stmt, 2);

    printf("  ID %d: %s (Tipo: %d)\n", id,
           texto_sqlite_o_default(nombre, "Sin nombre"), tipo);
}

static void imprimir_fila_temporada(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *nombre = sqlite3_column_text(stmt, 1);
    int anio = sqlite3_column_int(stmt, 2);

    printf("  ID %d: %s (%d)\n", id, texto_sqlite_o_default(nombre, "Sin nombre"),
           anio);
}

static void imprimir_fila_financiamiento(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *desc = sqlite3_column_text(stmt, 1);
    double monto = sqlite3_column_double(stmt, 2);

    printf("  ID %d: %s ($%.2f)\n", id,
           texto_sqlite_o_default(desc, "Sin descripcion"), monto);
}

static void imprimir_fila_bienestar(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *nombre = sqlite3_column_text(stmt, 1);
    const unsigned char *desc = sqlite3_column_text(stmt, 2);

    printf("  ID %d: %s - %s\n", id, texto_sqlite_o_default(nombre, "Sin nombre"),
           texto_sqlite_o_default(desc, ""));
}

int buscar_en_partidos(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM partido p "
                            "LEFT JOIN camiseta c ON p.camiseta_id = c.id "
                            "LEFT JOIN cancha can ON p.cancha_id = can.id "
                            "WHERE can.nombre COLLATE NOCASE LIKE ? OR c.nombre "
                            "COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT p.id, can.nombre, substr(p.fecha_hora, 1, "
                           "10), substr(p.fecha_hora, 12), "
                           "       p.goles, p.asistencias, c.nombre "
                           "FROM partido p "
                           "LEFT JOIN camiseta c ON p.camiseta_id = c.id "
                           "LEFT JOIN cancha can ON p.cancha_id = can.id "
                           "WHERE can.nombre COLLATE NOCASE LIKE ? OR c.nombre "
                           "COLLATE NOCASE LIKE ? "
                           "ORDER BY p.fecha_hora DESC "
                           "LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "🎯 Partidos encontrados:", "Partidos encontrados:", patron_lower, 2,
               imprimir_fila_partido);
}

int buscar_en_equipos(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM equipo "
                            "WHERE nombre COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, nombre, modalidad FROM equipo "
                           "WHERE nombre COLLATE NOCASE LIKE ? "
                           "ORDER BY nombre LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "👥 Equipos encontrados:", "Equipos encontrados:", patron_lower, 1,
               imprimir_fila_equipo);
}

int buscar_en_camisetas(const char *termino)
{
    const char *sql_count =
        "SELECT COUNT(*) FROM camiseta "
        "WHERE nombre COLLATE NOCASE LIKE ? OR color COLLATE NOCASE LIKE ?;";

    const char *sql_data =
        "SELECT id, nombre, color FROM camiseta "
        "WHERE nombre COLLATE NOCASE LIKE ? OR color COLLATE NOCASE LIKE ? "
        "ORDER BY nombre LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "👕 Camisetas encontradas:", "Camisetas encontradas:", patron_lower, 2,
               imprimir_fila_camiseta);
}

int buscar_en_canchas(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM cancha "
                            "WHERE nombre COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, nombre FROM cancha "
                           "WHERE nombre COLLATE NOCASE LIKE ? "
                           "ORDER BY nombre LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "🏟️  Canchas encontradas:", "Canchas encontradas:", patron_lower, 1,
               imprimir_fila_cancha);
}

int buscar_en_jugadores(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM jugador "
                            "WHERE nombre COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, nombre, numero, posicion FROM jugador "
                           "WHERE nombre COLLATE NOCASE LIKE ? "
                           "ORDER BY nombre LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "👤 Jugadores encontrados:", "Jugadores encontrados:", patron_lower, 1,
               imprimir_fila_jugador);
}

int buscar_en_lesiones(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM lesion "
                            "WHERE jugador COLLATE NOCASE LIKE ? OR tipo COLLATE "
                            "NOCASE LIKE ? OR descripcion COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, jugador, tipo, descripcion FROM lesion "
                           "WHERE jugador COLLATE NOCASE LIKE ? OR tipo COLLATE "
                           "NOCASE LIKE ? OR descripcion COLLATE NOCASE LIKE ? "
                           "ORDER BY fecha DESC LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "🩹 Lesiones encontradas:", "Lesiones encontradas:", patron_lower, 3,
               imprimir_fila_lesion);
}

int buscar_en_torneos(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM torneo "
                            "WHERE nombre COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, nombre, tipo_torneo FROM torneo "
                           "WHERE nombre COLLATE NOCASE LIKE ? "
                           "ORDER BY nombre LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "🏆 Torneos encontrados:", "Torneos encontrados:", patron_lower, 1,
               imprimir_fila_torneo);
}

int buscar_en_temporadas(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM temporada "
                            "WHERE nombre COLLATE NOCASE LIKE ? OR "
                            "COALESCE(descripcion, '') COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, nombre, anio FROM temporada "
                           "WHERE nombre COLLATE NOCASE LIKE ? OR "
                           "COALESCE(descripcion, '') COLLATE NOCASE LIKE ? "
                           "ORDER BY anio DESC LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "📅 Temporadas encontradas:", "Temporadas encontradas:", patron_lower, 2,
               imprimir_fila_temporada);
}

int buscar_en_financiamiento(const char *termino)
{
    const char *sql_count =
        "SELECT COUNT(*) FROM financiamiento "
        "WHERE descripcion COLLATE NOCASE LIKE ? OR COALESCE(item_especifico, "
        "'') COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, descripcion, monto FROM financiamiento "
                           "WHERE descripcion COLLATE NOCASE LIKE ? OR "
                           "COALESCE(item_especifico, '') COLLATE NOCASE LIKE ? "
                           "ORDER BY fecha DESC LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(sql_count, sql_data,
                                      "💰 Financiamiento encontrado:",
                                      "Financiamiento encontrado:", patron_lower,
                                      2, imprimir_fila_financiamiento);
}

int buscar_en_bienestar(const char *termino)
{
    const char *sql_count = "SELECT COUNT(*) FROM bienestar_objetivo "
                            "WHERE descripcion COLLATE NOCASE LIKE ?;";

    const char *sql_data = "SELECT id, descripcion, '' FROM bienestar_objetivo "
                           "WHERE descripcion COLLATE NOCASE LIKE ? "
                           "ORDER BY id LIMIT ? OFFSET ?;";

    char patron_lower[256];
    if (!construir_patron_busqueda(termino, patron_lower, sizeof(patron_lower)))
        return 0;

    return ejecutar_busqueda_generica(
               sql_count, sql_data,
               "💪 Bienestar encontrado:", "Bienestar encontrado:", patron_lower, 1,
               imprimir_fila_bienestar);
}

void buscar_global(const char *termino)
{
    clear_screen();
    print_header("BUSQUEDA GLOBAL");
    const char *linea =
        consola_soporta_unicode()
        ? "════════════════════════════════════════════════════════════════"
        : "================================================================";

    printf("Buscando: \"%s\"\n", termino);
    printf("%s\n\n", linea);

    int total = 0;

    // Buscar en todas las tablas
    total += buscar_en_partidos(termino);
    total += buscar_en_equipos(termino);
    total += buscar_en_camisetas(termino);
    total += buscar_en_canchas(termino);
    total += buscar_en_jugadores(termino);
    total += buscar_en_lesiones(termino);
    total += buscar_en_torneos(termino);
    total += buscar_en_temporadas(termino);
    total += buscar_en_financiamiento(termino);
    total += buscar_en_bienestar(termino);

    printf("\n%s\n", linea);
    printf("Total de resultados: %d\n\n", total);

    pause_console();
}

void menu_busqueda_global()
{
    clear_screen();
    print_header("BUSQUEDA GLOBAL");

    printf("Ingrese termino de busqueda (min. 2 caracteres): ");

    char termino[100];
    if (!fgets(termino, sizeof(termino), stdin))
    {
        return;
    }

    // Eliminar salto de línea
    termino[strcspn(termino, "\n")] = '\0';

    // Validar longitud mínima
    if (longitud_segura(termino, sizeof(termino)) < 2)
    {
        printf("\nEl termino debe tener al menos 2 caracteres.\n");
        pause_console();
        return;
    }

    buscar_global(termino);
}
