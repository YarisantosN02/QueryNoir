#include "Audio.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cstdlib>

// ─── Oscillators ─────────────────────────────────────────────────────────────
float Audio::osc_sine(float& ph, float freq, int sr){
    float v = sinf(ph * 2.f * 3.14159265f);
    ph += freq / sr;
    if(ph >= 1.f) ph -= 1.f;
    return v;
}
float Audio::osc_saw(float& ph, float freq, int sr){
    float v = 2.f*ph - 1.f;
    ph += freq / sr;
    if(ph >= 1.f) ph -= 1.f;
    return v;
}
float Audio::osc_sq(float& ph, float freq, float duty, int sr){
    float v = ph < duty ? 1.f : -1.f;
    ph += freq / sr;
    if(ph >= 1.f) ph -= 1.f;
    return v;
}
float Audio::envelope(float t, float atk, float dec, float sus, float rel, float dur){
    if(t < atk)                  return t/atk;
    if(t < atk+dec)              return 1.f - (1.f-sus)*((t-atk)/dec);
    if(t < dur-rel)              return sus;
    if(t < dur)                  return sus*(1.f-(t-(dur-rel))/rel);
    return 0.f;
}
// ─── Per-voice synthesis ──────────────────────────────────────────────────────
float Audio::synthesize_voice(Voice& v, float dt_s){
    float sr = (float)m_sample_rate;
    float t  = v.t;
    float dur= v.duration;
    float out= 0.f;

    switch(v.type){

    case SFX::KEYCLICK: {
        // Short noise burst + 2kHz click, 30ms
        float env = envelope(t,0.001f,0.015f,0.0f,0.010f,dur);
        float click = osc_sine(v.phase, 2200.f, (int)sr) * 0.3f;
        v.phase2 += 0.37f; if(v.phase2>6.28f) v.phase2-=6.28f;
        float n_val = sinf(v.phase2)*0.7f;
        out = (click + n_val) * env;
        break;
    }
    case SFX::KEYCLICK_SOFT: {
        float env = envelope(t,0.001f,0.012f,0.0f,0.008f,dur);
        float click = osc_sine(v.phase, 1400.f, (int)sr) * 0.25f;
        v.phase2 += 0.22f; if(v.phase2>6.28f) v.phase2-=6.28f;
        out = (click + sinf(v.phase2)*0.5f) * env;
        break;
    }
    case SFX::EXECUTE: {
        // Rising frequency beep — digital charge-up feel
        float progress = t / dur;
        float freq = 180.f + progress*progress * 2200.f;
        float env  = envelope(t,0.005f,0.05f,0.6f,0.08f,dur);
        float sq   = osc_sq(v.phase, freq, 0.35f, (int)sr);
        float sine = osc_sine(v.phase2, freq*1.5f, (int)sr) * 0.3f;
        out = (sq*0.55f + sine) * env;
        break;
    }
    case SFX::UNLOCK: {
        // Two-tone ascending chime — 3rd + 5th interval
        float f1 = 660.f, f2 = 990.f;
        float env = envelope(t,0.01f,0.08f,0.4f,0.25f,dur);
        float a = osc_sine(v.phase,  f1, (int)sr);
        float b = osc_sine(v.phase2, f2, (int)sr) * 0.6f;
        out = (a+b) * env;
        break;
    }
    case SFX::CLUE: {
        // Crystalline ping — pure sine with long decay
        float env = envelope(t,0.005f,0.02f,0.7f,0.40f,dur);
        float a = osc_sine(v.phase,  1320.f,(int)sr);
        float b = osc_sine(v.phase2, 1980.f,(int)sr)*0.4f;
        out = (a+b)*env;
        break;
    }
    case SFX::ERROR_BEEP: {
        // Descending buzzer — alarm feel
        float freq = 320.f - (t/dur)*160.f;
        float env  = envelope(t,0.01f,0.05f,0.5f,0.1f,dur);
        float sq   = osc_sq(v.phase, freq, 0.6f, (int)sr);
        out = sq * env;
        break;
    }
    case SFX::GLITCH: {
        // Rapid noise burst with pitch flutter
        float freq  = 800.f + sinf(t*80.f)*600.f;
        float env   = envelope(t,0.002f,0.03f,0.0f,0.02f,dur);
        v.phase2 += 0.41f; if(v.phase2>6.28f) v.phase2-=6.28f;
        float glit  = osc_sq(v.phase,freq,0.5f,(int)sr)*0.5f
                    + sinf(v.phase2)*0.5f;
        out = glit * env;
        break;
    }
    case SFX::TERMINAL_BOOT: {
        // Rising sweep + crackle — terminal powering on
        float progress = t/dur;
        float freq = 60.f + progress*progress*1200.f;
        float env  = envelope(t,0.05f,0.2f,0.5f,0.3f,dur);
        float saw  = osc_saw(v.phase, freq, (int)sr)*0.4f;
        float ping = osc_sine(v.phase2, freq*2.f, (int)sr)*0.2f;
        v.noise_phase_local += 0.5f;
        if(v.noise_phase_local > 6.28f) v.noise_phase_local -= 6.28f;
        float crackle = sinf(v.noise_phase_local * 7.3f) * (1.f-progress)*0.15f;
        out = (saw + ping + crackle) * env;
        break;
    }
    case SFX::ACCUSE: {
        // Dramatic descending sting — three notes
        float third = dur / 3.f;
        float freqs[3] = { 880.f, 660.f, 440.f };
        int   note = std::min((int)(t/third), 2);
        float nt   = t - note*third;
        float env  = envelope(nt,0.005f,0.02f,0.6f,0.08f,third);
        float a = osc_sine(v.phase, freqs[note], (int)sr);
        float b = osc_sine(v.phase2, freqs[note]*0.5f,(int)sr)*0.3f;
        out = (a+b)*env;
        break;
    }
    case SFX::SOLVED: {
        // Resolution chord — major triad arpeggiated
        float third = dur / 4.f;
        float freqs[4] = { 330.f, 415.f, 495.f, 660.f };
        int note = std::min((int)(t/third), 3);
        float nt = t - note*third;
        float env = envelope(nt,0.008f,0.05f,0.7f,0.15f,third);
        float a = osc_sine(v.phase, freqs[note], (int)sr);
        float b = osc_sine(v.phase2, freqs[note]*2.f, (int)sr)*0.2f;
        out = (a+b)*env*0.9f;
        break;
    }
    } // switch

    v.t += dt_s;
    if(v.t >= v.duration) v.done = true;
    return out * v.volume * m_sfx_vol;
}

