// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR  —  Forensics Terminal
//  SDL2 + Dear ImGui + SQLite3
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
#include <sstream>

// ─── Window size (updated every frame) ───────────────────────────────────────
static float g_W=1440.f, g_H=900.f;

// ─── Layout ──────────────────────────────────────────────────────────────────
static constexpr float TOP_H     = 48.f;
static constexpr float LEFT_W_F  = 0.20f;
static constexpr float RIGHT_W   = 290.f;
static constexpr float RESULTS_H = 255.f;

// ─── Global state ─────────────────────────────────────────────────────────────
static GameState g_state;

// ─── Query input ──────────────────────────────────────────────────────────────
static char   s_qbuf[4096] = "";
static int    s_hist_idx   = -1;
static bool   s_focus_q    = true;
static QueryResult s_result;
static bool   s_has_result = false;

// ─── Accusation input ─────────────────────────────────────────────────────────
static char s_accuse_buf[256] = "";
static bool s_accuse_focus    = false;

// ─── Colors ───────────────────────────────────────────────────────────────────
static ImVec4 C_NEON  (float a=1.f){ return {0.f,   0.831f,1.f,   a}; }
static ImVec4 C_PURPLE(float a=1.f){ return {0.478f,0.361f,1.f,   a}; }
static ImVec4 C_RED   (float a=1.f){ return {1.f,   0.302f,0.302f,a}; }
static ImVec4 C_GREEN (float a=1.f){ return {0.196f,0.902f,0.588f,a}; }
static ImVec4 C_DIM   (float a=1.f){ return {0.502f,0.651f,0.8f,  a}; }
static ImVec4 C_TEXT  (float a=1.f){ return {0.867f,0.933f,1.f,   a}; }
static ImU32  U32(ImVec4 c){ return ImGui::ColorConvertFloat4ToU32(c); }

