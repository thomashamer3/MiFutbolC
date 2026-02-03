#!/bin/bash

# Compile all C source files into an executable with SonarQube build wrapper
# Note: Requires libqrencode installed (e.g., sudo apt-get install libqrencode-dev)
./build-wrapper-win-x86-64.exe --out-dir bw-output gcc -Wall -g analisis.c cJSON.c camiseta.c cancha.c db.c estadisticas.c estadisticas_meta.c estadisticas_anio.c estadisticas_generales.c estadisticas_lesiones.c estadisticas_mes.c export.c export_all.c export_all_mejorado.c export_camisetas.c export_camisetas_mejorado.c export_estadisticas.c export_estadisticas_generales.c export_lesiones.c export_lesiones_mejorado.c export_partidos.c export_records_rankings.c export_pdf.c import.c lesion.c logros.c main.c menu.c partido.c records_rankings.c sqlite3.c utils.c equipo.c torneo.c temporada.c financiamiento.c settings.c comparator.c entrenador_ia.c qr.c -lqrencode -lhpdf -lz -o MiFutbolC

# Check if compilation was successful
if [[ $? -eq 0 ]]; then
    echo "Compilation successful. Running the program..."
    ./MiFutbolC
else
    echo "Compilation failed."
    echo "Make sure libqrencode is installed: sudo apt-get install libqrencode-dev (Linux) or brew install qrencode (macOS)"
fi
