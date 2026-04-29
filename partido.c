#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include "equipo.h"
#include "ascii_art.h"
#include "entrenador_ia.h"
#include "financiamiento.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#else
#include "compat_windows.h"
#endif
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <process.h>
#else
#include "process.h"
#endif
#include <memory.h>
#include <limits.h>

#ifndef UNUSED
#  if defined(__GNUC__) || defined(__clang__)
#    define UNUSED __attribute__((unused))
#  else
#    define UNUSED
#  endif
#endif

// Prototipos de funciones estaticas usadas antes de su definicion
static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo);
static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo);
static UNUSED void guardar_estadisticas_equipo(const Equipo *equipo, int const *estadisticas, int const *asistencias,
        int resultado, int cancha_id, char const *fecha_simulacion);
static int crear_cancha_inline(void);
static int crear_camiseta_inline(void);

// Declaracion externa para funcion de financiamiento
extern void obtener_fecha_actual(char *fecha);

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

static int mostrar_partidos_desde_stmt(sqlite3_stmt *stmt)
{
    int hay = 0;
    char fecha_formateada[20];

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // Formatear la fecha para visualizacion
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2), fecha_formateada, sizeof(fecha_formateada));

        ui_printf_centered_line("ID: %d", sqlite3_column_int(stmt, 0));
        ui_printf_centered_line("Cancha: %s", sqlite3_column_text(stmt, 1));
        ui_printf_centered_line("Fecha: %s", fecha_formateada);
        ui_printf_centered_line("Goles: %d, Asistencias: %d", sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
        ui_printf_centered_line("Camiseta: %s", sqlite3_column_text(stmt, 5));
        ui_printf_centered_line("Resultado: %s", resultado_to_text(sqlite3_column_int(stmt, 6)));
        ui_printf_centered_line("Rendimiento General: %d/10", sqlite3_column_int(stmt, 7));
        ui_printf_centered_line("Cansancio: %d/10", sqlite3_column_int(stmt, 8));
        ui_printf_centered_line("Estado de Animo: %d/10", sqlite3_column_int(stmt, 9));
        ui_printf_centered_line("Comentario Personal: %s", sqlite3_column_text(stmt, 10) ? (const char *)sqlite3_column_text(stmt, 10) : "N/A");
        ui_printf_centered_line("Clima: %s", clima_to_text(sqlite3_column_int(stmt, 11)));
        ui_printf_centered_line("Dia: %s", dia_to_text(sqlite3_column_int(stmt, 12)));
        ui_printf_centered_line("Precio: %d", sqlite3_column_int(stmt, 13));

        int tipo_partido = sqlite3_column_int(stmt, 14);
        ui_printf_centered_line("Tipo Partido: %s", tipo_partido == 2 ? "FORMAL" : "AMISTOSO");
        if (tipo_partido == 2)
        {
            ui_printf_centered_line("Rival: %s (%s)",
                                    sqlite3_column_text(stmt, 15) ? (const char *)sqlite3_column_text(stmt, 15) : "N/A",
                                    sqlite3_column_text(stmt, 16) ? (const char *)sqlite3_column_text(stmt, 16) : "N/A");
            ui_printf_centered_line("Posicion: %s | Minutos: %d",
                                    sqlite3_column_text(stmt, 17) ? (const char *)sqlite3_column_text(stmt, 17) : "N/A",
                                    sqlite3_column_int(stmt, 18));
            ui_printf_centered_line("Intensidad: %d | Esfuerzo: %d",
                                    sqlite3_column_int(stmt, 19),
                                    sqlite3_column_int(stmt, 20));
            ui_printf_centered_line("Condicion Cancha: %s | Arbitraje: %s",
                                    sqlite3_column_text(stmt, 21) ? (const char *)sqlite3_column_text(stmt, 21) : "N/A",
                                    sqlite3_column_text(stmt, 22) ? (const char *)sqlite3_column_text(stmt, 22) : "N/A");
            ui_printf_centered_line("Eventos Clave: %s",
                                    sqlite3_column_text(stmt, 23) ? (const char *)sqlite3_column_text(stmt, 23) : "N/A");
            ui_printf_centered_line("Ratings T/F/M: %d/%d/%d",
                                    sqlite3_column_int(stmt, 24),
                                    sqlite3_column_int(stmt, 25),
                                    sqlite3_column_int(stmt, 26));
        }
        ui_printf_centered_line("----------------------------------------");
        hay = 1;
    }

    return hay;
}

typedef struct
{
    char rival_nombre[100];
    char tipo_rival[40];
    char posicion_jugada[40];
    int minutos_jugados;
    int intensidad;
    int esfuerzo_percibido;
    char condicion_cancha[60];
    char arbitraje[60];
    char eventos_clave[300];
    int rating_tecnico;
    int rating_fisico;
    int rating_mental;
} DatosPartidoFormal;

typedef struct
{
    int cancha_id;
    int goles;
    int asistencias;
    int camiseta;
    int resultado;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    char comentario_personal[256];
    int clima;
    int dia;
    int precio;
    int tipo_partido;
    DatosPartidoFormal formal;
} DatosPartido;

typedef struct
{
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} EstadisticasPartido;

typedef struct
{
    Equipo equipo_local;
    Equipo equipo_visitante;
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} DatosSimulacion;

#include "random_utils.h"

#ifdef _WIN32

static int secure_rand(int max)
{
    if (max <= 0)
        return 0;
    unsigned char rand_bytes[4];
    if (secure_random_bytes(rand_bytes, sizeof(rand_bytes)) == 0)
    {
        unsigned int r = (rand_bytes[0] << 24) | (rand_bytes[1] << 16) |
                        (rand_bytes[2] << 8) | rand_bytes[3];
        return (int)(r % max);
    }
    return (int)(((unsigned int)(time(NULL) ^ clock())) % max);
}

static int cancha_esta_activa(int cancha_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT IFNULL(activa, 1) FROM cancha WHERE id = ?", &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, cancha_id);
    int activa = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activa = sqlite3_column_int(stmt, 0) == 1;
    }
    sqlite3_finalize(stmt);
    return activa;
}

static int camiseta_esta_activa(int camiseta_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT IFNULL(activa, 1) FROM camiseta WHERE id = ?", &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, camiseta_id);
    int activa = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activa = sqlite3_column_int(stmt, 0) == 1;
    }
    sqlite3_finalize(stmt);
    return activa;
}

static int verificar_prerrequisitos_partido()
{
    sqlite3_stmt *stmt_count_canchas;
    if (!preparar_stmt("SELECT COUNT(*) FROM cancha WHERE IFNULL(activa, 1) = 1", &stmt_count_canchas))
    {
        return 0;
    }
    sqlite3_step(stmt_count_canchas);
    int count_canchas = sqlite3_column_int(stmt_count_canchas, 0);
    sqlite3_finalize(stmt_count_canchas);

    sqlite3_stmt *stmt_count_camisetas;
    if (!preparar_stmt("SELECT COUNT(*) FROM camiseta WHERE IFNULL(activa, 1) = 1", &stmt_count_camisetas))
    {
        return 0;
    }
    sqlite3_step(stmt_count_camisetas);
    int count_camisetas = sqlite3_column_int(stmt_count_camisetas, 0);
    sqlite3_finalize(stmt_count_camisetas);

    if (count_canchas == 0 || count_camisetas == 0)
    {
        if (count_canchas == 0 && count_camisetas == 0)
            printf("No se puede crear un partido porque no hay canchas ni camisetas activas registradas.\n");
        else if (count_canchas == 0)
            printf("No se puede crear un partido porque no hay canchas activas registradas.\n");
        else
            printf("No se puede crear un partido porque no hay camisetas activas registradas.\n");
        pause_console();
        return 0;
    }
    return 1;
}

static void listar_canchas_disponibles()
{
    ui_printf_centered_line("Canchas disponibles:");
    sqlite3_stmt *stmt_canchas;
    if (!preparar_stmt("SELECT id, nombre FROM cancha WHERE IFNULL(activa, 1) = 1 ORDER BY id", &stmt_canchas))
    {
        return;
    }
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d | %s", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);
}

static void listar_camisetas_disponibles()
{
    ui_printf_centered_line("Camisetas disponibles:");
    sqlite3_stmt *stmt_camisetas;
    if (!preparar_stmt("SELECT id, nombre FROM camiseta WHERE IFNULL(activa, 1) = 1 ORDER BY id", &stmt_camisetas))
    {
        return;
    }
    while (sqlite3_step(stmt_camisetas) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d | %s", sqlite3_column_int(stmt_camisetas, 0), sqlite3_column_text(stmt_camisetas, 1));
    }
    sqlite3_finalize(stmt_camisetas);
}

/* Muestra el listado de canchas con la opcion "Nueva Cancha" al final */
static void listar_canchas_con_nueva()
{
    listar_canchas_disponibles();
    ui_printf_centered_line("-1 | [+ Nueva Cancha]");
}

/* Muestra el listado de camisetas con la opcion "Nueva Camiseta" al final */
static void listar_camisetas_con_nueva()
{
    listar_camisetas_disponibles();
    ui_printf_centered_line("-1 | [+ Nueva Camiseta]");
}

