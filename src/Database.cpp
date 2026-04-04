#include "Database.h"
#include <iostream>
#include <algorithm>
#include <cctype>

Database::Database()  {}
Database::~Database() { close(); }

bool Database::open(const std::string& path) {
    close();
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "[DB] open: " << sqlite3_errmsg(m_db) << "\n";
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
    auto r=execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(!row.empty()) v.push_back(row[0]);
    return v;
}
std::vector<std::string> Database::get_column_names(const std::string& t) {
    auto r=execute("PRAGMA table_info("+t+");");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(row.size()>=2) v.push_back(row[1]);
    return v;
}
std::vector<std::pair<std::string,std::string>> Database::get_column_info(const std::string& t) {
    auto r=execute("PRAGMA table_info("+t+");");
    std::vector<std::pair<std::string,std::string>> v;
    for (auto& row:r.rows) if(row.size()>=3) v.push_back({row[1],row[2]});
    return v;
}
int Database::get_row_count(const std::string& t) {
    auto r=execute("SELECT COUNT(*) FROM "+t+";");
    if(!r.rows.empty()&&!r.rows[0].empty()) {
        try { return std::stoi(r.rows[0][0]); } catch(...) {}
    }
    return 0;
}

// Helper — run one statement, log failure but continue
static bool exec1(Database* db, const char* label, const std::string& sql) {
    auto r = db->execute(sql);
    if (r.is_error) {
        std::cerr << "[DB] FAILED [" << label << "]: " << r.error << "\n";
        return false;
    }
    return true;
}

