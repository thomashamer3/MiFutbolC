# Bloque A — Rendimiento y seguridad de memoria (MiFutbolC)

**Rama:** `perf/bloque-a-rendimiento`
**Estado:** en progreso
**Fecha inicio:** 2026-09-21

## Objetivo

Reducir el coste de CPU, I/O y memoria en las rutas verificadas como problemáticas,
sin cambiar la arquitectura ni los contratos públicos de los módulos.

## Problema

Auditoría verificada `archivo:línea` sobre el código actual. Los cuatro problemas
reales confirmados son:

1. **Cuatro listados cargan y renderizan toda la tabla.** `financiamiento.c:2060`,
   `lesion.c:228`, `camiseta.c:1284` y `cancha.c:1683` no usan `LIMIT`. Solo
   `partido.c:1011` pagina. Con miles de filas la UI se vuelve inusable y el
   consumo de memoria crece con el volumen.
2. **`torneo.c` no tiene ninguna transacción.** `ingresar_resultado`
   (`torneo.c:1359-1361`) ejecuta una `UPDATE` más tres funciones de agregación
   (posiciones de 2 equipos, estadísticas de jugadores, fase) en autocommit: cada
   sentencia paga su propia escritura de journal. Lo mismo en `generar_fixture`
   (`torneo.c:1222-1232`): un `DELETE` y N inserciones de fixture sueltas.
3. **`lang_set()` reserva ~1.25 MB en la pila.** `lang.c:264`
   `LangPair backup[LANG_MAX_KEYS]` = 2048 × 640 B = 1.310.720 B. Se ejecuta en
   cada arranque (`settings.c:344`) y en cada cambio de idioma. El proyecto no
   define `-Wl,--stack`, así que ese frame consume más de la mitad de la pila
   por defecto de Windows.
4. **Dos defectos de memoria en `export_common.c`.** Doble `sqlite3_finalize`
   sobre el mismo statement en la ruta de fallo (`export_common.c:23` y
   `export_common.c:34`), y fuga del root cJSON cuando `fopen_s` falla
   (`export_common.c:70-75`), porque el `write_footer` que lo libera nunca se
   ejecuta.

## Fuera de alcance (verificado como ya correcto — NO tocar)

- PRAGMAs (`db.c:644-651`) ya incluyen WAL, `synchronous=NORMAL`,
  `temp_store=MEMORY`, `cache_size=-32768`, `mmap_size=268435456`,
  `journal_size_limit`, `cache_spill=OFF`, `automatic_index=ON` + `PRAGMA optimize`.
- ~54 `CREATE INDEX` presentes, incluido `mensual_resumen(temporada_id, mes_anio)`,
  `camiseta(activa)`, `cancha(activa)`, `temporada(estado, anio)`.
- `export_calendario.c:170` ya usa `malloc` (no hay stack overflow).
- `save_equipo_to_db` (`equipo.c:1286-1328`) **ya** envuelve la creación de equipo
  y sus jugadores en `BEGIN IMMEDIATE`/`COMMIT`. Añadir un `BEGIN` dentro de
  `insert_jugadores_for_equipo` sería un error de anidamiento.
- Exports batch (`export_common.c:58-102`) ya usan 1 `prepare` + `sqlite3_reset`.

## Restricciones

- No cambiar firmas públicas: `tests/*.c` verifican los punteros de función
  (`TEST_ASSERT_NOT_NULL(listar_lesiones)`, etc.).
- Las cabeceras públicas de `utils.h` deben conservar estilo Doxygen y orden de
  includes del proyecto.
- Comentarios en español (convención del proyecto).
- Prohibido introducir un `BEGIN` anidado: `torneo.c` no tiene transacciones hoy,
  así que envolver allí es seguro.
- Los bucles de paginación DEBEN salir con `-1`: `input_int` devuelve `-1` tras
  dos EOF consecutivos (`utils.c:884-891`) y `ejecutar_menu` trata `-1` como
  salida preventiva (`menu.c:508-515`). Ignorarlo produce bucles infinitos en
  entornos sin stdin.
- Sin VLA, sin `sprintf`, sin `system()` (reglas del proyecto).

## Modo TDD

**Resuelto: OFF.** No existe configuración de TDD en el proyecto ni elección
explícita del usuario. La presencia de Unity y de `tests/` NO habilita TDD.
Se aplican comprobaciones funcionales ordinarias:

- Build: `cmake --preset mingw-debug -DMIFUTBOLC_BUILD_TESTS=ON && cmake --build --preset mingw-debug`
- Tests: `ctest --preset mingw-debug --output-on-failure`

## Estrategia de entrega

`ask-on-risk`. Previsión de líneas autoradas: **~360** (< 400), por lo que NO se
divide en PRs encadenados. Una sola rama `perf/bloque-a-rendimiento`.

## Tareas

### [ ] A1 — Transacciones en las escrituras batch de torneo
**Archivos:** `torneo.c`
**Cambio:**
- `ingresar_resultado`: envolver la `UPDATE` de `partido_torneo` y las tres
  llamadas de agregación en `BEGIN IMMEDIATE` / `COMMIT`, con `ROLLBACK` ante
  cualquier fallo. Las tres funciones de agregación tienen un único llamador
  (`torneo.c:1359-1361`), así que la transacción no se anida.
- `generar_fixture`: envolver el `DELETE FROM partido_torneo` y
  `generate_round_robin_fixture` en una sola transacción, con `ROLLBACK` en error.
