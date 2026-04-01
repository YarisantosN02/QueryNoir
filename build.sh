#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════
#  QUERY NOIR — Build Script
# ═══════════════════════════════════════════════════════
set -e

CYAN='\033[0;36m'; GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${CYAN}[build]${NC} $1"; }
ok()    { echo -e "${GREEN}[  ok ]${NC} $1"; }
warn()  { echo -e "${YELLOW}[ warn]${NC} $1"; }
error() { echo -e "${RED}[error]${NC} $1"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Check ImGui is present
if [[ ! -f "$SCRIPT_DIR/vendor/imgui/imgui.cpp" ]]; then
    error "ImGui not found at vendor/imgui/. Run ./setup.sh first."
fi

# ── Always do a clean configure to avoid stale CMake cache ──────────────────
info "Cleaning CMake cache..."
rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"

# ── macOS: help CMake find Homebrew SDL2 ────────────────────────────────────
CMAKE_EXTRA_ARGS=""
if [[ "$(uname)" == "Darwin" ]]; then
    # Detect Apple Silicon vs Intel
    BREW_PREFIX="$(brew --prefix 2>/dev/null || echo /usr/local)"
    info "Homebrew prefix: $BREW_PREFIX"
    CMAKE_EXTRA_ARGS="-DCMAKE_PREFIX_PATH=$BREW_PREFIX"
    # Also set SDL2_DIR explicitly if cmake find_package needs it
    if [[ -d "$BREW_PREFIX/opt/sdl2" ]]; then
        CMAKE_EXTRA_ARGS="$CMAKE_EXTRA_ARGS;$BREW_PREFIX/opt/sdl2"
    fi
    if [[ -d "$BREW_PREFIX/opt/sdl2_ttf" ]]; then
        CMAKE_EXTRA_ARGS="$CMAKE_EXTRA_ARGS;$BREW_PREFIX/opt/sdl2_ttf"
    fi
    if [[ -d "$BREW_PREFIX/opt/sdl2_mixer" ]]; then
        CMAKE_EXTRA_ARGS="$CMAKE_EXTRA_ARGS;$BREW_PREFIX/opt/sdl2_mixer"
    fi
fi

info "Configuring with CMake..."
mkdir -p "$BUILD_DIR"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    ${CMAKE_EXTRA_ARGS:+-DCMAKE_PREFIX_PATH="$CMAKE_EXTRA_ARGS"} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

info "Compiling (this takes ~30s first time)..."
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build "$BUILD_DIR" --parallel "$CORES"

ok "Build successful!"
echo ""
echo -e "${GREEN}Run the game:${NC}"
echo "  ./run.sh"
echo "  OR: $BUILD_DIR/QueryNoir"
