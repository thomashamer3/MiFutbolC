/*
 * Fixture comun de los tests que usan una base SQLite propia: rutas del perfil
 * de prueba en el directorio de datos, borrado de la base con sus -wal/-shm,
 * consultas auxiliares y el arranque/cierre del runner. Es un header (no un .c)
 * porque CMake convierte cada .c de tests/ en un ejecutable. Las funciones son
 * static inline para que cada test use solo las que necesita sin warnings.
 */
#ifndef TEST_DB_FIXTURE_H
#define TEST_DB_FIXTURE_H

#include "unity/unity.h"
#include "db.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define TEST_SEP "\\"
#else
#define TEST_SEP "/"
#endif

static char g_db_path[1024];
static char g_log_path[1024];

/* Este Unity se compila sin precision double (TEST_ASSERT_DOUBLE_WITHIN
 * falla con "Double Precision Disabled"): comparacion propia con tolerancia. */
#define ASSERT_CERCA(esperado, real) assert_cerca((esperado), (real), __LINE__)

static inline void assert_cerca(double esperado, double real, int linea)
{
    if (fabs(esperado - real) > 0.001)
    {
        static char mensaje[128];
        snprintf(mensaje, sizeof(mensaje), "Esperado %.4f, obtenido %.4f", esperado, real);
        UNITY_TEST_FAIL(linea, mensaje);
    }
}

static inline void borrar_base(const char *path)
{
    char aux[1100];
    remove(path);
    snprintf(aux, sizeof(aux), "%s-wal", path);
    remove(aux);
    snprintf(aux, sizeof(aux), "%s-shm", path);
    remove(aux);
}

static inline void exec_ok(const char *sql)
{
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(db, sql, NULL, NULL, NULL));
}

static inline int consultar_int(const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    int valor = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return -1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        valor = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return valor;
}

/* Lee el archivo entero (malloc) y lo borra; @p largo_out puede ser NULL. */
static inline char *leer_y_borrar(const char *path, long *largo_out)
{
    FILE *f = NULL;
    TEST_ASSERT_EQUAL_INT(0, fopen_s(&f, path, "rb"));
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long largo = ftell(f);
    fseek(f, 0, SEEK_SET);
    TEST_ASSERT_TRUE(largo >= 0);
    char *texto = malloc((size_t)largo + 1);
    TEST_ASSERT_NOT_NULL(texto);
    size_t leidos = fread(texto, 1, (size_t)largo, f);
    texto[leidos] = '\0';
    fclose(f);
    remove(path);
    if (largo_out)
    {
        *largo_out = (long)leidos;
    }
    return texto;
}

/* setUp/tearDown minimos: base nueva en cada test y borrada al terminar. */
static inline void abrir_base_limpia(void)
{
    borrar_base(g_db_path);
    TEST_ASSERT_TRUE(db_init());
    TEST_ASSERT_NOT_NULL(db);
}

static inline void cerrar_y_borrar_base(void)
{
    db_close();
    borrar_base(g_db_path);
}

/*
 * Selecciona el perfil de prueba (aisla la base y el log del usuario) y arma
 * g_db_path, g_log_path y, si @p extra no es NULL, la ruta
 * "<datos>/<perfil><sufijo>" del archivo temporal del test. Devuelve 0 si fallo.
 */
static inline int fixture_iniciar(const char *perfil, char *extra, size_t tam_extra,
                                  const char *sufijo)
{
    if (!db_set_active_user(perfil))
    {
        fprintf(stderr, "No se pudo seleccionar el perfil de prueba\n");
        return 0;
    }
    if (!db_init())
    {
        fprintf(stderr, "db_init fallo\n");
        return 0;
    }
    const char *data_dir = get_data_dir();
    if (!data_dir)
    {
        fprintf(stderr, "Sin directorio de datos\n");
        return 0;
    }
    snprintf(g_db_path, sizeof(g_db_path), "%s" TEST_SEP "mifutbol_%s.db", data_dir, perfil);
    snprintf(g_log_path, sizeof(g_log_path), "%s" TEST_SEP "mifutbol_%s.log", data_dir, perfil);
    if (extra)
    {
        snprintf(extra, tam_extra, "%s" TEST_SEP "%s%s", data_dir, perfil, sufijo);
    }
    db_close();
    return 1;
}

/* Cierra Unity, borra el log del perfil de prueba y devuelve la cantidad de fallos. */
static inline int fixture_terminar(void)
{
    int fallos = UNITY_END();
    remove(g_log_path);
    return fallos;
}

#endif
