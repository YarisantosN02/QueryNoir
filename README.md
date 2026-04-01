# 🕵️ QUERY NOIR
### A Cyber-Detective SQL Investigation Game

```
╔══════════════════════════════════════════════════════╗
║   QUERY NOIR — FORENSICS TERMINAL v4.7              ║
║   CASE: ORION MURDER  |  STATUS: ACTIVE             ║
╚══════════════════════════════════════════════════════╝
```

Marcus Orion is dead. You have access to NovaCorp's internal systems.
Use SQL to dig through logs, messages, and transactions — and find the killer.

---

## 📦 Requirements

| Tool | Version |
|------|---------|
| C++ Compiler | GCC 13+ or Clang 16+ |
| CMake | 3.16+ |
| SDL2 | 2.0.18+ |
| SDL2_ttf | 2.0+ |
| SDL2_mixer | 2.0+ |
| SQLite3 | 3.x |
| Git | any |
| curl + unzip | for setup script |

---

## 🚀 Quick Start (Linux / macOS)

```bash
# 1. Clone or extract the project
cd query_noir

# 2. Run the setup script (installs deps, downloads ImGui + fonts)
chmod +x setup.sh build.sh run.sh
./setup.sh

# 3. Build
./build.sh

# 4. Play
./run.sh
```

---

## 🪟 Windows (WSL2 — Recommended)

1. Install WSL2 with Ubuntu 22.04+
2. Install an X server: [VcXsrv](https://sourceforge.net/projects/vcxsrv/) or use WSLg (Windows 11)
3. In WSL2:
```bash
export DISPLAY=:0   # if not using WSLg
./setup.sh
./build.sh
./run.sh
```

### Windows Native (MinGW / MSYS2)
```bash
# In MSYS2 UCRT64 shell:
pacman -S mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-SDL2_mixer \
          mingw-w64-ucrt-x86_64-sqlite3 \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-gcc \
          git

# Then run setup manually (skip apt steps) and:
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./QueryNoir.exe
```

---

## 📁 Project Structure

```
query_noir/
├── CMakeLists.txt          ← Build configuration
├── setup.sh                ← Dependency installer
├── build.sh                ← Build runner
├── run.sh                  ← Game launcher
├── README.md               ← This file
│
├── src/
│   ├── main.cpp            ← Entry point, SDL2 init, all UI panels
│   ├── Database.cpp        ← SQLite3 wrapper + case data seeding
│   ├── GameState.cpp       ← Core game logic, clue engine, narrative
│   ├── UITheme.cpp         ← ImGui cyber-noir styling system
│   └── [stubs]             ← Renderer, Console, etc. (expandable)
│
├── include/
│   ├── Types.h             ← All shared types, structs, enums
│   ├── Database.h
│   ├── GameState.h
│   └── UITheme.h
│
├── vendor/
│   └── imgui/              ← Dear ImGui (downloaded by setup.sh)
│       └── backends/       ← SDL2 + SDLRenderer2 backends
│
└── assets/
    └── fonts/
        ├── JetBrainsMono-Regular.ttf   ← Downloaded by setup.sh
        └── JetBrainsMono-Bold.ttf
```

---

## 🎮 How to Play

### The Interface
```
┌─────────────────────────────────────────────────────────────────┐
│  ◈ CASE: ORION MURDER    ● ACTIVE    CLUES: 0/6    22:00:00    │
├──────────────┬──────────────────────────────┬───────────────────┤
│              │                              │                   │
│  DATABASES   │      QUERY CONSOLE           │  DETECTIVE FEED   │
│              │                              │                   │
│  💀 VICTIM   │  noir@forensics:~$           │  > Start with     │
│  👤 SUSPECTS │  SELECT * FROM ...           │    what you know  │
│  📡 LOGS     │                              │                   │
│  ✉️ [LOCKED] │  ┌─ RESULTS ──────────────┐ │  ? TRY: SELECT *  │
│  💰 [LOCKED] │  │ rows glow when suspect  │ │    FROM logs...   │
│              │  └─────────────────────────┘ │                   │
│  CLUE BOARD  │                              │                   │
│  ✓ Found...  │                              │                   │
│  ○ Unknown   │                              │                   │
└──────────────┴──────────────────────────────┴───────────────────┘
```

### Controls
| Key | Action |
|-----|--------|
| `Enter` | Execute query |
| `↑ / ↓` | Browse query history |
| `Escape` | Quit |
| Click table name | Auto-fill query |
| Hover table name | See columns |

### SQL Tips
```sql
-- See all suspects
SELECT * FROM suspects;

-- Timeline of building access
SELECT * FROM access_logs ORDER BY timestamp;

-- Filter for nighttime entries
SELECT * FROM access_logs
WHERE timestamp LIKE '%2047-03-15 2%'
ORDER BY timestamp;

-- Cross-reference suspects with logs
SELECT a.timestamp, a.person_name, a.action, s.alibi
FROM access_logs a
LEFT JOIN suspects s ON a.person_name = s.name
ORDER BY a.timestamp;

-- Search messages (unlock by querying access_logs first)
SELECT * FROM messages WHERE encrypted = 0;

-- Follow the money (unlock by querying messages first)
SELECT * FROM transactions ORDER BY timestamp;
```

### Unlocking Tables
- **messages** — Query `access_logs` first
- **transactions** — Query `messages` first

### Winning
Find **4 or more clues** and click **"SUBMIT CASE REPORT"** in the left panel.

---

## 🎨 Visual Design

- **Background**: Deep blue-black (#0A0F1C)
- **Primary**: Neon cyan (#00D4FF)
- **Accent**: Electric purple (#7A5CFF)
- **Danger**: Red glow (#FF4D4D)
- **Success**: Teal green (#32E696)
- **Font**: JetBrains Mono
- **Effects**: Scanlines, grid overlay, row glow, typewriter text, screen glitch on query

---

## 🧩 Expanding the Game

### Add a New Case
1. Create a new seed method in `Database.cpp`
2. Define tables, clues, and a `Case` struct in `GameState::init_case_*()`
3. Modify `check_clues()` with the new trigger conditions

### Add Sound Effects
The `AudioManager.cpp` stub is ready — hook in SDL2_mixer calls there and call from `GameState::run_query()`.

### Add More Tables
```cpp
// In Database.cpp seed method:
const char* sql = "CREATE TABLE new_table (...); INSERT INTO ...;";
execute(sql);

// In GameState::init_case_orion():
m_tables.push_back({ "new_table", "NEW TABLE", false, "unlock_condition", {}, "🔍" });
```

---

## 🐛 Troubleshooting

**"SDL2 not found"** → Run `./setup.sh` or install `libsdl2-dev`

**"ImGui not found"** → Run `./setup.sh` (clones from GitHub)

**Font not loading** → Game falls back to ImGui default font automatically

**Black screen** → Check if SDL2 renderer is hardware-accelerated (`SDL_RENDERER_ACCELERATED`)

**Wayland issues (Linux)** → `export SDL_VIDEODRIVER=x11` before running


*"The data doesn't lie. People do."*
