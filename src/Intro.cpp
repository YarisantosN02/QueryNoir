#include "Intro.h"
#include "Audio.h"
#include "imgui.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

// ─── Timing constants (seconds) ──────────────────────────────────────────────
static constexpr float T_BLACKOUT        = 0.8f;
static constexpr float T_LOGO_FLICKER    = 2.2f;
static constexpr float T_TIMESTAMP       = 3.6f;
static constexpr float T_LOCATION        = 4.9f;
static constexpr float T_HEARTBEAT       = 6.6f;
static constexpr float T_FLATLINE        = 8.2f;
static constexpr float T_CASE_TITLE      = 10.6f;
static constexpr float T_STATIC          = 11.8f;
static constexpr float T_TERMINAL_BOOT   = 14.0f;

// Boot sequence lines
static const char* BOOT_LINES[] = {
    "NOVACORP FORENSICS TERMINAL v4.7",
    "Initialising secure enclave...",
    "Loading case file: CASE-0447",
    "Subject: ORION, Marcus  DOB: 2005-01-14",
    "Status: DECEASED  ToD: 2047-03-16 02:14:00",
    "Decrypting NovaCorp internal database...",
    "Access granted -- Level 4 forensic clearance",
    "WARNING: This case involves active suspects.",
    "Proceed with caution. You are being logged.",
    "> FORENSICS TERMINAL READY",
    nullptr
};

// ─── Color helpers ────────────────────────────────────────────────────────────
static ImU32 rgba(int r,int g,int b,int a){ return IM_COL32(r,g,b,a); }
static ImU32 neon(int a=255)  { return rgba(0,212,255,a); }
static ImU32 red_c(int a=255) { return rgba(255,60,60,a); }
static ImU32 white(int a=255) { return rgba(220,235,255,a); }
static ImU32 dim(int a=255)   { return rgba(128,166,204,a); }

// ─── Pseudo-noise for static effect ──────────────────────────────────────────
static float pcg(uint32_t& s){
    s = s*747796405u + 2891336453u;
    uint32_t w = ((s>>((s>>28)+4))^s)*277803737u;
    w = (w>>22)^w;
    return (float)w / (float)0xFFFFFFFF;
}

// ─── Typewriter helper ────────────────────────────────────────────────────────
// Returns the visible prefix of 'text' based on timer and chars/sec
static std::string typewriter(const char* text, float t, float cps,
                               bool& just_typed,
                               int& prev_chars)
{
    int len = (int)strlen(text);
    int chars = (int)(t * cps);
    chars = chars < 0 ? 0 : chars > len ? len : chars;
    just_typed = (chars > prev_chars);
    prev_chars = chars;
    return std::string(text, chars);
}

// ─── Advance phase ────────────────────────────────────────────────────────────
static void advance(IntroState& s, IntroPhase next){
    s.phase   = next;
    s.phase_t = 0.f;
    s.typed_chars = 0;
}

