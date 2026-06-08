#include "cancha.h"
#include "menu.h"
#include "db.h"
#include "export.h"
#include "utils.h"
#include "pdfgen.h"
#include "cJSON.h"
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

static void listar_canchas_simple(void);
static void solicitar_nombre_cancha(const char *prompt, char *buffer, int size);
typedef void (*CanchaInputReader)(const char *prompt, char *buffer, int size);
typedef int (*CanchaInputValidator)(const char *buffer);

static int cancha_valor_no_vacio(const char *buffer)
{
    return buffer && buffer[0] != '\0';
}

static int cancha_telefono_valido(const char *buffer)
{
    if (!cancha_valor_no_vacio(buffer))
    {
        return 0;
    }

    for (int i = 0; buffer[i] != '\0'; i++)
    {
        if (isdigit((unsigned char)buffer[i]))
        {
            return 1;
        }
    }
    return 0;
}

static void solicitar_texto_validado(const char *prompt, char *buffer, int size,
                                     CanchaInputReader reader,
                                     CanchaInputValidator validator,
                                     const char *error_message)
{
    while (1) //NOSONAR
    {
        reader(prompt, buffer, size);
        trim_whitespace(buffer);

        if (validator(buffer))
        {
            return;
        }

        printf("%s\n", error_message);
    }
}

static void solicitar_campo_no_vacio(const char *prompt, char *buffer, int size)
{
    solicitar_texto_validado(prompt, buffer, size, input_string_extended,
                             cancha_valor_no_vacio, "El campo no puede estar vacio.");
}

static void solicitar_telefono_no_vacio(const char *prompt, char *buffer, int size)
{
    solicitar_texto_validado(prompt, buffer, size, input_string_extended,
                             cancha_telefono_valido,
                             "El telefono debe contener al menos un numero.");
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

static int parsear_hora_formato_corto(const char *texto, int *out_hh, int *out_mm)
{
    if (!isdigit((unsigned char)texto[0]) || !parsear_dos_digitos(&texto[2], out_mm))
    {
        return 0;
    }

    *out_hh = (texto[0] - '0');
    return 1;
}

static int parsear_hora_formato_largo(const char *texto, int *out_hh, int *out_mm)
{
    if (!parsear_dos_digitos(texto, out_hh) || !parsear_dos_digitos(&texto[3], out_mm))
    {
        return 0;
    }

    return 1;
}

static int hora_hhmm_en_rango(int hh, int mm)
{
    return (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59);
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
        if (!parsear_hora_formato_corto(texto, &hh, &mm))
        {
            return 0;
        }
    }
    else if (len == 5 && texto[2] == ':')
    {
        if (!parsear_hora_formato_largo(texto, &hh, &mm))
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }

    if (!hora_hhmm_en_rango(hh, mm))
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
    int tiene_grabacion;
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
    info->tiene_grabacion = solicitar_si_no("Grabacion de Partido (1=SI, 0=NO): ");
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
    if (!db_prepare_stmt(&stmt,
                         "SELECT nombre, IFNULL(telefono, ''), IFNULL(direccion, ''), IFNULL(localidad, ''), "
                         "IFNULL(tipo_cancha_codigo, 0), IFNULL(superficie_codigo, 0), IFNULL(techada_estado_codigo, 2), "
                         "IFNULL(tiene_iluminacion, 0), IFNULL(horario_apertura_min, -1), IFNULL(horario_cierre_min, -1), "
                         "IFNULL(precio_hora_dia_centavos, 0), IFNULL(precio_hora_noche_centavos, 0), "
                         "IFNULL(tiene_vestuarios, 0), IFNULL(tiene_duchas, 0), IFNULL(tiene_buffet, 0), "
                         "IFNULL(tiene_estacionamiento, 0), IFNULL(cantidad_canchas, 1), IFNULL(estado, ''), "
                         "IFNULL(descripcion, ''), IFNULL(contacto_alt, ''), IFNULL(activa, 1), "
                         "IFNULL(tiene_grabacion, 0) "
                         "FROM cancha WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        db_stmt_release(stmt);
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
    info->tiene_grabacion = sqlite3_column_int(stmt, 21) ? 1 : 0;

    db_stmt_release(stmt);
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
    printf("Grabacion Partido  : %s\n", info->tiene_grabacion ? "SI" : "NO");
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
    if (!db_prepare_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    db_stmt_release(stmt);
    return ok;
}

static int actualizar_campo_entero_cancha(int id, const char *campo, int valor)
{
    char sql[192];
    sqlite3_stmt *stmt;

    snprintf(sql, sizeof(sql), "UPDATE cancha SET %s = ? WHERE id = ?", campo);
    if (!db_prepare_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    db_stmt_release(stmt);
    return ok;
}

static int completar_informacion_cancha(int id)
{
    CanchaInfoDetalle info = {0};
    solicitar_datos_comunes_cancha(&info, 0);

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt,
                         "UPDATE cancha "
                         "SET telefono = ?, direccion = ?, localidad = ?, tipo_cancha_codigo = ?, "
                         "superficie_codigo = ?, techada_estado_codigo = ?, tiene_iluminacion = ?, "
                         "horario_apertura_min = ?, horario_cierre_min = ?, precio_hora_dia_centavos = ?, "
                         "precio_hora_noche_centavos = ?, tiene_vestuarios = ?, tiene_duchas = ?, tiene_buffet = ?, "
                         "tiene_estacionamiento = ?, cantidad_canchas = ?, estado = ?, descripcion = ?, contacto_alt = ?, "
                         "tiene_grabacion = ? "
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
    sqlite3_bind_int(stmt, 20, info.tiene_grabacion);
    sqlite3_bind_int(stmt, 21, id);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    db_stmt_release(stmt);
    return ok;
}

static int contar_partidos_por_cancha(int cancha_id)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "SELECT COUNT(*) FROM partido WHERE cancha_id = ?"))
    {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, cancha_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    db_stmt_release(stmt);
    return count;
}

static int contar_total_canchas_activas(void)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "SELECT COUNT(*) FROM cancha WHERE IFNULL(activa, 1) = 1"))
    {
        return -1;
    }

    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }
    db_stmt_release(stmt);
    return total;
}

static int cancha_esta_activa(int cancha_id)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "SELECT IFNULL(activa, 1) FROM cancha WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, cancha_id);
    int activa = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        activa = sqlite3_column_int(stmt, 0) == 1;
    }
    db_stmt_release(stmt);
    return activa;
}

