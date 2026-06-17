#include "db.h"
#include "export.h"
#include "export_bienestar.h"
#include "export_camisetas.h"
#include "export_dashboard.h"
#include "export_equipo.h"
#include "export_estadisticas.h"
#include "export_estadisticas_generales.h"
#include "export_lesiones.h"
#include "export_partidos.h"
#include "export_temporada.h"
#include "export_torneo.h"
#include "utils.h"
#include <stdio.h>
#include <time.h>

static double diff_sec(clock_t start, clock_t end) {
  return (double)(end - start) / (double)CLOCKS_PER_SEC;
}

typedef struct {
  const char *name;
  void (*func)(void);
  int skip_if_empty;
  const char *check_table;
} ExportBench;

int main(void) {
  printf("=== BENCHMARK DE EXPORTACION ===\n\n");

  if (!db_init()) {
    printf("ERROR: No se pudo inicializar la base de datos.\n");
    return 1;
  }
  printf("Base de datos inicializada correctamente.\n\n");

  double total = 0.0;
  int executed = 0;
  int skipped = 0;

  ExportBench benchmarks[] = {
      {"Camisetas", &exportar_camisetas_all, 1, "camiseta"},
      {"Partidos", &exportar_partidos_all, 1, "partido"},
      {"Lesiones", &exportar_lesiones_all, 1, "lesion"},
      {"Estadisticas", &exportar_estadisticas_all, 1, "partido"},
      {"Estadisticas por anio", &exportar_estadisticas_por_anio_all, 1,
       "partido"},
      {"Estadisticas generales", &exportar_estadisticas_generales_all, 1,
       "partido"},
      {"Estadisticas por mes", &exportar_estadisticas_por_mes_all, 1,
       "partido"},
      {"Temporadas", &exportar_temporadas_all, 1, "temporada"},
      {"Torneos", &exportar_torneos_all, 1, "torneo"},
      {"Bienestar", &exportar_bienestar_all, 1, "bienestar_sesion_mental"},
      {"Dashboard", &exportar_dashboard_all, 1, "partido"},
      {"Equipos", &exportar_equipos_all, 1, "equipo"},
      {"Analisis", &exportar_analisis_all, 1, "partido"},
  };
  int n = sizeof(benchmarks) / sizeof(benchmarks[0]);

  printf("%-35s %12s  %s\n", "Modulo", "Tiempo", "Estado");
  printf("%-35s %12s  %s\n", "-----", "------", "------");

  for (int i = 0; i < n; i++) {
    if (benchmarks[i].skip_if_empty &&
        !has_records(benchmarks[i].check_table)) {
      printf("%-35s %12s  %s\n", benchmarks[i].name, "-", "SIN DATOS");
      skipped++;
      continue;
    }

    clock_t start = clock();
    benchmarks[i].func();
    clock_t end = clock();

    double elapsed = diff_sec(start, end);
    total += elapsed;
    executed++;

    const char *status;
    if (elapsed < 0.05)
        status = "RAPIDO";
    else if (elapsed < 0.2)
        status = "OK";
    else if (elapsed < 1.0)
        status = "LENTO";
    else
        status = "MUY LENTO";
    printf("%-35s %9.4fs  %s\n", benchmarks[i].name, elapsed, status);
  }

  printf("\n%s\n", "---");
  printf("%-35s %9.4fs  (%d ejecutados, %d sin datos)\n", "TOTAL", total,
         executed, skipped);

  db_close();
  return 0;
}
