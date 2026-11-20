# Estándares de código

Complementa la sección "Convenciones de código" de `AGENTS.md`.

- Reusar los helpers existentes antes de escribir uno nuevo: `preparar_stmt`, `input_int`, `input_date`, `confirmar`, `pause_console`, `mostrar_alerta_operacion`, `ejecutar_menu`, `convert_display_date_to_storage`.
- Las fechas se guardan como `YYYY-MM-DD HH:MM` y se muestran como `dd/mm/yyyy hh:mm`.
- Las funciones con lógica no interactiva se separan de la interfaz para poder probarlas (por ejemplo `partido_sincronizar_transaccion` y `partido_reencolar_clima`).
- Un atajo con un límite conocido lleva un comentario `shortcut: <el límite>, <cuándo mejorarlo>`.
- Para probar flujos interactivos: redirigir `stdin`/`stdout` con `_dup`/`_dup2` (ver `correr_con_guion` en `tests/test_partido_modificar.c`). Cada pausa consume una línea del guion, así que una pausa de más o de menos desfasa el guion y el test falla.