// ─── UI helpers ───────────────────────────────────────────────────────────────
static void neon_sep(float a=0.30f){
    ImVec2 p=ImGui::GetCursorScreenPos();
    float  w=ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(p,{p.x+w,p.y},U32(C_NEON(a)),1.f);
    ImGui::Dummy({0,2});
}
static void bg_dots(float a=0.018f){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
    ImU32 col=IM_COL32(0,212,255,(int)(255*a));
    for(float x=wp.x+16;x<wp.x+ws.x;x+=32)
        for(float y=wp.y+16;y<wp.y+ws.y;y+=32)
            dl->AddCircleFilled({x,y},1.f,col,4);
}
static void sec_label(const char* s, ImVec4 col){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    dl->AddRectFilled({p.x,p.y+2},{p.x+3,p.y+13},U32(col));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::TextColored(col,"%s",s);
}
static void centered(const char* t, ImVec4 c){
    float tw=ImGui::CalcTextSize(t).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(ImGui::GetContentRegionAvail().x-tw)*.5f);
    ImGui::TextColored(c,"%s",t);
}
static std::string fmt_time(float s){
    int h=(22+(int)(s/3600))%24,m=(int)(s/60)%60,ss=(int)s%60;
    char b[12]; snprintf(b,sizeof(b),"%02d:%02d:%02d",h,m,ss); return b;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TOP BAR
// ─────────────────────────────────────────────────────────────────────────────
static void draw_top_bar(){
    float p=UITheme::pulse(1.4f,0.6f,1.f);
    auto& app=g_state.app();

    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize({g_W,TOP_H});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16.f,0.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  {16.f,0.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.014f,0.024f,0.052f,1.f));
    ImGui::Begin("##top",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos();
        dl->AddRectFilled({wp.x,wp.y+TOP_H-2},{wp.x+g_W,wp.y+TOP_H},
            IM_COL32(0,212,255,(int)(50*p)));
        dl->AddLine({wp.x,wp.y+TOP_H-1},{wp.x+80,wp.y+TOP_H-1},
            IM_COL32(0,212,255,200),1.5f);
        dl->AddLine({wp.x+g_W-80,wp.y+TOP_H-1},{wp.x+g_W,wp.y+TOP_H-1},
            IM_COL32(0,212,255,200),1.5f);
    }

    float ty=(TOP_H-ImGui::GetTextLineHeight())*.5f;
    ImGui::SetCursorPosY(ty);

    ImGui::TextColored(C_NEON(p),"◈"); ImGui::SameLine();
    ImGui::TextColored(C_TEXT(0.88f),"QUERY NOIR"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.22f),"│"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.55f),"CASE:"); ImGui::SameLine(0,4);
    ImGui::TextColored(C_NEON(0.82f),"ORION MURDER"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.22f),"│"); ImGui::SameLine();

    if(app.status==GameStatus::CASE_SOLVED)
        ImGui::TextColored(C_GREEN(),"✓ CLOSED");
    else if(g_state.can_accuse())
        ImGui::TextColored(C_RED(p),"● ACCUSE READY");
    else
        ImGui::TextColored(C_GREEN(p),"● ACTIVE");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.22f),"│"); ImGui::SameLine();

    int tot=(int)g_state.clues().size();
    ImGui::TextColored(C_DIM(0.55f),"EVIDENCE:"); ImGui::SameLine(0,4);
    ImGui::TextColored(app.discovered_clues>0?C_GREEN():C_DIM(0.4f),
        "%d/%d",app.discovered_clues,tot);
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.22f),"│"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.55f),"TIME:"); ImGui::SameLine(0,4);
    ImGui::TextColored(C_NEON(0.65f+0.35f*p),"%s",fmt_time(app.game_time).c_str());

    // Right-side buttons
    float bx=g_W-250.f;
    ImGui::SetCursorPosY(ty);
    ImGui::SameLine(bx);

    // SCHEMA toggle
    ImGui::PushStyleColor(ImGuiCol_Button,
        app.show_schema?ImVec4(0.f,0.5f,0.75f,0.35f):ImVec4(0.f,0.5f,0.75f,0.10f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.f,0.5f,0.75f,0.28f));
    ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10.f,5.f});
    if(ImGui::Button("SCHEMA")) app.show_schema=!app.show_schema;
    ImGui::PopStyleColor(3); ImGui::PopStyleVar();

    ImGui::SameLine(0,8);

    // HELP toggle
    UITheme::push_neon_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10.f,5.f});
    if(ImGui::Button("? HELP")) app.show_help=!app.show_help;
    ImGui::PopStyleVar();
    UITheme::pop_neon_button();

    // ACCUSE button — only visible when enough evidence
    if(g_state.can_accuse() && app.status!=GameStatus::CASE_SOLVED){
        ImGui::SameLine(0,8);
        float ap2=UITheme::pulse(3.f,0.6f,1.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.f,0.3f,0.3f,0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f,0.3f,0.3f,0.30f));
        ImGui::PushStyleColor(ImGuiCol_Text,          C_RED(ap2));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10.f,5.f});
        if(ImGui::Button("▶ ACCUSE")){
            app.accuse.active=true;
            s_accuse_focus=true;
            memset(s_accuse_buf,0,sizeof(s_accuse_buf));
        }
        ImGui::PopStyleColor(3); ImGui::PopStyleVar(2);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  LEFT PANEL — Tables + Evidence Board
