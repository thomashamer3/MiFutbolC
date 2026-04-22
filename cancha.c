#include "cancha.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static void listar_canchas_simple(void);
static void solicitar_nombre_cancha(const char *prompt, char *buffer, int size);

static void solicitar_campo_no_vacio(const char *prompt, char *buffer, int size)
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

static void solicitar_telefono_no_vacio(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string_extended(prompt, buffer, size);
        trim_whitespace(buffer);

        if (buffer[0] == '\0')
        {
            printf("El telefono no puede estar vacio.\n");
            continue;
        }

        int tiene_digito = 0;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            if (isdigit((unsigned char)buffer[i]))
            {
                tiene_digito = 1;
                break;
            }
        }

        if (tiene_digito)
        {
            return;
        }

        printf("El telefono debe contener al menos un numero.\n");
    }
}

#define TIPO_CANCHA_MULTI_BASE 1000
#define SUPERFICIE_MULTI_BASE 2000

static int tipo_cancha_bit_desde_codigo(int codigo)
{
    switch (codigo)
    {
    case 5:
        return 1 << 0;
    case 7:
        return 1 << 1;
    case 8:
        return 1 << 2;
    case 11:
        return 1 << 3;
    case 99:
        return 1 << 4;
    default:
        return 0;
    }
}

static int tipo_cancha_codigo_unico_desde_mask(int mask)
{
    switch (mask)
    {
    case (1 << 0):
        return 5;
    case (1 << 1):
        return 7;
    case (1 << 2):
        return 8;
    case (1 << 3):
        return 11;
    case (1 << 4):
        return 99;
    default:
        return 0;
    }
}

static void agregar_texto_tipo_en_buffer(char *buffer, size_t size, const char *texto)
{
    if (!buffer || !texto || size == 0)
    {
        return;
    }

    size_t buffer_len = strlen_s(buffer, size);
    if (buffer_len >= size)
    {
        buffer[size - 1] = '\0';
        return;
    }

    const char *separador = (buffer_len > 0) ? ", " : "";
    (void)strncat_s(buffer, size, separador, _TRUNCATE);
    (void)strncat_s(buffer, size, texto, _TRUNCATE);
}

static int es_separador_lista(int c)
{
    return c == ' ' || c == ',' || c == ';' || c == '/' || c == '-' || c == '\t';
}

static int caracter_valido_tras_numero(int c)
{
    return c == '\0' || es_separador_lista((unsigned char)c);
}

static int parsear_seleccion_tipo_cancha(const char *entrada, int *out_mask, int *out_count)
{
    if (!entrada || !out_mask || !out_count)
    {
        return 0;
    }

    int mask = 0;
    int count = 0;
    const char *p = entrada;

    while (*p)
    {
        while (*p && es_separador_lista((unsigned char)*p))
        {
            p++;
        }

        if (!*p)
        {
            break;
        }

        if (!isdigit((unsigned char)*p))
        {
            return 0;
        }

        int codigo = 0;
        while (*p && isdigit((unsigned char)*p))
        {
            codigo = codigo * 10 + (*p - '0');
            p++;
        }

        int bit = tipo_cancha_bit_desde_codigo(codigo);
        if (bit == 0)
        {
            return 0;
        }

        if ((mask & bit) == 0)
        {
            mask |= bit;
            count++;
        }

        if (!caracter_valido_tras_numero((unsigned char)*p))
        {
            return 0;
        }
    }

    if (count == 0)
    {
        return 0;
    }

    *out_mask = mask;
    *out_count = count;
    return 1;
}

static int solicitar_tipo_cancha_codigo(void)
{
    char entrada[64];

    while (1)
    {
        printf("Tipo de cancha:\n");
        printf("5) Futbol 5\n");
        printf("7) Futbol 7\n");
        printf("8) Futbol 8\n");
        printf("11) Futbol 11\n");
        printf("99) Otro\n");

        input_string_extended("Codigos (ej: 5 o 5-7-8-11): ", entrada, sizeof(entrada));
        trim_whitespace(entrada);
        if (entrada[0] == '\0')
        {
            printf("Opcion invalida.\n");
            continue;
        }

        int mask = 0;
        int count = 0;
        if (!parsear_seleccion_tipo_cancha(entrada, &mask, &count))
        {
            printf("Opcion invalida.\n");
            continue;
        }

        if (count == 1)
        {
            int codigo_unico = tipo_cancha_codigo_unico_desde_mask(mask);
            if (codigo_unico != 0)
            {
                return codigo_unico;
            }
        }

        return TIPO_CANCHA_MULTI_BASE + mask;
    }
}

static const char *texto_tipo_cancha(int codigo)
{
    if (codigo >= TIPO_CANCHA_MULTI_BASE)
    {
        int mask = codigo - TIPO_CANCHA_MULTI_BASE;
        static char buffer[100];
        buffer[0] = '\0';

        if (mask & (1 << 0))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Futbol 5");
        }
        if (mask & (1 << 1))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Futbol 7");
        }
        if (mask & (1 << 2))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Futbol 8");
        }
        if (mask & (1 << 3))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Futbol 11");
        }
        if (mask & (1 << 4))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Otro");
        }

        if (buffer[0] != '\0')
        {
            return buffer;
        }
    }

    switch (codigo)
    {
    case 5:
        return "Futbol 5";
    case 7:
        return "Futbol 7";
    case 8:
        return "Futbol 8";
    case 11:
        return "Futbol 11";
    case 99:
        return "Otro";
    default:
        return "No especificado";
    }
}

static int superficie_bit_desde_opcion(int opcion)
{
    switch (opcion)
    {
    case 1:
        return 1 << 0;
    case 2:
        return 1 << 1;
    case 3:
        return 1 << 2;
    case 4:
        return 1 << 3;
    default:
        return 0;
    }
}

static int contar_bits_superficie(int mask)
{
    int count = 0;
    for (int i = 0; i < 4; i++)
    {
        if (mask & (1 << i))
        {
            count++;
        }
    }
    return count;
}

static const char *saltar_separadores_lista(const char *cursor)
{
    const char *p = cursor;
    while (*p && es_separador_lista((unsigned char)*p))
    {
        p++;
    }
    return p;
}

static int parsear_codigo_lista(const char **cursor, int *out_codigo)
{
    if (!cursor || !*cursor || !out_codigo)
    {
        return 0;
    }

    const char *p = *cursor;
    if (!isdigit((unsigned char)*p))
    {
        return 0;
    }

    int codigo = 0;
    while (*p && isdigit((unsigned char)*p))
    {
        codigo = codigo * 10 + (*p - '0');
        p++;
    }

    if (!caracter_valido_tras_numero((unsigned char)*p))
    {
        return 0;
    }

    *out_codigo = codigo;
    *cursor = p;
    return 1;
}

