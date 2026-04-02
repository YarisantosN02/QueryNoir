#pragma once
#include <SDL.h>
#include <vector>
#include <cmath>
#include <cstdint>
#include <functional>
#include <deque>
#include <mutex>

// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Procedural Audio System
//  All sound is synthesized at runtime — no audio files required.
//  Uses SDL2 audio callback to mix a dark ambient drone + one-shot SFX.
// ═══════════════════════════════════════════════════════════════════════════

// ── Sound effect identifiers ─────────────────────────────────────────────────
enum class SFX {
    KEYCLICK,      // mechanical key press — short tick
    KEYCLICK_SOFT, // softer tick for held keys
    EXECUTE,       // query execution — rising digital beep
    UNLOCK,        // table unlock — ascending chime
    CLUE,          // clue found — crystalline ping
    ERROR_BEEP,    // SQL error — low buzzer
    GLITCH,        // screen glitch — noise burst
    TERMINAL_BOOT, // startup sequence
    ACCUSE,        // accusation — dramatic sting
    SOLVED,        // case closed — resolution chord
};

// ── One-shot voice ────────────────────────────────────────────────────────────
struct Voice {
    float phase              = 0.f;
    float phase2             = 0.f;
    float t                  = 0.f;
    float duration           = 0.f;
    float volume             = 0.8f;
    SFX   type               = SFX::KEYCLICK;
    bool  done               = false;
    float noise_phase_local  = 0.f;
};

// ── Ambient layer ─────────────────────────────────────────────────────────────
struct AmbientLayer {
    float phase   = 0.f;
    float freq    = 55.f;  // Hz
    float volume  = 0.f;   // current (lerped)
    float target  = 0.12f;
    float detune  = 0.f;   // slight detune for chorus
};

class Audio {
public:
    static Audio& get() { static Audio a; return a; }

    bool init();
    void shutdown();
    void play(SFX sfx, float vol=1.0f);
    void set_music_intensity(float t); // 0=quiet, 1=tense
    void update(float dt);             // call each frame

    bool enabled = true;

private:
    Audio() = default;
    ~Audio() { shutdown(); }

    SDL_AudioDeviceID m_dev = 0;
    SDL_AudioSpec     m_spec{};

    std::mutex              m_mtx;
    std::deque<Voice>       m_voices;
    std::vector<AmbientLayer> m_layers;

    float m_master_vol   = 0.70f;
    float m_music_vol    = 0.28f;
    float m_sfx_vol      = 0.72f;
    float m_noise_phase  = 0.f;
    float m_lfo_phase    = 0.f;
    float m_intensity    = 0.f;
    float m_tension_vol  = 0.f;
    int   m_sample_rate  = 44100;
    float m_amb_ph[6]    = {}; // ambient oscillator phases

    // Synthesis helpers (called from audio callback — must be fast & lock-free)
    static float osc_sine(float& phase, float freq, int sr);
    static float osc_saw (float& phase, float freq, int sr);
    static float osc_sq  (float& phase, float freq, float duty, int sr);
    static float envelope(float t, float atk, float dec, float sus, float rel, float dur);

    float synthesize_voice(Voice& v, float dt_sample);
    float synthesize_ambient(float dt_sample);

    static void audio_callback(void* userdata, Uint8* stream, int len);
    void fill_buffer(float* out, int frames);
};
