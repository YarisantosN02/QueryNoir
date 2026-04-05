#pragma once
#include <string>
#include <vector>
#include <functional>
#include <deque>
#include <map>

// ─── Game Status ──────────────────────────────────────────────────────────────
enum class GameStatus { ACTIVE, ACCUSE_MODE, PLAYING, CASE_SOLVED, CASE_FAILED };

// ─── Query Result ─────────────────────────────────────────────────────────────
struct QueryResult {
    std::vector<std::string>              columns;
    std::vector<std::vector<std::string>> rows;
    std::string                           error;
    bool                                  is_error    = false;
    std::vector<size_t>                   flagged_rows;
};

// ─── Narrative ────────────────────────────────────────────────────────────────
enum class NarrativeType { MONOLOGUE, HINT, ALERT, SUCCESS, SYSTEM };

struct NarrativeEntry {
    NarrativeType type;
    std::string   text;
    float         reveal_timer = 0.f;
    bool          revealed     = false;
};

// ─── Notification ─────────────────────────────────────────────────────────────
enum class NotifType { INFO, ERROR_MSG, CLUE, TABLE_UNLOCKED, UNLOCK, SUCCESS };

struct Notification {
    NotifType   type;
    std::string message;
    float       lifetime = 5.f;
    float       age      = 0.f;
};

// ─── Column info for schema display ──────────────────────────────────────────
struct ColumnInfo {
    std::string name;
    std::string type;
    std::string note; // e.g. "PK", "nullable"
};

struct TableInfo {
    std::string              name;
    std::string              display_name;
    bool                     unlocked        = false;
    std::string              unlock_condition; // human-readable hint shown when locked
    std::vector<ColumnInfo>  columns;          // full schema info
    std::string              icon;
    int                      row_count       = 0;
};

// ─── Puzzle gate ──────────────────────────────────────────────────────────────
// A clue is earned only when the player writes a query that satisfies ALL conditions.
// Conditions are checked against the SQL text AND/OR the result rows.
struct PuzzleCondition {
    // SQL text requirements (case-insensitive substring checks on the query)
    std::vector<std::string> sql_must_contain;    // ALL must appear
    std::vector<std::string> sql_must_not_contain;// NONE may appear
    // Result requirements
    std::string result_must_contain_cell; // at least one cell in result must contain this
    int         min_rows = 0;             // result must have at least this many rows
};

struct Clue {
    std::string      id;
    std::string      title;
    std::string      description;      // shown once found
    std::string      hint;             // shown in left panel when not yet found
    bool             discovered  = false;
    PuzzleCondition  condition;         // what the player must query to earn this
};

// ─── Case ────────────────────────────────────────────────────────────────────
struct Case {
    std::string id;
    std::string title;
    std::string help_objective;
    std::string accusation_prompt; 
    std::string resolved_title;   
    std::string charged_name;     
    std::string accessory_name;   
    std::string resolution_text;

    const std::string& get_accusation_prompt() const { return accusation_prompt; }
    const std::string& get_resolved_title() const { return resolved_title; }
    const std::string& get_charged_name() const { return charged_name; }
    const std::string& get_accessory_name() const { return accessory_name; }
    const std::string& get_resolution_text() const { return resolution_text; }
};

// ─── Accusation ───────────────────────────────────────────────────────────────
struct AccuseState {
    bool          active           = false;
    std::string   input;           // what the player has typed
    std::string   wrong_feedback;  // shown when wrong
    float         wrong_timer      = 0.f;
};

// ─── App State ────────────────────────────────────────────────────────────────
struct AppState {
    GameStatus  status          = GameStatus::ACTIVE;
    int         discovered_clues = 0;
    float       game_time       = 0.f;
    bool        show_help       = false;
    bool        show_schema     = false;
    float       glitch_timer    = 0.f;
    bool        query_executing = false;
    float       exec_anim_timer = 0.f;
    AccuseState accuse;
};
