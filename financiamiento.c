/**
 * @file financiamiento.c
 * @brief Implementación de funciones para la gestión financiera en MiFutbolC
 */

#include "financiamiento.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "ascii_art.h"
#include "cJSON.h"
#include "sqlite3.h"
#include "partido.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief Convierte un tipo de transacción enumerado a su nombre textual
 */
const char* get_nombre_tipo_transaccion(TipoTransaccion tipo)
{
    switch (tipo)
    {
    case INGRESO:
        return "Ingreso";
    case GASTO:
        return "Gasto";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Convierte una categoría financiera enumerada a su nombre textual
 */
const char* get_nombre_categoria(CategoriaFinanciera categoria)
{
    switch (categoria)
    {
    case TRANSPORTE:
        return "Transporte";
    case EQUIPAMIENTO:
        return "Equipamiento";
    case CUOTAS:
        return "Cuotas";
    case TORNEOS:
        return "Torneos";
    case ARBITRAJE:
        return "Arbitraje";
    case CANCHAS:
        return "Canchas";
    case MEDICINA:
        return "Medicina";
    case OTROS:
        return "Otros";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Retorna un monto entero formateado como string con puntos como separadores
 */
char* formato_monto(int monto)
{
    static char buf[20];
    char temp[20];
    sprintf(temp, "%d", monto);

    int len = strlen(temp);
    int cont = 0;
    int idx = 0;

    for (int i = len - 1; i >= 0; i--)
    {
        buf[idx++] = temp[i];
        cont++;
        if (cont == 3 && i != 0)
        {
            buf[idx++] = '.';
            cont = 0;
        }
    }
    buf[idx] = '\0';

    // Reverse the string
    for (int i = 0; i < idx / 2; i++)
    {
        char t = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = t;
    }

    return buf;
}

/**
 * @brief Muestra un monto entero con formato de miles (puntos como separadores)
 */
void mostrar_monto(int monto)
{
    printf("%s\n", formato_monto(monto));
}

/**
 * @brief Muestra por pantalla toda la información detallada de una transacción financiera
 */
void mostrar_transaccion(TransaccionFinanciera *transaccion)
{
    printf("ID: %d\n", transaccion->id);

    // Convertir fecha de YYYY-MM-DD a DD/MM/YYYY para mostrar
    char fecha_display[11];
    int year, month, day;
    if (sscanf(transaccion->fecha, "%4d-%2d-%2d", &year, &month, &day) == 3)
    {
        sprintf(fecha_display, "%02d/%02d/%04d", day, month, year);
    }
    else
    {
        strcpy(fecha_display, transaccion->fecha); // fallback si no se puede convertir
    }
    printf("Fecha: %s\n", fecha_display);

    printf("Tipo: %s\n", get_nombre_tipo_transaccion(transaccion->tipo));
    printf("Categoria: %s\n", get_nombre_categoria(transaccion->categoria));
    printf("Descripcion: %s\n", transaccion->descripcion);
    printf("Monto: $");
    mostrar_monto(transaccion->monto);
    if (strlen(transaccion->item_especifico) > 0)
    {
        printf("Item Especifico: %s\n", transaccion->item_especifico);
    }
    printf("\n");
}

/**
 * @brief Obtiene la fecha actual en formato YYYY-MM-DD
 */
void obtener_fecha_actual(char *fecha)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    sprintf(fecha, "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
}

/**
 * @brief Convierte una fecha de formato DD/MM/YYYY a YYYY-MM-DD
 * @param fecha_ddmmyyyy Fecha en formato DD/MM/YYYY
 * @param fecha_yyyymmdd Buffer donde se almacenará la fecha convertida (YYYY-MM-DD)
 * @return 1 si la conversión fue exitosa, 0 si hubo error
 */
int convertir_fecha_ddmmyyyy_a_yyyymmdd(const char *fecha_ddmmyyyy, char *fecha_yyyymmdd)
{
    int dia, mes, anio;

    // Intentar parsear la fecha en formato DD/MM/YYYY
    if (sscanf(fecha_ddmmyyyy, "%d/%d/%d", &dia, &mes, &anio) != 3)
    {
        return 0; // Error en el formato
    }

    // Validar rangos básicos
    if (dia < 1 || dia > 31 || mes < 1 || mes > 12 || anio < 1900 || anio > 2100)
    {
        return 0; // Fecha inválida
    }

    // Formatear a YYYY-MM-DD
    sprintf(fecha_yyyymmdd, "%04d-%02d-%02d", anio, mes, dia);
    return 1; // Éxito
}

/**
 * @brief Obtiene el siguiente ID disponible para una nueva transacción financiera
 *
 * Busca el ID más pequeño disponible reutilizando espacios de IDs eliminados.
 * Utiliza una consulta SQL que encuentra el primer hueco en la secuencia de IDs.
 *
 * @return El ID disponible más pequeño (comenzando desde 1 si la tabla está vacía)
 */
static int obtener_siguiente_id_financiamiento()
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "WITH RECURSIVE seq(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM seq WHERE id < (SELECT COALESCE(MAX(id),0)+1 FROM financiamiento)) SELECT MIN(id) FROM seq WHERE id NOT IN (SELECT id FROM financiamiento)",
                       -1, &stmt, NULL);

    int id = 1; // Default si la tabla está vacía
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief Agregar una nueva transacción financiera
 */
void agregar_transaccion()
{
    clear_screen();
    print_header("AGREGAR TRANSACCION FINANCIERA");

    TransaccionFinanciera transaccion;

    // Fecha - usar fecha actual automáticamente (formato YYYY-MM-DD para BD)
    time_t t = time(NULL);
    strftime(transaccion.fecha, sizeof(transaccion.fecha), "%Y-%m-%d", localtime(&t));

    // Tipo de transacción
    printf("\nSeleccione el tipo de transaccion:\n");
    printf("1. Ingreso\n");
    printf("2. Gasto\n");
    printf("0. Volver\n");

    int opcion_tipo = input_int(">");
    switch (opcion_tipo)
    {
    case 1:
        transaccion.tipo = INGRESO;
        break;
    case 2:
        transaccion.tipo = GASTO;
        break;
    case 0:
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    default:
        printf("Opcion invalida. Cancelando.\n");
        pause_console();
        return;
    }

    // Categoría
    printf("\nSeleccione la categoria:\n");
    printf("1. Transporte\n");
    printf("2. Equipamiento\n");
    printf("3. Cuotas\n");
    printf("4. Torneos\n");
    printf("5. Arbitraje\n");
    printf("6. Canchas\n");
    printf("7. Medicina\n");
    printf("8. Otros\n");

    int opcion_categoria = input_int(">");
    switch (opcion_categoria)
    {
    case 1:
        transaccion.categoria = TRANSPORTE;
        break;
    case 2:
        transaccion.categoria = EQUIPAMIENTO;
        break;
    case 3:
        transaccion.categoria = CUOTAS;
        break;
    case 4:
        transaccion.categoria = TORNEOS;
        break;
    case 5:
        transaccion.categoria = ARBITRAJE;
        break;
    case 6:
        transaccion.categoria = CANCHAS;
        break;
    case 7:
        transaccion.categoria = MEDICINA;
        break;
    case 8:
        transaccion.categoria = OTROS;
        break;
    default:
        printf("Opcion invalida. Cancelando.\n");
        pause_console();
        return;
    }

    // Descripción
    input_string("Descripcion: ", transaccion.descripcion, sizeof(transaccion.descripcion));

    // Monto
    transaccion.monto = input_int("Monto: ");

    // Item específico
    if (transaccion.tipo == GASTO && transaccion.categoria == CANCHAS)
    {
        // Mostrar lista de partidos
        printf("\n=== PARTIDOS DISPONIBLES ===\n");
        listar_partidos();
        printf("\n");

        int id_partido = input_int("Ingrese el ID del partido: ");

        // Obtener detalles del partido seleccionado
        sqlite3_stmt *stmt_partido;
        const char *sql_partido = "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                                  "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                                  "JOIN cancha can ON p.cancha_id = can.id WHERE p.id = ?";

        if (sqlite3_prepare_v2(db, sql_partido, -1, &stmt_partido, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt_partido, 1, id_partido);

            if (sqlite3_step(stmt_partido) == SQLITE_ROW)
            {
                // Formatear la fecha para visualización
                char fecha_formateada[20];
                format_date_for_display((const char *)sqlite3_column_text(stmt_partido, 2), fecha_formateada, sizeof(fecha_formateada));

                // Crear el string con los detalles del partido
                sprintf(transaccion.item_especifico, "(%d |Cancha:%s |Fecha:%s | G:%d A:%d |Camiseta:%s | %s |Clima:%s |Dia:%s)",
                        sqlite3_column_int(stmt_partido, 0),
                        sqlite3_column_text(stmt_partido, 1),
                        fecha_formateada,
                        sqlite3_column_int(stmt_partido, 3),
                        sqlite3_column_int(stmt_partido, 4),
                        sqlite3_column_text(stmt_partido, 5),
                        resultado_to_text(sqlite3_column_int(stmt_partido, 6)),
                        clima_to_text(sqlite3_column_int(stmt_partido, 7)),
                        dia_to_text(sqlite3_column_int(stmt_partido, 8)));
            }
            else
            {
                sprintf(transaccion.item_especifico, "Partido ID: %d (no encontrado)", id_partido);
            }
            sqlite3_finalize(stmt_partido);
        }
        else
        {
            sprintf(transaccion.item_especifico, "Partido ID: %d", id_partido);
        }
    }
    else
    {
        printf("Item especifico (opcional, ej: 'Botines Nike', 'Cuota enero'): ");
        input_string("", transaccion.item_especifico, sizeof(transaccion.item_especifico));
    }

    // Obtener el ID y asignarlo a la transacción
    transaccion.id = obtener_siguiente_id_financiamiento();

    // Mostrar resumen y confirmar
    clear_screen();
    print_header("CONFIRMAR TRANSACCION");
    mostrar_transaccion(&transaccion);

    if (confirmar("Desea guardar esta transaccion?"))
    {
        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO financiamiento (id, fecha, tipo, categoria, descripcion, monto, item_especifico) VALUES (?, ?, ?, ?, ?, ?, ?);";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
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
                printf("Transaccion guardada exitosamente con ID: %lld\n", sqlite3_last_insert_rowid(db));
            }
            else
            {
                printf("Error al guardar la transaccion: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        }
    }
    else
    {
        printf("Transaccion cancelada.\n");
    }

    pause_console();
}

/**
 * @brief Mostrar resumen financiero del equipo
 */
void mostrar_resumen_financiero()
{
    clear_screen();
    print_header("RESUMEN FINANCIERO DEL EQUIPO");

    sqlite3_stmt *stmt;

    // Obtener estadísticas generales
    int total_ingresos = 0;
    int total_gastos = 0;
    int num_transacciones = 0;

    const char *sql_totales = "SELECT tipo, SUM(monto), COUNT(*) FROM financiamiento GROUP BY tipo;";

    if (sqlite3_prepare_v2(db, sql_totales, -1, &stmt, 0) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int tipo = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            if (tipo == INGRESO)
            {
                total_ingresos = suma;
            }
            else
            {
                total_gastos = suma;
            }
            num_transacciones += count;
        }
        sqlite3_finalize(stmt);
    }

    // Obtener desglose por categorías
    printf("\n=== RESUMEN GENERAL ===\n");
    printf("Total de transacciones: %d\n", num_transacciones);
    printf("Total Ingresos: $");
    mostrar_monto(total_ingresos);
    printf("Total Gastos: $");
    mostrar_monto(total_gastos);
    printf("Balance Neto: $");
    mostrar_monto(total_ingresos - total_gastos);

    if (num_transacciones == 0)
    {
        printf("\nNo hay transacciones registradas.\n");
        pause_console();
        return;
    }

    // Desglose por categorías de ingresos
    printf("\n=== INGRESOS POR CATEGORIA ===\n");
    const char *sql_ingresos = "SELECT categoria, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 0 GROUP BY categoria ORDER BY SUM(monto) DESC;";

    if (sqlite3_prepare_v2(db, sql_ingresos, -1, &stmt, 0) == SQLITE_OK)
    {
        int found_ingresos = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_ingresos = 1;
            int categoria = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%s: $", get_nombre_categoria(categoria));
            mostrar_monto(suma);
            printf(" (%d transacciones)\n", count);
        }
        sqlite3_finalize(stmt);

        if (!found_ingresos)
        {
            printf("No hay ingresos registrados.\n");
        }
    }

    // Desglose por categorías de gastos
    printf("\n=== GASTOS POR CATEGORIA ===\n");
    const char *sql_gastos = "SELECT categoria, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 GROUP BY categoria ORDER BY SUM(monto) DESC;";

    if (sqlite3_prepare_v2(db, sql_gastos, -1, &stmt, 0) == SQLITE_OK)
    {
        int found_gastos = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_gastos = 1;
            int categoria = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%s: $", get_nombre_categoria(categoria));
            mostrar_monto(suma);
            printf(" (%d transacciones)\n", count);
        }
        sqlite3_finalize(stmt);

        if (!found_gastos)
        {
            printf("No hay gastos registrados.\n");
        }
    }

    // Estadísticas de equipamiento (gastos más comunes)
    printf("\n=== TOP ITEMS DE EQUIPAMIENTO ===\n");
    const char *sql_equipamiento = "SELECT item_especifico, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 AND categoria = 1 AND item_especifico != '' GROUP BY item_especifico ORDER BY SUM(monto) DESC LIMIT 10;";

    if (sqlite3_prepare_v2(db, sql_equipamiento, -1, &stmt, 0) == SQLITE_OK)
    {
        int found_equip = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_equip = 1;
            const char *item = (const char*)sqlite3_column_text(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%s: $", item);
            mostrar_monto(suma);
            printf(" (%d compras)\n", count);
        }
        sqlite3_finalize(stmt);

        if (!found_equip)
        {
            printf("No hay compras de equipamiento especificadas.\n");
        }
    }

    // Balance mensual (últimos 12 meses)
    printf("\n=== BALANCE MENSUAL (ULTIMOS 12 MESES) ===\n");
    const char *sql_mensual = "SELECT strftime('%Y-%m', fecha) as mes, "
                              "SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) as ingresos, "
                              "SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) as gastos "
                              "FROM financiamiento "
                              "WHERE fecha >= date('now', '-12 months') "
                              "GROUP BY mes ORDER BY mes DESC;";

    if (sqlite3_prepare_v2(db, sql_mensual, -1, &stmt, 0) == SQLITE_OK)
    {
        int found_mensual = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_mensual = 1;
            const char *mes = (const char*)sqlite3_column_text(stmt, 0);
            int ingresos_mes = sqlite3_column_int(stmt, 1);
            int gastos_mes = sqlite3_column_int(stmt, 2);

            printf("%s: Ingresos $", mes);
            mostrar_monto(ingresos_mes);
            printf(", Gastos $");
            mostrar_monto(gastos_mes);
            printf(", Balance $");
            mostrar_monto(ingresos_mes - gastos_mes);
            printf("\n");
        }
        sqlite3_finalize(stmt);

        if (!found_mensual)
        {
            printf("No hay datos suficientes para mostrar balance mensual.\n");
        }
    }

    pause_console();
}