// ─────────────────────────────────────────────────────────────────────────────
static void draw_left_panel(){
    float lw=floorf(g_W*LEFT_W_F);
    ImGui::SetNextWindowPos({0,TOP_H});
    ImGui::SetNextWindowSize({lw,g_H-TOP_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.018f,0.032f,0.068f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{13.f,11.f});
    ImGui::Begin("##left",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
        dl->AddLine({wp.x+ws.x-1,wp.y},{wp.x+ws.x-1,wp.y+ws.y},
            IM_COL32(0,212,255,65));
        bg_dots(0.016f);
    }

    ImGui::Spacing();
    sec_label("TABLES",C_DIM(0.6f));
    ImGui::Spacing();

    for(auto& t:g_state.tables()){
        float tp=UITheme::pulse(2.f+(float)(&t-&g_state.tables()[0])*.8f,.5f,1.f);
        if(!t.unlocked){
            ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.20f));
            ImGui::Text("  %s  %s",t.icon.c_str(),t.name.c_str());
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,ImVec4(0.018f,0.032f,0.068f,.98f));
                ImGui::BeginTooltip();
                ImGui::TextColored(C_RED(0.7f),"[LOCKED]");
                ImGui::Spacing();
                ImGui::PushTextWrapPos(260.f);
                ImGui::TextColored(C_DIM(0.55f),"%s",t.unlock_condition.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0,0.831f,1.f,.08f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(0,0.831f,1.f,.15f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0,0.831f,1.f,.25f));
            ImGui::PushStyleColor(ImGuiCol_Text,C_NEON(tp));

            // Show row count badge
            char lbl[128];
            snprintf(lbl,sizeof(lbl),"  %s  %s  (%d rows)##%s",
                t.icon.c_str(),t.name.c_str(),t.row_count,t.name.c_str());
            bool click=ImGui::Selectable(lbl,false,ImGuiSelectableFlags_None,{-1,22});
            ImGui::PopStyleColor(4);

            if(click){
                snprintf(s_qbuf,sizeof(s_qbuf),"SELECT * FROM %s LIMIT 5;",t.name.c_str());
                s_focus_q=true;
            }

            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,ImVec4(0.015f,0.028f,0.060f,.98f));
                ImGui::BeginTooltip();
                ImGui::TextColored(C_NEON(),"%s",t.display_name.c_str());
                ImGui::TextColored(C_DIM(0.4f),"%d rows",t.row_count);
                ImGui::Separator(); ImGui::Spacing();
                for(auto& col:t.columns){
                    ImGui::TextColored(C_DIM(0.35f),"  %-20s",col.name.c_str());
                    ImGui::SameLine(0,0);
                    ImGui::TextColored(C_DIM(0.22f),"%s",col.type.c_str());
                }
                ImGui::Spacing();
                ImGui::TextColored(C_DIM(0.3f),"[click → LIMIT 5 preview]");
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        }
        ImGui::Spacing();
    }

    // ── Evidence Board ────────────────────────────────────────────────────────
    ImGui::Spacing();
    neon_sep(0.18f);
    ImGui::Spacing();
    sec_label("EVIDENCE",C_DIM(0.6f));
    ImGui::Spacing();

    for(auto& clue:g_state.clues()){
        if(clue.discovered){
            ImVec2 p2=ImGui::GetCursorScreenPos();
            float rw=ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p2.x-2,p2.y},{p2.x+rw+2,p2.y+20},IM_COL32(50,230,150,12),2.f);
            ImGui::PushStyleColor(ImGuiCol_Text,C_GREEN(0.85f));
            ImGui::TextWrapped("  ✓  %s",clue.title.c_str());
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,ImVec4(0.012f,0.022f,0.050f,.98f));
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(280.f);
                ImGui::TextColored(C_GREEN(),"%s",clue.title.c_str());
                ImGui::Spacing();
                ImGui::TextColored(C_TEXT(0.72f),"%s",clue.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.20f));
            ImGui::Text("  ○  [UNSOLVED]");
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,ImVec4(0.012f,0.022f,0.050f,.98f));
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(280.f);
                ImGui::TextColored(C_DIM(0.5f),"Hint:");
                ImGui::Spacing();
                ImGui::TextColored(C_DIM(0.45f),"%s",clue.hint.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        }
        ImGui::Spacing();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  CENTER — Console + Results
