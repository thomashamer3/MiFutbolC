#include "reclutamiento.h"
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

#define RECLUTAMIENTO_ESTADOS 6

static const char *g_estados[RECLUTAMIENTO_ESTADOS] = {"Visto",      "Prospecto", "En Seguimiento",
                                                       "Contactado", "Reclutado", "Descartado"
                                                      };

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return db_prepare_stmt(stmt, sql);
}

static const char *estado_to_text(int estado)
{
    if (estado >= 1 && estado <= RECLUTAMIENTO_ESTADOS)
    {
        return g_estados[estado - 1];
    }
    return "Desconocido";
}

static void mostrar_prospecto_detalle(sqlite3_stmt *stmt)
{
    int id = sqlite3_column_int(stmt, 0);
    const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
    int estado = sqlite3_column_int(stmt, 2);
    const char *posicion = (const char *)sqlite3_column_text(stmt, 3);
    const char *equipo_origen = (const char *)sqlite3_column_text(stmt, 4);
    const char *fecha_visto = (const char *)sqlite3_column_text(stmt, 5);
    const char *notas = (const char *)sqlite3_column_text(stmt, 6);

    printf("  %d. %s\n", id, nombre ? nombre : "");
    printf("     Estado: %s\n", estado_to_text(estado));
    if (posicion && posicion[0])
    {
        printf("     Posicion: %s\n", posicion);
    }
    if (equipo_origen && equipo_origen[0])
    {
        printf("     Equipo: %s\n", equipo_origen);
    }
    if (fecha_visto && fecha_visto[0])
    {
        printf("     Visto: %s\n", fecha_visto);
    }
    if (notas && notas[0])
    {
        printf("     Notas: %s\n", notas);
    }
    printf("\n");
}

void reclutamiento_crear(void)
{
    clear_screen();
    print_header("NUEVO PROSPECTO");

    char nombre[256] = {0};
    char posicion[128] = {0};
    char equipo_origen[256] = {0};
    char fecha_visto[64] = {0};
    char notas[1024] = {0};

    input_string("Nombre del jugador: ", nombre, (int)sizeof(nombre));
    if (nombre[0] == '\0')
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    printf("Estado inicial (1=Visto, 2=Prospecto, 3=Seguimiento):\n");
    printf("  1 - Visto\n  2 - Prospecto\n  3 - En Seguimiento\n");
    int estado = input_int_rango("Seleccione estado inicial", 1, 3);

    input_string("Posicion: ", posicion, (int)sizeof(posicion));
    input_string("Equipo de origen: ", equipo_origen, (int)sizeof(equipo_origen));
    input_date("Fecha visto (dd/mm/aaaa): ", fecha_visto, (int)sizeof(fecha_visto));
    input_string("Notas: ", notas, (int)sizeof(notas));

    long long id = obtener_siguiente_id("reclutamiento");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "INSERT INTO reclutamiento(id, nombre, estado, posicion, "
                       "equipo_origen, fecha_visto, notas) VALUES (?,?,?,?,?,?,?)"))
    {
        mostrar_error_operacion("Reclutamiento", "crear");
        return;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 3, estado);
    sqlite3_bind_text(stmt, 4, posicion[0] ? posicion : "", -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 5, equipo_origen[0] ? equipo_origen : "", -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 6, fecha_visto[0] ? fecha_visto : "", -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 7, notas[0] ? notas : "", -1, DB_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Prospecto", "Agregado", nombre);
    }
    else
    {
        mostrar_error_operacion("Reclutamiento", "guardar");
    }
    pause_console();
}