// ─── Main draw function ───────────────────────────────────────────────────────
bool draw_intro(IntroState& s, float dt, float W, float H,
                std::function<void(float)> play_sfx_cb)
{
    (void)play_sfx_cb; // audio handled directly via Audio::get()
    if(s.complete || s.skipped) return true;

    s.total_t  += dt;
    s.phase_t  += dt;

    // Skip on any key / click
    if(ImGui::IsKeyPressed(ImGuiKey_Space)     ||
       ImGui::IsKeyPressed(ImGuiKey_Enter)     ||
       ImGui::IsKeyPressed(ImGuiKey_Escape)    ||
       ImGui::IsMouseClicked(0))
    {
        s.complete = true;
        Audio::get().play(SFX::TERMINAL_BOOT, 0.5f);
        return true;
    }

    // ── Full-screen ImGui overlay ─────────────────────────────────────────────
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize({W,H});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f,0.f,0.f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("##intro",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoNav|
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    float cx = W*0.5f, cy = H*0.5f;

    // ── BLACKOUT ──────────────────────────────────────────────────────────────
    if(s.phase == IntroPhase::BLACKOUT){
        dl->AddRectFilled({0,0},{W,H},rgba(0,0,0,255));
        if(s.phase_t > T_BLACKOUT){
            advance(s, IntroPhase::LOGO_FLICKER);
            Audio::get().play(SFX::GLITCH, 0.6f);
        }
    }

    // ── LOGO FLICKER ─────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::LOGO_FLICKER){
        dl->AddRectFilled({0,0},{W,H},rgba(0,0,0,255));

        // Glitch grid
        if((int)(s.phase_t*30)%3==0){
            for(int i=0;i<8;i++){
                float gx=pcg(s.rng)*W, gy=pcg(s.rng)*H;
                float gw=pcg(s.rng)*120+20, gh=pcg(s.rng)*8+2;
                dl->AddRectFilled({gx,gy},{gx+gw,gy+gh},
                    neon((int)(pcg(s.rng)*40)));
            }
        }

        // Flicker the NOVACORP logo
        float fi = s.phase_t / (T_LOGO_FLICKER - T_BLACKOUT);
        float flicker = 0.4f + 0.6f*fabsf(sinf(s.phase_t*22.f))
                             * fabsf(sinf(s.phase_t*7.3f));
        if(fi > 0.7f) flicker = 1.f; // stabilise near end

        int al = (int)(flicker*220);

        // "NOVACORP" — large, split with corruption
        const char* logo = "NOVACORP";
        float lw = ImGui::CalcTextSize(logo).x * 3.5f;
        float lx = cx - lw*0.5f;

        // Draw with slight per-char color glitch
        for(int ci=0;ci<(int)strlen(logo);ci++){
            char ch[2]={logo[ci],0};
            float cx2 = lx + ci*(lw/strlen(logo));
            float cx2_off = (pcg(s.rng)-.5f)*6.f*(1.f-fi);
            float cy2_off = (pcg(s.rng)-.5f)*4.f*(1.f-fi);
            ImGui::GetForegroundDrawList()->AddText(
                ImGui::GetFont(), 48.f*3.5f,
                {cx2+cx2_off, cy-40.f+cy2_off},
                fi>0.8f ? neon(al) : rgba(200,220,255,al),
                ch);
        }

        // Subtitle
        if(fi>0.5f){
            int sal=(int)((fi-0.5f)*2.f*120);
            dl->AddText(ImGui::GetFont(), 18.f,
                {cx-120.f, cy+80.f}, rgba(128,160,192,sal),
                "INTERNAL FORENSICS SYSTEM - RESTRICTED ACCESS");
        }

        // Scanlines
        for(float y=0;y<H;y+=3.f)
            dl->AddLine({0,y},{W,y},rgba(0,212,255,4));

        if(s.phase_t > T_LOGO_FLICKER - T_BLACKOUT){
            advance(s, IntroPhase::TIMESTAMP);
        }
    }

    // ── TIMESTAMP ────────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::TIMESTAMP){
        dl->AddRectFilled({0,0},{W,H},rgba(0,0,0,255));
        for(float y=0;y<H;y+=3.f)
            dl->AddLine({0,y},{W,y},rgba(0,212,255,4));

        // NovaCorp dim logo stays
        dl->AddText(ImGui::GetFont(),18.f,
            {cx-60.f, cy-200.f}, rgba(0,212,255,40),"NOVACORP");

        // Date/time typed out
        const char* ts = "2047-03-16   02:14:00";
        bool just_typed=false;
        std::string visible = typewriter(ts, s.phase_t, 18.f,
                                         just_typed, s.typed_chars);
        if(just_typed) Audio::get().play(SFX::KEYCLICK_SOFT, 0.35f);

        float tw = ImGui::CalcTextSize(visible.c_str()).x * 2.f;
        dl->AddText(ImGui::GetFont(), 36.f,
            {cx-tw*0.5f, cy-30.f}, neon(200), visible.c_str());

        // Blinking cursor
        if((int)(s.total_t*4)%2==0)
            dl->AddText(ImGui::GetFont(),36.f,
                {cx-tw*0.5f+tw,cy-30.f},neon(180),"_");

        if(s.phase_t > T_TIMESTAMP - T_LOGO_FLICKER){
            advance(s, IntroPhase::LOCATION);
        }
    }

    // ── LOCATION ──────────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::LOCATION){
        dl->AddRectFilled({0,0},{W,H},rgba(0,0,0,255));
        for(float y=0;y<H;y+=3.f)
            dl->AddLine({0,y},{W,y},rgba(0,212,255,4));

        // Timestamp faded
        dl->AddText(ImGui::GetFont(),36.f,
            {cx-180.f,cy-80.f},neon(80),"2047-03-16   02:14:00");

        // Location typed
        const char* loc = "SERVER ROOM B-7  //  NOVACORP HQ";
        bool just_typed=false;
        std::string vis = typewriter(loc, s.phase_t, 22.f,
                                      just_typed, s.typed_chars);
        if(just_typed) Audio::get().play(SFX::KEYCLICK_SOFT, 0.35f);

        float tw = ImGui::CalcTextSize(vis.c_str()).x * 1.4f;
        dl->AddText(ImGui::GetFont(),24.f,
            {cx-tw*0.5f,cy+20.f},rgba(180,220,255,200),vis.c_str());
        if((int)(s.total_t*4)%2==0)
            dl->AddText(ImGui::GetFont(),24.f,
                {cx-tw*0.5f+tw,cy+20.f},rgba(180,220,255,180),"_");

        if(s.phase_t > T_LOCATION - T_TIMESTAMP){
            advance(s, IntroPhase::HEARTBEAT);
            Audio::get().play(SFX::GLITCH, 0.4f);
        }
    }

    // ── HEARTBEAT ────────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::HEARTBEAT){
        dl->AddRectFilled({0,0},{W,H},rgba(2,4,10,255));
        for(float y=0;y<H;y+=3.f)
            dl->AddLine({0,y},{W,y},rgba(0,212,255,3));

        // Dim context text
        dl->AddText(ImGui::GetFont(),14.f,
            {cx-130.f,cy-180.f},neon(50),"BIOMETRIC MONITOR — SUBJECT A-001");
        dl->AddText(ImGui::GetFont(),36.f,
            {cx-180.f,cy-130.f},neon(30),"2047-03-16   02:14:00");

        // EKG line — flat with occasional beat while t < 1.5, then flatlining
        float dur = T_HEARTBEAT - T_LOCATION;
        float prog = s.phase_t / dur;

        // Draw EKG strip
        float ekgx = cx - 320.f;
        float ekgy = cy + 10.f;
        float ekgw = 640.f;

        // Heartbeat waveform points
        // While prog < 0.6 show beating, after that flatline
        float beat_prog = prog / 0.65f;
        if(beat_prog > 1.f) beat_prog = 1.f;

        std::vector<ImVec2> pts;
        int N = 200;
        for(int i=0;i<N;i++){
            float fx = (float)i/N;
            float x  = ekgx + fx*ekgw;
            float y  = ekgy;

            // Three heartbeat pulses
            for(int beat=0;beat<3;beat++){
                float bc = 0.15f + beat*0.30f;
                if(bc > beat_prog) break;
                float d = fx - bc;
                if(d >= 0.f && d < 0.04f){
                    float bt = d/0.04f;
                    // QRS complex shape
                    if(bt < 0.15f) y -= bt*800.f;
                    else if(bt < 0.30f) y += (bt-0.15f)*2400.f - 120.f;
                    else if(bt < 0.50f) y -= (bt-0.30f)*800.f + 240.f;
                    else y += (bt-0.50f)*200.f;
                }
            }
            pts.push_back({x,y});
        }

        // Flatline after beat_prog
        if(beat_prog >= 1.f){
            float fprog = (prog - 0.65f)/0.35f;
            int al = (int)std::min(255.f, fprog*600.f);
            // Red flat line
            for(int i=0;i<N;i++){
                float x = ekgx + (float)i/N * ekgw;
                if(pts[i].y != ekgy)
                    pts[i].y = ekgy + (pts[i].y-ekgy)*(1.f-fprog);
            }
            dl->AddLine({ekgx,ekgy},{ekgx+ekgw,ekgy},red_c(al),2.f);
        }

        // Draw EKG line
        for(int i=1;i<(int)pts.size();i++){
            ImU32 col = (pts[i].y < ekgy-10.f || pts[i].y > ekgy+10.f)
                ? rgba(255,80,80,220) : neon(180);
            dl->AddLine(pts[i-1],pts[i],col,1.8f);
        }

        // BPM counter dropping to 0
        float bpm = 72.f * (1.f - prog*prog);
        char bpm_str[32];
        snprintf(bpm_str,sizeof(bpm_str),"%.0f BPM", bpm);
        int bpm_col = bpm < 5.f ? red_c(200) : neon(160);
        dl->AddText(ImGui::GetFont(),28.f,
            {ekgx+ekgw+16.f, ekgy-14.f}, bpm_col, bpm_str);

        s.heartbeat_phase += dt;

        if(s.phase_t > dur){
            advance(s, IntroPhase::FLATLINE);
            Audio::get().play(SFX::ERROR_BEEP, 0.8f);
        }
    }

    // ── FLATLINE ──────────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::FLATLINE){
        float dur = T_FLATLINE - T_HEARTBEAT;
        float prog = s.phase_t / dur;

        dl->AddRectFilled({0,0},{W,H},rgba(2,3,8,255));
        for(float y=0;y<H;y+=3.f)
            dl->AddLine({0,y},{W,y},rgba(0,212,255,3));

        // Flatline
        int ral = (int)std::min(255.f, prog*4.f*255.f);
        dl->AddLine({cx-320.f,cy+10.f},{cx+320.f,cy+10.f},red_c(ral),2.5f);

        // "SUBJECT DECEASED" text — slams in
        if(prog > 0.25f){
            float a2 = (prog-0.25f)/0.25f;
            a2 = a2>1.f?1.f:a2;
            int ta = (int)(a2*220);
            float scale = 1.f + (1.f-a2)*0.8f;

            // Shadow/glow
            for(int off=1;off<=3;off++)
                dl->AddText(ImGui::GetFont(),24.f*scale,
                    {cx-148.f,cy-60.f-(float)off}, red_c(ta/4),
                    "SUBJECT DECEASED");
            dl->AddText(ImGui::GetFont(),24.f*scale,
                {cx-148.f,cy-60.f}, red_c(ta),
                "SUBJECT DECEASED");
        }

        // 0 BPM
        dl->AddText(ImGui::GetFont(),28.f,
            {cx+328.f,cy-4.f},red_c(200),"0 BPM");

        if(s.phase_t > dur){
            advance(s, IntroPhase::CASE_TITLE);
            Audio::get().play(SFX::ACCUSE, 0.9f);
        }
    }

    // ── CASE TITLE ────────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::CASE_TITLE){
        float dur = T_CASE_TITLE - T_FLATLINE;
        float prog = s.phase_t / dur;

        dl->AddRectFilled({0,0},{W,H},rgba(0,0,0,255));
        for(float y=0;y<H;y+=4.f)
            dl->AddLine({0,y},{W,y},rgba(0,212,255,5));

        // CASE NUMBER
        if(prog > 0.1f){
            float a=(prog-0.1f)/0.15f; a=a>1.f?1.f:a;
            dl->AddText(ImGui::GetFont(),15.f,
                {cx-80.f,cy-130.f},neon((int)(a*130)),
                "CASE FILE  #0447");
        }

        // Main title — "THE ORION MURDER" — slams in with scale
        if(prog > 0.25f){
            float a=(prog-0.25f)/0.20f; a=a>1.f?1.f:a;
            float boom = 1.f + (1.f-a)*1.2f; // scale from large
            float sz = 42.f;
            const char* title = "THE  ORION  MURDER";
            float tw = ImGui::CalcTextSize(title).x * sz/14.f;

            // Glow layers
            for(int g=3;g>=0;g--)
                dl->AddText(ImGui::GetFont(), sz+g*2.f,
                    {cx-tw*0.5f-(float)g, cy-30.f-(float)g},
                    g==0 ? white((int)(a*240)) : neon((int)(a*30/g)),
                    title);
        }

        // Author/file info
        if(prog > 0.6f){
            float a=(prog-0.6f)/0.25f; a=a>1.f?1.f:a;
            dl->AddText(ImGui::GetFont(),13.f,
                {cx-160.f,cy+55.f}, dim((int)(a*140)),
                "INVESTIGATING OFFICER: [CLASSIFIED]");
            dl->AddText(ImGui::GetFont(),13.f,
                {cx-160.f,cy+75.f}, dim((int)(a*140)),
                "CLEARANCE LEVEL: FORENSIC-4");
            dl->AddText(ImGui::GetFont(),13.f,
                {cx-160.f,cy+95.f}, dim((int)(a*140)),
                "DATABASE: NOVACORP INTERNAL  //  ENCRYPTED");
        }

        // SKIP hint
        if(prog > 0.8f){
            float a=(prog-0.8f)/0.2f;
            dl->AddText(ImGui::GetFont(),12.f,
                {cx-90.f,H-40.f},dim((int)(a*80)),
                "PRESS SPACE TO SKIP INTRO");
        }

        if(s.phase_t > dur){
            advance(s, IntroPhase::STATIC);
            Audio::get().play(SFX::GLITCH, 1.0f);
        }
    }

    // ── STATIC ────────────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::STATIC){
        float dur = T_STATIC - T_CASE_TITLE;
        float prog = s.phase_t / dur;

        // Full screen TV static
        s.rng += 7919;

        // Draw noise in a grid
        int nx=80, ny=50;
        float bw=W/nx, bh=H/ny;
        for(int y=0;y<ny;y++){
            for(int x=0;x<nx;x++){
                float n=pcg(s.rng);
                int v=(int)(n*255);
                dl->AddRectFilled(
                    {x*bw,y*bh},{(x+1)*bw,(y+1)*bh},
                    rgba(v,v,(int)(v*0.85f+40),240));
            }
        }

        // Fade to black at end
        if(prog > 0.7f){
            float a=(prog-0.7f)/0.3f;
            dl->AddRectFilled({0,0},{W,H},rgba(0,0,0,(int)(a*255)));
        }

        if(s.phase_t > dur){
            advance(s, IntroPhase::TERMINAL_BOOT);
            Audio::get().play(SFX::TERMINAL_BOOT, 0.8f);
        }
    }

    // ── TERMINAL BOOT ─────────────────────────────────────────────────────────
    else if(s.phase == IntroPhase::TERMINAL_BOOT){
        float dur = T_TERMINAL_BOOT - T_STATIC;
        float prog = s.phase_t / dur;

        dl->AddRectFilled({0,0},{W,H},rgba(0,0,0,255));

        // CRT scanlines
        for(float y=0;y<H;y+=2.f)
            dl->AddLine({0,y},{W,y},rgba(0,12,24,180));

        // Phosphor glow border
        dl->AddRect({8.f,8.f},{W-8.f,H-8.f},neon(25),0.f,0,2.f);
        dl->AddRect({12.f,12.f},{W-12.f,H-12.f},neon(12),0.f,0,1.f);

        // Count which lines to show
        float line_interval = dur / 11.f;
        int max_line = (int)(s.phase_t / line_interval);
        if(max_line > 10) max_line = 10;

        float start_y = cy - 120.f;

        for(int li=0; li<=max_line && BOOT_LINES[li]; li++){
            float line_age = s.phase_t - li * line_interval;

            // Typewriter within this line
            int len = (int)strlen(BOOT_LINES[li]);
            int chars = (int)(line_age * 40.f);
            chars = chars<0?0:chars>len?len:chars;

            // Color coding
            ImU32 col;
            if(li == 9)       col = neon(220);
            else if(li >= 6)  col = rgba(255,200,80,(int)(std::min(1.f,line_age*3.f)*180));
            else              col = rgba(140,200,140,(int)(std::min(1.f,line_age*4.f)*160));

            std::string txt(BOOT_LINES[li], chars);
            dl->AddText(ImGui::GetFont(),14.f,
                {cx-300.f, start_y + li*22.f}, col, txt.c_str());

            // Cursor on active line
            if(li==max_line && chars<len && (int)(s.total_t*6)%2==0){
                float tw=ImGui::CalcTextSize(txt.c_str()).x;
                dl->AddText(ImGui::GetFont(),14.f,
                    {cx-300.f+tw, start_y+li*22.f},
                    li==9?neon(200):rgba(140,200,140,200),"_");
            }

            // Play key click when a new char appears on current line
            if(li==max_line && chars>0 && chars<len &&
               (int)(line_age*40)!=(int)((line_age-0.025f)*40))
                Audio::get().play(SFX::KEYCLICK_SOFT,0.25f);
        }

        // PRESS ANY KEY prompt
        if(prog > 0.85f){
            float a=(prog-0.85f)/0.15f;
            if((int)(s.total_t*2)%2==0)
                dl->AddText(ImGui::GetFont(),13.f,
                    {cx-110.f, start_y+11*22.f+20.f},
                    neon((int)(a*160)),"PRESS ANY KEY TO BEGIN INVESTIGATION");
        }

        if(s.phase_t > dur){
            s.complete = true;
        }
    }

    // ── Skip hint (always) ────────────────────────────────────────────────────
    if(s.phase != IntroPhase::TERMINAL_BOOT &&
       s.phase != IntroPhase::BLACKOUT &&
       s.total_t > 1.5f)
    {
        dl->AddText(ImGui::GetFont(),11.f,
            {W-180.f,H-24.f},rgba(80,100,130,90),"[SPACE/CLICK TO SKIP]");
    }

    ImGui::End();
    return s.complete;
}
