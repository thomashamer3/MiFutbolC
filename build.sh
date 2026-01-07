#!/bin/bash

# Compile all C source files into an executable
gcc -Wall -g analisis.c cJSON.c cJSON_Utils.c camiseta.c cancha.c db.c estadisticas.c estadisticas_meta.c estadisticas_anio.c estadisticas_generales.c estadisticas_lesiones.c estadisticas_mes.c export.c export_all.c export_all_mejorado.c export_camisetas.c export_camisetas_mejorado.c export_estadisticas.c export_estadisticas_generales.c export_lesiones.c export_lesiones_mejorado.c export_partidos.c export_records_rankings.c import.c lesion.c logros.c main.c menu.c partido.c records_rankings.c sqlite3.c utils.c equipo.c torneo.c -o MiFutbolC

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful. Running the program..."
    ./MiFutbolC
else
    echo "Compilation failed."
fi
