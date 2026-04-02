#include "Database.h"
#include <iostream>
#include <algorithm>
#include <cctype>

Database::Database()  {}
Database::~Database() { close(); }

bool Database::open(const std::string& path) {
    close();
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "[DB] " << sqlite3_errmsg(m_db) << "\n";
        m_db = nullptr; return false;
    }
    return true;
}
void Database::close() { if (m_db) { sqlite3_close(m_db); m_db = nullptr; } }

struct CB { QueryResult* r; bool hdr=false; };
int Database::row_callback(void* d, int argc, char** argv, char** cols) {
    auto* c = static_cast<CB*>(d);
    if (!c->hdr) {
        for (int i=0;i<argc;i++) c->r->columns.push_back(cols[i]?cols[i]:"");
        c->hdr = true;
    }
    std::vector<std::string> row;
    for (int i=0;i<argc;i++) row.push_back(argv[i]?argv[i]:"NULL");
    c->r->rows.push_back(std::move(row));
    return 0;
}

QueryResult Database::execute(const std::string& sql) {
    QueryResult res;
    if (!m_db) { res.is_error=true; res.error="No database."; return res; }
    CB cb{&res};
    char* err=nullptr;
    if (sqlite3_exec(m_db,sql.c_str(),row_callback,&cb,&err)!=SQLITE_OK) {
        res.is_error=true; res.error=err?err:"Error"; sqlite3_free(err);
    }
    return res;
}

std::vector<std::string> Database::get_table_names() {
    auto r = execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(!row.empty()) v.push_back(row[0]);
    return v;
}

std::vector<std::string> Database::get_column_names(const std::string& table) {
    auto r = execute("PRAGMA table_info("+table+");");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(row.size()>=2) v.push_back(row[1]);
    return v;
}

// Returns "col_name TYPE" pairs
std::vector<std::pair<std::string,std::string>> Database::get_column_info(const std::string& table) {
    auto r = execute("PRAGMA table_info("+table+");");
    std::vector<std::pair<std::string,std::string>> v;
    for (auto& row:r.rows)
        if (row.size()>=3) v.push_back({row[1], row[2]});
    return v;
}