// ============================================================================
//  CASE SEED — each INSERT is a separate execute() so one failure
//  cannot silently block everything that follows.
//  IMPORTANT: no Unicode em-dashes inside SQL string literals.
// ============================================================================
bool Database::seed_case(const Case&) {

    // ── Schema ───────────────────────────────────────────────────────────────
    const std::string schema = R"SQL(
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
        id                    INTEGER PRIMARY KEY,
        name                  TEXT,
        age                   INTEGER,
        job_title             TEXT,
        department            TEXT,
        badge_id              TEXT,
        alibi                 TEXT,
        alibi_status          TEXT,
        motive_flag           TEXT,
        last_building_exit    TEXT
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
    )SQL";

    if (execute(schema).is_error) { std::cerr<<"[DB] schema failed\n"; return false; }

    // ── victims ──────────────────────────────────────────────────────────────
    exec1(this,"victims", R"SQL(
    INSERT INTO victims VALUES (
        1,
        'Marcus Orion',
        42,
        'Senior Data Architect',
        'Engineering',
        '2047-03-16 02:14:00',
        'Server Room B-7, NovaCorp HQ',
        'Asphyxiation - manual strangulation',
        'Personal tablet not found at scene. Was reviewing Q4 vendor ledger on the day of death.'
    );)SQL");

    // ── suspects ─────────────────────────────────────────────────────────────
    exec1(this,"suspects-1", R"SQL(
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
         'RC-EXT','Hotel Grand Hyatt check-in 20:00','PARTIAL',
         'received_fraudulent_payments','2047-03-14 17:30:00'),
        (5,'Lena Park',31,'Senior Data Analyst','Engineering',
         'L-019','Working late then went home - no witnesses','UNVERIFIED',
         'ex_partner_paid_by_mori','2047-03-16 00:16:00');
    )SQL");

    // ── access_logs ──────────────────────────────────────────────────────────
    exec1(this,"access_logs", R"SQL(
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
    )SQL");

    // ── badge_registry ───────────────────────────────────────────────────────
    exec1(this,"badge_registry", R"SQL(
    INSERT INTO badge_registry VALUES
        ('A-001',  'Marcus Orion',  3, 'Main,Engineering,Finance,ServerRooms', '2046-01-10', 'C-055', 'ACTIVE'),
        ('S-007',  'Elena Vasquez', 5, 'ALL-ZONES',                            '2047-03-01', 'C-055', 'ACTIVE'),
        ('D-042',  'Dorian Kast',   1, 'Main,Engineering',                     '2045-01-05', 'C-055', 'ACTIVE'),
        ('M-003',  'Hana Mori',     4, 'Main,Finance,Executive,ServerRooms',   '2046-09-15', 'C-055', 'ACTIVE'),
        ('RC-EXT', 'Rex Calloway',  0, 'Visitor',                              '2047-01-15', 'C-055', 'ACTIVE'),
        ('L-019',  'Lena Park',     2, 'Main,Engineering,ServerRooms',         '2047-02-28', 'C-055', 'ACTIVE'),
        ('C-055',  'Carl Bremer',   3, 'Main,IT,ServerRooms',                  '2046-03-15', 'C-055', 'ACTIVE'),
        ('MASTER', 'Elena Vasquez', 9, 'ALL-ZONES-OVERRIDE',                   '2047-01-19', 'L-019', 'ACTIVE');
    )SQL");

    // ── messages ─────────────────────────────────────────────────────────────
    exec1(this,"messages", R"SQL(
    INSERT INTO messages VALUES
        (1, '2047-03-14 09:15','Marcus Orion','Lena Park',
            'I found something weird in the Q4 vendor export. Routing anomalies. Need your eyes on it.',0),
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
        (7, '2047-03-15 09:10','Hana Mori','Elena Vasquez',
            '[ENCRYPTED]',1),
        (8, '2047-03-15 09:45','Elena Vasquez','Hana Mori',
            '[ENCRYPTED]',1),
        (9, '2047-03-15 20:05','Hana Mori','Elena Vasquez',
            '[ENCRYPTED]',1),
        (10,'2047-03-15 22:15','Lena Park','unknown_ext',
            'He has the Q4 files. Tonight is the only chance to stop this. Proceeding.',0),
        (11,'2047-03-12 14:00','Rex Calloway','Hana Mori',
            'Transfer confirmed. Arrangement holds. Next payment Friday as agreed.',0),
        (12,'2047-03-15 22:10','Dorian Kast','friend_ext',
            'Marcus blocked my raise again. Three times now. I am done.',0);
    )SQL");

    // ── transactions ─────────────────────────────────────────────────────────
    exec1(this,"transactions", R"SQL(
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
    )SQL");

    // ── decrypted_messages ───────────────────────────────────────────────────
    exec1(this,"decrypted_messages", R"SQL(
    INSERT INTO decrypted_messages VALUES
        (1, 7, '2047-03-15 09:10','Hana Mori','Elena Vasquez',
         'Orion accessed something in Finance yesterday. He cannot be allowed to share those files. Watch him tonight.'),
        (2, 8, '2047-03-15 09:45','Elena Vasquez','Hana Mori',
         'Understood. I will monitor. If it comes to it, the MASTER credential is available. But I want this in writing from you.'),
        (3, 9, '2047-03-15 20:05','Hana Mori','Elena Vasquez',
         'Park is handling it. She will meet him in B-7 at 22:30. MASTER key is hers to use. This conversation does not exist.');
    )SQL");

    // ── incident_reports ─────────────────────────────────────────────────────
    exec1(this,"incident_reports-1", R"SQL(
    INSERT INTO incident_reports VALUES
        (1,'2047-02-10','Sandra Obi',
         'Ledger discrepancy - ExtShell-LLC payments',
         'Q4 vendor invoices from ExtShell-LLC total $1.35M with no deliverables logged. Three invoices approved solely by CFO Mori. No secondary sign-off.',
         'CLOSED','H.Mori');
    )SQL");

    exec1(this,"incident_reports-2", R"SQL(
    INSERT INTO incident_reports VALUES
        (2,'2047-01-20','Carl Bremer',
         'Unauthorised badge modification - MASTER credential',
         'Badge MASTER had zone permissions elevated at 23:44 on Jan 19th. Modification logged under badge L-019. L-019 (Lena Park) has no admin rights to modify MASTER-level credentials. Encryption key for internal audit log: BLACK-OMEGA-7',
         'CLOSED','E.Vasquez');
    )SQL");

    exec1(this,"incident_reports-3", R"SQL(
    INSERT INTO incident_reports VALUES
        (3,'2047-03-05','Marcus Orion',
         'Attempted Finance Suite data export - badge A-001',
         'Someone used my credentials to attempt a full export of the vendor ledger at 14:55. Access was denied. I did not initiate this. Requesting review.',
         'OPEN', NULL);
    )SQL");

    exec1(this,"incident_reports-4", R"SQL(
    INSERT INTO incident_reports VALUES
        (4,'2047-03-15','Elena Vasquez',
         'After-hours MASTER badge use - Server Room B-7',
         'Auto-flag: MASTER badge used at 02:14 in Server Room B-7. No authorised personnel on night shift.',
         'OPEN', NULL);
    )SQL");

    // Verify all tables have data
    auto check = [&](const char* t, int expected) {
        int n = get_row_count(t);
        if (n < expected)
            std::cerr << "[DB] WARNING: " << t << " has " << n
                      << " rows (expected " << expected << ")\n";
        else
            std::cerr << "[DB] OK: " << t << " = " << n << " rows\n";
    };
    check("victims",           1);
    check("suspects",          5);
    check("access_logs",      24);
    check("badge_registry",    8);
    check("messages",         12);
    check("transactions",     16);
    check("decrypted_messages",3);
    check("incident_reports",  4);

    return true;
}

