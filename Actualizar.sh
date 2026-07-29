#!/usr/bin/env bash

# Script de actualizacion para MiFutbolC (Linux / macOS)
# -------------------------------------------------------
# Descarga la ultima version desde el repositorio remoto,
# compila y reinstala el proyecto.
#
# Usage:
#   ./Actualizar.sh              # buscar actualizaciones, compilar e instalar
#   ./Actualizar.sh -y           # no pedir confirmacion
#   ./Actualizar.sh --check      # solo verificar si hay actualizaciones
#   ./Actualizar.sh --run        # compilar, instalar y ejecutar
#   ./Actualizar.sh --source     # usar source (git pull) en vez de tag
#   REMOTE=codeberg ./Actualizar.sh  # usar remoto codeberg en vez de origin

set -euo pipefail

REMOTE="${REMOTE:-origin}"
AUTO_YES=0
CHECK_ONLY=0
RUN_AFTER_UPDATE=0
USE_SOURCE=0

# Colores (si el terminal soporta)
if [[ -t 1 ]]; then
  GREEN='\033[0;32m'
  YELLOW='\033[1;33m'
  RED='\033[0;31m'
  CYAN='\033[0;36m'
  BOLD='\033[1m'
  NC='\033[0m'
else
  GREEN=''
  YELLOW=''
  RED=''
  CYAN=''
  BOLD=''
  NC=''
fi

info()    { local msg="$1"; printf "${CYAN}[INFO]${NC} %s\n" "$msg"; }
success() { local msg="$1"; printf "${GREEN}[OK]${NC} %s\n" "$msg"; }
warn()    { local msg="$1"; printf "${YELLOW}[AVISO]${NC} %s\n" "$msg"; }
error()   { local msg="$1"; printf "${RED}[ERROR]${NC} %s\n" "$msg"; }

# Parse args
while [[ "$#" -gt 0 ]]; do
  case "$1" in
    -y|--yes)
      AUTO_YES=1
      shift
      ;;
    --check)
      CHECK_ONLY=1
      shift
      ;;
    --run)
      RUN_AFTER_UPDATE=1
      shift
      ;;
    --source)
      USE_SOURCE=1
      shift
      ;;
    -h|--help)
      cat <<'EOF'
Usage: ./Actualizar.sh [options]

Options:
  -y, --yes       No pedir confirmacion antes de actualizar
  --check         Solo verificar si hay actualizaciones disponibles
  --run           Ejecutar MiFutbolC despues de compilar
  --source        Actualizar desde la rama main (git pull) en vez de tags
  -h, --help      Mostrar este mensaje de ayuda

Variables de entorno:
  REMOTE          Remoto git a usar (default: origin)
                  Valores: origin, codeberg
EOF
      exit 0
      ;;
    *)
      error "Opcion desconocida: $1"
      exit 1
      ;;
  esac
done

# Verificar que estamos en un repositorio git
if ! git rev-parse --is-inside-work-tree &>/dev/null; then
  error "No se detecto un repositorio git."
  error "Ejecuta este script desde la carpeta del proyecto MiFutbolC."
  exit 1
fi

PROJECT_DIR="$(pwd)"

# Obtener la version actual (ultimo tag en el repositorio local)
get_current_version() {
  local tag
  tag=$(git describe --tags --abbrev=0 2>/dev/null || echo "sin-version")
  echo "$tag"
}

# Obtener la ultima version disponible en el remoto
get_remote_version() {
  git fetch "$REMOTE" --tags --quiet 2>/dev/null
  local tag
  tag=$(git describe --tags --abbrev=0 "$REMOTE/main" 2>/dev/null || echo "sin-version")
  echo "$tag"
}

CURRENT_VERSION="$(get_current_version)"

info "Proyecto: ${BOLD}MiFutbolC${NC}"
info "Remoto:   ${BOLD}${REMOTE}${NC}"
info "Directorio: ${BOLD}${PROJECT_DIR}${NC}"
echo ""

# Verificar que el remoto existe
if ! git remote get-url "$REMOTE" &>/dev/null; then
  error "El remoto '${REMOTE}' no existe."
  error "Remotos disponibles:"
  git remote -v | sed 's/^/  /'
  exit 1
fi

