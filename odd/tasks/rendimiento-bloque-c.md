# Bloque C — Deuda estructural

**Estado:** cerrado parcialmente. Una tarea entregada, tres descartadas con motivo, una bloqueada.
**Depende de:** Bloques A y B (rama `perf/bloque-a-rendimiento`)

## C4 — ENTREGADO: `qsort` en lugar de bubble sort
**Archivo:** `ranking_amigos.c` · **Commit:** posterior a `27ee207`

El ordenamiento era bubble sort O(n²) sobre `total_entries` (amigos + el usuario local).

`qsort` **no es estable** y el bubble sort sí lo era, así que la sustitución directa habría
cambiado el orden impreso de las entradas con los mismos goles. Por eso:

1. `RankingEntry` pasó de tipo local de función a tipo de archivo (un comparador necesita el
   tipo visible).
2. Se añadió el campo `orden` (índice de inserción), asignado en los dos bucles de llenado.
3. El comparador ordena por `goles` descendente y desempata por `orden` ascendente.

Resultado: O(n log n) con **orden de salida idéntico** al anterior para cualquier entrada,
incluidos los empates.

Nota honesta: con el número típico de amigos el coste O(n²) era pequeño, así que la ganancia
práctica es modesta. El valor real es quitar el patrón y dejar la estabilidad explícita.

## C3 — DESCARTADO, y era un error de mi auditoría
Mi auditoría recomendaba añadir `idx_torneo_nombre`. **Verificado que habría sido un índice
inútil:**

- Ninguna consulta filtra `torneo` por nombre: solo hay `ORDER BY nombre`, `ORDER BY id` y
  `WHERE id = ?` (clave primaria, ya indexada implícitamente). Ver `torneo.c:126`,
  `torneo.c:1019`, `temporada.c:1410`, `carrera.c:2909`, `busqueda.c:519`,
  `notificaciones.c:205`, `export_ods.c:336` y `:721`.
- El helper genérico `obtener_id_por_nombre(tabla, nombre)` (`utils.c:3140`) solo se llama con
  `"camiseta"` (`import.c`).

Un índice sin consulta que lo use solo encarece cada INSERT/UPDATE. **No se añade.**

## C2 — DESCARTADO: la centralización habría empeorado el código
Los fragmentos `SUM(CASE WHEN resultado = ...)` aparecen ~40 veces en 12 archivos, pero **no son
idénticos**: varían en alias de tabla (`resultado` vs `p.resultado`), espaciado (`= 1` vs `=1`),
alias de salida (`AS victorias`, `AS pg`, ninguno) y envoltorio (`SUM` vs `COUNT`, con o sin
`IFNULL`), y en `formaciones.c:100-102` están troceados con continuaciones de línea.

Dos razones para no hacerlo:
1. **Riesgo no verificable sin ejecutar:** son *cadenas SQL*. Una SQL mal formada compila sin
   un solo aviso y solo falla en tiempo de ejecución. Cuarenta sustituciones así, sin poder
   compilar ni ejecutar, es exponer el proyecto a fallos silenciosos.
2. **Contra la legibilidad:** sustituir un literal SQL legible por un macro lo vuelve opaco
   justo para quien tiene que leer la consulta. El objetivo de C era mantenibilidad y esto la
   empeora.

Si algún día se aborda, la vía correcta no es un macro sino una *vista* de SQLite o una función
que devuelva el fragmento con alias parametrizado — y con el build y los tests corriendo.

## C1 — DESCARTADO: 32 archivos de churn por ganancia cosmética
Hay **32 definiciones locales** de `preparar_stmt` en 32 archivos, y con dos convenciones
opuestas de parámetros: `preparar_stmt(const char *sql, sqlite3_stmt **stmt)` (torneo.c,
dashboard.c, partido.c, lesion.c, …) y `preparar_stmt(sqlite3_stmt **stmt, const char *sql)`
(equipo.c, camisetas.c, bienestar.c, …). El mismo nombre con orden invertido.

Unificarlas a `db_prepare_stmt` tocaría 32 archivos y cientos de puntos de llamada para un
cambio **cosmético sin efecto en comportamiento**. Eso supera con creces el presupuesto de
revisión de 400 líneas por PR y sepultaría cualquier cambio funcional en el mismo diff. Los
wrappers son `static` y no filtran nada fuera de su archivo. **No se toca.**

## C5 — BLOQUEADO: partir `partido.c` (6.414 líneas) necesita un build funcional
Mover funciones entre unidades de traducción rompe el **enlazado**: definiciones que faltan,
símbolos duplicados, declaraciones ausentes. `gcc -fsyntax-only` por archivo **no detecta nada de
eso** — solo ve un archivo.

Producir un diff de miles de líneas que "compila por archivo" sin poder comprobar que enlaza y
arranca no es un refactor, es una apuesta. Queda bloqueado hasta que
`cmake --build --preset build-debug` pueda completarse y `ctest --test-dir build/debug` corra.

## Verificación ejecutada en este bloque

`gcc -std=c11 -Wall -Wextra -fsyntax-only -I. -include compat_port.h` → exit 0 y sin salida en
`ranking_amigos.c`. El resto de tareas no produjeron código que verificar.

**El build completo y `ctest` siguen sin ejecutarse** en toda la sesión: el usuario abortó el
build tres veces.