static int crear_entidad_inline(const char *tabla,
                                const char *prompt_nombre,
                                int tam_nombre,
                                const char *error_creacion,
                                const char *error_guardado,
                                const char *etiqueta_entidad)
{
    char nombre[100];
    int tam_efectivo = tam_nombre;

    if (tam_efectivo <= 0 || tam_efectivo > (int)sizeof(nombre))
    {
        tam_efectivo = (int)sizeof(nombre);
    }

    input_string(prompt_nombre, nombre, tam_efectivo);
    trim_whitespace(nombre);
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        return 0;
    }

    long long id = obtener_siguiente_id(tabla);
    char sql[96];
    snprintf(sql, sizeof(sql), "INSERT INTO %s(id, nombre) VALUES(?, ?)", tabla);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("%s\n", error_creacion);
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        printf("%s \"%s\" creada con ID %lld.\n", etiqueta_entidad, nombre, id);
        return (int)id;
    }

    printf("%s\n", error_guardado);
    return 0;
}

/* Crea una cancha nueva de forma rapida durante la creacion de un partido.
   Devuelve el nuevo ID, o 0 si fallo. */
static int crear_cancha_inline(void)
{
    return crear_entidad_inline("cancha",
                                "Nombre de la nueva cancha: ",
                                100,
                                "Error al crear la cancha.",
                                "Error al guardar la cancha.",
                                "Cancha");
}

/* Crea una camiseta nueva de forma rapida durante la creacion de un partido.
   Devuelve el nuevo ID, o 0 si fallo. */
static int crear_camiseta_inline(void)
{
    return crear_entidad_inline("camiseta",
                                "Nombre y Numero de la nueva camiseta: ",
                                50,
                                "Error al crear la camiseta.",
                                "Error al guardar la camiseta.",
                                "Camiseta");
}

/* Pide un ID de cancha; si el usuario ingresa -1 ofrece crear una nueva.
   Devuelve 0 si el usuario cancela. */
static int pedir_cancha_o_nueva(int permite_cancelar)
{
    while (1)
    {
        int id = input_int("ID Cancha, (-1 Nueva Cancha, 0 para Cancelar): ");
        if (permite_cancelar && id == 0)
            return 0;
        if (id == -1)
        {
            int nuevo_id = crear_cancha_inline();
            if (nuevo_id > 0)
                return nuevo_id;
            /* Si fallo volver a mostrar lista */
            listar_canchas_con_nueva();
            continue;
        }
        if (existe_id("cancha", id))
        {
            if (cancha_esta_activa(id))
                return id;

            printf("La cancha esta inactiva. Seleccione una cancha activa o cree una nueva.\n");
            continue;
        }
        printf("La cancha no existe. Intente nuevamente.\n");
    }
}

/* Pide un ID de camiseta; si el usuario ingresa -1 ofrece crear una nueva. */
static int pedir_camiseta_o_nueva(void)
{
    while (1)
    {
        int id = input_int("ID Camiseta, (-1 Nueva Camiseta): ");
        if (id == -1)
        {
            int nuevo_id = crear_camiseta_inline();
            if (nuevo_id > 0)
                return nuevo_id;
            listar_camisetas_con_nueva();
            continue;
        }
        if (existe_id("camiseta", id))
        {
            if (camiseta_esta_activa(id))
                return id;

            printf("La camiseta esta inactiva. Seleccione una camiseta activa o cree una nueva.\n");
            continue;
        }
        printf("La camiseta no existe. Intente nuevamente.\n");
    }
}

static void inicializar_datos_partido(DatosPartido *datos)
{
    memset(datos, 0, sizeof(*datos));
    strcpy_s(datos->comentario_personal, sizeof(datos->comentario_personal), "");
    datos->tipo_partido = 1;
    strcpy_s(datos->formal.rival_nombre, sizeof(datos->formal.rival_nombre), "");
    strcpy_s(datos->formal.tipo_rival, sizeof(datos->formal.tipo_rival), "");
    strcpy_s(datos->formal.posicion_jugada, sizeof(datos->formal.posicion_jugada), "");
    datos->formal.minutos_jugados = 0;
    datos->formal.intensidad = 0;
    datos->formal.esfuerzo_percibido = 0;
    strcpy_s(datos->formal.condicion_cancha, sizeof(datos->formal.condicion_cancha), "");
    strcpy_s(datos->formal.arbitraje, sizeof(datos->formal.arbitraje), "");
    strcpy_s(datos->formal.eventos_clave, sizeof(datos->formal.eventos_clave), "");
    datos->formal.rating_tecnico = 0;
    datos->formal.rating_fisico = 0;
    datos->formal.rating_mental = 0;
}

static UNUSED int pedir_id_existente(const char *prompt, const char *tabla,
                                     const char *mensaje_error, int permite_cancelar)
{
    while (1)
    {
        int id = input_int(prompt);
        if (permite_cancelar && id == 0)
        {
            return 0;
        }

        if (existe_id(tabla, id))
        {
            return id;
        }

        printf("%s\n", mensaje_error);
    }
}

static int pedir_entero_minimo(const char *prompt_inicial, int minimo, const char *prompt_error)
{
    int valor = input_int(prompt_inicial);
    while (valor < minimo)
    {
        valor = input_int(prompt_error);
    }
    return valor;
}

static int pedir_entero_en_rango(const char *prompt_inicial, int min, int max, const char *prompt_error)
{
    int valor = input_int(prompt_inicial);
    while (valor < min || valor > max)
    {
        valor = input_int(prompt_error);
    }
    return valor;
}

static void solicitar_texto_no_vacio(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string_extended(prompt, buffer, size);
        trim_whitespace(buffer);
        if (buffer[0] != '\0')
        {
            return;
        }
        printf("El campo no puede estar vacio.\n");
    }
}

static int seleccionar_modalidad_partido(void)
{
    while (1)
    {
        print_header("TIPO DE PARTIDO");
        printf("1) Partido Amistoso (carga clasica)\n");
        printf("2) Partido Formal (carga clasica + detalle ampliado)\n");
        printf("0) Cancelar\n");

        int opcion = input_int("Opcion: ");
        if (opcion == 0 || opcion == 1 || opcion == 2)
        {
            return opcion;
        }
        printf("Opcion invalida.\n");
    }
}

static void solicitar_tipo_rival(char *buffer, int size)
{
    while (1)
    {
        printf("Tipo de rival:\n");
        printf("1) Amistoso\n");
        printf("2) Torneo\n");
        printf("3) Entrenamiento\n");
        printf("4) Otro\n");

        int opcion = input_int("Opcion: ");
        switch (opcion)
        {
        case 1:
            snprintf(buffer, size, "Amistoso");
            return;
        case 2:
            snprintf(buffer, size, "Torneo");
            return;
        case 3:
            snprintf(buffer, size, "Entrenamiento");
            return;
        case 4:
            solicitar_texto_no_vacio("Tipo de rival (texto): ", buffer, size);
            return;
        default:
            printf("Opcion invalida.\n");
            break;
        }
    }
}