**Criterios de aceptación:**
- Un fallo en la `UPDATE` no deja posiciones/estadísticas parcialmente aplicadas.
- El número de transacciones por resultado registrado baja de N+1 a 1.
- `grep BEGIN torneo.c` deja de estar vacío y nunca hay dos `BEGIN` en el mismo
  camino de ejecución.
**Verificación:** build Debug + `ctest`; `EXPLAIN` no aplica. Revisión estructural
del balance `BEGIN`/`COMMIT`/`ROLLBACK` en cada rama de salida.

### [ ] A2 — Paginación en los cuatro listados sin `LIMIT`
**Archivos:** `utils.c`, `utils.h`, `financiamiento.c`, `lesion.c`, `camiseta.c`, `cancha.c`
**Cambio:**
- Nuevo helper genérico en `utils.c`/`utils.h`:
  `typedef void (*ListadoFilaFn)(sqlite3_stmt *stmt, void *ctx);` y
  `void listado_paginado(const char *sql_conteo, const char *sql_pagina, ListadoFilaFn render_fila, void *ctx);`
  El helper ejecuta el conteo, calcula páginas, renderiza la página actual y
  ofrece `1) Anterior 2) Siguiente 3) Ir a pagina 0) Volver`, replicando el patrón
  ya existente en `partido.c:3351-3367` y `partido.c:994-1052`. Sale con `0` y
  con `-1`. `sql_pagina` debe terminar en `LIMIT ? OFFSET ?` y no tener otros
  binds (los cuatro listados no tienen filtros).
- Tamaño de página constante (20 filas), sin persistencia: no se crean tablas de
  configuración ni migraciones de esquema.
- `financiamiento.c`: el resumen general pasa de acumularse en el bucle a
  calcularse con UNA consulta agregada (`COUNT(*)`, `SUM(CASE WHEN tipo = ? ...)`),
  porque con paginación los acumuladores del bucle solo verían una página.
- `lesion.c`, `camiseta.c`, `cancha.c`: mover el cuerpo del bucle a una función
  de render y sustituir el bucle por el helper. La salida en pantalla debe ser
  byte-idéntica por fila.
**Criterios de aceptación:**
- Los cuatro listados emiten `LIMIT ? OFFSET ?`.
- El resumen de financiamiento conserva totales y balance exactos comparados con
  el comportamiento anterior.
- Cada fila se renderiza igual que antes (mismos campos, mismo separador).
- El bucle termina con `0`, con `-1` y con cualquier entrada inválida.
**Verificación:** build Debug + `ctest`; revisión de que las 4 queries terminan en
`LIMIT ? OFFSET ?` y que el render por fila no cambió.

### [ ] A3 — `lang_set()` sin 1.25 MB de pila
**Archivos:** `lang.c`
**Cambio:** sustituir `LangPair backup[LANG_MAX_KEYS]` por una copia en heap con
`malloc`, comprobando el retorno; si `malloc` falla, no se pierde el estado: se
devuelve sin cambiar de idioma. Liberar en todas las ramas.
**Criterios de aceptación:**
- No queda ningún array grande en pila en `lang_set`.
- Si `try_load` falla, el idioma anterior queda intacto (mismo comportamiento que hoy).
- La ruta de fallo de `malloc` no deja el idioma en estado parcial.
**Verificación:** build Debug + `ctest`; ASan activo en Debug.

### [ ] A4 — Defectos de memoria en `export_common.c`
**Archivos:** `export_common.c`
**Cambio:**
- Eliminar el doble `sqlite3_finalize` en la ruta de fallo de
  `export_generic_rows` (hoy finaliza en `open_export_file` y otra vez en el
  llamador).
- Liberar el root cJSON cuando no se puede abrir el archivo en
  `export_all_formats`, mediante un `write_footer` que ya existe o liberación
  explícita del contexto antes del `continue`.
**Criterios de aceptación:**
- Un `fopen_s` fallido no produce doble finalize ni deja el root cJSON vivo.
- ASan no reporta errores en la ruta de fallo forzada.
**Verificación:** build Debug + `ctest`; revisión de la ruta de fallo.

## Progreso

| Tarea | Estado | Commit | Evidencia |
|---|---|---|---|
| A1 | pendiente | — | — |
| A2 | pendiente | — | — |
| A3 | pendiente | — | — |
| A4 | pendiente | — | — |

## Registro de decisiones

- **A1: el `BEGIN` va en el llamador, no en el bucle.** `actualizar_tabla_posiciones`,
  `actualizar_estadisticas_jugadores` y `actualizar_fase_torneo` son públicas pero
  tienen un único llamador; envolver en `ingresar_resultado` da atomicidad de
  operación completa y evita anidamiento.
- **A2: helper compartido en `utils.c` en lugar de duplicar 4 veces.** El patrón ya
  existe en `partido.c`; centralizarlo sigue la regla del proyecto de mover lógica
  genérica a `utils`. No se introduce capa nueva de acceso a datos.
- **A2: sin persistir el tamaño de página.** Añadir 4 tablas de configuración +
  migraciones no aporta rendimiento y arriesga integridad de la BD.
- **A2: `listar_camisetas()`, `listar_canchas()` y `listar_lesiones()` también se
  usan como selectores** desde otros flujos (`crear_lesion` llama
  `listar_camisetas`). Al paginar, esos flujos ganan navegación por páginas y el
  usuario debe pulsar `0` para continuar. Es un cambio de comportamiento visible
  y aceptado como parte del alcance autorizado.
- **Descartado: transacción en `insert_jugadores_for_equipo`.** Su llamador ya
  abre `BEGIN IMMEDIATE` (`equipo.c:1289`). Modificarlo habría roto el guardado de
  equipos con un `BEGIN` anidado.