void reclutamiento_listar(void)
{
    if (!hay_registros("reclutamiento"))
    {
        mostrar_no_hay_registros("prospectos");
        pause_console();
        return;
    }

    clear_screen();
    print_header("PROSPECTOS - PIPELINE DE RECLUTAMIENTO");

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT id, nombre, estado, posicion, equipo_origen, "
                       "fecha_visto, notas FROM reclutamiento ORDER BY estado, nombre"))
    {
        return;
    }

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total++;
        mostrar_prospecto_detalle(stmt);
    }
    sqlite3_finalize(stmt);

    printf("Total: %d prospectos\n", total);
    pause_console();
}

static int leer_campo_recluta_estado(int default_val)
{
    printf("Estado (1=Visto, 2=Prospecto, 3=Seguimiento, 4=Contactado, 5=Reclutado, "
           "6=Descartado) [%d]: ",
           default_val);
    int v = input_int("");
    return (v >= 1 && v <= RECLUTAMIENTO_ESTADOS) ? v : default_val;
}

typedef struct
{
    char nombre[256];
    int estado;
    char posicion[128];
    char equipo[256];
    char fecha[64];
    char notas[1024];
} ReclutaActual;

static bool leer_recluta_actual(int id, ReclutaActual *out)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT nombre, estado, posicion, equipo_origen, "
                       "fecha_visto, notas FROM reclutamiento WHERE id = ?"))
    {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return false;
    }

    memset(out, 0, sizeof(*out));
    strncpy_s(out->nombre, sizeof(out->nombre), (const char *)sqlite3_column_text(stmt, 0),
              _TRUNCATE);
    out->estado = sqlite3_column_int(stmt, 1);
    for (int i = 2; i <= 5; i++)
    {
        const char *val = (const char *)sqlite3_column_text(stmt, i);
        if (!val)
        {
            continue;
        }

        switch (i)
        {
        case 2:
            strncpy_s(out->posicion, sizeof(out->posicion), val, _TRUNCATE);
            break;
        case 3:
            strncpy_s(out->equipo, sizeof(out->equipo), val, _TRUNCATE);
            break;
        case 4:
            strncpy_s(out->fecha, sizeof(out->fecha), val, _TRUNCATE);
            break;
        case 5:
            strncpy_s(out->notas, sizeof(out->notas), val, _TRUNCATE);
            break;
        default:
            break;
        }
    }
    sqlite3_finalize(stmt);
    return true;
}

typedef struct
{
    char nombre[256];
    char posicion[128];
    char equipo[256];
    char fecha[64];
    char notas[1024];
    int estado;
} ReclutaInput;

static ReclutaInput pedir_datos_recluta(const ReclutaActual *actual)
{
    ReclutaInput in;
    memset(&in, 0, sizeof(in));

    printf("Editando: %s (estado actual: %s)\n\n", actual->nombre, estado_to_text(actual->estado));

    input_string_default("Nombre", actual->nombre, in.nombre, (int)sizeof(in.nombre));
    in.estado = leer_campo_recluta_estado(actual->estado);
    input_string_default("Posicion", actual->posicion, in.posicion, (int)sizeof(in.posicion));
    input_string_default("Equipo origen", actual->equipo, in.equipo, (int)sizeof(in.equipo));
    input_date_default("Fecha visto", actual->fecha, in.fecha, (int)sizeof(in.fecha));
    input_string_default("Notas", actual->notas, in.notas, (int)sizeof(in.notas));

    return in;
}

static void actualizar_recluta_en_db(int id, const ReclutaInput *in)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "UPDATE reclutamiento SET nombre=?, estado=?, posicion=?, "
                       "equipo_origen=?, fecha_visto=?, notas=? WHERE id=?"))
    {
        mostrar_error_operacion("Reclutamiento", "actualizar");
        return;
    }

    sqlite3_bind_text(stmt, 1, in->nombre, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 2, in->estado);
    sqlite3_bind_text(stmt, 3, in->posicion, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 4, in->equipo, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 5, in->fecha, -1, DB_TRANSIENT);
    sqlite3_bind_text(stmt, 6, in->notas, -1, DB_TRANSIENT);
    sqlite3_bind_int(stmt, 7, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Prospecto", "Editado", in->nombre);
    }
    else
    {
        mostrar_error_operacion("Reclutamiento", "editar");
    }
}