/**
 * @brief Mostrar balance general de gastos
 */
void ver_balance_gastos()
{
    clear_screen();
    print_header("BALANCE GENERAL DE GASTOS");

    sqlite3_stmt *stmt;

    // Obtener total de gastos
    int total_gastos = 0;
    int num_gastos = 0;

    const char *sql_total_gastos = "SELECT SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1;";

    if (sqlite3_prepare_v2(db, sql_total_gastos, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total_gastos = sqlite3_column_int(stmt, 0);
            num_gastos = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    printf("\n=== BALANCE GENERAL DE GASTOS ===\n");
    printf("Total de gastos registrados: %d\n", num_gastos);
    printf("Monto total de gastos: $");
    mostrar_monto(total_gastos);
    printf("\n\n");

    if (num_gastos == 0)
    {
        printf("No hay gastos registrados.\n");
        pause_console();
        return;
    }

    // Desglose por categorías de gastos
    printf("=== DESGLOSE POR CATEGORIAS ===\n");
    const char *sql_gastos_categoria = "SELECT categoria, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 GROUP BY categoria ORDER BY SUM(monto) DESC;";

    if (sqlite3_prepare_v2(db, sql_gastos_categoria, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("%-15s %-12s %-10s %-8s\n", "Categoria", "Total", "Cantidad", "Porcentaje");
        printf("--------------------------------------------------\n");

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int categoria = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);
            double porcentaje = (suma / (double)total_gastos) * 100.0;

            printf("%-15s $%s %-10d %-7.1f%%\n",
                   get_nombre_categoria(categoria), formato_monto(suma), count, porcentaje);
        }
        sqlite3_finalize(stmt);
    }

    // Top 5 gastos más altos
    printf("\n=== TOP 5 GASTOS MAS ALTOS ===\n");
    const char *sql_top_gastos = "SELECT descripcion, monto, fecha, categoria FROM financiamiento WHERE tipo = 1 ORDER BY monto DESC LIMIT 5;";

    if (sqlite3_prepare_v2(db, sql_top_gastos, -1, &stmt, 0) == SQLITE_OK)
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count++;
            const char *descripcion = (const char*)sqlite3_column_text(stmt, 0);
            int monto = sqlite3_column_int(stmt, 1);
            const char *fecha = (const char*)sqlite3_column_text(stmt, 2);
            int categoria = sqlite3_column_int(stmt, 3);

            printf("%d. $", count);
            mostrar_monto(monto);
            printf(" - %s (%s - %s)\n", descripcion, fecha, get_nombre_categoria(categoria));
        }
        sqlite3_finalize(stmt);

        if (count == 0)
        {
            printf("No hay gastos registrados.\n");
        }
    }

    // Balance mensual de gastos (últimos 6 meses)
    printf("\n=== BALANCE MENSUAL DE GASTOS (ULTIMOS 6 MESES) ===\n");
    const char *sql_mensual_gastos = "SELECT strftime('%Y-%m', fecha) as mes, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 AND fecha >= date('now', '-6 months') GROUP BY mes ORDER BY mes DESC;";

    if (sqlite3_prepare_v2(db, sql_mensual_gastos, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("%-8s %-12s %-10s\n", "Mes", "Total", "Cantidad");
        printf("----------------------------\n");

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *mes = (const char*)sqlite3_column_text(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%-8s $", mes);
            mostrar_monto(suma);
            printf(" %-10d\n", count);
        }
        sqlite3_finalize(stmt);
    }

    printf("\n=== RESUMEN EJECUTIVO ===\n");
    printf("Total gastado por el equipo: $");
    mostrar_monto(total_gastos);
    float promedio = num_gastos > 0 ? (float)total_gastos / num_gastos : 0.0;
    printf("Promedio por gasto: $%.2f\n", promedio);

    pause_console();
}

