# Pruebas unitarias (Unity minimal)

Este directorio contiene un *runner* de pruebas unitarias estilo Unity y un conjunto de pruebas de
humo por módulo: `utils`, `camiseta`, `cancha`, `lesion`, `partido`, `temporada`, `torneo`, `equipo`
y `botin`.

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
- Las pruebas actuales validan funciones sin dependencia de I/O ni base de datos: comprueban que los
  símbolos públicos existan y que cada menú se construya con su sentinela de salida.
