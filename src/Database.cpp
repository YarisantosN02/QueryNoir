#include "Database.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

// ─────────────────────────────────────────────────────────────────────────────

Database::Database() {}

Database::~Database() { close(); }

bool Database::open(const std::string& path) {
    close();
    int rc = sqlite3_open(path == ":memory:" ? ":memory:" : path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] Cannot open: " << sqlite3_errmsg(m_db) << "\n";
        m_db = nullptr;
        return false;
    }
    return true;
}

void Database::close() {
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

// ─── Row callback ────────────────────────────────────────────────────────────

struct CallbackData {
    QueryResult* result;
    bool         headers_set = false;
};

int Database::row_callback(void* data, int argc, char** argv, char** col_names) {
    auto* cbd = static_cast<CallbackData*>(data);
    if (!cbd->headers_set) {
        for (int i = 0; i < argc; i++)
            cbd->result->columns.push_back(col_names[i] ? col_names[i] : "");
        cbd->headers_set = true;
    }
    std::vector<std::string> row;
    for (int i = 0; i < argc; i++)
        row.push_back(argv[i] ? argv[i] : "NULL");
    cbd->result->rows.push_back(std::move(row));
    return 0;
}

// ─── Execute ─────────────────────────────────────────────────────────────────

QueryResult Database::execute(const std::string& sql) {
    QueryResult result;
    if (!m_db) {
        result.is_error = true;
        result.error    = "No database open.";
        return result;
    }

    CallbackData cbd{ &result };
    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), row_callback, &cbd, &err_msg);
    if (rc != SQLITE_OK) {
        result.is_error = true;
        result.error    = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
    }
    return result;
}

// ─── Introspection ────────────────────────────────────────────────────────────

std::vector<std::string> Database::get_table_names() {
    auto result = execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");
    std::vector<std::string> names;
    for (auto& row : result.rows)
        if (!row.empty()) names.push_back(row[0]);
    return names;
}

std::vector<std::string> Database::get_column_names(const std::string& table) {
    auto result = execute("PRAGMA table_info(" + table + ");");
    std::vector<std::string> cols;
    // PRAGMA table_info: cid, name, type, notnull, dflt_value, pk
    for (auto& row : result.rows)
        if (row.size() >= 2) cols.push_back(row[1]);
    return cols;
}

// ─── Case Seeding (Case 1: Orion Murder) ─────────────────────────────────────