static int actualizar_estado_cancha(int cancha_id, int activa)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "UPDATE cancha SET activa = ? WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, activa ? 1 : 0);
    sqlite3_bind_int(stmt, 2, cancha_id);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    db_stmt_release(stmt);
    return ok;
}

static void listar_canchas_excluyendo(int cancha_excluida)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "SELECT id, nombre FROM cancha WHERE id <> ? AND IFNULL(activa, 1) = 1 ORDER BY id"))
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

    db_stmt_release(stmt);
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

    if (!db_prepare_stmt(&stmt_update, "UPDATE partido SET cancha_id = ? WHERE cancha_id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_update, 1, cancha_destino);
    sqlite3_bind_int(stmt_update, 2, cancha_origen);
    if (sqlite3_step(stmt_update) != SQLITE_DONE)
    {
        db_stmt_release(stmt_update);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    db_stmt_release(stmt_update);

    if (!db_prepare_stmt(&stmt_delete, "DELETE FROM cancha WHERE id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete, 1, cancha_origen);
    if (sqlite3_step(stmt_delete) != SQLITE_DONE)
    {
        db_stmt_release(stmt_delete);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    db_stmt_release(stmt_delete);

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

    if (!db_prepare_stmt(&stmt_delete_partidos, "DELETE FROM partido WHERE cancha_id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete_partidos, 1, cancha_id);
    if (sqlite3_step(stmt_delete_partidos) != SQLITE_DONE)
    {
        db_stmt_release(stmt_delete_partidos);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    db_stmt_release(stmt_delete_partidos);

    if (!db_prepare_stmt(&stmt_delete_cancha, "DELETE FROM cancha WHERE id = ?"))
    {
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    sqlite3_bind_int(stmt_delete_cancha, 1, cancha_id);
    if (sqlite3_step(stmt_delete_cancha) != SQLITE_DONE)
    {
        db_stmt_release(stmt_delete_cancha);
        ejecutar_sql_simple("ROLLBACK;");
        return 0;
    }
    db_stmt_release(stmt_delete_cancha);

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

    if (!app_is_path_safe_for_shell(ruta) || !app_validate_file_exists(ruta))
    {
        return 0;
    }

    return app_open_with_default_app(ruta);
}

static int obtener_ruta_imagen_cancha_db(int id, char *ruta, size_t size)
{
    if (!ruta || size == 0)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "SELECT imagen_ruta FROM cancha WHERE id=?"))
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

    db_stmt_release(stmt);
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
    solicitar_texto_validado(prompt, buffer, size, input_string,
                             cancha_valor_no_vacio, "El nombre no puede estar vacio.");
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
    if (!db_prepare_stmt(&stmt,
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
    db_stmt_release(stmt);

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
        "SELECT c.id, c.nombre, "
        "COUNT(p.id), "
        "IFNULL(SUM(p.goles), 0), "
        "IFNULL(SUM(p.asistencias), 0) "
        "FROM cancha c "
        "LEFT JOIN partido p ON c.id = p.cancha_id "
        "WHERE IFNULL(c.activa, 1) = 1 "
        "GROUP BY c.id, c.nombre "
        "ORDER BY c.id;";

    if (!db_prepare_stmt(&stmt, sql))
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
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        int partidos = sqlite3_column_int(stmt, 2);
        int goles = sqlite3_column_int(stmt, 3);
        int asistencias = sqlite3_column_int(stmt, 4);

        ui_printf_centered_line("%2d - %-24s%sPartidos: %2d%sGoles: %2d%sAsistencias: %2d",
                                id,
                                nombre ? nombre : "(sin nombre)",
                                sep,
                                partidos,
                                sep,
                                goles,
                                sep,
                                asistencias);
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("canchas cargadas");

    db_stmt_release(stmt);
    pause_console();
}

static void listar_canchas_simple()
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "SELECT id, nombre, IFNULL(activa, 1), IFNULL(tipo_cancha_codigo, 0) FROM cancha ORDER BY id"))
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

    db_stmt_release(stmt);
}

static int listar_canchas_con_info_pendiente(void)
{
    sqlite3_stmt *stmt;
    if (!db_prepare_stmt(&stmt, "SELECT id, nombre FROM cancha ORDER BY id"))
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

    db_stmt_release(stmt);
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
    if (!db_prepare_stmt(&stmt, "DELETE FROM cancha WHERE id = ?"))
    {
        printf("Error al eliminar la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    db_stmt_release(stmt);

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
    printf("21) Grabacion Partido: %s\n", info->tiene_grabacion ? "SI" : "NO");
    if (mostrar_completar_info)
    {
        printf("22) Completar Informacion\n");
    }
    printf("0) Volver\n\n");
}

enum
{
    TAM_VALOR_OPCION_CANCHA = 200
};

static int solicitar_texto_y_campo_cancha(int opcion, char *valor, const char **campo)
{
    if (!valor || !campo)
    {
        return 0;
    }

    switch (opcion)
    {
    case 1:
        solicitar_nombre_cancha("Nuevo nombre: ", valor, TAM_VALOR_OPCION_CANCHA);
        *campo = "nombre";
        return 1;
    case 2:
        solicitar_telefono_no_vacio("Nuevo numero de telefono: ", valor, TAM_VALOR_OPCION_CANCHA);
        *campo = "telefono";
        return 1;
    case 3:
        solicitar_campo_no_vacio("Nueva direccion: ", valor, TAM_VALOR_OPCION_CANCHA);
        *campo = "direccion";
        return 1;
    case 4:
        solicitar_campo_no_vacio("Nueva localidad/zona: ", valor, TAM_VALOR_OPCION_CANCHA);
        *campo = "localidad";
        return 1;
    case 18:
        solicitar_campo_no_vacio("Estado (ej: habilitada, en mantenimiento): ", valor, TAM_VALOR_OPCION_CANCHA);
        *campo = "estado";
        return 1;
    case 19:
        input_string("Descripcion breve: ", valor, TAM_VALOR_OPCION_CANCHA);
        trim_whitespace(valor);
        *campo = "descripcion";
        return 1;
    case 20:
        solicitar_campo_no_vacio("Nuevo contacto alternativo: ", valor, TAM_VALOR_OPCION_CANCHA);
        *campo = "contacto_alt";
        return 1;
    default:
        return 0;
    }
}

static int procesar_opcion_texto_cancha(int id, int opcion, int *actualizado)
{
    char valor[TAM_VALOR_OPCION_CANCHA] = {0};
    const char *campo = NULL;

    if (!actualizado)
    {
        return 0;
    }

    if (!solicitar_texto_y_campo_cancha(opcion, valor, &campo))
    {
        return 0;
    }

    *actualizado = actualizar_campo_texto_cancha(id, campo, valor);
    return 1;
}

static int procesar_opcion_entero_cancha(int id, int opcion, int *actualizado)
{
    if (!actualizado)
    {
        return 0;
    }

    switch (opcion)
    {
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
    default:
        return 0;
    }
}

static int procesar_opcion_cantidad_cancha(int id, int opcion, int *actualizado)
{
    if (!actualizado || opcion != 17)
    {
        return 0;
    }

    int cantidad = input_int("Cantidad de canchas del complejo: ");
    if (cantidad <= 0)
    {
        cantidad = 1;
    }

    *actualizado = actualizar_campo_entero_cancha(id, "cantidad_canchas", cantidad);
    return 1;
}

static int procesar_opcion_completar_info_cancha(int id, int opcion, int mostrar_completar_info, int *actualizado)
{
    if (!actualizado || opcion != 21)
    {
        return 0;
    }

    if (mostrar_completar_info)
    {
        *actualizado = completar_informacion_cancha(id);
        return 1;
    }

    printf("La cancha ya tiene informacion suficiente para editar campo por campo.\n");
    pause_console();
    return 0;
}

static int procesar_opcion_modificar_cancha(int id, int opcion, int mostrar_completar_info, int *actualizado)
{
    if (!actualizado)
    {
        return 0;
    }

    *actualizado = 0;

    if (procesar_opcion_texto_cancha(id, opcion, actualizado))
    {
        return 1;
    }

    if (procesar_opcion_entero_cancha(id, opcion, actualizado))
    {
        return 1;
    }

    if (procesar_opcion_cantidad_cancha(id, opcion, actualizado))
    {
        return 1;
    }

    if (opcion == 21)
    {
        return procesar_opcion_completar_info_cancha(id, opcion, mostrar_completar_info, actualizado);
    }

    printf("Opcion invalida.\n");
    pause_console();
    return 0;
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
    mostrar_pantalla("REACTIVAR / DESACTIVAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas_simple();
    printf("\n");

    int id = input_int("ID Cancha (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    int esta_activa = cancha_esta_activa(id);
    int nuevo_estado = esta_activa ? 0 : 1;

    if (esta_activa)
    {
        if (!confirmar("Desea desactivar esta cancha?"))
            return;
    }
    else
    {
        if (!confirmar("Desea reactivar esta cancha?"))
            return;
    }

    if (!actualizar_estado_cancha(id, nuevo_estado))
    {
        printf("No se pudo actualizar el estado de la cancha.\n");
        pause_console();
        return;
    }

    if (nuevo_estado == 1)
    {
        mostrar_alerta_operacion("Cancha", "Reactivada", NULL);
    }
    else
    {
        mostrar_alerta_operacion("Cancha", "Desactivada (Inactiva)", NULL);
    }
}

static const char *cancha_export_col_text(sqlite3_stmt *stmt, int col)
{
    const unsigned char *value = sqlite3_column_text(stmt, col);
    return value ? (const char *)value : "";
}

static int cancha_export_preparar_stmt(sqlite3_stmt **stmt, int exportar_todas, int cancha_id)
{
    const char *sql_todas =
        "SELECT id, nombre, IFNULL(telefono, ''), IFNULL(direccion, ''), IFNULL(localidad, ''), "
        "IFNULL(tipo_cancha_codigo, 0), IFNULL(superficie_codigo, 0), IFNULL(techada_estado_codigo, 2), "
        "IFNULL(tiene_iluminacion, 0), IFNULL(horario_apertura_min, -1), IFNULL(horario_cierre_min, -1), "
        "IFNULL(precio_hora_dia_centavos, 0), IFNULL(precio_hora_noche_centavos, 0), "
        "IFNULL(tiene_vestuarios, 0), IFNULL(tiene_duchas, 0), IFNULL(tiene_buffet, 0), "
        "IFNULL(tiene_estacionamiento, 0), IFNULL(cantidad_canchas, 1), IFNULL(estado, ''), "
        "IFNULL(descripcion, ''), IFNULL(contacto_alt, ''), IFNULL(activa, 1), "
        "IFNULL(tiene_grabacion, 0) "
        "FROM cancha ORDER BY id";

    const char *sql_una =
        "SELECT id, nombre, IFNULL(telefono, ''), IFNULL(direccion, ''), IFNULL(localidad, ''), "
        "IFNULL(tipo_cancha_codigo, 0), IFNULL(superficie_codigo, 0), IFNULL(techada_estado_codigo, 2), "
        "IFNULL(tiene_iluminacion, 0), IFNULL(horario_apertura_min, -1), IFNULL(horario_cierre_min, -1), "
        "IFNULL(precio_hora_dia_centavos, 0), IFNULL(precio_hora_noche_centavos, 0), "
        "IFNULL(tiene_vestuarios, 0), IFNULL(tiene_duchas, 0), IFNULL(tiene_buffet, 0), "
        "IFNULL(tiene_estacionamiento, 0), IFNULL(cantidad_canchas, 1), IFNULL(estado, ''), "
        "IFNULL(descripcion, ''), IFNULL(contacto_alt, ''), IFNULL(activa, 1), "
        "IFNULL(tiene_grabacion, 0) "
        "FROM cancha WHERE id = ? ORDER BY id";

    if (!db_prepare_stmt(stmt, exportar_todas ? sql_todas : sql_una))
    {
        return 0;
    }

    if (!exportar_todas)
    {
        sqlite3_bind_int(*stmt, 1, cancha_id);
    }
    return 1;
}

static void cancha_export_cargar_desde_stmt(sqlite3_stmt *stmt, int *out_id, CanchaInfoDetalle *info)
{
    if (!stmt || !out_id || !info)
    {
        return;
    }

    memset(info, 0, sizeof(*info));
    *out_id = sqlite3_column_int(stmt, 0);

    snprintf(info->nombre, sizeof(info->nombre), "%s", cancha_export_col_text(stmt, 1));
    snprintf(info->telefono, sizeof(info->telefono), "%s", cancha_export_col_text(stmt, 2));
    snprintf(info->direccion, sizeof(info->direccion), "%s", cancha_export_col_text(stmt, 3));
    snprintf(info->localidad, sizeof(info->localidad), "%s", cancha_export_col_text(stmt, 4));

    info->tipo_cancha_codigo = sqlite3_column_int(stmt, 5);
    info->superficie_codigo = sqlite3_column_int(stmt, 6);
    info->techada_estado = sqlite3_column_int(stmt, 7);
    info->tiene_iluminacion = sqlite3_column_int(stmt, 8) ? 1 : 0;
    info->horario_apertura_min = sqlite3_column_int(stmt, 9);
    info->horario_cierre_min = sqlite3_column_int(stmt, 10);
    info->precio_hora_dia_centavos = sqlite3_column_int(stmt, 11);
    info->precio_hora_noche_centavos = sqlite3_column_int(stmt, 12);
    info->servicios.vestuarios = sqlite3_column_int(stmt, 13) ? 1 : 0;
    info->servicios.duchas = sqlite3_column_int(stmt, 14) ? 1 : 0;
    info->servicios.buffet = sqlite3_column_int(stmt, 15) ? 1 : 0;
    info->servicios.estacionamiento = sqlite3_column_int(stmt, 16) ? 1 : 0;
    info->cantidad_canchas = sqlite3_column_int(stmt, 17);
    if (info->cantidad_canchas <= 0)
    {
        info->cantidad_canchas = 1;
    }

    snprintf(info->estado, sizeof(info->estado), "%s", cancha_export_col_text(stmt, 18));
    snprintf(info->descripcion, sizeof(info->descripcion), "%s", cancha_export_col_text(stmt, 19));
    snprintf(info->contacto_alt, sizeof(info->contacto_alt), "%s", cancha_export_col_text(stmt, 20));
    info->activa = sqlite3_column_int(stmt, 21) == 1;
    info->tiene_grabacion = sqlite3_column_int(stmt, 22) ? 1 : 0;
}

static void cancha_export_generar_nombre_archivo(char *dest, size_t size, const char *ext,
        int exportar_todas, int cancha_id)
{
    if (!dest || size == 0 || !ext)
    {
        return;
    }

    if (exportar_todas)
    {
        snprintf(dest, size, "canchas_informacion.%s", ext);
    }
    else
    {
        snprintf(dest, size, "cancha_%d_informacion.%s", cancha_id, ext);
    }
}

static void cancha_export_write_csv_field(FILE *f, const char *value)
{
    const char *safe = value ? value : "";
    fputc('"', f);
    while (*safe)
    {
        if (*safe == '"')
        {
            fputs("\"\"", f);
        }
        else
        {
            fputc(*safe, f);
        }
        safe++;
    }
    fputc('"', f);
}

static void cancha_export_write_html_text(FILE *f, const char *value)
{
    const char *safe = value ? value : "";
    while (*safe)
    {
        if (*safe == '&')
        {
            fputs("&amp;", f);
        }
        else if (*safe == '<')
        {
            fputs("&lt;", f);
        }
        else if (*safe == '>')
        {
            fputs("&gt;", f);
        }
        else if (*safe == '"')
        {
            fputs("&quot;", f);
        }
        else
        {
            fputc(*safe, f);
        }
        safe++;
    }
}

static const char *cancha_export_estado_texto(int activa)
{
    return activa ? "ACTIVA" : "INACTIVA";
}

static int cancha_export_info_txt(int exportar_todas, int cancha_id)
{
    sqlite3_stmt *stmt = NULL;
    char filename[128];
    cancha_export_generar_nombre_archivo(filename, sizeof(filename), "txt", exportar_todas, cancha_id);

    FILE *f = abrir_archivo_exportacion(filename, "Error al crear archivo TXT de canchas.");
    if (!f)
    {
        return 0;
    }

    if (!cancha_export_preparar_stmt(&stmt, exportar_todas, cancha_id))
    {
        fclose(f);
        printf("Error al consultar canchas para exportar.\n");
        return 0;
    }

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = 0;
        CanchaInfoDetalle info = {0};
        char hora_apertura[16];
        char hora_cierre[16];

        cancha_export_cargar_desde_stmt(stmt, &id, &info);
        formatear_hora_minutos(info.horario_apertura_min, hora_apertura, sizeof(hora_apertura));
        formatear_hora_minutos(info.horario_cierre_min, hora_cierre, sizeof(hora_cierre));

        fprintf(f, "========================================\n");
        fprintf(f, "ID                 : %d\n", id);
        fprintf(f, "Nombre             : %s\n", texto_o_defecto(info.nombre, "(sin dato)"));
        fprintf(f, "Telefono           : %s\n", texto_o_defecto(info.telefono, "(sin dato)"));
        fprintf(f, "Direccion          : %s\n", texto_o_defecto(info.direccion, "(sin dato)"));
        fprintf(f, "Localidad          : %s\n", texto_o_defecto(info.localidad, "(sin dato)"));
        fprintf(f, "Tipo de Cancha     : %s\n", texto_tipo_cancha(info.tipo_cancha_codigo));
        fprintf(f, "Superficie         : %s\n", texto_superficie(info.superficie_codigo));
        fprintf(f, "Techada            : %s\n", texto_estado_techada(info.techada_estado));
        fprintf(f, "Iluminacion        : %s\n", info.tiene_iluminacion ? "SI" : "NO");
        fprintf(f, "Horario            : %s - %s\n", hora_apertura, hora_cierre);
        fprintf(f, "Precio Hora Dia    : %.2f\n", (double)info.precio_hora_dia_centavos / 100.0);
        fprintf(f, "Precio Hora Noche  : %.2f\n", (double)info.precio_hora_noche_centavos / 100.0);
        fprintf(f, "Vestuarios         : %s\n", info.servicios.vestuarios ? "SI" : "NO");
        fprintf(f, "Duchas             : %s\n", info.servicios.duchas ? "SI" : "NO");
        fprintf(f, "Buffet             : %s\n", info.servicios.buffet ? "SI" : "NO");
        fprintf(f, "Estacionamiento    : %s\n", info.servicios.estacionamiento ? "SI" : "NO");
        fprintf(f, "Cantidad Canchas   : %d\n", info.cantidad_canchas);
        fprintf(f, "Estado Pasto       : %s\n", texto_o_defecto(info.estado, "(sin dato)"));
        fprintf(f, "Descripcion        : %s\n", texto_o_defecto(info.descripcion, "(sin dato)"));
        fprintf(f, "Contacto Alterno   : %s\n", texto_o_defecto(info.contacto_alt, "(sin dato)"));
        fprintf(f, "Grabacion Partido  : %s\n", info.tiene_grabacion ? "SI" : "NO");
        fprintf(f, "Estado             : %s\n", cancha_export_estado_texto(info.activa));
        fprintf(f, "========================================\n\n");
        total++;
    }

    db_stmt_release(stmt);
    fclose(f);

    if (total == 0)
    {
        printf("No se encontraron canchas para exportar.\n");
        return 0;
    }

    printf("Exportado TXT: %s\n", filename);
    return 1;
}

static int cancha_export_info_csv(int exportar_todas, int cancha_id)
{
    sqlite3_stmt *stmt = NULL;
    char filename[128];
    cancha_export_generar_nombre_archivo(filename, sizeof(filename), "csv", exportar_todas, cancha_id);

    FILE *f = abrir_archivo_exportacion(filename, "Error al crear archivo CSV de canchas.");
    if (!f)
    {
        return 0;
    }

    if (!cancha_export_preparar_stmt(&stmt, exportar_todas, cancha_id))
    {
        fclose(f);
        printf("Error al consultar canchas para exportar.\n");
        return 0;
    }

    fprintf(f, "id,nombre,telefono,direccion,localidad,tipo_cancha,superficie,techada,iluminacion,horario_apertura,horario_cierre,precio_hora_dia,precio_hora_noche,vestuarios,duchas,buffet,estacionamiento,cantidad_canchas,estado_pasto,descripcion,contacto_alterno,grabacion_partido,estado\n");

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = 0;
        CanchaInfoDetalle info = {0};
        char hora_apertura[16];
        char hora_cierre[16];

        cancha_export_cargar_desde_stmt(stmt, &id, &info);
        formatear_hora_minutos(info.horario_apertura_min, hora_apertura, sizeof(hora_apertura));
        formatear_hora_minutos(info.horario_cierre_min, hora_cierre, sizeof(hora_cierre));

        fprintf(f, "%d,", id);
        cancha_export_write_csv_field(f, texto_o_defecto(info.nombre, ""));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_o_defecto(info.telefono, ""));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_o_defecto(info.direccion, ""));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_o_defecto(info.localidad, ""));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_tipo_cancha(info.tipo_cancha_codigo));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_superficie(info.superficie_codigo));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_estado_techada(info.techada_estado));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, info.tiene_iluminacion ? "SI" : "NO");
        fprintf(f, ",");
        cancha_export_write_csv_field(f, hora_apertura);
        fprintf(f, ",");
        cancha_export_write_csv_field(f, hora_cierre);
        fprintf(f, ",%.2f,%.2f,%d,%d,%d,%d,%d,",
                (double)info.precio_hora_dia_centavos / 100.0,
                (double)info.precio_hora_noche_centavos / 100.0,
                info.servicios.vestuarios,
                info.servicios.duchas,
                info.servicios.buffet,
                info.servicios.estacionamiento,
                info.cantidad_canchas);
        cancha_export_write_csv_field(f, texto_o_defecto(info.estado, ""));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_o_defecto(info.descripcion, ""));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, texto_o_defecto(info.contacto_alt, ""));
        fprintf(f, ",");
        cancha_export_write_csv_field(f, info.tiene_grabacion ? "SI" : "NO");
        fprintf(f, ",");
        cancha_export_write_csv_field(f, cancha_export_estado_texto(info.activa));
        fprintf(f, "\n");

        total++;
    }

    db_stmt_release(stmt);
    fclose(f);

    if (total == 0)
    {
        printf("No se encontraron canchas para exportar.\n");
        return 0;
    }

    printf("Exportado CSV: %s\n", filename);
    return 1;
}