// ─────────────────────────────────────────────────────────────────────────────
static void draw_center(){
    float lw=floorf(g_W*LEFT_W_F);
    float cx=lw, cw=g_W-lw-RIGHT_W;
    float ch=g_H-TOP_H-RESULTS_H;

    // ── Console ───────────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos({cx,TOP_H});
    ImGui::SetNextWindowSize({cw,ch});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.014f,0.026f,0.055f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18.f,14.f});
    ImGui::Begin("##con",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
        bg_dots(0.012f);
        float p2=UITheme::pulse(1.9f,.4f,1.f);
        dl->AddRectFilled({wp.x,wp.y},{wp.x+ws.x,wp.y+2.f},
            IM_COL32(0,212,255,(int)(42*p2)));
    }

    ImGui::Spacing();
    float p=UITheme::pulse(1.7f,.55f,1.f);
    ImGui::TextColored(C_NEON(p),"// QUERY CONSOLE");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.28f),"— NovaCorp Internal DB  ·  SQLite3  ·  Type SCHEMA to inspect tables");
    ImGui::Spacing();
    neon_sep(0.16f);
    ImGui::Spacing();

    // Prompt
    ImGui::TextColored(C_NEON(0.82f),"noir@forensics");
    ImGui::SameLine(0,0); ImGui::TextColored(C_DIM(0.4f),":");
    ImGui::SameLine(0,0); ImGui::TextColored(C_PURPLE(0.72f),"~/orion");
    ImGui::SameLine(0,0); ImGui::TextColored(C_DIM(0.55f),"$ ");
    ImGui::SameLine(0,6);

    float btn_w=108.f;
    float iw=ImGui::GetContentRegionAvail().x-btn_w-10.f;
    ImGui::SetNextItemWidth(iw);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.005f,0.010f,0.028f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f,0.831f,1.f,.05f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.f,0.831f,1.f,.09f));
    ImGui::PushStyleColor(ImGuiCol_Border,
        g_state.app().query_executing?C_NEON(0.9f):C_NEON(0.28f));
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
        } else { d->DeleteChars(0,d->BufTextLen); }
        return 0;
    };

    bool enter=ImGui::InputText("##qi",s_qbuf,sizeof(s_qbuf),
        ImGuiInputTextFlags_EnterReturnsTrue|
        ImGuiInputTextFlags_CallbackHistory,hcb);

    ImGui::PopStyleColor(4); ImGui::PopStyleVar(2);
    ImGui::SameLine(0,10);

    bool exec_click=false;
    {
        float ep=UITheme::pulse(2.2f,.65f,1.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f,0.831f,1.f,.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f,0.831f,1.f,.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.f,0.831f,1.f,.40f));
        ImGui::PushStyleColor(ImGuiCol_Text,C_NEON(ep));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0.f,7.f});
        exec_click=ImGui::Button("EXECUTE ▶",{btn_w,-1});
        ImGui::PopStyleColor(4); ImGui::PopStyleVar(2);
    }

    if((enter||exec_click)&&strlen(s_qbuf)>0){
        g_state.app().glitch_timer=0.18f;
        s_result=g_state.run_query(s_qbuf);
        s_has_result=true;
        s_hist_idx=-1;
    }

    if(g_state.app().query_executing){
        ImGui::Spacing();
        float ep=UITheme::pulse(9.f,.3f,1.f);
        ImGui::TextColored(C_NEON(ep),"  ◌  PROCESSING...");
    }

    // ── Reference section — just column reminders, NO pre-filled queries ───
    ImGui::Spacing();
    neon_sep(0.08f);
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.30f),"↑↓ history  ·  Enter execute  ·  SCHEMA command toggles schema view  ·  ACCUSE when ready to name the killer");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ── Results ───────────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos({cx,TOP_H+ch});
    ImGui::SetNextWindowSize({cw,RESULTS_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.010f,0.018f,0.040f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18.f,10.f});
    ImGui::Begin("##res",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_HorizontalScrollbar);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos();
        dl->AddRectFilled({wp.x,wp.y},{wp.x+cw,wp.y+2},IM_COL32(0,212,255,38));
        bg_dots(0.009f);
    }

    ImGui::TextColored(C_DIM(0.42f),"OUTPUT");
    if(s_has_result&&!s_result.is_error){
        ImGui::SameLine(0,10); ImGui::TextColored(C_DIM(0.25f),"—");
        ImGui::SameLine(0,8);
        ImGui::TextColored(C_GREEN(0.62f),"%zu row%s",
            s_result.rows.size(),s_result.rows.size()==1?"":"s");
        if(!s_result.flagged_rows.empty()){
            ImGui::SameLine(0,10);
            ImGui::TextColored(C_RED(0.55f),"⚠ %zu anomalous",
                s_result.flagged_rows.size());
        }
    }
    ImGui::Spacing(); neon_sep(0.10f); ImGui::Spacing();

    float rh=RESULTS_H-56.f;

    if(!s_has_result){
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+rh*.3f);
        centered("No query yet.",C_DIM(0.20f));
    } else if(s_result.is_error){
        ImGui::PushStyleColor(ImGuiCol_Text,C_RED(0.85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.2f,.02f,.02f,.3f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10.f,7.f});
        std::string em="[ERROR]  "+s_result.error;
        ImGui::InputTextMultiline("##em",em.data(),em.size(),
            {-1,rh},ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
    } else if(s_result.rows.empty()){
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+rh*.3f);
        centered("Query returned 0 rows.",C_DIM(0.28f));
    } else {
        int nc=(int)s_result.columns.size();
        if(nc>0&&ImGui::BeginTable("##rt",nc,
            ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|
            ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY|
            ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoHostExtendX,
            {-1,rh}))
        {
            for(auto& col:s_result.columns)
                ImGui::TableSetupColumn(col.c_str(),
                    ImGuiTableColumnFlags_WidthFixed,130.f);
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,ImVec4(0.f,.22f,.36f,1.f));
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            auto& flg=s_result.flagged_rows;
            for(size_t ri=0;ri<s_result.rows.size();ri++){
                bool flag=std::find(flg.begin(),flg.end(),ri)!=flg.end();
                ImGui::TableNextRow();
                if(flag){
                    ImVec2 rm=ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        rm,{rm.x+9999.f,rm.y+22.f},IM_COL32(255,80,80,16));
                }
                for(size_t ci=0;ci<s_result.rows[ri].size();ci++){
                    ImGui::TableSetColumnIndex((int)ci);
                    ImGui::TextColored(
                        flag?C_RED(0.82f):C_TEXT(0.78f),
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
    ImGui::SetNextWindowPos({rx,TOP_H});
    ImGui::SetNextWindowSize({RIGHT_W,g_H-TOP_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.016f,0.030f,0.062f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{13.f,11.f});
    ImGui::Begin("##right",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);

    {
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
        dl->AddLine({wp.x+1,wp.y},{wp.x+1,wp.y+ws.y},IM_COL32(0,212,255,70));
        bg_dots(0.016f);
    }

    ImGui::Spacing();
    float p=UITheme::pulse(1.4f,.55f,1.f);
    ImGui::TextColored(C_NEON(p),"◈");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_TEXT(0.75f),"DETECTIVE FEED");
    ImGui::Spacing(); neon_sep(0.16f); ImGui::Spacing();

    ImGui::BeginChild("##fc",{-1,g_H-TOP_H-62.f},false,ImGuiWindowFlags_NoScrollbar);

    for(auto& e:g_state.feed()){
        ImVec4 col; const char* pfx;
        switch(e.type){
            case NarrativeType::MONOLOGUE: col=C_TEXT(0.75f);  pfx=">  "; break;
            case NarrativeType::HINT:      col=C_NEON(0.80f);  pfx="?  "; break;
            case NarrativeType::ALERT:     col=C_RED(0.85f);   pfx="!  "; break;
            case NarrativeType::SUCCESS:   col=C_GREEN(0.90f); pfx="✓  "; break;
            case NarrativeType::SYSTEM:    col=C_PURPLE(0.70f);pfx="#  "; break;
            default:                       col=C_DIM(0.55f);   pfx="   "; break;
        }
        {
            ImVec2 cp=ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                {cp.x+3,cp.y+7},2.5f,U32(col),8);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+11);
        int chars=e.revealed?(int)e.text.size()
            :std::min((int)e.reveal_timer,(int)e.text.size());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+RIGHT_W-24.f);
        ImGui::TextColored(col,"%s%s",pfx,e.text.substr(0,chars).c_str());
        ImGui::PopTextWrapPos();
        if(!e.revealed){
            ImGui::SameLine(0,0);
            if(UITheme::pulse(6.f,0.f,1.f)>0.5f) ImGui::TextColored(col,"▌");
        }
        ImGui::Spacing();
    }

    ImGui::EndChild();
    neon_sep(0.08f);
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.25f),"↑↓ history · Enter run · SCHEMA cmd");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  SCHEMA OVERLAY — full table structure viewer
// ─────────────────────────────────────────────────────────────────────────────
static void draw_schema(){
    if(!g_state.app().show_schema) return;

    float sw=420.f, sh=g_H-TOP_H-20.f;
    float lw=floorf(g_W*LEFT_W_F);
    ImGui::SetNextWindowPos({lw+10,TOP_H+10});
    ImGui::SetNextWindowSize({sw,sh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.010f,0.020f,0.045f,0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,C_NEON(0.40f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16.f,12.f});

    ImGui::Begin("##schema",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize);

    ImGui::TextColored(C_NEON(),"DATABASE SCHEMA");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.3f),"— NovaCorp Internal Systems");
    ImGui::Spacing(); neon_sep(0.25f); ImGui::Spacing();

    ImGui::BeginChild("##sc2",{-1,-1});
    for(auto& t:g_state.tables()){
        if(!t.unlocked){
            ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.22f));
            ImGui::Text("  %s  [LOCKED]",t.name.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            continue;
        }

        // Table header
        ImGui::PushStyleColor(ImGuiCol_Text,C_NEON(0.9f));
        bool open=ImGui::TreeNodeEx(t.name.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen,"%s",t.name.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextColored(C_DIM(0.3f),"(%d rows)",t.row_count);

        if(open){
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing,14.f);
            for(auto& col:t.columns){
                ImGui::TextColored(C_DIM(0.45f),"    %-22s",col.name.c_str());
                ImGui::SameLine(0,0);
                ImGui::TextColored(C_DIM(0.25f),"%s",col.type.c_str());
                if(!col.note.empty()){
                    ImGui::SameLine(0,6);
                    ImGui::TextColored(C_PURPLE(0.5f),"%s",col.note.c_str());
                }
            }
            ImGui::PopStyleVar();
            ImGui::TreePop();
        }
        ImGui::Spacing();
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ACCUSATION MODAL
// ─────────────────────────────────────────────────────────────────────────────
static void draw_accuse_modal(){
    auto& app=g_state.app();
    if(!app.accuse.active || app.status==GameStatus::CASE_SOLVED) return;

    float mw=500.f, mh=260.f;
    ImGui::SetNextWindowPos({(g_W-mw)*.5f,(g_H-mh)*.5f});
    ImGui::SetNextWindowSize({mw,mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.015f,0.028f,0.060f,0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,C_RED(UITheme::pulse(2.f,.5f,1.f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26.f,20.f});
    ImGui::Begin("##acc",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize);

    ImGui::TextColored(C_RED(),"MAKE YOUR ACCUSATION");
    ImGui::Spacing(); neon_sep(0.20f); ImGui::Spacing();
    ImGui::PushTextWrapPos(mw-52.f);
    ImGui::TextColored(C_TEXT(0.70f),
        "You have enough evidence to name a suspect. "
        "Type the full name of the person you believe murdered Marcus Orion. "
        "Choose carefully — a wrong accusation costs you.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // Wrong feedback
    if(app.accuse.wrong_timer>0.f){
        ImGui::PushTextWrapPos(mw-52.f);
        ImGui::TextColored(C_RED(0.80f),"✗  %s",app.accuse.wrong_feedback.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }

    ImGui::TextColored(C_DIM(0.5f),"Name:");
    ImGui::SameLine(0,8);
    ImGui::SetNextItemWidth(mw-120.f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0.f,0.f,0.f,.6f));
    ImGui::PushStyleColor(ImGuiCol_Border,C_RED(0.50f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10.f,7.f});

    if(s_accuse_focus){ ImGui::SetKeyboardFocusHere(); s_accuse_focus=false; }
    bool enter=ImGui::InputText("##ac",s_accuse_buf,sizeof(s_accuse_buf),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);

    ImGui::SameLine(0,8);
    UITheme::push_danger_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12.f,7.f});
    bool submit=(ImGui::Button("CHARGE")||enter)&&strlen(s_accuse_buf)>0;
    ImGui::PopStyleVar();
    UITheme::pop_danger_button();

    if(submit){
        bool correct=g_state.try_accuse(s_accuse_buf);
        if(!correct){
            memset(s_accuse_buf,0,sizeof(s_accuse_buf));
            s_accuse_focus=true;
        }
    }

    ImGui::SameLine(0,8);
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12.f,7.f});
    if(ImGui::Button("CANCEL")) app.accuse.active=false;
    ImGui::PopStyleColor(2); ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CASE SOLVED OVERLAY
