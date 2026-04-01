#include "GameState.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

// ─── Helpers ─────────────────────────────────────────────────────────────────

std::string GameState::sql_upper(const std::string& sql) {
    std::string up = sql;
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    return up;
}

bool GameState::sql_mentions(const std::string& sql, const std::string& kw) {
    return sql_upper(sql).find(kw) != std::string::npos;
}

// ─── Construction ─────────────────────────────────────────────────────────────

GameState::GameState() {}

void GameState::init_case_orion() {
    // Open in-memory SQLite
    m_db.open(":memory:");

    // Build case struct
    Case c;
    c.id       = "orion_murder";
    c.title    = "THE ORION CASE";
    c.subtitle = "Someone silenced him before he could talk.";
    c.opening_text =
        "Marcus Orion, 42, was found dead in Server Room B-7 at 02:14.\n"
        "You have access to the NovaCorp internal systems.\n"
        "Find out who — and why.";

    // Seed the database
    m_db.seed_case(c);

    // Tables (some locked initially)
    m_tables = {
        { "victims",      "VICTIM FILE",    true,  "",             {}, "💀" },
        { "suspects",     "SUSPECTS",       true,  "",             {}, "👤" },
        { "access_logs",  "ACCESS LOGS",    true,  "",             {}, "📡" },
        { "messages",     "MESSAGES",       false, "access_logs",  {}, "✉️"  },
        { "transactions", "TRANSACTIONS",   false, "messages",     {}, "💰" },
    };

    // Populate column info
    for (auto& t : m_tables) {
        if (t.unlocked)
            t.columns = m_db.get_column_names(t.name);
    }

    // Clues
    m_clues = {
        { "c1", "Server Room Entry",
          "Lena Park entered Server Room B-7 at 22:30 — the same time as the victim.",
          false, "access_logs" },
        { "c2", "The Last Message",
          "Lena Park sent a message at 22:15: 'He has the files. I can not let him expose this.'",
          false, "messages" },
        { "c3", "The Money Trail",
          "Hana Mori paid Lena Park $12,000 twelve days before the murder.",
          false, "transactions" },
        { "c4", "Bribery Network",
          "NovaCorp funds are flowing to ExtShell-LLC, then to Rex Calloway. $900,000 total.",
          false, "transactions" },
        { "c5", "Master Override",
          "At 02:14, a MASTER badge overrode the door — the same minute Marcus died.",
          false, "access_logs" },
        { "c6", "Vasquez Presence",
          "Elena Vasquez entered Server Room B-7 at 23:58 — but no EXIT until 00:16.",
          false, "access_logs" },
    };

    m_app.current_case = c;
    m_app.status       = GameStatus::ACTIVE;

    push_narrative(NarrativeType::SYSTEM,
        "QUERY NOIR — FORENSICS TERMINAL v4.7 — CASE LOADED");
    push_narrative(NarrativeType::MONOLOGUE,
        "Marcus Orion is dead. Start with what you know. Check the logs.");
    push_narrative(NarrativeType::HINT,
        "TRY: SELECT * FROM access_logs ORDER BY timestamp;");
}

// ─── Query Execution ──────────────────────────────────────────────────────────

