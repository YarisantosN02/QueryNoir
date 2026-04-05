#pragma once
#include "ICase.h"

class EspionageCase : public ICase {
public:

// ─────────────────────────────────────────────────────────────────────────
    // 1. INITIALIZATION (Setup the story, tables, and clues)
    // ─────────────────────────────────────────────────────────────────────────
    void init(GameState& state) override {
        // 1. Set the Case Narrative Data
        Case c;
        c.id = "espionage";
        c.title = "PROJECT BLUEBIRD";
        c.help_objective = "Identify the employee who leaked the Bluebird prototype. Find their accomplice in IT and follow the money. Collect 8 clues, then click ACCUSE.";
        c.accusation_prompt = "You have enough evidence. Type the full name of the employee who leaked Project Bluebird, or their IT accomplice. A wrong accusation costs you.";
        c.resolved_title = "PROJECT BLUEBIRD -- RESOLVED";
        c.charged_name   = "Sarah Chen (E-101) -- Lead Engineer";
        c.accessory_name = "David Kim (E-103) (IT accomplice & cover-up)";
        c.resolution_text = 
            "ApexTech wired $500,000 to a dummy account (Shell-77) to purchase the proprietary "
            "Bluebird prototype. Sarah Chen accepted the bribe, keeping $450,000 and paying a $50,000 "
            "cut to David Kim in IT to cover her tracks.\n\n"
            "At 14:22, Kim confirmed he would run a 'SYS_WIPE' during the graveyard shift. "
            "Using a GhostProtocol VPN session masking as IP 45.22.11.9, Chen pulled the 1500MB "
            "payload directly from the secure servers at 03:45.\n\n"
            "When the massive data spike triggered a firewall security alert, Kim manually "
            "resolved the warning at 03:50 and wiped the system logs as planned.\n\n"
            "The data is already in ApexTech's hands, but the internal leak has been permanently plugged.";

        state.set_current_case(c);

        // 2. Set up the Tables
        state.tables() = {
            {"project_files",   "PROJECT FILES",      true,  "", {}, "📁"},
            {"employees",       "ROSTER",             true,  "", {}, "👤"},
            {"network_logs",    "NETWORK TRAFFIC",    true,  "", {}, "📡"},
            {"security_alerts", "SECURITY ALERTS",    false, "Find the 'ANOMALY' in network_logs", {}, "🚨"},
            {"vpn_registry",    "VPN REGISTRY",       false, "Identify who resolved the firewall alert", {}, "🔐"},
            {"emails",          "EMAILS",             false, "Trace the VPN IP to a specific employee", {}, "✉"},
            {"bank_transfers",  "FINANCIALS",         false, "Find the payment confirmation in emails", {}, "$"},
            {"decrypted_emails","DECRYPTED COMMS",    false, "Find the $500,000 ApexTech wire transfer", {}, "⚿"},
        };

        // 3. Set up the Clues
        state.clues().clear();

        Clue c1; c1.id = "c1"; c1.title = "The 1.5GB Exfiltration";
        c1.hint = "Check network_logs for any flagged anomalies or large transfers.";
        c1.description = "A massive 1500MB transfer was executed at 03:45 to IP 45.22.11.9. This matches the exact size of the Bluebird prototype.";
        c1.condition.sql_must_contain = {"NETWORK_LOGS", "ANOMALY"};
        c1.condition.result_must_contain_cell = "1500";
        state.clues().push_back(c1);

        Clue c2; c2.id = "c2"; c2.title = "The IT Cover-Up";
        c2.hint = "Check security_alerts. Who cleared the firewall warning at 03:45?";
        c2.description = "David Kim (E-103) manually resolved the massive data spike alert and wiped the system logs at 03:50 to hide the tracks.";
        c2.condition.sql_must_contain = {"SECURITY_ALERTS", "E-103"};
        c2.condition.result_must_contain_cell = "E-103";
        state.clues().push_back(c2);

        Clue c3; c3.id = "c3"; c3.title = "The Ghost Protocol";
        c3.hint = "Query vpn_registry. Who was assigned the IP 45.22.11.9 during the leak?";
        c3.description = "The anomalous IP belongs to a 'GhostProtocol' VPN session initiated by E-101 (Sarah Chen). She pulled the data.";
        c3.condition.sql_must_contain = {"VPN_REGISTRY", "45.22.11.9"};
        c3.condition.result_must_contain_cell = "E-101";
        state.clues().push_back(c3);

        Clue c4; c4.id = "c4"; c4.title = "Pre-Planned Wipe";
        c4.hint = "Check emails between Sarah and David before the incident.";
        c4.description = "At 14:22, David confirmed to Sarah that he would run a 'SYS_WIPE' at 03:50. They planned the cover-up hours in advance.";
        c4.condition.sql_must_contain = {"EMAILS", "E-103"};
        c4.condition.result_must_contain_cell = "SYS_WIPE";
        state.clues().push_back(c4);

        Clue c5; c5.id = "c5"; c5.title = "ApexTech Payout";
        c5.hint = "In bank_transfers, look for massive wires from competitors.";
        c5.description = "ApexTech wired $500,000 to a dummy account (Shell-77) just minutes after the data was successfully exfiltrated.";
        c5.condition.sql_must_contain = {"BANK_TRANSFERS", "APEXTECH"};
        c5.condition.result_must_contain_cell = "Shell-77";
        state.clues().push_back(c5);

        Clue c6; c6.id = "c6"; c6.title = "Splitting the Bribe";
        c6.hint = "Trace where Shell-77 sent the money in bank_transfers.";
        c6.description = "The $500k bribe was split: $450k forwarded directly to Sarah Chen (E-101), and a $50k cut sent to David Kim (E-103) for the log wipe.";
        c6.condition.sql_must_contain = {"BANK_TRANSFERS", "SHELL-77"};
        c6.condition.result_must_contain_cell = "E-103";
        state.clues().push_back(c6);

        Clue c7; c7.id = "c7"; c7.title = "The Bird Has Flown";
        c7.hint = "Look at emails from EXT-1 right after the transfer.";
        c7.description = "At 04:10, the ApexTech rep confirmed receipt: 'The bird has flown. Funds transferred.' The timeline perfectly matches the bank wire.";
        c7.condition.sql_must_contain = {"EMAILS", "EXT-1"};
        c7.condition.result_must_contain_cell = "The bird has flown";
        state.clues().push_back(c7);

        Clue c8; c8.id = "c8"; c8.title = "The Smoking Gun";
        c8.hint = "Read the decrypted_emails table. Only one message was encrypted.";
        c8.description = "Sarah's decrypted message to ApexTech: 'I am initiating the 1.5GB transfer of Bluebird tonight... David is handling the log wipes.' Absolute proof of collusion.";
        c8.condition.sql_must_contain = {"DECRYPTED_EMAILS"};
        c8.condition.result_must_contain_cell = "David is handling";
        state.clues().push_back(c8);

        // 4. Set App Status & Typewriter Narratives
        state.app().status = GameStatus::ACTIVE;
        state.push_narrative(NarrativeType::SYSTEM, "FORENSICS TERMINAL v4.7 — CASE: PROJECT BLUEBIRD — INITIALISED");
        state.push_narrative(NarrativeType::MONOLOGUE, "The Project Bluebird prototype was stolen from the R&D server at 03:45 AM.");
        state.push_narrative(NarrativeType::MONOLOGUE, "An external competitor is paying millions for it. We need to find the internal leak.");
        state.push_narrative(NarrativeType::HINT, "Start with network_logs. Look for data spikes that match the size of the prototype (1500MB).");
    }