static void recopilar_datos_formales(DatosPartido *datos)
{
    solicitar_texto_no_vacio("Rival: ", datos->formal.rival_nombre, sizeof(datos->formal.rival_nombre));
    solicitar_tipo_rival(datos->formal.tipo_rival, sizeof(datos->formal.tipo_rival));
    solicitar_texto_no_vacio("Posicion jugada: ", datos->formal.posicion_jugada, sizeof(datos->formal.posicion_jugada));
    datos->formal.minutos_jugados = pedir_entero_en_rango("Minutos jugados (0-180): ",
                                    0, 180,
                                    "Valor invalido. Ingrese entre 0 y 180: ");
    datos->formal.intensidad = pedir_entero_en_rango("Intensidad del partido (1-10): ",
                               1, 10,
                               "Valor invalido. Ingrese entre 1 y 10: ");
    datos->formal.esfuerzo_percibido = pedir_entero_en_rango("Esfuerzo percibido (1-10): ",
                                       1, 10,
                                       "Valor invalido. Ingrese entre 1 y 10: ");
    solicitar_texto_no_vacio("Condicion de cancha: ", datos->formal.condicion_cancha, sizeof(datos->formal.condicion_cancha));
    solicitar_texto_no_vacio("Arbitraje: ", datos->formal.arbitraje, sizeof(datos->formal.arbitraje));
    input_string_extended("Eventos clave: ", datos->formal.eventos_clave, sizeof(datos->formal.eventos_clave));
    trim_whitespace(datos->formal.eventos_clave);
    if (datos->formal.eventos_clave[0] == '\0')
    {
        snprintf(datos->formal.eventos_clave, sizeof(datos->formal.eventos_clave), "(sin eventos)");
    }
    datos->formal.rating_tecnico = pedir_entero_en_rango("Rating tecnico (1-10): ",
                                   1, 10,
                                   "Valor invalido. Ingrese entre 1 y 10: ");
    datos->formal.rating_fisico = pedir_entero_en_rango("Rating fisico (1-10): ",
                                  1, 10,
                                  "Valor invalido. Ingrese entre 1 y 10: ");
    datos->formal.rating_mental = pedir_entero_en_rango("Rating mental (1-10): ",
                                  1, 10,
                                  "Valor invalido. Ingrese entre 1 y 10: ");
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

static int recopilar_datos_partido(DatosPartido *datos)
{
    if (!datos)
    {
        return 0;
    }

    inicializar_datos_partido(datos);

    listar_canchas_con_nueva();
    datos->cancha_id = pedir_cancha_o_nueva(1);
    if (datos->cancha_id == 0)
    {
        return 0;
    }

    datos->goles = pedir_entero_minimo("Goles: ", 0,
                                       "Goles invalidos. Ingrese 0 o mas: ");
    datos->asistencias = pedir_entero_minimo("Asistencias: ", 0,
                         "Asistencias invalidas. Ingrese 0 o mas: ");
    datos->resultado = pedir_entero_en_rango("Resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ",
                       1, 3,
                       "Resultado invalido. (1=VICTORIA, 2=EMPATE, 3=DERROTA):");

    listar_camisetas_con_nueva();
    datos->camiseta = pedir_camiseta_o_nueva();
    datos->rendimiento_general = pedir_entero_en_rango("Rendimiento general (1-10): ",
                                 1, 10,
                                 "Rendimiento invalido. Ingrese entre 1 y 10: ");
    datos->cansancio = pedir_entero_en_rango("Cansancio (1-10): ",
                       1, 10,
                       "Cansancio invalido. Ingrese entre 1 y 10:  ");
    datos->estado_animo = pedir_entero_en_rango("Estado de Animo (1-10): ",
                          1, 10,
                          "Estado de Animo invalido. Ingrese entre 1 y 10: ");
    input_string("Comentario personal: ", datos->comentario_personal, 256);
    datos->clima = pedir_entero_en_rango("Clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio):",
                                         1, 6,
                                         "Clima invalido (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    datos->dia = pedir_entero_en_rango("Dia (1=Dia, 2=Tarde, 3=Noche): ",
                                       1, 3,
                                       "Dia invalido (1=Dia, 2=Tarde, 3=Noche): ");
    datos->precio = pedir_entero_minimo("Precio del partido: ", 0,
                                        "Precio invalido. Ingrese 0 o mas: ");
    datos->tipo_partido = 1;

    return 1;
}

static int recopilar_datos_partido_formal(DatosPartido *datos)
{
    if (!recopilar_datos_partido(datos))
    {
        return 0;
    }

    datos->tipo_partido = 2;
    recopilar_datos_formales(datos);
    return 1;
}

static void insertar_partido(long long id, DatosPartido const *datos, char const *fecha)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO partido(id, cancha_id,fecha_hora,goles,asistencias,camiseta_id,resultado,rendimiento_general,cansancio,estado_animo,comentario_personal,clima,dia,precio,"
                "tipo_partido,rival_nombre,tipo_rival,posicion_jugada,minutos_jugados,intensidad,esfuerzo_percibido,condicion_cancha,arbitraje,eventos_clave,rating_tecnico,rating_fisico,rating_mental)"
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                &stmt))
    {
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, datos->cancha_id);
    /* Convertir fecha a formato de almacenamiento (YYYY-MM-DD HH:MM) */
    char fecha_storage[64] = {0};
    convert_display_date_to_storage(fecha, fecha_storage, sizeof(fecha_storage));
    sqlite3_bind_text(stmt, 3, fecha_storage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, datos->goles);
    sqlite3_bind_int(stmt, 5, datos->asistencias);
    sqlite3_bind_int(stmt, 6, datos->camiseta);
    sqlite3_bind_int(stmt, 7, datos->resultado);
    sqlite3_bind_int(stmt, 8, datos->rendimiento_general);
    sqlite3_bind_int(stmt, 9, datos->cansancio);
    sqlite3_bind_int(stmt, 10, datos->estado_animo);
    sqlite3_bind_text(stmt, 11, datos->comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, datos->clima);
    sqlite3_bind_int(stmt, 13, datos->dia);
    sqlite3_bind_int(stmt, 14, datos->precio);
    sqlite3_bind_int(stmt, 15, datos->tipo_partido);
    sqlite3_bind_text(stmt, 16, datos->formal.rival_nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 17, datos->formal.tipo_rival, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 18, datos->formal.posicion_jugada, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 19, datos->formal.minutos_jugados);
    sqlite3_bind_int(stmt, 20, datos->formal.intensidad);
    sqlite3_bind_int(stmt, 21, datos->formal.esfuerzo_percibido);
    sqlite3_bind_text(stmt, 22, datos->formal.condicion_cancha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 23, datos->formal.arbitraje, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 24, datos->formal.eventos_clave, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 25, datos->formal.rating_tecnico);
    sqlite3_bind_int(stmt, 26, datos->formal.rating_fisico);
    sqlite3_bind_int(stmt, 27, datos->formal.rating_mental);
    int result = sqlite3_step(stmt);
    if (result == SQLITE_DONE)
    {
        char descripcion[100];
        snprintf(descripcion, sizeof(descripcion), "ID: %lld", id);
        mostrar_alerta_operacion("Partido", "Creado", descripcion);
    }
    else
    {
        printf("Error al crear el partido: %s\n", sqlite3_errmsg(db));
        pause_console();
    }
    sqlite3_finalize(stmt);
}

static void crear_transaccion_partido(long long partido_id, int precio)
{
    // Verificar si el partido ya tiene una transaccion asociada
    sqlite3_stmt *stmt_check;
    const char *sql_check = "SELECT COUNT(*) FROM financiamiento WHERE tipo = 1 AND categoria = 6 AND item_especifico LIKE ?";
    char item_pattern[256];
    snprintf(item_pattern, sizeof(item_pattern), "Partido ID: %lld%%", partido_id);

    if (preparar_stmt(sql_check, &stmt_check))
    {
        sqlite3_bind_text(stmt_check, 1, item_pattern, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt_check) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt_check, 0);
            if (count > 0)
            {
                printf("El partido ya tiene una transaccion financiera asociada.\n");
                sqlite3_finalize(stmt_check);
                return;
            }
        }
        sqlite3_finalize(stmt_check);
    }

    // Crear la transaccion financiera
    TransaccionFinanciera transaccion;
    transaccion.id = (int)obtener_siguiente_id("financiamiento");
    obtener_fecha_actual(transaccion.fecha);
    transaccion.tipo = GASTO;
    transaccion.categoria = CANCHAS;
    transaccion.monto = precio;
    strcpy_s(transaccion.descripcion, sizeof(transaccion.descripcion), "Pago por alquiler de cancha");

    // Obtener detalles del partido para el item_especifico
    sqlite3_stmt *stmt_partido;
    const char *sql_partido = "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                              "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                              "JOIN cancha can ON p.cancha_id = can.id WHERE p.id = ?";

    if (preparar_stmt(sql_partido, &stmt_partido))
    {
        sqlite3_bind_int64(stmt_partido, 1, partido_id);

        if (sqlite3_step(stmt_partido) == SQLITE_ROW)
        {
            // Formatear la fecha para visualizacion
            char fecha_formateada[20];
            format_date_for_display((const char *)sqlite3_column_text(stmt_partido, 2), fecha_formateada, sizeof(fecha_formateada));

            // Crear el string con los detalles del partido
            snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "(%lld |Cancha:%s |Fecha:%s | G:%d A:%d |Camiseta:%s | %s)",
                     sqlite3_column_int64(stmt_partido, 0),
                     sqlite3_column_text(stmt_partido, 1),
                     fecha_formateada,
                     sqlite3_column_int(stmt_partido, 3),
                     sqlite3_column_int(stmt_partido, 4),
                     sqlite3_column_text(stmt_partido, 5),
                     resultado_to_text(sqlite3_column_int(stmt_partido, 6)));
        }
        else
        {
            snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "Partido ID: %lld (no encontrado)", partido_id);
        }
        sqlite3_finalize(stmt_partido);
    }
    else
    {
        snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "Partido ID: %lld", partido_id);
    }

    // Insertar la transaccion en la base de datos
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO financiamiento (id, fecha, tipo, categoria, descripcion, monto, item_especifico) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, transaccion.id);
        sqlite3_bind_text(stmt, 2, transaccion.fecha, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, transaccion.tipo);
        sqlite3_bind_int(stmt, 4, transaccion.categoria);
        sqlite3_bind_text(stmt, 5, transaccion.descripcion, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, transaccion.monto);
        sqlite3_bind_text(stmt, 7, transaccion.item_especifico, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Transaccion financiera creada para el partido con ID %lld\n", partido_id);
        }
        else
        {
            printf("Error al crear la transaccion financiera: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        printf("Error al preparar la consulta de transaccion: %s\n", sqlite3_errmsg(db));
    }
}

void crear_partido()
{
    // Activar IA antes de crear partido
    activar_ia_antes_partido();

    if (!verificar_prerrequisitos_partido())
        return;

    int modalidad = seleccionar_modalidad_partido();
    if (modalidad == 0)
    {
        return;
    }

    DatosPartido datos;
    int datos_ok = (modalidad == 1)
                   ? recopilar_datos_partido(&datos)
                   : recopilar_datos_partido_formal(&datos);
    if (!datos_ok)
        return;

    char fecha[20];
    printf("\nFecha y Hora del partido (dd/mm/yyyy hh:mm):\n");
    printf("(Presione Enter para usar fecha/hora actual): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;

    // Si el usuario presiona Enter (string vacío), usar fecha/hora actual
    if (fecha[0] == '\0' || (fecha[0] == ' ' && fecha[1] == '\0'))
    {
        get_datetime(fecha, sizeof(fecha));
        printf("Usando fecha/hora actual: %s\n", fecha);
    }
    else
    {
        // Validar que tenga al menos un formato básico
        trim_whitespace(fecha);
        if (longitud_segura(fecha, sizeof(fecha)) < 10)
        {
            printf("Formato de fecha invalido. Usando fecha/hora actual.\n");
            get_datetime(fecha, sizeof(fecha));
        }
        printf("Fecha/hora ingresada: %s\n", fecha);
    }

    long long id = obtener_siguiente_id("partido");
    insertar_partido(id, &datos, fecha);

    // Crear transaccion financiera si el precio es mayor a 0
    if (datos.precio > 0)
    {
        crear_transaccion_partido(id, datos.precio);
    }
}

void listar_partidos()
{
    clear_screen();
    print_header("LISTADO DE PARTIDOS");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio, "
                "IFNULL(p.tipo_partido, 1), IFNULL(p.rival_nombre, ''), IFNULL(p.tipo_rival, ''), IFNULL(p.posicion_jugada, ''), "
                "IFNULL(p.minutos_jugados, 0), IFNULL(p.intensidad, 0), IFNULL(p.esfuerzo_percibido, 0), IFNULL(p.condicion_cancha, ''), "
                "IFNULL(p.arbitraje, ''), IFNULL(p.eventos_clave, ''), IFNULL(p.rating_tecnico, 0), IFNULL(p.rating_fisico, 0), IFNULL(p.rating_mental, 0) "
                "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                "JOIN cancha can ON p.cancha_id = can.id ORDER BY p.id ASC",
                &stmt))
    {
        pause_console();
        return;
    }

    int hay = mostrar_partidos_desde_stmt(stmt);

    if (!hay)
        ui_printf_centered_line("No hay partidos cargados.");

    sqlite3_finalize(stmt);
    pause_console();
}

