#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Case Manager
//  Handles case lifecycle: initialisation, future case loading/switching.
//  Currently wraps init_case_orion(); designed to support multiple cases.
// ═══════════════════════════════════════════════════════════════════════════

class GameState;

namespace CaseManager {
    // Load and initialise the first (and currently only) case.
    void init(GameState& state);

    // Future: void load_case(GameState& state, const std::string& case_id);
    // Future: void reset_case(GameState& state);
}
