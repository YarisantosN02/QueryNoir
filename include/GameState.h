#pragma once
#include "Types.h"
#include "Database.h"
#include <functional>
#include <string>
#include <vector>
#include <deque>

// ─── Callback types ───────────────────────────────────────────────────────────
using ClueCallback   = std::function<void(const Clue&)>;
using UnlockCallback = std::function<void(const TableInfo&)>;

class GameState {
public:
    GameState();

    // Init
    void init_case_orion();

    // Query execution — main game loop entry
    QueryResult run_query(const std::string& sql);

    // Accessors
    AppState&               app()        { return m_app; }
    const AppState&         app()  const { return m_app; }
    std::deque<NarrativeEntry>& feed()  { return m_feed; }
    std::vector<Notification>&  notifs(){ return m_notifications; }
    std::vector<TableInfo>&     tables(){ return m_tables; }
    std::vector<Clue>&          clues() { return m_clues; }
    const QueryResult&          last_result() const { return m_last_result; }
    std::string&                query_history_at(int i);
    std::vector<std::string>&   history()     { return m_query_history; }

    // Update (call each frame with delta seconds)
    void update(float dt);

    // Callbacks
    void on_clue_found(ClueCallback cb)     { m_clue_cb   = cb; }
    void on_table_unlocked(UnlockCallback cb){ m_unlock_cb = cb; }

    // Helpers
    void push_narrative(NarrativeType type, const std::string& text);
    void push_notification(NotifType type, const std::string& msg);

    bool is_solved() const;

private:
    AppState                   m_app;
    Database                   m_db;
    QueryResult                m_last_result;
    std::deque<NarrativeEntry> m_feed;         // right-side narrative
    std::vector<Notification>  m_notifications;
    std::vector<TableInfo>     m_tables;
    std::vector<Clue>          m_clues;
    std::vector<std::string>   m_query_history;

    ClueCallback               m_clue_cb;
    UnlockCallback             m_unlock_cb;

    // Internals
    void analyse_query(const std::string& sql, const QueryResult& result);
    void check_clues(const std::string& sql, const QueryResult& result);
    void check_unlocks(const std::string& sql);
    void flag_suspicious_rows(QueryResult& result);
    bool sql_mentions(const std::string& sql, const std::string& keyword);
    std::string sql_upper(const std::string& sql);
};
