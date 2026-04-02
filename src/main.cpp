// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR  —  A Cyber-Detective SQL Game
//  Built with: SDL2 + Dear ImGui + SQLite3
// ═══════════════════════════════════════════════════════════════════════════

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include <SDL.h>

#include "Types.h"
#include "GameState.h"
#include "UITheme.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

// ─── Runtime window dimensions (updated every frame from SDL) ──────────────
static float g_W = 1440.f;
static float g_H = 900.f;

// ─── Layout constants ──────────────────────────────────────────────────────
static constexpr float TOP_H     = 48.f;
static constexpr float LEFT_W_F  = 0.195f;  // fraction of total width
static constexpr float RIGHT_W   = 300.f;   // fixed px
static constexpr float RESULTS_H = 260.f;   // fixed px for results pane

// ─── Global game state ────────────────────────────────────────────────────
static GameState g_state;

// ─── Query UI state ───────────────────────────────────────────────────────
static char        s_qbuf[4096]  = "SELECT * FROM access_logs ORDER BY timestamp;";
static int         s_hist_idx    = -1;
static bool        s_focus_q     = true;
static QueryResult s_result;
static bool        s_has_result  = false;

// ─── Color helpers ────────────────────────────────────────────────────────
static ImVec4 C_NEON  (float a=1.f){ return {0.000f,0.831f,1.000f,a}; }
static ImVec4 C_PURPLE(float a=1.f){ return {0.478f,0.361f,1.000f,a}; }
static ImVec4 C_RED   (float a=1.f){ return {1.000f,0.302f,0.302f,a}; }
static ImVec4 C_GREEN (float a=1.f){ return {0.196f,0.902f,0.588f,a}; }
static ImVec4 C_DIM   (float a=1.f){ return {0.502f,0.651f,0.800f,a}; }
static ImVec4 C_TEXT  (float a=1.f){ return {0.867f,0.933f,1.000f,a}; }

static ImU32 U32(ImVec4 c){ return ImGui::ColorConvertFloat4ToU32(c); }

// ─── Utility helpers ──────────────────────────────────────────────────────

static std::string fmt_time(float s){
    int h=(22+(int)(s/3600))%24, m=(int)(s/60)%60, ss=(int)s%60;
    char b[12]; snprintf(b,sizeof(b),"%02d:%02d:%02d",h,m,ss); return b;
}

static void centered_text(const char* t, ImVec4 col){
    float tw=ImGui::CalcTextSize(t).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(ImGui::GetContentRegionAvail().x-tw)*0.5f);
    ImGui::TextColored(col,"%s",t);
}

static void neon_sep(float a=0.35f){
    ImVec2 p=ImGui::GetCursorScreenPos();
    float w=ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(p,{p.x+w,p.y},U32(C_NEON(a)),1.f);
    ImGui::Dummy({0,2});
}

static void bg_dots(float a=0.022f){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
    ImU32 col=IM_COL32(0,212,255,(int)(255*a));
    for(float x=wp.x+16;x<wp.x+ws.x;x+=32)
        for(float y=wp.y+16;y<wp.y+ws.y;y+=32)
            dl->AddCircleFilled({x,y},1.1f,col,4);
}

static void section_label(const char* lbl, ImVec4 col){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    dl->AddRectFilled({p.x,p.y+2},{p.x+3,p.y+14},U32(col));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+9);
    ImGui::TextColored(col,"%s",lbl);
}

