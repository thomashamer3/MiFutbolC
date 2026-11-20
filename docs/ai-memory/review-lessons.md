# Lecciones de revisión

Lo que salió de revisiones anteriores y vale la pena revisar siempre. Se completa con la sección "Para recordar" de cada revisión, cuando el usuario la acepta.

- Un comentario o mensaje que dice "no modifica nada" no es una garantía: comprobarlo en el código (en los agentes de OpenCode, los permisos reales se ven con `opencode debug agents`).
- Antes de reportar una condición de carrera, buscar si ya hay una protección: `clima_diferido` descarta el resultado si cambiaron la fecha o la cancha (`partido_sigue_igual`).
- Que un test pase no prueba nada si no falla sin el arreglo: forzar el bug una vez para confirmarlo.
- **Falso positivo ya visto (2026-11-20):** en SQLite, `sqlite3_changes()` cuenta las filas que coinciden con el `WHERE` aunque el valor no cambie (`UPDATE t SET g=5` sobre un `g` que ya vale 5 da 1). Eso es MySQL, no SQLite. Un 0 en un UPDATE por id significa que la fila no existe.