void eliminar_partido()
{
    print_header("ELIMINAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Eliminar (0 para cancelar): ");

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        return;
    }

    if (!confirmar("Seguro que desea eliminar este partido?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt("DELETE FROM partido WHERE id = ?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    mostrar_alerta_operacion("Partido", "Eliminado", NULL);
}

static int current_partido_id;

static void modificar_campo_partido(const char *campo, const char *prompt, const char *mensaje_exito,
                                    int min_val, int max_val, void (*mostrar_lista)(void))
{
    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (min_val != 0 || max_val != 0)
    {
        while (valor < min_val || valor > max_val)
        {
            char prompt_error[256];
            snprintf(prompt_error, sizeof(prompt_error), "Valor invalido. Ingrese entre %d y %d: ", min_val, max_val);
            valor = input_int(prompt_error);
        }
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    pause_console();
}

static void modificar_campo_texto_partido(const char *campo, const char *prompt, const char *mensaje_exito, int buffer_size)
{
    char valor[buffer_size];
    printf("%s", prompt);
    size_t valor_size = sizeof(valor);
    if (valor_size > INT_MAX)
    {
        return;
    }
    fgets(valor, (int)valor_size, stdin);
    valor[strcspn(valor, "\n")] = 0;

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    pause_console();
}

static void buscar_partidos_generico(const char *header, const char *campo, const char *prompt,
                                     void (*mostrar_lista)(void), int validar_id)
{
    print_header(header);

    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (validar_id && !existe_id(campo, valor))
    {
        printf("El %s no existe.\n", campo);
        pause_console();
        return;
    }

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio "
             "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
             "JOIN cancha can ON p.cancha_id = can.id "
             "WHERE p.%s = ?",
             campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);

    int hay = mostrar_partidos_desde_stmt(stmt);

    if (!hay)
        printf("No se encontraron partidos con ese criterio.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

static void modificar_cancha_partido()
{
    listar_canchas_disponibles();
    int cancha = 0;
    while (1)
    {
        cancha = input_int("Nuevo ID Cancha (0 para cancelar): ");
        if (cancha == 0)
            return;
        if (existe_id("cancha", cancha) && cancha_esta_activa(cancha))
            break;
        printf("La cancha no existe o esta inactiva. Intente nuevamente.\n");
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET cancha_id=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, cancha);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Cancha Modificada", NULL);
}

static void modificar_fecha_hora_partido()
{
    char fecha[20];
    char hora[10];
    char fecha_hora[30];
    printf("Nueva fecha (dd/mm/yyyy): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;
    printf("Nueva hora (hh:mm): ");
    fgets(hora, sizeof(hora), stdin);
    hora[strcspn(hora, "\n")] = 0;
    snprintf(fecha_hora, sizeof(fecha_hora), "%s %s", fecha, hora);
    /* Convertir a formato de almacenamiento antes de actualizar */
    char fecha_storage[64] = {0};
    convert_display_date_to_storage(fecha_hora, fecha_storage, sizeof(fecha_storage));
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET fecha_hora=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, fecha_storage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Fecha y Hora Modificadas", NULL);
}

static void modificar_goles_partido()
{
    modificar_campo_partido("goles", "Nuevos goles: ", "Goles modificados correctamente", 0, 0, NULL);
}

static void modificar_asistencias_partido()
{
    modificar_campo_partido("asistencias", "Nuevas asistencias: ", "Asistencias modificadas correctamente", 0, 0, NULL);
}

static void modificar_resultado_partido()
{
    modificar_campo_partido("resultado", "Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ", "Resultado modificado correctamente", 1, 3, NULL);
}

static void modificar_camiseta_partido()
{
    listar_camisetas_disponibles();
    int camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", camiseta) || !camiseta_esta_activa(camiseta))
    {
        printf("La camiseta no existe o esta inactiva\n");
        return;
    }
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET camiseta_id=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, camiseta);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Camiseta Modificada", NULL);
}

static void modificar_clima_partido()
{
    modificar_campo_partido("clima", "Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ", "Clima modificado correctamente", 1, 6, NULL);
}

static void modificar_dia_partido()
{
    modificar_campo_partido("dia", "Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ", "Dia modificado correctamente", 1, 3, NULL);
}

static void modificar_comentario_partido()
{
    modificar_campo_texto_partido("comentario_personal", "Nuevo comentario personal: ", "Comentario modificado correctamente", 256);
}

static void modificar_precio_partido()
{
    modificar_campo_partido("precio", "Nuevo precio del partido: ", "Precio modificado correctamente", 0, 0, NULL);
}

static int recopilar_datos_completos_partido(DatosPartido *datos)
{
    if (!datos)
        return 0;

    memset(datos, 0, sizeof(*datos));

    listar_canchas_disponibles();
    datos->cancha_id = input_int("Nuevo ID Cancha: ");
    if (!existe_id("cancha", datos->cancha_id) || !cancha_esta_activa(datos->cancha_id))
    {
        printf("La cancha no existe o esta inactiva\n");
        return 0;
    }
    char fecha[20];
    char hora[10];
    input_date("Nueva fecha (DD/MM/AAAA, Enter=hoy): ", fecha, 20);
    input_date("Nueva hora (HH:MM, Enter=ahora): ", hora, 10);
    snprintf(datos->comentario_personal, sizeof(datos->comentario_personal), "%s %s", fecha, hora);
    datos->goles = input_int("Nuevos goles: ");
    datos->asistencias = input_int("Nuevas asistencias: ");
    datos->resultado = input_int("Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (datos->resultado < 1 || datos->resultado > 3)
    {
        datos->resultado = input_int("Resultado invalido. Ingrese 1, 2 o 3: ");
    }
    listar_camisetas_disponibles();
    datos->camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", datos->camiseta) || !camiseta_esta_activa(datos->camiseta))
    {
        printf("La camiseta no existe o esta inactiva\n");
        return 0;
    }
    datos->clima = input_int("Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    while (datos->clima < 1 || datos->clima > 6)
    {
        datos->clima = input_int("Clima invalido. Ingrese entre 1 y 6: ");
    }
    datos->dia = input_int("Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (datos->dia < 1 || datos->dia > 3)
    {
        datos->dia = input_int("Dia invalido. Ingrese 1, 2 o 3: ");
    }
    datos->precio = input_int("Nuevo precio del partido: ");

    return 1;
}

static void actualizar_partido_completo(DatosPartido const *datos, char const *fecha_hora)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "UPDATE partido "
                "SET cancha_id=?, fecha_hora=?, goles=?, asistencias=?, camiseta_id=?, resultado=?, clima=?, dia=?, precio=? "
                "WHERE id=?",
                &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, datos->cancha_id);
    /* Asegurar formato de almacenamiento (YYYY-MM-DD HH:MM) */
    char fecha_storage[64] = {0};
    convert_display_date_to_storage(fecha_hora, fecha_storage, sizeof(fecha_storage));
    sqlite3_bind_text(stmt, 2, fecha_storage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, datos->goles);
    sqlite3_bind_int(stmt, 4, datos->asistencias);
    sqlite3_bind_int(stmt, 5, datos->camiseta);
    sqlite3_bind_int(stmt, 6, datos->resultado);
    sqlite3_bind_int(stmt, 7, datos->clima);
    sqlite3_bind_int(stmt, 8, datos->dia);
    sqlite3_bind_int(stmt, 9, datos->precio);
    sqlite3_bind_int(stmt, 10, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    mostrar_alerta_operacion("Partido", "Modificado", NULL);
}

static void modificar_todo_partido()
{
    DatosPartido datos;
    if (!recopilar_datos_completos_partido(&datos))
    {
        pause_console();
        return;
    }
    actualizar_partido_completo(&datos, datos.comentario_personal);
}

void modificar_partido()
{
    print_header("MODIFICAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Modificar (0 para cancelar): ");

    if (id == 0)
        return;

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        pause_console();
        return;
    }

    current_partido_id = id;

    MenuItem items[] =
    {
        {1, "Cancha", modificar_cancha_partido},
        {2, "Fecha y Hora", modificar_fecha_hora_partido},
        {3, "Goles", modificar_goles_partido},
        {4, "Asistencias", modificar_asistencias_partido},
        {5, "Resultado", modificar_resultado_partido},
        {6, "Camiseta", modificar_camiseta_partido},
        {7, "Clima", modificar_clima_partido},
        {8, "Dia", modificar_dia_partido},
        {9, "Comentario", modificar_comentario_partido},
        {10, "Precio", modificar_precio_partido},
        {11, "Modificar Todo", modificar_todo_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR PARTIDO", items, 12);
}

static void buscar_por_camiseta()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CAMISETA", "camiseta_id", "ID de la camiseta: ", &listar_camisetas, 1);
}

static void buscar_por_goles()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR GOLES", "goles", "Numero de goles: ", NULL, 0);
}

static void buscar_por_asistencias()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR ASISTENCIAS", "asistencias", "Numero de asistencias: ", NULL, 0);
}

static void buscar_por_cancha()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CANCHA", "cancha_id", "ID de la cancha: ", &listar_canchas_disponibles, 1);
}

void buscar_partidos()
{
    MenuItem items[] =
    {
        {1, "Por Camiseta", buscar_por_camiseta},
        {2, "Por Goles", buscar_por_goles},
        {3, "Por Asistencias", buscar_por_asistencias},
        {4, "Por Cancha", buscar_por_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu("BUSQUEDA DE PARTIDOS", items, 5);
}

static void manejar_gol_local(Equipo const *equipo_local, int minuto, int jugador_idx, int asistente_idx,
                              int *estadisticas_local, int *asistencias_local, int *goles_local)
{
    if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
    }

    (*goles_local)++;
    estadisticas_local[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_local[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_local->jugadores[jugador_idx].nombre,
           equipo_local->jugadores[jugador_idx].numero,
           equipo_local->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_local->jugadores[asistente_idx].nombre,
               equipo_local->jugadores[asistente_idx].numero);
    }
}

static void manejar_gol_visitante(Equipo const *equipo_visitante, int minuto, int jugador_idx, int asistente_idx,
                                  int *estadisticas_visitante, int *asistencias_visitante, int *goles_visitante)
{
    if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
    }

    (*goles_visitante)++;
    estadisticas_visitante[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_visitante[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_visitante->jugadores[jugador_idx].nombre,
           equipo_visitante->jugadores[jugador_idx].numero,
           equipo_visitante->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_visitante->jugadores[asistente_idx].nombre,
               equipo_visitante->jugadores[asistente_idx].numero);
    }
}

static int verificar_equipos_disponibles()
{
    sqlite3_stmt *stmt_count;
    if (!preparar_stmt("SELECT COUNT(*) FROM equipo", &stmt_count))
    {
        return 0;
    }
    sqlite3_step(stmt_count);
    int total_equipos = sqlite3_column_int(stmt_count, 0);
    sqlite3_finalize(stmt_count);

    if (total_equipos < 2)
    {
        printf("Se necesitan al menos 2 equipos guardados para simular un partido.\n");
        printf("Por favor, cree equipos primero.\n");
        pause_console();
        return 0;
    }
    return 1;
}

static void mostrar_equipos_disponibles()
{
    printf("=== EQUIPOS DISPONIBLES ===\n\n");
    sqlite3_stmt *stmt_equipos;
    if (!preparar_stmt("SELECT id, nombre FROM equipo ORDER BY id", &stmt_equipos))
    {
        return;
    }

    while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
    {
        printf("%d. %s\n", sqlite3_column_int(stmt_equipos, 0),
               sqlite3_column_text(stmt_equipos, 1));
    }
    sqlite3_finalize(stmt_equipos);
}

static void seleccionar_equipos(int *equipo_local_id, int *equipo_visitante_id)
{
    // Seleccionar equipo local
    do
    {
        *equipo_local_id = input_int("\nSeleccione el equipo LOCAL (ID): ");
        if (!existe_id("equipo", *equipo_local_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (!existe_id("equipo", *equipo_local_id));

    // Seleccionar equipo visitante (diferente al local)
    do
    {
        *equipo_visitante_id = input_int("Seleccione el equipo VISITANTE (ID): ");
        if (*equipo_visitante_id == *equipo_local_id)
        {
            printf("El equipo visitante debe ser diferente al local.\n");
        }
        else if (!existe_id("equipo", *equipo_visitante_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (*equipo_visitante_id == *equipo_local_id || !existe_id("equipo", *equipo_visitante_id));
}

static int cargar_equipos(int equipo_local_id, int equipo_visitante_id, Equipo *equipo_local, Equipo *equipo_visitante)
{
    memset(equipo_local, 0, sizeof(Equipo));
    memset(equipo_visitante, 0, sizeof(Equipo));

    if (!cargar_equipo_desde_bd(equipo_local_id, equipo_local))
    {
        printf("Error al cargar el equipo local.\n");
        pause_console();
        return 0;
    }

    if (!cargar_equipo_desde_bd(equipo_visitante_id, equipo_visitante))
    {
        printf("Error al cargar el equipo visitante.\n");
        pause_console();
        return 0;
    }
    return 1;
}

static void mostrar_inicio_partido(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    printf("\n*** INICIANDO SIMULACION ***\n");
    printf("EQUIPO LOCAL: %s\n", equipo_local->nombre);
    printf("EQUIPO VISITANTE: %s\n\n", equipo_visitante->nombre);
}

static void mostrar_alineacion(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    clear_screen();
    printf("%s\n", ASCII_SIMULACION);
    printf("                    SIMULACION DE PARTIDO\n\n");

    printf("=== %s VS %s ===\n\n", equipo_local->nombre, equipo_visitante->nombre);

    // Mostrar cancha inicial
    mostrar_cancha_animada(0, 0);

    // Mostrar equipos alineados
    imprimir_alineacion_equipo("EQUIPO LOCAL", equipo_local);
    printf("\n");
    imprimir_alineacion_equipo("EQUIPO VISITANTE", equipo_visitante);

    printf("\n*** INICIO DEL PARTIDO ***\n");
    printf("La simulacion comenzara automaticamente en 3 segundos...\n");
    Sleep(3000);
}

static void simular_partido_logica(Equipo const *equipo_local, Equipo const *equipo_visitante,
                                   EstadisticasPartido *estadisticas)
{
    for (int minuto = 1; minuto <= 60; minuto++)
    {
        clear_screen();
        print_header("SIMULACION DE PARTIDO");

        printf("=== %s %d - %d %s ===\n\n",
               equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);
        printf("MINUTO: %d\n\n", minuto);

        // Generar eventos aleatorios
        int evento = secure_rand(100);

        if (evento < 25) // Gol local
        {
            int jugador_idx = secure_rand(equipo_local->num_jugadores);
            int asistente_idx = secure_rand(equipo_local->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
            }

            manejar_gol_local(equipo_local, minuto, jugador_idx, asistente_idx,
                              estadisticas->estadisticas_local, estadisticas->asistencias_local, &estadisticas->goles_local);
        }
        else if (evento < 50) // Gol visitante
        {
            int jugador_idx = secure_rand(equipo_visitante->num_jugadores);
            int asistente_idx = secure_rand(equipo_visitante->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
            }

            manejar_gol_visitante(equipo_visitante, minuto, jugador_idx, asistente_idx,
                                  estadisticas->estadisticas_visitante, estadisticas->asistencias_visitante, &estadisticas->goles_visitante);
        }
        else if (evento < 70)
        {
            printf("*** Oportunidad de gol ***\n");
        }
        else
        {
            printf("*** El partido continua... ***\n");
        }

        mostrar_cancha_animada(minuto, (evento < 4) ? 1 : 0);
        Sleep(1000);
    }
}

static void mostrar_resultados(Equipo const *equipo_local, Equipo const *equipo_visitante,
                               EstadisticasPartido const *estadisticas)
{
    // Resultados finales
    clear_screen();
    print_header("FIN DEL PARTIDO");

    printf("*** RESULTADO FINAL ***\n\n");
    printf("*** 60 MINUTOS COMPLETADOS ***\n\n");

    printf("*** %s %d - %d %s ***\n\n",
           equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);

    if (estadisticas->goles_local > estadisticas->goles_visitante)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_local->nombre);
    }
    else if (estadisticas->goles_visitante > estadisticas->goles_local)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_visitante->nombre);
    }
    else
    {
        printf("*** ¡EMPATE! ***\n\n");
    }

    // Mostrar estadisticas
    printf("*** ESTADISTICAS DEL PARTIDO ***\n\n");

    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_local[i] > 0 || estadisticas->asistencias_local[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_local->jugadores[i].nombre,
                   equipo_local->jugadores[i].numero,
                   estadisticas->estadisticas_local[i], estadisticas->asistencias_local[i]);
        }
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_visitante[i] > 0 || estadisticas->asistencias_visitante[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_visitante->jugadores[i].nombre,
                   equipo_visitante->jugadores[i].numero,
                   estadisticas->estadisticas_visitante[i], estadisticas->asistencias_visitante[i]);
        }
    }
}

static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_equipo;
    const char *sql_equipo = "SELECT nombre, tipo, tipo_futbol, num_jugadores FROM equipo WHERE id = ?";

    if (!preparar_stmt(sql_equipo, &stmt_equipo))
    {
        return 0;
    }

    sqlite3_bind_int(stmt_equipo, 1, equipo_id);

    if (sqlite3_step(stmt_equipo) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt_equipo);
        return 0;
    }

    equipo->id = equipo_id;
    strncpy_s(equipo->nombre, sizeof(equipo->nombre), (const char *)sqlite3_column_text(stmt_equipo, 0), _TRUNCATE);
    equipo->tipo = sqlite3_column_int(stmt_equipo, 1);
    equipo->tipo_futbol = sqlite3_column_int(stmt_equipo, 2);
    equipo->num_jugadores = sqlite3_column_int(stmt_equipo, 3);
    equipo->partido_id = -1;

    sqlite3_finalize(stmt_equipo);

    // Cargar jugadores
    return cargar_jugadores_equipo(equipo_id, equipo);
}

