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
    m_current_case.id = "orion";
    m_current_case.title = "THE ORION MURDER";
    m_current_case.help_objective = "Collect at least 5 pieces of evidence including badge tampering and the pre-murder message. Then click ACCUSE and name the killer.";
    m_current_case.accusation_prompt = 
        "You have enough evidence. Type the full name of the person you believe "
        "murdered Marcus Orion. A wrong accusation costs you.";
    m_current_case.resolved_title = "THE ORION MURDER -- RESOLVED";
    m_current_case.charged_name   = "Lena Park -- Senior Data Analyst";
    m_current_case.accessory_name = "Hana Mori (ordered the act)";
    m_current_case.resolution_text = 
        "Marcus Orion found $1.35M in fraudulent vendor payments routed from NovaCorp through "
        "ExtShell-LLC to Rex Calloway, looping offshore back to Mori. He planned to expose it "
        "that night.\n\n"
        "Mori paid Park $12,000 to stop him. Park modified badge MASTER two months prior, giving "
        "herself override access she had no right to. At 22:15 she messaged an external contact: "
        "'Tonight is the only chance.'\n\n"
        "She met Marcus in Server Room B-7 at 22:30. He never left.\n\n"
        "Elena Vasquez received Mori's order at 20:05. She arrived at 23:58 -- too late. Park "
        "had already used the MASTER override at 02:14.\n\n"
        "Carl Bremer flagged the badge tampering in January. Vasquez buried the report.";

    m_db.seed_case(m_current_case);
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

    // ── ORION CASE LOGIC ──
    if (m_current_case.id == "orion") {
        // badge_registry unlocks when player queries access_logs
        if (sql_has(up,"ACCESS_LOGS") &&
            (result_has_cell(result,"MASTER") || result_has_cell(result,"AFTER_HOURS")
             || result_has_cell(result,"CRITICAL") || result_has_cell(result,"ANOMALY")))
            unlock("badge_registry");

        // messages unlocks when player queries badge_registry and finds L-019
        if (sql_has(up,"BADGE_REGISTRY") &&
            result_has_cell(result,"L-019") &&
            result_has_cell(result,"MASTER"))
            unlock("messages");

        // transactions unlocks when player finds Lena's 22:15 message
        if (sql_has(up,"MESSAGES") && !sql_has(up,"DECRYPTED") &&
            result_has_cell(result,"Proceeding"))
            unlock("transactions");

        // incident_reports unlocks when player finds the Mori->Park transaction
        if (sql_has(up,"TRANSACTIONS") &&
            result_has_cell(result,"Mori-Personal"))
            unlock("incident_reports");

        // decrypted_messages unlocks ONLY when player finds the key "BLACK-OMEGA-7"
        if ((sql_has(up,"INCIDENT_REPORTS") && result_has_cell(result,"BLACK-OMEGA-7")) ||
            sql_has(up,"BLACK-OMEGA-7"))
            unlock("decrypted_messages");
    }
    
    // ── BLUEBIRD / ESPIONAGE CASE LOGIC ──
    else if (m_current_case.id == "espionage") {
        // Unlock security_alerts when player finds the 1500MB anomaly
        if (sql_has(up, "NETWORK_LOGS") && result_has_cell(result, "1500"))
            unlock("security_alerts");

        // Unlock vpn_registry when player finds David Kim (E-103) clearing alerts
        if (sql_has(up, "SECURITY_ALERTS") && result_has_cell(result, "E-103"))
            unlock("vpn_registry");

        // Unlock emails when player finds Sarah Chen's (E-101) IP address
        if (sql_has(up, "VPN_REGISTRY") && result_has_cell(result, "E-101"))
            unlock("emails");

        // Unlock bank_transfers when player finds the SYS_WIPE email
        if (sql_has(up, "EMAILS") && result_has_cell(result, "SYS_WIPE"))
            unlock("bank_transfers");

        // Unlock decrypted_emails when player finds the ApexTech transfer
        if (sql_has(up, "BANK_TRANSFERS") && result_has_cell(result, "ApexTech"))
            unlock("decrypted_emails");
    }
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

    // ── 2. ORION CASE LOGIC ──
    if (m_current_case.id == "orion") {
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
    }

    // ── 3. BLUEBIRD (ESPIONAGE) CASE LOGIC ──
    else if (m_current_case.id == "espionage") {
        
        if (sql_has(up, "NETWORK_LOGS")) {
            if (is_naked_select && rows > 10)
                push_narrative(NarrativeType::HINT,
                    "Thousands of packets. Look for anomalies or massive transfers. "
                    "Filter by action or bytes.");
            else if (result_has_cell(result, "1500"))
                push_narrative(NarrativeType::MONOLOGUE,
                    "1500 MB. Exactly the size of the Bluebird prototype. "
                    "Someone backed up a truck to the servers at 03:45.");
        }
        
        if (sql_has(up, "SECURITY_ALERTS")) {
            if (is_naked_select)
                push_narrative(NarrativeType::HINT,
                    "Check who was on duty to clear the alerts around the time of the leak.");
            else if (result_has_cell(result, "E-103"))
                push_narrative(NarrativeType::MONOLOGUE,
                    "E-103 manually resolved the alert. That's David Kim in IT. "
                    "He didn't just clear it, he wiped the logs.");
        }

        if (sql_has(up, "VPN_REGISTRY")) {
            if (is_naked_select)
                push_narrative(NarrativeType::HINT,
                    "Match the anomalous IP from the network logs to a specific session.");
            else if (result_has_cell(result, "45.22.11.9"))
                push_narrative(NarrativeType::MONOLOGUE,
                    "IP 45.22.11.9 maps to E-101. Sarah Chen. "
                    "She pulled the trigger, but David held the door open.");
        }

        if (sql_has(up, "EMAILS")) {
            if (is_naked_select)
                push_narrative(NarrativeType::HINT,
                    "Too much noise. Search for communications involving E-103 or specific keywords.");
            else if (result_has_cell(result, "SYS_WIPE"))
                push_narrative(NarrativeType::MONOLOGUE,
                    "SYS_WIPE confirmed hours in advance. "
                    "This wasn't opportunistic. It was highly coordinated.");
        }

        if (sql_has(up, "BANK_TRANSFERS")) {
            if (is_naked_select)
                push_narrative(NarrativeType::HINT,
                    "Filter by large amounts or unknown entities. Someone got paid.");
            else if (result_has_cell(result, "ApexTech"))
                push_narrative(NarrativeType::MONOLOGUE,
                    "Half a million dollars from ApexTech. Corporate espionage confirmed. "
                    "Follow the money trail from Shell-77 to see who got paid.");
        }
    }

    // ── 4. Shared Mechanics (Both Cases) ──
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
        "1500",             // The massive exfiltration
        "SYS_WIPE",         // The premeditated cover-up command
        "ApexTech",         // The rival company
        "Shell-77",         // The dummy account
        "GhostProtocol",    // The illicit VPN session
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

    // ── 1. ORION CASE LOGIC ──
    if (m_current_case.id == "orion") {
        // Correct answer: Lena Park (accept variations)
        if (n == "LENA PARK" || n == "Lena" || n == "L-019" || n == "LENA") {
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

    // ── 2. BLUEBIRD (ESPIONAGE) CASE LOGIC ──
    else if (m_current_case.id == "espionage") {
        // Correct answers: Sarah Chen (Leaker) or David Kim (Accomplice)
        if (n == "SARAH CHEN" || n == "CHEN" || n == "E-101" || n == "SARAH") {
            m_app.status = GameStatus::CASE_SOLVED;
            push_narrative(NarrativeType::SUCCESS,
                "CASE CLOSED. Sarah Chen. Arrested for corporate espionage and grand larceny.");
            return true;
        } else if (n == "DAVID KIM" || n == "KIM" || n == "E-103" || n == "DAVID") {
            m_app.status = GameStatus::CASE_SOLVED;
            push_narrative(NarrativeType::SUCCESS,
                "CASE CLOSED. David Kim. Arrested for accessory to espionage and tampering with evidence.");
            return true;
        }

        // Wrong — give specific reactive feedback
        m_app.accuse.wrong_timer = 4.f;
        if (n == "APEXTECH" || n == "APEX") {
            m_app.accuse.wrong_feedback = 
                "They bought the data, but you need to arrest the employee who sold it.";
        } else if (n == "GHOSTPROTOCOL" || n == "GHOST") {
            m_app.accuse.wrong_feedback = 
                "That's the VPN session, not a person. Cross-reference the IP to an employee.";
        } else {
            m_app.accuse.wrong_feedback =
                "'" + name + "' is not a match. Review the IP logs and bank transfers.";
        }

        push_narrative(NarrativeType::ALERT,
            "Wrong. " + m_app.accuse.wrong_feedback);
        return false;
    }

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

void GameState::init_case_espionage() {
    m_db.open(":memory:");
    m_current_case.id = "espionage";
    m_current_case.title = "PROJECT BLUEBIRD";
    m_current_case.help_objective = "Identify the employee who leaked the Bluebird prototype. Find their accomplice in IT and follow the money. Collect 8 clues, then click ACCUSE.";
    m_current_case.accusation_prompt = 
        "You have enough evidence. Type the full name of the employee who leaked "
        "Project Bluebird, or their IT accomplice. A wrong accusation costs you.";
    m_current_case.resolved_title = "PROJECT BLUEBIRD -- RESOLVED";
    m_current_case.charged_name   = "Sarah Chen (E-101) -- Lead Engineer";
    m_current_case.accessory_name = "David Kim (E-103) (IT accomplice & cover-up)";
    m_current_case.resolution_text = 
        "ApexTech wired $500,000 to a dummy account (Shell-77) to purchase the proprietary "
        "Bluebird prototype. Sarah Chen accepted the bribe, keeping $450,000 and paying a $50,000 "
        "cut to David Kim in IT to cover her tracks.\n\n"
        "At 14:22, Kim confirmed he would run a 'SYS_WIPE' during the graveyard shift. "
        "Using a GhostProtocol VPN session masking as IP 45.22.11.9, Chen pulled the 1500MB "
        "payload directly from the secure servers at 03:45.\n\n"
        "When the massive data spike triggered a firewall security alert, Kim manually "
        "resolved the warning at 03:50 and wiped the system logs as planned.\n\n"
        "The data is already in ApexTech's hands, but the internal leak has been permanently plugged.";
    m_db.seed_espionage(m_current_case);

    // ── Tables ──
    m_tables = {
        {"project_files",   "PROJECT FILES",      true,  "", {}, "📁"},
        {"employees",       "ROSTER",             true,  "", {}, "👤"},
        {"network_logs",    "NETWORK TRAFFIC",    true,  "", {}, "📡"},
        {"security_alerts", "SECURITY ALERTS",    false, "Find the 'ANOMALY' in network_logs", {}, "🚨"},
        {"vpn_registry",    "VPN REGISTRY",       false, "Identify who resolved the firewall alert", {}, "🔐"},
        {"emails",          "EMAILS",             false, "Trace the VPN IP to a specific employee", {}, "✉"},
        {"bank_transfers",  "FINANCIALS",         false, "Find the payment confirmation in emails", {}, "$"},
        {"decrypted_emails","DECRYPTED COMMS",    false, "Find the $500,000 ApexTech wire transfer", {}, "⚿"},
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

    // ── Clues ──
    m_clues.clear();

    {
        Clue c; c.id = "c1"; c.title = "The 1.5GB Exfiltration";
        c.hint = "Check network_logs for any flagged anomalies or large transfers.";
        c.description = "A massive 1500MB transfer was executed at 03:45 to IP 45.22.11.9. This matches the exact size of the Bluebird prototype.";
        c.condition.sql_must_contain = {"NETWORK_LOGS", "ANOMALY"};
        c.condition.result_must_contain_cell = "1500";
        m_clues.push_back(c);
    }
    {
        Clue c; c.id = "c2"; c.title = "The IT Cover-Up";
        c.hint = "Check security_alerts. Who cleared the firewall warning at 03:45?";
        c.description = "David Kim (E-103) manually resolved the massive data spike alert and wiped the system logs at 03:50 to hide the tracks.";
        c.condition.sql_must_contain = {"SECURITY_ALERTS", "E-103"};
        c.condition.result_must_contain_cell = "E-103";
        m_clues.push_back(c);
    }
    {
        Clue c; c.id = "c3"; c.title = "The Ghost Protocol";
        c.hint = "Query vpn_registry. Who was assigned the IP 45.22.11.9 during the leak?";
        c.description = "The anomalous IP belongs to a 'GhostProtocol' VPN session initiated by E-101 (Sarah Chen). She pulled the data.";
        c.condition.sql_must_contain = {"VPN_REGISTRY", "45.22.11.9"};
        c.condition.result_must_contain_cell = "E-101";
        m_clues.push_back(c);
    }
    {
        Clue c; c.id = "c4"; c.title = "Pre-Planned Wipe";
        c.hint = "Check emails between Sarah and David before the incident.";
        c.description = "At 14:22, David confirmed to Sarah that he would run a 'SYS_WIPE' at 03:50. They planned the cover-up hours in advance.";
        c.condition.sql_must_contain = {"EMAILS", "E-103"};
        c.condition.result_must_contain_cell = "SYS_WIPE";
        m_clues.push_back(c);
    }
    {
        Clue c; c.id = "c5"; c.title = "ApexTech Payout";
        c.hint = "In bank_transfers, look for massive wires from competitors.";
        c.description = "ApexTech wired $500,000 to a dummy account (Shell-77) just minutes after the data was successfully exfiltrated.";
        c.condition.sql_must_contain = {"BANK_TRANSFERS", "APEXTECH"};
        c.condition.result_must_contain_cell = "Shell-77";
        m_clues.push_back(c);
    }
    {
        Clue c; c.id = "c6"; c.title = "Splitting the Bribe";
        c.hint = "Trace where Shell-77 sent the money in bank_transfers.";
        c.description = "The $500k bribe was split: $450k forwarded directly to Sarah Chen (E-101), and a $50k cut sent to David Kim (E-103) for the log wipe.";
        c.condition.sql_must_contain = {"BANK_TRANSFERS", "SHELL-77"};
        c.condition.result_must_contain_cell = "E-103";
        m_clues.push_back(c);
    }
    {
        Clue c; c.id = "c7"; c.title = "The Bird Has Flown";
        c.hint = "Look at emails from EXT-1 right after the transfer.";
        c.description = "At 04:10, the ApexTech rep confirmed receipt: 'The bird has flown. Funds transferred.' The timeline perfectly matches the bank wire.";
        c.condition.sql_must_contain = {"EMAILS", "EXT-1"};
        c.condition.result_must_contain_cell = "The bird has flown";
        m_clues.push_back(c);
    }
    {
        Clue c; c.id = "c8"; c.title = "The Smoking Gun";
        c.hint = "Read the decrypted_emails table. Only one message was encrypted.";
        c.description = "Sarah's decrypted message to ApexTech: 'I am initiating the 1.5GB transfer of Bluebird tonight... David is handling the log wipes.' Absolute proof of collusion.";
        c.condition.sql_must_contain = {"DECRYPTED_EMAILS"};
        c.condition.result_must_contain_cell = "David is handling";
        m_clues.push_back(c);
    }

    m_app.status = GameStatus::ACTIVE;

    push_narrative(NarrativeType::SYSTEM, "FORENSICS TERMINAL v4.7 — CASE: PROJECT BLUEBIRD — INITIALISED");
    push_narrative(NarrativeType::MONOLOGUE, "The Project Bluebird prototype was stolen from the R&D server at 03:45 AM.");
    push_narrative(NarrativeType::MONOLOGUE, "An external competitor is paying millions for it. We need to find the internal leak.");
    push_narrative(NarrativeType::HINT, "Start with network_logs. Look for data spikes that match the size of the prototype (1500MB).");
}

