# Lecciones de revisión

Lo que salió de revisiones anteriores y vale la pena revisar siempre. Se completa con la sección "Para recordar" de cada revisión, cuando el usuario la acepta.

- Un comentario o mensaje que dice "no modifica nada" no es una garantía: comprobarlo en el código (en los agentes de OpenCode, los permisos reales se ven con `opencode debug agents`).
- **Falsos positivos ya vistos sobre `claude-code-action` (PR #4):** `id-token: write` sí se usa (la action cambia el token OIDC por el de su GitHub App), y `Bash(cmd:*)` en `--allowedTools` no deja pasar `cmd; otro` (Claude Code valida cada parte de un comando compuesto).
- Varios jobs que comentan con `github.token` comparten el autor `github-actions[bot]`: `gh pr comment --edit-last` puede editar el comentario de otro job. Editar el propio buscándolo por su título.
- En CI (`opencode run`) un permiso en `ask` no tiene quien lo conteste y cancela toda la corrida con exit 1. Todo agente que corre en CI necesita `allow` o `deny` explícito para cada herramienta que pueda intentar usar (`websearch`, `question`...).
- Antes de reportar una condición de carrera, buscar si ya hay una protección: `clima_diferido` descarta el resultado si cambiaron la fecha o la cancha (`partido_sigue_igual`).
- Que un test pase no prueba nada si no falla sin el arreglo: forzar el bug una vez para confirmarlo.
- **Falso positivo ya visto (2026-11-20):** en SQLite, `sqlite3_changes()` cuenta las filas que coinciden con el `WHERE` aunque el valor no cambie (`UPDATE t SET g=5` sobre un `g` que ya vale 5 da 1). Eso es MySQL, no SQLite. Un 0 en un UPDATE por id significa que la fila no existe.
- Hallazgos de revisión refutados con evidencia (PR #4): el modelo `deepseek/deepseek-v4-pro` sí resuelve en CI sin `opencode.json` (la corrida lo usa); y en OpenCode 2.x gana la última regla de permisos que coincide, así que los `deny` al final sí bloquean.
