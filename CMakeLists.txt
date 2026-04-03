cmake_minimum_required(VERSION 3.16)
project(QueryNoir VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── SDL2: use find_package (works on macOS Homebrew + Linux) ─────────────────
# On macOS: brew install sdl2 sdl2_ttf sdl2_mixer
# On Linux: apt install libsdl2-dev libsdl2-ttf-dev libsdl2-mixer-dev

# Tell CMake where Homebrew puts things on Apple Silicon and Intel
if(APPLE)
    # Apple Silicon
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/sdl2)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/sdl2_ttf)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/sdl2_mixer)
    # Intel Mac fallback
    list(APPEND CMAKE_PREFIX_PATH /usr/local)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/sdl2)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/sdl2_ttf)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/sdl2_mixer)
endif()

find_package(SDL2       REQUIRED)
find_package(SDL2_ttf   REQUIRED)
find_package(SDL2_mixer REQUIRED)

# ── SQLite3 ──────────────────────────────────────────────────────────────────
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "System SQLite3 not found -- using vendor amalgamation")
    set(SQLITE3_AMALGAMATION ON)
    set(SQLITE3_SRC  vendor/sqlite3/sqlite3.c)
    set(SQLITE3_INC  vendor/sqlite3)
else()
    set(SQLITE3_AMALGAMATION OFF)
    message(STATUS "Found SQLite3: ${SQLite3_VERSION}")
endif()

# ── ImGui ────────────────────────────────────────────────────────────────────
set(IMGUI_DIR vendor/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

# ── Game sources ─────────────────────────────────────────────────────────────
set(GAME_SOURCES
    src/main.cpp
    src/Renderer.cpp
    src/QueryConsole.cpp
    src/NarrativeFeed.cpp
    src/CaseManager.cpp
    src/AudioManager.cpp
    src/Database.cpp
    src/GameState.cpp
    src/UITheme.cpp
    src/Audio.cpp
    src/Intro.cpp
)

if(SQLITE3_AMALGAMATION)
    list(APPEND GAME_SOURCES ${SQLITE3_SRC})
endif()

add_executable(QueryNoir ${GAME_SOURCES} ${IMGUI_SOURCES})

# ── Includes ─────────────────────────────────────────────────────────────────
target_include_directories(QueryNoir PRIVATE
    include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# SDL2 include dirs differ between find_package versions
if(TARGET SDL2::SDL2)
    # Modern CMake targets (preferred)
    target_link_libraries(QueryNoir PRIVATE SDL2::SDL2)
else()
    target_include_directories(QueryNoir PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(QueryNoir PRIVATE ${SDL2_LIBRARIES})
endif()

if(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(QueryNoir PRIVATE SDL2_ttf::SDL2_ttf)
elseif(TARGET SDL2_ttf)
    target_link_libraries(QueryNoir PRIVATE SDL2_ttf)
else()
    target_include_directories(QueryNoir PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(QueryNoir PRIVATE ${SDL2_TTF_LIBRARIES})
endif()

if(TARGET SDL2_mixer::SDL2_mixer)
    target_link_libraries(QueryNoir PRIVATE SDL2_mixer::SDL2_mixer)
elseif(TARGET SDL2_mixer)
    target_link_libraries(QueryNoir PRIVATE SDL2_mixer)
else()
    target_include_directories(QueryNoir PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
    target_link_libraries(QueryNoir PRIVATE ${SDL2_MIXER_LIBRARIES})
endif()

# ── SQLite3 link ──────────────────────────────────────────────────────────────
if(SQLITE3_AMALGAMATION)
    target_include_directories(QueryNoir PRIVATE ${SQLITE3_INC})
    if(UNIX AND NOT APPLE)
        target_link_libraries(QueryNoir PRIVATE dl pthread)
    endif()
else()
    target_include_directories(QueryNoir PRIVATE ${SQLite3_INCLUDE_DIRS})
    if(TARGET SQLite::SQLite3)
        target_link_libraries(QueryNoir PRIVATE SQLite::SQLite3)
    else()
        target_link_libraries(QueryNoir PRIVATE ${SQLite3_LIBRARIES})
    endif()
endif()

# ── macOS frameworks ─────────────────────────────────────────────────────────
if(APPLE)
    target_link_libraries(QueryNoir PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework Metal"
    )
endif()

# ── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(QueryNoir PRIVATE /W3 /O2)
else()
    target_compile_options(QueryNoir PRIVATE -Wall -Wno-unused-function -O2)
endif()

# ── Copy assets ───────────────────────────────────────────────────────────────
add_custom_command(TARGET QueryNoir POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:QueryNoir>/assets
    COMMENT "Copying assets..."
)

# ── Summary ───────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "  ╔══ QUERY NOIR BUILD CONFIG ══╗")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  SQLite3:  amalgamation=${SQLITE3_AMALGAMATION}")
message(STATUS "  Output:   ${CMAKE_BINARY_DIR}/QueryNoir")
message(STATUS "  ╚════════════════════════════╝")
