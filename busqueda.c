/**
 * @file busqueda.c
 * @brief Implementación del sistema de búsqueda global
 */

#include "busqueda.h"
#include "db.h"
#include "utils.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/**
 * @brief Convierte una cadena a minúsculas para búsqueda case-insensitive
 */
static void to_lowercase(char *str)
{
    for (int i = 0; str[i]; i++)
    {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
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

/**
 * @brief Busca en partidos
 */
int buscar_en_partidos(const char *termino)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT p.id, p.cancha, p.fecha, p.hora, p.goles, p.asistencias, c.nombre "
        "FROM partido p "
        "LEFT JOIN camiseta c ON p.id_camiseta = c.id "
        "WHERE LOWER(p.cancha) LIKE ? OR LOWER(c.nombre) LIKE ? "
        "ORDER BY p.fecha DESC, p.hora DESC;";

    char patron[256];
    snprintf(patron, sizeof(patron), "%%%s%%", termino);

    char patron_lower[256];
    if (copiar_cadena_segura(patron_lower, sizeof(patron_lower), patron) != 0)
    {
        patron_lower[0] = '\0';
    }
    to_lowercase(patron_lower);

    int count = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, patron_lower, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, patron_lower, -1, SQLITE_TRANSIENT);

    printf("\n  🎯 Partidos encontrados:\n");
    printf("  %s\n", "────────────────────────────────────────────────────────");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *cancha = sqlite3_column_text(stmt, 1);
        const unsigned char *fecha = sqlite3_column_text(stmt, 2);
        const unsigned char *hora = sqlite3_column_text(stmt, 3);
        int goles = sqlite3_column_int(stmt, 4);
        int asistencias = sqlite3_column_int(stmt, 5);
        const unsigned char *camiseta = sqlite3_column_text(stmt, 6);

        printf("  ID %d: %s | %s %s | ⚽%d 🎯%d | 👕%s\n",
               id, cancha, fecha, hora, goles, asistencias,
               camiseta ? (const char*)camiseta : "Sin camiseta");
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        printf("  (No se encontraron resultados)\n");
    }

    return count;
}

/**
 * @brief Busca en equipos
 */
int buscar_en_equipos(const char *termino)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, nombre, modalidad "
        "FROM equipo "
        "WHERE LOWER(nombre) LIKE ? "
        "ORDER BY nombre;";

    char patron[256];
    snprintf(patron, sizeof(patron), "%%%s%%", termino);

    char patron_lower[256];
    if (copiar_cadena_segura(patron_lower, sizeof(patron_lower), patron) != 0)
    {
        patron_lower[0] = '\0';
    }
    to_lowercase(patron_lower);

    int count = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, patron_lower, -1, SQLITE_TRANSIENT);

    printf("\n  👥 Equipos encontrados:\n");
    printf("  %s\n", "────────────────────────────────────────────────────────");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *nombre = sqlite3_column_text(stmt, 1);
        int modalidad = sqlite3_column_int(stmt, 2);

        printf("  ID %d: %s (%s)\n", id, nombre, modalidad_to_texto(modalidad));
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        printf("  (No se encontraron resultados)\n");
    }

    return count;
}

/**
 * @brief Busca en camisetas
 */
int buscar_en_camisetas(const char *termino)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, nombre, color "
        "FROM camiseta "
        "WHERE LOWER(nombre) LIKE ? OR LOWER(color) LIKE ? "
        "ORDER BY nombre;";

    char patron[256];
    snprintf(patron, sizeof(patron), "%%%s%%", termino);

    char patron_lower[256];
    if (copiar_cadena_segura(patron_lower, sizeof(patron_lower), patron) != 0)
    {
        patron_lower[0] = '\0';
    }
    to_lowercase(patron_lower);

    int count = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, patron_lower, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, patron_lower, -1, SQLITE_TRANSIENT);

    printf("\n  👕 Camisetas encontradas:\n");
    printf("  %s\n", "────────────────────────────────────────────────────────");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *nombre = sqlite3_column_text(stmt, 1);
        const unsigned char *color = sqlite3_column_text(stmt, 2);

        printf("  ID %d: %s (%s)\n", id, nombre, color);
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        printf("  (No se encontraron resultados)\n");
    }

    return count;
}

/**
 * @brief Busca en canchas
 */
int buscar_en_canchas(const char *termino)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, nombre "
        "FROM cancha "
        "WHERE LOWER(nombre) LIKE ? "
        "ORDER BY nombre;";

    char patron[256];
    snprintf(patron, sizeof(patron), "%%%s%%", termino);

    char patron_lower[256];
    if (copiar_cadena_segura(patron_lower, sizeof(patron_lower), patron) != 0)
    {
        patron_lower[0] = '\0';
    }
    to_lowercase(patron_lower);

    int count = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, patron_lower, -1, SQLITE_TRANSIENT);

    printf("\n  🏟️  Canchas encontradas:\n");
    printf("  %s\n", "────────────────────────────────────────────────────────");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *nombre = sqlite3_column_text(stmt, 1);

        printf("  ID %d: %s\n", id, nombre);
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        printf("  (No se encontraron resultados)\n");
    }

    return count;
}

/**
 * @brief Búsqueda global en todas las tablas
 */
void buscar_global(const char *termino)
{
    clear_screen();
    print_header("BUSQUEDA GLOBAL");

    printf("Buscando: \"%s\"\n", termino);
    printf("════════════════════════════════════════════════════════════════\n\n");

    int total = 0;

    // Buscar en todas las tablas
    total += buscar_en_partidos(termino);
    total += buscar_en_equipos(termino);
    total += buscar_en_camisetas(termino);
    total += buscar_en_canchas(termino);

    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("Total de resultados: %d\n\n", total);

    pause_console();
}

/**
 * @brief Menú de búsqueda global
 */
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
