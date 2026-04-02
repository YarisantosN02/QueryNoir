#include "GameState.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string GameState::sql_upper(const std::string& sql) {
    std::string u = sql;
    std::transform(u.begin(), u.end(), u.begin(), ::toupper);
    return u;
}

// Case-insensitive substring check
bool GameState::sql_mentions(const std::string& sql, const std::string& kw) {
    std::string kwu = kw;
    std::transform(kwu.begin(), kwu.end(), kwu.begin(), ::toupper);
    return sql_upper(sql).find(kwu) != std::string::npos;
}

GameState::GameState() {}

// ─────────────────────────────────────────────────────────────────────────────
//  Case Init
// ─────────────────────────────────────────────────────────────────────────────

void GameState::init_case_orion() {
    m_db.open(":memory:");

    Case c;
    c.id    = "orion_murder";
    c.title = "THE ORION CASE";

    m_db.seed_case(c);

    // ── Tables — unlock chain ─────────────────────────────────────────────
    // Unlock conditions are UPPERCASE strings to match against uppercased SQL
    m_tables = {
        { "victims",           "VICTIM FILE",         true,  "",                  {}, "◈" },
        { "suspects",          "SUSPECTS",            true,  "",                  {}, "?" },
        { "access_logs",       "ACCESS LOGS",         true,  "",                  {}, "⊡" },
        { "employees",         "EMPLOYEES",           true,  "",                  {}, "⊞" },
        { "badge_registry",    "BADGE REGISTRY",      false, "ACCESS_LOGS",       {}, "⊕" },
        { "incident_reports",  "INCIDENT REPORTS",    false, "BADGE_REGISTRY",    {}, "⚑" },
        { "messages",          "MESSAGES",            false, "INCIDENT_REPORTS",  {}, "✉" },
        { "transactions",      "TRANSACTIONS",        false, "MESSAGES",          {}, "$" },
        { "decrypted_messages","DECRYPTED MSGS",      false, "NOVACORP-BLACK-09", {}, "⚿" },
    };

    for (auto& t : m_tables)
        if (t.unlocked)
            t.columns = m_db.get_column_names(t.name);

    // ── Clues — each requires a specific query pattern, not just table access ─
    m_clues = {
        { "c1", "Anomaly Flag on Orion",
          "Marcus Orion's badge A-001 was flagged ANOMALY entering the Finance Suite at 14:55 — "
          "the day of his death. He saw something in there.",
          false, "" },
        { "c2", "Lena and Marcus Entered Together",
          "Lena Park (L-019) entered Server Room B-7 at 22:30, followed by Marcus one minute later. "
          "She arranged the meeting.",
          false, "" },
        { "c3", "Master Badge Modified by Lena",
          "Badge MASTER — which triggered the final 02:14 override — was last modified by badge L-019 "
          "(Lena Park) on Jan 19th. She gave herself override access.",
          false, "" },
        { "c4", "Vasquez Got the Order from Mori",
          "Decrypted message from Mori to Vasquez at 20:05: 'The MASTER key is yours.' "
          "Vasquez was instructed to act — but Lena was already inside.",
          false, "" },
        { "c5", "Lena's Incriminating Message",
          "Lena Park messaged an external contact at 22:15 — 15 minutes before the meeting: "
          "'He has the files. I cannot let that happen.'",
          false, "" },
        { "c6", "The Money Trail: Mori to Park",
          "Hana Mori paid Lena Park $12,000 on March 1st from a personal account — "
          "labelled 'discretionary bonus'. Park cashed it out 9 days later.",
          false, "" },
        { "c7", "NovaCorp Fraud: $1.35M to Calloway",
          "Three payments totalling $1.35M went from NovaCorp-Operations to ExtShell-LLC, "
          "then immediately to Rex Calloway's private account. Approved by Mori.",
          false, "" },
        { "c8", "Carl Bremer's Buried Report",
          "IT Admin Carl Bremer filed a report: badge L-019 modified MASTER-level badge permissions — "
          "something it has no right to do. The report was closed with no action by Vasquez.",
          false, "" },
    };

    m_app.current_case = c;
    m_app.status       = GameStatus::ACTIVE;

    push_narrative(NarrativeType::SYSTEM,
        "FORENSICS TERMINAL v4.7 — CASE FILE: ORION MURDER — LOADED");
    push_narrative(NarrativeType::MONOLOGUE,
        "Marcus Orion. 42. Found dead at 02:14 in Server Room B-7.");
    push_narrative(NarrativeType::MONOLOGUE,
        "Five colleagues had access. Nobody's talking. The data will.");
    push_narrative(NarrativeType::HINT,
        "Start with the basics. Check the access logs for the night of the 15th.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Query Execution
// ─────────────────────────────────────────────────────────────────────────────

QueryResult GameState::run_query(const std::string& sql) {
    if (sql.empty()) {
        QueryResult r; r.is_error=true; r.error="Empty query."; return r;
    }

    m_query_history.push_back(sql);
    if (m_query_history.size() > 80)
        m_query_history.erase(m_query_history.begin());

    std::string up = sql_upper(sql);

    // ── Block locked tables ────────────────────────────────────────────────
    for (auto& t : m_tables) {
        if (!t.unlocked) {
            std::string tname_up = t.name;
            std::transform(tname_up.begin(), tname_up.end(), tname_up.begin(), ::toupper);
            if (up.find(tname_up) != std::string::npos &&
                up.find("FROM") != std::string::npos)
            {
                QueryResult err;
                err.is_error = true;
                // Give a puzzle-flavored hint based on what needs to be done
                if (t.name == "badge_registry")
                    err.error = "[LOCKED] badge_registry — First examine access_logs to find which badge IDs were active that night.";
                else if (t.name == "incident_reports")
                    err.error = "[LOCKED] incident_reports — Cross-reference badge_registry to find the anomaly, then reports will open.";
                else if (t.name == "messages")
                    err.error = "[LOCKED] messages — There are open incident reports that point to who had motive. Investigate those first.";
                else if (t.name == "transactions")
                    err.error = "[LOCKED] transactions — You're not ready for the money trail yet. Check the messages first.";
                else if (t.name == "decrypted_messages")
                    err.error = "[LOCKED] decrypted_messages — You need a decryption key. Look carefully at badge_registry and incident_reports for a key string.";
                else
                    err.error = "[LOCKED] " + t.name + " — Investigate '" + t.unlock_condition + "' first.";
                push_notification(NotifType::ERROR_MSG, "ACCESS DENIED: " + t.display_name);
                push_narrative(NarrativeType::ALERT,
                    "That file is sealed. You need to follow the evidence chain.");
                return err;
            }
        }
    }

    m_app.query_executing = true;
    m_app.exec_anim_timer = 0.f;

    auto result = m_db.execute(sql);

    if (result.is_error) {
        // Friendly SQL error messages
        std::string errmsg = result.error;
        push_narrative(NarrativeType::ALERT, "SQL Error — " + errmsg);
        push_notification(NotifType::ERROR_MSG, errmsg);
    } else {
        analyse_query(sql, result);
        check_clues(sql, result);
        check_unlocks(sql, result);  // now takes result too
        flag_suspicious_rows(result);
        m_last_result = result;
    }

    m_last_result = result;
    return m_last_result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Narrative Analysis Engine
// ─────────────────────────────────────────────────────────────────────────────

void GameState::analyse_query(const std::string& sql, const QueryResult& result) {
    std::string up = sql_upper(sql);

    if (result.rows.empty() && !result.is_error) {
        push_narrative(NarrativeType::MONOLOGUE,
            "Nothing. Empty. Either there's nothing here, or it was already cleaned up.");
        return;
    }

    int rowcount = (int)result.rows.size();

    // ── access_logs reactions ────────────────────────────────────────────────
    if (up.find("ACCESS_LOGS") != std::string::npos) {
        if (up.find("ORDER") == std::string::npos && rowcount > 5)
            push_narrative(NarrativeType::HINT,
                "Order by timestamp. You need to see the sequence of events.");

        // Saw the ANOMALY flag on Orion
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell == "ANOMALY")
                    push_narrative(NarrativeType::MONOLOGUE,
                        "ANOMALY flag. Orion was flagged entering the Finance Suite. "
                        "What was he looking for in there?");

        // Saw the CRITICAL override entry
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell == "CRITICAL")
                    push_narrative(NarrativeType::MONOLOGUE,
                        "02:14. OVERRIDE. MASTER badge. That's not normal access — "
                        "that's a bypass. Who controls the MASTER badge?");

        // Found the server room entries at 22:30
        bool saw_2230 = false;
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("22:30") != std::string::npos ||
                    cell.find("22:31") != std::string::npos) saw_2230 = true;
        if (saw_2230)
            push_narrative(NarrativeType::MONOLOGUE,
                "22:30 — Lena Park enters Server Room B-7. "
                "22:31 — Marcus Orion follows her in. "
                "She was already there. She arranged the meeting.");

        // Unlocked suggestion
        if (up.find("BADGE") == std::string::npos)
            push_narrative(NarrativeType::HINT,
                "The badge IDs in these logs are key. "
                "Cross-reference them with badge_registry when you can.");
    }

    // ── badge_registry reactions ──────────────────────────────────────────────
    if (up.find("BADGE_REGISTRY") != std::string::npos) {
        for (auto& row : result.rows) {
            for (size_t i=0; i<row.size(); i++) {
                if (row[i] == "MASTER")
                    push_narrative(NarrativeType::MONOLOGUE,
                        "There it is. The MASTER badge. Issued to Vasquez — "
                        "but look at who last modified it. That's not an admin account.");
                if (row[i] == "L-019" && i > 0)
                    push_narrative(NarrativeType::MONOLOGUE,
                        "L-019 modified the MASTER badge. "
                        "L-019 is Lena Park. She has no admin privileges. "
                        "How did she do this?");
            }
        }
        if (up.find("WHERE") == std::string::npos)
            push_narrative(NarrativeType::HINT,
                "Filter on badge_id = 'MASTER' to isolate the override credential.");
    }

    // ── incident_reports reactions ────────────────────────────────────────────
    if (up.find("INCIDENT_REPORTS") != std::string::npos) {
        for (auto& row : result.rows)
            for (auto& cell : row) {
                if (cell.find("ExtShell") != std::string::npos)
                    push_narrative(NarrativeType::MONOLOGUE,
                        "An accountant flagged the ExtShell payments back in February. "
                        "CFO Mori personally closed the report. No action taken.");
                if (cell.find("L-019") != std::string::npos &&
                    cell.find("MASTER") != std::string::npos)
                    push_narrative(NarrativeType::MONOLOGUE,
                        "Carl Bremer caught it. L-019 modified the MASTER badge permissions. "
                        "He reported it. Vasquez closed it. Why would she bury that?");
                if (cell.find("NOVACORP-BLACK-09") != std::string::npos)
                    push_narrative(NarrativeType::SUCCESS,
                        "Decryption key found in incident report: NOVACORP-BLACK-09. "
                        "The encrypted messages between Mori and Vasquez can now be read.");
            }
    }

    // ── messages reactions ─────────────────────────────────────────────────────
    if (up.find("MESSAGES") != std::string::npos &&
        up.find("DECRYPTED") == std::string::npos) {
        bool saw_encrypted = false;
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("ENCRYPTED") != std::string::npos) saw_encrypted = true;

        if (saw_encrypted)
            push_narrative(NarrativeType::HINT,
                "Several messages are encrypted. The key is buried somewhere in the evidence. "
                "Check incident_reports carefully — every word matters.");

        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("cannot let that happen") != std::string::npos ||
                    cell.find("falls apart") != std::string::npos)
                    push_narrative(NarrativeType::MONOLOGUE,
                        "22:15 — fifteen minutes before the meeting. "
                        "She already knew what she was going to do.");

        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("could kill him") != std::string::npos)
                    push_narrative(NarrativeType::MONOLOGUE,
                        "Dorian Kast, 22:10: 'I swear I could kill him.' "
                        "Impulsive. Public. But he was already gone from the building by then.");
    }

    // ── decrypted_messages reactions ──────────────────────────────────────────
    if (up.find("DECRYPTED_MESSAGES") != std::string::npos) {
        push_narrative(NarrativeType::MONOLOGUE,
            "Mori to Vasquez, 20:05: 'The MASTER key is yours.' "
            "She ordered it. But Lena Park was already inside with Marcus.");
        push_narrative(NarrativeType::MONOLOGUE,
            "Two people had the same idea that night. "
            "Only one of them acted first.");
    }

    // ── transactions reactions ────────────────────────────────────────────────
    if (up.find("TRANSACTIONS") != std::string::npos) {
        bool saw_extshell = false, saw_mori_park = false, saw_cayman = false;
        for (auto& row : result.rows)
            for (auto& cell : row) {
                if (cell.find("ExtShell") != std::string::npos) saw_extshell = true;
                if (cell.find("Mori-Personal") != std::string::npos) saw_mori_park = true;
                if (cell.find("Cayman") != std::string::npos) saw_cayman = true;
            }

        if (saw_extshell && up.find("WHERE") == std::string::npos)
            push_narrative(NarrativeType::HINT,
                "Filter out payroll. Try: WHERE category = 'Vendor-Consulting' "
                "or WHERE from_account LIKE '%NovaCorp%'");

        if (saw_mori_park)
            push_narrative(NarrativeType::MONOLOGUE,
                "Mori-Personal to LPark-Account. $12,000. March 1st. "
                "'Discretionary bonus.' Park cashed it out 9 days later. "
                "Nine days before the murder.");

        if (saw_cayman)
            push_narrative(NarrativeType::MONOLOGUE,
                "Cayman-Shell-7. Then to H.Mori-Offshore. "
                "$415,000 of NovaCorp money, laundered in 48 hours. "
                "Marcus found this. That's why he had to die.");

        if (!saw_extshell && !saw_mori_park)
            push_narrative(NarrativeType::HINT,
                "Aggregate the data. Try: "
                "SELECT from_account, to_account, SUM(amount) "
                "FROM transactions GROUP BY from_account, to_account");
    }

    // ── JOIN reactions ────────────────────────────────────────────────────────
    if (up.find("JOIN") != std::string::npos) {
        push_narrative(NarrativeType::MONOLOGUE,
            "Good. Connecting the tables — that's how you see the full picture.");
    }

    // ── suspects reactions ─────────────────────────────────────────────────────
    if (up.find("FROM SUSPECTS") != std::string::npos ||
        up.find("FROM  SUSPECTS") != std::string::npos) {
        push_narrative(NarrativeType::MONOLOGUE,
            "Five suspects. Two unverified alibis. "
            "Alibis can be checked against the access logs.");
        push_narrative(NarrativeType::HINT,
            "Try joining suspects with access_logs on person_name "
            "to see who was actually in the building.");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Clue Detection — requires SPECIFIC queries, not just table access
// ─────────────────────────────────────────────────────────────────────────────

void GameState::check_clues(const std::string& sql, const QueryResult& result) {
    if (result.is_error || result.rows.empty()) return;
    std::string up = sql_upper(sql);

    auto reveal = [&](const std::string& id) {
        for (auto& clue : m_clues) {
            if (clue.id == id && !clue.discovered) {
                clue.discovered = true;
                m_app.discovered_clues++;
                push_notification(NotifType::CLUE, "CLUE FOUND: " + clue.title);
                push_narrative(NarrativeType::SUCCESS,
                    "FILE UPDATED: " + clue.description);
                if (m_clue_cb) m_clue_cb(clue);
            }
        }
    };

    // C1: Must find the ANOMALY flag in access_logs (requires seeing that row)
    for (auto& row : result.rows)
        for (auto& cell : row)
            if (cell == "ANOMALY") reveal("c1");

    // C2: Must see both Lena Park and Server Room B-7 in same result at 22:30
    {
        bool saw_park=false, saw_b7=false, saw_time=false;
        for (auto& row : result.rows) {
            bool row_park=false, row_b7=false, row_time=false;
            for (auto& cell : row) {
                if (cell.find("Lena Park") != std::string::npos || cell == "L-019")
                    row_park=true;
                if (cell.find("Server Room B-7") != std::string::npos)
                    row_b7=true;
                if (cell.find("22:30") != std::string::npos ||
                    cell.find("22:31") != std::string::npos)
                    row_time=true;
            }
            if (row_park) saw_park=true;
            if (row_b7)   saw_b7=true;
            if (row_time) saw_time=true;
        }
        if (saw_park && saw_b7 && saw_time) reveal("c2");
    }

    // C3: Must find MASTER badge with modified_by = L-019 in badge_registry
    {
        bool saw_master_badge=false;
        for (auto& row : result.rows) {
            bool has_master=false, has_l019=false;
            for (auto& cell : row) {
                if (cell == "MASTER") has_master=true;
                if (cell == "L-019")  has_l019=true;
            }
            if (has_master && has_l019) { saw_master_badge=true; break; }
        }
        if (saw_master_badge) reveal("c3");
    }

    // C4: Must read decrypted_messages and find the 20:05 message from Mori
    if (up.find("DECRYPTED_MESSAGES") != std::string::npos)
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("MASTER key is yours") != std::string::npos)
                    reveal("c4");

    // C5: Must find Lena Park's 22:15 external message in messages table
    if (up.find("MESSAGES") != std::string::npos &&
        up.find("DECRYPTED") == std::string::npos)
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("cannot let that happen") != std::string::npos ||
                    cell.find("falls apart") != std::string::npos)
                    reveal("c5");

    // C6: Must find the Mori->Park transaction (specific enough — needs transactions table)
    if (up.find("TRANSACTIONS") != std::string::npos)
        for (auto& row : result.rows) {
            bool has_mori_src=false, has_park_dst=false;
            for (auto& cell : row) {
                if (cell.find("Mori-Personal") != std::string::npos) has_mori_src=true;
                if (cell.find("LPark") != std::string::npos)          has_park_dst=true;
            }
            if (has_mori_src && has_park_dst) reveal("c6");
        }

    // C7: Must see ExtShell->Calloway in transactions (needs aggregation or filter)
    if (up.find("TRANSACTIONS") != std::string::npos)
        for (auto& row : result.rows)
            for (auto& cell : row)
                if (cell.find("RCCalloway") != std::string::npos) reveal("c7");

    // C8: Must find Carl Bremer's incident report about L-019 modifying MASTER
    if (up.find("INCIDENT_REPORTS") != std::string::npos)
        for (auto& row : result.rows) {
            bool has_bremer=false, has_l019=false, has_master=false;
            for (auto& cell : row) {
                if (cell.find("Carl Bremer") != std::string::npos ||
                    cell.find("L-019") != std::string::npos) { has_bremer=true; }
                if (cell.find("L-019") != std::string::npos) has_l019=true;
                if (cell.find("MASTER") != std::string::npos) has_master=true;
            }
            if (has_l019 && has_master) reveal("c8");
        }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Table Unlock Logic  (KEY FIX: compare UPPERCASE to UPPERCASE)