static int aplicar_opcion_superficie(int opcion, int *mask, int *no_se)
{
    if (!mask || !no_se)
    {
        return 0;
    }

    if (opcion == 5)
    {
        *no_se = 1;
        return 1;
    }

    int bit = superficie_bit_desde_opcion(opcion);
    if (bit == 0)
    {
        return 0;
    }

    *mask |= bit;
    return 1;
}

static int parsear_seleccion_superficie(const char *entrada, int *out_mask, int *out_count, int *out_no_se)
{
    if (!entrada || !out_mask || !out_count || !out_no_se)
    {
        return 0;
    }

    int mask = 0;
    int no_se = 0;
    const char *p = entrada;

    while (*p)
    {
        p = saltar_separadores_lista(p);

        if (!*p)
        {
            break;
        }

        int opcion = 0;
        if (!parsear_codigo_lista(&p, &opcion))
        {
            return 0;
        }

        if (!aplicar_opcion_superficie(opcion, &mask, &no_se))
        {
            return 0;
        }
    }

    if (no_se && mask != 0)
    {
        return 0;
    }

    int count = no_se ? 1 : contar_bits_superficie(mask);
    if (count == 0)
    {
        return 0;
    }

    *out_mask = mask;
    *out_count = count;
    *out_no_se = no_se;
    return 1;
}

static int solicitar_superficie_codigo(void)
{
    char entrada[64];

    while (1)
    {
        printf("Tipo de superficie:\n");
        printf("1) Natural\n");
        printf("2) Sintetico\n");
        printf("3) Cemento\n");
        printf("4) Otra\n");
        printf("5) No se\n");

        input_string_extended("Opciones (ej: 1 o 1-2-3, 5=No se): ", entrada, sizeof(entrada));
        trim_whitespace(entrada);
        if (entrada[0] == '\0')
        {
            printf("Opcion invalida.\n");
            continue;
        }

        int mask = 0;
        int count = 0;
        int no_se = 0;
        if (!parsear_seleccion_superficie(entrada, &mask, &count, &no_se))
        {
            printf("Opcion invalida.\n");
            continue;
        }

        if (no_se)
        {
            return 0;
        }

        if (count == 1)
        {
            if (mask == (1 << 0))
            {
                return 1;
            }
            if (mask == (1 << 1))
            {
                return 2;
            }
            if (mask == (1 << 2))
            {
                return 3;
            }
            if (mask == (1 << 3))
            {
                return 4;
            }
        }

        return SUPERFICIE_MULTI_BASE + mask;
    }
}

static const char *texto_superficie(int codigo)
{
    if (codigo >= SUPERFICIE_MULTI_BASE)
    {
        int mask = codigo - SUPERFICIE_MULTI_BASE;
        static char buffer[80];
        buffer[0] = '\0';

        if (mask & (1 << 0))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Natural");
        }
        if (mask & (1 << 1))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Sintetico");
        }
        if (mask & (1 << 2))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Cemento");
        }
        if (mask & (1 << 3))
        {
            agregar_texto_tipo_en_buffer(buffer, sizeof(buffer), "Otra");
        }

        if (buffer[0] != '\0')
        {
            return buffer;
        }
    }

    switch (codigo)
    {
    case 1:
        return "Natural";
    case 2:
        return "Sintetico";
    case 3:
        return "Cemento";
    case 4:
        return "Otra";
    default:
        return "No especificada";
    }
}

static int solicitar_estado_techada_codigo(void)
{
    while (1)
    {
        printf("Cancha techada:\n");
        printf("1) Si\n");
        printf("2) No\n");
        printf("3) No se\n");

        int opcion = input_int("Opcion: ");
        switch (opcion)
        {
        case 1:
            return 1;
        case 2:
            return 0;
        case 3:
            return 2;
        default:
            printf("Opcion invalida.\n");
            break;
        }
    }
}

static const char *texto_estado_techada(int estado)
{
    switch (estado)
    {
    case 1:
        return "SI";
    case 0:
        return "NO";
    default:
        return "NO SE";
    }
}

static int solicitar_si_no(const char *titulo)
{
    while (1)
    {
        printf("%s\n", titulo);
        printf("1) Si\n");
        printf("2) No\n");
        int opcion = input_int("Opcion: ");
        if (opcion == 1)
        {
            return 1;
        }
        if (opcion == 2)
        {
            return 0;
        }
        printf("Opcion invalida.\n");
    }
}

static int parsear_dos_digitos(const char *texto, int *out_valor)
{
    if (!texto || !out_valor)
    {
        return 0;
    }

    if (!isdigit((unsigned char)texto[0]) || !isdigit((unsigned char)texto[1]))
    {
        return 0;
    }

    *out_valor = (texto[0] - '0') * 10 + (texto[1] - '0');
    return 1;
}

static int parsear_hora_hhmm(const char *texto, int *out_minutos)
{
    if (!texto || !out_minutos)
    {
        return 0;
    }

    size_t len = strlen_s(texto, 6);
    if (!((len == 4 && texto[1] == ':') || (len == 5 && texto[2] == ':')))
    {
        return 0;
    }

    int hh = 0;
    int mm = 0;

    if (len == 4 && texto[1] == ':')
    {
        if (!isdigit((unsigned char)texto[0]) || !parsear_dos_digitos(&texto[2], &mm))
        {
            return 0;
        }
        hh = (texto[0] - '0');
    }
    else if (len == 5 && texto[2] == ':')
    {
        if (!parsear_dos_digitos(texto, &hh) || !parsear_dos_digitos(&texto[3], &mm))
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }

    if (hh < 0 || hh > 23 || mm < 0 || mm > 59)
    {
        return 0;
    }

    *out_minutos = hh * 60 + mm;
    return 1;
}

static int solicitar_hora_minutos(const char *prompt)
{
    char hora[16];
    int minutos = -1;
    while (1)
    {
        input_string_extended(prompt, hora, sizeof(hora));
        trim_whitespace(hora);
        if (parsear_hora_hhmm(hora, &minutos))
        {
            return minutos;
        }
        printf("Formato invalido. Use H:MM o HH:MM (ejemplo 8:00 o 09:30).\n");
    }
}