    std::string get_sql_script() override {
        // Return ONE giant string containing all the SQL for the engine to execute!
        return R"SQL(
        CREATE TABLE IF NOT EXISTS project_files (
            file_id     TEXT PRIMARY KEY,
            filename    TEXT,
            size_mb     INTEGER,
            clearance   TEXT,
            last_editor TEXT
        );
        CREATE TABLE IF NOT EXISTS employees (
            emp_id      TEXT PRIMARY KEY,
            name        TEXT,
            department  TEXT,
            clearance   TEXT,
            status      TEXT
        );
        CREATE TABLE IF NOT EXISTS network_logs (
            log_id      INTEGER PRIMARY KEY,
            ts          TEXT,
            ip_addr     TEXT,
            action      TEXT,
            bytes_mb    INTEGER,
            flag        TEXT
        );
        CREATE TABLE IF NOT EXISTS vpn_registry (
            emp_id      TEXT,
            provider    TEXT,
            assigned_ip TEXT,
            last_login  TEXT
        );
        CREATE TABLE IF NOT EXISTS emails (
            msg_id      INTEGER PRIMARY KEY,
            ts          TEXT,
            sender      TEXT,
            recipient   TEXT,
            subject     TEXT,
            body        TEXT,
            encrypted   INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS bank_transfers (
            txn_id      TEXT PRIMARY KEY,
            ts          TEXT,
            from_acct   TEXT,
            to_acct     TEXT,
            amount      REAL,
            currency    TEXT,
            flag        TEXT
        );
        CREATE TABLE IF NOT EXISTS decrypted_emails (
            id          INTEGER PRIMARY KEY,
            orig_msg_id INTEGER,
            body        TEXT
        );
        CREATE TABLE IF NOT EXISTS security_alerts (
            alert_id    INTEGER PRIMARY KEY,
            ts          TEXT,
            system      TEXT,
            description TEXT,
            resolved_by TEXT
        );

        INSERT INTO project_files VALUES 
            ('P-01', 'BLUEBIRD_PROTOTYPE_V1', 1500, 'LEVEL-5', 'Sarah Chen'),
            ('P-02', 'Q3_FINANCIALS_DRAFT',   12,   'LEVEL-3', 'David Kim'),
            ('P-03', 'HR_ROSTER_2047',        4,    'LEVEL-2', 'Marcus Vance');

        INSERT INTO employees VALUES
            ('E-101', 'Sarah Chen',    'R&D',     'LEVEL-5', 'ACTIVE'),
            ('E-102', 'Marcus Vance',  'Sales',   'LEVEL-2', 'ACTIVE'),
            ('E-103', 'David Kim',     'IT',      'LEVEL-4', 'ACTIVE'),
            ('E-104', 'Elena Vasquez', 'Legal',   'LEVEL-5', 'ACTIVE'),
            ('EXT-1', 'ApexTech_Rep',  'External','NONE',    'N/A');

        INSERT INTO network_logs VALUES
            (1, '2047-04-02 01:14', '192.168.1.15', 'LOGIN',      0,    'NORMAL'),
            (2, '2047-04-02 02:30', '192.168.1.22', 'DATA_PULL',  12,   'NORMAL'),
            (3, '2047-04-02 03:45', '45.22.11.9',   'DATA_XFER',  1500, 'ANOMALY'),
            (4, '2047-04-02 03:50', '192.168.1.55', 'SYS_WIPE',   0,    'WARNING'),
            (5, '2047-04-02 04:00', '192.168.1.15', 'LOGOUT',     0,    'NORMAL');

        INSERT INTO vpn_registry VALUES
            ('E-102', 'CorpNet',       '192.168.1.22', '2047-04-02 02:00'),
            ('E-101', 'GhostProtocol', '45.22.11.9',   '2047-04-02 03:40'),
            ('E-103', 'SafeSurf',      '192.168.1.55', '2047-04-02 03:48');

        INSERT INTO emails VALUES
            (1, '2047-04-01 09:00', 'E-101', 'E-103', 'Server Maint', 'Please clear the audit logs tonight.', 0),
            (2, '2047-04-01 14:22', 'E-103', 'E-101', 'Re: Maint', 'Will do. I will run the SYS_WIPE at 03:50.', 0),
            (3, '2047-04-01 18:30', 'E-101', 'EXT-1', 'Delivery', '[ENCRYPTED PAYLOAD]', 1),
            (4, '2047-04-02 04:10', 'EXT-1', 'E-101', 'Confirmation', 'The bird has flown. Funds transferred.', 0);

        INSERT INTO bank_transfers VALUES
            ('TX-991', '2047-04-01 10:00', 'NovaCorp', 'E-101', 5000,   'USD', 'PAYROLL'),
            ('TX-992', '2047-04-02 04:05', 'ApexTech', 'Shell-77', 500000, 'USD', 'WIRE'),
            ('TX-993', '2047-04-02 04:30', 'Shell-77', 'E-101', 450000, 'USD', 'WIRE_FWD'),
            ('TX-994', '2047-04-02 04:35', 'Shell-77', 'E-103', 50000,  'USD', 'WIRE_FWD');

        INSERT INTO decrypted_emails VALUES
            (1, 3, 'I am initiating the 1.5GB transfer of Bluebird tonight via GhostProtocol. David is handling the log wipes.');

        INSERT INTO security_alerts VALUES
            (101, '2047-04-02 03:45', 'FIREWALL', 'Massive outbound data spike detected (1500MB).', 'E-103'),
            (102, '2047-04-02 03:50', 'SYS_MON',  'Manual audit log wipe initiated.', 'E-103');
        )SQL";
    }

  
    void check_unlocks(GameState& state, const std::string& sql, const QueryResult& result) override {
        std::string up = GameState::upper(sql);

        // Unlock security_alerts when player finds the 1500MB anomaly
        if (GameState::sql_has(up, "NETWORK_LOGS") && GameState::result_has_cell(result, "1500"))
            state.unlock_table("security_alerts");

        // Unlock vpn_registry when player finds David Kim (E-103) clearing alerts
        if (GameState::sql_has(up, "SECURITY_ALERTS") && GameState::result_has_cell(result, "E-103"))
            state.unlock_table("vpn_registry");

        // Unlock emails when player finds Sarah Chen's (E-101) IP address
        if (GameState::sql_has(up, "VPN_REGISTRY") && GameState::result_has_cell(result, "E-101"))
            state.unlock_table("emails");

        // Unlock bank_transfers when player finds the SYS_WIPE email
        if (GameState::sql_has(up, "EMAILS") && GameState::result_has_cell(result, "SYS_WIPE"))
            state.unlock_table("bank_transfers");

        // Unlock decrypted_emails when player finds the ApexTech transfer
        if (GameState::sql_has(up, "BANK_TRANSFERS") && GameState::result_has_cell(result, "ApexTech"))
            state.unlock_table("decrypted_emails");
    }

 
    void add_context_narrative(GameState& state, const std::string& sql, const QueryResult& result) override {
        std::string up = GameState::upper(sql);
        int rows = (int)result.rows.size();
        
        bool is_naked_select = GameState::sql_has(up,"SELECT *") && !GameState::sql_has(up,"WHERE") &&
                               !GameState::sql_has(up,"JOIN") && !GameState::sql_has(up,"GROUP BY") &&
                               !GameState::sql_has(up,"HAVING");

        if (GameState::sql_has(up, "NETWORK_LOGS")) {
            if (is_naked_select && rows > 10)
                state.push_narrative(NarrativeType::HINT,
                    "Thousands of packets. Look for anomalies or massive transfers. Filter by action or bytes.");
            else if (GameState::result_has_cell(result, "1500"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "1500 MB. Exactly the size of the Bluebird prototype. Someone backed up a truck to the servers at 03:45.");
        }
        
        if (GameState::sql_has(up, "SECURITY_ALERTS")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "Check who was on duty to clear the alerts around the time of the leak.");
            else if (GameState::result_has_cell(result, "E-103"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "E-103 manually resolved the alert. That's David Kim in IT. He didn't just clear it, he wiped the logs.");
        }

        if (GameState::sql_has(up, "VPN_REGISTRY")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "Match the anomalous IP from the network logs to a specific session.");
            else if (GameState::result_has_cell(result, "45.22.11.9"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "IP 45.22.11.9 maps to E-101. Sarah Chen. She pulled the trigger, but David held the door open.");
        }

        if (GameState::sql_has(up, "EMAILS")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "Too much noise. Search for communications involving E-103 or specific keywords.");
            else if (GameState::result_has_cell(result, "SYS_WIPE"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "SYS_WIPE confirmed hours in advance. This wasn't opportunistic. It was highly coordinated.");
        }

        if (GameState::sql_has(up, "BANK_TRANSFERS")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "Filter by large amounts or unknown entities. Someone got paid.");
            else if (GameState::result_has_cell(result, "ApexTech"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "Half a million dollars from ApexTech. Corporate espionage confirmed. Follow the money trail from Shell-77 to see who got paid.");
        }

        // Shared Mechanics: We can leave this in both files because JOINs are universally good to reward!
        if (GameState::sql_has(up,"JOIN")) {
            state.push_narrative(NarrativeType::MONOLOGUE,
                "Good. Connecting tables is how you see what doesn't fit.");
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 5. ACCUSATION LOGIC (Sarah Chen or David Kim)
    // ─────────────────────────────────────────────────────────────────────────
    bool try_accuse(GameState& state, const std::string& name) override {
        // Name is already uppercase and trimmed by GameState!

        // Correct answers: Sarah Chen (Leaker) or David Kim (Accomplice)
        if (name == "SARAH CHEN" || name == "CHEN" || name == "E-101" || name == "SARAH") {
            state.app().status = GameStatus::CASE_SOLVED;
            state.push_narrative(NarrativeType::SUCCESS,
                "CASE CLOSED. Sarah Chen. Arrested for corporate espionage and grand larceny.");
            return true;
        } 
        else if (name == "DAVID KIM" || name == "KIM" || name == "E-103" || name == "DAVID") {
            state.app().status = GameStatus::CASE_SOLVED;
            state.push_narrative(NarrativeType::SUCCESS,
                "CASE CLOSED. David Kim. Arrested for accessory to espionage and tampering with evidence.");
            return true;
        }

        // Wrong answers (Specific feedback)
        state.app().accuse.wrong_timer = 4.f;

        if (name == "APEXTECH" || name == "APEX") {
            state.app().accuse.wrong_feedback = 
                "They bought the data, but you need to arrest the employee who sold it.";
        } 
        else if (name == "GHOSTPROTOCOL" || name == "GHOST") {
            state.app().accuse.wrong_feedback = 
                "That's the VPN session, not a person. Cross-reference the IP to an employee.";
        } 
        else {
            state.app().accuse.wrong_feedback =
                "'" + name + "' is not a match. Review the IP logs and bank transfers.";
        }

        state.push_narrative(NarrativeType::ALERT,
            "Wrong. " + state.app().accuse.wrong_feedback);
            
        return false;
    }

    std::vector<std::string> get_flag_keywords() override {
        return {
            "1500",             // The massive exfiltration
            "SYS_WIPE",         // The premeditated cover-up command
            "ApexTech",         // The rival company
            "Shell-77",         // The dummy account
            "GhostProtocol"     // The illicit VPN session
        };
    }

    bool can_accuse(const GameState& state) override {
        int found = 0;
        
        for (auto& c : state.clues()) {
            if (c.discovered) { found++; }
        }
        // Espionage needs all 8 clues to be found!
        return found >= 8;
    }
    
    virtual bool seed_data(Database& db) override {
        auto result = db.run_query(get_sql_script());
        return !result.is_error;
    }

};