// ─────────────────────────────────────────────────────────────────────────────

void GameState::check_unlocks(const std::string& sql, const QueryResult& result) {
    std::string up = sql_upper(sql); // entire query uppercased

    for (auto& t : m_tables) {
        if (t.unlocked || t.unlock_condition.empty()) continue;

        // Uppercase the condition to match against the uppercased query
        std::string cond = t.unlock_condition;
        std::transform(cond.begin(), cond.end(), cond.begin(), ::toupper);

        bool unlock = false;

        // Special case: decrypted_messages requires finding the actual key string
        // in query results, not just querying a specific table
        if (t.name == "decrypted_messages") {
            // Player must use the key in their query (WHERE content LIKE '%NOVACORP-BLACK-09%'
            // or direct string literal) OR find it in result data
            if (up.find("NOVACORP-BLACK-09") != std::string::npos) {
                unlock = true;
            } else {
                // Also unlock if they found it in incident_reports results
                if (!result.is_error)
                    for (auto& row : result.rows)
                        for (auto& cell : row)
                            if (cell.find("NOVACORP-BLACK-09") != std::string::npos)
                                unlock = true;
            }
        } else {
            // Normal unlock: player must have queried the prerequisite table
            // AND gotten non-empty results from it
            unlock = (up.find(cond) != std::string::npos && !result.rows.empty()
                      && !result.is_error);
        }

        if (unlock) {
            t.unlocked = true;
            t.columns  = m_db.get_column_names(t.name);
            push_notification(NotifType::UNLOCK, "UNLOCKED: " + t.display_name);
            push_narrative(NarrativeType::SYSTEM,
                "New file accessible: " + t.display_name);
            if (m_unlock_cb) m_unlock_cb(t);

            // Contextual hint when each table unlocks
            if (t.name == "badge_registry")
                push_narrative(NarrativeType::HINT,
                    "badge_registry is now open. "
                    "Look up who has access to what — and who modified what.");
            else if (t.name == "incident_reports")
                push_narrative(NarrativeType::HINT,
                    "incident_reports unlocked. "
                    "Someone filed a complaint that was buried. Find it.");
            else if (t.name == "messages")
                push_narrative(NarrativeType::HINT,
                    "messages unlocked. "
                    "Filter encrypted = 0 first. "
                    "The encrypted ones need a key — it's hidden in the evidence.");
            else if (t.name == "transactions")
                push_narrative(NarrativeType::HINT,
                    "transactions unlocked. "
                    "This is a large table — filter by category or account. "
                    "Follow the money from NovaCorp outward.");
            else if (t.name == "decrypted_messages")
                push_narrative(NarrativeType::ALERT,
                    "DECRYPTED MESSAGES unlocked. "
                    "The conversations between Mori and Vasquez are now readable. "
                    "This changes everything.");
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Row Flagging
// ─────────────────────────────────────────────────────────────────────────────

void GameState::flag_suspicious_rows(QueryResult& result) {
    static const std::vector<std::string> terms = {
        "MASTER", "OVERRIDE", "CRITICAL", "ANOMALY",
        "22:30", "22:31", "23:58", "02:14",
        "Lena Park", "L-019",
        "ExtShell", "RCCalloway", "Cayman",
        "cannot let that happen", "falls apart",
        "NOVACORP-BLACK-09",
        "Mori-Personal",
        "ENCRYPTED",
    };

    for (size_t i=0; i<result.rows.size(); i++) {
        for (auto& cell : result.rows[i]) {
            for (auto& term : terms) {
                if (cell.find(term) != std::string::npos) {
                    result.flagged_rows.push_back(i);
                    goto next_row;
                }
            }
        }
        next_row:;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Narrative + Notification helpers
// ─────────────────────────────────────────────────────────────────────────────

void GameState::push_narrative(NarrativeType type, const std::string& text) {
    NarrativeEntry e;
    e.type         = type;
    e.text         = text;
    e.reveal_timer = 0.f;
    e.revealed     = false;
    m_feed.push_front(e);
    if (m_feed.size() > 40) m_feed.pop_back();
}

void GameState::push_notification(NotifType type, const std::string& msg) {
    m_notifications.push_back({ type, msg, 5.0f, 0.0f });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update (per frame)
// ─────────────────────────────────────────────────────────────────────────────

void GameState::update(float dt) {
    m_app.game_time += dt;

    if (m_app.query_executing) {
        m_app.exec_anim_timer += dt;
        if (m_app.exec_anim_timer > 0.4f)
            m_app.query_executing = false;
    }

    if (m_app.glitch_timer > 0.f) m_app.glitch_timer -= dt;

    for (auto& e : m_feed) {
        if (!e.revealed) {
            e.reveal_timer += dt * 45.f;
            if (e.reveal_timer >= (float)e.text.size())
                e.revealed = true;
        }
    }

    for (auto& n : m_notifications) n.age += dt;
    m_notifications.erase(
        std::remove_if(m_notifications.begin(), m_notifications.end(),
            [](const Notification& n){ return n.age >= n.lifetime; }),
        m_notifications.end());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Win Condition — need 6 of 8 clues AND must have read decrypted messages
// ─────────────────────────────────────────────────────────────────────────────

bool GameState::is_solved() const {
    int found = 0;
    bool has_decrypt = false;
    for (auto& c : m_clues) {
        if (c.discovered) found++;
        if (c.id == "c4" && c.discovered) has_decrypt = true;
    }
    return found >= 6 && has_decrypt;
}

std::string& GameState::query_history_at(int i) {
    static std::string empty;
    if (i < 0 || i >= (int)m_query_history.size()) return empty;
    return m_query_history[i];
}