/**
 * @brief Exportar transacciones financieras a múltiples formatos
 */
void exportar_financiamiento()
{
    clear_screen();
    print_header("EXPORTAR FINANCIAMIENTO");

    // Obtener directorio de exportación
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        printf("Error: No se pudo obtener el directorio de exportacion.\n");
        pause_console();
        return;
    }

    // Generar timestamp para los archivos
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timestamp[32];
    sprintf(timestamp, "%04d%02d%02d_%02d%02d%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    printf("Exportando datos de financiamiento en todos los formatos...\n\n");

    // Obtener todas las transacciones
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento ORDER BY fecha DESC, id DESC;";

    int count = 0;
    int total_ingresos = 0;
    int total_gastos = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        pause_console();
        return;
    }

    // Almacenar datos en memoria para múltiples exports
    cJSON *json_array = cJSON_CreateArray();

    // CSV Export
    char csv_filename[300];
    sprintf(csv_filename, "%s\\financiamiento_%s.csv", export_dir, timestamp);
    FILE *csv_file = fopen(csv_filename, "w");
    if (csv_file)
    {
        fprintf(csv_file, "ID,Fecha,Tipo,Categoria,Descripcion,Monto,Item_Especifico\n");
    }

    // TXT Export
    char txt_filename[300];
    sprintf(txt_filename, "%s\\financiamiento_%s.txt", export_dir, timestamp);
    FILE *txt_file = fopen(txt_filename, "w");
    if (txt_file)
    {
        fprintf(txt_file, "LISTADO DE TRANSACCIONES FINANCIERAS\n");
        fprintf(txt_file, "=====================================\n\n");
    }

    // HTML Export
    char html_filename[300];
    sprintf(html_filename, "%s\\financiamiento_%s.html", export_dir, timestamp);
    FILE *html_file = fopen(html_filename, "w");
    if (html_file)
    {
        fprintf(html_file, "<html><body><h1>Transacciones Financieras</h1>");
        fprintf(html_file, "<table border='1'><tr><th>ID</th><th>Fecha</th><th>Tipo</th><th>Categoria</th><th>Descripcion</th><th>Monto</th><th>Item Especifico</th></tr>");
    }

    // Procesar todas las transacciones
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char*)sqlite3_column_text(stmt, 1);
        int tipo = sqlite3_column_int(stmt, 2);
        int categoria = sqlite3_column_int(stmt, 3);
        const char *descripcion = (const char*)sqlite3_column_text(stmt, 4);
        int monto = sqlite3_column_int(stmt, 5);
        const char *item = (const char*)sqlite3_column_text(stmt, 6);

        count++;
        if (tipo == INGRESO)
        {
            total_ingresos += monto;
        }
        else
        {
            total_gastos += monto;
        }

        // CSV
        if (csv_file)
        {
            fprintf(csv_file, "%d,%s,%s,%s,\"%s\",%d",
                    id, fecha, get_nombre_tipo_transaccion(tipo),
                    get_nombre_categoria(categoria), descripcion, monto);

            if (item && strlen(item) > 0)
            {
                fprintf(csv_file, ",\"%s\"", item);
            }
            else
            {
                fprintf(csv_file, ",");
            }
            fprintf(csv_file, "\n");
        }

        // TXT
        if (txt_file)
        {
            fprintf(txt_file, "ID: %d\n", id);
            fprintf(txt_file, "Fecha: %s\n", fecha);
            fprintf(txt_file, "Tipo: %s\n", get_nombre_tipo_transaccion(tipo));
            fprintf(txt_file, "Categoria: %s\n", get_nombre_categoria(categoria));
            fprintf(txt_file, "Descripcion: %s\n", descripcion);
            fprintf(txt_file, "Monto: $%s\n", formato_monto(monto));
            if (item && strlen(item) > 0)
            {
                fprintf(txt_file, "Item Especifico: %s\n", item);
            }
            fprintf(txt_file, "----------------------------------------\n");
        }

        // HTML
        if (html_file)
        {
            fprintf(html_file, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>$%s</td><td>%s</td></tr>",
                    id, fecha, get_nombre_tipo_transaccion(tipo),
                    get_nombre_categoria(categoria), descripcion, formato_monto(monto),
                    (item && strlen(item) > 0) ? item : "");
        }

        // JSON
        cJSON *item_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(item_obj, "id", id);
        cJSON_AddStringToObject(item_obj, "fecha", fecha);
        cJSON_AddStringToObject(item_obj, "tipo", get_nombre_tipo_transaccion(tipo));
        cJSON_AddStringToObject(item_obj, "categoria", get_nombre_categoria(categoria));
        cJSON_AddStringToObject(item_obj, "descripcion", descripcion);
        cJSON_AddNumberToObject(item_obj, "monto", monto);
        if (item && strlen(item) > 0)
        {
            cJSON_AddStringToObject(item_obj, "item_especifico", item);
        }
        else
        {
            cJSON_AddStringToObject(item_obj, "item_especifico", "");
        }
        cJSON_AddItemToArray(json_array, item_obj);
    }

    sqlite3_finalize(stmt);

    // Finalizar archivos

    // CSV - Agregar resumen
    if (csv_file)
    {
        fprintf(csv_file, "\n");
        fprintf(csv_file, "RESUMEN,,Total Transacciones:,%d\n", count);
        fprintf(csv_file, "RESUMEN,,Total Ingresos:,$%d\n", total_ingresos);
        fprintf(csv_file, "RESUMEN,,Total Gastos:,$%d\n", total_gastos);
        fprintf(csv_file, "RESUMEN,,Balance Neto:,$%d\n", total_ingresos - total_gastos);
        fclose(csv_file);
        printf("CSV exportado: %s\n", csv_filename);
    }

    // TXT - Agregar resumen
    if (txt_file)
    {
        fprintf(txt_file, "\nRESUMEN GENERAL\n");
        fprintf(txt_file, "================\n");
        fprintf(txt_file, "Total de transacciones: %d\n", count);
        fprintf(txt_file, "Total Ingresos: $%s\n", formato_monto(total_ingresos));
        fprintf(txt_file, "Total Gastos: $%s\n", formato_monto(total_gastos));
        fprintf(txt_file, "Balance Neto: $%s\n", formato_monto(total_ingresos - total_gastos));
        fclose(txt_file);
        printf("TXT exportado: %s\n", txt_filename);
    }

    // HTML - Cerrar tabla y agregar resumen
    if (html_file)
    {
        fprintf(html_file, "</table>");
        fprintf(html_file, "<h2>Resumen General</h2>");
        fprintf(html_file, "<table border='1'>");
        fprintf(html_file, "<tr><th>Total Transacciones</th><td>%d</td></tr>", count);
        fprintf(html_file, "<tr><th>Total Ingresos</th><td>$%s</td></tr>", formato_monto(total_ingresos));
        fprintf(html_file, "<tr><th>Total Gastos</th><td>$%s</td></tr>", formato_monto(total_gastos));
        fprintf(html_file, "<tr><th>Balance Neto</th><td>$%s</td></tr>", formato_monto(total_ingresos - total_gastos));
        fprintf(html_file, "</table></body></html>");
        fclose(html_file);
        printf("HTML exportado: %s\n", html_filename);
    }

    // JSON
    char json_filename[300];
    sprintf(json_filename, "%s\\financiamiento_%s.json", export_dir, timestamp);
    FILE *json_file = fopen(json_filename, "w");
    if (json_file)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "transacciones", json_array);

        cJSON *resumen = cJSON_CreateObject();
        cJSON_AddNumberToObject(resumen, "total_transacciones", count);
        cJSON_AddNumberToObject(resumen, "total_ingresos", total_ingresos);
        cJSON_AddNumberToObject(resumen, "total_gastos", total_gastos);
        cJSON_AddNumberToObject(resumen, "balance_neto", total_ingresos - total_gastos);
        cJSON_AddItemToObject(root, "resumen", resumen);

        char *json_string = cJSON_Print(root);
        fprintf(json_file, "%s", json_string);
        free(json_string);
        cJSON_Delete(root);
        fclose(json_file);
        printf("JSON exportado: %s\n", json_filename);
    }
    else
    {
        cJSON_Delete(json_array);
    }

    printf("\nExportacion completada exitosamente!\n");
    printf("Total de transacciones exportadas: %d\n", count);
    printf("Balance neto: $");
    mostrar_monto(total_ingresos - total_gastos);

    pause_console();
}

