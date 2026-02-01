@echo off

REM Compile tests with project sources (excluding main.c)
gcc -DUNIT_TEST -Wall -g analisis.c cJSON.c camiseta.c cancha.c db.c estadisticas.c estadisticas_meta.c estadisticas_anio.c estadisticas_generales.c estadisticas_lesiones.c estadisticas_mes.c export.c export_all.c export_all_mejorado.c export_camisetas.c export_camisetas_mejorado.c export_estadisticas.c export_estadisticas_generales.c export_lesiones.c export_lesiones_mejorado.c export_partidos.c export_records_rankings.c import.c lesion.c logros.c menu.c partido.c records_rankings.c sqlite3.c utils.c equipo.c torneo.c temporada.c financiamiento.c settings.c entrenador_ia.c qr.c tests\test_utils.c tests\unity\unity.c -I. -Itests -Itests\unity -o MiFutbolC_tests

REM Check if compilation was successful
if %errorlevel% equ 0 (
    echo Tests compilation successful.
) else (
    echo Tests compilation failed.
)