static void formatear_hora_minutos(int minutos, char *buffer, size_t size)
{
    if (!buffer || size == 0)
    {
        return;
    }

    if (minutos < 0 || minutos > (23 * 60 + 59))
    {
        snprintf(buffer, size, "--:--");
        return;
    }

    int hh = minutos / 60;
    int mm = minutos % 60;
    snprintf(buffer, size, "%02d:%02d", hh, mm);
}

static int solicitar_precio_centavos(const char *prompt)
{
    while (1)
    {
        double monto = input_double(prompt);
        if (monto < 0.0)
        {
            printf("El monto no puede ser negativo.\n");
            continue;
        }
        return (int)(monto * 100.0 + 0.5);
    }
}

typedef struct
{
    int vestuarios;
    int duchas;
    int buffet;
    int estacionamiento;
} CanchaServicios;

typedef struct
{
    char nombre[100];
    char telefono[40];
    char direccion[200];
    char localidad[100];
    int tipo_cancha_codigo;
    int superficie_codigo;
    int techada_estado;
    int tiene_iluminacion;
    int horario_apertura_min;
    int horario_cierre_min;
    int precio_hora_dia_centavos;
    int precio_hora_noche_centavos;
    CanchaServicios servicios;
    int cantidad_canchas;
    char estado[50];
    char descripcion[200];
    char contacto_alt[120];
    int activa;
} CanchaInfoDetalle;

static void solicitar_datos_comunes_cancha(CanchaInfoDetalle *info, int incluir_nombre)
{
    if (!info)
    {
        return;
    }

    if (incluir_nombre)
    {
        solicitar_nombre_cancha("Nombre de la cancha: ", info->nombre, sizeof(info->nombre));
    }

    solicitar_telefono_no_vacio("Numero de telefono: ", info->telefono, sizeof(info->telefono));
    solicitar_campo_no_vacio("Direccion: ", info->direccion, sizeof(info->direccion));
    solicitar_campo_no_vacio("Localidad/Zona: ", info->localidad, sizeof(info->localidad));

    info->tipo_cancha_codigo = solicitar_tipo_cancha_codigo();
    info->superficie_codigo = solicitar_superficie_codigo();
    info->techada_estado = solicitar_estado_techada_codigo();
    info->tiene_iluminacion = solicitar_si_no("Tiene iluminacion?");
    info->horario_apertura_min = solicitar_hora_minutos("Horario de apertura (HH:MM): ");
    info->horario_cierre_min = solicitar_hora_minutos("Horario de cierre (HH:MM): ");
    info->precio_hora_dia_centavos = solicitar_precio_centavos("Precio por hora (Dia): ");
    info->precio_hora_noche_centavos = solicitar_precio_centavos("Precio por hora (Noche): ");
    info->servicios.vestuarios = solicitar_si_no("Tiene vestuarios?");
    info->servicios.duchas = solicitar_si_no("Tiene duchas?");
    info->servicios.buffet = solicitar_si_no("Tiene buffet?");
    info->servicios.estacionamiento = solicitar_si_no("Tiene estacionamiento?");
    info->cantidad_canchas = input_int("Cantidad de canchas del complejo: ");
    if (info->cantidad_canchas <= 0)
    {
        info->cantidad_canchas = 1;
    }

    solicitar_campo_no_vacio("Estado (ej: habilitada, en mantenimiento): ", info->estado, sizeof(info->estado));
    input_string("Descripcion breve: ", info->descripcion, sizeof(info->descripcion));
    trim_whitespace(info->descripcion);
    solicitar_campo_no_vacio("Contacto alternativo (WhatsApp/Instagram): ", info->contacto_alt, sizeof(info->contacto_alt));
}

static const char *texto_o_defecto(const char *valor, const char *defecto)
{
    if (valor && valor[0] != '\0')
    {
        return valor;
    }
    return defecto;
}

static int cargar_info_cancha_detalle(int id, CanchaInfoDetalle *info)
{
    if (!info)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "SELECT nombre, IFNULL(telefono, ''), IFNULL(direccion, ''), IFNULL(localidad, ''), "
                       "IFNULL(tipo_cancha_codigo, 0), IFNULL(superficie_codigo, 0), IFNULL(techada_estado_codigo, 2), "
                       "IFNULL(tiene_iluminacion, 0), IFNULL(horario_apertura_min, -1), IFNULL(horario_cierre_min, -1), "
                       "IFNULL(precio_hora_dia_centavos, 0), IFNULL(precio_hora_noche_centavos, 0), "
                       "IFNULL(tiene_vestuarios, 0), IFNULL(tiene_duchas, 0), IFNULL(tiene_buffet, 0), "
                       "IFNULL(tiene_estacionamiento, 0), IFNULL(cantidad_canchas, 1), IFNULL(estado, ''), "
                       "IFNULL(descripcion, ''), IFNULL(contacto_alt, ''), IFNULL(activa, 1) "
                       "FROM cancha WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    snprintf(info->nombre, sizeof(info->nombre), "%s", (const char *)sqlite3_column_text(stmt, 0));
    snprintf(info->telefono, sizeof(info->telefono), "%s", (const char *)sqlite3_column_text(stmt, 1));
    snprintf(info->direccion, sizeof(info->direccion), "%s", (const char *)sqlite3_column_text(stmt, 2));
    snprintf(info->localidad, sizeof(info->localidad), "%s", (const char *)sqlite3_column_text(stmt, 3));
    info->tipo_cancha_codigo = sqlite3_column_int(stmt, 4);
    info->superficie_codigo = sqlite3_column_int(stmt, 5);
    info->techada_estado = sqlite3_column_int(stmt, 6);
    info->tiene_iluminacion = sqlite3_column_int(stmt, 7) ? 1 : 0;
    info->horario_apertura_min = sqlite3_column_int(stmt, 8);
    info->horario_cierre_min = sqlite3_column_int(stmt, 9);
    info->precio_hora_dia_centavos = sqlite3_column_int(stmt, 10);
    info->precio_hora_noche_centavos = sqlite3_column_int(stmt, 11);
    info->servicios.vestuarios = sqlite3_column_int(stmt, 12) ? 1 : 0;
    info->servicios.duchas = sqlite3_column_int(stmt, 13) ? 1 : 0;
    info->servicios.buffet = sqlite3_column_int(stmt, 14) ? 1 : 0;
    info->servicios.estacionamiento = sqlite3_column_int(stmt, 15) ? 1 : 0;
    info->cantidad_canchas = sqlite3_column_int(stmt, 16);
    if (info->cantidad_canchas <= 0)
    {
        info->cantidad_canchas = 1;
    }
    snprintf(info->estado, sizeof(info->estado), "%s", (const char *)sqlite3_column_text(stmt, 17));
    snprintf(info->descripcion, sizeof(info->descripcion), "%s", (const char *)sqlite3_column_text(stmt, 18));
    snprintf(info->contacto_alt, sizeof(info->contacto_alt), "%s", (const char *)sqlite3_column_text(stmt, 19));
    info->activa = sqlite3_column_int(stmt, 20) == 1;

    sqlite3_finalize(stmt);
    return 1;
}

