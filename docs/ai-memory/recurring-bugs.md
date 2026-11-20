# Bugs que se repiten

Errores que ya aparecieron en MiFutbolC. Revisar cada cambio contra esta lista.

## Entrada de usuario
- **Bucle infinito con EOF.** `input_int` devuelve -1 en EOF o tras 5 intentos; un `while (valor fuera de rango)` que vuelve a llamarlo nunca termina si `stdin` está cerrado. Cortar con `feof(stdin)`. Visto en `pedir_entero_en_rango`, `pedir_entero_minimo`, cancha, camiseta y `listar_partidos`.
- **Rango 0,0 como "sin validar".** En `modificar_campo_partido` dejaba guardar negativos y el -1 de error.
- **`fgets` sin validar** guardado tal cual (la fecha del partido aceptaba cualquier texto).

## SQLite
- **`sqlite3_step` sin revisar:** se informaba "modificado correctamente" aunque el UPDATE fallara. Revisar `SQLITE_DONE` y, en UPDATE/DELETE por id, `sqlite3_changes(db) > 0`.
- **`sqlite3_errmsg` leído después de un ROLLBACK:** el ROLLBACK lo pisa con "not an error". Guardar el mensaje antes.
- **Borrados que tienen que ir juntos** (partido + gasto en Finanzas) van en un SAVEPOINT; los SAVEPOINT anidan y funcionan dentro de una transacción del llamador.

## Datos derivados
- **`memset` + campos que nunca se vuelven a pedir:** "Modificar Todo" ponía en 0 la diferencia de gol.
- **Cambiar un campo sin actualizar los que dependen de él:** resultado → signo de `diferencia_gol`; fecha → `dia` (franja) y `mes_anio`; precio → gasto en `financiamiento`; fecha o cancha → clima real (`partido_reencolar_clima`); cantidad de goles → `goles_detalle`.

## Finanzas
- **Categoría equivocada:** `CANCHAS` vale 5 (no 6). Usar siempre el enum, nunca el número.
- **Buscar la transacción de un partido:** usar `partido_buscar_transaccion` (texto `"(N |..."` y los de respaldo `"Partido ID: N"`). Un `LIKE 'N%'` sin delimitador confunde el partido 1 con el 10.
