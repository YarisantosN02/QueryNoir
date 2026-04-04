#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Case Manager
//  Handles case lifecycle: initialisation, future case loading/switching.
//  Currently wraps init_case_orion(); designed to support multiple cases.
// ═══════════════════════════════════════════════════════════════════════════

#include <string>
#include "GameState.h"

namespace CaseManager {
    // Load and initialise the first (and currently only) case.
    void init(GameState& state);
    void load_case(GameState& state, const std::string& case_id);
    void reset_case(GameState& state);
}