bool Database::seed_case(const Case& c) {
    // We always seed case "orion_murder" with hard-coded data
    // In a real game, you'd parse JSON/SQL seed files from disk

    const char* schema = R"SQL(

    CREATE TABLE IF NOT EXISTS suspects (
        id          INTEGER PRIMARY KEY,
        name        TEXT NOT NULL,
        age         INTEGER,
        occupation  TEXT,
        alibi       TEXT,
        relationship_to_victim TEXT
    );

    CREATE TABLE IF NOT EXISTS access_logs (
        id          INTEGER PRIMARY KEY,
        timestamp   TEXT NOT NULL,
        person_name TEXT NOT NULL,
        location    TEXT NOT NULL,
        action      TEXT NOT NULL,
        badge_id    TEXT
    );

    CREATE TABLE IF NOT EXISTS messages (
        id          INTEGER PRIMARY KEY,
        sender      TEXT NOT NULL,
        receiver    TEXT NOT NULL,
        timestamp   TEXT NOT NULL,
        content     TEXT NOT NULL,
        encrypted   INTEGER DEFAULT 0
    );

    CREATE TABLE IF NOT EXISTS transactions (
        id          INTEGER PRIMARY KEY,
        from_account TEXT NOT NULL,
        to_account   TEXT NOT NULL,
        amount       REAL NOT NULL,
        timestamp    TEXT NOT NULL,
        note         TEXT
    );

    CREATE TABLE IF NOT EXISTS victims (
        id          INTEGER PRIMARY KEY,
        name        TEXT NOT NULL,
        age         INTEGER,
        occupation  TEXT,
        time_of_death TEXT,
        location_found TEXT,
        cause_of_death TEXT
    );

    )SQL";

    if (execute(schema).is_error) return false;

    const char* seed = R"SQL(

    INSERT INTO victims VALUES
        (1, 'Marcus Orion', 42, 'Senior Data Architect', '2047-03-15 02:14:00',
         'Server Room B-7, NovaCorp HQ', 'Asphyxiation');

    INSERT INTO suspects VALUES
        (1, 'Elena Vasquez', 35, 'Head of Security', 'Home — unverified', 'Colleague'),
        (2, 'Dorian Kast',   29, 'Junior Developer',  'At the gym — 22:00-23:30', 'Direct report of victim'),
        (3, 'Hana Mori',     44, 'CFO',               'Client dinner — verified until 21:00', 'Supervisor'),
        (4, 'Rex Calloway',  51, 'External Contractor','Hotel — key card confirms 20:00 check-in', 'Unknown'),
        (5, 'Lena Park',     31, 'Data Analyst',       'Unknown', 'Colleague and ex-partner');

    INSERT INTO access_logs VALUES
        (1,  '2047-03-15 19:45:00', 'Marcus Orion',   'Main Lobby',     'ENTRY',   'A-001'),
        (2,  '2047-03-15 20:01:00', 'Elena Vasquez',  'Security Office','ENTRY',   'S-007'),
        (3,  '2047-03-15 20:33:00', 'Dorian Kast',    'Main Lobby',     'ENTRY',   'D-042'),
        (4,  '2047-03-15 21:00:00', 'Hana Mori',      'Main Lobby',     'EXIT',    'M-003'),
        (5,  '2047-03-15 21:15:00', 'Lena Park',      'Main Lobby',     'ENTRY',   'L-019'),
        (6,  '2047-03-15 21:47:00', 'Dorian Kast',    'Main Lobby',     'EXIT',    'D-042'),
        (7,  '2047-03-15 22:30:00', 'Lena Park',      'Server Room B-7','ENTRY',   'L-019'),
        (8,  '2047-03-15 22:31:00', 'Marcus Orion',   'Server Room B-7','ENTRY',   'A-001'),
        (9,  '2047-03-15 23:58:00', 'Elena Vasquez',  'Server Room B-7','ENTRY',   'S-007'),
        (10, '2047-03-16 00:15:00', 'Elena Vasquez',  'Server Room B-7','EXIT',    'S-007'),
        (11, '2047-03-16 00:16:00', 'Lena Park',      'Server Room B-7','EXIT',    'L-019'),
        (12, '2047-03-16 02:14:00', '[UNKNOWN]',      'Server Room B-7','OVERRIDE','MASTER');

    INSERT INTO messages VALUES
        (1, 'Lena Park',    'Marcus Orion', '2047-03-14 18:22:00',
            'We need to talk. Tonight. Do NOT involve Vasquez.', 0),
        (2, 'Marcus Orion', 'Lena Park',    '2047-03-14 18:45:00',
            'I found something in the Q4 ledger. Meet me at 22:30.', 0),
        (3, 'Hana Mori',    'Elena Vasquez','2047-03-15 09:10:00',
            '[ENCRYPTED — DECRYPTION KEY REQUIRED]', 1),
        (4, 'Elena Vasquez','Hana Mori',    '2047-03-15 09:45:00',
            '[ENCRYPTED — DECRYPTION KEY REQUIRED]', 1),
        (5, 'Lena Park',    'unknown_ext',  '2047-03-15 22:15:00',
            'He has the files. I can not let him expose this.', 0),
        (6, 'Rex Calloway', 'Hana Mori',    '2047-03-12 14:00:00',
            'Transfer confirmed. The arrangement holds. — R', 0);

    INSERT INTO transactions VALUES
        (1, 'NovaCorp-Operations', 'ExtShell-LLC',     450000.00, '2047-01-05 00:00:00', 'Consulting — Q4'),
        (2, 'ExtShell-LLC',        'RCCalloway-Priv',  440000.00, '2047-01-06 00:00:00', 'Services rendered'),
        (3, 'NovaCorp-Operations', 'ExtShell-LLC',     450000.00, '2047-02-03 00:00:00', 'Consulting — Q1'),
        (4, 'ExtShell-LLC',        'RCCalloway-Priv',  440000.00, '2047-02-04 00:00:00', 'Services rendered'),
        (5, 'Mori-Private',        'LPark-Account',     12000.00, '2047-03-01 00:00:00', 'Bonus — discretionary'),
        (6, 'LPark-Account',       'CashOut-Terminal',  12000.00, '2047-03-10 00:00:00', NULL);

    )SQL";

    return !execute(seed).is_error;
}