/**
 * @brief Menú principal de gestión financiera
 */
void menu_financiamiento()
{
    MenuItem items[] =
    {
        {1, "Agregar Transaccion", agregar_transaccion},
        {2, "Listar Transacciones", listar_transacciones},
        {3, "Modificar Transaccion", modificar_transaccion},
        {4, "Eliminar Transaccion", eliminar_transaccion},
        {5, "Ver Resumen Financiero", mostrar_resumen_financiero},
        {6, "Balance General de Gastos", ver_balance_gastos},
        {7, "Exportar Datos", exportar_financiamiento},
        {0, "Volver", NULL}
    };

    ejecutar_menu("FINANCIAMIENTO", items, 8);
}

/**
 * @brief Modificar una transacción financiera existente
 */
void modificar_transaccion()
{
    clear_screen();
    print_header("MODIFICAR TRANSACCION FINANCIERA");

    // Mostrar lista de todas las transacciones
    sqlite3_stmt *stmt;
    const char *sql_lista = "SELECT id, fecha, tipo, categoria, descripcion, monto FROM financiamiento ORDER BY fecha DESC, id DESC;";

    if (sqlite3_prepare_v2(db, sql_lista, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== TODAS LAS TRANSACCIONES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *fecha_db = (const char*)sqlite3_column_text(stmt, 1);
            int tipo = sqlite3_column_int(stmt, 2);
            int categoria = sqlite3_column_int(stmt, 3);
            const char *descripcion = (const char*)sqlite3_column_text(stmt, 4);
            int monto = sqlite3_column_int(stmt, 5);

            // Convertir fecha de YYYY-MM-DD a DD/MM/YYYY para mostrar
            char fecha_display[11];
            int year, month, day;
            if (sscanf(fecha_db, "%4d-%2d-%2d", &year, &month, &day) == 3)
            {
                sprintf(fecha_display, "%02d/%02d/%04d", day, month, year);
            }
            else
            {
                strcpy(fecha_display, fecha_db); // fallback si no se puede convertir
            }

            printf("ID: %d | %s | %s | %s | %s | $", id, fecha_display, get_nombre_tipo_transaccion(tipo), get_nombre_categoria(categoria), descripcion);
            mostrar_monto(monto);
            printf("\n");
        }
        sqlite3_finalize(stmt);

        if (!found)
        {
            printf("No hay transacciones registradas.\n");
            pause_console();
            return;
        }
    }

    int id_transaccion = input_int("\nIngrese el ID de la transaccion a modificar (0 para cancelar): ");

    if (id_transaccion == 0) return;

    // Verificar que existe
    if (!existe_id("financiamiento", id_transaccion))
    {
        printf("ID de transaccion invalido.\n");
        pause_console();
        return;
    }

    // Obtener datos actuales
    TransaccionFinanciera transaccion;
    const char *sql_obtener = "SELECT fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql_obtener, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, id_transaccion);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            transaccion.id = id_transaccion;
            strncpy(transaccion.fecha, (const char*)sqlite3_column_text(stmt, 0), sizeof(transaccion.fecha));
            transaccion.tipo = sqlite3_column_int(stmt, 1);
            transaccion.categoria = sqlite3_column_int(stmt, 2);
            strncpy(transaccion.descripcion, (const char*)sqlite3_column_text(stmt, 3), sizeof(transaccion.descripcion));
            transaccion.monto = sqlite3_column_int(stmt, 4);
            const char *item = (const char*)sqlite3_column_text(stmt, 5);
            if (item)
            {
                strncpy(transaccion.item_especifico, item, sizeof(transaccion.item_especifico));
            }
            else
            {
                transaccion.item_especifico[0] = '\0';
            }
        }
        sqlite3_finalize(stmt);
    }

    // Mostrar datos actuales y opciones de modificación
    clear_screen();
    print_header("MODIFICAR TRANSACCION");
    printf("Datos actuales:\n");
    mostrar_transaccion(&transaccion);

    printf("Seleccione que desea modificar:\n");
    printf("1. Fecha\n");
    printf("2. Tipo\n");
    printf("3. Categoria\n");
    printf("4. Descripcion\n");
    printf("5. Monto\n");
    printf("6. Item especifico\n");
    printf("7. Volver\n");

    int opcion = input_int(">");

    char sql_update[200];
    strcpy(sql_update, "UPDATE financiamiento SET ");

    switch (opcion)
    {
    case 1:
    {
        printf("Nueva fecha (YYYY-MM-DD): ");
        char nueva_fecha[20];
        input_date("", nueva_fecha, sizeof(nueva_fecha));
        if (strlen(nueva_fecha) > 0)
        {
            sprintf(sql_update + strlen(sql_update), "fecha = ? WHERE id = ?;");
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt, 1, nueva_fecha, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 2, id_transaccion);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                printf("Fecha actualizada exitosamente.\n");
            }
        }
        break;
    }

    case 2:
    {
        printf("Nuevo tipo:\n1. Ingreso\n2. Gasto\n");
        int nuevo_tipo = input_int(">") - 1;
        if (nuevo_tipo >= 0 && nuevo_tipo <= 1)
        {
            sprintf(sql_update + strlen(sql_update), "tipo = ? WHERE id = ?;");
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, nuevo_tipo);
                sqlite3_bind_int(stmt, 2, id_transaccion);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                printf("Tipo actualizado exitosamente.\n");
            }
        }
        break;
    }

    case 3:
    {
        printf("Nueva categoria:\n");
        printf("1. Transporte\n2. Equipamiento\n3. Cuotas\n4. Torneos\n");
        printf("5. Arbitraje\n6. Canchas\n7. Medicina\n8. Otros\n");
        int nueva_categoria = input_int(">") - 1;
        if (nueva_categoria >= 0 && nueva_categoria <= 7)
        {
            sprintf(sql_update + strlen(sql_update), "categoria = ? WHERE id = ?;");
            if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, nueva_categoria);
                sqlite3_bind_int(stmt, 2, id_transaccion);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                printf("Categoria actualizada exitosamente.\n");
            }
        }
        break;
    }

    case 4:
    {
        printf("Nueva descripcion: ");
        char nueva_descripcion[200];
        input_string("", nueva_descripcion, sizeof(nueva_descripcion));
        sprintf(sql_update + strlen(sql_update), "descripcion = ? WHERE id = ?;");
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, nueva_descripcion, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, id_transaccion);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            printf("Descripcion actualizada exitosamente.\n");
        }
        break;
    }

    case 5:
    {
        int nuevo_monto = input_int("Nuevo monto: ");
        sprintf(sql_update + strlen(sql_update), "monto = ? WHERE id = ?;");
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, nuevo_monto);
            sqlite3_bind_int(stmt, 2, id_transaccion);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            printf("Monto actualizado exitosamente.\n");
        }
        break;
    }

    case 6:
    {
        printf("Nuevo item especifico: ");
        char nuevo_item[100];
        input_string("", nuevo_item, sizeof(nuevo_item));
        sprintf(sql_update + strlen(sql_update), "item_especifico = ? WHERE id = ?;");
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, nuevo_item, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, id_transaccion);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            printf("Item especifico actualizado exitosamente.\n");
        }
        break;
    }

    case 7:
        return;

    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