QueryResult GameState::run_query(const std::string& sql) {
    if (sql.empty()) {
        QueryResult r; r.is_error = true; r.error = "Empty query.";
        return r;
    }

    // Add to history
    m_query_history.push_back(sql);
    if (m_query_history.size() > 50) m_query_history.erase(m_query_history.begin());

    // Block locked tables
    std::string up = sql_upper(sql);
    for (auto& t : m_tables) {
        if (!t.unlocked && up.find(t.name) != std::string::npos &&
            up.find("FROM") != std::string::npos) {
            QueryResult err;
            err.is_error = true;
            err.error    = "[ACCESS DENIED] Table '" + t.name + "' is locked. "
                           "Investigate '" + t.unlock_condition + "' first.";
            push_notification(NotifType::ERROR_MSG, "ACCESS DENIED: " + t.name);
            push_narrative(NarrativeType::ALERT,
                "That database is locked. You haven't earned it yet.");
            return err;
        }
    }

    m_app.query_executing = true;
    m_app.exec_anim_timer = 0.0f;

    auto result = m_db.execute(sql);
    m_last_result = result;

    if (result.is_error) {
        push_narrative(NarrativeType::ALERT, "Query error — " + result.error);
        push_notification(NotifType::ERROR_MSG, result.error);
    } else {
        analyse_query(sql, result);
        check_clues(sql, result);
        check_unlocks(sql);
        flag_suspicious_rows(result);
        m_last_result = result; // update with flags
    }

    return m_last_result;
}

// ─── Analysis Engine ──────────────────────────────────────────────────────────

void GameState::analyse_query(const std::string& sql, const QueryResult& result) {
    std::string up = sql_upper(sql);

    if (result.rows.empty() && !result.is_error) {
        push_narrative(NarrativeType::MONOLOGUE, "Nothing. Either it's clean — or it was cleaned.");
        return;
    }

    // Context-aware reactions
    if (up.find("ACCESS_LOGS") != std::string::npos) {
        if (up.find("ORDER") == std::string::npos)
            push_narrative(NarrativeType::HINT,
                "Try ordering by timestamp. Sequence matters in murder.");
        else if (up.find("WHERE") != std::string::npos &&
                 (up.find("22:") != std::string::npos || up.find("23:") != std::string::npos))
            push_narrative(NarrativeType::MONOLOGUE,
                "That window... 22:30 to 02:14. Something happened in those hours.");

        // Check if they found the master override
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell == "MASTER" || cell == "[UNKNOWN]")
                    push_narrative(NarrativeType::MONOLOGUE,
                        "A MASTER badge. That bypasses all security. Who has access to that?");
    }

    if (up.find("SUSPECTS") != std::string::npos)
        push_narrative(NarrativeType::MONOLOGUE,
            "Five suspects. At least one of them is lying about their alibi.");

    if (up.find("JOIN") != std::string::npos)
        push_narrative(NarrativeType::MONOLOGUE,
            "Good thinking. The connections between tables — that's where the truth hides.");

    if (up.find("MESSAGES") != std::string::npos)
        push_narrative(NarrativeType::MONOLOGUE,
            "People think messages disappear. They don't.");

    if (up.find("TRANSACTIONS") != std::string::npos) {
        push_narrative(NarrativeType::MONOLOGUE,
            "Follow the money. It always leads somewhere ugly.");
        bool found_mori = false;
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("Mori") != std::string::npos ||
                    cell.find("LPark") != std::string::npos) found_mori = true;
        if (found_mori)
            push_narrative(NarrativeType::MONOLOGUE,
                "Mori paid Park directly. Twelve thousand. Twelve days before the murder.");
    }
}

void GameState::check_clues(const std::string& sql, const QueryResult& result) {
    auto reveal = [&](const std::string& id) {
        for (auto& clue : m_clues) {
            if (clue.id == id && !clue.discovered) {
                clue.discovered = true;
                m_app.discovered_clues++;
                push_notification(NotifType::CLUE, "CLUE FOUND: " + clue.title);
                push_narrative(NarrativeType::SUCCESS, "— " + clue.description);
                if (m_clue_cb) m_clue_cb(clue);
            }
        }
    };

    // Check result content for clue triggers
    for (auto& row : result.rows) {
        for (auto& cell : row) {
            if (cell == "MASTER")       reveal("c5");
            if (cell == "[UNKNOWN]")    reveal("c5");
            if (cell.find("22:30") != std::string::npos) reveal("c1");
            if (cell.find("I can not let him expose") != std::string::npos) reveal("c2");
            if (cell.find("Mori-Private") != std::string::npos) reveal("c3");
            if (cell.find("ExtShell") != std::string::npos)     reveal("c4");
            if (cell == "Elena Vasquez" &&
                cell.find("23:58") != std::string::npos)         reveal("c6");
        }
    }

    // Also check SQL keywords
    std::string up = sql_upper(sql);
    if (up.find("ACCESS_LOGS") != std::string::npos && up.find("22:") != std::string::npos)
        reveal("c1");
    if (up.find("MESSAGES") != std::string::npos && !result.rows.empty())
        reveal("c2");
    if (up.find("TRANSACTIONS") != std::string::npos && !result.rows.empty()) {
        reveal("c3");
        reveal("c4");
    }
}

