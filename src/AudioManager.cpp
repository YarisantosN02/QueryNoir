// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Audio Manager
//  Initialises the Audio synthesizer and wires it to game events via
//  GameState callbacks. Keeps audio concerns out of main.cpp.
// ═══════════════════════════════════════════════════════════════════════════

#include "AudioManager.h"
#include "Audio.h"
#include "GameState.h"
#include <algorithm>

namespace AudioManager {

void init(GameState& state){
    Audio::get().init();
    Audio::get().play(SFX::TERMINAL_BOOT, 0.7f);

    // Play a crystalline ping whenever a clue is discovered,
    // and raise music tension proportional to evidence found.
    state.on_clue_found([&state](const Clue&){
        Audio::get().play(SFX::CLUE, 0.9f);
        float intensity = std::min(1.f,
            (float)state.app().discovered_clues / 6.f);
        Audio::get().set_music_intensity(intensity);
    });

    // Ascending chime when a new database table unlocks.
    state.on_table_unlocked([](const TableInfo&){
        Audio::get().play(SFX::UNLOCK, 0.85f);
    });
}

void shutdown(){
    Audio::get().shutdown();
}

void on_intro_complete(){
    // Soft boot sound + gentle ambient fade-in after the intro ends.
    Audio::get().play(SFX::TERMINAL_BOOT, 0.5f);
    Audio::get().set_music_intensity(0.15f);
}

} // namespace AudioManager
