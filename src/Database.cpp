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

QueryResult Database::run_query(const std::string& sql) {
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
    auto r=run_query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(!row.empty()) v.push_back(row[0]);
    return v;
}
std::vector<std::string> Database::get_column_names(const std::string& t) {
    auto r=run_query("PRAGMA table_info("+t+");");
    std::vector<std::string> v;
    for (auto& row:r.rows) if(row.size()>=2) v.push_back(row[1]);
    return v;
}
std::vector<std::pair<std::string,std::string>> Database::get_column_info(const std::string& t) {
    auto r=run_query("PRAGMA table_info("+t+");");
    std::vector<std::pair<std::string,std::string>> v;
    for (auto& row:r.rows) if(row.size()>=3) v.push_back({row[1],row[2]});
    return v;
}
int Database::get_row_count(const std::string& t) {
    auto r=run_query("SELECT COUNT(*) FROM "+t+";");
    if(!r.rows.empty()&&!r.rows[0].empty()) {
        try { return std::stoi(r.rows[0][0]); } catch(...) {}
    }
    return 0;
}

// Helper — run one statement, log failure but continue
static bool exec1(Database* db, const char* label, const std::string& sql) {
    auto r = db->run_query(sql);
    if (r.is_error) {
        std::cerr << "[DB] FAILED [" << label << "]: " << r.error << "\n";
        return false;
    }
    return true;
}

bool Database::seed_from_script(const std::string& sql_script) {
    // It simply takes the script handed to it by the case and executes it.
    auto result = run_query(sql_script);
    
    if (result.is_error) {
        std::cerr << "[DB] Seeding failed: " << result.error << "\n";
        return false;
    }
    
    return true;
}