void GameState::check_unlocks(const std::string& sql) {
    std::string up = sql_upper(sql);
    for (auto& t : m_tables) {
        if (!t.unlocked && !t.unlock_condition.empty()) {
            std::string cond_up = t.unlock_condition;
            std::transform(cond_up.begin(), cond_up.end(), cond_up.begin(), ::toupper);
            if (up.find(cond_up) != std::string::npos) {
                t.unlocked = true;
                t.columns  = m_db.get_column_names(t.name);
                push_notification(NotifType::UNLOCK, "UNLOCKED: " + t.display_name);
                push_narrative(NarrativeType::SYSTEM,
                    "New table unlocked: " + t.display_name);
                if (m_unlock_cb) m_unlock_cb(t);
            }
        }
    }
}

void GameState::flag_suspicious_rows(QueryResult& result) {
    // Highlight rows containing suspicious keywords
    std::vector<std::string> suspicious_terms = {
        "MASTER", "[UNKNOWN]", "OVERRIDE",
        "22:30", "23:58", "02:14",
        "Lena Park", "ExtShell",
        "I can not let him expose"
    };

    for (size_t i = 0; i < result.rows.size(); i++) {
        for (auto& cell : result.rows[i]) {
            for (auto& term : suspicious_terms) {
                if (cell.find(term) != std::string::npos) {
                    result.flagged_rows.push_back(i);
                    break;
                }
            }
        }
    }
}

// ─── Narrative & Notifications ────────────────────────────────────────────────

void GameState::push_narrative(NarrativeType type, const std::string& text) {
    NarrativeEntry e;
    e.type         = type;
    e.text         = text;
    e.reveal_timer = 0.0f;
    e.revealed     = false;
    m_feed.push_front(e);
    if (m_feed.size() > 30) m_feed.pop_back();
}

void GameState::push_notification(NotifType type, const std::string& msg) {
    m_notifications.push_back({ type, msg, 4.0f, 0.0f });
}

// ─── Update ───────────────────────────────────────────────────────────────────

void GameState::update(float dt) {
    m_app.game_time += dt;

    // Execute animation
    if (m_app.query_executing) {
        m_app.exec_anim_timer += dt;
        if (m_app.exec_anim_timer > 0.5f)
            m_app.query_executing = false;
    }

    // Glitch decay
    if (m_app.glitch_timer > 0.0f) m_app.glitch_timer -= dt;

    // Reveal narrative entries char by char
    for (auto& e : m_feed) {
        if (!e.revealed) {
            e.reveal_timer += dt * 40.0f; // chars per second
            if (e.reveal_timer >= (float)e.text.size())
                e.revealed = true;
        }
    }

    // Age out notifications
    for (auto& n : m_notifications) n.age += dt;
    m_notifications.erase(
        std::remove_if(m_notifications.begin(), m_notifications.end(),
            [](const Notification& n){ return n.age >= n.lifetime; }),
        m_notifications.end());
}

bool GameState::is_solved() const {
    int found = 0;
    for (auto& c : m_clues) if (c.discovered) found++;
    return found >= 4; // Needs 4+ clues to solve
}

std::string& GameState::query_history_at(int i) {
    static std::string empty;
    if (i < 0 || i >= (int)m_query_history.size()) return empty;
    return m_query_history[i];
}