static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero";

    if (!preparar_stmt(sql_jugadores, &stmt_jugadores))
    {
        return 0;
    }

    sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

    int jugador_idx = 0;
    while (sqlite3_step(stmt_jugadores) == SQLITE_ROW && jugador_idx < 11)
    {
        strncpy_s(equipo->jugadores[jugador_idx].nombre,
                  sizeof(equipo->jugadores[jugador_idx].nombre),
                  (const char *)sqlite3_column_text(stmt_jugadores, 0),
                  _TRUNCATE);
        equipo->jugadores[jugador_idx].numero = sqlite3_column_int(stmt_jugadores, 1);
        equipo->jugadores[jugador_idx].posicion = sqlite3_column_int(stmt_jugadores, 2);
        equipo->jugadores[jugador_idx].es_capitan = sqlite3_column_int(stmt_jugadores, 3);
        jugador_idx++;
    }

    sqlite3_finalize(stmt_jugadores);
    return 1;
}

static void determinar_resultado_partido(int goles_local, int goles_visitante,
        int *resultado_local, int *resultado_visitante)
{
    if (goles_local > goles_visitante)
    {
        *resultado_local = 1;     // VICTORIA
        *resultado_visitante = 3; // DERROTA
    }
    else if (goles_visitante > goles_local)
    {
        *resultado_local = 3;     // DERROTA
        *resultado_visitante = 1; // VICTORIA
    }
    else
    {
        *resultado_local = 2;     // EMPATE
        *resultado_visitante = 2; // EMPATE
    }
}