static cJSON *cancha_export_json_build_item(sqlite3_stmt *stmt)
{
    int id = 0;
    CanchaInfoDetalle info = {0};
    char hora_apertura[16];
    char hora_cierre[16];
    cJSON *item = cJSON_CreateObject();

    if (!item)
    {
        return NULL;
    }

    cancha_export_cargar_desde_stmt(stmt, &id, &info);
    formatear_hora_minutos(info.horario_apertura_min, hora_apertura, sizeof(hora_apertura));
    formatear_hora_minutos(info.horario_cierre_min, hora_cierre, sizeof(hora_cierre));

    cJSON_AddNumberToObject(item, "id", id);
    cJSON_AddStringToObject(item, "nombre", texto_o_defecto(info.nombre, ""));
    cJSON_AddStringToObject(item, "telefono", texto_o_defecto(info.telefono, ""));
    cJSON_AddStringToObject(item, "direccion", texto_o_defecto(info.direccion, ""));
    cJSON_AddStringToObject(item, "localidad", texto_o_defecto(info.localidad, ""));
    cJSON_AddStringToObject(item, "tipo_cancha", texto_tipo_cancha(info.tipo_cancha_codigo));
    cJSON_AddStringToObject(item, "superficie", texto_superficie(info.superficie_codigo));
    cJSON_AddStringToObject(item, "techada", texto_estado_techada(info.techada_estado));
    cJSON_AddBoolToObject(item, "tiene_iluminacion", info.tiene_iluminacion);
    cJSON_AddStringToObject(item, "horario_apertura", hora_apertura);
    cJSON_AddStringToObject(item, "horario_cierre", hora_cierre);
    cJSON_AddNumberToObject(item, "precio_hora_dia", (double)info.precio_hora_dia_centavos / 100.0);
    cJSON_AddNumberToObject(item, "precio_hora_noche", (double)info.precio_hora_noche_centavos / 100.0);
    cJSON_AddBoolToObject(item, "tiene_vestuarios", info.servicios.vestuarios);
    cJSON_AddBoolToObject(item, "tiene_duchas", info.servicios.duchas);
    cJSON_AddBoolToObject(item, "tiene_buffet", info.servicios.buffet);
    cJSON_AddBoolToObject(item, "tiene_estacionamiento", info.servicios.estacionamiento);
    cJSON_AddNumberToObject(item, "cantidad_canchas", info.cantidad_canchas);
    cJSON_AddStringToObject(item, "estado_pasto", texto_o_defecto(info.estado, ""));
    cJSON_AddStringToObject(item, "descripcion", texto_o_defecto(info.descripcion, ""));
    cJSON_AddStringToObject(item, "contacto_alterno", texto_o_defecto(info.contacto_alt, ""));
    cJSON_AddBoolToObject(item, "tiene_grabacion", info.tiene_grabacion);
    cJSON_AddStringToObject(item, "estado", cancha_export_estado_texto(info.activa));

    return item;
}

