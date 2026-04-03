#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Audio Manager
//  Wraps the low-level Audio synthesizer with game-specific logic:
//  callback registration, music intensity tracking, lifecycle.
// ═══════════════════════════════════════════════════════════════════════════

class GameState;

namespace AudioManager {
    void init(GameState& state);
    void shutdown();
    void on_intro_complete();
}
