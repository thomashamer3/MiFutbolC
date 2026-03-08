#!/usr/bin/env bash

# Installer + build script for MiFutbolC (Linux / macOS)
# ------------------------------------------------------
# This script checks for required development packages (Debian/Ubuntu) and builds the project.
#
# Usage:
#   ./Instalador-Linux.sh              # install deps + build (release)
#   ./Instalador-Linux.sh -d           # build debug (no optimizations)
#   ./Instalador-Linux.sh run          # build + run
#   BUILD_TYPE=Debug ./Instalador-Linux.sh  # alternate method to choose debug

set -euo pipefail

# Build configuration
CC="${CC:-gcc}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
RUN_AFTER_BUILD=0
STRIP_BINARY=0

# Simple argument parser
while [ "$#" -gt 0 ]; do
  case "$1" in
    -d|--debug)
      BUILD_TYPE=Debug
      shift
      ;;
    run)
      RUN_AFTER_BUILD=1
      shift
      ;;
    --strip)
      STRIP_BINARY=1
      shift
      ;;
    -h|--help)
      cat <<'EOF'
Usage: ./Instalador-Linux.sh [options]

Options:
  -d, --debug     Build in debug mode (no optimizations, includes debug symbols)
  run             Run the built binary after a successful build
  --strip         Strip symbols from the binary after building
  -h, --help      Show this help message

You can also set BUILD_TYPE=Debug to enable debug build.
EOF
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

if [ "${BUILD_TYPE}" = "Debug" ]; then
  CFLAGS="${CFLAGS:--Wall -g -O0 -std=c11}"
else
  CFLAGS="${CFLAGS:--Wall -O2 -std=c11 -march=native}"
fi

LDFLAGS="${LDFLAGS:--lhpdf -lz -lm}"

# Dependency installation (Debian/Ubuntu)
# This script attempts to ensure the minimal build dependencies are available.
check_deps() {
  local missing=()

  command -v "$CC" >/dev/null 2>&1 || missing+=("$CC")
  command -v pkg-config >/dev/null 2>&1 || missing+=(pkg-config)
  pkg-config --exists libharu 2>/dev/null || missing+=(libhpdf-dev)
  pkg-config --exists zlib 2>/dev/null || missing+=(zlib1g-dev)
  pkg-config --exists libpng 2>/dev/null || missing+=(libpng-dev)

  if [ "${#missing[@]}" -eq 0 ]; then
    return 0
  fi

  echo "\nMissing build dependencies: ${missing[*]}"

  # Try installing dependencies with the current distro's package manager
  if command -v apt-get >/dev/null 2>&1; then
    echo "Attempting to install dependencies via apt-get..."
    sudo apt-get update
    sudo apt-get install -y build-essential libhpdf-dev zlib1g-dev libpng-dev
    return
  fi

  if command -v dnf >/dev/null 2>&1; then
    echo "Attempting to install dependencies via dnf..."
    sudo dnf install -y gcc gcc-c++ make pkgconfig libharu-devel zlib-devel libpng-devel
    return
  fi

  if command -v zypper >/dev/null 2>&1; then
    echo "Attempting to install dependencies via zypper..."
    sudo zypper refresh
    sudo zypper install -y gcc gcc-c++ make pkg-config libharu-devel zlib-devel libpng-devel
    return
  fi

  if command -v pacman >/dev/null 2>&1; then
    echo "Attempting to install dependencies via pacman..."
    sudo pacman -Sy --noconfirm base-devel pkgconf libharu zlib libpng
    return
  fi

  echo "\nPlease install the missing packages manually and re-run this script."
  echo "Debian/Ubuntu: sudo apt-get install build-essential libhpdf-dev zlib1g-dev libpng-dev"
  echo "Fedora: sudo dnf install gcc gcc-c++ make pkgconfig libharu-devel zlib-devel libpng-devel"
  echo "openSUSE: sudo zypper install -y gcc gcc-c++ make pkg-config libharu-devel zlib-devel libpng-devel"
  echo "Arch: sudo pacman -Sy --noconfirm base-devel pkgconf libharu zlib libpng"
  exit 1
}

check_deps

# Warn if sqlite3.c contains Windows-specific configuration options
warn_sqlite_windows_macros() {
  if grep -q "SQLITE_WIN32_MALLOC" sqlite3.c; then
    echo "\nWARNING: sqlite3.c contains Windows-specific malloc configuration (SQLITE_WIN32_MALLOC)."
    echo "This may require adjusting compile-time defines when building on Linux."
  fi

  if grep -q "#ifdef _WIN32" sqlite3.c; then
    echo "\nWARNING: sqlite3.c contains _WIN32 conditionals (Windows-only code)."
    echo "This is usually fine, but ensure you're compiling in a non-Windows environment."
  fi
}

warn_sqlite_windows_macros

# Source files
SRC=(
  analisis.c
  bienestar.c
  cJSON.c
  camiseta.c
  cancha.c
  db.c
  estadisticas.c
  estadisticas_meta.c
  estadisticas_anio.c
  estadisticas_generales.c
  estadisticas_lesiones.c
  estadisticas_mes.c
  export.c
  export_all.c
  export_all_mejorado.c
  export_camisetas.c
  export_camisetas_mejorado.c
  export_estadisticas.c
  export_estadisticas_generales.c
  export_lesiones.c
  export_lesiones_mejorado.c
  export_partidos.c
  export_records_rankings.c
  export_pdf.c
  import.c
  lesion.c
  logros.c
  main.c
  menu.c
  partido.c
  records_rankings.c
  sqlite3.c
  utils.c
  equipo.c
  torneo.c
  temporada.c
  financiamiento.c
  settings.c
  entrenador_ia.c
)

OUT="MiFutbolC"

# Build
# -----
# This script is intended for Unix-like environments (Linux/macOS).
# It compiles the project with gcc/clang and links against libharu, zlib and libm.

echo "Building (BUILD_TYPE=${BUILD_TYPE}) with $CC..."
"$CC" $CFLAGS "${SRC[@]}" $LDFLAGS -o "$OUT"

echo "Compilation successful: $OUT"
if [ "$STRIP_BINARY" -eq 1 ]; then
  if command -v strip >/dev/null 2>&1; then
    echo "Stripping symbols from $OUT..."
    strip "$OUT" || echo "Warning: strip failed"
  else
    echo "Warning: strip not found; binary will include debug symbols"
  fi
fi

# Show portability info
if command -v file >/dev/null 2>&1; then
  file "$OUT"
fi

if command -v ldd >/dev/null 2>&1; then
  echo "\nShared library dependencies (ldd):"
  ldd "$OUT"
fi

ls -lh "$OUT"
if [ "$RUN_AFTER_BUILD" -eq 1 ]; then
  echo "Running $OUT..."
  ./"$OUT"
fi