// ─────────────────────────────────────────────────────────────────────────────
//  TOP BAR
// ─────────────────────────────────────────────────────────────────────────────
static void draw_top_bar(){
    float p=UITheme::pulse(1.4f,0.6f,1.f);
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize({g_W,TOP_H});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16.f,0.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  {18.f,0.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.014f,0.024f,0.052f,1.f));

    ImGui::Begin("##top",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_NoScrollWithMouse);

    // Bottom glow bar
    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos();
        dl->AddRectFilled({wp.x,wp.y+TOP_H-2},{wp.x+g_W,wp.y+TOP_H},
            IM_COL32(0,212,255,(int)(55*p)));
        dl->AddLine({wp.x,wp.y+TOP_H-1},{wp.x+100,wp.y+TOP_H-1},
            IM_COL32(0,212,255,220),1.5f);
        dl->AddLine({wp.x+g_W-100,wp.y+TOP_H-1},{wp.x+g_W,wp.y+TOP_H-1},
            IM_COL32(0,212,255,220),1.5f);
    }

    // Vertical center
    float ty=(TOP_H-ImGui::GetTextLineHeight())*0.5f;
    ImGui::SetCursorPosY(ty);

    ImGui::TextColored(C_NEON(p),"◈"); ImGui::SameLine();
    ImGui::TextColored(C_TEXT(0.9f),"QUERY NOIR"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.25f),"│"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.55f),"CASE:"); ImGui::SameLine(0,5);
    ImGui::TextColored(C_NEON(0.85f),"ORION MURDER"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.25f),"│"); ImGui::SameLine();

    auto& app=g_state.app();
    if(app.status==GameStatus::CASE_SOLVED)
        ImGui::TextColored(C_GREEN(),"✓ SOLVED");
    else
        ImGui::TextColored(C_GREEN(p),"● ACTIVE");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.25f),"│"); ImGui::SameLine();

    int tot=(int)g_state.clues().size();
    ImGui::TextColored(C_DIM(0.55f),"CLUES:"); ImGui::SameLine(0,5);
    ImGui::TextColored(app.discovered_clues>0?C_GREEN():C_DIM(0.4f),
        "%d/%d",app.discovered_clues,tot);
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.25f),"│"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.55f),"TIME:"); ImGui::SameLine(0,5);
    ImGui::TextColored(C_NEON(0.7f+0.3f*p),"%s",fmt_time(app.game_time).c_str());

    // Right-align HELP
    ImGui::SetCursorPosY(ty); // reset Y for same-line trick
    ImGui::SameLine(g_W-100.f);
    UITheme::push_neon_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12.f,5.f});
    if(ImGui::Button("? HELP")) app.show_help=!app.show_help;
    ImGui::PopStyleVar();
    UITheme::pop_neon_button();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  LEFT PANEL — Evidence Navigator
