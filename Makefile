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
CFLAGS += -fomit-frame-pointer
CFLAGS += -Wno-unused-parameter

LDFLAGS ?= C:/msys64/mingw64/lib/libhpdf.dll.a C:/msys64/mingw64/lib/libz.a C:/msys64/mingw64/lib/libpng.a -lm -lbcrypt -lcomdlg32 -lshell32 -lucrt -Wl,--allow-multiple-definition

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

# Auto-discover all .c files in the project root so new files are picked up
# automatically (e.g. export_common.c, undo.c, backup.c, filtros.c, etc.).
# Files under tests/, sqlite/, and unity/ are excluded explicitly below.
SRC = $(filter-out $(wildcard tests/*.c) $(wildcard sqlite/*.c) tests/unity/unity.c, $(wildcard *.c))

OUT = MiFutbolC

.PHONY: all clean run

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) $(AUDIO_LDFLAGS) -o $(OUT)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
