#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Intro Sequence
//  A beat-by-beat animated intro that plays before the game starts.
//  Rendered entirely with ImGui drawlists — no textures required.
// ═══════════════════════════════════════════════════════════════════════════

enum class IntroPhase {
    BLACKOUT,         // 0.0 – 0.8s   — pure black silence
    LOGO_FLICKER,     // 0.8 – 2.2s   — "NOVACORP" logo glitches in
    TIMESTAMP,        // 2.2 – 3.5s   — "2047-03-16  02:14:00" typed out
    LOCATION,         // 3.5 – 4.8s   — "SERVER ROOM B-7" typed out
    HEARTBEAT,        // 4.8 – 6.5s   — pulsing red circle — life sign flat-lining
    FLATLINE,         // 6.5 – 8.0s   — flatline — "SUBJECT DECEASED"
    CASE_TITLE,       // 8.0 – 10.5s  — "CASE #0447 — THE ORION MURDER" slams in
    STATIC,           // 10.5 – 11.5s — full screen static/noise
    TERMINAL_BOOT,    //11.5 – 13.5s  — forensics terminal booting
    DONE,             // sequence complete — transition to game
};

struct IntroState {
    IntroPhase phase       = IntroPhase::BLACKOUT;
    float      phase_t     = 0.f;   // seconds since this phase started
    float      total_t     = 0.f;   // total elapsed seconds
    bool       complete    = false;
    bool       skipped     = false;

    // Typewriter state
    int        typed_chars = 0;

    // Effects
    float      glitch_intensity = 0.f;
    float      flicker_val      = 1.f;
    float      heartbeat_phase  = 0.f;
    float      static_seed      = 0.f;
    uint32_t   rng              = 12345u;

    // Boot lines (shown during TERMINAL_BOOT)
    int        boot_line    = 0;
    float      boot_line_t  = 0.f;
};

// All intro rendering lives in one function — call each frame
// Returns true when the intro is complete.
// draw_intro() mutates state and renders into the current ImGui frame.
bool draw_intro(IntroState& s, float dt, float W, float H,
                std::function<void(float)> play_sfx_cb);