static void imprimir_info_cancha_detalle(int id, const CanchaInfoDetalle *info)
{
    char hora_apertura[16];
    char hora_cierre[16];
    formatear_hora_minutos(info->horario_apertura_min, hora_apertura, sizeof(hora_apertura));
    formatear_hora_minutos(info->horario_cierre_min, hora_cierre, sizeof(hora_cierre));

    printf("========================================\n");
    printf("ID                 : %d\n", id);
    printf("Nombre             : %s\n", texto_o_defecto(info->nombre, "(sin dato)"));
    printf("Telefono           : %s\n", texto_o_defecto(info->telefono, "(sin dato)"));
    printf("Direccion          : %s\n", texto_o_defecto(info->direccion, "(sin dato)"));
    printf("Localidad          : %s\n", texto_o_defecto(info->localidad, "(sin dato)"));
    printf("Tipo de Cancha     : %s\n", texto_tipo_cancha(info->tipo_cancha_codigo));
    printf("Superficie         : %s\n", texto_superficie(info->superficie_codigo));
    printf("Techada            : %s\n", texto_estado_techada(info->techada_estado));
    printf("Iluminacion        : %s\n", info->tiene_iluminacion ? "SI" : "NO");
    printf("Horario            : %s - %s\n", hora_apertura, hora_cierre);
    printf("Precio Hora Dia    : %.2f\n", (double)info->precio_hora_dia_centavos / 100.0);
    printf("Precio Hora Noche  : %.2f\n", (double)info->precio_hora_noche_centavos / 100.0);
    printf("Vestuarios         : %s\n", info->servicios.vestuarios ? "SI" : "NO");
    printf("Duchas             : %s\n", info->servicios.duchas ? "SI" : "NO");
    printf("Buffet             : %s\n", info->servicios.buffet ? "SI" : "NO");
    printf("Estacionamiento    : %s\n", info->servicios.estacionamiento ? "SI" : "NO");
    printf("Cantidad Canchas   : %d\n", info->cantidad_canchas);
    printf("Estado Pasto       : %s\n", texto_o_defecto(info->estado, "(sin dato)"));
    printf("Descripcion        : %s\n", texto_o_defecto(info->descripcion, "(sin dato)"));
    printf("Contacto Alterno   : %s\n", texto_o_defecto(info->contacto_alt, "(sin dato)"));
    printf("Estado             : %s\n", info->activa ? "ACTIVA" : "INACTIVA");
    printf("========================================\n");
}

static int cancha_necesita_completar_info(const CanchaInfoDetalle *info)
{
    if (!info)
    {
        return 0;
    }

    const char *textos[] =
    {
        info->telefono,
        info->direccion,
        info->localidad,
        info->estado,
        info->descripcion,
        info->contacto_alt
    };

    for (size_t i = 0; i < sizeof(textos) / sizeof(textos[0]); i++)
    {
        if (textos[i][0] != '\0')
        {
            return 0;
        }
    }

    const int condiciones[] =
    {
        info->tipo_cancha_codigo == 0,
        info->superficie_codigo == 0,
        info->techada_estado == 2,
        info->tiene_iluminacion == 0,
        info->horario_apertura_min < 0,
        info->horario_cierre_min < 0,
        info->precio_hora_dia_centavos == 0,
        info->precio_hora_noche_centavos == 0,
        info->servicios.vestuarios == 0,
        info->servicios.duchas == 0,
        info->servicios.buffet == 0,
        info->servicios.estacionamiento == 0,
        info->cantidad_canchas <= 1
    };

    for (size_t i = 0; i < sizeof(condiciones) / sizeof(condiciones[0]); i++)
    {
        if (!condiciones[i])
        {
            return 0;
        }
    }

    return 1;
}

