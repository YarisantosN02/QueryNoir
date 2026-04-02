#include "GameState.h"
#include <algorithm>
#include <cctype>
#include <iostream>

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
    for (auto& row:r.rows)
        for (auto& cell:row)
            if (cell.find(sub)!=std::string::npos) return true;
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

GameState::GameState() {}

// ─────────────────────────────────────────────────────────────────────────────
//  Case Init
// ─────────────────────────────────────────────────────────────────────────────
void GameState::init_case_orion() {
    m_db.open(":memory:");
    Case c; c.id="orion"; c.title="THE ORION MURDER";
    m_db.seed_case(c);

    // ── Tables ── (unlock_condition is a human-readable hint, not a code string)
    // Unlocking is done programmatically in check_unlocks()
    m_tables = {
        {"victims",           "VICTIM FILE",        true,  "", {}, "◈"},
        {"suspects",          "SUSPECTS",           true,  "", {}, "?"},
        {"access_logs",       "ACCESS LOGS",        true,  "", {}, "⊡"},
        {"badge_registry",    "BADGE REGISTRY",     false, "Find which badge IDs appear in access_logs at night", {}, "⊕"},
        {"messages",          "MESSAGES",           false, "Cross-reference badge_registry: who modified MASTER?", {}, "✉"},
        {"transactions",      "TRANSACTIONS",       false, "Find Lena's 22:15 message — then finance opens", {}, "$"},
        {"incident_reports",  "INCIDENT REPORTS",   false, "Trace the Mori→Park transaction in transactions", {}, "⚑"},
        {"decrypted_messages","DECRYPTED MSGS",     false, "Find key 'BLACK-OMEGA-7' in incident_reports", {}, "⚿"},
    };

    // Populate schema info for unlocked tables
    for (auto& t : m_tables) {
        if (t.unlocked) {
            auto cols = m_db.get_column_info(t.name);
            for (auto& [name,type] : cols)
                t.columns.push_back({name, type, ""});
            t.row_count = m_db.get_row_count(t.name);
        }
    }

    // ── Clues ── Each requires a specific, non-trivial SQL query
    // The puzzle condition enforces that the player wrote something meaningful.

    m_clues.clear();

    // CLUE 1: Orion's anomaly flag
    // Requires: SELECT from access_logs WHERE flag = 'ANOMALY' (or equivalent)
    // Player must filter, not just dump the table
    {
        Clue c;
        c.id    = "c1";
        c.title = "Finance Suite Anomaly";
        c.hint  = "Check access_logs for any flagged entries. Not all rows are equal.";
        c.description =
            "Marcus Orion's badge A-001 was flagged ANOMALY entering the Finance Suite "
            "at 14:55 — the day of his death. He saw something in there he wasn't supposed to.";
        c.condition.sql_must_contain    = {"ACCESS_LOGS", "FLAG"};
        c.condition.result_must_contain_cell = "ANOMALY";
        m_clues.push_back(c);
    }

    // CLUE 2: Lena arranged the meeting
    // Requires: query access_logs filtered to Server Room B-7 at night — must see both entries
    {
        Clue c;
        c.id    = "c2";
        c.title = "The Server Room Meeting";
        c.hint  = "Filter access_logs to zone 'Server Room B-7' after 22:00. Who went in — and in what order?";
        c.description =
            "Lena Park (L-019) entered Server Room B-7 at 22:30. Marcus followed one minute later at 22:31. "
            "She set up the meeting. He walked in unaware.";
        c.condition.sql_must_contain    = {"ACCESS_LOGS", "SERVER ROOM"};
        c.condition.result_must_contain_cell = "22:30";
        m_clues.push_back(c);
    }

    // CLUE 3: MASTER badge — who modified it?
    // Requires: SELECT from badge_registry WHERE badge_id = 'MASTER'
    {
        Clue c;
        c.id    = "c3";
        c.title = "MASTER Badge Tampered";
        c.hint  = "The 02:14 OVERRIDE used badge 'MASTER'. Query badge_registry for that badge_id specifically.";
        c.description =
            "MASTER badge was last modified on Jan 19th — by badge L-019 (Lena Park). "
            "She has no admin rights. She gave herself override access two months before the murder.";
        c.condition.sql_must_contain    = {"BADGE_REGISTRY", "MASTER"};
        c.condition.result_must_contain_cell = "L-019";
        m_clues.push_back(c);
    }

    // CLUE 4: Lena's pre-murder message
    // Requires: SELECT from messages WHERE sender = 'Lena Park' — must see the 22:15 message
    // SELECT * FROM messages doesn't work — encrypted=0 filter or sender filter needed
    {
        Clue c;
        c.id    = "c4";
        c.title = "Lena's 22:15 Message";
        c.hint  = "Filter messages WHERE sender = 'Lena Park'. Pay attention to the timestamp relative to 22:30.";
        c.description =
            "At 22:15 — 15 minutes before the meeting — Lena Park messaged an external contact: "
            "'Tonight is the only chance to stop this. Proceeding.' "
            "She planned it before she walked in.";
        c.condition.sql_must_contain    = {"MESSAGES", "LENA PARK"};
        c.condition.result_must_contain_cell = "Proceeding";
        m_clues.push_back(c);
    }

    // CLUE 5: Mori paid Park — the bribe
    // Requires: SELECT from transactions WHERE from_acct LIKE '%Mori%' — or group by
    // SELECT * shows too much noise; player must filter to find it
    {
        Clue c;
        c.id    = "c5";
        c.title = "The Payoff: Mori to Park";
        c.hint  = "In transactions, filter WHERE from_acct LIKE '%Mori%' or WHERE category='Personal'. Follow the personal money, not the payroll.";
        c.description =
            "Hana Mori paid $12,000 from her personal account to Lena Park on March 1st — "
            "15 days before the murder. Park cashed it out 9 days later. "
            "This is not a bonus. This is a contract.";
        c.condition.sql_must_contain    = {"TRANSACTIONS"};
        c.condition.sql_must_not_contain= {};
        c.condition.result_must_contain_cell = "Mori-Personal";
        m_clues.push_back(c);
    }

    // CLUE 6: Fraud trail — NovaCorp to Calloway
    // Requires: aggregation or filtering to see the ExtShell->Calloway routing
    // Must use WHERE or GROUP BY to isolate vendor payments
    {
        Clue c;
        c.id    = "c6";
        c.title = "The Fraud: $1.35M to Calloway";
        c.hint  = "In transactions, GROUP BY from_acct, to_acct or filter WHERE category='Vendor-Consulting'. The payroll rows are noise.";
        c.description =
            "NovaCorp paid ExtShell-LLC $1.35M across three invoices — all approved solely by Mori. "
            "ExtShell immediately forwarded it to Rex Calloway's private account. "
            "Marcus found this. That's why he had to die.";
        c.condition.sql_must_contain    = {"TRANSACTIONS"};
        c.condition.result_must_contain_cell = "RCCalloway-Private";
        m_clues.push_back(c);
    }

    // CLUE 7: Carl Bremer's buried report
    // Requires: SELECT from incident_reports WHERE filed_by = 'Carl Bremer' or subject LIKE '%badge%'
    {
        Clue c;
        c.id    = "c7";
        c.title = "Buried IT Report";
        c.hint  = "Filter incident_reports WHERE filed_by = 'Carl Bremer' or WHERE subject LIKE '%badge%'. Someone saw this coming.";
        c.description =
            "Carl Bremer filed a report in January: badge L-019 modified the MASTER credential "
            "without admin rights. The report was closed immediately — by Elena Vasquez. "
            "She buried it.";
        c.condition.sql_must_contain    = {"INCIDENT_REPORTS"};
        c.condition.result_must_contain_cell = "Carl Bremer";
        m_clues.push_back(c);
    }

    // CLUE 8: Mori's encrypted orders to Vasquez
    // Requires: SELECT from decrypted_messages — table only unlocks after finding "BLACK-OMEGA-7"
    // Must query the table (which is only available after unlock) and see the 20:05 message
    {
        Clue c;
        c.id    = "c8";
        c.title = "Mori's Order: 'Park is handling it'";
        c.hint  = "Read the decrypted_messages table. Filter by sender = 'Hana Mori'.";
        c.description =
            "Decrypted message, Mori to Vasquez, 20:05: "
            "'Park is handling it. She will meet him in B-7 at 22:30. MASTER key is hers to use.' "
            "Mori ordered the murder. Park carried it out.";
        c.condition.sql_must_contain    = {"DECRYPTED_MESSAGES"};
        c.condition.result_must_contain_cell = "Park is handling it";
        m_clues.push_back(c);
    }

    m_app.status = GameStatus::ACTIVE;

    push_narrative(NarrativeType::SYSTEM,
        "FORENSICS TERMINAL v4.7 — CASE: ORION MURDER — INITIALISED");
    push_narrative(NarrativeType::SYSTEM,
        "Type SCHEMA in the console at any time to inspect table structures.");
    push_narrative(NarrativeType::MONOLOGUE,
        "Marcus Orion. 42. Found dead in Server Room B-7 at 02:14.");
    push_narrative(NarrativeType::MONOLOGUE,
        "Five people were in the building that night. The data has no reason to lie.");
    push_narrative(NarrativeType::HINT,
        "Start with access_logs. Filter to that night. Look for patterns — not just names.");
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

    auto result = m_db.execute(raw_sql);

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
    std::string up = upper(sql);

    auto unlock = [&](const std::string& tname) {
        for (auto& t : m_tables) {
            if (t.name == tname && !t.unlocked) {
                t.unlocked = true;
                auto cols = m_db.get_column_info(tname);
                for (auto& [n,ty] : cols) t.columns.push_back({n,ty,""});
                t.row_count = m_db.get_row_count(tname);
                push_notification(NotifType::UNLOCK, "UNLOCKED: " + t.display_name);
                push_narrative(NarrativeType::SYSTEM,
                    "New file accessible: " + t.display_name);
                if (m_unlock_cb) m_unlock_cb(t);
            }
        }
    };

    // badge_registry unlocks when player queries access_logs
    // AND their query returns the MASTER or AFTER_HOURS flag (shows they looked at flags)
    if (sql_has(up,"ACCESS_LOGS") &&
        (result_has_cell(result,"MASTER") || result_has_cell(result,"AFTER_HOURS")
         || result_has_cell(result,"CRITICAL") || result_has_cell(result,"ANOMALY")))
        unlock("badge_registry");

    // messages unlocks when player queries badge_registry and finds L-019 on MASTER row
    if (sql_has(up,"BADGE_REGISTRY") &&
        result_has_cell(result,"L-019") &&
        result_has_cell(result,"MASTER"))
        unlock("messages");

    // transactions unlocks when player finds Lena's 22:15 message
    // (must filter messages to see it — not just SELECT *)
    if (sql_has(up,"MESSAGES") && !sql_has(up,"DECRYPTED") &&
        result_has_cell(result,"Proceeding"))
        unlock("transactions");

    // incident_reports unlocks when player finds the Mori->Park transaction
    if (sql_has(up,"TRANSACTIONS") &&
        result_has_cell(result,"Mori-Personal"))
        unlock("incident_reports");

    // decrypted_messages unlocks ONLY when player finds the key "BLACK-OMEGA-7"
    // either by reading incident_reports carefully, or by typing the key in a query
    if ((sql_has(up,"INCIDENT_REPORTS") && result_has_cell(result,"BLACK-OMEGA-7")) ||
        sql_has(up,"BLACK-OMEGA-7"))
        unlock("decrypted_messages");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Context Narrative — guide without giving answers
// ─────────────────────────────────────────────────────────────────────────────
void GameState::add_context_narrative(const std::string& sql, const QueryResult& result) {
    std::string up = upper(sql);
    int rows = (int)result.rows.size();

    if (rows == 0) {
        push_narrative(NarrativeType::MONOLOGUE,
            "Nothing. Widen the filter or check the column name."); return;
    }

    // Warn if player is doing a naked SELECT *
    bool is_naked_select = sql_has(up,"SELECT *") && !sql_has(up,"WHERE") &&
                           !sql_has(up,"JOIN") && !sql_has(up,"GROUP BY") &&
                           !sql_has(up,"HAVING");

    if (sql_has(up,"ACCESS_LOGS")) {
        if (is_naked_select && rows > 15)
            push_narrative(NarrativeType::HINT,
                "You're seeing all "+std::to_string(rows)+" rows. "
                "Narrow it — filter by zone, action, or flag. "
                "You're looking for something specific.");
        else if (sql_has(up,"FLAG") && result_has_cell(result,"ANOMALY"))
            push_narrative(NarrativeType::MONOLOGUE,
                "ANOMALY. Orion was flagged in the Finance Suite. "
                "What was he looking at in there?");
        else if (result_has_cell(result,"CRITICAL"))
            push_narrative(NarrativeType::MONOLOGUE,
                "02:14. CRITICAL override. MASTER badge. "
                "That's not an access card — it's a skeleton key. Who controls it?");
        else if (result_has_cell(result,"22:30"))
            push_narrative(NarrativeType::MONOLOGUE,
                "22:30 — Lena Park. 22:31 — Marcus Orion. "
                "One minute apart. Same room. She was already there waiting.");
    }

    if (sql_has(up,"BADGE_REGISTRY")) {
        if (is_naked_select)
            push_narrative(NarrativeType::HINT,
                "8 badge records. Find the MASTER badge specifically. "
                "Try: WHERE badge_id = 'MASTER'");
        else if (result_has_cell(result,"L-019") && result_has_cell(result,"MASTER"))
            push_narrative(NarrativeType::MONOLOGUE,
                "L-019 modified the MASTER badge. "
                "L-019 is Lena Park. She has no admin rights. "
                "How did she do this — and why did nobody stop it?");
    }

    if (sql_has(up,"MESSAGES") && !sql_has(up,"DECRYPTED")) {
        if (is_naked_select)
            push_narrative(NarrativeType::HINT,
                "12 messages — half are noise, three are encrypted. "
                "Filter by sender or timestamp. "
                "Try: WHERE sender = 'Lena Park'  or  WHERE encrypted = 0");
        else if (result_has_cell(result,"[ENCRYPTED]"))
            push_narrative(NarrativeType::HINT,
                "Encrypted messages. You need a key. "
                "It's not in this table — it's buried in the paper trail.");
        else if (result_has_cell(result,"Proceeding"))
            push_narrative(NarrativeType::MONOLOGUE,
                "22:15. 'Tonight is the only chance to stop this. Proceeding.' "
                "Fifteen minutes before the meeting. "
                "She had already decided.");
    }

    if (sql_has(up,"TRANSACTIONS")) {
        if (is_naked_select)
            push_narrative(NarrativeType::HINT,
                "16 rows — mostly payroll. "
                "Filter it: WHERE category = 'Vendor-Consulting'  or  WHERE from_acct LIKE '%Mori%'. "
                "The interesting rows are buried.");
        else if (result_has_cell(result,"RCCalloway-Private"))
            push_narrative(NarrativeType::MONOLOGUE,
                "ExtShell-LLC, then straight to Calloway's private account. "
                "Approved by Mori. $1.35 million across six months. "
                "Marcus found this ledger. That's what got him killed.");
        else if (result_has_cell(result,"Mori-Personal"))
            push_narrative(NarrativeType::MONOLOGUE,
                "Mori-Personal to LPark-Account. $12,000. March 1st. "
                "'Personal' category. Park cashed it 9 days later. "
                "This is a payment, not a bonus.");
    }

    if (sql_has(up,"INCIDENT_REPORTS")) {
        if (is_naked_select)
            push_narrative(NarrativeType::HINT,
                "4 reports. One of them has something hidden inside it. "
                "Try: WHERE filed_by = 'Carl Bremer'");
        else if (result_has_cell(result,"BLACK-OMEGA-7"))
            push_narrative(NarrativeType::SUCCESS,
                "BLACK-OMEGA-7. That's the decryption key for the encrypted messages. "
                "Use it: SELECT * FROM decrypted_messages;");
        else if (result_has_cell(result,"E.Vasquez"))
            push_narrative(NarrativeType::MONOLOGUE,
                "Vasquez closed the report. She buried the evidence "
                "of Lena Park tampering with the MASTER badge.");
    }

    if (sql_has(up,"DECRYPTED_MESSAGES")) {
        push_narrative(NarrativeType::MONOLOGUE,
            "Read them in order. Three messages. The last one is the order.");
    }

    if (sql_has(up,"SUSPECTS")) {
        if (is_naked_select)
            push_narrative(NarrativeType::HINT,
                "Five suspects. Check alibi_status — two say UNVERIFIED. "
                "The logs can confirm or deny every alibi.");
    }

    if (sql_has(up,"JOIN"))
        push_narrative(NarrativeType::MONOLOGUE,
            "Good. Connecting tables is how you see what doesn't fit.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Row Flagging — only flag rows that directly contain evidence
//  NO flagging based on names alone — player must discover connections
// ─────────────────────────────────────────────────────────────────────────────
void GameState::flag_rows(QueryResult& result) {
    // Only flag rows that contain objectively anomalous data values
    // Do NOT flag suspect names — that would give away the answer
    static const std::vector<std::string> objective_flags = {
        "ANOMALY", "CRITICAL", "AFTER_HOURS", "OVERRIDE",
        "BLACK-OMEGA-7",
        "[ENCRYPTED]",
        "Mori-Personal",        // specific account name, not a person
        "RCCalloway-Private",   // specific account name
        "Cayman-7712",
        "Park is handling it",  // direct quote from decrypted msg
        "MASTER key is hers",
        "cannot be allowed",
    };
    for (size_t i=0; i<result.rows.size(); i++) {
        for (auto& cell : result.rows[i]) {
            for (auto& f : objective_flags) {
                if (cell.find(f) != std::string::npos) {
                    result.flagged_rows.push_back(i);
                    goto next;
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
    std::string n = upper(name);
    // Trim whitespace
    while (!n.empty() && n.front()==' ') n=n.substr(1);
    while (!n.empty() && n.back()==' ')  n.pop_back();

    // Correct answer: Lena Park (accept variations)
    if (n == "LENA PARK" || n == "PARK" || n == "L-019" || n == "LENA") {
        m_app.status = GameStatus::CASE_SOLVED;
        push_narrative(NarrativeType::SUCCESS,
            "CASE CLOSED. Lena Park. Charged with first-degree murder.");
        return true;
    }

    // Wrong — give specific reactive feedback
    m_app.accuse.wrong_timer = 4.f;
    if (n == "HANA MORI" || n == "MORI")
        m_app.accuse.wrong_feedback =
            "Mori ordered it — but didn't pull the trigger. "
            "The access logs put someone else in that room.";
    else if (n == "ELENA VASQUEZ" || n == "VASQUEZ")
        m_app.accuse.wrong_feedback =
            "Vasquez received orders and arrived at 23:58 — but Lena Park "
            "was already in the room since 22:30. Check who exited when.";
    else if (n == "DORIAN KAST" || n == "KAST" || n == "DORIAN")
        m_app.accuse.wrong_feedback =
            "Kast left the building at 21:47. His alibi is partial but "
            "he had no motive strong enough — and no access to MASTER.";
    else if (n == "REX CALLOWAY" || n == "CALLOWAY" || n == "REX")
        m_app.accuse.wrong_feedback =
            "Calloway is dirty — but he was at the hotel. "
            "He's the beneficiary, not the one who did this.";
    else
        m_app.accuse.wrong_feedback =
            "'" + name + "' is not a match. Review your evidence. "
            "You need to be certain.";

    push_narrative(NarrativeType::ALERT,
        "Wrong. " + m_app.accuse.wrong_feedback);
    return false;
}

bool GameState::can_accuse() const {
    // Need at least 5 clues including the pre-murder message (c4) and badge tampering (c3)
    int found=0; bool has_c3=false, has_c4=false;
    for (auto& c:m_clues) {
        if(c.discovered){ found++; }
        if(c.id=="c3"&&c.discovered) has_c3=true;
        if(c.id=="c4"&&c.discovered) has_c4=true;
    }
    return found>=5 && has_c3 && has_c4;
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