// ─────────────────────────────────────────────────────────────────────────────
static void draw_solved(){
    if(g_state.app().status!=GameStatus::CASE_SOLVED) return;
    float p=UITheme::pulse(1.3f,.6f,1.f);
    float mw=640.f, mh=460.f;
    ImGui::SetNextWindowPos({(g_W-mw)*.5f,(g_H-mh)*.5f});
    ImGui::SetNextWindowSize({mw,mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.015f,0.028f,0.060f,.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,C_GREEN(p));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{28.f,22.f});
    ImGui::Begin("##sol",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);

    ImGui::Spacing();
    centered("CASE CLOSED",C_GREEN(p));
    ImGui::Spacing();
    centered("THE ORION MURDER — RESOLVED",C_TEXT(0.85f));
    ImGui::Spacing(); neon_sep(0.14f); ImGui::Spacing();
    ImGui::PushTextWrapPos(mw-56.f);
    ImGui::TextColored(C_DIM(0.55f),"CHARGED:");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_RED(0.90f),"Lena Park — Senior Data Analyst");
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.42f),"ACCESSORY / CONSPIRACY:");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_RED(0.65f),"Hana Mori (ordered the act)");
    ImGui::Spacing();
    neon_sep(0.10f);
    ImGui::Spacing();
    ImGui::TextColored(C_TEXT(0.68f),
        "Marcus Orion found $1.35M in fraudulent vendor payments — routed from NovaCorp "
        "through ExtShell-LLC to Rex Calloway, looping offshore back to Mori. "
        "He planned to expose it that night.\n\n"
        "Mori paid Park $12,000 to stop him. Park modified badge MASTER two months prior "
        "— giving herself override access she had no right to. At 22:15 she messaged an "
        "external contact: 'Tonight is the only chance.'\n\n"
        "She met Marcus in Server Room B-7 at 22:30. He never left.\n\n"
        "Elena Vasquez received Mori's order at 20:05. She arrived at 23:58 — "
        "too late. Park had already used the MASTER override at 02:14.\n\n"
        "Carl Bremer flagged the badge tampering in January. Vasquez buried the report.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    centered("Justice served.",C_GREEN(p));
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(.196f,.902f,.588f,.09f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.196f,.902f,.588f,.22f));
    ImGui::PushStyleColor(ImGuiCol_Text,C_GREEN(p));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0.f,9.f});
    if(ImGui::Button("CLOSE FILE",{-1,0}))
        g_state.app().status=GameStatus::ACTIVE;
    ImGui::PopStyleColor(3); ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  HELP OVERLAY