static int cancha_export_info_json(int exportar_todas, int cancha_id)
{
    sqlite3_stmt *stmt = NULL;
    char filename[128];
    cancha_export_generar_nombre_archivo(filename, sizeof(filename), "json", exportar_todas, cancha_id);

    FILE *f = abrir_archivo_exportacion(filename, "Error al crear archivo JSON de canchas.");
    if (!f)
    {
        return 0;
    }

    if (!cancha_export_preparar_stmt(&stmt, exportar_todas, cancha_id))
    {
        fclose(f);
        printf("Error al consultar canchas para exportar.\n");
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    if (!root || !array)
    {
        if (root)
        {
            cJSON_Delete(root);
        }
        if (array)
        {
            cJSON_Delete(array);
        }
        db_stmt_release(stmt);
        fclose(f);
        printf("Error al construir el JSON de canchas.\n");
        return 0;
    }

    cJSON_AddStringToObject(root, "tipo_exportacion", "canchas_informacion");
    cJSON_AddStringToObject(root, "alcance", exportar_todas ? "todas" : "una");
    if (!exportar_todas)
    {
        cJSON_AddNumberToObject(root, "cancha_id", cancha_id);
    }
    cJSON_AddItemToObject(root, "canchas", array);

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cancha_export_json_build_item(stmt);
        if (!item)
        {
            continue;
        }

        cJSON_AddItemToArray(array, item);
        total++;
    }

    db_stmt_release(stmt);

    if (total == 0)
    {
        cJSON_Delete(root);
        fclose(f);
        printf("No se encontraron canchas para exportar.\n");
        return 0;
    }

    cJSON_AddNumberToObject(root, "total", total);
    char *json_str = cJSON_Print(root);
    if (!json_str)
    {
        cJSON_Delete(root);
        fclose(f);
        printf("Error al serializar el JSON de canchas.\n");
        return 0;
    }

    fprintf(f, "%s", json_str);
    free(json_str);
    cJSON_Delete(root);
    fclose(f);

    printf("Exportado JSON: %s\n", filename);
    return 1;
}

