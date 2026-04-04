// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Renderer
//  Draws: top bar, left panel (tables + evidence), schema overlay,
//  accusation modal, case-solved overlay, help overlay, notifications.
//  Shared low-level UI helpers (neon_sep, bg_dots, etc.) also live here.
// ═══════════════════════════════════════════════════════════════════════════

#include "RenderCommon.h"
#include "CaseManager.h"
#include <cstring>
#include <sstream>

// ─── Accusation input state (used by draw_top_bar + draw_accuse_modal) ────────
char s_accuse_buf[256] = "";
bool s_accuse_focus    = false;

// ─────────────────────────────────────────────────────────────────────────────
//  Shared UI helpers
// ─────────────────────────────────────────────────────────────────────────────

void neon_sep(float a){
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(p, {p.x+w, p.y}, U32(C_NEON(a)), 1.f);
    ImGui::Dummy({0, 2});
}

void bg_dots(float a){
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
    ImU32 col = IM_COL32(0, 212, 255, (int)(255*a));
    for(float x = wp.x+16; x < wp.x+ws.x; x += 32)
        for(float y = wp.y+16; y < wp.y+ws.y; y += 32)
            dl->AddCircleFilled({x,y}, 1.f, col, 4);
}

void sec_label(const char* label, ImVec4 col){
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled({p.x, p.y+2}, {p.x+3, p.y+13}, U32(col));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
    ImGui::TextColored(col, "%s", label);
}

void centered(const char* text, ImVec4 col){
    float tw = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(
        ImGui::GetCursorPosX() +
        (ImGui::GetContentRegionAvail().x - tw) * 0.5f);
    ImGui::TextColored(col, "%s", text);
}