// ─── Ambient synthesizer ──────────────────────────────────────────────────────
float Audio::synthesize_ambient(float dt_s){
    if(!m_music_vol) return 0.f;

    float sr  = (float)m_sample_rate;
    float out = 0.f;

    // LFO for slow tremolo
    m_lfo_phase += 0.4f / sr;
    if(m_lfo_phase > 1.f) m_lfo_phase -= 1.f;
    float lfo = 0.5f + 0.5f*sinf(m_lfo_phase * 6.2831f);

    // Layer 0 — deep root drone (55 Hz A1)
    // Layer 1 — fifth above (82.5 Hz E2)  
    // Layer 2 — tension layer (unlocked by intensity, higher + slight dissonance)

    float* ph = m_amb_ph;

    // Root
    float root = osc_sine(ph[0], 55.f, (int)sr)*0.5f
               + osc_sine(ph[1], 55.f*2.01f, (int)sr)*0.25f; // slight chorus
    root *= (0.7f + 0.3f*lfo);

    // Fifth
    float fifth = osc_sine(ph[2], 82.5f, (int)sr)*0.35f
                + osc_saw  (ph[3], 82.5f, (int)sr)*0.08f;
    fifth *= (0.5f + 0.5f*lfo);

    // Tension layer — high tension cluster
    float tension_target = m_intensity * 0.4f;
    m_tension_vol += (tension_target - m_tension_vol) * dt_s * 0.8f;

    float tension = osc_sq (ph[4], 220.f,  0.4f, (int)sr)*0.3f
                  + osc_sine(ph[5], 233.f, (int)sr)*0.2f; // tritone dissonance
    tension *= m_tension_vol * lfo;

    out = (root + fifth + tension) * m_music_vol * m_master_vol;
    return std::max(-1.f, std::min(1.f, out));
}

