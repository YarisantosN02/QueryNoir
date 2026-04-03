// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Narrative Feed
//  Draws the right-panel detective monologue feed with typewriter animation.
// ═══════════════════════════════════════════════════════════════════════════

#include "RenderCommon.h"
#include <algorithm>

void draw_right_panel(){
    float rx = g_W - RIGHT_W;
    ImGui::SetNextWindowPos({rx, TOP_H});
    ImGui::SetNextWindowSize({RIGHT_W, g_H - TOP_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(0.016f,0.030f,0.062f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {13.f,11.f});
    ImGui::Begin("##right", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize     | ImGuiWindowFlags_NoScrollbar);

    // Left border glow
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        dl->AddLine({wp.x+1, wp.y}, {wp.x+1, wp.y+ws.y},
            IM_COL32(0,212,255,70));
        bg_dots(0.016f);
    }

    ImGui::Spacing();
    float p = UITheme::pulse(1.4f, 0.55f, 1.f);
    ImGui::TextColored(C_NEON(p), "◈");
    ImGui::SameLine(0,8);
    ImGui::TextColored(C_TEXT(0.75f), "DETECTIVE FEED");
    ImGui::Spacing();
    neon_sep(0.16f);
    ImGui::Spacing();

    // Scrollable feed area
    float feed_h = g_H - TOP_H - 62.f;
    ImGui::BeginChild("##fc", {-1, feed_h}, false,
        ImGuiWindowFlags_NoScrollbar);

    for(auto& e : g_state.feed()){
        // Choose color and prefix symbol by narrative type
        ImVec4 col; const char* pfx;
        switch(e.type){
            case NarrativeType::MONOLOGUE: col=C_TEXT(0.75f);   pfx=">  "; break;
            case NarrativeType::HINT:      col=C_NEON(0.80f);   pfx="?  "; break;
            case NarrativeType::ALERT:     col=C_RED(0.85f);    pfx="!  "; break;
            case NarrativeType::SUCCESS:   col=C_GREEN(0.90f);  pfx="✓  "; break;
            case NarrativeType::SYSTEM:    col=C_PURPLE(0.70f); pfx="#  "; break;
            default:                       col=C_DIM(0.55f);    pfx="   "; break;
        }

        // Accent dot beside each entry
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                {cp.x+3, cp.y+7}, 2.5f, U32(col), 8);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 11);

        // Typewriter: reveal chars progressively until fully revealed
        int chars = e.revealed
            ? (int)e.text.size()
            : std::min((int)e.reveal_timer, (int)e.text.size());

        ImGui::PushTextWrapPos(
            ImGui::GetCursorPosX() + RIGHT_W - 24.f);
        ImGui::TextColored(col, "%s%s",
            pfx, e.text.substr(0, chars).c_str());
        ImGui::PopTextWrapPos();

        // Blinking cursor on the actively-revealing entry
        if(!e.revealed){
            ImGui::SameLine(0,0);
            if(UITheme::pulse(6.f, 0.f, 1.f) > 0.5f)
                ImGui::TextColored(col, "▌");
        }

        ImGui::Spacing();
    }

    ImGui::EndChild();

    neon_sep(0.08f);
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.25f),
        "↑↓ history  ·  Enter run  ·  SCHEMA cmd");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
