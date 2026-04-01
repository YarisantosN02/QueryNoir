#pragma once

// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Core Types & Constants
// ═══════════════════════════════════════════════════════════════════════════

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <deque>
#include <map>
#include <optional>
#include <chrono>

// ─── Color Palette ───────────────────────────────────────────────────────────
namespace Color {
    // ImVec4 equivalents as float[4]: {r, g, b, a}
    constexpr float BG_DEEP[4]       = {0.039f, 0.059f, 0.110f, 1.0f}; // #0A0F1C
    constexpr float NEON_BLUE[4]     = {0.000f, 0.831f, 1.000f, 1.0f}; // #00D4FF
    constexpr float NEON_PURPLE[4]   = {0.478f, 0.361f, 1.000f, 1.0f}; // #7A5CFF
    constexpr float DANGER_RED[4]    = {1.000f, 0.302f, 0.302f, 1.0f}; // #FF4D4D
    constexpr float SUCCESS_GREEN[4] = {0.196f, 0.902f, 0.588f, 1.0f}; // #32E696
    constexpr float PANEL_BG[4]      = {0.055f, 0.082f, 0.149f, 0.92f};
    constexpr float TEXT_DIM[4]      = {0.502f, 0.651f, 0.800f, 1.0f};
    constexpr float TEXT_BRIGHT[4]   = {0.867f, 0.933f, 1.000f, 1.0f};
    constexpr float GRID_LINE[4]     = {0.000f, 0.831f, 1.000f, 0.04f};
    constexpr float HIGHLIGHT_ROW[4] = {0.000f, 0.831f, 1.000f, 0.08f};
    constexpr float SUSPECT_GLOW[4]  = {1.000f, 0.302f, 0.302f, 0.15f};
}

// ─── Game Status ─────────────────────────────────────────────────────────────
enum class GameStatus { ACTIVE, PAUSED, CASE_SOLVED, CASE_FAILED };

// ─── Clue / Discovery ────────────────────────────────────────────────────────
struct Clue {
    std::string id;
    std::string title;
    std::string description;
    bool discovered = false;
    std::string trigger_keyword; // SQL keyword that reveals this clue
};

// ─── Query Result ─────────────────────────────────────────────────────────────
struct QueryResult {
    std::vector<std::string>              columns;
    std::vector<std::vector<std::string>> rows;
    std::string                           error;
    bool                                  is_error   = false;
    bool                                  is_suspicious_row(size_t i) const;
    std::vector<size_t>                   flagged_rows;   // rows that glow
    std::vector<size_t>                   flagged_cols;   // cols that highlight
};

// ─── Narrative Entry ─────────────────────────────────────────────────────────
enum class NarrativeType { MONOLOGUE, HINT, ALERT, SUCCESS, SYSTEM };

struct NarrativeEntry {
    NarrativeType       type;
    std::string         text;
    float               alpha        = 1.0f;
    float               reveal_timer = 0.0f; // 0→1 as text reveals char by char
    bool                revealed     = false;
};

// ─── Database Table Info ──────────────────────────────────────────────────────
struct TableInfo {
    std::string              name;
    std::string              display_name;
    bool                     unlocked = false;
    std::string              unlock_condition; // query keyword needed
    std::vector<std::string> columns;
    std::string              icon; // emoji or text icon
};

// ─── Case ─────────────────────────────────────────────────────────────────────
struct Case {
    std::string            id;
    std::string            title;
    std::string            subtitle;
    std::string            db_path;         // SQLite file for this case
    std::vector<Clue>      clues;
    std::vector<TableInfo> tables;
    std::string            solve_condition; // SQL or clue combo needed
    std::string            opening_text;
    std::string            solved_text;
};

// ─── Notification ────────────────────────────────────────────────────────────
enum class NotifType { INFO, ERROR_MSG, CLUE, UNLOCK, SUCCESS };

struct Notification {
    NotifType   type;
    std::string message;
    float       lifetime = 3.0f;
    float       age      = 0.0f;
};

// ─── Global App State ────────────────────────────────────────────────────────
struct AppState {
    GameStatus      status         = GameStatus::ACTIVE;
    Case            current_case;
    int             discovered_clues = 0;
    float           game_time       = 0.0f; // seconds elapsed
    bool            show_help       = false;
    float           glitch_timer    = 0.0f; // for screen glitch effects
    bool            query_executing = false;
    float           exec_anim_timer = 0.0f;
};
