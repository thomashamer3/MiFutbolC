# Makefile for MiFutbolC
# ---------------------
# Supports both debug and release builds (Linux/macOS/Windows via MinGW)

CC ?= gcc
BUILD_TYPE ?= Release
ENABLE_NATIVE ?= 0

ifeq ($(BUILD_TYPE),Debug)
  CFLAGS ?= -Wall -g -O0 -std=c11
else
  CFLAGS ?= -Wall -O2 -std=c11
  ifeq ($(ENABLE_NATIVE),1)
    CFLAGS += -march=native
  endif
endif

CFLAGS += -I. -include compat_port.h

LDFLAGS ?= -lhpdf -lz -lpng -lm -lbcrypt

# Platform-specific audio flags required by miniaudio
ifeq ($(OS),Windows_NT)
  AUDIO_LDFLAGS :=
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    AUDIO_LDFLAGS := -framework CoreAudio -framework AudioToolbox -framework CoreFoundation
  else
    AUDIO_LDFLAGS := -lpthread -ldl
  endif
endif

SRC = \
  analisis.c \
  bienestar.c \
  cJSON.c \
  camiseta.c \
  colecciones.c \
  cancha.c \
  db.c \
  estadisticas.c \
  estadisticas_meta.c \
  estadisticas_anio.c \
  estadisticas_generales.c \
  estadisticas_lesiones.c \
  estadisticas_mes.c \
  export.c \
  export_all.c \
  export_all_mejorado.c \
  export_camisetas.c \
  export_camisetas_mejorado.c \
  export_estadisticas.c \
  export_estadisticas_generales.c \
  export_lesiones.c \
  export_lesiones_mejorado.c \
  export_partidos.c \
  export_records_rankings.c \
  export_pdf.c \
  import.c \
  lesion.c \
  logros.c \
  main.c \
  menu.c \
  partido.c \
  records_rankings.c \
  pdfgen.c \
  sqlite3.c \
  utils.c \
  equipo.c \
  torneo.c \
  temporada.c \
  financiamiento.c \
  settings.c \
  entrenador_ia.c \
  carrera.c \
  dashboard.c \
  busqueda.c \
  calendario.c \
  atajos.c \
  musica.c \
  musica_helpers.c \
  recordatorios.c

OUT = MiFutbolC

.PHONY: all clean run

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) $(AUDIO_LDFLAGS) -o $(OUT)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