// ─────────────────────────────────────────────────────────────────────────────
static void draw_help(){
    if(!g_state.app().show_help) return;
    float mw=560.f, mh=500.f;
    ImGui::SetNextWindowPos({(g_W-mw)*.5f,(g_H-mh)*.5f});
    ImGui::SetNextWindowSize({mw,mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.015f,0.028f,0.060f,.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,C_NEON(0.42f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{22.f,18.f});
    ImGui::Begin("##hlp",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);

    ImGui::TextColored(C_NEON(),"HOW TO PLAY");
    ImGui::Spacing(); neon_sep(0.22f); ImGui::Spacing();

    ImGui::PushTextWrapPos(mw-44.f);
    ImGui::TextColored(C_TEXT(0.72f),
        "Write SQL queries to investigate NovaCorp's internal database. "
        "Evidence is only unlocked by writing specific, targeted queries. "
        "SELECT * from a table is rarely enough.\n");

    auto kv=[](const char* k, const char* v){
        ImGui::TextColored(C_NEON(0.62f),"%-20s",k);
        ImGui::SameLine(0,6);
        ImGui::TextColored(C_TEXT(0.65f),"%s",v);
    };
    kv("Enter / EXECUTE","Run query");
    kv("↑ / ↓","Query history");
    kv("SCHEMA","Toggle schema viewer (or click button in top bar)");
    kv("ACCUSE","Appears in top bar when you have enough evidence");
    kv("Hover clue ○","See the hint for that clue");
    kv("Hover table name","See column schema");

    ImGui::Spacing(); neon_sep(0.12f); ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.5f),"USEFUL SQL PATTERNS:");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text,C_DIM(0.58f));
    ImGui::Text("  SELECT col1, col2 FROM table WHERE col = 'value';");
    ImGui::Text("  SELECT * FROM table WHERE col LIKE '%%keyword%%';");
    ImGui::Text("  SELECT * FROM table ORDER BY col DESC;");
    ImGui::Text("  SELECT a.col, b.col FROM a JOIN b ON a.id = b.id;");
    ImGui::Text("  SELECT from_acct, SUM(amount) FROM transactions");
    ImGui::Text("         GROUP BY from_acct;");
    ImGui::Text("  SELECT * FROM table WHERE col1 = 'x' AND col2 > 100;");
    ImGui::PopStyleColor();
    ImGui::Spacing(); neon_sep(0.12f); ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.5f),"HOW UNLOCKS WORK:");
    ImGui::Spacing();
    ImGui::TextColored(C_TEXT(0.62f),
        "New tables unlock when your query on the previous table "
        "returns meaningful results — not just any query. "
        "You need to find the right data to earn the next file.");
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.5f),"TO SOLVE:");
    ImGui::Spacing();
    ImGui::TextColored(C_TEXT(0.62f),
        "Collect at least 5 pieces of evidence (including badge tampering "
        "and Lena's pre-murder message). Then click ACCUSE and type the killer's name.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    UITheme::push_neon_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0.f,9.f});
    if(ImGui::Button("CLOSE",{-1,0})) g_state.app().show_help=false;
    ImGui::PopStyleVar();
    UITheme::pop_neon_button();

    ImGui::End();
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NOTIFICATIONS
// ─────────────────────────────────────────────────────────────────────────────
static void draw_notifications(){
    float yoff=g_H-18.f;
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
        float pw=sz.x+28.f,ph=34.f,px=(g_W-pw)*.5f,py=yoff-ph;
        ImGui::SetNextWindowPos({px,py}); ImGui::SetNextWindowSize({pw,ph});
        ImGui::SetNextWindowBgAlpha(0.92f*a);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.01f,0.02f,0.05f,.95f));
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
        ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);
        yoff-=ph+5.f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────────────────────