// ─────────────────────────────────────────────────────────────────────────────
static void draw_left_panel(){
    float lw=floorf(g_W*LEFT_W_F);
    ImGui::SetNextWindowPos({0,TOP_H});
    ImGui::SetNextWindowSize({lw,g_H-TOP_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.020f,0.036f,0.072f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{13.f,12.f});

    ImGui::Begin("##left",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
        dl->AddLine({wp.x+ws.x-1,wp.y},{wp.x+ws.x-1,wp.y+ws.y},
            IM_COL32(0,212,255,70));
        bg_dots(0.020f);
    }

    ImGui::Spacing();
    section_label("DATABASES",C_DIM(0.65f));
    ImGui::Spacing();

    for(auto& t : g_state.tables()){
        float tp=UITheme::pulse(2.0f+(float)(&t-&g_state.tables()[0])*0.8f,
                                0.5f,1.f);
        if(!t.unlocked){
            ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.22f));
            ImGui::Text("  %s  %s  [LOCK]",t.icon.c_str(),t.display_name.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0,0.831f,1.f,0.09f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(0,0.831f,1.f,0.17f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0,0.831f,1.f,0.27f));
            ImGui::PushStyleColor(ImGuiCol_Text,C_NEON(tp));
            bool click=ImGui::Selectable(
                ("  "+t.icon+"  "+t.display_name+"##"+t.name).c_str(),
                false,ImGuiSelectableFlags_None,{-1,22});
            ImGui::PopStyleColor(4);
            if(click){
                snprintf(s_qbuf,sizeof(s_qbuf),"SELECT * FROM %s;",t.name.c_str());
                s_focus_q=true;
            }
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,
                    ImVec4(0.022f,0.040f,0.082f,0.98f));
                ImGui::BeginTooltip();
                ImGui::TextColored(C_NEON(),"TABLE: %s",t.display_name.c_str());
                ImGui::Separator();
                ImGui::Spacing();
                for(auto& c:t.columns){
                    ImGui::TextColored(C_DIM(0.45f),"  ·  ");
                    ImGui::SameLine(0,0);
                    ImGui::TextColored(C_TEXT(0.8f),"%s",c.c_str());
                }
                ImGui::Spacing();
                ImGui::TextColored(C_DIM(0.35f),"[click to query]");
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        }
        ImGui::Spacing();
    }

    // ── CLUE BOARD ────────────────────────────────────────────────────────
    ImGui::Spacing();
    neon_sep(0.20f);
    ImGui::Spacing();
    section_label("CLUE BOARD",C_DIM(0.65f));
    ImGui::Spacing();

    for(auto& clue:g_state.clues()){
        if(clue.discovered){
            ImVec2 p2=ImGui::GetCursorScreenPos();
            float rw=ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p2.x-2,p2.y},{p2.x+rw+2,p2.y+20},
                IM_COL32(50,230,150,15),3.f);
            ImGui::PushStyleColor(ImGuiCol_Text,C_GREEN(0.88f));
            ImGui::TextWrapped("  ✓  %s",clue.title.c_str());
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,
                    ImVec4(0.015f,0.028f,0.060f,0.98f));
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(280.f);
                ImGui::TextColored(C_GREEN(),"%s",clue.title.c_str());
                ImGui::Spacing();
                ImGui::TextColored(C_TEXT(0.75f),"%s",clue.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.18f));
            ImGui::Text("  ○  [UNDISCOVERED]");
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    // ── SOLVE BUTTON ──────────────────────────────────────────────────────
    if(g_state.is_solved()){
        float p2=UITheme::pulse(3.f,0.5f,1.f);
        ImGui::Spacing(); neon_sep(0.18f); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.196f,0.902f,0.588f,0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.196f,0.902f,0.588f,0.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.196f,0.902f,0.588f,0.40f));
        ImGui::PushStyleColor(ImGuiCol_Text,C_GREEN(p2));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0.f,10.f});
        if(ImGui::Button("▶  SUBMIT REPORT",{-1,-1}))
            g_state.app().status=GameStatus::CASE_SOLVED;
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  CENTER — Query Console + Results
// ─────────────────────────────────────────────────────────────────────────────
static void draw_center(){
    float lw  = floorf(g_W*LEFT_W_F);
    float cx  = lw;
    float cw  = g_W - lw - RIGHT_W;
    float ch  = g_H - TOP_H - RESULTS_H;

    // ── Console input area ───────────────────────────────────────────────
    ImGui::SetNextWindowPos({cx,TOP_H});
    ImGui::SetNextWindowSize({cw,ch});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.016f,0.028f,0.058f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18.f,14.f});
    ImGui::Begin("##con",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
        // Subtle dot grid
        bg_dots(0.014f);
        // Top accent
        float p2=UITheme::pulse(1.9f,0.4f,1.f);
        dl->AddRectFilled({wp.x,wp.y},{wp.x+ws.x,wp.y+2.f},
            IM_COL32(0,212,255,(int)(45*p2)));
    }

    // Header
    ImGui::Spacing();
    float p=UITheme::pulse(1.7f,0.55f,1.f);
    ImGui::TextColored(C_NEON(p),"// QUERY CONSOLE");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.30f),"— SQLite 3  ·  UTF-8  ·  In-Memory DB");
    ImGui::Spacing();
    neon_sep(0.18f);
    ImGui::Spacing();

    // Prompt prefix
    ImGui::TextColored(C_NEON(0.85f),"noir@forensics");
    ImGui::SameLine(0,0);
    ImGui::TextColored(C_DIM(0.45f),":");
    ImGui::SameLine(0,0);
    ImGui::TextColored(C_PURPLE(0.75f),"~/orion");
    ImGui::SameLine(0,0);
    ImGui::TextColored(C_DIM(0.6f),"$ ");
    ImGui::SameLine(0,6);

    // Input + button
    float btn_w=108.f;
    float input_w=ImGui::GetContentRegionAvail().x-btn_w-10.f;
    ImGui::SetNextItemWidth(input_w);

    bool exec_flag=false;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.006f,0.012f,0.030f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f,0.831f,1.f,0.05f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.f,0.831f,1.f,0.09f));
    ImGui::PushStyleColor(ImGuiCol_Border,
        g_state.app().query_executing ? C_NEON(0.9f) : C_NEON(0.32f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10.f,7.f});

    if(s_focus_q){ ImGui::SetKeyboardFocusHere(); s_focus_q=false; }

    auto hcb=[](ImGuiInputTextCallbackData* d)->int{
        auto& H=g_state.history();
        if(H.empty()) return 0;
        if(d->EventKey==ImGuiKey_UpArrow)
            s_hist_idx=std::min(s_hist_idx+1,(int)H.size()-1);
        else if(d->EventKey==ImGuiKey_DownArrow)
            s_hist_idx=std::max(s_hist_idx-1,-1);
        if(s_hist_idx>=0){
            int i=(int)H.size()-1-s_hist_idx;
            d->DeleteChars(0,d->BufTextLen);
            d->InsertChars(0,H[i].c_str());
        } else {
            d->DeleteChars(0,d->BufTextLen);
        }
        return 0;
    };

    if(ImGui::InputText("##qi",s_qbuf,sizeof(s_qbuf),
        ImGuiInputTextFlags_EnterReturnsTrue|
        ImGuiInputTextFlags_CallbackHistory, hcb))
        exec_flag=true;

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    ImGui::SameLine(0,10);

    // Execute button
    {
        float ep=UITheme::pulse(2.2f,0.65f,1.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f,0.831f,1.f,0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f,0.831f,1.f,0.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.f,0.831f,1.f,0.40f));
        ImGui::PushStyleColor(ImGuiCol_Text,C_NEON(ep));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0.f,7.f});
        if(ImGui::Button("EXECUTE ▶",{btn_w,-1})) exec_flag=true;
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    if(exec_flag && strlen(s_qbuf)>0){
        g_state.app().glitch_timer=0.20f;
        s_result=g_state.run_query(s_qbuf);
        s_has_result=true;
        s_hist_idx=-1;
    }

    // Executing indicator
    if(g_state.app().query_executing){
        ImGui::Spacing();
        float ep=UITheme::pulse(9.f,0.3f,1.f);
        ImGui::TextColored(C_NEON(ep),"  ◌  PROCESSING...");
    }

    // ── Quick-query chips ─────────────────────────────────────────────────
    ImGui::Spacing();
    neon_sep(0.10f);
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.38f),"Quick:");

    struct QC{ const char* lbl; const char* sql; };
    static QC chips[]={
        {"suspects",
         "SELECT name, alibi, alibi_verified, badge_id, has_master_access FROM suspects;"},
        {"access log (night)",
         "SELECT timestamp, badge_id, person_name, location, action, flag\n"
         "FROM access_logs\n"
         "WHERE timestamp >= '2047-03-15 19:00:00'\n"
         "ORDER BY timestamp;"},
        {"victim",
         "SELECT * FROM victims;"},
        {"join logs+suspects",
         "SELECT a.timestamp, a.person_name, a.location, a.action, a.flag,\n"
         "       s.alibi, s.alibi_verified\n"
         "FROM access_logs a\n"
         "LEFT JOIN suspects s ON a.person_name = s.name\n"
         "WHERE a.timestamp >= '2047-03-15 19:00:00'\n"
         "ORDER BY a.timestamp;"},
        {"employees",
         "SELECT name, department, job_title, badge_id FROM employees ORDER BY department;"},
    };

    ImGui::SameLine(0,8);
    for(auto& ch:chips){
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f,0.45f,0.7f,0.09f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f,0.831f,1.f,0.16f));
        ImGui::PushStyleColor(ImGuiCol_Text,          C_DIM(0.72f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{7.f,3.f});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0.f);
        if(ImGui::SmallButton(ch.lbl)){
            strncpy(s_qbuf,ch.sql,sizeof(s_qbuf)-1);
            s_focus_q=true;
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::SameLine(0,5);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ── Results pane ──────────────────────────────────────────────────────
    float ry=TOP_H+ch;
    ImGui::SetNextWindowPos({cx,ry});
    ImGui::SetNextWindowSize({cw,RESULTS_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.010f,0.018f,0.040f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18.f,10.f});
    ImGui::Begin("##res",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_HorizontalScrollbar);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos();
        dl->AddRectFilled({wp.x,wp.y},{wp.x+cw,wp.y+2},IM_COL32(0,212,255,40));
        bg_dots(0.010f);
    }

    // Results header row
    ImGui::TextColored(C_DIM(0.45f),"RESULTS");
    if(s_has_result && !s_result.is_error){
        ImGui::SameLine(0,10);
        ImGui::TextColored(C_DIM(0.28f),"—");
        ImGui::SameLine(0,8);
        ImGui::TextColored(C_GREEN(0.65f),"%zu row%s",
            s_result.rows.size(), s_result.rows.size()==1?"":"s");
        if(!s_result.flagged_rows.empty()){
            ImGui::SameLine(0,10);
            ImGui::TextColored(C_RED(0.65f),"⚠ %zu flagged",
                s_result.flagged_rows.size());
        }
    }
    ImGui::Spacing();
    neon_sep(0.12f);
    ImGui::Spacing();

    float row_h=RESULTS_H-58.f;

    if(!s_has_result){
        float sy=ImGui::GetCursorPosY()+row_h*0.28f;
        ImGui::SetCursorPosY(sy);
        centered_text("Awaiting query...", C_DIM(0.22f));

    } else if(s_result.is_error){
        std::string em="[!]  "+s_result.error;
        ImGui::PushStyleColor(ImGuiCol_Text,C_RED(0.85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0.25f,0.04f,0.04f,0.25f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10.f,7.f});
        ImGui::InputTextMultiline("##em",em.data(),em.size(),
            {-1,row_h},ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

    } else if(s_result.rows.empty()){
        float sy=ImGui::GetCursorPosY()+row_h*0.28f;
        ImGui::SetCursorPosY(sy);
        centered_text("Query returned 0 rows.", C_DIM(0.30f));

    } else {
        int ncols=(int)s_result.columns.size();
        if(ncols>0 && ImGui::BeginTable("##rt",ncols,
            ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|
            ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY|
            ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoHostExtendX,
            {-1,row_h}))
        {
            for(auto& c:s_result.columns)
                ImGui::TableSetupColumn(c.c_str(),
                    ImGuiTableColumnFlags_WidthFixed,138.f);
            ImGui::TableSetupScrollFreeze(0,1);

            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,ImVec4(0.f,0.22f,0.35f,1.f));
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            auto& flg=s_result.flagged_rows;
            for(size_t ri=0;ri<s_result.rows.size();ri++){
                bool flag=std::find(flg.begin(),flg.end(),ri)!=flg.end();
                ImGui::TableNextRow();
                if(flag){
                    ImVec2 rm=ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        rm,{rm.x+9999.f,rm.y+22.f},IM_COL32(255,60,60,18));
                }
                for(size_t ci=0;ci<s_result.rows[ri].size();ci++){
                    ImGui::TableSetColumnIndex((int)ci);
                    ImGui::TextColored(
                        flag?C_RED(0.85f):C_TEXT(0.80f),
                        "%s",s_result.rows[ri][ci].c_str());
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RIGHT PANEL — Detective Feed
// ─────────────────────────────────────────────────────────────────────────────
static void draw_right_panel(){
    float rx=g_W-RIGHT_W;
    float rh=g_H-TOP_H;

    ImGui::SetNextWindowPos({rx,TOP_H});
    ImGui::SetNextWindowSize({RIGHT_W,rh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.018f,0.032f,0.066f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14.f,12.f});
    ImGui::Begin("##right",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
        dl->AddLine({wp.x+1,wp.y},{wp.x+1,wp.y+ws.y},IM_COL32(0,212,255,75));
        bg_dots(0.018f);
    }

    ImGui::Spacing();
    float p=UITheme::pulse(1.4f,0.55f,1.f);
    ImGui::TextColored(C_NEON(p),"◈");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_TEXT(0.78f),"DETECTIVE FEED");
    ImGui::Spacing();
    neon_sep(0.18f);
    ImGui::Spacing();

    float feed_h=rh-68.f;
    ImGui::BeginChild("##fc",{-1,feed_h},false,ImGuiWindowFlags_NoScrollbar);

    for(auto& entry:g_state.feed()){
        ImVec4 col; const char* pfx;
        switch(entry.type){
            case NarrativeType::MONOLOGUE: col=C_TEXT(0.78f);  pfx=">  "; break;
            case NarrativeType::HINT:      col=C_NEON(0.82f);  pfx="?  "; break;
            case NarrativeType::ALERT:     col=C_RED(0.88f);   pfx="!  "; break;
            case NarrativeType::SUCCESS:   col=C_GREEN(0.92f); pfx="✓  "; break;
            case NarrativeType::SYSTEM:    col=C_PURPLE(0.72f);pfx="#  "; break;
            default:                       col=C_DIM(0.55f);   pfx="   "; break;
        }

        // Accent dot
        {
            ImVec2 cp=ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                {cp.x+3,cp.y+7},2.5f,U32(col),8);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+11);

        // Typewriter
        int chars=entry.revealed
            ? (int)entry.text.size()
            : std::min((int)entry.reveal_timer,(int)entry.text.size());
        std::string disp=entry.text.substr(0,chars);

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+RIGHT_W-26.f);
        ImGui::TextColored(col,"%s%s",pfx,disp.c_str());
        ImGui::PopTextWrapPos();

        if(!entry.revealed){
            ImGui::SameLine(0,0);
            if(UITheme::pulse(6.f,0.f,1.f)>0.5f)
                ImGui::TextColored(col,"▌");
        }
        ImGui::Spacing();
    }

    ImGui::EndChild();

    neon_sep(0.10f);
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.28f),"↑↓ history  ·  Enter to run");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  NOTIFICATIONS
// ─────────────────────────────────────────────────────────────────────────────
static void draw_notifications(){
    float yoff=g_H-20.f;
    for(auto& n:g_state.notifs()){
        float a=1.f-(n.age/n.lifetime);
        if(a<=0.f) continue;
        ImVec4 col; const char* icon;
        switch(n.type){
            case NotifType::CLUE:      col=C_GREEN(a);  icon="✓"; break;
            case NotifType::UNLOCK:    col=C_PURPLE(a); icon="◈"; break;
            case NotifType::ERROR_MSG: col=C_RED(a);    icon="!"; break;
            default:                   col=C_NEON(a);   icon="·"; break;
        }
        std::string msg=std::string(icon)+" "+n.message;
        ImVec2 sz=ImGui::CalcTextSize(msg.c_str());
        float pw=sz.x+28.f, ph=34.f;
        float px=(g_W-pw)*0.5f, py=yoff-ph;

        ImGui::SetNextWindowPos({px,py});
        ImGui::SetNextWindowSize({pw,ph});
        ImGui::SetNextWindowBgAlpha(0.92f*a);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.012f,0.022f,0.050f,0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border,col);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14.f,8.f});
        char wid[32]; snprintf(wid,sizeof(wid),"##nt%p",(void*)&n);
        ImGui::Begin(wid,nullptr,
            ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
            ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoNav|
            ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(col,"%s",msg.c_str());
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        yoff-=ph+5.f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  CASE SOLVED OVERLAY
// ─────────────────────────────────────────────────────────────────────────────
static void draw_solved(){
    if(g_state.app().status!=GameStatus::CASE_SOLVED) return;
    float p=UITheme::pulse(1.3f,0.6f,1.f);
    float mw=600.f, mh=440.f;
    ImGui::SetNextWindowPos({(g_W-mw)*0.5f,(g_H-mh)*0.5f});
    ImGui::SetNextWindowSize({mw,mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.016f,0.030f,0.062f,0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,C_GREEN(p));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{28.f,22.f});
    ImGui::Begin("##sol",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize);

    ImGui::Spacing();
    centered_text("CASE CLOSED",C_GREEN(p));
    ImGui::Spacing();
    centered_text("THE ORION MURDER — RESOLVED",C_TEXT(0.85f));
    ImGui::Spacing();
    neon_sep(0.15f);
    ImGui::Spacing();

    ImGui::PushTextWrapPos(mw-56.f);
    ImGui::TextColored(C_DIM(0.55f),"PERPETRATOR:");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_RED(0.92f),"Lena Park  —  Senior Data Analyst");
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.45f),"ACCESSORY:");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_RED(0.75f),"Hana Mori (ordered the act) + Elena Vasquez (complicit)");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(C_TEXT(0.70f),
        "Marcus Orion discovered $1.35M in fraudulent vendor payments routed "
        "from NovaCorp through ExtShell-LLC to Rex Calloway — all approved by "
        "CFO Hana Mori. The money looped back offshore to Mori herself.\n\n"
        "Mori paid Lena Park $12,000 to silence Marcus. Park, a former partner "
        "who still had access to his schedule, arranged the meeting in Server Room B-7 "
        "at 22:30 — then modified badge MASTER to give herself override access.\n\n"
        "At 22:15 she messaged an external contact: 'He has the files. "
        "I cannot let that happen.'\n\n"
        "Elena Vasquez received Mori's encrypted order at 20:05: "
        "'The MASTER key is yours.' She arrived at 23:58 — but Park had "
        "already acted. The MASTER override logged at 02:14 was Park's final move.\n\n"
        "Carl Bremer flagged the badge tampering in January. His report was buried.");
    ImGui::PopTextWrapPos();

    ImGui::Spacing(); ImGui::Spacing();
    centered_text("Justice served.", C_GREEN(p));
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.196f,0.902f,0.588f,0.09f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.196f,0.902f,0.588f,0.22f));
    ImGui::PushStyleColor(ImGuiCol_Text,C_GREEN(p));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0.f,10.f});
    if(ImGui::Button("CLOSE FILE",{-1,0}))
        g_state.app().status=GameStatus::ACTIVE;
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  HELP OVERLAY
// ─────────────────────────────────────────────────────────────────────────────
static void draw_help(){
    if(!g_state.app().show_help) return;
    float mw=550.f, mh=470.f;
    ImGui::SetNextWindowPos({(g_W-mw)*0.5f,(g_H-mh)*0.5f});
    ImGui::SetNextWindowSize({mw,mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.016f,0.030f,0.062f,0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,C_NEON(0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{22.f,18.f});
    ImGui::Begin("##hlp",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize);

    ImGui::TextColored(C_NEON(),"HOW TO PLAY");
    ImGui::Spacing();
    neon_sep(0.25f);
    ImGui::Spacing();

    ImGui::PushTextWrapPos(mw-44.f);
    ImGui::TextColored(C_TEXT(0.75f),
        "Write SQL queries to investigate the murder. Explore tables, "
        "look for patterns, and unlock new data as you dig deeper.\n");

    auto hint=[](const char* k, const char* v){
        ImGui::TextColored(C_NEON(0.65f),"%s",k);
        ImGui::SameLine(0,8);
        ImGui::TextColored(C_TEXT(0.65f),"%s",v);
    };
    hint("Enter / EXECUTE ▶","Run the current query");
    hint("↑ / ↓","Browse query history");
    hint("Click table name","Auto-fill a SELECT");
    hint("Hover table name","Preview columns");
    ImGui::Spacing();

    ImGui::TextColored(C_DIM(0.5f),"PATTERNS:");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.60f));
    ImGui::Text("  SELECT * FROM table;");
    ImGui::Text("  SELECT * FROM t WHERE col = 'val';");
    ImGui::Text("  SELECT * FROM t ORDER BY col;");
    ImGui::Text("  SELECT a.*, b.x FROM a JOIN b ON a.id=b.id;");
    ImGui::Text("  SELECT * FROM t WHERE col LIKE '%%text%%';");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::TextColored(C_DIM(0.5f),"TABLES:");
    ImGui::Spacing();
    for(auto& t:g_state.tables())
        ImGui::TextColored(t.unlocked?C_GREEN(0.75f):C_DIM(0.25f),
            "  %s  %s%s",t.unlocked?"✓":"○",t.name.c_str(),
            t.unlocked?"":" [locked]");

    ImGui::Spacing();
    ImGui::PopTextWrapPos();

    UITheme::push_neon_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0.f,9.f});
    if(ImGui::Button("CLOSE",{-1,0})) g_state.app().show_help=false;
    ImGui::PopStyleVar();
    UITheme::pop_neon_button();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────────────────────