void reclutamiento_editar(void)
{
    if (!hay_registros("reclutamiento"))
    {
        mostrar_no_hay_registros("prospectos");
        pause_console();
        return;
    }

    listar_entidades("reclutamiento", "EDITAR PROSPECTO", "No hay prospectos");

    int id = input_int("ID del prospecto a editar (0=cancelar): ");
    if (id <= 0 || !existe_id("reclutamiento", id))
    {
        if (id > 0)
            mostrar_no_existe("Prospecto");
        pause_console();
        return;
    }

    ReclutaActual actual;
    if (!leer_recluta_actual(id, &actual))
    {
        mostrar_no_existe("Prospecto");
        pause_console();
        return;
    }

    ReclutaInput in = pedir_datos_recluta(&actual);
    actualizar_recluta_en_db(id, &in);
    pause_console();
}

void reclutamiento_avanzar(void)
{
    if (!hay_registros("reclutamiento"))
    {
        mostrar_no_hay_registros("prospectos");
        pause_console();
        return;
    }

    listar_entidades("reclutamiento", "AVANZAR PROSPECTO", "No hay prospectos");

    int id = input_int("ID del prospecto a avanzar (0=cancelar): ");
    if (id <= 0 || !existe_id("reclutamiento", id))
    {
        if (id > 0)
        {
            mostrar_no_existe("Prospecto");
        }
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT nombre, estado FROM reclutamiento WHERE id = ?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    char nombre[256] = {0};
    int estado_actual = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strncpy_s(nombre, sizeof(nombre), (const char *)sqlite3_column_text(stmt, 0), _TRUNCATE);
        estado_actual = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (estado_actual >= RECLUTAMIENTO_ESTADOS)
    {
        printf("%s ya esta en el ultimo estado: %s\n", nombre, estado_to_text(estado_actual));
        pause_console();
        return;
    }

    int nuevo_estado = estado_actual + 1;
    printf("Avanzar '%s' de '%s' a '%s'?\n", nombre, estado_to_text(estado_actual),
           estado_to_text(nuevo_estado));

    if (!confirmar("Confirmar avance"))
    {
        printf("Cancelado.\n");
        pause_console();
        return;
    }

    if (!preparar_stmt(&stmt, "UPDATE reclutamiento SET estado = ? WHERE id = ?"))
    {
        mostrar_error_operacion("Reclutamiento", "avanzar");
        return;
    }

    sqlite3_bind_int(stmt, 1, nuevo_estado);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        printf("%s avanzado a: %s\n", nombre, estado_to_text(nuevo_estado));
        if (nuevo_estado == 5)
        {
            printf("¡Felicidades! %s ha sido RECLUTADO.\n", nombre);
        }
        else if (nuevo_estado == 6)
        {
            printf("%s ha sido DESCARTADO.\n", nombre);
        }
        app_log_event("RECLUTAMIENTO", nombre);
    }
    else
    {
        mostrar_error_operacion("Reclutamiento", "avanzar");
    }
    pause_console();
}

