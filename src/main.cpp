// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Entry Point
//  Responsibilities: SDL2 init, ImGui setup, font loading, main game loop.
//  All rendering is delegated to Renderer.cpp, QueryConsole.cpp,
//  NarrativeFeed.cpp. Game logic lives in GameState.cpp.
// ═══════════════════════════════════════════════════════════════════════════

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include <SDL.h>

#include "RenderCommon.h"
#include "AudioManager.h"
#include "CaseManager.h"

#include <algorithm>
#include <cstring>

// ─── Window dimensions — written here, read via extern in all render files ───
float g_W = 1440.f;
float g_H = 900.f;

// ─── Global game state ────────────────────────────────────────────────────────
GameState g_state;

// ─── Intro state ──────────────────────────────────────────────────────────────
IntroState g_intro;
bool       g_intro_done = false;

// ─────────────────────────────────────────────────────────────────────────────
int main(int, char**){
    // ── SDL2 ─────────────────────────────────────────────────────────────────
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0){
        SDL_Log("SDL_Init: %s", SDL_GetError()); return 1;
    }

    SDL_Window* win = SDL_CreateWindow(
        "QUERY NOIR -- Forensics Terminal",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1440, 900,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if(!win){ SDL_Log("Window: %s", SDL_GetError()); return 1; }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if(!ren){ SDL_Log("Renderer: %s", SDL_GetError()); return 1; }

    // HiDPI scale
    {
        int rw, rh, ww, wh;
        SDL_GetRendererOutputSize(ren, &rw, &rh);
        SDL_GetWindowSize(win, &ww, &wh);
        if(rw > ww) SDL_RenderSetScale(ren, (float)rw/ww, (float)rh/wh);
    }

    // ── ImGui ─────────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr; // no imgui.ini

    UITheme::apply_cyber_noir();
    ImGui_ImplSDL2_InitForSDLRenderer(win, ren);
    ImGui_ImplSDLRenderer2_Init(ren);

    // ── Fonts ─────────────────────────────────────────────────────────────────
    {
        auto try_font = [&](const char* path, float sz) -> ImFont* {
            ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 1;
            return io.Fonts->AddFontFromFileTTF(path, sz, &cfg);
        };
        const char* rp[] = {
            "assets/fonts/JetBrainsMono-Regular.ttf",
            "../assets/fonts/JetBrainsMono-Regular.ttf", nullptr };
        const char* bp[] = {
            "assets/fonts/JetBrainsMono-Bold.ttf",
            "../assets/fonts/JetBrainsMono-Bold.ttf", nullptr };
        for(int i=0; rp[i]; i++){
            UITheme::font_mono = try_font(rp[i], 14.f);
            if(UITheme::font_mono) break;
        }
        if(!UITheme::font_mono) UITheme::font_mono = io.Fonts->AddFontDefault();
        for(int i=0; bp[i]; i++){
            UITheme::font_mono_lg = try_font(bp[i], 17.f);
            if(UITheme::font_mono_lg) break;
        }
        if(!UITheme::font_mono_lg) UITheme::font_mono_lg = io.Fonts->AddFontDefault();
        io.Fonts->Build();
    }

    // ── Game + Audio init ─────────────────────────────────────────────────────
    CaseManager::init(g_state);
    AudioManager::init(g_state);

    // ── Main loop ─────────────────────────────────────────────────────────────
    Uint64 last    = SDL_GetPerformanceCounter();
    bool   running = true;

    while(running){
        // Events
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if(ev.type == SDL_QUIT) running = false;
            if(ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE){
                if(g_state.app().accuse.active)
                    g_state.app().accuse.active = false;
                else
                    running = false;
            }
        }

        // Update window size every frame (handles resize + HiDPI)
        {
            int ww, wh;
            SDL_GetWindowSize(win, &ww, &wh);
            g_W = (float)ww;
            g_H = (float)wh;
        }

        // Delta time
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = std::min(
            (float)(now - last) / SDL_GetPerformanceFrequency(), 0.05f);
        last = now;

        g_state.update(dt);

        // Begin ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if(UITheme::font_mono) ImGui::PushFont(UITheme::font_mono);

        SDL_SetRenderDrawColor(ren, 9, 14, 26, 255);
        SDL_RenderClear(ren);

        // ── Intro sequence — blocks game UI until complete ────────────────────
        if(!g_intro_done){
            g_intro_done = draw_intro(g_intro, dt, g_W, g_H, nullptr);
            if(g_intro_done) AudioManager::on_intro_complete();
            if(UITheme::font_mono) ImGui::PopFont();
            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), ren);
            SDL_RenderPresent(ren);
            continue;
        }

        // ── Game rendering ────────────────────────────────────────────────────

        // Full-screen scanline overlay (behind all panels)
        {
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize({g_W,g_H});
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::Begin("##bg", nullptr,
                ImGuiWindowFlags_NoDecoration     | ImGuiWindowFlags_NoMove  |
                ImGuiWindowFlags_NoResize         | ImGuiWindowFlags_NoNav   |
                ImGuiWindowFlags_NoInputs         |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoBackground);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for(float y = 0; y < g_H; y += 3.f)
                dl->AddLine({0,y},{g_W,y}, IM_COL32(0,212,255,3));
            ImGui::End();
        }

        // Panels
        draw_top_bar();
        draw_left_panel();
        draw_center();
        draw_right_panel();

        // Overlays
        draw_schema();
        draw_accuse_modal();
        draw_notifications();
        draw_solved();
        draw_help();

        if(UITheme::font_mono) ImGui::PopFont();

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), ren);
        SDL_RenderPresent(ren);
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    AudioManager::shutdown();
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
