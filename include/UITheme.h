#pragma once
#include "imgui.h"

namespace UITheme {
    void apply_cyber_noir();
    void push_neon_button();
    void pop_neon_button();
    void push_danger_button();
    void pop_danger_button();

    // Fonts (loaded in main)
    extern ImFont* font_mono;
    extern ImFont* font_mono_lg;
    extern ImFont* font_mono_sm;
    extern ImFont* font_title;

    // Animated helpers
    float pulse(float speed = 2.0f, float min = 0.6f, float max = 1.0f);
    ImVec4 lerp_color(ImVec4 a, ImVec4 b, float t);
}
