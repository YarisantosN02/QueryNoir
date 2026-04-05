#pragma once
#include <string>
#include <vector>

class GameState;
struct QueryResult;

class ICase {
public:
    virtual ~ICase() = default;
    
    virtual void init(GameState& state) = 0; 
    virtual std::string get_sql_script() = 0;

    virtual void check_unlocks(GameState& state, const std::string& sql, const QueryResult& result) = 0;
    virtual void add_context_narrative(GameState& state, const std::string& sql, const QueryResult& result) = 0;
    virtual bool try_accuse(GameState& state, const std::string& name) = 0;
    virtual std::vector<std::string> get_flag_keywords() = 0;
    virtual bool can_accuse(const GameState& state) = 0;
    virtual bool seed_data(Database& db) = 0;
};