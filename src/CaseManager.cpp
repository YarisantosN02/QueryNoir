#include "CaseManager.h"
#include "GameState.h"

// The Manager MUST include the cartridges to plug them in!
#include "OrionCase.h"     
#include "EspionageCase.h" 

namespace CaseManager {

    void init(GameState& state) { 
        // Start the game with the Orion case by default!
        load_case(state, "orion"); 
    }

    void load_case(GameState& state, const std::string& case_id) {
        
        // 1. THE CARTRIDGE SWAPPER (Factory Pattern)
        ICase* active_case = nullptr;
        
        if (case_id == "orion") {
            active_case = new OrionCase();
        } 
        else if (case_id == "espionage") {
            active_case = new EspionageCase();
        } 
        else {
            return; // Safety fallback if the case ID doesn't exist
        }

        // 2. Hand the cartridge to the GameState engine so it can ask for rules later
        state.set_case_ptr(active_case);

        // 3. Open the blank database
        state.get_db().open(":memory:");

        // 4. Load the Story variables, tables, and clues
        active_case->init(state);

        // 5. Seed the database with the case's specific SQL script
        std::string script = active_case->get_sql_script();
        // state.get_db().seed_from_script(script);
        state.get_case_ptr()->seed_data(state.get_db());

        // 6. Fetch the Schemas automatically for all unlocked tables!
        for (auto& t : state.tables()) {
            if (t.unlocked) {
                auto cols = state.get_db().get_column_info(t.name);
                for (auto& [name,type] : cols)
                    t.columns.push_back({name, type, ""});
                t.row_count = state.get_db().get_row_count(t.name);
            }
        }
    }

    void reset_case(GameState& state) {
        // Clear the database and reset the GameState
        state.get_db().close();
        state.reset();
    }
    
} // namespace CaseManager