static int cancha_export_info_html(int exportar_todas, int cancha_id)
{
    sqlite3_stmt *stmt = NULL;
    char filename[128];
    cancha_export_generar_nombre_archivo(filename, sizeof(filename), "html", exportar_todas, cancha_id);

    FILE *f = abrir_archivo_exportacion(filename, "Error al crear archivo HTML de canchas.");
    if (!f)
    {
        return 0;
    }

    if (!cancha_export_preparar_stmt(&stmt, exportar_todas, cancha_id))
    {
        fclose(f);
        printf("Error al consultar canchas para exportar.\n");
        return 0;
    }

    fprintf(f, "<!doctype html>\n");
    fprintf(f, "<html><head><meta charset=\"utf-8\"><title>Canchas</title>");
    fprintf(f, "<style>body{font-family:Arial,sans-serif;margin:20px;}table{border-collapse:collapse;width:100%%;}th,td{border:1px solid #ccc;padding:6px;text-align:left;}th{background:#f0f0f0;}h1{margin-bottom:16px;}</style>");
    fprintf(f, "</head><body>\n");
    fprintf(f, "<h1>Informacion de Canchas</h1>\n");
    fprintf(f, "<table><thead><tr>");
    fprintf(f, "<th>ID</th><th>Nombre</th><th>Telefono</th><th>Direccion</th><th>Localidad</th><th>Tipo</th><th>Superficie</th><th>Techada</th><th>Iluminacion</th><th>Apertura</th><th>Cierre</th><th>Precio Dia</th><th>Precio Noche</th><th>Vestuarios</th><th>Duchas</th><th>Buffet</th><th>Estacionamiento</th><th>Cantidad</th><th>Estado Pasto</th><th>Descripcion</th><th>Contacto Alterno</th><th>Grabacion</th><th>Estado</th>");
    fprintf(f, "</tr></thead><tbody>\n");

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = 0;
        CanchaInfoDetalle info = {0};
        char hora_apertura[16];
        char hora_cierre[16];

        cancha_export_cargar_desde_stmt(stmt, &id, &info);
        formatear_hora_minutos(info.horario_apertura_min, hora_apertura, sizeof(hora_apertura));
        formatear_hora_minutos(info.horario_cierre_min, hora_cierre, sizeof(hora_cierre));

        fprintf(f, "<tr><td>%d</td><td>", id);
        cancha_export_write_html_text(f, texto_o_defecto(info.nombre, ""));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_o_defecto(info.telefono, ""));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_o_defecto(info.direccion, ""));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_o_defecto(info.localidad, ""));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_tipo_cancha(info.tipo_cancha_codigo));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_superficie(info.superficie_codigo));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_estado_techada(info.techada_estado));
        fprintf(f, "</td><td>%s</td>", info.tiene_iluminacion ? "SI" : "NO");
        fprintf(f, "<td>%s</td><td>%s</td>", hora_apertura, hora_cierre);
        fprintf(f, "<td>%.2f</td><td>%.2f</td>",
                (double)info.precio_hora_dia_centavos / 100.0,
                (double)info.precio_hora_noche_centavos / 100.0);
        fprintf(f, "<td>%s</td><td>%s</td><td>%s</td><td>%s</td>",
                info.servicios.vestuarios ? "SI" : "NO",
                info.servicios.duchas ? "SI" : "NO",
                info.servicios.buffet ? "SI" : "NO",
                info.servicios.estacionamiento ? "SI" : "NO");
        fprintf(f, "<td>%d</td><td>", info.cantidad_canchas);
        cancha_export_write_html_text(f, texto_o_defecto(info.estado, ""));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_o_defecto(info.descripcion, ""));
        fprintf(f, "</td><td>");
        cancha_export_write_html_text(f, texto_o_defecto(info.contacto_alt, ""));
        fprintf(f, "</td><td>%s</td>", info.tiene_grabacion ? "SI" : "NO");
        fprintf(f, "<td>%s</td></tr>\n", cancha_export_estado_texto(info.activa));

        total++;
    }

    db_stmt_release(stmt);

    fprintf(f, "</tbody></table></body></html>\n");
    fclose(f);

    if (total == 0)
    {
        printf("No se encontraron canchas para exportar.\n");
        return 0;
    }

    printf("Exportado HTML: %s\n", filename);
    return 1;
}

