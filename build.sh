#!/bin/bash

# Compile all C source files into an executable
gcc -Wall -g analisis.c cJSON.c cJSON_Utils.c camiseta.c cancha.c db.c estadisticas.c export.c export_all.c import.c lesion.c logros.c main.c menu.c partido.c sqlite3.c utils.c -o MiFutbolC

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful. Running the program..."
    ./MiFutbolC
else
    echo "Compilation failed."
fi