int main(int,char**){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){
        SDL_Log("SDL_Init: %s",SDL_GetError()); return 1;
    }
    SDL_Window* win=SDL_CreateWindow(
        "QUERY NOIR — Forensics Terminal",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1440,900,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!win){ SDL_Log("Window: %s",SDL_GetError()); return 1; }

    SDL_Renderer* ren=SDL_CreateRenderer(win,-1,
        SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_ACCELERATED);
    if(!ren){ SDL_Log("Renderer: %s",SDL_GetError()); return 1; }

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

    auto try_font=[&](const char* path,float sz)->ImFont*{
        ImFontConfig cfg; cfg.OversampleH=2; cfg.OversampleV=1;
        return io.Fonts->AddFontFromFileTTF(path,sz,&cfg);
    };
    const char* rp[]={"assets/fonts/JetBrainsMono-Regular.ttf",
                       "../assets/fonts/JetBrainsMono-Regular.ttf",nullptr};
    const char* bp[]={"assets/fonts/JetBrainsMono-Bold.ttf",
                       "../assets/fonts/JetBrainsMono-Bold.ttf",nullptr};
    for(int i=0;rp[i];i++){ UITheme::font_mono=try_font(rp[i],14.f); if(UITheme::font_mono) break; }
    if(!UITheme::font_mono) UITheme::font_mono=io.Fonts->AddFontDefault();
    for(int i=0;bp[i];i++){ UITheme::font_mono_lg=try_font(bp[i],17.f); if(UITheme::font_mono_lg) break; }
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
            if(ev.type==SDL_KEYDOWN&&ev.key.keysym.sym==SDLK_ESCAPE){
                // Escape closes accuse modal first, then quits
                if(g_state.app().accuse.active)
                    g_state.app().accuse.active=false;
                else
                    running=false;
            }
        }
        {
            int ww,wh; SDL_GetWindowSize(win,&ww,&wh);
            g_W=(float)ww; g_H=(float)wh;
        }

        Uint64 now=SDL_GetPerformanceCounter();
        float dt=std::min((float)(now-last)/SDL_GetPerformanceFrequency(),0.05f);
        last=now;
        g_state.update(dt);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if(UITheme::font_mono) ImGui::PushFont(UITheme::font_mono);

        SDL_SetRenderDrawColor(ren,9,14,26,255);
        SDL_RenderClear(ren);

        // Global scanline bg
        {
            ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({g_W,g_H});
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::Begin("##bg",nullptr,
                ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
                ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoNav|
                ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoBringToFrontOnFocus|
                ImGuiWindowFlags_NoBackground);
            ImDrawList* dl=ImGui::GetWindowDrawList();
            for(float y=0;y<g_H;y+=3.f)
                dl->AddLine({0,y},{g_W,y},IM_COL32(0,212,255,3));
            ImGui::End();
        }

        draw_top_bar();
        draw_left_panel();
        draw_center();
        draw_right_panel();
        draw_schema();          // floating schema overlay
        draw_accuse_modal();    // accusation modal
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
