#pragma once
#include <string>
#include "GameState.h"

namespace CaseManager {
    // Load and initialise the initial case.
    void init(GameState& state);
    
    // The master function that swaps the cartridges based on the ID string
    void load_case(GameState& state, const std::string& case_id);
    
    void reset_case(GameState& state);
}