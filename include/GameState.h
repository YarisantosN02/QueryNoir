#pragma once
#include "Types.h"
#include "Database.h"
#include <functional>
#include <string>
#include <vector>
#include <deque>

using ClueCallback   = std::function<void(const Clue&)>;
using UnlockCallback = std::function<void(const TableInfo&)>;

class GameState {
public:
    GameState();
    void init_case_orion();
    void init_case_espionage();

    // Main entry point — runs query, checks puzzles, returns displayable result
    QueryResult run_query(const std::string& sql);

    // Accusation mechanic
    bool try_accuse(const std::string& name); // returns true if correct

    // Accessors
    AppState&                    app()    { return m_app; }
    std::deque<NarrativeEntry>&  feed()  { return m_feed; }
    std::vector<Notification>&   notifs(){ return m_notifications; }
    std::vector<TableInfo>&      tables(){ return m_tables; }
    std::vector<Clue>&           clues() { return m_clues; }
    std::vector<std::string>&    history(){ return m_query_history; }
    const QueryResult&           last_result() const { return m_last_result; }
    const Case&                  get_current_case() const { return m_current_case; }

    void update(float dt);
    void reset();
    void push_narrative(NarrativeType type, const std::string& text);
    void push_notification(NotifType type, const std::string& msg);

    // Callback registration
    void on_clue_found(ClueCallback cb)      { m_clue_cb   = cb; }
    void on_table_unlocked(UnlockCallback cb) { m_unlock_cb = cb; }

    // Returns true when player has enough clues to accuse
    bool can_accuse() const;
    // Returns true when case is solved
    bool is_solved() const { return m_app.status == GameStatus::CASE_SOLVED; }

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
    Case                       m_current_case;

    // Puzzle engine
    void check_puzzles(const std::string& sql, const QueryResult& result);
    void check_unlocks(const std::string& sql, const QueryResult& result);
    void add_context_narrative(const std::string& sql, const QueryResult& result);
    void flag_rows(QueryResult& result);

    // Helpers
    static std::string upper(const std::string& s);
    static bool sql_has(const std::string& up, const std::string& kw);
    static bool result_has_cell(const QueryResult& r, const std::string& substr);
    bool puzzle_satisfied(const PuzzleCondition& p,
                          const std::string& sql_up,
                          const QueryResult& result) const;
};
