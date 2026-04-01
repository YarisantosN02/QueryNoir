#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════
#  QUERY NOIR — Setup Script
#  Installs dependencies, downloads ImGui + SQLite3
# ═══════════════════════════════════════════════════════
set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${CYAN}[setup]${NC} $1"; }
ok()    { echo -e "${GREEN}[  ok ]${NC} $1"; }
error() { echo -e "${RED}[error]${NC} $1"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Detect OS ──────────────────────────────────────────────────────────────
OS="$(uname -s)"

# ── Install system packages ────────────────────────────────────────────────
info "Installing system packages..."

if [[ "$OS" == "Linux" ]]; then
    if command -v apt-get &>/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y \
            build-essential cmake pkg-config git \
            libsdl2-dev libsdl2-ttf-dev libsdl2-mixer-dev \
            libsqlite3-dev
        ok "apt packages installed"
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y \
            gcc-c++ cmake pkg-config git \
            SDL2-devel SDL2_ttf-devel SDL2_mixer-devel \
            sqlite-devel
        ok "dnf packages installed"
    elif command -v pacman &>/dev/null; then
        sudo pacman -S --noconfirm \
            base-devel cmake pkg-config git \
            sdl2 sdl2_ttf sdl2_mixer sqlite
        ok "pacman packages installed"
    else
        error "Unknown Linux package manager. Install SDL2, SDL2_ttf, SDL2_mixer, sqlite3 dev packages manually."
    fi

elif [[ "$OS" == "Darwin" ]]; then
    if ! command -v brew &>/dev/null; then
        error "Homebrew not found. Install from https://brew.sh"
    fi
    brew install cmake sdl2 sdl2_ttf sdl2_mixer sqlite3 pkg-config
    ok "Homebrew packages installed"

else
    error "Windows: please use WSL2 (Ubuntu) and re-run this script, or see README.md for manual setup."
fi

# ── Download Dear ImGui ────────────────────────────────────────────────────
info "Downloading Dear ImGui..."
IMGUI_DIR="$SCRIPT_DIR/vendor/imgui"

if [[ -d "$IMGUI_DIR/.git" ]]; then
    ok "ImGui already cloned, pulling latest..."
    git -C "$IMGUI_DIR" pull --quiet
else
    rm -rf "$IMGUI_DIR"
    git clone --depth=1 https://github.com/ocornut/imgui.git "$IMGUI_DIR"
    ok "ImGui cloned"
fi

# Copy required backend files into vendor/imgui/backends
mkdir -p "$IMGUI_DIR/backends"
# (they're already in the repo under backends/)
ok "ImGui backends ready"

# ── Download SQLite3 amalgamation (as fallback if system not found) ────────
SQLITE_DIR="$SCRIPT_DIR/vendor/sqlite3"
if ! pkg-config --exists sqlite3 2>/dev/null; then
    info "sqlite3 not found via pkg-config, downloading amalgamation..."
    mkdir -p "$SQLITE_DIR"
    curl -L "https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip" \
         -o /tmp/sqlite.zip
    unzip -o /tmp/sqlite.zip -d /tmp/sqlite_src
    cp /tmp/sqlite_src/sqlite-amalgamation-*/sqlite3.{h,c} "$SQLITE_DIR/"
    ok "SQLite3 amalgamation downloaded"
else
    ok "sqlite3 found via pkg-config"
fi

# ── Download JetBrains Mono font ───────────────────────────────────────────
info "Downloading JetBrains Mono font..."
FONT_DIR="$SCRIPT_DIR/assets/fonts"
mkdir -p "$FONT_DIR"

if [[ ! -f "$FONT_DIR/JetBrainsMono-Regular.ttf" ]]; then
    FONT_URL="https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip"
    curl -L "$FONT_URL" -o /tmp/jbmono.zip
    unzip -o /tmp/jbmono.zip -d /tmp/jbmono
    cp /tmp/jbmono/fonts/ttf/JetBrainsMono-Regular.ttf "$FONT_DIR/"
    cp /tmp/jbmono/fonts/ttf/JetBrainsMono-Bold.ttf    "$FONT_DIR/"
    ok "JetBrains Mono installed"
else
    ok "Font already present"
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo -e "${GREEN}  Setup complete! Now run: ./build.sh  ${NC}"
echo -e "${GREEN}═══════════════════════════════════════${NC}"