static UNUSED int obtener_cancha_defecto()
{
    int cancha_id = 1;
    sqlite3_stmt *stmt_cancha;
    if (!preparar_stmt("SELECT id FROM cancha ORDER BY id LIMIT 1", &stmt_cancha))
    {
        return cancha_id;
    }
    if (sqlite3_step(stmt_cancha) == SQLITE_ROW)
    {
        cancha_id = sqlite3_column_int(stmt_cancha, 0);
    }
    sqlite3_finalize(stmt_cancha);
    return cancha_id;
}

static UNUSED void guardar_resultados_simulacion(DatosSimulacion const *datos_simulacion)
{
    char fecha_simulacion[20] = "2023-01-01 00:00";
    get_datetime(fecha_simulacion, sizeof(fecha_simulacion));

    // Determinar resultados
    int resultado_local;
    int resultado_visitante;
    determinar_resultado_partido(datos_simulacion->goles_local, datos_simulacion->goles_visitante,
                                 &resultado_local, &resultado_visitante);

    // Obtener cancha por defecto
    int cancha_id = obtener_cancha_defecto();

    // Guardar estadisticas para cada jugador del equipo local
    guardar_estadisticas_equipo(&datos_simulacion->equipo_local, datos_simulacion->estadisticas_local,
                                datos_simulacion->asistencias_local, resultado_local, cancha_id, fecha_simulacion);

    // Guardar estadisticas para cada jugador del equipo visitante
    guardar_estadisticas_equipo(&datos_simulacion->equipo_visitante, datos_simulacion->estadisticas_visitante,
                                datos_simulacion->asistencias_visitante, resultado_visitante, cancha_id, fecha_simulacion);

    printf("*** RESULTADOS GUARDADOS EN LA BASE DE DATOS ***\n");
}

static UNUSED void guardar_estadisticas_equipo(Equipo const *equipo, int const *estadisticas, int const *asistencias,
        int resultado, int cancha_id, char const *fecha_simulacion)
{
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        if (estadisticas[i] > 0 || asistencias[i] > 0)
        {
            // Buscar o crear camiseta para este jugador
            int camiseta_id = 1; // Usar camiseta por defecto

            DatosPartido datos = {0};
            datos.cancha_id = cancha_id;
            datos.goles = estadisticas[i];
            datos.asistencias = asistencias[i];
            datos.camiseta = camiseta_id;
            datos.resultado = resultado;
            datos.rendimiento_general = 8;
            datos.cansancio = 5;
            datos.estado_animo = 7;
            strncpy_s(datos.comentario_personal, sizeof(datos.comentario_personal), "Partido simulado", _TRUNCATE);
            datos.clima = 1;
            datos.dia = 1;

            long long partido_id = obtener_siguiente_id("partido");
            insertar_partido(partido_id, &datos, fecha_simulacion);
        }
    }
}

static UNUSED DatosSimulacion crear_datos_simulacion(Equipo equipo_local, Equipo equipo_visitante,
        EstadisticasPartido const *estadisticas)
{
    DatosSimulacion datos_simulacion = {0};
    datos_simulacion.equipo_local = equipo_local;
    datos_simulacion.equipo_visitante = equipo_visitante;
    memcpy(datos_simulacion.estadisticas_local, estadisticas->estadisticas_local, sizeof(estadisticas->estadisticas_local));
    memcpy(datos_simulacion.estadisticas_visitante, estadisticas->estadisticas_visitante, sizeof(estadisticas->estadisticas_visitante));
    memcpy(datos_simulacion.asistencias_local, estadisticas->asistencias_local, sizeof(estadisticas->asistencias_local));
    memcpy(datos_simulacion.asistencias_visitante, estadisticas->asistencias_visitante, sizeof(estadisticas->asistencias_visitante));
    datos_simulacion.goles_local = estadisticas->goles_local;
    datos_simulacion.goles_visitante = estadisticas->goles_visitante;
    return datos_simulacion;
}

void simular_partido_guardados()
{
    clear_screen();
    print_header("SIMULAR PARTIDO CON EQUIPOS GUARDADOS");

    // Verificar que hay al menos 2 equipos disponibles
    if (!verificar_equipos_disponibles())
    {
        return;
    }

    // Mostrar equipos disponibles
    mostrar_equipos_disponibles();

    // Seleccionar equipos
    int equipo_local_id;
    int equipo_visitante_id;
    seleccionar_equipos(&equipo_local_id, &equipo_visitante_id);

    // Cargar equipos desde la base de datos
    Equipo equipo_local;
    Equipo equipo_visitante;
    if (!cargar_equipos(equipo_local_id, equipo_visitante_id, &equipo_local, &equipo_visitante))
    {
        return;
    }

    // Mostrar informacion inicial
    mostrar_inicio_partido(&equipo_local, &equipo_visitante);

    // Mostrar alineacion y comenzar partido
    mostrar_alineacion(&equipo_local, &equipo_visitante);

    // Preparar arrays para estadisticas
    EstadisticasPartido estadisticas = {0};

    // Ejecutar simulacion del partido
    simular_partido_logica(&equipo_local, &equipo_visitante, &estadisticas);

    // Mostrar resultados finales
    mostrar_resultados(&equipo_local, &equipo_visitante, &estadisticas);

    printf("Simulacion finalizada. Este partido no se guarda en Partidos.\n");

    printf("\nPresione Enter para volver al menu...");
    getchar();
}

#define TACTIC_W 40
#define TACTIC_H 20

static UNUSED void tactica_init_grid(char grid[TACTIC_H][TACTIC_W + 1])
{
    for (int y = 0; y < TACTIC_H; y++)
    {
        for (int x = 0; x < TACTIC_W; x++)
        {
            grid[y][x] = '.';
        }
        grid[y][TACTIC_W] = '\0';
    }
}

static UNUSED void tactica_build_grid_string(char grid[TACTIC_H][TACTIC_W + 1], char *out, size_t size)
{
    size_t used = 0;
    if (!out || size == 0)
    {
        return;
    }

    for (int y = 0; y < TACTIC_H; y++)
    {
        for (int x = 0; x < TACTIC_W; x++)
        {
            if (used + 1 >= size)
            {
                out[used] = '\0';
                return;
            }
            out[used++] = grid[y][x];
        }
        if (used + 1 >= size)
        {
            out[used] = '\0';
            return;
        }
        out[used++] = '\n';
    }
    out[used] = '\0';
}

static UNUSED void tactica_guardar_diagrama(int partido_id, const char *nombre, const char *grid_text)
{
    const char *sql =
        "INSERT INTO tactica_diagrama (partido_id, nombre, fecha, grid) "
        "VALUES (?, ?, date('now'), ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error preparando insercion.\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, grid_text, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error guardando diagrama: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

#ifndef RSIZE_MAX_STR
#define RSIZE_MAX_STR (SIZE_MAX >> 1)
#endif

#ifndef TACTICA_TRUNCATE
#define TACTICA_TRUNCATE ((size_t)-1)
#endif

static int tactica_strncpy_s(char *dest, size_t destsz, const char *src, size_t count)
{
    if (!dest || !src || destsz == 0 || destsz > RSIZE_MAX_STR)
        return 1;

#if defined(__STDC_LIB_EXT1__)
    return strncpy_s(dest, destsz, src, count == TACTICA_TRUNCATE ? _TRUNCATE : count);
#elif defined(_MSC_VER)
    return strncpy_s(dest, destsz, src, count == TACTICA_TRUNCATE ? _TRUNCATE : count);
#else
    size_t max_src = (count == TACTICA_TRUNCATE) ? (destsz - 1) : count;
    size_t src_len = strnlen(src, max_src);
    size_t to_copy = src_len;
    if (to_copy > destsz - 1)
        to_copy = destsz - 1;

    if (to_copy > 0)
        memcpy(dest, src, to_copy);

    dest[to_copy] = '\0';
    return 0;
#endif
}

static size_t tactica_strlen_secure(const char *s, size_t max)
{
    if (!s)
        return 0;

#if defined(__STDC_LIB_EXT1__)
    return strlen_s(s, max);
#elif defined(_MSC_VER)
    return strnlen_s(s, max);
#else
    return strnlen(s, max);
#endif
}

static void tactica_trim_newline(char *s)
{
    if (!s)
        return;

    size_t len = tactica_strlen_secure(s, RSIZE_MAX_STR);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[--len] = '\0';
    }
}

