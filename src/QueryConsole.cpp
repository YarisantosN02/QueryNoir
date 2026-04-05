// ═══════════════════════════════════════════════════════════════════════════
//  QUERY NOIR — Query Console
//  Draws the center console (SQL input + execute button) and the results
//  table below it. Also owns the query input state shared with Renderer.
// ═══════════════════════════════════════════════════════════════════════════

#include "RenderCommon.h"
#include <cstring>
#include <algorithm>

// ─── Query input state — declared here, extern'd via RenderCommon.h ───────────
char         s_qbuf[4096]  = "";
int          s_hist_idx    = -1;
bool         s_focus_q     = true;
QueryResult s_result;
bool         s_has_result  = false;
int          s_prev_qbuf_len = 0;

// Cell expand popup state (private to this TU)
static std::string s_popup_cell;
static std::string s_last_query = "";
static std::vector<std::string> query_history;  
static bool show_history_placeholder = true; 

// ─────────────────────────────────────────────────────────────────────────────
void draw_center(){
    float lw = floorf(g_W * LEFT_W_F);
    float cx = lw;
    float cw = g_W - lw - RIGHT_W;
    float ch = g_H - TOP_H - RESULTS_H;

    // ── Console input area ────────────────────────────────────────────────────
    ImGui::SetNextWindowPos({cx, TOP_H});
    ImGui::SetNextWindowSize({cw, ch});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.014f,0.026f,0.055f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {18.f,14.f});
    ImGui::Begin("##con", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize     | ImGuiWindowFlags_NoScrollbar);

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        bg_dots(0.012f);
        float p2 = UITheme::pulse(1.9f, 0.4f, 1.f);
        dl->AddRectFilled({wp.x, wp.y}, {wp.x+ws.x, wp.y+2.f},
            IM_COL32(0,212,255,(int)(42*p2)));
    }

    ImGui::Spacing();
    float p = UITheme::pulse(1.7f, 0.55f, 1.f);
    ImGui::TextColored(C_NEON(p), "// QUERY CONSOLE");
    ImGui::SameLine();
    ImGui::TextColored(C_DIM(0.28f),
        "-- NovaCorp Internal DB  |  SQLite3  |  Type SCHEMA to inspect tables");
    ImGui::Spacing();
    neon_sep(0.16f);
    ImGui::Spacing();

    // Retrieve the current case ID from the data struct
    std::string current_id = g_state.get_current_case().id; 
    ImGui::TextColored(C_NEON(0.82f), "noir@forensics");
    ImGui::SameLine(0,0); ImGui::TextColored(C_DIM(0.4f),  ":");
    ImGui::SameLine(0,0); ImGui::TextColored(C_PURPLE(0.72f), "~/%s", current_id.c_str());
    ImGui::SameLine(0,0); ImGui::TextColored(C_DIM(0.55f), "$ ");
    ImGui::SameLine(0,6);


    // History navigation callback
    auto hcb = [](ImGuiInputTextCallbackData* d) -> int {
        extern std::vector<std::string> query_history;  // Our history
        if(query_history.empty()) return 0;
        if(d->EventKey == ImGuiKey_UpArrow)
            s_hist_idx = std::min(s_hist_idx+1, (int)query_history.size()-1);
        else if(d->EventKey == ImGuiKey_DownArrow)
            s_hist_idx = std::max(s_hist_idx-1, -1);
        if(s_hist_idx >= 0){
            int i = (int)query_history.size()-1-s_hist_idx;
            d->DeleteChars(0, d->BufTextLen);
            d->InsertChars(0, query_history[i].c_str());
        } else {
            d->DeleteChars(0, d->BufTextLen);
        }
        return 0;
    };


    // ── INPUT + EXECUTE + HISTORY LAYOUT ─────────────────────────────────────
    const float input_section_width = ImGui::GetContentRegionAvail().x;
    const float btn_w = 108.f;
    
    ImGui::BeginGroup();
    
    // INPUT (left - full width minus button)
    float iw = input_section_width - btn_w - 20.f;
    ImGui::SetNextItemWidth(iw);
    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.005f,0.010f,0.028f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f,0.831f,1.f,0.05f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f,0.831f,1.f,0.09f));
    ImGui::PushStyleColor(ImGuiCol_Border, g_state.app().query_executing ? C_NEON(0.9f) : C_NEON(0.28f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.f,7.f});
    
    if(s_focus_q){ ImGui::SetKeyboardFocusHere(); s_focus_q = false; }
    bool enter = ImGui::InputText("##qi", s_qbuf, sizeof(s_qbuf),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, hcb);
    
    ImGui::PopStyleColor(4); ImGui::PopStyleVar(2);
    
    // EXECUTE (right)
    ImGui::SameLine();
    bool exec_click = false;
    {
        float ep = UITheme::pulse(2.2f, 0.65f, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f,0.831f,1.f,0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f,0.831f,1.f,0.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f,0.831f,1.f,0.40f));
        ImGui::PushStyleColor(ImGuiCol_Text, C_NEON(ep));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.f,7.f});
        exec_click = ImGui::Button("EXECUTE ▶", {btn_w, 0});
        ImGui::PopStyleColor(4); ImGui::PopStyleVar(2);
    }
    ImGui::EndGroup();
    
    // HISTORY BELOW INPUT (perfect alignment)
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    
    float history_height = ImGui::GetContentRegionAvail().y * 0.90f; 
    history_height = std::max(history_height, 140.0f);  
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.015f, 0.025f, 0.045f, 0.6f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, C_NEON(0.25f));
    
    if(ImGui::BeginChild("##query_history", ImVec2(0, history_height), true)) {
        ImGui::TextColored(C_PURPLE(0.75f), "QUERY HISTORY (%zu)", query_history.size());
        ImGui::Spacing();
        if(query_history.empty()) {
            ImGui::TextColored(C_DIM(0.4f), "No queries executed yet...");
        } else {
            for(int i = (int)query_history.size() - 1; i >= 0; i--) {
                char label[1024];
                std::string display = query_history[i];
                if(display.length() > 85) display = display.substr(0, 82) + "...";
                snprintf(label, sizeof(label), "▸ %s##h%d", display.c_str(), i);
                if(ImGui::Selectable(label)) {
                    strncpy(s_qbuf, query_history[i].c_str(), sizeof(s_qbuf)-1);
                    s_qbuf[sizeof(s_qbuf)-1] = '\0';
                    s_focus_q = true;
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

//_______________________________________________________________________________________________________________________

    // Run the query
    if((enter || exec_click) && strlen(s_qbuf) > 0){
        g_state.app().glitch_timer = 0.18f;
        Audio::get().play(SFX::EXECUTE);
        Audio::get().play(SFX::GLITCH, 0.4f);
        
        // ADD TO OUR LOCAL HISTORY 
        query_history.push_back(s_qbuf);
        show_history_placeholder = false;  // Hide placeholder
        
        s_result = g_state.run_query(s_qbuf);
        s_has_result = true;
        s_hist_idx = -1;
        if(s_result.is_error) Audio::get().play(SFX::ERROR_BEEP);
    }

    // Key click on every typed character
    {
        int cur_len = (int)strlen(s_qbuf);
        if(cur_len != s_prev_qbuf_len){
            Audio::get().play(SFX::KEYCLICK, 0.45f);
            s_prev_qbuf_len = cur_len;
        }
    }

    
    // Hover tooltip for full query
    if(ImGui::IsItemHovered() && !s_last_query.empty()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(400);
        ImGui::TextUnformatted(s_last_query.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    // Executing indicator
    if(g_state.app().query_executing){
        ImGui::Spacing();
        float ep = UITheme::pulse(9.f, 0.3f, 1.f);
        ImGui::TextColored(C_NEON(ep), "  ◌  PROCESSING...");
    }

    ImGui::Spacing();
    neon_sep(0.08f);
    ImGui::Spacing();
    ImGui::TextColored(C_DIM(0.30f),
        "↑↓ history  ·  Enter execute  ·  SCHEMA inspects tables  ·  ACCUSE when ready");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ── Results panel ─────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos({cx, TOP_H + ch});
    ImGui::SetNextWindowSize({cw, RESULTS_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(0.010f,0.018f,0.040f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {18.f,10.f});
    ImGui::Begin("##res", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_HorizontalScrollbar);

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        dl->AddRectFilled({wp.x,wp.y},{wp.x+cw,wp.y+2},
            IM_COL32(0,212,255,38));
        bg_dots(0.009f);
    }

    // Header row
    ImGui::TextColored(C_DIM(0.42f), "OUTPUT");
    if(s_has_result && !s_result.is_error){
        ImGui::SameLine(0,10); ImGui::TextColored(C_DIM(0.25f), "--");
        ImGui::SameLine(0,8);
        ImGui::TextColored(C_GREEN(0.62f), "%zu row%s",
            s_result.rows.size(), s_result.rows.size()==1?"":"s");
        if(!s_result.flagged_rows.empty()){
            ImGui::SameLine(0,10);
            ImGui::TextColored(C_RED(0.55f), "⚠ %zu anomalous",
                s_result.flagged_rows.size());
        }
    }
    ImGui::Spacing(); neon_sep(0.10f); ImGui::Spacing();

    float rh = RESULTS_H - 56.f;

    if(!s_has_result){
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rh*0.3f);
        centered("No query yet.", C_DIM(0.20f));

    } else if(s_result.is_error){
        ImGui::PushStyleColor(ImGuiCol_Text,    C_RED(0.85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.2f,.02f,.02f,.3f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.f,7.f});
        std::string em = "[ERROR]  " + s_result.error;
        ImGui::InputTextMultiline("##em", em.data(), em.size(),
            {-1, rh}, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);

    } else if(s_result.rows.empty()){
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rh*0.3f);
        centered("Query returned 0 rows.", C_DIM(0.28f));

    } else {
        int nc = (int)s_result.columns.size();
        if(nc > 0 && ImGui::BeginTable("##rt", nc,
            ImGuiTableFlags_Borders            |
            ImGuiTableFlags_RowBg              |
            ImGuiTableFlags_ScrollX            |
            ImGuiTableFlags_ScrollY            |
            ImGuiTableFlags_SizingFixedFit   |
            ImGuiTableFlags_NoHostExtendX,
            {-1, rh}))
        {
            for(auto& col : s_result.columns)
                ImGui::TableSetupColumn(col.c_str(),
                    ImGuiTableColumnFlags_WidthFixed, 130.f);
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,
                ImVec4(0.f,0.22f,0.36f,1.f));
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            auto& flg = s_result.flagged_rows;
            for(size_t ri = 0; ri < s_result.rows.size(); ri++){
                bool flag = std::find(flg.begin(),flg.end(),ri)!=flg.end();
                ImGui::TableNextRow();
                if(flag){
                    ImVec2 rm = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        rm, {rm.x+9999.f, rm.y+22.f},
                        IM_COL32(255,80,80,16));
                }
                for(size_t ci = 0; ci < s_result.rows[ri].size(); ci++){
                    ImGui::TableSetColumnIndex((int)ci);
                    const std::string& cell = s_result.rows[ri][ci];

                    // Truncate long cells; full content on click
                    bool long_cell = cell.size() > 24;
                    std::string display = long_cell
                        ? cell.substr(0,22) + "..."
                        : cell;

                    ImGui::PushStyleColor(ImGuiCol_Header,
                        ImVec4(0,0,0,0));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        ImVec4(0.f,0.831f,1.f,0.10f));
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        flag ? C_RED(0.82f) : C_TEXT(0.78f));

                    // Buffer size increased to 64 to avoid snprintf warning
                    char sel_id[64];
                    snprintf(sel_id, sizeof(sel_id), "##c%zu_%zu", ri, ci);
                    if(ImGui::Selectable((display + sel_id).c_str(),
                        false, ImGuiSelectableFlags_None, {130.f,0.f}))
                    {
                        s_popup_cell = cell;
                        ImGui::OpenPopup("##cell_popup");
                    }
                    ImGui::PopStyleColor(3);

                    // Hover tooltip for truncated cells
                    if(long_cell && ImGui::IsItemHovered()){
                        ImGui::PushStyleColor(ImGuiCol_PopupBg,
                            ImVec4(0.010f,0.020f,0.045f,0.98f));
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(400.f);
                        ImGui::TextColored(C_TEXT(0.85f), "%s", cell.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::TextColored(C_DIM(0.35f),
                            "[click to open full view]");
                        ImGui::EndTooltip();
                        ImGui::PopStyleColor();
                    }
                }
            }

            // Cell expand popup
            if(ImGui::BeginPopup("##cell_popup")){
                ImGui::PushStyleColor(ImGuiCol_PopupBg,
                    ImVec4(0.010f,0.018f,0.040f,0.99f));
                float popup_w = std::min(g_W*0.55f, 700.f);
                ImGui::SetNextWindowSize({popup_w, 0});
                ImGui::TextColored(C_NEON(0.7f), "CELL CONTENTS");
                ImGui::SameLine(popup_w - 60.f);
                ImGui::PushStyleColor(ImGuiCol_Button,          ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f,.3f,.3f,.2f));
                ImGui::PushStyleColor(ImGuiCol_Text,           C_DIM(0.5f));
                if(ImGui::Button("✕##cpclose")) ImGui::CloseCurrentPopup();
                ImGui::PopStyleColor(3);
                ImGui::Separator(); ImGui::Spacing();
                ImGui::PushTextWrapPos(popup_w - 20.f);
                ImGui::TextColored(C_TEXT(0.88f), "%s", s_popup_cell.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Spacing();
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}