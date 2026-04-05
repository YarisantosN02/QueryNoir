#pragma once
#include "Types.h"
#include "Database.h"
#include <functional>
#include <string>
#include <vector>
#include <deque>

class ICase; 

using ClueCallback   = std::function<void(const Clue&)>;
using UnlockCallback = std::function<void(const TableInfo&)>;

class GameState {
public:
    GameState();
    ~GameState(); // Just the declaration!

    // Main entry point
    QueryResult run_query(const std::string& sql);
    bool try_accuse(const std::string& name); 

    // Accessors
    AppState&                   app()    { return m_app; }
    Database&                   get_db() { return m_db; }
    std::deque<NarrativeEntry>& feed()   { return m_feed; }
    std::vector<Notification>&  notifs() { return m_notifications; }
    std::vector<TableInfo>&     tables() { return m_tables; }
    const std::vector<TableInfo>& tables() const { return m_tables; }
    std::vector<Clue>&          clues()  { return m_clues; }
    const std::vector<Clue>&    clues() const { return m_clues; } 
    std::vector<std::string>&   history(){ return m_query_history; }
    const QueryResult&          last_result() const { return m_last_result; }

    // DATA vs LOGIC SEPARATION
    const Case& get_current_case() const { return m_current_case_data; }
    void set_current_case(const Case& c) { m_current_case_data = c; }

    ICase* get_case_ptr() const { return m_current_case_ptr; }
    
    void set_case_ptr(ICase* new_case); 

    void update(float dt);
    void reset();
    void push_narrative(NarrativeType type, const std::string& text);
    void push_notification(NotifType type, const std::string& msg);
    void unlock_table(const std::string& name);

    void on_clue_found(ClueCallback cb)       { m_clue_cb   = cb; }
    void on_table_unlocked(UnlockCallback cb) { m_unlock_cb = cb; }

    bool can_accuse() const;
    bool is_solved() const { return m_app.status == GameStatus::CASE_SOLVED; }

    static std::string upper(const std::string& s);
    static bool sql_has(const std::string& up, const std::string& kw);
    static bool result_has_cell(const QueryResult& r, const std::string& substr);

private:
    AppState                   m_app;
    Database                   m_db;
    QueryResult                m_last_result;
    std::deque<NarrativeEntry> m_feed;
    std::vector<Notification>  m_notifications;
    std::vector<TableInfo>     m_tables;
    std::vector<Clue>          m_clues;
    std::vector<std::string>   m_query_history;
    ClueCallback               m_clue_cb;
    UnlockCallback             m_unlock_cb;

    Case                       m_current_case_data; 
    ICase* m_current_case_ptr = nullptr;

    void check_puzzles(const std::string& sql, const QueryResult& result);
    void check_unlocks(const std::string& sql, const QueryResult& result);
    void add_context_narrative(const std::string& sql, const QueryResult& result);
    void flag_rows(QueryResult& result);

    bool puzzle_satisfied(const PuzzleCondition& p,
                          const std::string& sql_up,
                          const QueryResult& result) const;
};