static void tactica_leer_nombre_diagrama(char *nombre, size_t size)
{
    ui_printf("Nombre del diagrama: ");
    if (!fgets(nombre, (int)size, stdin))
    {
        if (size > 0)
            nombre[0] = '\0';
        return;
    }

    tactica_trim_newline(nombre);

    if (nombre[0] == '\0' && tactica_strncpy_s(nombre, size, "Diagrama", TACTICA_TRUNCATE) != 0 && size > 0)
    {
        nombre[0] = '\0';
    }
}

static void tactica_mostrar_partidos_disponibles(void)
{
    const char *sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
                      "JOIN cancha can ON p.cancha_id = can.id ORDER BY p.id ASC";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt)) return;

    ui_printf_centered_line("--- Partidos disponibles ---");
    ui_printf_centered_line("ID | Cancha | Fecha");
    ui_printf_centered_line("---------------------------");

    int count = 0;
    char fecha_fmt[20];
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2),
                                fecha_fmt, sizeof(fecha_fmt));
        ui_printf_centered_line("%d | %s | %s",
                                sqlite3_column_int(stmt, 0),
                                (const char *)sqlite3_column_text(stmt, 1),
                                fecha_fmt);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        ui_printf_centered_line("No hay partidos registrados.");
    ui_printf("\n");
}

static void tactica_mostrar_grid_con_ejes(char grid[TACTIC_H][TACTIC_W + 1])
{
    ui_printf("     ");
    for (int x = 0; x < TACTIC_W; x += 5)
        ui_printf("%-5d", x);
    ui_printf("\n");

    for (int y = 0; y < TACTIC_H; y++)
        ui_printf("%2d   %s\n", y, grid[y]);
}

static int tactica_colocar(const char *args, char grid[TACTIC_H][TACTIC_W + 1],
                           char *grid_text, size_t grid_text_size)
{
    int x = -1;
    int y = -1;
    char c = '\0';
#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(args, "%d %d %c", &x, &y, &c, 1) == 3)
#else
    if (sscanf(args, "%d %d %c", &x, &y, &c) == 3)
#endif
    {
        if (x >= 0 && x < TACTIC_W && y >= 0 && y < TACTIC_H)
        {
            grid[y][x] = c;
            tactica_build_grid_string(grid, grid_text, grid_text_size);
        }
        else
        {
            ui_printf("Fuera de rango (Col: 0..%d, Fila: 0..%d).\n", TACTIC_W - 1, TACTIC_H - 1);
            pause_console();
        }
    }
    else
    {
        ui_printf("Formato: Col Fila Caracter (ej: 10 5 X)\n");
        pause_console();
    }
    return 0;
}

static int tactica_borrar(const char *args, char grid[TACTIC_H][TACTIC_W + 1],
                          char *grid_text, size_t grid_text_size)
{
    int x = -1;
    int y = -1;
#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(args, "%d %d", &x, &y) == 2)
#else
    if (sscanf(args, "%d %d", &x, &y) == 2)
#endif
    {
        if (x >= 0 && x < TACTIC_W && y >= 0 && y < TACTIC_H)
        {
            grid[y][x] = '.';
            tactica_build_grid_string(grid, grid_text, grid_text_size);
        }
        else
        {
            ui_printf("Fuera de rango.\n");
            pause_console();
        }
    }
    else
    {
        ui_printf("Formato: d Col Fila (ej: d 10 5)\n");
        pause_console();
    }

    return 0;
}

static int tactica_procesar_comando(const char *line, char grid[TACTIC_H][TACTIC_W + 1], char *grid_text,
                                    size_t grid_text_size, int partido_id, const char *nombre)
{
    if (!line || !grid || !grid_text)
        return 0;

    char cmd = line[0];
    switch (cmd)
    {
    case 'q':
    case 'Q':
        return 1;

    case 'r':
    case 'R':
        tactica_init_grid(grid);
        tactica_build_grid_string(grid, grid_text, grid_text_size);
        return 0;

    case 's':
    case 'S':
        tactica_build_grid_string(grid, grid_text, grid_text_size);
        tactica_guardar_diagrama(partido_id, nombre, grid_text);
        ui_printf("Diagrama guardado.\n");
        pause_console();
        return 1;

    case 'p':
    case 'P':
        return tactica_colocar(line + 1, grid, grid_text, grid_text_size);

    case 'd':
    case 'D':
        return tactica_borrar(line + 1, grid, grid_text, grid_text_size);

    default:
        if (isdigit((unsigned char)cmd))
            return tactica_colocar(line, grid, grid_text, grid_text_size);
        ui_printf("Comando no reconocido.\n");
        pause_console();
        return 0;
    }
}

static int tactica_listar_diagramas_simple(int con_pause)
{
    const char *sql =
        "SELECT id, partido_id, nombre, fecha FROM tactica_diagrama ORDER BY id DESC LIMIT 20";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando diagramas.\n");
        if (con_pause)
        {
            pause_console();
        }
        return 0;
    }

    ui_printf_centered_line("ID | Partido | Fecha | Nombre");
    ui_printf_centered_line("-----------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int partido_id = sqlite3_column_int(stmt, 1);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);

        ui_printf_centered_line("%d | %d | %s | %s", id, partido_id, fecha ? fecha : "", nombre ? nombre : "");
        count++;
    }

    sqlite3_finalize(stmt);
    if (count == 0)
    {
        ui_printf_centered_line("No hay diagramas registrados.");
    }

    if (con_pause)
    {
        pause_console();
    }

    return count;
}

static void tactica_ver_diagrama()
{
    clear_screen();
    print_header("DIAGRAMAS TACTICOS");

    int count = tactica_listar_diagramas_simple(0);
    if (count == 0)
    {
        pause_console();
        return;
    }

    int id = input_int("ID del diagrama (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    const char *sql = "SELECT nombre, grid FROM tactica_diagrama WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando diagrama.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        const char *grid = (const char *)sqlite3_column_text(stmt, 1);

        clear_screen();
        print_header(nombre ? nombre : "DIAGRAMA");
        if (grid && grid[0])
        {
            ui_puts(grid);
        }
        else
        {
            ui_puts("(Sin contenido)");
        }
    }
    else
    {
        printf("Diagrama no encontrado.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void tactica_crear_diagrama()
{
    clear_screen();
    print_header("CREAR DIAGRAMA TACTICO");

    tactica_mostrar_partidos_disponibles();

    int partido_id = input_int("ID de partido (0 para cancelar): ");
    if (partido_id <= 0)
        return;

    if (!existe_id("partido", partido_id))
    {
        printf("Partido no encontrado.\n");
        pause_console();
        return;
    }

    char nombre[128] = {0};
    tactica_leer_nombre_diagrama(nombre, sizeof(nombre));

    char grid[TACTIC_H][TACTIC_W + 1];
    tactica_init_grid(grid);

    char grid_text[(TACTIC_H * (TACTIC_W + 1)) + 1];
    tactica_build_grid_string(grid, grid_text, sizeof(grid_text));

    for (;;)
    {
        clear_screen();
        print_header("EDITAR DIAGRAMA");
        tactica_mostrar_grid_con_ejes(grid);
        ui_printf("\nComandos:\n");
        ui_printf("  Col Fila C -> colocar caracter C (ej: 10 5 X)\n");
        ui_printf("  d Col Fila -> borrar posicion\n");
        ui_printf("  r          -> reiniciar diagrama\n");
        ui_printf("  s          -> guardar diagrama\n");
        ui_printf("  q          -> cancelar\n");
        ui_printf("\nIngrese comando: ");

        char line[64] = {0};
        if (!fgets(line, sizeof(line), stdin))
            return;

        tactica_trim_newline(line);

        if (line[0] == '\0')
            continue;

        if (tactica_procesar_comando(line, grid, grid_text, sizeof(grid_text), partido_id, nombre))
            return;
    }
}

void menu_tacticas_partido()
{
    MenuItem items[] =
    {
        {1, "Crear diagrama", tactica_crear_diagrama},
        {2, "Ver diagramas", tactica_ver_diagrama},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("ANALISIS TACTICO");
    ejecutar_menu("ANALISIS TACTICO", items, 3);
}

/* ------------------- Favoritos y Etiquetas (Tags) para partidos ------------------- */

/* Longitud maxima para una etiqueta */
#define PARTIDO_TAG_MAX_LEN 64
/* Usar `trim_whitespace` global definida en `utils.c` */

static void partido_init_meta_tables(void)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_meta = "CREATE TABLE IF NOT EXISTS partido_meta (partido_id INTEGER PRIMARY KEY, favorito INTEGER DEFAULT 0)";
    if (preparar_stmt(sql_meta, &stmt))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char *sql_tags = "CREATE TABLE IF NOT EXISTS partido_tag (id INTEGER PRIMARY KEY AUTOINCREMENT, partido_id INTEGER NOT NULL, tag TEXT NOT NULL, UNIQUE(partido_id, tag))";
    if (preparar_stmt(sql_tags, &stmt))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

static int partido_obtener_favorito(int partido_id)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT favorito FROM partido_meta WHERE partido_id = ?";
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, partido_id);
    int fav = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        fav = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return fav;
}

static void partido_marcar_favorito(int partido_id, int favorito)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR REPLACE INTO partido_meta (partido_id, favorito) VALUES (?, ?)";
    if (!preparar_stmt(sql, &stmt))
        return;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_int(stmt, 2, favorito ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static int partido_agregar_tag(int partido_id, const char *tag)
{
    if (!tag || tag[0] == '\0') return 0;
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR IGNORE INTO partido_tag (partido_id, tag) VALUES (?, ?)";
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, tag, -1, SQLITE_TRANSIENT);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return res;
}

static int partido_quitar_tag(int partido_id, const char *tag)
{
    if (!tag || tag[0] == '\0') return 0;
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM partido_tag WHERE partido_id = ? AND tag = ?";
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, tag, -1, SQLITE_TRANSIENT);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return res;
}

static int partido_listar_tags_print(int partido_id)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT tag FROM partido_tag WHERE partido_id = ? ORDER BY tag ASC";
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando etiquetas.\n");
        return 0;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    int count = 0;
    ui_printf("Etiquetas: ");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *t = (const char *)sqlite3_column_text(stmt, 0);
        if (count) ui_printf(", ");
        ui_printf("%s", t ? t : "");
        count++;
    }
    ui_printf("\n");
    sqlite3_finalize(stmt);
    return count;
}