std::string fmt_time(float s){
    int h  = (22 + (int)(s/3600)) % 24;
    int m  = (int)(s/60) % 60;
    int ss = (int)s % 60;
    char b[12];
    snprintf(b, sizeof(b), "%02d:%02d:%02d", h, m, ss);
    return b;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TOP BAR
// ─────────────────────────────────────────────────────────────────────────────
void draw_top_bar(){
    float p = UITheme::pulse(1.4f, 0.6f, 1.f);
    auto& app = g_state.app();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({g_W, TOP_H});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16.f, 0.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   {16.f, 0.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.014f,0.024f,0.052f,1.f));
    ImGui::Begin("##top", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize     | ImGuiWindowFlags_NoScrollbar);

    // Bottom glow bar + corner accents
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        dl->AddRectFilled({wp.x, wp.y+TOP_H-2}, {wp.x+g_W, wp.y+TOP_H},
            IM_COL32(0,212,255,(int)(50*p)));
        dl->AddLine({wp.x, wp.y+TOP_H-1}, {wp.x+80, wp.y+TOP_H-1},
            IM_COL32(0,212,255,200), 1.5f);
        dl->AddLine({wp.x+g_W-80, wp.y+TOP_H-1}, {wp.x+g_W, wp.y+TOP_H-1},
            IM_COL32(0,212,255,200), 1.5f);
    }

    float ty = (TOP_H - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPosY(ty);

ImGui::TextColored(C_NEON(p), "◈"); ImGui::SameLine();
ImGui::TextColored(C_TEXT(0.88f), "QUERY NOIR"); ImGui::SameLine();
ImGui::TextColored(C_DIM(0.22f), "|"); ImGui::SameLine();
ImGui::TextColored(C_DIM(0.55f), "CASE:"); ImGui::SameLine(0, 4);
static int current_case_idx = 0;
const char* cases[] = { "ORION MURDER", "PROJECT BLUEBIRD" };
ImGui::PushStyleColor(ImGuiCol_Text, C_NEON(0.82f)); 
ImGui::SetNextItemWidth(180.f);

if (ImGui::Combo("##case_combo", &current_case_idx, cases, IM_ARRAYSIZE(cases))) {
    if (current_case_idx == 0) CaseManager::load_case(g_state, "orion");
    if (current_case_idx == 1) CaseManager::load_case(g_state, "espionage");
}
ImGui::PopStyleColor(); // Return to normal text color for the rest of the UI

ImGui::SameLine();
ImGui::TextColored(C_DIM(0.22f), "|"); ImGui::SameLine();

    if(app.status == GameStatus::CASE_SOLVED)
        ImGui::TextColored(C_GREEN(), "✓ CLOSED");
    else if(g_state.can_accuse())
        ImGui::TextColored(C_RED(p), "● ACCUSE READY");
    else
        ImGui::TextColored(C_GREEN(p), "● ACTIVE");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.22f), "|"); ImGui::SameLine();

    int tot = (int)g_state.clues().size();
    ImGui::TextColored(C_DIM(0.55f), "EVIDENCE:"); ImGui::SameLine(0,4);
    ImGui::TextColored(app.discovered_clues > 0 ? C_GREEN() : C_DIM(0.4f),
        "%d/%d", app.discovered_clues, tot);
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.22f), "|"); ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.55f), "TIME:"); ImGui::SameLine(0,4);
    ImGui::TextColored(C_NEON(0.65f + 0.35f*p), "%s",
        fmt_time(app.game_time).c_str());

    // Right-side buttons
    ImGui::SetCursorPosY(ty);
    ImGui::SameLine(g_W - 250.f);

    // SCHEMA toggle
    ImGui::PushStyleColor(ImGuiCol_Button,
        app.show_schema ? ImVec4(0.f,0.5f,0.75f,0.35f)
                        : ImVec4(0.f,0.5f,0.75f,0.10f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f,0.5f,0.75f,0.28f));
    ImGui::PushStyleColor(ImGuiCol_Text, C_DIM(0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.f,5.f});
    if(ImGui::Button("SCHEMA")) app.show_schema = !app.show_schema;
    ImGui::PopStyleColor(3); ImGui::PopStyleVar();

    ImGui::SameLine(0,8);

    // HELP toggle
    UITheme::push_neon_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.f,5.f});
    if(ImGui::Button("? HELP")) app.show_help = !app.show_help;
    ImGui::PopStyleVar();
    UITheme::pop_neon_button();

    // ACCUSE — only visible when enough evidence gathered
    if(g_state.can_accuse() && app.status != GameStatus::CASE_SOLVED){
        ImGui::SameLine(0,8);
        float ap2 = UITheme::pulse(3.f, 0.6f, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.f,0.3f,0.3f,0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f,0.3f,0.3f,0.30f));
        ImGui::PushStyleColor(ImGuiCol_Text,          C_RED(ap2));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    {10.f,5.f});
        if(ImGui::Button("▶ ACCUSE")){
            app.accuse.active = true;
            s_accuse_focus    = true;
            memset(s_accuse_buf, 0, sizeof(s_accuse_buf));
        }
        ImGui::PopStyleColor(3); ImGui::PopStyleVar(2);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  LEFT PANEL — Table Navigator + Evidence Board