/**
 * @brief Eliminar una transacción financiera
 */
void eliminar_transaccion()
{
    clear_screen();
    print_header("ELIMINAR TRANSACCION FINANCIERA");

    // Mostrar lista de transacciones recientes
    sqlite3_stmt *stmt;
    const char *sql_lista = "SELECT id, fecha, tipo, categoria, descripcion, monto FROM financiamiento ORDER BY fecha DESC, id DESC LIMIT 10;";

    if (sqlite3_prepare_v2(db, sql_lista, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== ULTIMAS 10 TRANSACCIONES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *fecha = (const char*)sqlite3_column_text(stmt, 1);
            int tipo = sqlite3_column_int(stmt, 2);
            int categoria = sqlite3_column_int(stmt, 3);
            const char *descripcion = (const char*)sqlite3_column_text(stmt, 4);
            int monto = sqlite3_column_int(stmt, 5);

            printf("ID: %d | %s | %s | %s | %s | $", id, fecha, get_nombre_tipo_transaccion(tipo), get_nombre_categoria(categoria), descripcion);
            mostrar_monto(monto);
            printf("\n");
        }
        sqlite3_finalize(stmt);

        if (!found)
        {
            printf("No hay transacciones registradas.\n");
            pause_console();
            return;
        }
    }

    int id_transaccion = input_int("\nIngrese el ID de la transaccion a eliminar (0 para cancelar): ");

    if (id_transaccion == 0) return;

    // Verificar que existe
    if (!existe_id("financiamiento", id_transaccion))
    {
        printf("ID de transaccion invalido.\n");
        pause_console();
        return;
    }

    // Mostrar la transacción antes de eliminar
    const char *sql_obtener = "SELECT fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql_obtener, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, id_transaccion);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            TransaccionFinanciera transaccion;
            transaccion.id = id_transaccion;
            strncpy(transaccion.fecha, (const char*)sqlite3_column_text(stmt, 0), sizeof(transaccion.fecha));
            transaccion.tipo = sqlite3_column_int(stmt, 1);
            transaccion.categoria = sqlite3_column_int(stmt, 2);
            strncpy(transaccion.descripcion, (const char*)sqlite3_column_text(stmt, 3), sizeof(transaccion.descripcion));
            transaccion.monto = sqlite3_column_int(stmt, 4);
            const char *item = (const char*)sqlite3_column_text(stmt, 5);
            if (item)
            {
                strncpy(transaccion.item_especifico, item, sizeof(transaccion.item_especifico));
            }
            else
            {
                transaccion.item_especifico[0] = '\0';
            }

            printf("\nTransaccion a eliminar:\n");
            mostrar_transaccion(&transaccion);
        }
        sqlite3_finalize(stmt);
    }

    if (confirmar("Esta seguro que desea eliminar esta transaccion? Esta accion no se puede deshacer."))
    {
        const char *sql_delete = "DELETE FROM financiamiento WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, id_transaccion);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Transaccion eliminada exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar la transaccion: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Eliminacion cancelada.\n");
    }

    pause_console();
}