static void cancha_export_pdf_ascii(const char *src, char *dst, size_t dst_size)
{
    sanitizar_ascii_basico(src, dst, dst_size);
}

typedef struct
{
    struct pdf_doc *pdf;
    struct pdf_object *page;
    float y;
    int warned;
    float margin;
} CanchaExportPdfCtx;

static CanchaExportPdfCtx cancha_export_pdf_add_line(CanchaExportPdfCtx ctx,
        const char *text,
        float size,
        float leading)
{
    if (!ctx.pdf || !ctx.page || !text)
    {
        return ctx;
    }

    if (ctx.y < ctx.margin + leading)
    {
        ctx.page = pdf_append_page(ctx.pdf);
        if (!ctx.page)
        {
            return ctx;
        }
        pdf_page_set_size(ctx.pdf, ctx.page, PDF_A4_WIDTH, PDF_A4_HEIGHT);
        ctx.y = pdf_page_height(ctx.page) - ctx.margin;
    }

    int rc = pdf_add_text(ctx.pdf, ctx.page, text, size, ctx.margin, ctx.y, PDF_BLACK);
    if (rc < 0)
    {
        char ascii_text[1024];
        cancha_export_pdf_ascii(text, ascii_text, sizeof(ascii_text));
        rc = pdf_add_text(ctx.pdf, ctx.page, ascii_text, size, ctx.margin, ctx.y, PDF_BLACK);
        if (rc < 0 && !ctx.warned)
        {
            int errval = 0;
            const char *err = pdf_get_err(ctx.pdf, &errval);
            printf("Advertencia PDF: no se pudo escribir texto (%d): %s\n", errval,
                   err ? err : "error desconocido");
            ctx.warned = 1;
        }
    }

    ctx.y -= leading;
    return ctx;
}

