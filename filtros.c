#include "filtros.h"
#include "db.h"
#include "menu.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static void inicializar_filtros(FiltrosBusqueda *filtro)
{
    memset(filtro, 0, sizeof(FiltrosBusqueda));
}

static void editar_filtro_por_opcion(FiltrosBusqueda *filtro, int opcion)
{
    switch (opcion)
    {
    case 1:
        input_string("Fecha desde (YYYY-MM-DD): ", filtro->fecha_desde,
                     sizeof(filtro->fecha_desde));
        filtro->usar_fecha_desde = 1;
        break;
    case 2:
        input_string("Fecha hasta (YYYY-MM-DD): ", filtro->fecha_hasta,
                     sizeof(filtro->fecha_hasta));
        filtro->usar_fecha_hasta = 1;
        break;
    case 3:
        filtro->resultado = input_int("Resultado (1=Victoria, 2=Empate, 3=Derrota): ");
        filtro->usar_resultado = (filtro->resultado >= 1 && filtro->resultado <= 3) ? 1 : 0;
        break;
    case 4:
        filtro->equipo_id = input_int("ID del equipo: ");
        filtro->usar_equipo = (filtro->equipo_id > 0) ? 1 : 0;
        break;
    case 5:
        input_string("Formacion (ej: 4-4-2): ", filtro->formacion, sizeof(filtro->formacion));
        filtro->usar_formacion = (filtro->formacion[0] != '\0') ? 1 : 0;
        break;
    default:
        break;
    }
}

static void menu_configurar_filtros(FiltrosBusqueda *filtro)
{
    int opcion;
    do
    {
        mostrar_pantalla("FILTROS AVANZADOS");
        printf("  Filtros activos:\n");
        printf("  1. Fecha desde: %s\n", filtro->usar_fecha_desde ? filtro->fecha_desde : "(no)");
        printf("  2. Fecha hasta: %s\n", filtro->usar_fecha_hasta ? filtro->fecha_hasta : "(no)");
        printf("  3. Resultado: %s\n",
               filtro->usar_resultado ? resultado_to_text(filtro->resultado) : "(no)");
        printf("  4. ID Equipo: %s\n", filtro->usar_equipo ? "(activo)" : "(no)");
        printf("  5. Formacion: %s\n", filtro->usar_formacion ? filtro->formacion : "(no)");
        printf("  0. Aplicar y Volver\n");

        opcion = input_int("Opcion: ");
        editar_filtro_por_opcion(filtro, opcion);
    }
    while (opcion != 0);
}

static int construir_sql_filtros(FiltrosBusqueda const *filtro, char *sql, size_t sql_size,
                                 char condiciones[][256], int *num_condiciones)
{
    snprintf(sql, sql_size,
             "SELECT p.id, p.fecha_hora, p.goles, p.asistencias, "
             "p.resultado, p.clima, p.dia, can.nombre "
             "FROM partido p LEFT JOIN cancha can ON p.cancha_id = can.id WHERE 1=1");

    int num_cond = 0;
    if (filtro->usar_fecha_desde && filtro->fecha_desde[0])
    {
        snprintf(condiciones[num_cond], 256, " AND p.fecha_hora >= ?");
        num_cond++;
    }
    if (filtro->usar_fecha_hasta && filtro->fecha_hasta[0])
    {
        snprintf(condiciones[num_cond], 256, " AND p.fecha_hora <= ?");
        num_cond++;
    }
    if (filtro->usar_resultado)
    {
        snprintf(condiciones[num_cond], 256, " AND p.resultado = ?");
        num_cond++;
    }
    if (filtro->usar_equipo && filtro->equipo_id > 0)
    {
        snprintf(condiciones[num_cond], 256, " AND p.equipo_id = ?");
        num_cond++;
    }
    if (filtro->usar_formacion && filtro->formacion[0])
    {
        snprintf(condiciones[num_cond], 256, " AND p.formacion = ?");
        num_cond++;
    }
    *num_condiciones = num_cond;

    for (int i = 0; i < num_cond; i++)
    {
        strcat_s(sql, sql_size, condiciones[i]);
    }

    strcat_s(sql, sql_size, " ORDER BY p.fecha_hora DESC LIMIT 100");
    return 1;
}

static int bind_y_ejecutar_filtros(FiltrosBusqueda const *filtro, const char *sql)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error en consulta.\n");
        return 0;
    }

    int param_idx = 1;
    if (filtro->usar_fecha_desde && filtro->fecha_desde[0])
    {
        sqlite3_bind_text(stmt, param_idx++, filtro->fecha_desde, -1, SQLITE_STATIC);
    }
    if (filtro->usar_fecha_hasta && filtro->fecha_hasta[0])
    {
        sqlite3_bind_text(stmt, param_idx++, filtro->fecha_hasta, -1, SQLITE_STATIC);
    }
    if (filtro->usar_resultado)
    {
        sqlite3_bind_int(stmt, param_idx++, filtro->resultado);
    }
    if (filtro->usar_equipo && filtro->equipo_id > 0)
    {
        sqlite3_bind_int(stmt, param_idx++, filtro->equipo_id);
    }
    if (filtro->usar_formacion && filtro->formacion[0])
    {
        sqlite3_bind_text(stmt, param_idx++, filtro->formacion, -1, SQLITE_STATIC);
    }

    mostrar_pantalla("RESULTADOS DE BUSQUEDA");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. %s | G:%d A:%d | %s\n", sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3), resultado_to_text(sqlite3_column_int(stmt, 4)));
    }
    sqlite3_finalize(stmt);
    return count;
}

int aplicar_filtros_partidos(FiltrosBusqueda const *filtro)
{
    char sql[2048];
    char condiciones[8][256];
    int num_cond = 0;

    construir_sql_filtros(filtro, sql, sizeof(sql), condiciones, &num_cond);
    int count = bind_y_ejecutar_filtros(filtro, sql);

    if (count == 0)
    {
        mostrar_no_hay_registros("partidos con esos filtros");
    }
    else
    {
        printf("\nTotal: %d partidos encontrados.\n", count);

        pause_console();
    }
    return count;
}

static void buscar_con_filtros(void)
{
    FiltrosBusqueda filtro;
    inicializar_filtros(&filtro);
    menu_configurar_filtros(&filtro);
    aplicar_filtros_partidos(&filtro);
}

static void buscar_por_estadisticas(void)
{
    mostrar_pantalla("ESTADISTICAS FILTRADAS");

    FiltrosBusqueda filtro;
    inicializar_filtros(&filtro);

    filtro.usar_resultado = 1;
    filtro.resultado = input_int("Resultado (1=V, 2=E, 3=D): ");

    char fecha_desde[32];
    input_string("Fecha desde (YYYY-MM-DD, dejar vacio para todas): ", fecha_desde,
                 sizeof(fecha_desde));
    if (fecha_desde[0])
    {
        filtro.usar_fecha_desde = 1;
        strcpy_s(filtro.fecha_desde, sizeof(filtro.fecha_desde), fecha_desde);
    }

    int total = aplicar_filtros_partidos(&filtro);
    if (total > 0)
    {
        printf("\nResumen:\n");
        printf("  Total partidos: %d\n", total);
    }
}

void menu_filtros_avanzados(void)
{
    MenuItem items[] = {{1, "Busqueda por Filtros", &buscar_con_filtros},
        {2, "Estadisticas Filtradas", &buscar_por_estadisticas},
        {0, "Volver", NULL}
    };
    ejecutar_menu("FILTROS AVANZADOS", items, 3);
}
