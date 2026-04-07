#pragma once
#include "ICase.h"

// OrionCase inherits from ICase
class OrionCase : public ICase {
public:
    void init(GameState& state) override {
        // 1. Set the Case Narrative Data
        Case c;
        c.id = "orion";
        c.title = "THE ORION MURDER";
        c.help_objective = "Collect at least 5 pieces of evidence including badge tampering and the pre-murder message. Then click ACCUSE and name the killer.";
        c.accusation_prompt = "You have enough evidence. Type the full name of the person you believe murdered Marcus Orion. A wrong accusation costs you.";
        c.resolved_title = "THE ORION MURDER -- RESOLVED";
        c.charged_name   = "Lena Park -- Senior Data Analyst";
        c.accessory_name = "Hana Mori (ordered the act)";
        c.resolution_text = 
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

        state.set_current_case(c);

        // 2. Set up the Tables
        state.tables().clear();
        state.tables() = {
            {"victims",           "VICTIM FILE",        true,  "", {}, "◈"},
            {"suspects",          "SUSPECTS",           true,  "", {}, "?"},
            {"access_logs",       "ACCESS LOGS",        true,  "", {}, "⊡"},
            {"badge_registry",    "BADGE REGISTRY",     false, "Find which badge IDs appear in access_logs at night", {}, "⊕"},
            {"messages",          "MESSAGES",           false, "Cross-reference badge_registry: who modified MASTER?", {}, "✉"},
            {"transactions",      "TRANSACTIONS",       false, "Find Lena's 22:15 message — then finance opens", {}, "$"},
            {"incident_reports",  "INCIDENT REPORTS",   false, "Trace the Mori→Park transaction in transactions", {}, "⚑"},
            {"decrypted_messages","DECRYPTED MSGS",     false, "Find key 'BLACK-OMEGA-7' in incident_reports", {}, "⚿"},
        };

        // 3. Set up the Clues
        state.clues().clear();

        Clue c1;
        c1.id    = "c1";
        c1.title = "Finance Suite Anomaly";
        c1.hint  = "Check access_logs for any flagged entries. Not all rows are equal.";
        c1.description = "Marcus Orion's badge A-001 was flagged ANOMALY entering the Finance Suite at 14:55 — the day of his death. He saw something in there he wasn't supposed to.";
        c1.condition.sql_must_contain    = {"ACCESS_LOGS", "FLAG"};
        c1.condition.result_must_contain_cell = "ANOMALY";
        state.clues().push_back(c1);

        Clue c2;
        c2.id    = "c2";
        c2.title = "The Server Room Meeting";
        c2.hint  = "Filter access_logs to zone 'Server Room B-7' after 22:00. Who went in — and in what order?";
        c2.description = "Lena Park (L-019) entered Server Room B-7 at 22:30. Marcus followed one minute later at 22:31. She set up the meeting. He walked in unaware.";
        c2.condition.sql_must_contain    = {"ACCESS_LOGS", "SERVER ROOM"};
        c2.condition.result_must_contain_cell = "22:30";
        state.clues().push_back(c2);

        Clue c3;
        c3.id    = "c3";
        c3.title = "MASTER Badge Tampered";
        c3.hint  = "The 02:14 OVERRIDE used badge 'MASTER'. Query badge_registry for that badge_id specifically.";
        c3.description = "MASTER badge was last modified on Jan 19th — by badge L-019 (Lena Park). She has no admin rights. She gave herself override access two months before the murder.";
        c3.condition.sql_must_contain    = {"BADGE_REGISTRY", "MASTER"};
        c3.condition.result_must_contain_cell = "L-019";
        state.clues().push_back(c3);

        Clue c4;
        c4.id    = "c4";
        c4.title = "Lena's 22:15 Message";
        c4.hint  = "Filter messages WHERE sender = 'Lena Park'. Pay attention to the timestamp relative to 22:30.";
        c4.description = "At 22:15 — 15 minutes before the meeting — Lena Park messaged an external contact: 'Tonight is the only chance to stop this. Proceeding.' She planned it before she walked in.";
        c4.condition.sql_must_contain    = {"MESSAGES", "LENA PARK"};
        c4.condition.result_must_contain_cell = "Proceeding";
        state.clues().push_back(c4);

        Clue c5;
        c5.id    = "c5";
        c5.title = "The Payoff: Mori to Park";
        c5.hint  = "In transactions, filter WHERE from_acct LIKE '%Mori%' or WHERE category='Personal'. Follow the personal money, not the payroll.";
        c5.description = "Hana Mori paid $12,000 from her personal account to Lena Park on March 1st — 15 days before the murder. Park cashed it out 9 days later. This is not a bonus. This is a contract.";
        c5.condition.sql_must_contain    = {"TRANSACTIONS"};
        c5.condition.sql_must_not_contain= {};
        c5.condition.result_must_contain_cell = "Mori-Personal";
        state.clues().push_back(c5);

        Clue c6;
        c6.id    = "c6";
        c6.title = "The Fraud: $1.35M to Calloway";
        c6.hint  = "In transactions, GROUP BY from_acct, to_acct or filter WHERE category='Vendor-Consulting'. The payroll rows are noise.";
        c6.description = "NovaCorp paid ExtShell-LLC $1.35M across three invoices — all approved solely by Mori. ExtShell immediately forwarded it to Rex Calloway's private account. Marcus found this. That's why he had to die.";
        c6.condition.sql_must_contain    = {"TRANSACTIONS"};
        c6.condition.result_must_contain_cell = "RCCalloway-Private";
        state.clues().push_back(c6);

        Clue c7;
        c7.id    = "c7";
        c7.title = "Buried IT Report";
        c7.hint  = "Filter incident_reports WHERE filed_by = 'Carl Bremer' or WHERE subject LIKE '%badge%'. Someone saw this coming.";
        c7.description = "Carl Bremer filed a report in January: badge L-019 modified the MASTER credential without admin rights. The report was closed immediately — by Elena Vasquez. She buried it.";
        c7.condition.sql_must_contain    = {"INCIDENT_REPORTS"};
        c7.condition.result_must_contain_cell = "Carl Bremer";
        state.clues().push_back(c7);

        Clue c8;
        c8.id    = "c8";
        c8.title = "Mori's Order: 'Park is handling it'";
        c8.hint  = "Read the decrypted_messages table. Filter by sender = 'Hana Mori'.";
        c8.description = "Decrypted message, Mori to Vasquez, 20:05: 'Park is handling it. She will meet him in B-7 at 22:30. MASTER key is hers to use.' Mori ordered the murder. Park carried it out.";
        c8.condition.sql_must_contain    = {"DECRYPTED_MESSAGES"};
        c8.condition.result_must_contain_cell = "Park is handling it";
        state.clues().push_back(c8);

        // 4. Set App Status & Typewriter Narratives
        state.app().status = GameStatus::ACTIVE;
        state.push_narrative(NarrativeType::SYSTEM, "FORENSICS TERMINAL v4.7 — CASE: ORION MURDER — INITIALISED");
        state.push_narrative(NarrativeType::SYSTEM, "Type SCHEMA in the console at any time to inspect table structures.");
        state.push_narrative(NarrativeType::MONOLOGUE, "Marcus Orion. 42. Found dead in Server Room B-7 at 02:14.");
        state.push_narrative(NarrativeType::MONOLOGUE, "Five people were in the building that night. The data has no reason to lie.");
        state.push_narrative(NarrativeType::HINT, "Start with access_logs. Filter to that night. Look for patterns — not just names.");
    
    }

    
    // ─────────────────────────────────────────────────────────────────────────
    // 2. DATABASE SCRIPT (The data for the engine to load)
    // ─────────────────────────────────────────────────────────────────────────
    std::string get_sql_script() override {
        // Return ONE giant string containing all the SQL for the engine to execute!
        return R"SQL(
            CREATE TABLE IF NOT EXISTS victims (
                id             INTEGER PRIMARY KEY,
                name           TEXT,
                age            INTEGER,
                job_title      TEXT,
                department     TEXT,
                time_of_death  TEXT,
                location_found TEXT,
                cause_of_death TEXT,
                notes          TEXT
            );
            CREATE TABLE IF NOT EXISTS suspects (
                id                 INTEGER PRIMARY KEY,
                name               TEXT,
                age                INTEGER,
                job_title          TEXT,
                department         TEXT,
                badge_id           TEXT,
                alibi              TEXT,
                alibi_status       TEXT,
                motive_flag        TEXT,
                last_building_exit TEXT
            );
            CREATE TABLE IF NOT EXISTS access_logs (
                id       INTEGER PRIMARY KEY,
                ts       TEXT,
                badge_id TEXT,
                person   TEXT,
                zone     TEXT,
                action   TEXT,
                flag     TEXT
            );
            CREATE TABLE IF NOT EXISTS badge_registry (
                badge_id      TEXT PRIMARY KEY,
                issued_to     TEXT,
                access_level  INTEGER,
                zones         TEXT,
                last_modified TEXT,
                modified_by   TEXT,
                status        TEXT
            );
            CREATE TABLE IF NOT EXISTS messages (
                id        INTEGER PRIMARY KEY,
                ts        TEXT,
                sender    TEXT,
                recipient TEXT,
                body      TEXT,
                encrypted INTEGER DEFAULT 0
            );
            CREATE TABLE IF NOT EXISTS transactions (
                id          INTEGER PRIMARY KEY,
                ts          TEXT,
                ref         TEXT,
                from_acct   TEXT,
                to_acct     TEXT,
                amount      REAL,
                approved_by TEXT,
                category    TEXT
            );
            CREATE TABLE IF NOT EXISTS decrypted_messages (
                id          INTEGER PRIMARY KEY,
                orig_msg_id INTEGER,
                ts          TEXT,
                sender      TEXT,
                recipient   TEXT,
                body        TEXT
            );
            CREATE TABLE IF NOT EXISTS incident_reports (
                id         INTEGER PRIMARY KEY,
                filed_date TEXT,
                filed_by   TEXT,
                subject    TEXT,
                body       TEXT,
                status     TEXT,
                closed_by  TEXT
            );

            INSERT INTO victims VALUES (
                1, 'Marcus Orion', 42, 'Senior Data Architect', 'Engineering', 
                '2047-03-16 02:14:00', 'Server Room B-7, NovaCorp HQ', 
                'Asphyxiation - manual strangulation', 
                'Personal tablet not found at scene. Was reviewing Q4 vendor ledger on the day of death.'
            );

            INSERT INTO suspects VALUES
                (1,'Elena Vasquez',35,'Head of Security','Security','S-007','Claims to have been home all evening','UNVERIFIED','controls_building_security','2047-03-16 00:16:00'),
                (2,'Dorian Kast',29,'Junior Developer','Engineering','D-042','Gym session FitCore 22:00-23:30','PARTIAL','promotion_blocked_by_victim','2047-03-15 21:47:00'),
                (3,'Hana Mori',44,'Chief Financial Officer','Finance','M-003','Client dinner until 21:00 at Meridian Hotel','PARTIAL','approved_fraudulent_payments','2047-03-15 21:00:00'),
                (4,'Rex Calloway',51,'External Contractor','Vendor','RC-EXT','Hotel Grand Hyatt check-in 20:00','PARTIAL','received_fraudulent_payments','2047-03-14 17:30:00'),
                (5,'Lena Park',31,'Senior Data Analyst','Engineering','L-019','Working late then went home - no witnesses','UNVERIFIED','ex_partner_paid_by_mori','2047-03-16 00:16:00');

            INSERT INTO access_logs VALUES
                (1, '2047-03-15 07:52','M-003','Hana Mori',      'Main Lobby',      'ENTRY',    NULL),
                (2, '2047-03-15 08:04','S-007','Elena Vasquez',  'Security Office', 'ENTRY',    NULL),
                (3, '2047-03-15 08:11','A-001','Marcus Orion',   'Main Lobby',      'ENTRY',    NULL),
                (4, '2047-03-15 08:15','L-019','Lena Park',      'Main Lobby',      'ENTRY',    NULL),
                (5, '2047-03-15 08:44','D-042','Dorian Kast',    'Main Lobby',      'ENTRY',    NULL),
                (6, '2047-03-15 10:15','A-001','Marcus Orion',   'Server Room B-7', 'ENTRY',    NULL),
                (7, '2047-03-15 10:58','A-001','Marcus Orion',   'Server Room B-7', 'EXIT',     NULL),
                (8, '2047-03-15 13:20','L-019','Lena Park',      'Server Room B-7', 'ENTRY',    NULL),
                (9, '2047-03-15 14:55','A-001','Marcus Orion',   'Finance Suite',   'ENTRY',    'ANOMALY'),
                (10,'2047-03-15 15:02','A-001','Marcus Orion',   'Finance Suite',   'EXIT',     NULL),
                (11,'2047-03-15 16:30','L-019','Lena Park',      'Server Room B-7', 'EXIT',     NULL),
                (12,'2047-03-15 18:30','D-042','Dorian Kast',    'Main Lobby',      'EXIT',     NULL),
                (13,'2047-03-15 19:45','A-001','Marcus Orion',   'Main Lobby',      'ENTRY',    NULL),
                (14,'2047-03-15 20:01','S-007','Elena Vasquez',  'Security Office', 'ENTRY',    NULL),
                (15,'2047-03-15 20:33','D-042','Dorian Kast',    'Main Lobby',      'ENTRY',    NULL),
                (16,'2047-03-15 21:00','M-003','Hana Mori',      'Main Lobby',      'EXIT',     NULL),
                (17,'2047-03-15 21:15','L-019','Lena Park',      'Main Lobby',      'ENTRY',    NULL),
                (18,'2047-03-15 21:47','D-042','Dorian Kast',    'Main Lobby',      'EXIT',     NULL),
                (19,'2047-03-15 22:30','L-019','Lena Park',      'Server Room B-7', 'ENTRY',    NULL),
                (20,'2047-03-15 22:31','A-001','Marcus Orion',   'Server Room B-7', 'ENTRY',    NULL),
                (21,'2047-03-15 23:58','S-007','Elena Vasquez',  'Server Room B-7', 'ENTRY',    'AFTER_HOURS'),
                (22,'2047-03-16 00:15','S-007','Elena Vasquez',  'Server Room B-7', 'EXIT',     NULL),
                (23,'2047-03-16 00:16','L-019','Lena Park',      'Server Room B-7', 'EXIT',     NULL),
                (24,'2047-03-16 02:14','MASTER','[SYSTEM]',      'Server Room B-7', 'OVERRIDE', 'CRITICAL');

            INSERT INTO badge_registry VALUES
                ('A-001',  'Marcus Orion',  3, 'Main,Engineering,Finance,ServerRooms', '2046-01-10', 'C-055', 'ACTIVE'),
                ('S-007',  'Elena Vasquez', 5, 'ALL-ZONES',                            '2047-03-01', 'C-055', 'ACTIVE'),
                ('D-042',  'Dorian Kast',   1, 'Main,Engineering',                     '2045-01-05', 'C-055', 'ACTIVE'),
                ('M-003',  'Hana Mori',     4, 'Main,Finance,Executive,ServerRooms',   '2046-09-15', 'C-055', 'ACTIVE'),
                ('RC-EXT', 'Rex Calloway',  0, 'Visitor',                              '2047-01-15', 'C-055', 'ACTIVE'),
                ('L-019',  'Lena Park',     2, 'Main,Engineering,ServerRooms',         '2047-02-28', 'C-055', 'ACTIVE'),
                ('C-055',  'Carl Bremer',   3, 'Main,IT,ServerRooms',                  '2046-03-15', 'C-055', 'ACTIVE'),
                ('MASTER', 'Elena Vasquez', 9, 'ALL-ZONES-OVERRIDE',                   '2047-01-19', 'L-019', 'ACTIVE');

            INSERT INTO messages VALUES
                (1, '2047-03-14 09:15','Marcus Orion','Lena Park', 'I found something weird in the Q4 vendor export. Routing anomalies. Need your eyes on it.',0),
                (2, '2047-03-14 09:22','Lena Park','Marcus Orion', 'Send me the file. I will look tonight.',0),
                (3, '2047-03-14 18:22','Marcus Orion','Lena Park', 'Meet me in B-7 at 22:30. Come alone. Do not tell Mori.',0),
                (4, '2047-03-14 18:45','Lena Park','Marcus Orion', 'Understood. See you there.',0),
                (5, '2047-03-15 08:50','Dorian Kast','Marcus Orion', 'Can we reschedule the promotion review? I have new code to show you.',0),
                (6, '2047-03-15 09:05','Marcus Orion','Dorian Kast', 'Not this week.',0),
                (7, '2047-03-15 09:10','Hana Mori','Elena Vasquez', '[ENCRYPTED]',1),
                (8, '2047-03-15 09:45','Elena Vasquez','Hana Mori', '[ENCRYPTED]',1),
                (9, '2047-03-15 20:05','Hana Mori','Elena Vasquez', '[ENCRYPTED]',1),
                (10,'2047-03-15 22:15','Lena Park','unknown_ext', 'He has the Q4 files. Tonight is the only chance to stop this. Proceeding.',0),
                (11,'2047-03-12 14:00','Rex Calloway','Hana Mori', 'Transfer confirmed. Arrangement holds. Next payment Friday as agreed.',0),
                (12,'2047-03-15 22:10','Dorian Kast','friend_ext', 'Marcus blocked my raise again. Three times now. I am done.',0);

            INSERT INTO transactions VALUES
                (1, '2047-01-05','TXN-0105-A','NovaCorp-Operations','ExtShell-LLC',        450000,'H.Mori','Vendor-Consulting'),
                (2, '2047-01-06','TXN-0106-A','ExtShell-LLC',       'RCCalloway-Private',  440000,'AUTO',  'Vendor-Consulting'),
                (3, '2047-02-03','TXN-0203-A','NovaCorp-Operations','ExtShell-LLC',        450000,'H.Mori','Vendor-Consulting'),
                (4, '2047-02-04','TXN-0204-A','ExtShell-LLC',       'RCCalloway-Private',  440000,'AUTO',  'Vendor-Consulting'),
                (5, '2047-03-01','TXN-0301-A','NovaCorp-Operations','ExtShell-LLC',        450000,'H.Mori','Vendor-Consulting'),
                (6, '2047-03-02','TXN-0302-A','ExtShell-LLC',       'RCCalloway-Private',  440000,'AUTO',  'Vendor-Consulting'),
                (7, '2047-03-01','TXN-0301-B','Mori-Personal',      'LPark-Account',        12000,'SELF',  'Personal'),
                (8, '2047-03-10','TXN-0310-A','LPark-Account',      'ATM-CASH-4481',        12000,'AUTO',  'Personal'),
                (9, '2047-03-10','TXN-0310-B','ExtShell-LLC',       'Cayman-7712',         420000,'AUTO',  'Wire'),
                (10,'2047-03-11','TXN-0311-A','Cayman-7712',        'H.Mori-Offshore',     415000,'AUTO',  'Wire'),
                (11,'2047-01-15','PAY-0115',  'NovaCorp-Payroll',   'Marcus-Orion-Salary',  12500,'AUTO',  'Payroll'),
                (12,'2047-02-15','PAY-0215',  'NovaCorp-Payroll',   'Marcus-Orion-Salary',  12500,'AUTO',  'Payroll'),
                (13,'2047-01-15','PAY-0115',  'NovaCorp-Payroll',   'LPark-Salary',          9800,'AUTO',  'Payroll'),
                (14,'2047-02-15','PAY-0215',  'NovaCorp-Payroll',   'LPark-Salary',          9800,'AUTO',  'Payroll'),
                (15,'2047-01-15','PAY-0115',  'NovaCorp-Payroll',   'HMori-Salary',         38000,'AUTO',  'Payroll'),
                (16,'2047-02-15','PAY-0215',  'NovaCorp-Payroll',   'HMori-Salary',         38000,'AUTO',  'Payroll');

            INSERT INTO decrypted_messages VALUES
                (1, 7, '2047-03-15 09:10','Hana Mori','Elena Vasquez', 'Orion accessed something in Finance yesterday. He cannot be allowed to share those files. Watch him tonight.'),
                (2, 8, '2047-03-15 09:45','Elena Vasquez','Hana Mori', 'Understood. I will monitor. If it comes to it, the MASTER credential is available. But I want this in writing from you.'),
                (3, 9, '2047-03-15 20:05','Hana Mori','Elena Vasquez', 'Park is handling it. She will meet him in B-7 at 22:30. MASTER key is hers to use. This conversation does not exist.');

            INSERT INTO incident_reports VALUES
                (1,'2047-02-10','Sandra Obi', 'Ledger discrepancy - ExtShell-LLC payments', 'Q4 vendor invoices from ExtShell-LLC total $1.35M with no deliverables logged. Three invoices approved solely by CFO Mori. No secondary sign-off.', 'CLOSED','H.Mori'),
                (2,'2047-01-20','Carl Bremer', 'Unauthorised badge modification - MASTER credential', 'Badge MASTER had zone permissions elevated at 23:44 on Jan 19th. Modification logged under badge L-019. L-019 (Lena Park) has no admin rights to modify MASTER-level credentials. Encryption key for internal audit log: BLACK-OMEGA-7', 'CLOSED','E.Vasquez'),
                (3,'2047-03-05','Marcus Orion', 'Attempted Finance Suite data export - badge A-001', 'Someone used my credentials to attempt a full export of the vendor ledger at 14:55. Access was denied. I did not initiate this. Requesting review.', 'OPEN', NULL),
                (4,'2047-03-15','Elena Vasquez', 'After-hours MASTER badge use - Server Room B-7', 'Auto-flag: MASTER badge used at 02:14 in Server Room B-7. No authorised personnel on night shift.', 'OPEN', NULL);
        )SQL";
    }


    virtual void check_unlocks(GameState& state, const std::string& sql, const QueryResult& result) override {
    std::string up = GameState::upper(sql);


    // badge_registry unlocks when player queries access_logs
    if (GameState::sql_has(up,"ACCESS_LOGS") &&
        (GameState::result_has_cell(result,"MASTER") || GameState::result_has_cell(result,"AFTER_HOURS")
         || GameState::result_has_cell(result,"CRITICAL") || GameState::result_has_cell(result,"ANOMALY")))
        state.unlock_table("badge_registry");

    // messages unlocks when player queries badge_registry and finds L-019
    if (GameState::sql_has(up,"BADGE_REGISTRY") &&
        GameState::result_has_cell(result,"L-019") &&
        GameState::result_has_cell(result,"MASTER"))
        state.unlock_table("messages");

    // transactions unlocks when player finds Lena's 22:15 message
    if (GameState::sql_has(up,"MESSAGES") && !GameState::sql_has(up,"DECRYPTED") &&
        GameState::result_has_cell(result,"Proceeding"))
        state.unlock_table("transactions");

    // incident_reports unlocks when player finds the Mori->Park transaction
    if (GameState::sql_has(up,"TRANSACTIONS") &&
        GameState::result_has_cell(result,"Mori-Personal"))
        state.unlock_table("incident_reports");

    // decrypted_messages unlocks ONLY when player finds the key "BLACK-OMEGA-7"
    if ((GameState::sql_has(up,"INCIDENT_REPORTS") && GameState::result_has_cell(result,"BLACK-OMEGA-7")) ||
        GameState::sql_has(up,"BLACK-OMEGA-7"))
        state.unlock_table("decrypted_messages");
    }

    void add_context_narrative(GameState& state, const std::string& sql, const QueryResult& result) override {
        std::string up = GameState::upper(sql);
        int rows = (int)result.rows.size();

        // Check if the player is doing a basic SELECT * without filtering
        bool is_naked_select = GameState::sql_has(up,"SELECT *") && !GameState::sql_has(up,"WHERE") &&
                               !GameState::sql_has(up,"JOIN") && !GameState::sql_has(up,"GROUP BY") &&
                               !GameState::sql_has(up,"HAVING");

        if (GameState::sql_has(up,"ACCESS_LOGS")) {
            if (is_naked_select && rows > 15)
                state.push_narrative(NarrativeType::HINT,
                    "You're seeing all "+std::to_string(rows)+" rows. "
                    "Narrow it — filter by zone, action, or flag. "
                    "You're looking for something specific.");
            else if (GameState::sql_has(up,"FLAG") && GameState::result_has_cell(result,"ANOMALY"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "ANOMALY. Orion was flagged in the Finance Suite. "
                    "What was he looking at in there?");
            else if (GameState::result_has_cell(result,"CRITICAL"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "02:14. CRITICAL override. MASTER badge. "
                    "That's not an access card — it's a skeleton key. Who controls it?");
            else if (GameState::result_has_cell(result,"22:30"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "22:30 — Lena Park. 22:31 — Marcus Orion. "
                    "One minute apart. Same room. She was already there waiting.");
        }

        if (GameState::sql_has(up,"BADGE_REGISTRY")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "8 badge records. Find the MASTER badge specifically. "
                    "Try: WHERE badge_id = 'MASTER'");
            else if (GameState::result_has_cell(result,"L-019") && GameState::result_has_cell(result,"MASTER"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "L-019 modified the MASTER badge. "
                    "L-019 is Lena Park. She has no admin rights. "
                    "How did she do this — and why did nobody stop it?");
        }

        if (GameState::sql_has(up,"MESSAGES") && !GameState::sql_has(up,"DECRYPTED")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "12 messages — half are noise, three are encrypted. "
                    "Filter by sender or timestamp. "
                    "Try: WHERE sender = 'Lena Park'  or  WHERE encrypted = 0");
            else if (GameState::result_has_cell(result,"[ENCRYPTED]"))
                state.push_narrative(NarrativeType::HINT,
                    "Encrypted messages. You need a key. "
                    "It's not in this table — it's buried in the paper trail.");
            else if (GameState::result_has_cell(result,"Proceeding"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "22:15. 'Tonight is the only chance to stop this. Proceeding.' "
                    "Fifteen minutes before the meeting. "
                    "She had already decided.");
        }

        if (GameState::sql_has(up,"TRANSACTIONS")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "16 rows — mostly payroll. "
                    "Filter it: WHERE category = 'Vendor-Consulting'  or  WHERE from_acct LIKE '%Mori%'. "
                    "The interesting rows are buried.");
            else if (GameState::result_has_cell(result,"RCCalloway-Private"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "ExtShell-LLC, then straight to Calloway's private account. "
                    "Approved by Mori. $1.35 million across six months. "
                    "Marcus found this ledger. That's what got him killed.");
            else if (GameState::result_has_cell(result,"Mori-Personal"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "Mori-Personal to LPark-Account. $12,000. March 1st. "
                    "'Personal' category. Park cashed it 9 days later. "
                    "This is a payment, not a bonus.");
        }

        if (GameState::sql_has(up,"INCIDENT_REPORTS")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "4 reports. One of them has something hidden inside it. "
                    "Try: WHERE filed_by = 'Carl Bremer'");
            else if (GameState::result_has_cell(result,"BLACK-OMEGA-7"))
                state.push_narrative(NarrativeType::SUCCESS,
                    "BLACK-OMEGA-7. That's the decryption key for the encrypted messages. "
                    "Use it: SELECT * FROM decrypted_messages;");
            else if (GameState::result_has_cell(result,"E.Vasquez"))
                state.push_narrative(NarrativeType::MONOLOGUE,
                    "Vasquez closed the report. She buried the evidence "
                    "of Lena Park tampering with the MASTER badge.");
        }

        if (GameState::sql_has(up,"DECRYPTED_MESSAGES")) {
            state.push_narrative(NarrativeType::MONOLOGUE,
                "Read them in order. Three messages. The last one is the order.");
        }

        if (GameState::sql_has(up,"SUSPECTS")) {
            if (is_naked_select)
                state.push_narrative(NarrativeType::HINT,
                    "Five suspects. Check alibi_status — two say UNVERIFIED. "
                    "The logs can confirm or deny every alibi.");
        }
        
        if (GameState::sql_has(up,"JOIN")) {
            state.push_narrative(NarrativeType::MONOLOGUE,
                "Good. Connecting tables is how you see what doesn't fit.");
        }
    }

    
    bool try_accuse(GameState& state, const std::string& name) override {
        // Note: The 'name' string passed in is ALREADY uppercase and trimmed of spaces 
        // because the GameState engine did the cleanup before handing it to us!

        // Correct answer: Lena Park (accept variations)
        if (name == "LENA PARK" || name == "LENA" || name == "L-019") {
            state.app().status = GameStatus::CASE_SOLVED;
            state.push_narrative(NarrativeType::SUCCESS,
                "CASE CLOSED. Lena Park. Charged with first-degree murder.");
            return true;
        }

        // Wrong — give specific reactive feedback
        state.app().accuse.wrong_timer = 4.f;

        if (name == "HANA MORI" || name == "MORI") {
            state.app().accuse.wrong_feedback =
                "Mori ordered it — but didn't pull the trigger. "
                "The access logs put someone else in that room.";
        }
        else if (name == "ELENA VASQUEZ" || name == "VASQUEZ") {
            state.app().accuse.wrong_feedback =
                "Vasquez received orders and arrived at 23:58 — but Lena Park "
                "was already in the room since 22:30. Check who exited when.";
        }
        else if (name == "DORIAN KAST" || name == "KAST" || name == "DORIAN") {
            state.app().accuse.wrong_feedback =
                "Kast left the building at 21:47. His alibi is partial but "
                "he had no motive strong enough — and no access to MASTER.";
        }
        else if (name == "REX CALLOWAY" || name == "CALLOWAY" || name == "REX") {
            state.app().accuse.wrong_feedback =
                "Calloway is dirty — but he was at the hotel. "
                "He's the beneficiary, not the one who did this.";
        }
        else {
            // A generic wrong guess
            state.app().accuse.wrong_feedback =
                "'" + name + "' is not a match. Review your evidence. "
                "You need to be certain.";
        }

        // Push the alert to the screen
        state.push_narrative(NarrativeType::ALERT,
            "Wrong. " + state.app().accuse.wrong_feedback);
            
        return false;
    }

    std::vector<std::string> get_flag_keywords() override {
        return {
            "ANOMALY", "CRITICAL", "AFTER_HOURS", "OVERRIDE",
            "BLACK-OMEGA-7",
            "[ENCRYPTED]",
            "Mori-Personal",        // specific account name
            "RCCalloway-Private",   // specific account name
            "Cayman-7712",
            "Park is handling it",  // direct quote from decrypted msg
            "MASTER key is hers",
            "cannot be allowed"
        };
    }


    bool can_accuse(const GameState& state) override {
        int found = 0; 
        bool has_c3 = false, has_c4 = false;
        
        for (auto& c : state.clues()) {
            if (c.discovered) { found++; }
            if (c.id == "c3" && c.discovered) has_c3 = true;
            if (c.id == "c4" && c.discovered) has_c4 = true;
        }
        // Orion needs 5 clues including 3 and 4
        return found >= 5 && has_c3 && has_c4;
    }

    virtual bool seed_data(Database& db) override {
        auto result = db.run_query(get_sql_script());
        return !result.is_error;
        
}

};