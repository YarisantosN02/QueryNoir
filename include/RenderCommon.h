#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Shared Rendering State
//  Included by all rendering translation units.
//  Defines layout constants, color helpers, shared UI state, and
//  forward-declares all draw_* functions so each .cpp can call others.
// ═══════════════════════════════════════════════════════════════════════════

#include "imgui.h"
#include "Types.h"
#include "GameState.h"
#include "UITheme.h"
#include "Audio.h"
#include <string>
#include <cmath>
#include <cstdio>
#include <algorithm>

// ─── Window dimensions (set each frame from SDL, extern-linked in main.cpp) ──
extern float g_W;
extern float g_H;

// ─── Layout constants ─────────────────────────────────────────────────────────
static constexpr float TOP_H     = 48.f;
static constexpr float LEFT_W_F  = 0.20f;
static constexpr float RIGHT_W   = 290.f;
static constexpr float RESULTS_H = 255.f;

// ─── Global game state (defined in main.cpp) ──────────────────────────────────
extern GameState g_state;

// ─── Query console state (defined in QueryConsole.cpp) ────────────────────────
extern char        s_qbuf[4096];
extern int         s_hist_idx;
extern bool        s_focus_q;
extern QueryResult s_result;
extern bool        s_has_result;
extern int         s_prev_qbuf_len;

// ─── Accusation state (defined in Renderer.cpp) ───────────────────────────────
extern char s_accuse_buf[256];
extern bool s_accuse_focus;

// ─── Intro state (defined in main.cpp) ────────────────────────────────────────
#include "Intro.h"
extern IntroState g_intro;
extern bool       g_intro_done;

// ─── Color helpers (inline — safe to define in header, used in every TU) ──────
inline ImVec4 C_NEON  (float a=1.f){ return {0.f,   0.831f,1.f,   a}; }
inline ImVec4 C_PURPLE(float a=1.f){ return {0.478f,0.361f,1.f,   a}; }
inline ImVec4 C_RED   (float a=1.f){ return {1.f,   0.302f,0.302f,a}; }
inline ImVec4 C_GREEN (float a=1.f){ return {0.196f,0.902f,0.588f,a}; }
inline ImVec4 C_DIM   (float a=1.f){ return {0.502f,0.651f,0.8f,  a}; }
inline ImVec4 C_TEXT  (float a=1.f){ return {0.867f,0.933f,1.f,   a}; }
inline ImU32  U32(ImVec4 c)        { return ImGui::ColorConvertFloat4ToU32(c); }

// ─── Shared UI drawing helpers (implemented in Renderer.cpp) ──────────────────
void neon_sep(float alpha = 0.30f);
void bg_dots (float alpha = 0.018f);
void sec_label(const char* label, ImVec4 col);
void centered (const char* text,  ImVec4 col);
std::string fmt_time(float seconds);

// ─── Draw function declarations ───────────────────────────────────────────────
// Renderer.cpp
void draw_top_bar();
void draw_left_panel();
void draw_schema();
void draw_accuse_modal();
void draw_solved();
void draw_help();
void draw_notifications();

// QueryConsole.cpp
void draw_center();

// NarrativeFeed.cpp
void draw_right_panel();