/**
 * @brief Listar todas las transacciones financieras
 */
void listar_transacciones()
{
    clear_screen();
    print_header("LISTAR TRANSACCIONES FINANCIERAS");

    // Listar todas las transacciones sin filtros
    const char *sql = "SELECT id, fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento ORDER BY fecha DESC, id DESC;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        printf("\n=== TODAS LAS TRANSACCIONES FINANCIERAS ===\n\n");

        int total_ingresos = 0;
        int total_gastos = 0;
        int count = 0;

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count++;
            TransaccionFinanciera transaccion;

            transaccion.id = sqlite3_column_int(stmt, 0);
            strncpy(transaccion.fecha, (const char*)sqlite3_column_text(stmt, 1), sizeof(transaccion.fecha));
            transaccion.tipo = sqlite3_column_int(stmt, 2);
            transaccion.categoria = sqlite3_column_int(stmt, 3);
            strncpy(transaccion.descripcion, (const char*)sqlite3_column_text(stmt, 4), sizeof(transaccion.descripcion));
            transaccion.monto = sqlite3_column_int(stmt, 5);
            const char *item = (const char*)sqlite3_column_text(stmt, 6);
            if (item)
            {
                strncpy(transaccion.item_especifico, item, sizeof(transaccion.item_especifico));
            }
            else
            {
                transaccion.item_especifico[0] = '\0';
            }

            // Acumuladores para resumen
            if (transaccion.tipo == INGRESO)
            {
                total_ingresos += transaccion.monto;
            }
            else
            {
                total_gastos += transaccion.monto;
            }

            // Mostrar transacción
            printf("----------------------------------------\n");
            mostrar_transaccion(&transaccion);
        }

        sqlite3_finalize(stmt);

        if (count == 0)
        {
            printf("No hay transacciones registradas.\n");
        }
        else
        {
            printf("========================================\n");
            printf("RESUMEN GENERAL:\n");
            printf("Total Ingresos: $");
            mostrar_monto(total_ingresos);
            printf("Total Gastos: $");
            mostrar_monto(total_gastos);
            printf("Balance: $");
            mostrar_monto(total_ingresos - total_gastos);
            printf("Total de transacciones: %d\n", count);
        }
    }
    else
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
    }

    pause_console();
}