static CanchaExportPdfCtx cancha_export_pdf_print_fila(CanchaExportPdfCtx ctx, int id,
        CanchaInfoDetalle const *detalle,
        const char *hora_apertura,
        const char *hora_cierre)
{
    char line[512];

    snprintf(line, sizeof(line), "----------------------------------------");
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);

    snprintf(line, sizeof(line), "ID: %d", id);
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);

    snprintf(line, sizeof(line), "Nombre: %s", texto_o_defecto(detalle->nombre, "(sin dato)"));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Telefono: %s", texto_o_defecto(detalle->telefono, "(sin dato)"));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Direccion: %s", texto_o_defecto(detalle->direccion, "(sin dato)"));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Localidad: %s", texto_o_defecto(detalle->localidad, "(sin dato)"));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Tipo de Cancha: %s", texto_tipo_cancha(detalle->tipo_cancha_codigo));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Superficie: %s", texto_superficie(detalle->superficie_codigo));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Techada: %s", texto_estado_techada(detalle->techada_estado));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Iluminacion: %s", detalle->tiene_iluminacion ? "SI" : "NO");
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);

    snprintf(line, sizeof(line), "Horario: %s - %s", hora_apertura, hora_cierre);
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);

    snprintf(line, sizeof(line), "Precio Hora Dia: $%.2f", (double)detalle->precio_hora_dia_centavos / 100.0);
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Precio Hora Noche: $%.2f", (double)detalle->precio_hora_noche_centavos / 100.0);
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);

    snprintf(line, sizeof(line), "Vestuarios: %s | Duchas: %s | Buffet: %s | Estacionamiento: %s",
             detalle->servicios.vestuarios ? "SI" : "NO",
             detalle->servicios.duchas ? "SI" : "NO",
             detalle->servicios.buffet ? "SI" : "NO",
             detalle->servicios.estacionamiento ? "SI" : "NO");
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);

    snprintf(line, sizeof(line), "Cantidad Canchas: %d", detalle->cantidad_canchas);
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Estado Pasto: %s", texto_o_defecto(detalle->estado, "(sin dato)"));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Descripcion: %s", texto_o_defecto(detalle->descripcion, "(sin dato)"));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Contacto Alterno: %s", texto_o_defecto(detalle->contacto_alt, "(sin dato)"));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Grabacion Partido: %s", detalle->tiene_grabacion ? "SI" : "NO");
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 12.0f);
    snprintf(line, sizeof(line), "Estado: %s", cancha_export_estado_texto(detalle->activa));
    ctx = cancha_export_pdf_add_line(ctx, line, 10.0f, 14.0f);

    return ctx;
}

static int cancha_export_info_pdf(int exportar_todas, int cancha_id)
{
    sqlite3_stmt *stmt = NULL;
    char filename[128];
    cancha_export_generar_nombre_archivo(filename, sizeof(filename), "pdf", exportar_todas, cancha_id);

    if (!cancha_export_preparar_stmt(&stmt, exportar_todas, cancha_id))
    {
        printf("Error al consultar canchas para exportar.\n");
        return 0;
    }

    struct pdf_info info;
    memset(&info, 0, sizeof(info));
    snprintf(info.creator, sizeof(info.creator), "MiFutbolC");
    snprintf(info.producer, sizeof(info.producer), "MiFutbolC");
    snprintf(info.title, sizeof(info.title), "Informacion de Canchas");
    snprintf(info.author, sizeof(info.author), "MiFutbolC");
    snprintf(info.subject, sizeof(info.subject), "Exportacion de canchas");
    get_datetime(info.date, (int)sizeof(info.date));

    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    if (!pdf)
    {
        db_stmt_release(stmt);
        printf("Error al crear documento PDF.\n");
        return 0;
    }

    struct pdf_object *page = pdf_append_page(pdf);
    if (!page)
    {
        db_stmt_release(stmt);
        pdf_destroy(pdf);
        printf("Error al crear pagina PDF.\n");
        return 0;
    }

    pdf_page_set_size(pdf, page, PDF_A4_WIDTH, PDF_A4_HEIGHT);
    pdf_set_font(pdf, "Helvetica");

    float y = pdf_page_height(page) - 40.0f;
    CanchaExportPdfCtx pdf_ctx = {pdf, page, y, 0, 40.0f};

    pdf_ctx = cancha_export_pdf_add_line(pdf_ctx, "INFORMACION DE CANCHAS", 14.0f, 18.0f);
    pdf_ctx = cancha_export_pdf_add_line(pdf_ctx,
                                         exportar_todas ? "Alcance: Todas las canchas" : "Alcance: Una cancha",
                                         10.0f,
                                         14.0f);
    if (!exportar_todas)
    {
        char alcance_line[64];
        snprintf(alcance_line, sizeof(alcance_line), "Cancha ID: %d", cancha_id);
        pdf_ctx = cancha_export_pdf_add_line(pdf_ctx, alcance_line, 10.0f, 14.0f);
    }
    pdf_ctx = cancha_export_pdf_add_line(pdf_ctx, "", 10.0f, 10.0f);

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = 0;
        CanchaInfoDetalle detalle = {0};
        char hora_apertura[16];
        char hora_cierre[16];

        cancha_export_cargar_desde_stmt(stmt, &id, &detalle);
        formatear_hora_minutos(detalle.horario_apertura_min, hora_apertura, sizeof(hora_apertura));
        formatear_hora_minutos(detalle.horario_cierre_min, hora_cierre, sizeof(hora_cierre));

        pdf_ctx = cancha_export_pdf_print_fila(pdf_ctx, id, &detalle, hora_apertura, hora_cierre);
        total++;
    }

    db_stmt_release(stmt);

    if (total == 0)
    {
        pdf_destroy(pdf);
        printf("No se encontraron canchas para exportar.\n");
        return 0;
    }

    const char *pdf_path = get_export_path(filename);
    if (pdf_save(pdf, pdf_path) < 0)
    {
        int errval = 0;
        const char *err = pdf_get_err(pdf, &errval);
        printf("Error al guardar PDF (%d): %s\n", errval, err ? err : "error desconocido");
        pdf_destroy(pdf);
        return 0;
    }

    pdf_destroy(pdf);
    printf("Exportado PDF: %s\n", filename);
    return 1;
}