int main(int, char**){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){
        SDL_Log("SDL_Init: %s",SDL_GetError()); return 1;
    }

    SDL_Window* win=SDL_CreateWindow(
        "QUERY NOIR — Forensics Terminal",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1440,900,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!win){ SDL_Log("Window: %s",SDL_GetError()); return 1; }

    SDL_Renderer* ren=SDL_CreateRenderer(win,-1,
        SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_ACCELERATED);
    if(!ren){ SDL_Log("Renderer: %s",SDL_GetError()); return 1; }

    // HiDPI handling — match render scale to window scale
    {
        int rw,rh,ww,wh;
        SDL_GetRendererOutputSize(ren,&rw,&rh);
        SDL_GetWindowSize(win,&ww,&wh);
        if(rw>ww) SDL_RenderSetScale(ren,(float)rw/ww,(float)rh/wh);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io=ImGui::GetIO();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename=nullptr;

    UITheme::apply_cyber_noir();
    ImGui_ImplSDL2_InitForSDLRenderer(win,ren);
    ImGui_ImplSDLRenderer2_Init(ren);

    // Font loading — try several paths for portability
    auto try_font=[&](const char* path, float sz)->ImFont*{
        ImFontConfig cfg; cfg.OversampleH=2; cfg.OversampleV=1;
        return io.Fonts->AddFontFromFileTTF(path,sz,&cfg);
    };
    const char* reg_paths[]={
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "../assets/fonts/JetBrainsMono-Regular.ttf",
        nullptr
    };
    const char* bld_paths[]={
        "assets/fonts/JetBrainsMono-Bold.ttf",
        "../assets/fonts/JetBrainsMono-Bold.ttf",
        nullptr
    };
    for(int i=0;reg_paths[i];i++){
        UITheme::font_mono=try_font(reg_paths[i],14.5f);
        if(UITheme::font_mono) break;
    }
    if(!UITheme::font_mono) UITheme::font_mono=io.Fonts->AddFontDefault();

    for(int i=0;bld_paths[i];i++){
        UITheme::font_mono_lg=try_font(bld_paths[i],18.f);
        if(UITheme::font_mono_lg) break;
    }
    if(!UITheme::font_mono_lg) UITheme::font_mono_lg=io.Fonts->AddFontDefault();

    io.Fonts->Build();

    g_state.init_case_orion();

    Uint64 last=SDL_GetPerformanceCounter();
    bool running=true;

    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if(ev.type==SDL_QUIT) running=false;
            if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_ESCAPE)
                running=false;
        }

        // Always read current window size — handles resize correctly
        {
            int ww,wh;
            SDL_GetWindowSize(win,&ww,&wh);
            g_W=(float)ww; g_H=(float)wh;
        }

        Uint64 now=SDL_GetPerformanceCounter();
        float dt=(float)(now-last)/SDL_GetPerformanceFrequency();
        last=now;
        dt=std::min(dt,0.05f);
        g_state.update(dt);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Push our monospace font globally
        if(UITheme::font_mono) ImGui::PushFont(UITheme::font_mono);

        SDL_SetRenderDrawColor(ren,10,15,28,255);
        SDL_RenderClear(ren);

        // Global scanline overlay
        {
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize({g_W,g_H});
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::Begin("##bg",nullptr,
                ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
                ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoNav|
                ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoBringToFrontOnFocus|
                ImGuiWindowFlags_NoBackground);
            ImDrawList* dl=ImGui::GetWindowDrawList();
            for(float y=0;y<g_H;y+=3.f)
                dl->AddLine({0,y},{g_W,y},IM_COL32(0,212,255,4));
            ImGui::End();
        }

        draw_top_bar();
        draw_left_panel();
        draw_center();
        draw_right_panel();
        draw_notifications();
        draw_solved();
        draw_help();

        if(UITheme::font_mono) ImGui::PopFont();

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),ren);
        SDL_RenderPresent(ren);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
