#include "Database.h"
#include <iostream>
#include <algorithm>
#include <cctype>

Database::Database() {}
Database::~Database() { close(); }

bool Database::open(const std::string& path) {
    close();
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] open failed: " << sqlite3_errmsg(m_db) << "\n";
        m_db = nullptr;
        return false;
    }
    return true;
}

void Database::close() {
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

struct CbData { QueryResult* r; bool hdr=false; };

int Database::row_callback(void* data, int argc, char** argv, char** cols) {
    auto* d = static_cast<CbData*>(data);
    if (!d->hdr) {
        for (int i=0;i<argc;i++) d->r->columns.push_back(cols[i]?cols[i]:"");
        d->hdr=true;
    }
    std::vector<std::string> row;
    for (int i=0;i<argc;i++) row.push_back(argv[i]?argv[i]:"NULL");
    d->r->rows.push_back(std::move(row));
    return 0;
}

QueryResult Database::execute(const std::string& sql) {
    QueryResult res;
    if (!m_db) { res.is_error=true; res.error="No database."; return res; }
    CbData cbd{&res};
    char* err=nullptr;
    if (sqlite3_exec(m_db,sql.c_str(),row_callback,&cbd,&err)!=SQLITE_OK) {
        res.is_error=true;
        res.error=err?err:"Unknown error";
        sqlite3_free(err);
    }
    return res;
}

std::vector<std::string> Database::get_table_names() {
    auto r=execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(!row.empty()) v.push_back(row[0]);
    return v;
}

std::vector<std::string> Database::get_column_names(const std::string& table) {
    auto r=execute("PRAGMA table_info("+table+");");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(row.size()>=2) v.push_back(row[1]);
    return v;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  CASE DATA — THE ORION MURDER
//  Much richer data. Many dead ends. Puzzle requires specific SQL to unlock.
// ═══════════════════════════════════════════════════════════════════════════════
bool Database::seed_case(const Case&) {

    // ── Schema ────────────────────────────────────────────────────────────────
    const char* schema = R"SQL(
    CREATE TABLE IF NOT EXISTS victims (
        id              INTEGER PRIMARY KEY,
        name            TEXT,
        age             INTEGER,
        department      TEXT,
        job_title       TEXT,
        hire_date       TEXT,
        time_of_death   TEXT,
        location_found  TEXT,
        cause_of_death  TEXT,
        personal_device TEXT
    );

    CREATE TABLE IF NOT EXISTS suspects (
        id                      INTEGER PRIMARY KEY,
        name                    TEXT,
        age                     INTEGER,
        department              TEXT,
        job_title               TEXT,
        alibi                   TEXT,
        alibi_verified          TEXT,
        badge_id                TEXT,
        has_master_access       INTEGER DEFAULT 0,
        relationship_to_victim  TEXT,
        last_seen               TEXT
    );

    CREATE TABLE IF NOT EXISTS access_logs (
        id          INTEGER PRIMARY KEY,
        timestamp   TEXT,
        badge_id    TEXT,
        person_name TEXT,
        location    TEXT,
        action      TEXT,
        terminal_id TEXT,
        flag        TEXT
    );

    CREATE TABLE IF NOT EXISTS messages (
        id          INTEGER PRIMARY KEY,
        sender      TEXT,
        receiver    TEXT,
        timestamp   TEXT,
        channel     TEXT,
        content     TEXT,
        encrypted   INTEGER DEFAULT 0,
        read_receipt INTEGER DEFAULT 0
    );

    CREATE TABLE IF NOT EXISTS transactions (
        id              INTEGER PRIMARY KEY,
        transaction_ref TEXT,
        from_account    TEXT,
        to_account      TEXT,
        amount          REAL,
        currency        TEXT DEFAULT 'USD',
        timestamp       TEXT,
        category        TEXT,
        approved_by     TEXT,
        note            TEXT
    );

    CREATE TABLE IF NOT EXISTS employees (
        id              INTEGER PRIMARY KEY,
        name            TEXT,
        department      TEXT,
        job_title       TEXT,
        manager         TEXT,
        hire_date       TEXT,
        salary_band     TEXT,
        badge_id        TEXT,
        active          INTEGER DEFAULT 1,
        termination_date TEXT
    );

    CREATE TABLE IF NOT EXISTS badge_registry (
        badge_id        TEXT PRIMARY KEY,
        issued_to       TEXT,
        access_level    INTEGER,
        zones_permitted TEXT,
        issued_date     TEXT,
        last_modified   TEXT,
        modified_by     TEXT,
        status          TEXT
    );

    CREATE TABLE IF NOT EXISTS incident_reports (
        id              INTEGER PRIMARY KEY,
        report_date     TEXT,
        filed_by        TEXT,
        category        TEXT,
        subject         TEXT,
        description     TEXT,
        status          TEXT,
        resolution      TEXT
    );

    CREATE TABLE IF NOT EXISTS decrypted_messages (
        id          INTEGER PRIMARY KEY,
        original_id INTEGER,
        decrypted_by TEXT,
        timestamp   TEXT,
        content     TEXT
    );
    )SQL";

    if (execute(schema).is_error) { std::cerr<<"[DB] schema failed\n"; return false; }

    // ── Seed ──────────────────────────────────────────────────────────────────
    const char* seed = R"SQL(

    -- VICTIM
    INSERT INTO victims VALUES (
        1,'Marcus Orion',42,'Engineering','Senior Data Architect',
        '2031-06-12','2047-03-16 02:14:00',
        'Server Room B-7, NovaCorp HQ','Asphyxiation — manual strangulation',
        'Encrypted personal tablet, model NV-9 — not found at scene'
    );

    -- SUSPECTS (5, each with partial or false alibis)
    INSERT INTO suspects VALUES
        (1,'Elena Vasquez',35,'Security','Head of Security',
         'Claims to have been home. No corroboration.',
         'UNVERIFIED','S-007',1,
         'Professional — managed building security for 4 years',
         '2047-03-16 00:16:00'),
        (2,'Dorian Kast',29,'Engineering','Junior Developer',
         'Gym session at FitCore downtown — 22:00 to 23:30',
         'PARTIAL — gym entry confirmed, no exit record',
         'D-042',0,
         'Direct report of victim. Was passed over for promotion Marcus blocked.',
         '2047-03-15 21:47:00'),
        (3,'Hana Mori',44,'Finance','Chief Financial Officer',
         'Client dinner at Meridian Hotel, verified until 21:00.',
         'PARTIAL — hotel confirms reservation, not presence after 21:00',
         'M-003',1,
         'Marcus''s department supervisor. Approved his project budget.',
         '2047-03-15 21:00:00'),
        (4,'Rex Calloway',51,'External','Independent Contractor',
         'Checked into Grand Hyatt at 20:00, room key confirms entry.',
         'PARTIAL — room access confirmed 20:00, no further record',
         'RC-EXT',0,
         'No listed relationship. Hired by Finance dept under Mori.',
         '2047-03-14 17:30:00'),
        (5,'Lena Park',31,'Engineering','Senior Data Analyst',
         'States she was working late, then went home. No witnesses.',
         'UNVERIFIED','L-019',0,
         'Colleague. Former partner of victim — relationship ended 8 months ago.',
         '2047-03-16 00:16:00');

    -- ACCESS LOGS (28 entries — normal traffic plus the night of the murder)
    INSERT INTO access_logs VALUES
        -- Daytime 15th (normal activity, red herrings)
        (1, '2047-03-15 07:52:00','M-003','Hana Mori',       'Main Lobby',      'ENTRY','T-01',NULL),
        (2, '2047-03-15 08:04:00','S-007','Elena Vasquez',   'Security Office', 'ENTRY','T-01',NULL),
        (3, '2047-03-15 08:11:00','A-001','Marcus Orion',    'Main Lobby',      'ENTRY','T-01',NULL),
        (4, '2047-03-15 08:15:00','L-019','Lena Park',       'Main Lobby',      'ENTRY','T-01',NULL),
        (5, '2047-03-15 08:44:00','D-042','Dorian Kast',     'Main Lobby',      'ENTRY','T-01',NULL),
        (6, '2047-03-15 09:30:00','M-003','Hana Mori',       'Finance Suite',   'ENTRY','T-04',NULL),
        (7, '2047-03-15 10:15:00','A-001','Marcus Orion',    'Server Room B-7', 'ENTRY','T-09',NULL),
        (8, '2047-03-15 10:58:00','A-001','Marcus Orion',    'Server Room B-7', 'EXIT', 'T-09',NULL),
        (9, '2047-03-15 11:30:00','RC-EXT','Rex Calloway',   'Visitor Lobby',   'ENTRY','T-01','VISITOR_PASS'),
        (10,'2047-03-15 11:32:00','M-003','Hana Mori',       'Visitor Lobby',   'ENTRY','T-01',NULL),
        (11,'2047-03-15 12:50:00','RC-EXT','Rex Calloway',   'Visitor Lobby',   'EXIT', 'T-01','VISITOR_PASS'),
        (12,'2047-03-15 12:52:00','M-003','Hana Mori',       'Finance Suite',   'ENTRY','T-04',NULL),
        (13,'2047-03-15 14:20:00','L-019','Lena Park',       'Server Room B-7', 'ENTRY','T-09',NULL),
        (14,'2047-03-15 14:55:00','A-001','Marcus Orion',    'Finance Suite',   'ENTRY','T-04','ANOMALY'),
        (15,'2047-03-15 15:02:00','A-001','Marcus Orion',    'Finance Suite',   'EXIT', 'T-04',NULL),
        (16,'2047-03-15 16:30:00','L-019','Lena Park',       'Server Room B-7', 'EXIT', 'T-09',NULL),
        -- Evening (suspects leaving / arriving)
        (17,'2047-03-15 18:30:00','D-042','Dorian Kast',     'Main Lobby',      'EXIT', 'T-01',NULL),
        (18,'2047-03-15 19:00:00','RC-EXT','Rex Calloway',   'Visitor Lobby',   'EXIT', 'T-01','VISITOR_PASS'),
        (19,'2047-03-15 19:45:00','A-001','Marcus Orion',    'Main Lobby',      'ENTRY','T-01',NULL),
        (20,'2047-03-15 20:01:00','S-007','Elena Vasquez',   'Security Office', 'ENTRY','T-02',NULL),
        (21,'2047-03-15 20:33:00','D-042','Dorian Kast',     'Main Lobby',      'ENTRY','T-01',NULL),
        (22,'2047-03-15 21:00:00','M-003','Hana Mori',       'Main Lobby',      'EXIT', 'T-01',NULL),
        (23,'2047-03-15 21:15:00','L-019','Lena Park',       'Main Lobby',      'ENTRY','T-01',NULL),
        (24,'2047-03-15 21:47:00','D-042','Dorian Kast',     'Main Lobby',      'EXIT', 'T-01',NULL),
        -- Critical window: 22:00 onwards
        (25,'2047-03-15 22:30:00','L-019','Lena Park',       'Server Room B-7', 'ENTRY','T-09',NULL),
        (26,'2047-03-15 22:31:00','A-001','Marcus Orion',    'Server Room B-7', 'ENTRY','T-09',NULL),
        (27,'2047-03-15 23:58:00','S-007','Elena Vasquez',   'Server Room B-7', 'ENTRY','T-09','AFTER_HOURS'),
        (28,'2047-03-16 00:15:00','S-007','Elena Vasquez',   'Server Room B-7', 'EXIT', 'T-09',NULL),
        (29,'2047-03-16 00:16:00','L-019','Lena Park',       'Server Room B-7', 'EXIT', 'T-09',NULL),
        -- THE KEY ENTRY: 2h after Lena exited, MASTER badge used
        (30,'2047-03-16 02:14:00','MASTER','[SYSTEM]',       'Server Room B-7', 'OVERRIDE','T-09','CRITICAL');

    -- MESSAGES
    INSERT INTO messages VALUES
        (1,'Marcus Orion','Lena Park',    '2047-03-14 09:15:00','Internal','Hey — got a minute? Something weird in the Q4 audit export.',0,1),
        (2,'Lena Park',   'Marcus Orion', '2047-03-14 09:22:00','Internal','Sure. My desk or yours?',0,1),
        (3,'Marcus Orion','Lena Park',    '2047-03-14 18:22:00','Internal','I found something in the Q4 ledger. Routing anomalies. We need to talk tonight. Do NOT involve Vasquez or Mori.',0,1),
        (4,'Lena Park',   'Marcus Orion', '2047-03-14 18:45:00','Internal','Understood. Meet me server room B-7 at 22:30.',0,1),
        (5,'Dorian Kast', 'Marcus Orion', '2047-03-15 08:50:00','Internal','Marcus. About the promotion review — can we reschedule? I have new work to show.',0,1),
        (6,'Marcus Orion','Dorian Kast',  '2047-03-15 09:05:00','Internal','Not this week Dorian. I have a lot on.',0,1),
        -- Encrypted messages between Mori and Vasquez (cannot be read without key)
        (7,'Hana Mori',   'Elena Vasquez','2047-03-15 09:10:00','Secure','[ENCRYPTED — KEY: NOVACORP-BLACK-09]',1,1),
        (8,'Elena Vasquez','Hana Mori',   '2047-03-15 09:45:00','Secure','[ENCRYPTED — KEY: NOVACORP-BLACK-09]',1,1),
        (9,'Hana Mori',   'Elena Vasquez','2047-03-15 20:05:00','Secure','[ENCRYPTED — KEY: NOVACORP-BLACK-09]',1,0),
        -- Critical message — sent BEFORE the murder, from Lena to unknown external
        (10,'Lena Park',  'external_7731','2047-03-15 22:15:00','External','He has the files. If he sends them tonight this all falls apart. I cannot let that happen.',0,0),
        -- Rex confirming arrangement with Mori
        (11,'Rex Calloway','Hana Mori',   '2047-03-12 14:00:00','External','Transfer confirmed. The arrangement holds. I trust you have the situation contained. — R',0,1),
        (12,'Hana Mori',  'Rex Calloway', '2047-03-12 14:30:00','External','It will be. Your next payment processes Friday.',0,1),
        -- Dead end red herring: Dorian venting
        (13,'Dorian Kast','unknown_peer', '2047-03-15 22:10:00','External','Marcus blocked my promotion again. Third time. I swear I could kill him.',0,0);

    -- TRANSACTIONS (money trail — requires careful aggregation to see the full picture)
    INSERT INTO transactions VALUES
        (1,'TXN-2047-0105A','NovaCorp-Operations','ExtShell-LLC',  450000.00,'USD','2047-01-05 00:00:00','Vendor-Consulting','H.Mori','Q4 Consulting Services'),
        (2,'TXN-2047-0106A','ExtShell-LLC','RCCalloway-Private',   440000.00,'USD','2047-01-06 00:00:00','Vendor-Consulting','SYSTEM','Services rendered — retainer'),
        (3,'TXN-2047-0203A','NovaCorp-Operations','ExtShell-LLC',  450000.00,'USD','2047-02-03 00:00:00','Vendor-Consulting','H.Mori','Q1 Consulting Services'),
        (4,'TXN-2047-0204A','ExtShell-LLC','RCCalloway-Private',   440000.00,'USD','2047-02-04 00:00:00','Vendor-Consulting','SYSTEM','Services rendered — retainer'),
        (5,'TXN-2047-0301A','NovaCorp-Operations','ExtShell-LLC',  450000.00,'USD','2047-03-01 00:00:00','Vendor-Consulting','H.Mori','Q1 Consulting Services (supplement)'),
        (6,'TXN-2047-0302A','ExtShell-LLC','RCCalloway-Private',   440000.00,'USD','2047-03-02 00:00:00','Vendor-Consulting','SYSTEM','Services rendered — retainer'),
        -- Mori pays Park — the payoff
        (7,'TXN-2047-0301B','Mori-Personal',      'LPark-Account',  12000.00,'USD','2047-03-01 00:00:00','Personal','SELF','Bonus — discretionary performance'),
        (8,'TXN-2047-0310A','LPark-Account',      'CashDrop-4481',  12000.00,'USD','2047-03-10 00:00:00','Personal','SYSTEM',NULL),
        -- Normal payroll (red herring rows to make the table dense)
        (9, 'PAY-2047-0115','NovaCorp-Payroll','Marcus-Orion-Pay', 12500.00,'USD','2047-01-15 00:00:00','Payroll','SYSTEM','Regular salary'),
        (10,'PAY-2047-0215','NovaCorp-Payroll','Marcus-Orion-Pay', 12500.00,'USD','2047-02-15 00:00:00','Payroll','SYSTEM','Regular salary'),
        (11,'PAY-2047-0115','NovaCorp-Payroll','LPark-Pay',         9800.00,'USD','2047-01-15 00:00:00','Payroll','SYSTEM','Regular salary'),
        (12,'PAY-2047-0215','NovaCorp-Payroll','LPark-Pay',         9800.00,'USD','2047-02-15 00:00:00','Payroll','SYSTEM','Regular salary'),
        (13,'PAY-2047-0115','NovaCorp-Payroll','Mori-Exec-Pay',    38000.00,'USD','2047-01-15 00:00:00','Payroll','SYSTEM','Executive salary'),
        (14,'PAY-2047-0215','NovaCorp-Payroll','Mori-Exec-Pay',    38000.00,'USD','2047-02-15 00:00:00','Payroll','SYSTEM','Executive salary'),
        (15,'TXN-2047-0310B','ExtShell-LLC','Cayman-Shell-7',      420000.00,'USD','2047-03-10 00:00:00','Wire','SYSTEM','Intercompany transfer'),
        (16,'TXN-2047-0311A','Cayman-Shell-7','H.Mori-Offshore',   415000.00,'USD','2047-03-11 00:00:00','Wire','SYSTEM','Management fee');

    -- EMPLOYEES (full company directory — needed to cross-reference badge_registry)
    INSERT INTO employees VALUES
        (1, 'Marcus Orion',  'Engineering','Senior Data Architect','Hana Mori',  '2031-06-12','L5','A-001',1,NULL),
        (2, 'Elena Vasquez', 'Security',   'Head of Security',     'CEO Office', '2039-03-01','L6','S-007',1,NULL),
        (3, 'Dorian Kast',   'Engineering','Junior Developer',      'Marcus Orion','2044-09-15','L2','D-042',1,NULL),
        (4, 'Hana Mori',     'Finance',    'Chief Financial Officer','CEO Office','2028-11-01','L7','M-003',1,NULL),
        (5, 'Rex Calloway',  'External',   'Independent Contractor','Hana Mori', '2046-04-01','EXT','RC-EXT',1,NULL),
        (6, 'Lena Park',     'Engineering','Senior Data Analyst',   'Marcus Orion','2040-07-22','L4','L-019',1,NULL),
        (7, 'Yuki Tanaka',   'Engineering','DevOps Engineer',       'Marcus Orion','2043-02-10','L3','Y-088',1,NULL),
        (8, 'Carl Bremer',   'IT',         'Systems Administrator', 'Elena Vasquez','2035-05-05','L5','C-055',1,NULL),
        (9, 'Sandra Obi',    'Finance',    'Senior Accountant',     'Hana Mori',  '2038-08-20','L4','O-021',1,NULL),
        (10,'James Fitch',   'Legal',      'General Counsel',       'CEO Office', '2030-01-15','L6','J-003',1,NULL);

    -- BADGE REGISTRY (this is the key table — MASTER badge is the smoking gun)
    INSERT INTO badge_registry VALUES
        ('A-001','Marcus Orion',  3,'Main,Engineering,Finance,ServerRooms',   '2031-06-12','2046-01-10','IT-Admin','ACTIVE'),
        ('S-007','Elena Vasquez', 5,'ALL-ZONES',                               '2039-03-01','2047-03-01','IT-Admin','ACTIVE'),
        ('D-042','Dorian Kast',   1,'Main,Engineering',                        '2044-09-15','2045-01-05','IT-Admin','ACTIVE'),
        ('M-003','Hana Mori',     4,'Main,Finance,Executive,ServerRooms',      '2028-11-01','2046-09-15','IT-Admin','ACTIVE'),
        ('RC-EXT','Rex Calloway', 0,'Visitor',                                  '2046-04-01','2047-03-10','IT-Admin','ACTIVE'),
        ('L-019','Lena Park',     2,'Main,Engineering,ServerRooms',             '2040-07-22','2047-02-28','IT-Admin','ACTIVE'),
        ('Y-088','Yuki Tanaka',   2,'Main,Engineering,ServerRooms',             '2043-02-10','2044-06-01','IT-Admin','ACTIVE'),
        ('C-055','Carl Bremer',   3,'Main,IT,ServerRooms',                      '2035-05-05','2046-03-15','IT-Admin','ACTIVE'),
        -- THE MASTER BADGE: issued to Vasquez, but last modified by someone else
        ('MASTER','Elena Vasquez',9,'ALL-ZONES-OVERRIDE',                       '2039-03-01','2047-03-15','L-019','ACTIVE');

    -- INCIDENT REPORTS (paper trail that Mori tried to bury)
    INSERT INTO incident_reports VALUES
        (1,'2047-02-10','Sandra Obi',   'Finance','Ledger Anomaly',
         'Q4 vendor payments to ExtShell-LLC do not match approved purchase orders. Three invoices totaling $1.35M have no corresponding deliverables.',
         'CLOSED','Reviewed by CFO. Reclassified as approved consulting spend. No further action.'),
        (2,'2047-03-05','Marcus Orion', 'Security','Unauthorized Data Export Attempt',
         'Someone attempted to export the full vendor transaction ledger from the Finance Suite terminal at 14:20 on 03/05. Access was denied. Badge A-001 used.',
         'OPEN','Under investigation.'),
        (3,'2047-03-10','Dorian Kast',  'HR','Workplace Dispute',
         'Formal complaint against manager Marcus Orion regarding promotion deferral. Third such complaint in 18 months.',
         'CLOSED','Manager review scheduled.'),
        (4,'2047-03-15','Elena Vasquez','Security','After-Hours Badge Activity',
         'Badge MASTER used at 02:14 in Server Room B-7. No personnel on shift. Auto-flagged by system.',
         'OPEN','Pending investigation.'),
        (5,'2047-01-20','Carl Bremer',  'IT','Badge Modification Log',
         'Badge MASTER had its zone permissions modified at 23:44 on 01/19. Modification attributed to badge L-019. Carl Bremer flagged as suspicious — L-019 does not have admin rights to modify MASTER-level badges.',
         'CLOSED','Escalated to security. No response from Vasquez office.');

    -- DECRYPTED MESSAGES (only visible once player finds the key in badge_registry/incident_reports)
    INSERT INTO decrypted_messages VALUES
        (1,7,'SYSTEM','2047-03-15 09:10:00',
         'Elena — Orion has been in the Finance Suite. He accessed something he should not have. I need you to make sure he does not get a chance to send anything out tonight. Use the override if necessary.'),
        (2,8,'SYSTEM','2047-03-15 09:45:00',
         'Understood. I will monitor. But Hana — if this goes wrong, I want it on record that I was following your instruction.'),
        (3,9,'SYSTEM','2047-03-15 20:05:00',
         'He is still in the building. Park confirmed he plans to copy the files at 22:30. Do what needs to be done. The MASTER key is yours.');

    )SQL";

    bool ok = !execute(seed).is_error;
    if (!ok) std::cerr << "[DB] seed failed\n";
    return ok;
}