# Detectar si hay cambios locales sin commitear
LOCAL_CHANGES=0
if ! git diff --quiet HEAD 2>/dev/null; then
  LOCAL_CHANGES=1
fi

if [[ "$LOCAL_CHANGES" -eq 1 ]]; then
  warn "Hay cambios locales sin commitear."
  if [[ "$CHECK_ONLY" -eq 0 ]]; then
    info "Se creara un stash automaticamente antes de la actualizacion."
  fi
fi

# Obtener version remota
info "Buscando actualizaciones en ${REMOTE}..."
REMOTE_VERSION="$(get_remote_version)"

echo ""
printf "  Version local:  ${BOLD}%s${NC}\n" "$CURRENT_VERSION"
printf "  Version remota: ${BOLD}%s${NC}\n" "$REMOTE_VERSION"
echo ""

# Comparar versiones
if [[ "$CURRENT_VERSION" == "$REMOTE_VERSION" ]]; then
  success "Ya tienes la ultima version: ${CURRENT_VERSION}"
  if [[ "$CHECK_ONLY" -eq 1 ]]; then
    exit 0
  fi
  info "No hay nada que actualizar. Compilando de todas formas..."
  echo ""
else
  if [[ "$CHECK_ONLY" -eq 1 ]]; then
    warn "Hay una nueva version disponible: ${REMOTE_VERSION} (actual: ${CURRENT_VERSION})"
    exit 0
  fi

  info "Nueva version disponible: ${BOLD}${REMOTE_VERSION}${NC} (actual: ${CURRENT_VERSION})"
  echo ""

  if [[ "$AUTO_YES" -eq 0 ]]; then
    printf "Deseas actualizar a ${BOLD}%s${NC}? [s/N] " "$REMOTE_VERSION"
    read -r CONFIRM
    if [[ ! "$CONFIRM" =~ ^[sS]$ ]]; then
      info "Actualizacion cancelada."
      exit 0
    fi
  fi
fi

# Stash de cambios locales si existen
if [[ "$LOCAL_CHANGES" -eq 1 ]] && [[ "$CHECK_ONLY" -eq 0 ]]; then
  info "Guardando cambios locales (git stash)..."
  git stash push -m "Auto-stash antes de actualizacion $(date +%Y-%m-%d_%H-%M-%S)" --quiet
  success "Cambios locales guardados."
fi

# Obtener los ultimos cambios
if [[ "$CHECK_ONLY" -eq 1 ]]; then
  # En modo check no descargamos nada
  exit 0
fi

if [[ "$USE_SOURCE" -eq 1 ]]; then
  info "Actualizando desde la rama principal (git pull)..."
  git pull "$REMOTE" main --rebase --autostash
else
  info "Descargando tags y cambios recientes..."
  git fetch "$REMOTE" --tags --quiet

  if [[ "$REMOTE_VERSION" != "sin-version" ]]; then
    info "Cambiando a version ${REMOTE_VERSION}..."
    git checkout "$REMOTE_VERSION" --quiet 2>/dev/null || {
      # Si el checkout falla, intentar con pull
      warn "No se pudo hacer checkout al tag. Usando pull..."
      git pull "$REMOTE" main --rebase --autostash
    }
  else
    git pull "$REMOTE" main --rebase --autostash
  fi
fi

success "Codigo actualizado correctamente."
echo ""

# Detectar el metodo de build
BUILD_SCRIPT="./Instalador-Linux.sh"

if [[ -f "$BUILD_SCRIPT" ]]; then
  info "Compilando con ${BUILD_SCRIPT}..."
  chmod +x "$BUILD_SCRIPT"
  bash "$BUILD_SCRIPT" --path-user
else
  error "No se encontro el script de compilacion: ${BUILD_SCRIPT}"
  exit 1
fi

echo ""
success "Actualizacion completada!"

NEW_VERSION="$(get_current_version)"
if [[ "$NEW_VERSION" != "$CURRENT_VERSION" ]]; then
  info "Version instalada: ${BOLD}${NEW_VERSION}${NC}"
fi

# Ejecutar si se pidio
if [[ "$RUN_AFTER_UPDATE" -eq 1 ]]; then
  echo ""
  info "Ejecutando MiFutbolC..."
  ./MiFutbolC || error "Error al ejecutar MiFutbolC"
fi