static int actualizar_campo_texto_cancha(int id, const char *campo, const char *valor)
{
    char sql[192];
    sqlite3_stmt *stmt;

    snprintf(sql, sizeof(sql), "UPDATE cancha SET %s = ? WHERE id = ?", campo);
    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static int actualizar_campo_entero_cancha(int id, const char *campo, int valor)
{
    char sql[192];
    sqlite3_stmt *stmt;

    snprintf(sql, sizeof(sql), "UPDATE cancha SET %s = ? WHERE id = ?", campo);
    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static int completar_informacion_cancha(int id)
{
    CanchaInfoDetalle info = {0};
    solicitar_datos_comunes_cancha(&info, 0);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "UPDATE cancha "
                       "SET telefono = ?, direccion = ?, localidad = ?, tipo_cancha_codigo = ?, "
                       "superficie_codigo = ?, techada_estado_codigo = ?, tiene_iluminacion = ?, "
                       "horario_apertura_min = ?, horario_cierre_min = ?, precio_hora_dia_centavos = ?, "
                       "precio_hora_noche_centavos = ?, tiene_vestuarios = ?, tiene_duchas = ?, tiene_buffet = ?, "
                       "tiene_estacionamiento = ?, cantidad_canchas = ?, estado = ?, descripcion = ?, contacto_alt = ? "
                       "WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, info.telefono, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, info.direccion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, info.localidad, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, info.tipo_cancha_codigo);
    sqlite3_bind_int(stmt, 5, info.superficie_codigo);
    sqlite3_bind_int(stmt, 6, info.techada_estado);
    sqlite3_bind_int(stmt, 7, info.tiene_iluminacion);
    sqlite3_bind_int(stmt, 8, info.horario_apertura_min);
    sqlite3_bind_int(stmt, 9, info.horario_cierre_min);
    sqlite3_bind_int(stmt, 10, info.precio_hora_dia_centavos);
    sqlite3_bind_int(stmt, 11, info.precio_hora_noche_centavos);
    sqlite3_bind_int(stmt, 12, info.servicios.vestuarios);
    sqlite3_bind_int(stmt, 13, info.servicios.duchas);
    sqlite3_bind_int(stmt, 14, info.servicios.buffet);
    sqlite3_bind_int(stmt, 15, info.servicios.estacionamiento);
    sqlite3_bind_int(stmt, 16, info.cantidad_canchas);
    sqlite3_bind_text(stmt, 17, info.estado, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 18, info.descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 19, info.contacto_alt, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 20, id);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static int contar_partidos_por_cancha(int cancha_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM partido WHERE cancha_id = ?"))
    {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, cancha_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

static int contar_total_canchas_activas(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM cancha WHERE IFNULL(activa, 1) = 1"))
    {
        return -1;
    }

    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return total;
}

static int cancha_esta_activa(int cancha_id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT IFNULL(activa, 1) FROM cancha WHERE id = ?"))
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

static int actualizar_estado_cancha(int cancha_id, int activa)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE cancha SET activa = ? WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, activa ? 1 : 0);
    sqlite3_bind_int(stmt, 2, cancha_id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static void listar_canchas_excluyendo(int cancha_excluida)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM cancha WHERE id <> ? AND IFNULL(activa, 1) = 1 ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, cancha_excluida);

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d - %s",
                                sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("canchas destino");

    sqlite3_finalize(stmt);
}

static int ejecutar_sql_simple(const char *sql)
{
    char *errmsg = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        if (errmsg)
        {
            printf("Error SQL: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
        return 0;
    }
    return 1;
}

static int confirmar_borrado_irreversible_cancha(int cancha_id, const char *detalle)
{
    char esperado[64] = {0};
    char ingreso[64] = {0};

    printf("\n================ ADVERTENCIA =================\n");
    printf("Esta operacion es IRREVERSIBLE.\n");
    if (detalle && detalle[0] != '\0')
    {
        printf("%s\n", detalle);
    }
    printf("=============================================\n\n");

    if (!confirmar("Desea continuar?"))
    {
        return 0;
    }

    snprintf(esperado, sizeof(esperado), "BORRAR CANCHA %d", cancha_id);
    printf("Para confirmar, escriba exactamente: %s\n", esperado);
    input_string("Confirmacion: ", ingreso, sizeof(ingreso));
    trim_whitespace(ingreso);

    if (strcmp(ingreso, esperado) != 0)
    {
        printf("Confirmacion incorrecta. Operacion cancelada.\n");
        pause_console();
        return 0;
    }

    return 1;
}

static int reasignar_partidos_y_eliminar_cancha(int cancha_origen, int cancha_destino)
{
    if (!ejecutar_sql_simple("BEGIN IMMEDIATE TRANSACTION;"))
    {
        return 0;
    }

    sqlite3_stmt *stmt_update = NULL;
    sqlite3_stmt *stmt_delete = NULL;

    if (!preparar_stmt(&stmt_update, "UPDATE partido SET cancha_id = ? WHERE cancha_id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_update, 1, cancha_destino);
    sqlite3_bind_int(stmt_update, 2, cancha_origen);
    if (sqlite3_step(stmt_update) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_update);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_update);

    if (!preparar_stmt(&stmt_delete, "DELETE FROM cancha WHERE id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete, 1, cancha_origen);
    if (sqlite3_step(stmt_delete) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_delete);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_delete);

    if (!ejecutar_sql_simple("COMMIT;"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }

    return 1;
}

static int eliminar_cancha_y_partidos_asociados(int cancha_id)
{
    if (!ejecutar_sql_simple("BEGIN IMMEDIATE TRANSACTION;"))
    {
        return 0;
    }

    sqlite3_stmt *stmt_delete_partidos = NULL;
    sqlite3_stmt *stmt_delete_cancha = NULL;

    if (!preparar_stmt(&stmt_delete_partidos, "DELETE FROM partido WHERE cancha_id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete_partidos, 1, cancha_id);
    if (sqlite3_step(stmt_delete_partidos) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_delete_partidos);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_delete_partidos);

    if (!preparar_stmt(&stmt_delete_cancha, "DELETE FROM cancha WHERE id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete_cancha, 1, cancha_id);
    if (sqlite3_step(stmt_delete_cancha) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt_delete_cancha);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_finalize(stmt_delete_cancha);

    if (!ejecutar_sql_simple("COMMIT;"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }

    return 1;
}

static int abrir_imagen_en_sistema(const char *ruta)
{
    if (!ruta || ruta[0] == '\0')
    {
        return 0;
    }

    char cmd[1400];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", ruta);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" >/dev/null 2>&1", ruta);
#endif
    return system(cmd) == 0;
}

static int obtener_ruta_imagen_cancha_db(int id, char *ruta, size_t size)
{
    if (!ruta || size == 0)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT imagen_ruta FROM cancha WHERE id=?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *valor = sqlite3_column_text(stmt, 0);
        if (valor && valor[0] != '\0' && strncpy_s(ruta, size, (const char *)valor, _TRUNCATE) == 0)
        {
            ok = 1;
        }
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int construir_ruta_absoluta_imagen_cancha_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!obtener_ruta_imagen_cancha_db(id, ruta_db, sizeof(ruta_db)))
    {
        return 0;
    }

    return db_resolve_image_absolute_path(ruta_db, ruta_absoluta, size);
}

static void solicitar_nombre_cancha(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string(prompt, buffer, size);
        trim_whitespace(buffer);

        if (buffer[0] != '\0')
            return;

        printf("El nombre no puede estar vacio.\n");
    }
}

static int cargar_imagen_para_cancha_id(int id)
{
    return app_cargar_imagen_entidad(id, "cancha", "mifutbol_imagen_sel_cancha.txt");
}

void cargar_imagen_cancha()
{
    mostrar_pantalla("CARGAR IMAGEN DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas_simple();
    int id = input_int("\nID de cancha (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    if (!cargar_imagen_para_cancha_id(id))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    pause_console();
}

void ver_imagen_cancha()
{
    mostrar_pantalla("VER IMAGEN DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas_simple();
    int id = input_int("\nID de cancha (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    char ruta_absoluta[1200] = {0};
    if (!construir_ruta_absoluta_imagen_cancha_por_id(id, ruta_absoluta, sizeof(ruta_absoluta)))
    {
        printf("No se encontro imagen cargada para esa cancha.\n");
        pause_console();
        return;
    }

    if (!abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen en el sistema.\n");
        pause_console();
        return;
    }

    printf("Abriendo imagen...\n");
    pause_console();
}

void crear_cancha()
{
    CanchaInfoDetalle info = {0};
    solicitar_datos_comunes_cancha(&info, 1);

    long long id = obtener_siguiente_id("cancha");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "INSERT INTO cancha(id, nombre, telefono, direccion, localidad, tipo_cancha_codigo, "
                       "superficie_codigo, techada_estado_codigo, tiene_iluminacion, horario_apertura_min, "
                       "horario_cierre_min, precio_hora_dia_centavos, precio_hora_noche_centavos, "
                       "tiene_vestuarios, tiene_duchas, tiene_buffet, tiene_estacionamiento, cantidad_canchas, "
                       "estado, descripcion, contacto_alt) "
                       "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"))
    {
        printf("Error al crear la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, (int)id);
    sqlite3_bind_text(stmt, 2, info.nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, info.telefono, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, info.direccion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, info.localidad, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, info.tipo_cancha_codigo);
    sqlite3_bind_int(stmt, 7, info.superficie_codigo);
    sqlite3_bind_int(stmt, 8, info.techada_estado);
    sqlite3_bind_int(stmt, 9, info.tiene_iluminacion);
    sqlite3_bind_int(stmt, 10, info.horario_apertura_min);
    sqlite3_bind_int(stmt, 11, info.horario_cierre_min);
    sqlite3_bind_int(stmt, 12, info.precio_hora_dia_centavos);
    sqlite3_bind_int(stmt, 13, info.precio_hora_noche_centavos);
    sqlite3_bind_int(stmt, 14, info.servicios.vestuarios);
    sqlite3_bind_int(stmt, 15, info.servicios.duchas);
    sqlite3_bind_int(stmt, 16, info.servicios.buffet);
    sqlite3_bind_int(stmt, 17, info.servicios.estacionamiento);
    sqlite3_bind_int(stmt, 18, info.cantidad_canchas);
    sqlite3_bind_text(stmt, 19, info.estado, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 20, info.descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 21, info.contacto_alt, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada cancha id=%lld nombre=%.180s", id, info.nombre);
        app_log_event("CANCHA", log_msg);
        if (confirmar("Desea cargar imagen para esta cancha ahora?") &&
                !cargar_imagen_para_cancha_id((int)id))
        {
            printf("No se pudo cargar la imagen en este momento.\n");
        }
        mostrar_alerta_operacion("Cancha", "Creada", info.nombre);
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear cancha nombre=%.180s", info.nombre);
        app_log_event("CANCHA", log_msg);
        printf("Error al crear la cancha.\n");
        pause_console();
    }
}

void listar_canchas()
{
    clear_screen();
    print_header("LISTADO DE CANCHAS");
    app_log_event("CANCHA", "Listado de canchas consultado");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT c.id, c.nombre, c.activa, "
        "COUNT(p.id), "
        "IFNULL(SUM(p.goles), 0), "
        "IFNULL(SUM(p.asistencias), 0) "
        "FROM cancha c "
        "LEFT JOIN partido p ON c.id = p.cancha_id "
        "GROUP BY c.id, c.nombre "
        "ORDER BY c.id;";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    int usar_unicode = consola_soporta_unicode();
    const char *sep = usar_unicode ? " \u2502 " : " | ";

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        hay |= ui_print_stats_row_from_stmt(stmt, sep);
    }

    if (!hay)
        mostrar_no_hay_registros("canchas cargadas");

    sqlite3_finalize(stmt);
    pause_console();
}

static void listar_canchas_simple()
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id, nombre, IFNULL(activa, 1), IFNULL(tipo_cancha_codigo, 0) FROM cancha ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *estado = sqlite3_column_int(stmt, 2) == 1 ? "ACTIVA" : "INACTIVA";
        int tipo_codigo = sqlite3_column_int(stmt, 3);
        ui_printf_centered_line("%d - %s [%s] Tipo: %s",
                                sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1),
                                estado,
                                texto_tipo_cancha(tipo_codigo));
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("canchas cargadas");

    sqlite3_finalize(stmt);
}

static int listar_canchas_con_info_pendiente(void)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM cancha ORDER BY id"))
    {
        printf("Error al consultar la base de datos.\n");
        return -1;
    }

    int pendientes = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);

        CanchaInfoDetalle info;
        if (!cargar_info_cancha_detalle(id, &info))
        {
            continue;
        }

        if (cancha_necesita_completar_info(&info))
        {
            ui_printf_centered_line("%d - %s", id, nombre);
            pendientes++;
        }
    }

    sqlite3_finalize(stmt);
    return pendientes;
}

static void procesar_eliminacion_reasignando_partidos_cancha(int id, int partidos_asociados)
{
    int total_canchas = contar_total_canchas_activas();
    if (total_canchas <= 1)
    {
        printf("No hay otra cancha disponible para reasignar partidos.\n");
        pause_console();
        return;
    }

    printf("\nCanchas disponibles para reasignar:\n");
    listar_canchas_excluyendo(id);
    printf("\n");

    int cancha_destino = input_int("ID cancha destino (0 para cancelar): ");
    if (cancha_destino == 0)
    {
        return;
    }

    if (cancha_destino == id || !existe_id("cancha", cancha_destino) || !cancha_esta_activa(cancha_destino))
    {
        printf("Cancha destino invalida.\n");
        pause_console();
        return;
    }

    char detalle[160] = {0};
    snprintf(detalle, sizeof(detalle),
             "Se reasignaran %d partido(s) a la cancha %d y se eliminara la cancha %d.",
             partidos_asociados, cancha_destino, id);
    if (!confirmar_borrado_irreversible_cancha(id, detalle))
    {
        return;
    }

    if (!reasignar_partidos_y_eliminar_cancha(id, cancha_destino))
    {
        printf("No se pudo completar la reasignacion y eliminacion.\n");
        pause_console();
        return;
    }

    mostrar_alerta_operacion("Cancha", "Reasignada y Eliminada", NULL);
}

static void procesar_eliminacion_con_partidos_asociados_cancha(int id, int partidos_asociados)
{
    char detalle[160] = {0};
    snprintf(detalle, sizeof(detalle),
             "Se eliminara la cancha %d y TODOS sus %d partido(s) asociados.",
             id, partidos_asociados);
    if (!confirmar_borrado_irreversible_cancha(id, detalle))
    {
        return;
    }

    if (!eliminar_cancha_y_partidos_asociados(id))
    {
        printf("No se pudo eliminar la cancha y sus partidos asociados.\n");
        pause_console();
        return;
    }

    mostrar_alerta_operacion("Cancha", "Eliminada con Partidos Asociados", NULL);
}

static void procesar_retiro_cancha(int id)
{
    if (!confirmar("Se retirara la cancha: no aparecera para partidos nuevos, pero conservara historial. Continuar?"))
    {
        return;
    }

    if (!actualizar_estado_cancha(id, 0))
    {
        printf("No se pudo marcar la cancha como inactiva.\n");
        pause_console();
        return;
    }

    mostrar_alerta_operacion("Cancha", "Retirada (Inactiva)", NULL);
}

static void procesar_cancha_con_partidos_asociados(int id, int partidos_asociados)
{
    printf("La cancha esta asociada a %d partido(s).\n", partidos_asociados);
    printf("Elija una opcion:\n");
    printf("1) Reasignar esos partidos a otra cancha y eliminar esta cancha\n");
    printf("2) Eliminar esta cancha y TODOS los partidos asociados\n");
    printf("3) Retirar cancha (marcar INACTIVA y conservar historial)\n");
    printf("0) Cancelar\n");

    int opcion = input_int("Opcion: ");
    if (opcion == 0)
    {
        return;
    }

    switch (opcion)
    {
    case 1:
        procesar_eliminacion_reasignando_partidos_cancha(id, partidos_asociados);
        return;
    case 2:
        procesar_eliminacion_con_partidos_asociados_cancha(id, partidos_asociados);
        return;
    case 3:
        procesar_retiro_cancha(id);
        return;
    default:
        printf("Opcion invalida.\n");
        pause_console();
        return;
    }
}

static void eliminar_cancha_sin_partidos_asociados(int id)
{
    if (!confirmar_borrado_irreversible_cancha(id, "Se eliminara la cancha seleccionada."))
    {
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "DELETE FROM cancha WHERE id = ?"))
    {
        printf("Error al eliminar la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    mostrar_alerta_operacion("Cancha", "Eliminada", NULL);
}

void eliminar_cancha()
{
    mostrar_pantalla("ELIMINAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas_simple();
    printf("\n");

    int id = input_int("ID Cancha a Eliminar (0 para cancelar): ");

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    if (!cancha_esta_activa(id))
    {
        printf("La cancha seleccionada ya esta inactiva.\n");
        pause_console();
        return;
    }

    int partidos_asociados = contar_partidos_por_cancha(id);
    if (partidos_asociados < 0)
    {
        printf("No se pudo verificar si la cancha esta asociada a partidos.\n");
        pause_console();
        return;
    }

    if (partidos_asociados > 0)
    {
        procesar_cancha_con_partidos_asociados(id, partidos_asociados);
        return;
    }

    eliminar_cancha_sin_partidos_asociados(id);
}

static void imprimir_menu_modificar_cancha(const CanchaInfoDetalle *info, int mostrar_completar_info)
{
    char hora_apertura[16];
    char hora_cierre[16];
    formatear_hora_minutos(info->horario_apertura_min, hora_apertura, sizeof(hora_apertura));
    formatear_hora_minutos(info->horario_cierre_min, hora_cierre, sizeof(hora_cierre));

    clear_screen();
    print_header("MODIFICAR CANCHA");
    printf("Cancha seleccionada: %s\n\n", info->nombre);
    printf("1) Nombre: %s\n", texto_o_defecto(info->nombre, "(sin dato)"));
    printf("2) Numero de telefono: %s\n", texto_o_defecto(info->telefono, "(sin dato)"));
    printf("3) Direccion: %s\n", texto_o_defecto(info->direccion, "(sin dato)"));
    printf("4) Localidad/Zona: %s\n", texto_o_defecto(info->localidad, "(sin dato)"));
    printf("5) Tipo de cancha: %s\n", texto_tipo_cancha(info->tipo_cancha_codigo));
    printf("6) Superficie: %s\n", texto_superficie(info->superficie_codigo));
    printf("7) Estado Techada: %s\n", texto_estado_techada(info->techada_estado));
    printf("8) Iluminacion: %s\n", info->tiene_iluminacion ? "SI" : "NO");
    printf("9) Horario apertura: %s\n", hora_apertura);
    printf("10) Horario cierre: %s\n", hora_cierre);
    printf("11) Precio hora (Dia): %.2f\n", (double)info->precio_hora_dia_centavos / 100.0);
    printf("12) Precio hora (Noche): %.2f\n", (double)info->precio_hora_noche_centavos / 100.0);
    printf("13) Tiene vestuarios: %s\n", info->servicios.vestuarios ? "SI" : "NO");
    printf("14) Tiene duchas: %s\n", info->servicios.duchas ? "SI" : "NO");
    printf("15) Tiene buffet: %s\n", info->servicios.buffet ? "SI" : "NO");
    printf("16) Tiene estacionamiento: %s\n", info->servicios.estacionamiento ? "SI" : "NO");
    printf("17) Cantidad de canchas: %d\n", info->cantidad_canchas);
    printf("18) Estado: %s\n", texto_o_defecto(info->estado, "(sin dato)"));
    printf("19) Descripcion: %s\n", texto_o_defecto(info->descripcion, "(sin dato)"));
    printf("20) Contacto alternativo: %s\n", texto_o_defecto(info->contacto_alt, "(sin dato)"));
    if (mostrar_completar_info)
    {
        printf("21) Completar Informacion\n");
    }
    printf("0) Volver\n\n");
}

static int procesar_opcion_modificar_cancha(int id, int opcion, int mostrar_completar_info, int *actualizado)
{
    if (!actualizado)
    {
        return 0;
    }

    *actualizado = 0;

    switch (opcion)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 18:
    case 19:
    case 20:
    {
        char valor[200] = {0};
        const char *campo = NULL;

        if (opcion == 1)
        {
            solicitar_nombre_cancha("Nuevo nombre: ", valor, sizeof(valor));
            campo = "nombre";
        }
        else if (opcion == 2)
        {
            solicitar_telefono_no_vacio("Nuevo numero de telefono: ", valor, sizeof(valor));
            campo = "telefono";
        }
        else if (opcion == 3)
        {
            solicitar_campo_no_vacio("Nueva direccion: ", valor, sizeof(valor));
            campo = "direccion";
        }
        else if (opcion == 4)
        {
            solicitar_campo_no_vacio("Nueva localidad/zona: ", valor, sizeof(valor));
            campo = "localidad";
        }
        else if (opcion == 18)
        {
            solicitar_campo_no_vacio("Estado (ej: habilitada, en mantenimiento): ", valor, sizeof(valor));
            campo = "estado";
        }
        else if (opcion == 19)
        {
            input_string("Descripcion breve: ", valor, sizeof(valor));
            trim_whitespace(valor);
            campo = "descripcion";
        }
        else
        {
            solicitar_campo_no_vacio("Nuevo contacto alternativo: ", valor, sizeof(valor));
            campo = "contacto_alt";
        }

        *actualizado = actualizar_campo_texto_cancha(id, campo, valor);
        return 1;
    }
    case 5:
        *actualizado = actualizar_campo_entero_cancha(id, "tipo_cancha_codigo", solicitar_tipo_cancha_codigo());
        return 1;
    case 6:
        *actualizado = actualizar_campo_entero_cancha(id, "superficie_codigo", solicitar_superficie_codigo());
        return 1;
    case 7:
        *actualizado = actualizar_campo_entero_cancha(id, "techada_estado_codigo", solicitar_estado_techada_codigo());
        return 1;
    case 8:
        *actualizado = actualizar_campo_entero_cancha(id, "tiene_iluminacion", solicitar_si_no("Tiene iluminacion?"));
        return 1;
    case 9:
        *actualizado = actualizar_campo_entero_cancha(id, "horario_apertura_min", solicitar_hora_minutos("Horario de apertura (HH:MM): "));
        return 1;
    case 10:
        *actualizado = actualizar_campo_entero_cancha(id, "horario_cierre_min", solicitar_hora_minutos("Horario de cierre (HH:MM): "));
        return 1;
    case 11:
        *actualizado = actualizar_campo_entero_cancha(id, "precio_hora_dia_centavos", solicitar_precio_centavos("Precio por hora (Dia): "));
        return 1;
    case 12:
        *actualizado = actualizar_campo_entero_cancha(id, "precio_hora_noche_centavos", solicitar_precio_centavos("Precio por hora (Noche): "));
        return 1;
    case 13:
        *actualizado = actualizar_campo_entero_cancha(id, "tiene_vestuarios", solicitar_si_no("Tiene vestuarios?"));
        return 1;
    case 14:
        *actualizado = actualizar_campo_entero_cancha(id, "tiene_duchas", solicitar_si_no("Tiene duchas?"));
        return 1;
    case 15:
        *actualizado = actualizar_campo_entero_cancha(id, "tiene_buffet", solicitar_si_no("Tiene buffet?"));
        return 1;
    case 16:
        *actualizado = actualizar_campo_entero_cancha(id, "tiene_estacionamiento", solicitar_si_no("Tiene estacionamiento?"));
        return 1;
    case 17:
    {
        int cantidad = input_int("Cantidad de canchas del complejo: ");
        if (cantidad <= 0)
        {
            cantidad = 1;
        }
        *actualizado = actualizar_campo_entero_cancha(id, "cantidad_canchas", cantidad);
        return 1;
    }
    case 21:
        if (mostrar_completar_info)
        {
            *actualizado = completar_informacion_cancha(id);
            return 1;
        }

        printf("La cancha ya tiene informacion suficiente para editar campo por campo.\n");
        pause_console();
        return 0;
    default:
        printf("Opcion invalida.\n");
        pause_console();
        return 0;
    }

}
void modificar_cancha()
{
    mostrar_pantalla("MODIFICAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas_simple();
    printf("\n");

    int id = input_int("ID Cancha a Modificar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    while (1)
    {
        CanchaInfoDetalle info;
        if (!cargar_info_cancha_detalle(id, &info))
        {
            printf("No se pudo cargar la informacion actual de la cancha.\n");
            pause_console();
            return;
        }

        int mostrar_completar_info = cancha_necesita_completar_info(&info);
        imprimir_menu_modificar_cancha(&info, mostrar_completar_info);

        int opcion = input_int("Opcion: ");
        if (opcion == 0)
        {
            return;
        }

        int actualizado = 0;
        if (!procesar_opcion_modificar_cancha(id, opcion, mostrar_completar_info, &actualizado))
        {
            continue;
        }

        if (actualizado)
        {
            printf("Campo actualizado correctamente.\n");
        }
        else
        {
            printf("No se pudo actualizar el campo seleccionado.\n");
        }
        pause_console();
    }
}

static void ver_informacion_cancha()
{
    mostrar_pantalla("INFORMACION DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas_simple();
    printf("\n");

    int id = input_int("ID Cancha para ver informacion (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    CanchaInfoDetalle info;
    if (!cargar_info_cancha_detalle(id, &info))
    {
        printf("No se pudo recuperar la informacion de la cancha.\n");
        pause_console();
        return;
    }

    imprimir_info_cancha_detalle(id, &info);
    pause_console();
}

static void cargar_informacion_cancha()
{
    mostrar_pantalla("CARGAR INFORMACION DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    printf("Canchas con informacion pendiente:\n");
    int pendientes = listar_canchas_con_info_pendiente();
    if (pendientes < 0)
    {
        pause_console();
        return;
    }

    if (pendientes == 0)
    {
        printf("No hay canchas con informacion pendiente.\n");
        pause_console();
        return;
    }

    printf("\n");
    int id = input_int("ID Cancha para cargar informacion (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    CanchaInfoDetalle info;
    if (!cargar_info_cancha_detalle(id, &info))
    {
        printf("No se pudo recuperar la informacion actual de la cancha.\n");
        pause_console();
        return;
    }

    if (!cancha_necesita_completar_info(&info))
    {
        printf("La cancha seleccionada no tiene informacion pendiente.\n");
        pause_console();
        return;
    }

    if (!completar_informacion_cancha(id))
    {
        printf("No se pudo completar la informacion de la cancha.\n");
        pause_console();
        return;
    }

    mostrar_alerta_operacion("Cancha", "Informacion Cargada", info.nombre);
}

static void reactivar_cancha()
{
    mostrar_pantalla("REACTIVAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas_simple();
    printf("\n");

    int id = input_int("ID Cancha a Reactivar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    if (cancha_esta_activa(id))
    {
        printf("La cancha seleccionada ya esta activa.\n");
        pause_console();
        return;
    }

    if (!confirmar("Desea reactivar esta cancha?"))
        return;

    if (!actualizar_estado_cancha(id, 1))
    {
        printf("No se pudo reactivar la cancha.\n");
        pause_console();
        return;
    }

    mostrar_alerta_operacion("Cancha", "Reactivada", NULL);
}

void menu_canchas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_cancha},
        {2, "Listar", listar_canchas},
        {3, "Modificar", modificar_cancha},
        {4, "Eliminar", eliminar_cancha},
        {5, "Cargar Imagen", cargar_imagen_cancha},
        {6, "Ver Imagen", ver_imagen_cancha},
        {7, "Ver Informacion", ver_informacion_cancha},
        {8, "Cargar Informacion", cargar_informacion_cancha},
        {9, "Reactivar Cancha", reactivar_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu("CANCHAS", items, 10);
}