bool Database::seed_espionage(const Case& c) {
    // ── Schema ───────────────────────────────────────────────────────────────
    const std::string schema = R"SQL(
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
    )SQL";

    if (execute(schema).is_error) { std::cerr<<"[DB] schema failed\n"; return false; }

    // ── project_files ────────────────────────────────────────────────────────
    exec1(this,"project_files", R"SQL(
    INSERT INTO project_files VALUES 
        ('P-01', 'BLUEBIRD_PROTOTYPE_V1', 1500, 'LEVEL-5', 'Sarah Chen'),
        ('P-02', 'Q3_FINANCIALS_DRAFT',   12,   'LEVEL-3', 'David Kim'),
        ('P-03', 'HR_ROSTER_2047',        4,    'LEVEL-2', 'Marcus Vance');
    )SQL");

    // ── employees ────────────────────────────────────────────────────────────
    exec1(this,"employees", R"SQL(
    INSERT INTO employees VALUES
        ('E-101', 'Sarah Chen',    'R&D',     'LEVEL-5', 'ACTIVE'),
        ('E-102', 'Marcus Vance',  'Sales',   'LEVEL-2', 'ACTIVE'),
        ('E-103', 'David Kim',     'IT',      'LEVEL-4', 'ACTIVE'),
        ('E-104', 'Elena Vasquez', 'Legal',   'LEVEL-5', 'ACTIVE'),
        ('EXT-1', 'ApexTech_Rep',  'External','NONE',    'N/A');
    )SQL");

    // ── network_logs ─────────────────────────────────────────────────────────
    exec1(this,"network_logs", R"SQL(
    INSERT INTO network_logs VALUES
        (1, '2047-04-02 01:14', '192.168.1.15', 'LOGIN',      0,    'NORMAL'),
        (2, '2047-04-02 02:30', '192.168.1.22', 'DATA_PULL',  12,   'NORMAL'),
        (3, '2047-04-02 03:45', '45.22.11.9',   'DATA_XFER',  1500, 'ANOMALY'),
        (4, '2047-04-02 03:50', '192.168.1.55', 'SYS_WIPE',   0,    'WARNING'),
        (5, '2047-04-02 04:00', '192.168.1.15', 'LOGOUT',     0,    'NORMAL');
    )SQL");

    // ── vpn_registry ─────────────────────────────────────────────────────────
    exec1(this,"vpn_registry", R"SQL(
    INSERT INTO vpn_registry VALUES
        ('E-102', 'CorpNet',       '192.168.1.22', '2047-04-02 02:00'),
        ('E-101', 'GhostProtocol', '45.22.11.9',   '2047-04-02 03:40'),
        ('E-103', 'SafeSurf',      '192.168.1.55', '2047-04-02 03:48');
    )SQL");

    // ── emails ───────────────────────────────────────────────────────────────
    exec1(this,"emails", R"SQL(
    INSERT INTO emails VALUES
        (1, '2047-04-01 09:00', 'E-101', 'E-103', 'Server Maint', 'Please clear the audit logs tonight.', 0),
        (2, '2047-04-01 14:22', 'E-103', 'E-101', 'Re: Maint', 'Will do. I will run the SYS_WIPE at 03:50.', 0),
        (3, '2047-04-01 18:30', 'E-101', 'EXT-1', 'Delivery', '[ENCRYPTED PAYLOAD]', 1),
        (4, '2047-04-02 04:10', 'EXT-1', 'E-101', 'Confirmation', 'The bird has flown. Funds transferred.', 0);
    )SQL");

    // ── bank_transfers ───────────────────────────────────────────────────────
    exec1(this,"bank_transfers", R"SQL(
    INSERT INTO bank_transfers VALUES
        ('TX-991', '2047-04-01 10:00', 'NovaCorp', 'E-101', 5000,   'USD', 'PAYROLL'),
        ('TX-992', '2047-04-02 04:05', 'ApexTech', 'Shell-77', 500000, 'USD', 'WIRE'),
        ('TX-993', '2047-04-02 04:30', 'Shell-77', 'E-101', 450000, 'USD', 'WIRE_FWD'),
        ('TX-994', '2047-04-02 04:35', 'Shell-77', 'E-103', 50000,  'USD', 'WIRE_FWD');
    )SQL");

    // ── decrypted_emails ─────────────────────────────────────────────────────
    exec1(this,"decrypted_emails", R"SQL(
    INSERT INTO decrypted_emails VALUES
        (1, 3, 'I am initiating the 1.5GB transfer of Bluebird tonight via GhostProtocol. David is handling the log wipes.');
    )SQL");

    // ── security_alerts ──────────────────────────────────────────────────────
    exec1(this,"security_alerts", R"SQL(
    INSERT INTO security_alerts VALUES
        (101, '2047-04-02 03:45', 'FIREWALL', 'Massive outbound data spike detected (1500MB).', 'E-103'),
        (102, '2047-04-02 03:50', 'SYS_MON',  'Manual audit log wipe initiated.', 'E-103');
    )SQL");

    // Verify all tables have data
    auto check = [&](const char* t, int expected) {
        int n = get_row_count(t);
        if (n < expected)
            std::cerr << "[DB] WARNING: " << t << " has " << n << " rows (expected " << expected << ")\n";
    };
    check("project_files", 3); check("employees", 5); check("network_logs", 5);
    check("vpn_registry", 3); check("emails", 4); check("bank_transfers", 4);
    check("decrypted_emails", 1); check("security_alerts", 2);

    return true;
}