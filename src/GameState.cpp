#include "GameState.h"
#include "CaseManager.h"
#include "ICase.h"
#include <algorithm>
#include <cctype>
#include <iostream>


GameState::~GameState() {
    if (m_current_case_ptr != nullptr) {
        delete m_current_case_ptr;
    }
}

// ... rest of your functions (set_case_ptr, upper, etc) ...
void GameState::set_case_ptr(ICase* new_case) {
    if (m_current_case_ptr != nullptr) {
        delete m_current_case_ptr;
    }
    m_current_case_ptr = new_case;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Static helpers
// ─────────────────────────────────────────────────────────────────────────────
std::string GameState::upper(const std::string& s) {
    std::string u=s;
    std::transform(u.begin(),u.end(),u.begin(),::toupper);
    return u;
}
bool GameState::sql_has(const std::string& up, const std::string& kw) {
    return up.find(upper(kw)) != std::string::npos;
}

bool GameState::result_has_cell(const QueryResult& r, const std::string& sub) {
    std::string sub_up = upper(sub); // Uppercase the keyword we are looking for
    for (const auto& row : r.rows) {
        for (const auto& cell : row) {
            // Uppercase the cell data before checking
            if (upper(cell).find(sub_up) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

bool GameState::puzzle_satisfied(const PuzzleCondition& p,
                                  const std::string& sql_up,
                                  const QueryResult& result) const {
    if (result.is_error || result.rows.empty()) return false;
    for (auto& req : p.sql_must_contain)
        if (!sql_has(sql_up, req)) return false;
    for (auto& bad : p.sql_must_not_contain)
        if (sql_has(sql_up, bad)) return false;
    if (!p.result_must_contain_cell.empty())
        if (!result_has_cell(result, p.result_must_contain_cell)) return false;
    if ((int)result.rows.size() < p.min_rows) return false;
    return true;
}

GameState::GameState() {
    if (m_current_case_ptr != nullptr) {
        delete m_current_case_ptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Run Query
// ─────────────────────────────────────────────────────────────────────────────
QueryResult GameState::run_query(const std::string& raw_sql) {
    if (raw_sql.empty()) {
        QueryResult r; r.is_error=true; r.error="Empty query."; return r;
    }

    // Handle SCHEMA command
    std::string trimmed = raw_sql;
    while (!trimmed.empty() && (trimmed.front()==' '||trimmed.front()=='\n')) trimmed=trimmed.substr(1);
    std::string check = upper(trimmed);
    // strip trailing semicolons/spaces
    while (!check.empty() && (check.back()==';'||check.back()==' ')) check.pop_back();

    if (check == "SCHEMA") {
        m_app.show_schema = !m_app.show_schema;
        push_narrative(NarrativeType::SYSTEM,
            m_app.show_schema ? "Schema panel opened." : "Schema panel closed.");
        QueryResult r;
        r.columns = {"info"};
        r.rows    = {{"Type SCHEMA to toggle schema panel."}};
        return r;
    }

    if (check.find("LOAD ") == 0) {
        // Extract the case name (everything after "LOAD ")
        std::string target_case = trimmed.substr(5); 
        while (!target_case.empty() && target_case.back()==' ') target_case.pop_back();
        
        // Convert to lowercase so it matches "orion" or "espionage" exactly
        std::string target_lower = target_case;
        std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(), ::tolower);

        // Reset the engine variables (clears clues, tables, and narrative feed)
        this->reset(); 

        // Ask the CaseManager to load the new cartridge!
        CaseManager::load_case(*this, target_lower);

        QueryResult r;
        r.columns = {"system_msg"};
        r.rows = {{"CARTRIDGE LOADED: " + target_lower}};
        return r;
    }

    m_query_history.push_back(raw_sql);
    if (m_query_history.size() > 100) m_query_history.erase(m_query_history.begin());

    std::string up = upper(raw_sql);

    // ── Block locked tables ───────────────────────────────────────────────────
    for (auto& t : m_tables) {
        if (!t.unlocked) {
            std::string tup = upper(t.name);
            if (sql_has(up, tup) && sql_has(up, "FROM")) {
                QueryResult err;
                err.is_error = true;
                err.error    = "[LOCKED] " + t.name + "\n"
                               "How to unlock: " + t.unlock_condition;
                push_notification(NotifType::ERROR_MSG,
                    "LOCKED: " + t.display_name);
                push_narrative(NarrativeType::ALERT,
                    "That file is sealed. Follow the evidence chain first.");
                push_narrative(NarrativeType::HINT,
                    t.unlock_condition);
                m_last_result = err;
                return err;
            }
        }
    }

    m_app.query_executing = true;
    m_app.exec_anim_timer = 0.f;

    auto result = m_db.run_query(raw_sql);

    if (result.is_error) {
        push_narrative(NarrativeType::ALERT, "SQL Error — " + result.error);
        push_notification(NotifType::ERROR_MSG, result.error);
    } else {
        check_puzzles(raw_sql, result);
        check_unlocks(raw_sql, result);
        add_context_narrative(raw_sql, result);
        flag_rows(result);
    }

    m_last_result = result;
    return m_last_result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Puzzle Engine — clues earned only by specific queries
// ─────────────────────────────────────────────────────────────────────────────
void GameState::check_puzzles(const std::string& sql, const QueryResult& result) {
    std::string up = upper(sql);
    for (auto& clue : m_clues) {
        if (clue.discovered) continue;
        if (puzzle_satisfied(clue.condition, up, result)) {
            clue.discovered = true;
            m_app.discovered_clues++;
            push_notification(NotifType::CLUE, "EVIDENCE: " + clue.title);
            push_narrative(NarrativeType::SUCCESS,
                "[FILE UPDATED] " + clue.description);
            if (m_clue_cb) m_clue_cb(clue);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Unlock Logic — each requires querying the *previous* table meaningfully
// ─────────────────────────────────────────────────────────────────────────────
void GameState::check_unlocks(const std::string& sql, const QueryResult& result) {
    if (result.is_error || result.rows.empty()) return;
    
    if (m_current_case_ptr != nullptr) {
        m_current_case_ptr->check_unlocks(*this, sql, result);
    }


}

// ─────────────────────────────────────────────────────────────────────────────
//  Context Narrative — guide without giving answers
// ─────────────────────────────────────────────────────────────────────────────
void GameState::add_context_narrative(const std::string& sql, const QueryResult& result) {
    if (result.rows.empty()) {
        push_narrative(NarrativeType::MONOLOGUE, "Nothing. Widen the filter."); 
        return;
    }

    // Pass the work to the specific case file
    if (m_current_case_ptr != nullptr) {
        m_current_case_ptr->add_context_narrative(*this, sql, result);
    }

}

// ─────────────────────────────────────────────────────────────────────────────
//  Row Flagging — only flag rows that directly contain evidence
//  NO flagging based on names alone — player must discover connections
// ─────────────────────────────────────────────────────────────────────────────
void GameState::flag_rows(QueryResult& result) {
    // If no case is loaded, there is nothing to flag.
    if (m_current_case_ptr == nullptr) return;

    // Ask the active cartridge for its specific list of keywords
    std::vector<std::string> objective_flags = m_current_case_ptr->get_flag_keywords();

    // Loop through the results and flag rows that match the cartridge's keywords
    for (size_t i = 0; i < result.rows.size(); i++) {
        for (auto& cell : result.rows[i]) {
            for (auto& f : objective_flags) {
                if (cell.find(f) != std::string::npos) {
                    result.flagged_rows.push_back(i);
                    goto next; // Skip to the next row once a flag is found
                }
            }
        }
        next:;
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  Accusation mechanic
// ─────────────────────────────────────────────────────────────────────────────
bool GameState::try_accuse(const std::string& name) {
   // Trim whitespace logic (keep this part)
    std::string n = upper(name);
    while (!n.empty() && n.front()==' ') n=n.substr(1);
    while (!n.empty() && n.back()==' ')  n.pop_back();

    // Pass the name to the case file to see if the player is right!
    if (m_current_case_ptr != nullptr) {
        return m_current_case_ptr->try_accuse(*this, n);
    }
    
    return false;

}

bool GameState::can_accuse() const {
    if (m_current_case_ptr != nullptr) {
        return m_current_case_ptr->can_accuse(*this);
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Narrative helpers
// ─────────────────────────────────────────────────────────────────────────────
void GameState::push_narrative(NarrativeType t, const std::string& text) {
    NarrativeEntry e; e.type=t; e.text=text; e.reveal_timer=0.f; e.revealed=false;
    m_feed.push_front(e);
    if (m_feed.size()>50) m_feed.pop_back();
}
void GameState::push_notification(NotifType t, const std::string& msg) {
    m_notifications.push_back({t, msg, 5.f, 0.f});
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────
void GameState::update(float dt) {
    m_app.game_time += dt;
    if (m_app.query_executing) {
        m_app.exec_anim_timer += dt;
        if (m_app.exec_anim_timer > 0.35f) m_app.query_executing = false;
    }
    if (m_app.glitch_timer > 0.f) m_app.glitch_timer -= dt;
    if (m_app.accuse.wrong_timer > 0.f) m_app.accuse.wrong_timer -= dt;

    for (auto& e:m_feed)
        if (!e.revealed) {
            e.reveal_timer += dt * 50.f;
            if (e.reveal_timer >= (float)e.text.size()) e.revealed = true;
        }

    for (auto& n:m_notifications) n.age += dt;
    m_notifications.erase(
        std::remove_if(m_notifications.begin(), m_notifications.end(),
            [](auto& n){ return n.age >= n.lifetime; }),
        m_notifications.end());
}

void GameState::reset() {
    m_tables.clear();
    m_clues.clear();
    m_feed.clear();
    m_notifications.clear();
    m_query_history.clear();
    m_app.discovered_clues = 0;
}

std::string get_terminal_prompt(const GameState& state) {
    std::string user = "noir";
    std::string host = "forensics";
    std::string current_dir = state.get_current_case().id; 
    return user + "@" + host + ":~/" + current_dir + "$ ";
}

void GameState::unlock_table(const std::string& name) {
    for (auto& t : m_tables) {
        if (t.name == name && !t.unlocked) {
            t.unlocked = true;
            
            // Fetch the exact row count dynamically
            QueryResult count_res = m_db.run_query("SELECT COUNT(*) FROM " + t.name);
            if (!count_res.is_error && !count_res.rows.empty() && !count_res.rows[0].empty()) {
                try {
                    t.row_count = std::stoi(count_res.rows[0][0]);
                } catch (...) {
                    t.row_count = 0;
                }
            }

            // Fetch the column names and data types dynamically
            if (t.columns.empty()) {
                QueryResult pragma_res = m_db.run_query("PRAGMA table_info(" + t.name + ")");
                if (!pragma_res.is_error) {
                    for (const auto& row : pragma_res.rows) {
                        if (row.size() >= 3) {
                            t.columns.push_back({row[1], row[2], ""});
                        }
                    }
                }
            }
            
            push_notification(NotifType::TABLE_UNLOCKED, "ACCESS GRANTED: " + t.display_name);
            push_narrative(NarrativeType::SUCCESS, "Decryption complete. Table '" + t.name + "' is now readable.");
            if (m_unlock_cb) m_unlock_cb(t);
            return;
        }
    }
}