# Pruebas unitarias (Unity minimal)

Este directorio contiene un *runner* de pruebas unitarias estilo Unity y un conjunto de pruebas de
humo por módulo: `utils`, `camiseta`, `cancha`, `lesion`, `partido`, `temporada`, `torneo`, `equipo`,
`botin`, `estadisticas_canchas`, `resumen_compartible` y `openmeteo`, más ocho pruebas sobre una base
SQLite real: `db_integridad` (backup, restauración y `user_version`), `clima_diferido` (cola de clima
pendiente, tope de intentos y aplicación de una respuesta ya descargada), `import` (partidos desde
CSV/TXT/JSON), `sync` (backfill de uuid, cola de sincronización y borrado local, sin red) y
`estadisticas_clima` (rendimiento por lluvia, viento y cielo reales de Open-Meteo, más la opción 21 del
menú de rendimiento), `resumen_general` (datos del resumen compartible: efectividad, rachas, récords,
forma reciente en orden por fecha, top 3, años y escape de HTML/Markdown) y `resumen_anio` (Resumen
Anual: filtro por año con fechas mezcladas, mes a mes, mejor mes, tendencia, comparación con el año
anterior con y sin datos, lesiones, clima real por año y los escritores TXT, HTML y PDF) y
`partido_modificar` (Modificar Partido: signo de la diferencia de gol, UPDATE con chequeo, reajuste al
cambiar el resultado, sincronización de la transacción de Finanzas, reencolado del clima real, borrado
del partido con su gasto, EOF al pedir enteros y el flujo interactivo de `modificar_partido()` con
`stdin`/`stdout` redirigidos a archivos y un guion de entradas con las pausas exactas).

## Ejecutar en Windows (CMake + Ninja)

1. Configurar con las pruebas activadas:

   ```bash
   cmake --preset mingw-debug -DMIFUTBOLC_BUILD_TESTS=ON
   ```

2. Compilar (el preset de build es `build-debug`, no `mingw-debug`):

   ```bash
   cmake --build --preset build-debug
   ```

3. Ejecutar las pruebas:

   ```bash
   ctest --test-dir build/debug --output-on-failure
   ```

El proyecto no define `testPresets`, así que `ctest --preset ...` no es válido: hay que usar
`--test-dir build/debug` como arriba.

## Ejecutar en Linux/macOS

```bash
cmake --preset linux-debug -DMIFUTBOLC_BUILD_TESTS=ON
cmake --build --preset build-linux-debug
ctest --test-dir build/linux-debug --output-on-failure
```

## Agregar una prueba

Los archivos bajo `tests/` se descubren automáticamente (`file(GLOB_RECURSE ...)` en `CMakeLists.txt`),
así que basta con crear `tests/test_<modulo>.c` con su `main()` y los `RUN_TEST(...)`. Las pruebas de
menú usan el patrón de captura:

```c
MenuTestCapture capture = {0};
menu_test_set_capture(&capture);
mi_menu();
menu_test_set_capture(NULL);
TEST_ASSERT_NOT_NULL(capture.titulo);
TEST_ASSERT_TRUE(capture.last_item.accion == NULL);   /* el último ítem siempre es "Volver" */
```

## Notas

- Se usa un *subset* minimal de Unity incluido en `tests/unity`.
- Las pruebas de módulo validan funciones sin dependencia de I/O ni base de datos: comprueban que los
  símbolos públicos existan y que cada menú se construya con su sentinela de salida.
- `test_db_integridad`, `test_clima_diferido`, `test_import`, `test_sync`, `test_estadisticas_clima`,
  `test_resumen_general`, `test_resumen_anio` y `test_partido_modificar` sí usan SQLite: cada una trabaja con un perfil propio
  (`mifutbol_test_integridad.db`, `mifutbol_test_clima.db`, `mifutbol_test_import.db`,
  `mifutbol_test_sync.db`, `mifutbol_test_estadisticas_clima.db`, `mifutbol_test_resumen_general.db`,
  `mifutbol_test_resumen_anio.db`, `mifutbol_test_partido_mod.db` en el directorio de datos), recrea la base en cada
  caso y borra sus archivos (base, `-wal`, `-shm`, log y archivos de entrada) al terminar, así que no
  toca los perfiles reales ni la carpeta de importaciones.
- Ninguna prueba hace llamadas de red: `test_clima_diferido` aplica un JSON de Open-Meteo escrito en un
  archivo en lugar de lanzar curl, y `test_sync` solo prueba la parte local.
- Los internals que estas pruebas necesitan se exponen con ganchos `*_test_*` declarados bajo
  `#ifdef UNIT_TEST` en `clima_diferido.h`, `import.h`, `sync.h` y `partido.h` (el mismo patrón que `menu.h`); el
  core se compila con `UNIT_TEST` solo cuando `MIFUTBOLC_BUILD_TESTS=ON`.
- El Unity incluido no tiene precisión double (`TEST_ASSERT_DOUBLE_WITHIN` falla con "Double Precision
  Disabled"); `test_estadisticas_clima`, `test_resumen_general` y `test_resumen_anio` comparan
  promedios con su propio `ASSERT_CERCA` (con `fabs`).
