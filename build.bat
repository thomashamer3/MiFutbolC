@echo off

REM Compile all C source files into an executable with SonarQube build wrapper
REM Note: Requires libqrencode installed (e.g., via vcpkg: vcpkg install qrencode)
build-wrapper-win-x86-64.exe --out-dir bw-output gcc -Wall -g -DUSE_NCURSES analisis.c bienestar.c cJSON.c camiseta.c cancha.c db.c estadisticas.c estadisticas_meta.c estadisticas_anio.c estadisticas_generales.c estadisticas_lesiones.c estadisticas_mes.c export.c export_all.c export_all_mejorado.c export_camisetas.c export_camisetas_mejorado.c export_estadisticas.c export_estadisticas_generales.c export_lesiones.c export_lesiones_mejorado.c export_partidos.c export_records_rankings.c export_pdf.c import.c lesion.c logros.c main.c menu.c partido.c records_rankings.c sqlite3.c utils.c equipo.c torneo.c temporada.c financiamiento.c settings.c entrenador_ia.c qr.c -lqrencode -lhpdf -lz -lncursesw -o MiFutbolC

REM Check if compilation was successful
if %errorlevel% equ 0 (
    echo Compilation successful.
) else (
    echo Compilation failed.
    echo Make sure libqrencode is installed: vcpkg install qrencode
)