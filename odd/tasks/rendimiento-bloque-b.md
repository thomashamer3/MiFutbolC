# Bloque B — Coste de exportación

**Rama:** `perf/bloque-a-rendimiento` (misma rama; RDD está `off` en este clon por decisión del usuario)
**Depende de:** Bloque A (commits `a27ff10`, `e781a24`, `3d79003`, `fccf6fb`)

## Objetivo

Reducir el trabajo redundante de "exportar todo": E/S, consultas y serialización repetidas.

## Verificado en el código actual

1. **`has_records()` ejecuta `SELECT COUNT(*)` completo, y se llama 36 veces.**
   `utils.c:3833`: `snprintf(sql, "SELECT COUNT(*) FROM %s", table_name)` + prepare + step.
   Recorre la tabla entera (o su índice menor) para responder "¿hay filas?". 36 puntos de
   llamada en 6 archivos (`export_estadisticas_generales.c` 11, `export_estadisticas.c` 11,
   `export.c` 7, `utils.c` 4, `export_lesiones.c` 2, `export_lesiones_mejorado.c` 1), más los
   `exportar_archivo_si_hay_registros` (`export.c:43`) que lo invocan por dentro.
   En una corrida de "exportar todo" son decenas de conteos completos.

2. **`write_partido_json` serializa un cJSON POR FILA.** `export_partidos.c:64-84`:
   por cada fila crea el objeto, `cJSON_PrintUnformatted(item)`, `free`, `cJSON_Delete`.
   Con N partidos son N recorridos de árbol y N asignaciones de string.

3. **El calendario se recalcula 4 veces.** `exportar_calendario_base`
   (`export_calendario.c:167`) hace `malloc` + `cargar_eventos` + escribir + `free`, y lo
   llaman los 4 formatos (`:258-276`). `cargar_eventos` (`:157`) relee
   `Importaciones/recordatorios.json` y reconsulta `partido` en cada una de las 4 pasadas.

4. **El PDF relee 20 TXT recién escritos.** `export_pdf.c:760` `procesar_archivo_txt` abre
   con `fopen_s(..., "r")` y se llama en bucle en `:989` sobre los archivos que la propia
   exportación acaba de generar. Doble E/S.

## Restricciones

- No cambiar firmas públicas: los 4 `exportar_calendario_*` y `has_records` se usan en menús y
  en `export.c`.
- No cambiar la semántica observable de los archivos exportados, salvo **espacios en blanco**
  en JSON (documentado abajo).
- Comentarios en español; sin VLA; sin `sprintf`.

## Tareas

### [ ] B1 — `has_records` en O(1) en lugar de `COUNT(*)`
**Archivo:** `utils.c`
**Cambio:** `SELECT 1 FROM <tabla> LIMIT 1` en vez de `SELECT COUNT(*) FROM <tabla>`.
Se detiene en la primera fila. Si la tabla no existe, `sqlite3_step` falla y devuelve 0, igual
que antes. Los 37 llamadores se benefician sin cambiar ni una firma.
**Criterios:** mismo resultado booleano; sin cambios en los llamadores; sin `COUNT(*)`.

### [ ] B2 — Una sola serialización JSON en `write_partido_json`
**Archivo:** `export_partidos.c`
**Cambio:** construir un único array cJSON con todas las filas y serializar **una vez** con
`cJSON_PrintUnformatted(root)`, en vez de imprimir objeto por objeto.
**Criterios:** N partidos → 1 serialización en lugar de N; el JSON sigue siendo válido y con
los mismos datos.
**Desviación conocida y aceptada:** el archivo pasa de tener salto de línea por elemento a una
sola línea compacta. Es solo espacio en blanco; sigue siendo el mismo JSON.

### [ ] B3 — Cargar el calendario una vez para sus 4 formatos
**Archivos:** `export_calendario.c`, `export_all.c`
**Cambio:** añadir un camino que cargue los eventos **una vez** y escriba los 4 archivos,
y usarlo desde la orquestación de "exportar todo". Los 4 `exportar_calendario_*` públicos
siguen existiendo y funcionando por separado.
**Criterios:** en la corrida completa se lee `recordatorios.json` 1 vez y se consulta `partido`
1 vez, no 4.

### [ ] B4 — Diferido: doble E/S del PDF
`export_pdf.c` escribe ~20 TXT y luego los reabre y reparsea para armar el PDF. Corregirlo
implica reestructurar ese módulo (>1000 líneas) para construir el PDF desde datos en memoria.
**No se aborda en este bloque**: alcance y riesgo de su propia tarea.

## Verificación

- `gcc -std=c11 -Wall -Wextra -fsyntax-only -I. -include compat_port.h <archivo>.c` en cada
  archivo tocado.
- Revisión estructural de que ningún llamador cambia de semántica.
- El build completo y `ctest` NO se ejecutan salvo petición explícita (el usuario los abortó
  dos veces en esta sesión).

## Progreso

| Tarea | Estado | Commit |
|---|---|---|
| B1 | ✅ hecho | `eec23b7` (+ comentario `27ee207`) |
| B2 | ❌ **revertido** — optimización equivocada | `88e2df4` revertido en `a0f4d7b` |
| B3 | ✅ hecho | `2fc73cd` |
| B4 | diferido | — |

## B2 revertido: la optimización era equivocada

B2 acumulaba un array cJSON con todas las filas y lo serializaba una sola vez. Un verificador
independiente demostró que eso **cambia CPU por memoria pico sin límite**: se retiene el
listado completo *más* la cadena serializada entera en RAM, cuando el código original
serializaba fila a fila con memoria O(1). Es exactamente el problema que la auditoría original
le criticaba a los exports JSON.

Y el trabajo total de serialización es el **mismo** en ambos casos (O(tamaño total)); lo único
que ahorraba el batch era el overhead por llamada. El trade era malo. Revertido, y el código
queda con un comentario que explica por qué NO debe cambiarse a batch otra vez.

## Cambio deliberadamente NO hecho

`preparar_consulta_con_verificacion` (`utils.c:3782`) también usa `SELECT COUNT(*)`, pero
**expone el conteo** por el parámetro de salida `*count`, y sus 5 llamadores
(`export_camisetas.c`, `export_camisetas_mejorado.c`, `export_equipo.c`, `export_temporada.c`,
`export_torneo.c`) lo reciben. Sustituirlo por `LIMIT 1` cambiaría el significado de un
parámetro de salida público por un beneficio marginal: se invoca una vez por formato, no en el
camino caliente de 36 llamadas que arregló B1.


## Verificación ejecutada

`gcc -std=c11 -Wall -Wextra -fsyntax-only -I. -include compat_port.h` → exit 0 y sin salida
en `utils.c`, `export_partidos.c`, `export_calendario.c` y `export_all.c`.
El símbolo `exportar_calendario_all` queda coherente: declarado en `export_calendario.h:35`,
definido en `export_calendario.c:287`, llamado en `export_all.c:266`.
El build completo y `ctest` siguen sin ejecutarse.

