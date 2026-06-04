#include "filtros.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "ascii_charts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static void inicializar_filtros(FiltrosBusqueda *f)
{
    memset(f, 0, sizeof(FiltrosBusqueda));
}

static void menu_configurar_filtros(FiltrosBusqueda *f)
{
    int opcion;
    do
    {
        mostrar_pantalla("FILTROS AVANZADOS");
        printf("  Filtros activos:\n");
        printf("  1. Fecha desde: %s\n", f->usar_fecha_desde ? f->fecha_desde : "(no)");
        printf("  2. Fecha hasta: %s\n", f->usar_fecha_hasta ? f->fecha_hasta : "(no)");
        printf("  3. Resultado: %s\n", f->usar_resultado ? resultado_to_text(f->resultado) : "(no)");
        printf("  4. ID Equipo: %s\n", f->usar_equipo ? "(activo)" : "(no)");
        printf("  5. Formacion: %s\n", f->usar_formacion ? f->formacion : "(no)");
        printf("  0. Aplicar y Volver\n");

        opcion = input_int("Opcion: ");
        switch (opcion)
        {
        case 1:
            input_string("Fecha desde (YYYY-MM-DD): ", f->fecha_desde, sizeof(f->fecha_desde));
            f->usar_fecha_desde = 1;
            break;
        case 2:
            input_string("Fecha hasta (YYYY-MM-DD): ", f->fecha_hasta, sizeof(f->fecha_hasta));
            f->usar_fecha_hasta = 1;
            break;
        case 3:
            f->resultado = input_int("Resultado (1=Victoria, 2=Empate, 3=Derrota): ");
            f->usar_resultado = (f->resultado >= 1 && f->resultado <= 3) ? 1 : 0;
            break;
        case 4:
            f->equipo_id = input_int("ID del equipo: ");
            f->usar_equipo = (f->equipo_id > 0) ? 1 : 0;
            break;
        case 5:
            input_string("Formacion (ej: 4-4-2): ", f->formacion, sizeof(f->formacion));
            f->usar_formacion = (f->formacion[0] != '\0') ? 1 : 0;
            break;
        }
    }
    while (opcion != 0);
}

int aplicar_filtros_partidos(FiltrosBusqueda *f)
{
    char sql[2048];
    char condiciones[8][256];
    int num_cond = 0;

    snprintf(sql, sizeof(sql), "SELECT p.id, p.fecha_hora, p.goles, p.asistencias, "
             "p.resultado, p.clima, p.dia, can.nombre "
             "FROM partido p LEFT JOIN cancha can ON p.cancha_id = can.id WHERE 1=1");

    if (f->usar_fecha_desde && f->fecha_desde[0])
    {
        snprintf(condiciones[num_cond], sizeof(condiciones[num_cond]), " AND p.fecha_hora >= ?");
        num_cond++;
    }
    if (f->usar_fecha_hasta && f->fecha_hasta[0])
    {
        snprintf(condiciones[num_cond], sizeof(condiciones[num_cond]), " AND p.fecha_hora <= ?");
        num_cond++;
    }
    if (f->usar_resultado)
    {
        snprintf(condiciones[num_cond], sizeof(condiciones[num_cond]), " AND p.resultado = ?");
        num_cond++;
    }
    if (f->usar_equipo && f->equipo_id > 0)
    {
        snprintf(condiciones[num_cond], sizeof(condiciones[num_cond]), " AND p.equipo_id = ?");
        num_cond++;
    }
    if (f->usar_formacion && f->formacion[0])
    {
        snprintf(condiciones[num_cond], sizeof(condiciones[num_cond]), " AND p.formacion = ?");
        num_cond++;
    }

    for (int i = 0; i < num_cond; i++)
        strcat_s(sql, sizeof(sql), condiciones[i]);

    strcat_s(sql, sizeof(sql), " ORDER BY p.fecha_hora DESC LIMIT 100");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error en consulta.\n");
        return 0;
    }

    int param_idx = 1;
    if (f->usar_fecha_desde && f->fecha_desde[0])
        sqlite3_bind_text(stmt, param_idx++, f->fecha_desde, -1, SQLITE_STATIC);
    if (f->usar_fecha_hasta && f->fecha_hasta[0])
        sqlite3_bind_text(stmt, param_idx++, f->fecha_hasta, -1, SQLITE_STATIC);
    if (f->usar_resultado)
        sqlite3_bind_int(stmt, param_idx++, f->resultado);
    if (f->usar_equipo && f->equipo_id > 0)
        sqlite3_bind_int(stmt, param_idx++, f->equipo_id);
    if (f->usar_formacion && f->formacion[0])
        sqlite3_bind_text(stmt, param_idx++, f->formacion, -1, SQLITE_STATIC);

    mostrar_pantalla("RESULTADOS DE BUSQUEDA");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count++;
        printf("  %d. %s | G:%d A:%d | %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               resultado_to_text(sqlite3_column_int(stmt, 4)));
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        mostrar_no_hay_registros("partidos con esos filtros");
    else
        printf("\nTotal: %d partidos encontrados.\n", count);

    pause_console();
    return count;
}

static void buscar_con_filtros(void)
{
    FiltrosBusqueda f;
    inicializar_filtros(&f);
    menu_configurar_filtros(&f);
    aplicar_filtros_partidos(&f);
}

static void buscar_por_estadisticas(void)
{
    mostrar_pantalla("ESTADISTICAS FILTRADAS");

    FiltrosBusqueda f;
    inicializar_filtros(&f);

    f.usar_resultado = 1;
    f.resultado = input_int("Resultado (1=V, 2=E, 3=D): ");

    char fecha_desde[32];
    input_string("Fecha desde (YYYY-MM-DD, dejar vacio para todas): ", fecha_desde, sizeof(fecha_desde));
    if (fecha_desde[0])
    {
        f.usar_fecha_desde = 1;
        strcpy_s(f.fecha_desde, sizeof(f.fecha_desde), fecha_desde);
    }

    int total = aplicar_filtros_partidos(&f);
    if (total > 0)
    {
        printf("\nResumen:\n");
        printf("  Total partidos: %d\n", total);
    }
}

void menu_filtros_avanzados(void)
{
    MenuItem items[] =
    {
        {1, "Busqueda por Filtros", buscar_con_filtros},
        {2, "Estadisticas Filtradas", buscar_por_estadisticas},
        {0, "Volver", NULL}
    };
    ejecutar_menu("FILTROS AVANZADOS", items, 3);
}
