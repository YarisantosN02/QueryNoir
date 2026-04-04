#pragma once
#include "Types.h"
#include <sqlite3.h>
#include <string>
#include <vector>
#include <functional>

class Database {
public:
    Database();
    ~Database();

    bool        open(const std::string& path);
    void        close();
    bool        is_open() const { return m_db != nullptr; }

    QueryResult execute(const std::string& sql);

    // Seeding — called once to create the in-memory case DB
    bool        seed_case(const Case& c);
    bool        seed_espionage(const Case& c);

    // Introspection
    std::vector<std::string> get_table_names();
    std::vector<std::string> get_column_names(const std::string& table);
    std::vector<std::pair<std::string,std::string>> get_column_info(const std::string& table);
    int get_row_count(const std::string& table);

private:
    sqlite3*    m_db = nullptr;

    static int  row_callback(void* data, int argc, char** argv, char** col_names);
};
