# AGENTS.md — MiFutbolC

Reglas para cualquier agente (OpenCode, Claude Code, CI) que trabaje en este repositorio.
Aplicación de consola en C11 con SQLite (amalgamation incluida), Windows (MinGW) y Linux.

## Compilar y probar

Windows (MinGW + Ninja):

```bash
cmake --preset mingw-release -DMIFUTBOLC_BUILD_TESTS=ON
cmake --build --preset build-release
ctest --test-dir build/release --output-on-failure
```

Linux:

```bash
cmake --preset linux-release -DMIFUTBOLC_BUILD_TESTS=ON
cmake --build --preset build-linux-release
ctest --test-dir build/linux-release --output-on-failure
```

- No hay `testPresets`: usar `ctest --test-dir ...`, no `ctest --preset`.
- Cada `tests/test_*.c` es un ejecutable propio (CMake los descubre solo).
- Los tests que usan una base real incluyen `tests/test_db_fixture.h`: perfil de prueba propio y la base se borra al terminar. Ningún test toca los datos del usuario.
- Los ganchos de prueba van solo dentro de `#ifdef UNIT_TEST`.

## Convenciones de código

- Comentarios, mensajes y textos de la interfaz en español.
- Llaves en línea propia y 4 espacios. Comentar solo el porqué.
- Los archivos `.c` y `.h` usan finales de línea CRLF: no convertirlos (cuidado con `sed -i` en Git Bash).
- SQLite: `preparar_stmt` / `db_prepare_stmt`, enlazar texto con `DB_TRANSIENT`, revisar siempre el resultado de `sqlite3_step` y finalizar el statement en todos los caminos.
- No cambiar `input_int` (lo usa todo el proyecto). Todo bucle que lea entrada debe terminar con EOF (`feof(stdin)`).
- Ver `docs/ai-memory/` antes de cambiar código: estándares, lecciones de revisión y bugs que se repiten.

## Git y PRs

- Trabajar siempre en una rama; nunca commitear ni hacer push a `master`. El merge lo hace el usuario.
- Nunca `git push --force` ni `gh pr merge`.
- `git add` con archivos explícitos. Nunca agregar `MiFutbolC.layout` ni `.cache/` (cambios locales del IDE).
- Commits Conventional (`feat(partido): ...`, `fix(finanzas): ...`), en español.
- Cada cambio terminado lleva su entrada en `CHANGELOG.md`, sección "Sin publicar" (Agregado / Cambiado / Corregido), escrita para el usuario de la app.

## Memoria

- En el repo (compartida, la lee CI): este archivo, `docs/ai-memory/` y `CHANGELOG.md`.
- Local (privada): Engram, proyecto `mifutbolc`. En CI no hay Engram.
- Las revisiones terminan con "Para recordar". Lo que el usuario acepte va a Engram; si se vuelve una regla, se agrega a `docs/ai-memory/` o a este archivo.