static int cancha_export_solicitar_alcance(int *exportar_todas, int *cancha_id)
{
    if (!exportar_todas || !cancha_id)
    {
        return 0;
    }

    while (1)
    {
        printf("\nQue desea exportar?\n");
        printf("1) Todas las canchas\n");
        printf("2) Solo una cancha\n");
        printf("0) Cancelar\n");

        int opcion = input_int("Opcion: ");
        if (opcion == 0)
        {
            return 0;
        }

        if (opcion == 1)
        {
            *exportar_todas = 1;
            *cancha_id = 0;
            return 1;
        }

        if (opcion == 2)
        {
            listar_canchas_simple();
            printf("\n");
            int id = input_int("ID de cancha a exportar (0 para cancelar): ");
            if (id == 0)
            {
                return 0;
            }

            if (!existe_id("cancha", id))
            {
                mostrar_no_existe("cancha");
                continue;
            }

            *exportar_todas = 0;
            *cancha_id = id;
            return 1;
        }

        printf("Opcion invalida.\n");
    }
}

static int cancha_export_solicitar_formato_especifico(void)
{
    while (1)
    {
        printf("\nFormato a exportar:\n");
        printf("1) TXT\n");
        printf("2) CSV\n");
        printf("3) JSON\n");
        printf("4) HTML\n");
        printf("5) PDF\n");
        printf("0) Cancelar\n");

        int formato = input_int("Formato: ");
        if (formato >= 0 && formato <= 5)
        {
            return formato;
        }
        printf("Opcion invalida.\n");
    }
}

static int cancha_export_ejecutar_formato(int formato, int exportar_todas, int cancha_id)
{
    switch (formato)
    {
    case 1:
        return cancha_export_info_txt(exportar_todas, cancha_id);
    case 2:
        return cancha_export_info_csv(exportar_todas, cancha_id);
    case 3:
        return cancha_export_info_json(exportar_todas, cancha_id);
    case 4:
        return cancha_export_info_html(exportar_todas, cancha_id);
    case 5:
        return cancha_export_info_pdf(exportar_todas, cancha_id);
    default:
        return -1;
    }
}

static int cancha_export_ejecutar_todos(int exportar_todas, int cancha_id)
{
    int exitos = 0;
    exitos += cancha_export_info_txt(exportar_todas, cancha_id);
    exitos += cancha_export_info_csv(exportar_todas, cancha_id);
    exitos += cancha_export_info_json(exportar_todas, cancha_id);
    exitos += cancha_export_info_html(exportar_todas, cancha_id);
    exitos += cancha_export_info_pdf(exportar_todas, cancha_id);
    return exitos;
}

static int cancha_export_ejecutar_por_modo(int modo, int exportar_todas, int cancha_id)
{
    if (modo == 1)
    {
        int formato = cancha_export_solicitar_formato_especifico();
        if (formato == 0)
        {
            return -2;
        }

        return cancha_export_ejecutar_formato(formato, exportar_todas, cancha_id);
    }

    if (modo == 2)
    {
        return cancha_export_ejecutar_todos(exportar_todas, cancha_id);
    }

    return -1;
}

static void cancha_export_construir_detalle(int exportar_todas, int cancha_id, char *detalle, size_t detalle_size)
{
    if (exportar_todas)
    {
        snprintf(detalle, detalle_size, "Todas las canchas");
        return;
    }

    snprintf(detalle, detalle_size, "Cancha ID %d", cancha_id);
}

static void exportar_informacion_canchas()
{
    mostrar_pantalla("EXPORTAR INFORMACION DE CANCHAS");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    int exportar_todas = 1;
    int cancha_id = 0;
    if (!cancha_export_solicitar_alcance(&exportar_todas, &cancha_id))
    {
        return;
    }

    printf("\nComo desea exportar?\n");
    printf("1) En un formato especifico\n");
    printf("2) En todos los formatos\n");
    printf("0) Cancelar\n");

    int modo = input_int("Opcion: ");
    if (modo == 0)
    {
        return;
    }

    int exitos = cancha_export_ejecutar_por_modo(modo, exportar_todas, cancha_id);
    if (exitos == -2)
    {
        return;
    }

    if (exitos == -1)
    {
        printf("Opcion invalida.\n");
        pause_console();
        return;
    }

    if (exitos > 0)
    {
        char detalle[64];
        cancha_export_construir_detalle(exportar_todas, cancha_id, detalle, sizeof(detalle));
        mostrar_alerta_operacion("Cancha", "Informacion Exportada", detalle);
    }
    else
    {
        printf("No se pudo completar la exportacion.\n");
        pause_console();
    }
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
        {9, "Reactivar/Desactivar Cancha", reactivar_cancha},
        {10, "Exportar Informacion", exportar_informacion_canchas},
        {0, "Volver", NULL}
    };

    ejecutar_menu("CANCHAS", items, 11);
}
