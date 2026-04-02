#include "UITheme.h"
#include <cmath>
#include <chrono>

namespace UITheme {

ImFont* font_mono    = nullptr;
ImFont* font_mono_lg = nullptr;
ImFont* font_mono_sm = nullptr;
ImFont* font_title   = nullptr;

float pulse(float speed, float mn, float mx) {
    using namespace std::chrono;
    auto now = duration<double>(system_clock::now().time_since_epoch()).count();
    float t = (float)(0.5 + 0.5 * std::sin(now * speed));
    return mn + t * (mx - mn);
}

ImVec4 lerp_color(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

void apply_cyber_noir() {
    ImGuiStyle& s = ImGui::GetStyle();

    // ── Layout ──────────────────────────────────────────────────────────────
    s.WindowPadding     = { 14.f, 14.f };
    s.FramePadding      = {  8.f,  5.f };
    s.ItemSpacing       = {  8.f,  6.f };
    s.ItemInnerSpacing  = {  6.f,  4.f };
    s.ScrollbarSize     = 10.f;
    s.GrabMinSize       = 8.f;
    s.WindowBorderSize  = 1.f;
    s.FrameBorderSize   = 1.f;
    s.PopupBorderSize   = 1.f;
    s.WindowRounding    = 4.f;
    s.FrameRounding     = 3.f;
    s.ScrollbarRounding = 3.f;
    s.GrabRounding      = 3.f;
    s.TabRounding       = 3.f;

    ImVec4* c = s.Colors;

    auto bg       = ImVec4(0.039f, 0.059f, 0.110f, 1.0f);
    auto bg2      = ImVec4(0.055f, 0.082f, 0.149f, 0.97f);
    auto bg3      = ImVec4(0.070f, 0.105f, 0.188f, 1.0f);
    auto neon     = ImVec4(0.000f, 0.831f, 1.000f, 1.0f);
    auto neon_dim = ImVec4(0.000f, 0.831f, 1.000f, 0.35f);
    auto purple   = ImVec4(0.478f, 0.361f, 1.000f, 1.0f);
    (void)ImVec4(1.000f, 0.302f, 0.302f, 1.0f); // red — reserved
    auto text     = ImVec4(0.867f, 0.933f, 1.000f, 1.0f);
    auto text_dim = ImVec4(0.502f, 0.651f, 0.800f, 1.0f);
    auto border   = ImVec4(0.000f, 0.831f, 1.000f, 0.20f);

    c[ImGuiCol_WindowBg]             = bg;
    c[ImGuiCol_ChildBg]              = bg2;
    c[ImGuiCol_PopupBg]              = bg2;
    c[ImGuiCol_Border]               = border;
    c[ImGuiCol_BorderShadow]         = { 0,0,0,0 };

    c[ImGuiCol_FrameBg]              = bg3;
    c[ImGuiCol_FrameBgHovered]       = { 0.f, 0.831f, 1.f, 0.08f };
    c[ImGuiCol_FrameBgActive]        = { 0.f, 0.831f, 1.f, 0.15f };

    c[ImGuiCol_TitleBg]              = bg;
    c[ImGuiCol_TitleBgActive]        = { 0.f, 0.15f, 0.25f, 1.f };
    c[ImGuiCol_TitleBgCollapsed]     = bg;
    c[ImGuiCol_MenuBarBg]            = bg;

    c[ImGuiCol_ScrollbarBg]          = bg2;
    c[ImGuiCol_ScrollbarGrab]        = neon_dim;
    c[ImGuiCol_ScrollbarGrabHovered] = neon;
    c[ImGuiCol_ScrollbarGrabActive]  = neon;

    c[ImGuiCol_CheckMark]            = neon;
    c[ImGuiCol_SliderGrab]           = neon;
    c[ImGuiCol_SliderGrabActive]     = purple;

    c[ImGuiCol_Button]               = { 0.f, 0.831f, 1.f, 0.12f };
    c[ImGuiCol_ButtonHovered]        = { 0.f, 0.831f, 1.f, 0.25f };
    c[ImGuiCol_ButtonActive]         = { 0.f, 0.831f, 1.f, 0.40f };

    c[ImGuiCol_Header]               = { 0.f, 0.831f, 1.f, 0.10f };
    c[ImGuiCol_HeaderHovered]        = { 0.f, 0.831f, 1.f, 0.20f };
    c[ImGuiCol_HeaderActive]         = { 0.f, 0.831f, 1.f, 0.30f };

    c[ImGuiCol_Separator]            = border;
    c[ImGuiCol_SeparatorHovered]     = neon;
    c[ImGuiCol_SeparatorActive]      = neon;

    c[ImGuiCol_ResizeGrip]           = neon_dim;
    c[ImGuiCol_ResizeGripHovered]    = neon;
    c[ImGuiCol_ResizeGripActive]     = neon;

    c[ImGuiCol_Tab]                  = { 0.f, 0.3f, 0.5f, 0.3f };
    c[ImGuiCol_TabHovered]           = { 0.f, 0.831f, 1.f, 0.4f };
    c[ImGuiCol_TabActive]            = { 0.f, 0.831f, 1.f, 0.6f };
    c[ImGuiCol_TabUnfocused]         = { 0.f, 0.2f,  0.3f, 0.3f };
    c[ImGuiCol_TabUnfocusedActive]   = { 0.f, 0.5f,  0.7f, 0.4f };

    c[ImGuiCol_PlotLines]            = neon;
    c[ImGuiCol_PlotLinesHovered]     = purple;
    c[ImGuiCol_PlotHistogram]        = neon;
    c[ImGuiCol_PlotHistogramHovered] = purple;

    c[ImGuiCol_TableHeaderBg]        = { 0.f, 0.2f, 0.35f, 1.f };
    c[ImGuiCol_TableBorderStrong]    = border;
    c[ImGuiCol_TableBorderLight]     = { 0.f, 0.5f, 0.8f, 0.08f };
    c[ImGuiCol_TableRowBg]           = { 0.f, 0.f, 0.f, 0.f };
    c[ImGuiCol_TableRowBgAlt]        = { 0.f, 0.831f, 1.f, 0.03f };

    c[ImGuiCol_TextSelectedBg]       = { 0.f, 0.831f, 1.f, 0.25f };
    c[ImGuiCol_DragDropTarget]       = neon;

    c[ImGuiCol_NavHighlight]         = neon;
    c[ImGuiCol_NavWindowingHighlight]= { 1,1,1,0.7f };
    c[ImGuiCol_NavWindowingDimBg]    = { 0.1f,0.1f,0.1f,0.5f };
    c[ImGuiCol_ModalWindowDimBg]     = { 0.f, 0.f, 0.f, 0.6f };

    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = text_dim;
}

void push_neon_button() {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f, 0.831f, 1.f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.831f, 1.f, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.f, 0.831f, 1.f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.f, 0.831f, 1.f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
}

void pop_neon_button() {
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(1);
}

void push_danger_button() {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.f, 0.3f, 0.3f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 0.3f, 0.3f, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.f, 0.3f, 0.3f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
}

void pop_danger_button() {
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(1);
}

} // namespace UITheme