// ─────────────────────────────────────────────────────────────────────────────
void draw_left_panel(){
    float lw = floorf(g_W * LEFT_W_F);
    ImGui::SetNextWindowPos({0, TOP_H});
    ImGui::SetNextWindowSize({lw, g_H - TOP_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f,0.032f,0.068f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {13.f,11.f});
    ImGui::Begin("##left", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        dl->AddLine({wp.x+ws.x-1, wp.y}, {wp.x+ws.x-1, wp.y+ws.y},
            IM_COL32(0,212,255,65));
        bg_dots(0.016f);
    }

    ImGui::Spacing();
    sec_label("TABLES", C_DIM(0.6f));
    ImGui::Spacing();

    for(auto& t : g_state.tables()){
        float tp = UITheme::pulse(
            2.f + (float)(&t - &g_state.tables()[0]) * 0.8f, 0.5f, 1.f);

        if(!t.unlocked){
            ImGui::PushStyleColor(ImGuiCol_Text, C_DIM(0.20f));
            ImGui::Text("  %s  %s", t.icon.c_str(), t.name.c_str());
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,
                    ImVec4(0.018f,0.032f,0.068f,0.98f));
                ImGui::BeginTooltip();
                ImGui::TextColored(C_RED(0.7f), "[LOCKED]");
                ImGui::Spacing();
                ImGui::PushTextWrapPos(260.f);
                ImGui::TextColored(C_DIM(0.55f), "%s",
                    t.unlock_condition.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header,
                ImVec4(0,0.831f,1.f,0.08f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                ImVec4(0,0.831f,1.f,0.15f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                ImVec4(0,0.831f,1.f,0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, C_NEON(tp));

            char lbl[128];
            snprintf(lbl, sizeof(lbl), "  %s  %s  (%d rows)##%s",
                t.icon.c_str(), t.name.c_str(), t.row_count, t.name.c_str());
            bool click = ImGui::Selectable(lbl, false,
                ImGuiSelectableFlags_None, {-1, 22});
            ImGui::PopStyleColor(4);

            if(click){
                snprintf(s_qbuf, sizeof(s_qbuf),
                    "SELECT * FROM %s LIMIT 5;", t.name.c_str());
                s_focus_q = true;
            }

            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,
                    ImVec4(0.015f,0.028f,0.060f,0.98f));
                ImGui::BeginTooltip();
                ImGui::TextColored(C_NEON(), "%s", t.display_name.c_str());
                ImGui::TextColored(C_DIM(0.4f), "%d rows", t.row_count);
                ImGui::Separator(); ImGui::Spacing();
                for(auto& col : t.columns){
                    ImGui::TextColored(C_DIM(0.35f), "  %-20s",
                        col.name.c_str());
                    ImGui::SameLine(0,0);
                    ImGui::TextColored(C_DIM(0.22f), "%s", col.type.c_str());
                }
                ImGui::Spacing();
                ImGui::TextColored(C_DIM(0.3f), "[click to preview LIMIT 5]");
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
    sec_label("EVIDENCE", C_DIM(0.6f));
    ImGui::Spacing();

    for(auto& clue : g_state.clues()){
        if(clue.discovered){
            ImVec2 p2 = ImGui::GetCursorScreenPos();
            float  rw = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p2.x-2, p2.y}, {p2.x+rw+2, p2.y+20},
                IM_COL32(50,230,150,12), 2.f);
            ImGui::PushStyleColor(ImGuiCol_Text, C_GREEN(0.85f));
            ImGui::TextWrapped("  ✓  %s", clue.title.c_str());
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,
                    ImVec4(0.012f,0.022f,0.050f,0.98f));
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(280.f);
                ImGui::TextColored(C_GREEN(), "%s", clue.title.c_str());
                ImGui::Spacing();
                ImGui::TextColored(C_TEXT(0.72f), "%s",
                    clue.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, C_DIM(0.20f));
            ImGui::Text("  ○  [UNSOLVED]");
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered()){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,
                    ImVec4(0.012f,0.022f,0.050f,0.98f));
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(280.f);
                ImGui::TextColored(C_DIM(0.5f), "Hint:");
                ImGui::Spacing();
                ImGui::TextColored(C_DIM(0.45f), "%s", clue.hint.c_str());
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
//  SCHEMA OVERLAY
// ─────────────────────────────────────────────────────────────────────────────
void draw_schema(){
    if(!g_state.app().show_schema) return;

    float sw = 420.f, sh = g_H - TOP_H - 20.f;
    float lw = floorf(g_W * LEFT_W_F);
    ImGui::SetNextWindowPos({lw + 10, TOP_H + 10});
    ImGui::SetNextWindowSize({sw, sh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,  ImVec4(0.010f,0.020f,0.045f,0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,    C_NEON(0.40f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    {16.f,12.f});
    ImGui::Begin("##schema", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    ImGui::TextColored(C_NEON(), "DATABASE SCHEMA");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.3f), "- NovaCorp Internal Systems");
    ImGui::SameLine(sw - 44.f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f,.3f,.3f,.2f));
    ImGui::PushStyleColor(ImGuiCol_Text,          C_DIM(0.55f));
    if(ImGui::Button("✕##sclose")) g_state.app().show_schema = false;
    ImGui::PopStyleColor(3);
    ImGui::Spacing(); neon_sep(0.25f); ImGui::Spacing();

    ImGui::BeginChild("##sc2", {-1,-1});
    for(auto& t : g_state.tables()){
        if(!t.unlocked){
            ImGui::PushStyleColor(ImGuiCol_Text, C_DIM(0.22f));
            ImGui::Text("  %s  [LOCKED]", t.name.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            continue;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, C_NEON(0.9f));
        bool open = ImGui::TreeNodeEx(t.name.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen, "%s", t.name.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(C_DIM(0.3f), "(%d rows)", t.row_count);
        if(open){
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.f);
            for(auto& col : t.columns){
                ImGui::TextColored(C_DIM(0.45f), "    %-22s", col.name.c_str());
                ImGui::SameLine(0,0);
                ImGui::TextColored(C_DIM(0.25f), "%s", col.type.c_str());
                if(!col.note.empty()){
                    ImGui::SameLine(0,6);
                    ImGui::TextColored(C_PURPLE(0.5f), "%s", col.note.c_str());
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
void draw_accuse_modal(){
    auto& app = g_state.app();
    if(!app.accuse.active || app.status == GameStatus::CASE_SOLVED) return;

    float mw = 580.f, mh = 300.f;
    ImGui::SetNextWindowPos({(g_W-mw)*0.5f, (g_H-mh)*0.5f});
    ImGui::SetNextWindowSize({mw, mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(0.015f,0.028f,0.060f,0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,
        C_RED(UITheme::pulse(2.f,0.5f,1.f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    {26.f,20.f});
    ImGui::Begin("##acc", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    // Header + X
    ImGui::TextColored(C_RED(), "MAKE YOUR ACCUSATION");
    ImGui::SameLine(mw - 58.f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f,.3f,.3f,.2f));
    ImGui::PushStyleColor(ImGuiCol_Text,          C_DIM(0.55f));
    if(ImGui::Button("✕##aclose")) app.accuse.active = false;
    ImGui::PopStyleColor(3);

    ImGui::Spacing(); neon_sep(0.20f); ImGui::Spacing();
    ImGui::PushTextWrapPos(mw - 52.f);
    ImGui::TextColored(C_TEXT(0.68f), "%s", 
        g_state.get_current_case().get_accusation_prompt().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // Wrong-answer feedback
    if(app.accuse.wrong_timer > 0.f){
        ImGui::PushTextWrapPos(mw - 52.f);
        ImGui::TextColored(C_RED(0.78f), "X  %s",
            app.accuse.wrong_feedback.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }

    ImGui::TextColored(C_DIM(0.5f), "Suspect:");
    ImGui::SameLine(0,10);

    const float charge_w = 90.f, cancel_w = 80.f, gap = 8.f;
    float input_w = mw - 52.f*2 - 10.f - 90.f - charge_w - cancel_w - gap*2 - 4.f;
    ImGui::SetNextItemWidth(input_w);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,  ImVec4(0.f,0.f,0.f,0.6f));
    ImGui::PushStyleColor(ImGuiCol_Border,   C_RED(0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    {10.f,7.f});

    if(s_accuse_focus){ ImGui::SetKeyboardFocusHere(); s_accuse_focus = false; }
    bool enter = ImGui::InputText("##ac", s_accuse_buf, sizeof(s_accuse_buf),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);

    ImGui::SameLine(0,gap);
    UITheme::push_danger_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.f,7.f});
    bool submit = (ImGui::Button("CHARGE", {charge_w,0}) || enter)
                  && strlen(s_accuse_buf) > 0;
    ImGui::PopStyleVar();
    UITheme::pop_danger_button();

    ImGui::SameLine(0,gap);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f,0.f,0.f,0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.3f,.3f,.3f,.2f));
    ImGui::PushStyleColor(ImGuiCol_Text,          C_DIM(0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    {0.f,7.f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    if(ImGui::Button("CANCEL", {cancel_w,0})) app.accuse.active = false;
    ImGui::PopStyleColor(3); ImGui::PopStyleVar(2);

    if(submit){
        Audio::get().play(SFX::ACCUSE, 1.0f);
        bool correct = g_state.try_accuse(s_accuse_buf);
        if(!correct){
            memset(s_accuse_buf, 0, sizeof(s_accuse_buf));
            s_accuse_focus = true;
            Audio::get().play(SFX::ERROR_BEEP, 0.8f);
        } else {
            Audio::get().play(SFX::SOLVED, 1.0f);
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CASE SOLVED OVERLAY
// ─────────────────────────────────────────────────────────────────────────────
void draw_solved(){
    if(g_state.app().status != GameStatus::CASE_SOLVED) return;
    float p  = UITheme::pulse(1.3f, 0.6f, 1.f);
    float mw = 640.f, mh = 460.f;
    ImGui::SetNextWindowPos({(g_W-mw)*0.5f, (g_H-mh)*0.5f});
    ImGui::SetNextWindowSize({mw, mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.015f,0.028f,0.060f,0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,   C_GREEN(p));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    {28.f,22.f});
    ImGui::Begin("##sol", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    ImGui::Spacing();
    centered("CASE CLOSED", C_GREEN(p));
    ImGui::SameLine(mw - 50.f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f,.6f,.3f,.2f));
    ImGui::PushStyleColor(ImGuiCol_Text,          C_DIM(0.45f));
    if(ImGui::Button("✕##solclose")) g_state.app().status = GameStatus::ACTIVE;
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    centered(g_state.get_current_case().get_resolved_title().c_str(), C_TEXT(0.85f));
    ImGui::Spacing(); neon_sep(0.14f); ImGui::Spacing();
    ImGui::PushTextWrapPos(mw - 56.f);
        ImGui::TextColored(C_DIM(0.55f), "CHARGED:");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_RED(0.90f), "%s", g_state.get_current_case().get_charged_name().c_str());
    ImGui::Spacing();
        if (!g_state.get_current_case().get_accessory_name().empty()) {
        ImGui::TextColored(C_DIM(0.42f), "ACCESSORY / CONSPIRACY:");
        ImGui::SameLine(0,8);
        ImGui::TextColored(C_RED(0.65f), "%s", g_state.get_current_case().get_accessory_name().c_str());
        ImGui::Spacing(); 
    }
    ImGui::Spacing(); neon_sep(0.10f); ImGui::Spacing();
    ImGui::TextColored(C_TEXT(0.68f), "%s", g_state.get_current_case().get_resolution_text().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    centered("Justice served.", C_GREEN(p));
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(.196f,.902f,.588f,.09f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.196f,.902f,.588f,.22f));
    ImGui::PushStyleColor(ImGuiCol_Text,          C_GREEN(p));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    {0.f,9.f});
    if(ImGui::Button("CLOSE FILE", {-1,0}))
        g_state.app().status = GameStatus::ACTIVE;
    ImGui::PopStyleColor(3); ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  HELP OVERLAY
// ─────────────────────────────────────────────────────────────────────────────
void draw_help(){
    if(!g_state.app().show_help) return;
    float mw = 560.f, mh = 500.f;
    ImGui::SetNextWindowPos({(g_W-mw)*0.5f, (g_H-mh)*0.5f});
    ImGui::SetNextWindowSize({mw, mh});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.015f,0.028f,0.060f,0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,   C_NEON(0.42f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    {22.f,18.f});
    ImGui::Begin("##hlp", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    ImGui::TextColored(C_NEON(), "HOW TO PLAY");
    ImGui::SameLine(mw - 50.f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f,.5f,.8f,.2f));
    ImGui::PushStyleColor(ImGuiCol_Text,          C_DIM(0.55f));
    if(ImGui::Button("✕##hclose")) g_state.app().show_help = false;
    ImGui::PopStyleColor(3);
    ImGui::Spacing(); neon_sep(0.22f); ImGui::Spacing();

    ImGui::PushTextWrapPos(mw - 44.f);
    ImGui::TextColored(C_TEXT(0.72f),
        "Write SQL queries to investigate NovaCorp's internal database. "
        "Evidence is only unlocked by writing specific, targeted queries. "
        "SELECT * from a table is rarely enough.\n");

    auto kv = [](const char* k, const char* v){
        ImGui::TextColored(C_NEON(0.62f), "%-20s", k);
        ImGui::SameLine(0,6);
        ImGui::TextColored(C_TEXT(0.65f), "%s", v);
    };
    kv("Enter / EXECUTE",  "Run query");
    kv("↑ / ↓",            "Query history");
    kv("SCHEMA",           "Toggle schema viewer");
    kv("ACCUSE",           "Appears when you have enough evidence");
    kv("Hover clue ○",     "See the hint for that clue");
    kv("Hover table name", "Preview column schema");

    ImGui::Spacing(); neon_sep(0.12f); ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.5f), "USEFUL SQL PATTERNS:");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, C_DIM(0.58f));
    ImGui::Text("  SELECT col1, col2 FROM table WHERE col = 'value';");
    ImGui::Text("  SELECT * FROM table WHERE col LIKE '%%keyword%%';");
    ImGui::Text("  SELECT * FROM table ORDER BY col DESC;");
    ImGui::Text("  SELECT a.col, b.col FROM a JOIN b ON a.id = b.id;");
    ImGui::Text("  SELECT from_acct, SUM(amount) FROM transactions");
    ImGui::Text("         GROUP BY from_acct;");
    ImGui::Text("  SELECT * FROM table WHERE col1 = 'x' AND col2 > 100;");
    ImGui::PopStyleColor();
    ImGui::Spacing(); neon_sep(0.12f); ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.5f), "HOW UNLOCKS WORK:");
    ImGui::Spacing();
    ImGui::TextColored(C_TEXT(0.62f),
        "New tables unlock when your query on the previous table returns "
        "meaningful results. You need to find the right data.");
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.5f), "TO SOLVE:");
    ImGui::Spacing();
    ImGui::TextColored(C_TEXT(0.62f), "%s", g_state.get_current_case().help_objective.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    UITheme::push_neon_button();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.f,9.f});
    if(ImGui::Button("CLOSE", {-1,0})) g_state.app().show_help = false;
    ImGui::PopStyleVar();
    UITheme::pop_neon_button();

    ImGui::End();
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NOTIFICATIONS (floating toasts)
// ─────────────────────────────────────────────────────────────────────────────
void draw_notifications(){
    float yoff = g_H - 18.f;
    for(auto& n : g_state.notifs()){
        float a = 1.f - (n.age / n.lifetime);
        if(a <= 0.f) continue;

        ImVec4 col; const char* icon;
        switch(n.type){
            case NotifType::CLUE:      col=C_GREEN(a);  icon="✓"; break;
            case NotifType::UNLOCK:    col=C_PURPLE(a); icon="◈"; break;
            case NotifType::ERROR_MSG: col=C_RED(a);    icon="!"; break;
            default:                   col=C_NEON(a);   icon="·"; break;
        }

        std::string msg = std::string(icon) + " " + n.message;
        ImVec2 sz  = ImGui::CalcTextSize(msg.c_str());
        float pw   = sz.x + 28.f;
        float ph   = 34.f;
        float px   = (g_W - pw) * 0.5f;
        float py   = yoff - ph;

        ImGui::SetNextWindowPos({px, py});
        ImGui::SetNextWindowSize({pw, ph});
        ImGui::SetNextWindowBgAlpha(0.92f * a);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
            ImVec4(0.01f,0.02f,0.05f,0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, col);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    {14.f,8.f});

        char wid[32];
        snprintf(wid, sizeof(wid), "##nt%p", (void*)&n);
        ImGui::Begin(wid, nullptr,
            ImGuiWindowFlags_NoDecoration  | ImGuiWindowFlags_NoMove  |
            ImGuiWindowFlags_NoResize      | ImGuiWindowFlags_NoNav   |
            ImGuiWindowFlags_NoInputs      |
            ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(col, "%s", msg.c_str());
        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        yoff -= ph + 5.f;
    }
}