static int partido_prompt_tag_input(const char *prompt, char *out, size_t size)
{
    if (!prompt || !out || size == 0) return 0;
    ui_printf("%s", prompt);
    /* fgets expects an int for size; cast explicitly to avoid implicit
       conversion warnings when passing size_t (e.g. sizeof buffers). */
    if (!fgets(out, (int)size, stdin)) return 0;
    tactica_trim_newline(out);
    trim_whitespace(out);
    return out[0] != '\0';
}

static void partido_ui_agregar_tag(int partido_id)
{
    char tag[PARTIDO_TAG_MAX_LEN] = {0};
    if (!partido_prompt_tag_input("Etiqueta (ej: Final, Amistoso, Importante): ", tag, sizeof(tag)))
    {
        ui_printf("Etiqueta vacia.\n");
        pause_console();
        return;
    }
    if (partido_agregar_tag(partido_id, tag)) ui_printf("Etiqueta añadida.\n");
    else ui_printf("No se pudo agregar etiqueta o ya existe.\n");
    pause_console();
}

static void partido_ui_quitar_tag(int partido_id)
{
    char tag[PARTIDO_TAG_MAX_LEN] = {0};
    if (!partido_prompt_tag_input("Etiqueta a quitar: ", tag, sizeof(tag)))
    {
        ui_printf("Etiqueta vacía.\n");
        pause_console();
        return;
    }
    if (partido_quitar_tag(partido_id, tag)) ui_printf("Etiqueta eliminada.\n");
    else ui_printf("Etiqueta no encontrada.\n");
    pause_console();
}

/* Helper: UI loop to manage tags for a specific partido ID. Extracted to
   reduce cognitive complexity of the parent menu function. */
static void partido_manage_tags_for_id(int id)
{
    for (;;)
    {
        clear_screen();
        print_header("ETIQUETAS DEL PARTIDO");
        ui_printf("Partido ID: %d\n\n", id);
        partido_listar_tags_print(id);

        ui_printf("1) Agregar etiqueta\n");
        ui_printf("2) Quitar etiqueta\n");
        ui_printf("0) Volver\n");
        int opt2 = input_int("Opcion: ");
        if (opt2 == 0) break;

        switch (opt2)
        {
        case 1:
            partido_ui_agregar_tag(id);
            break;
        case 2:
            partido_ui_quitar_tag(id);
            break;
        default:
            ui_printf("Opcion invalida.\n");
            pause_console();
            break;
        }
    }
}

/* Helper: UI to input an optional tag and list partidos with that tag (or
   all partidos that have tags). Extracted to simplify the parent menu. */
static int partido_mostrar_partidos_con_tag(const char *tag);
static void partido_list_tags_ui(void)
{
    char tag[PARTIDO_TAG_MAX_LEN] = {0};
    ui_printf("Etiqueta (dejar vacia para listar todos con tags): ");
    if (!fgets(tag, (int)sizeof(tag), stdin)) tag[0] = '\0';
    tactica_trim_newline(tag);
    trim_whitespace(tag);

    int res = partido_mostrar_partidos_con_tag(tag[0] ? tag : NULL);
    if (res == 0)
    {
        if (tag[0]) ui_printf("No hay partidos con la etiqueta '%s'.\n", tag);
        else ui_printf("No hay partidos con etiquetas.\n");
    }
    pause_console();
}

static int partido_mostrar_favoritos(void)
{
    partido_init_meta_tables();
    const char *sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
                      "JOIN cancha can ON p.cancha_id = can.id "
                      "JOIN partido_meta m ON p.id = m.partido_id "
                      "WHERE m.favorito = 1 ORDER BY p.id ASC";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando partidos favoritos.\n");
        return 0;
    }

    ui_printf_centered_line("--- Partidos favoritos ---");
    ui_printf_centered_line("ID | Cancha | Fecha");
    ui_printf_centered_line("---------------------------");

    int count = 0;
    char fecha_fmt[20];
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2),
                                fecha_fmt, sizeof(fecha_fmt));
        ui_printf_centered_line("%d | %s | %s",
                                sqlite3_column_int(stmt, 0),
                                (const char *)sqlite3_column_text(stmt, 1),
                                fecha_fmt);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        ui_printf_centered_line("No hay partidos favoritos.");
    ui_printf("\n");
    return count;
}

static void menu_marcar_favorito_partido()
{
    for (;;)
    {
        clear_screen();
        print_header("FAVORITOS");

        ui_printf("1) Marcar/Desmarcar favorito\n");
        ui_printf("2) Listar solo favoritos\n");
        ui_printf("0) Volver\n");
        int opt = input_int("Opcion: ");
        if (opt == 0) return;

        switch (opt)
        {
        case 1:
        {
            tactica_mostrar_partidos_disponibles();
            int id = input_int("ID de partido (0 para cancelar): ");
            if (id == 0) break;
            if (!existe_id("partido", id))
            {
                printf("Partido no encontrado.\n");
                pause_console();
                break;
            }
            {
                int fav = partido_obtener_favorito(id);
                partido_marcar_favorito(id, !fav);
                ui_printf("Partido %s favorito.\n", !fav ? "marcado como" : "desmarcado como");
            }
            pause_console();
            break;
        }
        case 2:
        {
            clear_screen();
            print_header("PARTIDOS FAVORITOS");
            partido_mostrar_favoritos();
            pause_console();
            break;
        }
        default:
            ui_printf("Opcion no reconocida.\n");
            pause_console();
            break;
        }
    }
}

static int partido_mostrar_partidos_con_tag(const char *tag)
{
    partido_init_meta_tables();
    sqlite3_stmt *stmt = NULL;
    const char *sql = NULL;

    if (tag && tag[0] != '\0')
    {
        sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
              "JOIN cancha can ON p.cancha_id = can.id "
              "JOIN partido_tag pt ON p.id = pt.partido_id "
              "WHERE pt.tag = ? ORDER BY p.id ASC";
        if (!preparar_stmt(sql, &stmt))
        {
            printf("Error consultando partidos con etiquetas.\n");
            return 0;
        }
        sqlite3_bind_text(stmt, 1, tag, -1, SQLITE_TRANSIENT);
    }
    else
    {
        sql = "SELECT p.id, can.nombre, p.fecha_hora, GROUP_CONCAT(pt.tag, ', ') as tags FROM partido p "
              "JOIN cancha can ON p.cancha_id = can.id "
              "JOIN partido_tag pt ON p.id = pt.partido_id "
              "GROUP BY p.id ORDER BY p.id ASC";
        if (!preparar_stmt(sql, &stmt))
        {
            printf("Error consultando partidos con etiquetas.\n");
            return 0;
        }
    }

    ui_printf_centered_line("--- Partidos con etiquetas ---");
    ui_printf_centered_line("ID | Cancha | Fecha | Etiquetas");
    ui_printf_centered_line("-------------------------------------------");

    int count = 0;
    char fecha_fmt[20];
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *cancha = (const char *)sqlite3_column_text(stmt, 1);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 2);
        format_date_for_display(fecha, fecha_fmt, sizeof(fecha_fmt));

        if (tag && tag[0] != '\0')
        {
            ui_printf_centered_line("%d | %s | %s | %s", id,
                                    cancha ? cancha : "",
                                    fecha_fmt,
                                    tag);
        }
        else
        {
            const char *tags = (const char *)sqlite3_column_text(stmt, 3);
            ui_printf_centered_line("%d | %s | %s | %s", id,
                                    cancha ? cancha : "",
                                    fecha_fmt,
                                    tags ? tags : "");
        }
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        return 0;

    ui_printf("\n");
    return count;
}

static void menu_gestion_tags_partido()
{
    for (;;)
    {
        clear_screen();
        print_header("GESTIONAR ETIQUETAS (TAGS)");

        ui_printf("1) Gestionar etiquetas de un partido\n");
        ui_printf("2) Listar partidos con etiquetas\n");
        ui_printf("0) Volver\n");
        int opt = input_int("Opcion: ");
        if (opt == 0) return;

        switch (opt)
        {
        case 1:
        {
            tactica_mostrar_partidos_disponibles();
            int id = input_int("ID de partido (0 para cancelar): ");
            if (id == 0) break;
            if (!existe_id("partido", id))
            {
                printf("Partido no encontrado.\n");
                pause_console();
                break;
            }

            partido_manage_tags_for_id(id);
            break;
        }
        case 2:
            partido_list_tags_ui();
            break;
        default:
            ui_printf("Opcion invalida.\n");
            pause_console();
            break;
        }
    }
}

void menu_partidos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_partido},
        {2, "Listar", listar_partidos},
        {3, "Modificar", modificar_partido},
        {4, "Eliminar", eliminar_partido},
        {5, "Simular con Equipos Guardados", simular_partido_guardados},
        {6, "Analisis Tactico", menu_tacticas_partido},
        {7, "Favoritos", menu_marcar_favorito_partido},
        {8, "Etiquetas (Tags)", menu_gestion_tags_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu("PARTIDOS", items, 9);
}