void reclutamiento_retroceder(void)
{
    if (!hay_registros("reclutamiento"))
    {
        mostrar_no_hay_registros("prospectos");
        pause_console();
        return;
    }

    listar_entidades("reclutamiento", "RETROCEDER PROSPECTO", "No hay prospectos");

    int id = input_int("ID del prospecto a retroceder (0=cancelar): ");
    if (id <= 0 || !existe_id("reclutamiento", id))
    {
        if (id > 0)
        {
            mostrar_no_existe("Prospecto");
        }
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT nombre, estado FROM reclutamiento WHERE id = ?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    char nombre[256] = {0};
    int estado_actual = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strncpy_s(nombre, sizeof(nombre), (const char *)sqlite3_column_text(stmt, 0), _TRUNCATE);
        estado_actual = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (estado_actual <= 1)
    {
        printf("%s ya esta en el primer estado: %s\n", nombre, estado_to_text(estado_actual));
        pause_console();
        return;
    }

    int nuevo_estado = estado_actual - 1;
    printf("Retroceder '%s' de '%s' a '%s'?\n", nombre, estado_to_text(estado_actual),
           estado_to_text(nuevo_estado));

    if (!confirmar("Confirmar retroceso"))
    {
        printf("Cancelado.\n");
        pause_console();
        return;
    }

    if (!preparar_stmt(&stmt, "UPDATE reclutamiento SET estado = ? WHERE id = ?"))
    {
        mostrar_error_operacion("Reclutamiento", "retroceder");
        return;
    }

    sqlite3_bind_int(stmt, 1, nuevo_estado);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        printf("%s retrocedido a: %s\n", nombre, estado_to_text(nuevo_estado));
        app_log_event("RECLUTAMIENTO", nombre);
    }
    else
    {
        mostrar_error_operacion("Reclutamiento", "retroceder");
    }
    pause_console();
}

void reclutamiento_eliminar(void)
{
    if (!hay_registros("reclutamiento"))
    {
        mostrar_no_hay_registros("prospectos");
        pause_console();
        return;
    }

    listar_entidades("reclutamiento", "ELIMINAR PROSPECTO", "No hay prospectos");

    int id = input_int("ID del prospecto a eliminar (0=cancelar): ");
    if (id <= 0 || !existe_id("reclutamiento", id))
    {
        if (id > 0)
        {
            mostrar_no_existe("Prospecto");
        }
        pause_console();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "SELECT nombre FROM reclutamiento WHERE id = ?"))
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

    printf("Eliminar prospecto '%s'? (ID: %d)\n", nombre, id);
    if (!confirmar("Confirmar eliminacion"))
    {
        printf("Cancelado.\n");
        pause_console();
        return;
    }

    if (!preparar_stmt(&stmt, "DELETE FROM reclutamiento WHERE id = ?"))
    {
        mostrar_error_operacion("Reclutamiento", "eliminar");
        return;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        mostrar_alerta_operacion("Prospecto", "Eliminado", nombre);
    }
    else
    {
        mostrar_error_operacion("Reclutamiento", "eliminar");
    }
    pause_console();
}

void reclutamiento_estadisticas(void)
{
    if (!hay_registros("reclutamiento"))
    {
        mostrar_no_hay_registros("prospectos");
        pause_console();
        return;
    }

    clear_screen();
    print_header("ESTADISTICAS DE RECLUTAMIENTO");

    sqlite3_stmt *stmt = NULL;

    for (int i = 1; i <= RECLUTAMIENTO_ESTADOS; i++)
    {
        if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM reclutamiento WHERE estado = ?"))
        {
            continue;
        }
        sqlite3_bind_int(stmt, 1, i);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        printf("  %s: %d\n", g_estados[i - 1], count);
    }

    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM reclutamiento"))
    {
        return;
    }
    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    printf("  ----------------------\n");
    printf("  Total prospectos: %d\n", total);
    pause_console();
}

void menu_reclutamiento(void)
{
    MenuItem items[] = {{1, "Nuevo prospecto", &reclutamiento_crear},
        {2, "Listar prospectos", &reclutamiento_listar},
        {3, "Editar prospecto", &reclutamiento_editar},
        {4, "Avanzar estado", &reclutamiento_avanzar},
        {5, "Retroceder estado", &reclutamiento_retroceder},
        {6, "Eliminar prospecto", &reclutamiento_eliminar},
        {7, "Estadisticas del pipeline", &reclutamiento_estadisticas},
        {0, "Volver", NULL}
    };
    ejecutar_menu("RECLUTAMIENTO - PIPELINE", items, 8);
}