// ─── Audio callback ──────────────────────────────────────────────────────────
void Audio::audio_callback(void* ud, Uint8* stream, int len){
    Audio* a = static_cast<Audio*>(ud);
    int frames = len / sizeof(float);
    float* out = reinterpret_cast<float*>(stream);
    a->fill_buffer(out, frames);
}

void Audio::fill_buffer(float* out, int frames){
    float dt_s = 1.f / m_sample_rate;
    for(int i=0; i<frames; i++) out[i] = 0.f;

    if(!enabled){ return; }

    std::lock_guard<std::mutex> lk(m_mtx);

    for(int i=0; i<frames; i++){
        float s = synthesize_ambient(dt_s);
        // Mix voices
        for(auto& v : m_voices)
            if(!v.done)
                s += synthesize_voice(v, dt_s);
        s *= m_master_vol;
        out[i] = std::max(-1.f, std::min(1.f, s));
    }

    // Remove finished voices
    m_voices.erase(
        std::remove_if(m_voices.begin(), m_voices.end(),
            [](const Voice& v){ return v.done; }),
        m_voices.end());
}

// ─── Public API ──────────────────────────────────────────────────────────────
bool Audio::init(){
    SDL_AudioSpec want{};
    want.freq     = m_sample_rate;
    want.format   = AUDIO_F32SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = audio_callback;
    want.userdata = this;

    m_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &m_spec, 0);
    if(!m_dev){
        std::cerr<<"[Audio] SDL_OpenAudioDevice failed: "<<SDL_GetError()<<"\n";
        return false;
    }
    SDL_PauseAudioDevice(m_dev, 0);
    std::cerr<<"[Audio] OK — "<<m_spec.freq<<"Hz\n";
    return true;
}

void Audio::shutdown(){
    if(m_dev){ SDL_CloseAudioDevice(m_dev); m_dev=0; }
}

void Audio::play(SFX sfx, float vol){
    if(!enabled || !m_dev) return;

    Voice v;
    v.type   = sfx;
    v.volume = vol;
    v.t      = 0.f;
    v.phase  = 0.f;
    v.phase2 = 0.f;

    switch(sfx){
        case SFX::KEYCLICK:       v.duration = 0.035f; break;
        case SFX::KEYCLICK_SOFT:  v.duration = 0.025f; break;
        case SFX::EXECUTE:        v.duration = 0.28f;  break;
        case SFX::UNLOCK:         v.duration = 0.55f;  break;
        case SFX::CLUE:           v.duration = 0.70f;  break;
        case SFX::ERROR_BEEP:     v.duration = 0.22f;  break;
        case SFX::GLITCH:         v.duration = 0.07f;  break;
        case SFX::TERMINAL_BOOT:  v.duration = 1.80f;  break;
        case SFX::ACCUSE:         v.duration = 0.90f;  break;
        case SFX::SOLVED:         v.duration = 1.60f;  break;
    }

    std::lock_guard<std::mutex> lk(m_mtx);
    // Limit voice polyphony — keep last 8
    if(m_voices.size() > 8) m_voices.pop_front();
    m_voices.push_back(v);
}

void Audio::set_music_intensity(float t){
    m_intensity = std::max(0.f, std::min(1.f, t));
}

void Audio::update(float dt){
    // Could smooth music transitions here
}

// end of Audio.cpp
