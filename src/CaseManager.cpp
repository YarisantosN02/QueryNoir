// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Case Manager
//  Owns case lifecycle logic. Currently loads Case #0447 (The Orion Murder).
//  Extend here to support a case-selection menu or multiple cases.
// ═══════════════════════════════════════════════════════════════════════════

#include "CaseManager.h"
#include "GameState.h"

namespace CaseManager {
    void init(GameState& state) { load_case(state, "orion"); }

    void load_case(GameState& state, const std::string& case_id) {
        state.reset();
        if (case_id == "orion") state.init_case_orion();
        else if (case_id == "espionage") state.init_case_espionage();
    }
} // namespace CaseManager