int Database::get_row_count(const std::string& table) {
    auto r = execute("SELECT COUNT(*) FROM "+table+";");
    if (!r.rows.empty() && !r.rows[0].empty()) return std::stoi(r.rows[0][0]);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//  CASE DATA — THE ORION MURDER
// ═══════════════════════════════════════════════════════════════════════════
bool Database::seed_case(const Case&) {

    const char* schema = R"SQL(
    CREATE TABLE IF NOT EXISTS victims (
        id              INTEGER PRIMARY KEY,
        name            TEXT,
        age             INTEGER,
        job_title       TEXT,
        department      TEXT,
        time_of_death   TEXT,
        location_found  TEXT,
        cause_of_death  TEXT,
        notes           TEXT
    );

    CREATE TABLE IF NOT EXISTS suspects (
        id                     INTEGER PRIMARY KEY,
        name                   TEXT,
        age                    INTEGER,
        job_title              TEXT,
        department             TEXT,
        badge_id               TEXT,
        alibi                  TEXT,
        alibi_status           TEXT,
        motive_flag            TEXT,
        last_building_exit     TEXT
    );

    CREATE TABLE IF NOT EXISTS access_logs (
        id          INTEGER PRIMARY KEY,
        ts          TEXT,
        badge_id    TEXT,
        person      TEXT,
        zone        TEXT,
        action      TEXT,
        flag        TEXT
    );

    CREATE TABLE IF NOT EXISTS badge_registry (
        badge_id        TEXT PRIMARY KEY,
        issued_to       TEXT,
        access_level    INTEGER,
        zones           TEXT,
        last_modified   TEXT,
        modified_by     TEXT,
        status          TEXT
    );

    CREATE TABLE IF NOT EXISTS messages (
        id          INTEGER PRIMARY KEY,
        ts          TEXT,
        sender      TEXT,
        recipient   TEXT,
        body        TEXT,
        encrypted   INTEGER DEFAULT 0
    );

    CREATE TABLE IF NOT EXISTS transactions (
        id              INTEGER PRIMARY KEY,
        ts              TEXT,
        ref             TEXT,
        from_acct       TEXT,
        to_acct         TEXT,
        amount          REAL,
        approved_by     TEXT,
        category        TEXT
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
        id          INTEGER PRIMARY KEY,
        filed_date  TEXT,
        filed_by    TEXT,
        subject     TEXT,
        body        TEXT,
        status      TEXT,
        closed_by   TEXT
    );
    )SQL";

    if (execute(schema).is_error) { std::cerr<<"[DB] schema failed\n"; return false; }

    const char* seed = R"SQL(

    INSERT INTO victims VALUES
    (1,'Marcus Orion',42,'Senior Data Architect','Engineering',
     '2047-03-16 02:14:00','Server Room B-7, NovaCorp HQ',
     'Asphyxiation — manual strangulation',
     'Personal tablet not found at scene. Was reviewing Q4 vendor ledger day of death.');

    INSERT INTO suspects VALUES
    (1,'Elena Vasquez',35,'Head of Security','Security',
     'S-007','Claims to have been home all evening','UNVERIFIED',
     'controls_building_security','2047-03-16 00:16:00'),
    (2,'Dorian Kast',29,'Junior Developer','Engineering',
     'D-042','Gym session FitCore 22:00-23:30','PARTIAL',
     'promotion_blocked_by_victim','2047-03-15 21:47:00'),
    (3,'Hana Mori',44,'Chief Financial Officer','Finance',
     'M-003','Client dinner until 21:00 at Meridian Hotel','PARTIAL',
     'approved_fraudulent_payments','2047-03-15 21:00:00'),
    (4,'Rex Calloway',51,'External Contractor','Vendor',
     'RC-EXT','Hotel Grand Hyatt checkin 20:00','PARTIAL',
     'received_fraudulent_payments','2047-03-14 17:30:00'),
    (5,'Lena Park',31,'Senior Data Analyst','Engineering',
     'L-019','Working late then went home — no witnesses','UNVERIFIED',
     'ex_partner_paid_by_mori','2047-03-16 00:16:00');

    INSERT INTO access_logs VALUES
    -- Normal daytime traffic
    (1, '2047-03-15 07:52','M-003','Hana Mori',      'Main Lobby',      'ENTRY', NULL),
    (2, '2047-03-15 08:04','S-007','Elena Vasquez',  'Security Office', 'ENTRY', NULL),
    (3, '2047-03-15 08:11','A-001','Marcus Orion',   'Main Lobby',      'ENTRY', NULL),
    (4, '2047-03-15 08:15','L-019','Lena Park',      'Main Lobby',      'ENTRY', NULL),
    (5, '2047-03-15 08:44','D-042','Dorian Kast',    'Main Lobby',      'ENTRY', NULL),
    (6, '2047-03-15 10:15','A-001','Marcus Orion',   'Server Room B-7', 'ENTRY', NULL),
    (7, '2047-03-15 10:58','A-001','Marcus Orion',   'Server Room B-7', 'EXIT',  NULL),
    (8, '2047-03-15 13:20','L-019','Lena Park',      'Server Room B-7', 'ENTRY', NULL),
    (9, '2047-03-15 14:55','A-001','Marcus Orion',   'Finance Suite',   'ENTRY', 'ANOMALY'),
    (10,'2047-03-15 15:02','A-001','Marcus Orion',   'Finance Suite',   'EXIT',  NULL),
    (11,'2047-03-15 16:30','L-019','Lena Park',      'Server Room B-7', 'EXIT',  NULL),
    -- Evening exits
    (12,'2047-03-15 18:30','D-042','Dorian Kast',    'Main Lobby',      'EXIT',  NULL),
    (13,'2047-03-15 19:45','A-001','Marcus Orion',   'Main Lobby',      'ENTRY', NULL),
    (14,'2047-03-15 20:01','S-007','Elena Vasquez',  'Security Office', 'ENTRY', NULL),
    (15,'2047-03-15 20:33','D-042','Dorian Kast',    'Main Lobby',      'ENTRY', NULL),
    (16,'2047-03-15 21:00','M-003','Hana Mori',      'Main Lobby',      'EXIT',  NULL),
    (17,'2047-03-15 21:15','L-019','Lena Park',      'Main Lobby',      'ENTRY', NULL),
    (18,'2047-03-15 21:47','D-042','Dorian Kast',    'Main Lobby',      'EXIT',  NULL),
    -- The critical night window
    (19,'2047-03-15 22:30','L-019','Lena Park',      'Server Room B-7', 'ENTRY', NULL),
    (20,'2047-03-15 22:31','A-001','Marcus Orion',   'Server Room B-7', 'ENTRY', NULL),
    (21,'2047-03-15 23:58','S-007','Elena Vasquez',  'Server Room B-7', 'ENTRY', 'AFTER_HOURS'),
    (22,'2047-03-16 00:15','S-007','Elena Vasquez',  'Server Room B-7', 'EXIT',  NULL),
    (23,'2047-03-16 00:16','L-019','Lena Park',      'Server Room B-7', 'EXIT',  NULL),
    -- MASTER override: 2 hours after Lena exited — but her badge was still logged
    (24,'2047-03-16 02:14','MASTER','[SYSTEM]',      'Server Room B-7', 'OVERRIDE','CRITICAL');

    INSERT INTO badge_registry VALUES
    ('A-001','Marcus Orion', 3,'Main,Engineering,Finance,ServerRooms','2046-01-10','C-055','ACTIVE'),
    ('S-007','Elena Vasquez',5,'ALL-ZONES',                            '2047-03-01','C-055','ACTIVE'),
    ('D-042','Dorian Kast',  1,'Main,Engineering',                     '2045-01-05','C-055','ACTIVE'),
    ('M-003','Hana Mori',    4,'Main,Finance,Executive,ServerRooms',   '2046-09-15','C-055','ACTIVE'),
    ('RC-EXT','Rex Calloway',0,'Visitor',                              '2047-01-15','C-055','ACTIVE'),
    ('L-019','Lena Park',    2,'Main,Engineering,ServerRooms',         '2047-02-28','C-055','ACTIVE'),
    ('C-055','Carl Bremer',  3,'Main,IT,ServerRooms',                  '2046-03-15','C-055','ACTIVE'),
    -- THE KEY: MASTER badge, modified by L-019, not by IT admin
    ('MASTER','Elena Vasquez',9,'ALL-ZONES-OVERRIDE','2047-01-19','L-019','ACTIVE');

    INSERT INTO messages VALUES
    -- Normal traffic (mostly noise)
    (1, '2047-03-14 09:15','Marcus Orion','Lena Park',
        'I found something weird in the Q4 vendor export. Routing anomalies.',0),
    (2, '2047-03-14 09:22','Lena Park','Marcus Orion',
        'Send me the file. I will look tonight.',0),
    (3, '2047-03-14 18:22','Marcus Orion','Lena Park',
        'Meet me in B-7 at 22:30. Come alone. Do not tell Mori.',0),
    (4, '2047-03-14 18:45','Lena Park','Marcus Orion',
        'Understood. See you there.',0),
    (5, '2047-03-15 08:50','Dorian Kast','Marcus Orion',
        'Can we reschedule the promotion review? I have new code to show you.',0),
    (6, '2047-03-15 09:05','Marcus Orion','Dorian Kast',
        'Not this week.',0),
    -- ENCRYPTED — need key to read
    (7, '2047-03-15 09:10','Hana Mori','Elena Vasquez',
        '[ENCRYPTED]',1),
    (8, '2047-03-15 09:45','Elena Vasquez','Hana Mori',
        '[ENCRYPTED]',1),
    (9, '2047-03-15 20:05','Hana Mori','Elena Vasquez',
        '[ENCRYPTED]',1),
    -- Lena to external contact — the key smoking gun (timestamp matters)
    (10,'2047-03-15 22:15','Lena Park','unknown_ext',
        'He has the Q4 files. Tonight is the only chance to stop this. Proceeding.',0),
    -- Calloway confirming the arrangement with Mori
    (11,'2047-03-12 14:00','Rex Calloway','Hana Mori',
        'Transfer confirmed. Arrangement holds. Next payment Friday as agreed.',0),
    -- Red herring — Dorian venting (not actionable)
    (12,'2047-03-15 22:10','Dorian Kast','friend_ext',
        'Marcus blocked my raise again. Three times now. I am done.',0);

    INSERT INTO transactions VALUES
    -- Fraudulent payments: NovaCorp -> ExtShell -> Calloway
    (1, '2047-01-05','TXN-0105-A','NovaCorp-Operations','ExtShell-LLC',    450000,'H.Mori','Vendor-Consulting'),
    (2, '2047-01-06','TXN-0106-A','ExtShell-LLC','RCCalloway-Private',     440000,'AUTO',  'Vendor-Consulting'),
    (3, '2047-02-03','TXN-0203-A','NovaCorp-Operations','ExtShell-LLC',    450000,'H.Mori','Vendor-Consulting'),
    (4, '2047-02-04','TXN-0204-A','ExtShell-LLC','RCCalloway-Private',     440000,'AUTO',  'Vendor-Consulting'),
    (5, '2047-03-01','TXN-0301-A','NovaCorp-Operations','ExtShell-LLC',    450000,'H.Mori','Vendor-Consulting'),
    (6, '2047-03-02','TXN-0302-A','ExtShell-LLC','RCCalloway-Private',     440000,'AUTO',  'Vendor-Consulting'),
    -- Mori bribes Park (this is the personal payoff link)
    (7, '2047-03-01','TXN-0301-B','Mori-Personal','LPark-Account',          12000,'SELF',  'Personal'),
    (8, '2047-03-10','TXN-0310-A','LPark-Account','ATM-CASH-4481',          12000,'AUTO',  'Personal'),
    -- Offshore laundering
    (9, '2047-03-10','TXN-0310-B','ExtShell-LLC','Cayman-7712',            420000,'AUTO',  'Wire'),
    (10,'2047-03-11','TXN-0311-A','Cayman-7712','H.Mori-Offshore',         415000,'AUTO',  'Wire'),
    -- Regular payroll (noise/red herring rows)
    (11,'2047-01-15','PAY-0115','NovaCorp-Payroll','Marcus-Orion-Salary',  12500,'AUTO',  'Payroll'),
    (12,'2047-02-15','PAY-0215','NovaCorp-Payroll','Marcus-Orion-Salary',  12500,'AUTO',  'Payroll'),
    (13,'2047-01-15','PAY-0115','NovaCorp-Payroll','LPark-Salary',          9800,'AUTO',  'Payroll'),
    (14,'2047-02-15','PAY-0215','NovaCorp-Payroll','LPark-Salary',          9800,'AUTO',  'Payroll'),
    (15,'2047-01-15','PAY-0115','NovaCorp-Payroll','HMori-Salary',         38000,'AUTO',  'Payroll'),
    (16,'2047-02-15','PAY-0215','NovaCorp-Payroll','HMori-Salary',         38000,'AUTO',  'Payroll');

    -- Decrypted messages only readable once player finds the key "BLACK-OMEGA-7"
    -- (key is buried in incident_reports)
    INSERT INTO decrypted_messages VALUES
    (1,7,'2047-03-15 09:10','Hana Mori','Elena Vasquez',
     'Orion accessed something in Finance yesterday. He cannot be allowed to share those files. Watch him tonight.'),
    (2,8,'2047-03-15 09:45','Elena Vasquez','Hana Mori',
     'Understood. I will monitor. If it comes to it, the MASTER credential is available. But I want this in writing from you.'),
    (3,9,'2047-03-15 20:05','Hana Mori','Elena Vasquez',
     'Park is handling it. She will meet him in B-7 at 22:30. MASTER key is hers to use. This conversation does not exist.');

    INSERT INTO incident_reports VALUES
    (1,'2047-02-10','Sandra Obi',
     'Ledger discrepancy — ExtShell-LLC payments',
     'Q4 vendor invoices from ExtShell-LLC total $1.35M with no deliverables logged. '
     'Three invoices approved solely by CFO Mori. No secondary sign-off.',
     'CLOSED','H.Mori'),
    (2,'2047-01-20','Carl Bremer',
     'Unauthorised badge modification — MASTER credential',
     'Badge MASTER had zone permissions elevated at 23:44 on Jan 19th. '
     'Modification logged under badge L-019. L-019 (Lena Park) has no admin rights '
     'to modify MASTER-level credentials. Encryption key for internal audit log: BLACK-OMEGA-7',
     'CLOSED','E.Vasquez'),
    (3,'2047-03-05','Marcus Orion',
     'Attempted Finance Suite data export — badge A-001',
     'Someone used my credentials to attempt a full export of the vendor ledger at 14:55. '
     'Access was denied. I did not initiate this. Requesting review.',
     'OPEN',NULL),
    (4,'2047-03-15','Elena Vasquez',
     'After-hours MASTER badge use — Server Room B-7',
     'Auto-flag: MASTER badge used at 02:14 in Server Room B-7. No authorised personnel on night shift.',
     'OPEN',NULL);

    )SQL";

    bool ok = !execute(seed).is_error;
    if (!ok) std::cerr<<"[DB] seed failed\n";
    return ok;
}
