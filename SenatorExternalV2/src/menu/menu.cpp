#include "menu.h"
#include <Offsets/Offsets.hpp>
#include <branding/branding.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>

#include <imgui/misc/imgui_freetype.h>

#include "../../render/textures/texture.h"

#include "../../../ext/imgui/addons/imgui_addons.h"
#include "../../../ext/imgui/texteditor.hpp"

#include <settings.h>
#include <cache/cache.h>
#include <gamesupport/gamesupport.h>
#include <gamesupport/LumberTycoon2/lt2.h>
#include <menu/keybind/keybind.h>
#include <features/explorer/explorer.h>
#include <features/explorer/globals.h>
#include <features/config/config.h>
#include <features/esp/preview/visuals_preview.h>
#include <features/lighting/skybox/skybox.h>
#include <cache/custom_entities/custom_entities.h>
#include <memory/memory.h>
#include <game/game.h>
#include <Offsets/Offsets.hpp>
#include <features/aimbot/aimbot.h>
#include "../../ext/font/config/font_config.h"
#include "../../ext/font/tahoma.h"
#include "../../ext/font/sp7.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cfloat>
#include <unordered_map>
#include <unordered_set>

extern ImFont* esp_font;
extern ImFont* esp_font_tahoma;
extern ImFont* esp_font_sp7;
extern ImFont* esp_font_arial;

namespace helper
{
    void draw_text_outlined(ImDrawList* draw, ImFont* font, float font_size, ImVec2 pos, ImU32 col, const char* text_begin, const char* text_end = nullptr);
    void corner_box(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float thickness = 1.f);
}

#define PROJECT_NAME_LEFT    "Senator"
#define PROJECT_NAME_RIGHT   "External"

bool Menu::Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext)
{
    bool result = true;

    IMGUI_CHECKVERSION();
    if (!ImGui::GetCurrentContext())
    {
        ImGui::CreateContext();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    style.WindowRounding = 0;
    style.ChildRounding = 0;
    style.FrameRounding = 0;
    style.PopupRounding = 0;
    style.GrabRounding = 0;
    style.ScrollbarRounding = 0;

    style.WindowBorderSize = 1;
    style.FrameBorderSize = 1;
    style.PopupBorderSize = 1;

    style.WindowPadding = ImVec2(9, 9);
    style.ChildPadding = ImVec2(7, 7);
    style.FramePadding = ImVec2(5.0f, 4.5f);
    style.CellPadding = ImVec2(2, 2); // checkbox padding
    style.ItemSpacing = ImVec2(7, 7);
    style.ItemInnerSpacing = ImVec2(5, 6);
    style.WindowMinSize = ImVec2(0, 0);

    style.ScrollbarSize = 6.0f;

    style.Colors[ImGuiCol_WindowBg] = ImAdd::HexToColorVec4(0x1c1c1c);
    style.Colors[ImGuiCol_ChildBg] = ImAdd::HexToColorVec4(0x1c1c1c);
    style.Colors[ImGuiCol_PopupBg] = ImAdd::HexToColorVec4(0x181818);

    style.Colors[ImGuiCol_Text] = ImAdd::HexToColorVec4(0xffffff);
    style.Colors[ImGuiCol_TextDisabled] = ImAdd::HexToColorVec4(0x848484);

    style.Colors[ImGuiCol_Border] = ImAdd::HexToColorVec4(0x000000);
    style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];

    style.Colors[ImGuiCol_Header] = ImAdd::HexToColorVec4(0x544563);
    style.Colors[ImGuiCol_HeaderHovered] = ImAdd::HexToColorVec4(0x665875);
    style.Colors[ImGuiCol_HeaderActive] = ImAdd::HexToColorVec4(0x3c2d49);

    style.Colors[ImGuiCol_SliderGrab] = style.Colors[ImGuiCol_Header];
    style.Colors[ImGuiCol_SliderGrabActive] = style.Colors[ImGuiCol_HeaderActive];

    style.Colors[ImGuiCol_Button] = ImAdd::HexToColorVec4(0x232323);
    style.Colors[ImGuiCol_ButtonHovered] = ImAdd::HexToColorVec4(0x252525);
    style.Colors[ImGuiCol_ButtonActive] = ImAdd::HexToColorVec4(0x212121);

    style.Colors[ImGuiCol_FrameBg] = ImAdd::HexToColorVec4(0x232323);
    style.Colors[ImGuiCol_FrameBgHovered] = ImAdd::HexToColorVec4(0x252525);
    style.Colors[ImGuiCol_FrameBgActive] = ImAdd::HexToColorVec4(0x212121);

    style.Colors[ImGuiCol_Tab] = ImAdd::HexToColorVec4(0x1b1b1b);
    style.Colors[ImGuiCol_TabHovered] = ImAdd::HexToColorVec4(0x1c1c1c);
    style.Colors[ImGuiCol_TabActive] = ImAdd::HexToColorVec4(0x1a1a1a);

    style.Colors[ImGuiCol_FrameBgShadow] = ImAdd::HexToColorVec4(0x000000, 0.5f);
    style.Colors[ImGuiCol_ButtonShadow] = ImAdd::HexToColorVec4(0xffffff, 0.035f);

    ImFontConfig font_cfg_main;
    font_cfg_main.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_ForceAutoHint;
    font_cfg_main.GlyphOffset = ImVec2(0, 1);
    font_cfg_main.SizePixels = 12.0f;
    font_cfg_main.GlyphExtraAdvanceX = 1.0f;
    io.Fonts->AddFontDefault(&font_cfg_main);

    result = ImGui_ImplWin32_Init(hWnd);
    if (!result) return false;

    result = ImGui_ImplDX11_Init(pDevice, pDeviceContext);
    if (!result) return false;

    m_bInitialized = true;
    return true;
}


void Menu::DrawWatermark()
{
    if (!settings::menu::watermark)
        return;

    ImGuiStyle& style = ImGui::GetStyle();

    // Build watermark text from enabled elements
    static const char* separators[] = { " | ", " - ", " / ", " :: ", "  ", " \xC2\xB7 " };
    const char* sep = separators[std::clamp(settings::watermark::separator_type, 0, 5)];

    std::string wm_text;
    auto append = [&](const std::string& s) {
        if (s.empty()) return;
        if (!wm_text.empty()) wm_text += sep;
        wm_text += s;
    };

    if (settings::watermark::show_cheat_name)
        append(external_config::cheat_name);

    if (settings::watermark::show_game_name)
    {
        const std::string game_name = game::get_active_game_display_name();
        if (!game_name.empty())
            append("Game: " + game_name);
    }

    if (settings::watermark::show_display_name || settings::watermark::show_username)
    {
        std::string player_str;
        if (cache::cached_local_player.instance.address != 0)
        {
            if (settings::watermark::show_display_name && !cache::cached_local_player.display_name.empty())
                player_str = cache::cached_local_player.display_name;
            if (settings::watermark::show_username && !cache::cached_local_player.name.empty())
            {
                if (!player_str.empty())
                    player_str += " (@" + cache::cached_local_player.name + ")";
                else
                    player_str = "@" + cache::cached_local_player.name;
            }
        }
        append(player_str);
    }

    if (settings::watermark::show_fps)
        append(std::to_string(static_cast<int>(ImGui::GetIO().Framerate)) + " fps");

    if (settings::watermark::show_server_ip)
    {
        try
        {
            if (game::datamodel.address)
            {
                std::string ip = memory->read_string(game::datamodel.address + Offsets::DataModel::ServerIP);
                if (!ip.empty() && ip != "Unknown")
                {
                    // Replace | separator with :
                    for (auto& c : ip)
                        if (c == '|') c = ':';
                    append(ip);
                }
            }
        }
        catch (...) {}
    }

    if (wm_text.empty()) wm_text = external_config::cheat_name;

    // Calculate color
    ImU32 text_col;
    if (settings::watermark::rainbow)
    {
        float t = static_cast<float>(ImGui::GetTime()) * settings::watermark::rainbow_speed;
        float r = sinf(t) * 0.5f + 0.5f;
        float g = sinf(t + 2.094f) * 0.5f + 0.5f;
        float b = sinf(t + 4.189f) * 0.5f + 0.5f;
        text_col = IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255), 255);
    }
    else
    {
        text_col = ImGui::ColorConvertFloat4ToU32({
            settings::watermark::text_color[0],
            settings::watermark::text_color[1],
            settings::watermark::text_color[2],
            settings::watermark::text_color[3]
        });
    }

    float text_width = ImGui::CalcTextSize(wm_text.c_str()).x;
    float wm_width = text_width + style.FramePadding.x * 2.0f + style.CellPadding.x * 2.0f;
    float wm_height = ImGui::GetFrameHeight() + style.CellPadding.y * 2.0f + style.WindowBorderSize * 5.0f;

    // Draggable when menu is open, fixed when closed
    ImGuiWindowFlags wm_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav;
    if (!m_bMenuVisible)
        wm_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowSize(ImVec2(wm_width, wm_height), ImGuiCond_Always);
    if (!settings::watermark::pos_initialized)
    {
        ImGui::SetNextWindowPos(ImVec2(settings::watermark::pos_x, settings::watermark::pos_y), ImGuiCond_Always);
        settings::watermark::pos_initialized = true;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    bool wm_open = ImGui::Begin("##Watermark", (bool*)0, wm_flags);
    ImGui::PopStyleVar(2);

    if (wm_open)
    {
        // Save position for next launch
        ImVec2 pos = ImGui::GetWindowPos();
        settings::watermark::pos_x = pos.x;
        settings::watermark::pos_y = pos.y;

        ImRect window_bb(ImGui::GetCurrentWindow()->Rect());
        ImGuiWindow* window = ImGui::GetCurrentWindow();

        // Background
        window->DrawList->AddRectFilled(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_WindowBg));

        // Border + accent lines
        if (style.WindowBorderSize > 0.0f)
        {
            window->DrawList->AddRect(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);
            window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize, 0.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);

            if (settings::watermark::rainbow)
            {
                float t = static_cast<float>(ImGui::GetTime()) * settings::watermark::rainbow_speed;
                ImU32 c1 = IM_COL32(static_cast<int>((sinf(t) * 0.5f + 0.5f) * 255), static_cast<int>((sinf(t + 2.094f) * 0.5f + 0.5f) * 255), static_cast<int>((sinf(t + 4.189f) * 0.5f + 0.5f) * 255), 255);
                ImU32 c2 = IM_COL32(static_cast<int>((sinf(t + 1.0f) * 0.5f + 0.5f) * 255), static_cast<int>((sinf(t + 3.094f) * 0.5f + 0.5f) * 255), static_cast<int>((sinf(t + 5.189f) * 0.5f + 0.5f) * 255), 255);
                window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize * 1.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 1.0f), c1, style.WindowBorderSize);
                window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize * 2.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 2.0f), c2, style.WindowBorderSize);
            }
            else
            {
                window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize * 1.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 1.0f), ImGui::GetColorU32(ImGuiCol_Header), style.WindowBorderSize);
                window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize * 2.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 2.0f), ImGui::GetColorU32(ImGuiCol_HeaderActive), style.WindowBorderSize);
            }

            window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize * 3.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 3.0f), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
            window->DrawList->AddLine(ImVec2(window_bb.Min.x + style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
        }

        // Text
        ImVec2 text_pos = window_bb.Min + ImVec2(style.FramePadding.x + style.CellPadding.x, style.CellPadding.y + style.WindowBorderSize * 4.0f);

        // Outline
        window->DrawList->AddText(text_pos + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), wm_text.c_str());
        window->DrawList->AddText(text_pos + ImVec2(-1, -1), IM_COL32(0, 0, 0, 200), wm_text.c_str());
        window->DrawList->AddText(text_pos + ImVec2(1, -1), IM_COL32(0, 0, 0, 200), wm_text.c_str());
        window->DrawList->AddText(text_pos + ImVec2(-1, 1), IM_COL32(0, 0, 0, 200), wm_text.c_str());
        // Main text
        window->DrawList->AddText(text_pos, text_col, wm_text.c_str());
    }
    ImGui::End();
}

void Menu::DrawMenu()
{
    static bool m_bMainWindowOpen = true;
    static bool m_bExplorerWindowOpen = false;
    static bool m_bPlayerListOpen = false;
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, ImGui::GetFrameHeight() + style.CellPadding.y * 2.0f + style.WindowBorderSize * 5.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    bool menubar_window = ImGui::Begin("paste", (bool*)0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize);
    ImGui::PopStyleVar(2);

    if (menubar_window)
    {
        ImRect window_bb(ImGui::GetCurrentWindow()->Rect());

        if (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_NoBackground)
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();

            window->DrawList->AddRectFilled(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_WindowBg));

            if (style.WindowBorderSize > 0.0f)
            {

                window->DrawList->AddRect(window_bb.Min - ImVec2(style.WindowBorderSize, 0.0f), window_bb.Max + ImVec2(style.WindowBorderSize, 0.0f), ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);

                window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize, 0.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
                window->DrawList->AddLine(ImVec2(window_bb.Min.x + style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize * 4.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize * 4.0f), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
                window->DrawList->AddLine(ImVec2(window_bb.Min.x + style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize * 3.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize * 3.0f), ImGui::GetColorU32(ImGuiCol_Header), style.WindowBorderSize);
                window->DrawList->AddLine(ImVec2(window_bb.Min.x + style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize * 2.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize * 2.0f), ImGui::GetColorU32(ImGuiCol_HeaderActive), style.WindowBorderSize);
                window->DrawList->AddLine(ImVec2(window_bb.Min.x + style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Max.y - style.WindowBorderSize), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
            }
        }

        ImGui::SetCursorScreenPos(window_bb.Min + ImVec2(0.0f, style.ChildBorderSize));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.CellPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.CellPadding);
        if (ImGui::BeginChild("body", ImVec2(window_bb.GetWidth(), ImGui::GetFrameHeight() + style.CellPadding.y * 2.0f), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground))
        {

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, style.FramePadding.y));
            const float logo_width =
                ImGui::CalcTextSize(PROJECT_NAME_LEFT).x +
                ImGui::CalcTextSize(PROJECT_NAME_RIGHT).x +
                6.0f;
            if (ImGui::BeginChild("logo", ImVec2(logo_width, ImGui::GetFrameHeight()), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground))
            {
                const ImVec2 logo_pos = ImGui::GetCursorScreenPos();
                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->AddText(logo_pos, branding::accent_u32(), PROJECT_NAME_LEFT);
                draw->AddText(logo_pos + ImVec2(ImGui::CalcTextSize(PROJECT_NAME_LEFT).x + 4.0f, 0.0f), IM_COL32(245, 245, 248, 255), PROJECT_NAME_RIGHT);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::SameLine();
            ImAdd::VSeparator(style.CellPadding.y);
            ImGui::SameLine();

            if (ImAdd::ButtonAccent("Main"))
                m_bMainWindowOpen = !m_bMainWindowOpen;
            m_bMenuVisible = m_bMainWindowOpen;
            ImGui::SameLine();
            if (ImAdd::ButtonAccent("Explorer"))
                m_bExplorerWindowOpen = !m_bExplorerWindowOpen;
            ImGui::SameLine();
            if (ImAdd::ButtonAccent("Players"))
                m_bPlayerListOpen = !m_bPlayerListOpen;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }
    ImGui::End();

    if (m_bMainWindowOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(620, 700), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        bool main_window = ImGui::Begin("ImMagic - Menu", &m_bMainWindowOpen, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize);
        ImGui::PopStyleVar(2);

        if (main_window)
        {
            ImRect window_bb(ImGui::GetCurrentWindow()->Rect());

            if (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_NoBackground)
            {
                ImGuiWindow* window = ImGui::GetCurrentWindow();

                window->DrawList->AddRectFilled(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_WindowBg));

                if (style.WindowBorderSize > 0.0f)
                {
                    window->DrawList->AddRect(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);
                    window->DrawList->AddRect(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize), window_bb.Max - ImVec2(style.WindowBorderSize, style.WindowBorderSize), ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);

                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 2.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize * 2.0f, window_bb.Min.y + style.WindowBorderSize * 2.0f), ImGui::GetColorU32(ImGuiCol_Header), style.WindowBorderSize);
                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 3.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize * 2.0f, window_bb.Min.y + style.WindowBorderSize * 3.0f), ImGui::GetColorU32(ImGuiCol_HeaderActive), style.WindowBorderSize);
                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 4.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize * 2.0f, window_bb.Min.y + style.WindowBorderSize * 4.0f), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
                }

                const ImVec2 title_pos = window_bb.Min + ImVec2(style.FramePadding.x + style.WindowBorderSize * 3.0f, style.FramePadding.y + style.WindowBorderSize * 4.0f);
                window->DrawList->AddText(title_pos, branding::accent_u32(), PROJECT_NAME_LEFT);
                window->DrawList->AddText(title_pos + ImVec2(ImGui::CalcTextSize(PROJECT_NAME_LEFT).x + 5.0f, 0.0f), IM_COL32(245, 245, 248, 255), PROJECT_NAME_RIGHT);
            }

            static int tab_index = 0;

            ImGui::SetCursorScreenPos(window_bb.Min + ImVec2(style.WindowPadding.x, ImGui::GetFrameHeight() + style.WindowBorderSize * 3.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.WindowPadding);

            ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, style.WindowPadding);
            std::vector<const char*> tab_names;
            if (game::is_lumber_tycoon_2)
                tab_names = { "Visuals", "Rage", "LT2", "Settings" };
            else if (game::is_phantom_forces || game::is_murder_mystery_2)
                tab_names = { "Aimbot", "Visuals", "Rage", "Settings" };
            else if (game::is_blade_ball)
                tab_names = { "Aimbot", "Silent Aim", "Visuals", "Rage", "Blade Ball", "Settings" };
            else
                tab_names = { "Aimbot", "Silent Aim", "Visuals", "Rage", "Settings" };

            bool main_content_child = ImAdd::BeginChild("body", tab_names, &tab_index, ImGui::GetContentRegionAvail() - style.WindowPadding);
            ImGui::PopStyleVar(2);

            if (main_content_child)
            {
                float group_width = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2);
                float group_height = ImTrunc((ImGui::GetContentRegionAvail().y - style.ItemSpacing.y) / 2);

                // Remap tab_index based on game mode
                int logical_tab = tab_index;
                if (game::is_lumber_tycoon_2) {
                    // LT2 tabs: { "Visuals", "Rage", "LT2", "Settings" } -> logical 2, 3, 5, 4
                    if (tab_index == 0) logical_tab = 2;
                    else if (tab_index == 1) logical_tab = 3;
                    else if (tab_index == 2) logical_tab = 5;
                    else if (tab_index == 3) logical_tab = 4;
                } else if (game::is_blade_ball) {
                    // Blade Ball tabs: { "Aimbot", "Silent Aim", "Visuals", "Rage", "Blade Ball", "Settings" } -> logical 0, 1, 2, 3, 6, 4
                    if (tab_index == 4) logical_tab = 6;
                    else if (tab_index == 5) logical_tab = 4;
                } else if ((game::is_phantom_forces || game::is_murder_mystery_2) && tab_index >= 1) {
                    logical_tab = tab_index + 1; // skip silent aim index
                }

                if (logical_tab == 0)
                {
                    ImGui::BeginGroup();
                    static int aimbot_subtab = 0;
                    if (ImAdd::BeginChild("Aimbot", { "Main", "Settings" }, &aimbot_subtab, ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        if (aimbot_subtab == 0)
                        {
                            ImAdd::CheckBox("Enable", &settings::aimbot::enabled);
                            ImGui::SameLine();
                            static ImGuiKey aimbot_key = ImGuiKey_None;
                            aimbot_key = keybind::vk_to_imgui_key(settings::aimbot::keybind);
                            if (ImAdd::KeyBind("## Aimbot Keybind", &aimbot_key, ImVec2(0, 0), &settings::aimbot::activation_mode))
                            {
                                settings::aimbot::keybind = keybind::imgui_key_to_vk(aimbot_key);
                            }

                            ImAdd::CheckBox("Sticky Aim", &settings::aimbot::sticky_aim);

                            ImAdd::CheckBox("Draw FOV", &settings::aimbot::draw_fov);
                            if (settings::aimbot::draw_fov)
                            {
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() * 2.0f - style.ItemSpacing.x + style.ChildPadding.x);
                                ImAdd::ColorEdit4("## fov circle color", settings::aimbot::fov_circle_colour);
                                ImGui::SameLine();
                                ImAdd::ColorEdit4("## fov outline color", settings::aimbot::fov_outline_colour);
                            }

                            ImAdd::SliderFloat("FOV", &settings::aimbot::fov, 0.0f, 1000.0f, "%.0f");

                            ImGui::Text("Mode");
                            if (game::is_phantom_forces)
                            {
                                ImAdd::Combo("## Aimbot Mode", &settings::aimbot::mode, { "Mouse", "Camera", "Silent Aim" });
                            }
                            else
                            {
                                if (settings::aimbot::mode >= 2)
                                    settings::aimbot::mode = 1;  // PF-only modes not available
                                ImAdd::Combo("## Aimbot Mode", &settings::aimbot::mode, { "Mouse", "Camera" });
                            }

                            if (settings::aimbot::mode == 2)
                            {
                                ImGui::Separator();
                                ImGui::Text("Silent Aim Settings");
                                ImAdd::SliderFloat("Head Offset (Y)", &settings::aimbot::ai_silent_y_offset, -1.0f, 2.0f, "%.2f studs");
                                ImGui::Separator();
                            }
                        }
                        else if (aimbot_subtab == 1)
                        {
                            ImGui::Text("Target Part");
                            ImAdd::Combo("## Target Part", &settings::aimbot::target_part, { "Closest", "Head", "HumanoidRootPart", "LeftArm", "RightArm", "LeftLeg", "RightLeg" });

                            ImAdd::CheckBox("Air Part", &settings::aimbot::air_part_enabled);
                            if (settings::aimbot::air_part_enabled)
                            {
                                ImGui::Text("Air Part");
                                ImAdd::Combo("## Air Part", &settings::aimbot::air_part, { "Closest", "Head", "HumanoidRootPart", "LeftArm", "RightArm", "LeftLeg", "RightLeg" });
                            }

                            ImAdd::CheckBox("Enable Smoothing", &settings::aimbot::smoothing);
                            if (settings::aimbot::smoothing)
                            {
                                ImAdd::SliderFloat("Smoothing X", &settings::aimbot::smoothingx, 1.0f, 100.0f, "%.1f");
                                ImAdd::SliderFloat("Smoothing Y", &settings::aimbot::smoothingy, 1.0f, 100.0f, "%.1f");
                                ImGui::Text("Smoothing Style");
                                ImAdd::Combo("## Smoothing Style", &settings::aimbot::smoothing_style, { "None", "Linear", "EaseInQuad", "EaseOutQuad", "EaseInOutQuad", "EaseInCubic", "EaseOutCubic", "EaseInOutCubic", "EaseInSine", "EaseOutSine", "EaseInOutSine" });
                            }

                            ImAdd::CheckBox("Enable Prediction", &settings::aimbot::enable_prediction);
                            if (settings::aimbot::enable_prediction)
                            {
                                ImAdd::SliderFloat("Prediction X", &settings::aimbot::prediction_x, 0.0f, 20.0f, "%.1f");
                                ImAdd::SliderFloat("Prediction Y", &settings::aimbot::prediction_y, 0.0f, 20.0f, "%.1f");
                            }

                            ImAdd::CheckBox("Air Prediction", &settings::aimbot::air_prediction_enabled);
                            if (settings::aimbot::air_prediction_enabled)
                            {
                                ImAdd::SliderFloat("Air Prediction X", &settings::aimbot::air_prediction_x, 0.0f, 20.0f, "%.1f");
                                ImAdd::SliderFloat("Air Prediction Y", &settings::aimbot::air_prediction_y, 0.0f, 20.0f, "%.1f");
                            }

                            ImAdd::CheckBox("Offset", &settings::aimbot::offset_enabled);
                            if (settings::aimbot::offset_enabled)
                            {
                                ImAdd::SliderFloat("Offset X", &settings::aimbot::offset_x, -100.0f, 100.0f, "%.1f");
                                ImAdd::SliderFloat("Offset Y", &settings::aimbot::offset_y, -100.0f, 100.0f, "%.1f");
                            }
                        }

                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine(0, style.ItemSpacing.x);

                    ImGui::BeginGroup();
                    if (ImAdd::BeginChild("Aimbot Settings", ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        ImAdd::CheckBox("Use FOV", &settings::aimbot::use_fov);
                        ImAdd::CheckBox("Team Check", &settings::aimbot::teamcheck);
                        ImAdd::CheckBox("Knock Check", &settings::aimbot::knock_check);
                        ImAdd::CheckBox("Health Check", &settings::aimbot::health_check_enabled);
                        if (settings::aimbot::health_check_enabled)
                        {
                            ImAdd::SliderFloat("Min Health", &settings::aimbot::min_health, 0.0f, 100.0f, "%.1f");
                        }

                        ImAdd::CheckBox("Triggerbot [UNIVERSAL]", &settings::aimbot::triggerbot::enabled);
                        if (settings::aimbot::triggerbot::enabled)
                        {
                            ImGui::SameLine();
                            static ImGuiKey ab_tb_key = ImGuiKey_None;
                            ab_tb_key = keybind::vk_to_imgui_key(settings::aimbot::triggerbot::keybind);
                            if (ImAdd::KeyBind("## AB TB Keybind", &ab_tb_key, ImVec2(0, 0), &settings::aimbot::triggerbot::activation_mode))
                            {
                                settings::aimbot::triggerbot::keybind = keybind::imgui_key_to_vk(ab_tb_key);
                            }
                            ImAdd::Combo("Fire Mode##ab_tb", &settings::aimbot::triggerbot::fire_mode, { "Click", "Hold" });
                            if (settings::aimbot::triggerbot::fire_mode == 0)
                            {
                                ImAdd::SliderFloat("CPS##ab_tb", &settings::aimbot::triggerbot::clicks_per_second, 1.0f, 30.0f, "%.1f");
                            }
                            else
                            {
                                ImAdd::SliderFloat("Hold Duration##ab_tb", &settings::aimbot::triggerbot::hold_duration, 0.01f, 1.0f, "%.2f s");
                            }
                            ImAdd::SliderFloat("Reaction (ms)##ab_tb", &settings::aimbot::triggerbot::reaction_ms, 0.0f, 500.0f, "%.0f ms");
                            ImAdd::CheckBox("Max Distance##ab_tb", &settings::aimbot::triggerbot::max_distance_enabled);
                            if (settings::aimbot::triggerbot::max_distance_enabled)
                            {
                                ImAdd::SliderFloat("Distance##ab_tb", &settings::aimbot::triggerbot::max_distance, 1.0f, 2000.0f, "%.0f studs");
                            }
                            ImAdd::CheckBox("Wall Check##ab_tb", &settings::aimbot::triggerbot::wallcheck);
                        }

                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();
                }
                else if (logical_tab == 1)
                {
                    ImGui::BeginGroup();
                    static int silentaim_subtab = 0;
                    if (ImAdd::BeginChild("Silent Aim", { "Main", "Settings" }, &silentaim_subtab, ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        if (silentaim_subtab == 0)
                        {
                            ImAdd::CheckBox("Enable", &settings::silentaim::enabled);
                            ImGui::SameLine();
                            static ImGuiKey silentaim_key = ImGuiKey_None;
                            silentaim_key = keybind::vk_to_imgui_key(settings::silentaim::keybind);
                            if (ImAdd::KeyBind("## Silent Aim Keybind", &silentaim_key, ImVec2(0, 0), &settings::silentaim::activation_mode))
                            {
                                settings::silentaim::keybind = keybind::imgui_key_to_vk(silentaim_key);
                            }

                            ImAdd::CheckBox("Sticky Aim", &settings::silentaim::sticky_aim);

                            ImAdd::CheckBox("Draw FOV", &settings::silentaim::draw_fov);
                            if (settings::silentaim::draw_fov)
                            {
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() * 2.0f - style.ItemSpacing.x + style.ChildPadding.x);
                                ImAdd::ColorEdit4("## sa fov circle color", settings::silentaim::fov_circle_colour);
                                ImGui::SameLine();
                                ImAdd::ColorEdit4("## sa fov outline color", settings::silentaim::fov_outline_colour);
                                ImAdd::CheckBox("Attach FOV to Target", &settings::silentaim::attach_fov_to_target);
                            }

                            ImAdd::SliderFloat("FOV", &settings::silentaim::fov, 0.0f, 1000.0f, "%.0f");
                        }
                        else if (silentaim_subtab == 1)
                        {
                            ImGui::Text("Target Part");
                            ImAdd::Combo("## SA Target Part", &settings::silentaim::target_part, { "Nearest Point", "Closest", "Head", "HumanoidRootPart", "LeftArm", "RightArm", "LeftLeg", "RightLeg" });

                            ImAdd::CheckBox("Enable Prediction", &settings::silentaim::enable_prediction);
                            if (settings::silentaim::enable_prediction)
                            {
                                ImAdd::SliderFloat("Prediction X", &settings::silentaim::prediction_x, 0.0f, 20.0f, "%.1f");
                                ImAdd::SliderFloat("Prediction Y", &settings::silentaim::prediction_y, 0.0f, 20.0f, "%.1f");
                            }

                            ImAdd::CheckBox("Hit Chance", &settings::silentaim::hitchance_enabled);
                            if (settings::silentaim::hitchance_enabled)
                            {
                                ImAdd::SliderFloat("Chance %##sa_hc", &settings::silentaim::hitchance, 1.0f, 100.0f, "%.0f%%");
                            }
                        }

                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine(0, style.ItemSpacing.x);

                    ImGui::BeginGroup();
                    if (ImAdd::BeginChild("Silent Aim Settings", ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        ImAdd::CheckBox("Use Aimbot Target", &settings::silentaim::use_aimbot_target);
                        ImAdd::CheckBox("Use FOV", &settings::silentaim::use_fov);
                        ImAdd::CheckBox("Team Check", &settings::silentaim::teamcheck);
                        ImAdd::CheckBox("Gun Check", &settings::silentaim::guncheck);
                        ImAdd::CheckBox("Knock Check", &settings::silentaim::knock_check);
                        ImAdd::CheckBox("Health Check", &settings::silentaim::health_check_enabled);
                        if (settings::silentaim::health_check_enabled)
                        {
                            ImAdd::SliderFloat("Min Health", &settings::silentaim::min_health, 0.0f, 100.0f, "%.1f");
                        }

                        ImAdd::CheckBox("Draw Target Dot", &settings::silentaim::draw_target_dot);
                        if (settings::silentaim::draw_target_dot)
                        {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() - style.ItemSpacing.x + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## target dot color", settings::silentaim::target_dot_color);
                        }

                        ImAdd::CheckBox("Triggerbot", &settings::silentaim::triggerbot::enabled);
                        if (settings::silentaim::triggerbot::enabled)
                        {
                            ImGui::SameLine();
                            static ImGuiKey sa_tb_key = ImGuiKey_None;
                            sa_tb_key = keybind::vk_to_imgui_key(settings::silentaim::triggerbot::keybind);
                            if (ImAdd::KeyBind("## SA TB Keybind", &sa_tb_key, ImVec2(0, 0), &settings::silentaim::triggerbot::activation_mode))
                            {
                                settings::silentaim::triggerbot::keybind = keybind::imgui_key_to_vk(sa_tb_key);
                            }
                            ImAdd::Combo("Fire Mode##sa_tb", &settings::silentaim::triggerbot::fire_mode, { "Click", "Hold" });
                            if (settings::silentaim::triggerbot::fire_mode == 0)
                            {
                                ImAdd::SliderFloat("CPS##sa_tb", &settings::silentaim::triggerbot::clicks_per_second, 1.0f, 30.0f, "%.1f");
                            }
                            else
                            {
                                ImAdd::SliderFloat("Hold Duration##sa_tb", &settings::silentaim::triggerbot::hold_duration, 0.01f, 1.0f, "%.2f s");
                            }
                            ImAdd::SliderFloat("Reaction (ms)##sa_tb", &settings::silentaim::triggerbot::reaction_ms, 0.0f, 500.0f, "%.0f ms");
                            ImAdd::CheckBox("Max Distance##sa_tb", &settings::silentaim::triggerbot::max_distance_enabled);
                            if (settings::silentaim::triggerbot::max_distance_enabled)
                            {
                                ImAdd::SliderFloat("Distance##sa_tb", &settings::silentaim::triggerbot::max_distance, 1.0f, 2000.0f, "%.0f studs");
                            }
                            ImAdd::CheckBox("Wall Check##sa_tb", &settings::silentaim::triggerbot::wallcheck);
                        }

                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();
                }
                else if (logical_tab == 2)
                {
                    float group_width = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2);

                    static int enemy_client_settings_tab = 0;

                    ImGui::BeginGroup();

                    if (ImAdd::BeginChild("group1", { "Enemy", "Client", "Settings" }, &enemy_client_settings_tab, ImVec2(group_width, 0.0f)))
                    {
                        if (enemy_client_settings_tab == 0)
                        {
                            if (game::is_murder_mystery_2)
                            {
                                ImGui::Text("MM2 ESP");
                                ImAdd::CheckBox("Show Roles", &settings::visuals::mm2_esp);
                                if (settings::visuals::mm2_esp)
                                {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.15f, 0.15f, 1.0f));
                                    ImGui::Text("Red = Murderer");
                                    ImGui::PopStyleColor();
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.4f, 1.0f, 1.0f));
                                    ImGui::Text("Blue = Sheriff");
                                    ImGui::PopStyleColor();
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                                    ImGui::Text("Gray = Innocent");
                                    ImGui::PopStyleColor();
                                }
                                ImGui::Separator();
                            }

                            ImAdd::CheckBox("Enable Enemies", &settings::visuals::enable_enemies);
                            ImAdd::CheckBox("Team Check", &settings::visuals::teamcheck);

                            ImAdd::CheckBox("Box", &settings::visuals::box);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## box color", settings::visuals::box_color);

                            ImAdd::CheckBox("Box Fill", &settings::visuals::box_fill);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## box fill color", settings::visuals::box_fill_color);

                            ImAdd::CheckBox("Skeleton", &settings::visuals::skeleton);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## skeleton color", settings::visuals::skeleton_color);

                            ImAdd::CheckBox("Name", &settings::visuals::name);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## name color", settings::visuals::name_color);

                            ImAdd::CheckBox("Avatar", &settings::visuals::avatar);

                            ImAdd::CheckBox("Healthbar", &settings::visuals::healthbar);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## healthbar color", settings::visuals::healthbar_color);

                            ImAdd::CheckBox("Health Percent", &settings::visuals::health_percent);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## health percent color", settings::visuals::health_percent_color);

                            ImAdd::CheckBox("Armor Bar", &settings::visuals::armorbar);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## armorbar color", settings::visuals::armorbar_color);

                            ImAdd::CheckBox("Distance", &settings::visuals::distance);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## distance color", settings::visuals::distance_color);

                            ImAdd::CheckBox("Tool", &settings::visuals::tool);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## tool color", settings::visuals::tool_color);

                            ImAdd::CheckBox("Flags", &settings::visuals::flags);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## flags state color", settings::visuals::flags_state_colour);

                            ImAdd::CheckBox("Chams", &settings::visuals::chams);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() * 2 - style.ItemSpacing.x + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## chams fill color", settings::visuals::chams_fill_color);
                            ImGui::SameLine();
                            ImAdd::ColorEdit4("Outline Color", settings::visuals::chams_outline_color);

                            ImAdd::CheckBox("Target Warning Icon", &settings::visuals::target_warning_icon);
                        }
                        else if (enemy_client_settings_tab == 1)
                        {
                            ImAdd::CheckBox("Enable Client", &settings::visuals::enable_client);

                            ImAdd::CheckBox("Box", &settings::visuals::client_box);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local box color", settings::visuals::client_box_color);

                            ImAdd::CheckBox("Box Fill", &settings::visuals::client_box_fill);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local box fill color", settings::visuals::client_box_fill_color);

                            ImAdd::CheckBox("Skeleton", &settings::visuals::client_skeleton);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local skeleton color", settings::visuals::client_skeleton_color);

                            ImAdd::CheckBox("Name", &settings::visuals::client_name);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local name color", settings::visuals::client_name_color);

                            ImAdd::CheckBox("Avatar", &settings::visuals::client_avatar);

                            ImAdd::CheckBox("Healthbar", &settings::visuals::client_healthbar);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local healthbar color", settings::visuals::client_healthbar_color);

                            ImAdd::CheckBox("Health Percent", &settings::visuals::client_health_percent);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local health percent color", settings::visuals::client_health_percent_color);

                            ImAdd::CheckBox("Armor Bar", &settings::visuals::client_armorbar);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local armorbar color", settings::visuals::client_armorbar_color);

                            ImAdd::CheckBox("Distance", &settings::visuals::client_distance);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local distance color", settings::visuals::client_distance_color);

                            ImAdd::CheckBox("Tool", &settings::visuals::client_tool);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local tool color", settings::visuals::client_tool_color);

                            ImAdd::CheckBox("Flags", &settings::visuals::client_flags);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local flags state color", settings::visuals::client_flags_state_colour);

                            ImAdd::CheckBox("Chams", &settings::visuals::client_chams);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() * 2 - style.ItemSpacing.x + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## local chams fill color", settings::visuals::client_chams_fill_color);
                            ImGui::SameLine();
                            ImAdd::ColorEdit4("## local chams outline color", settings::visuals::client_chams_outline_color);

                            ImAdd::CheckBox("Headless", &settings::visuals::client_headless);
                            ImAdd::CheckBox("Korblox", &settings::visuals::client_korblox);
                        }
                        else if (enemy_client_settings_tab == 2)
                        {
                            ImGui::Text("Font Type");
                            ImAdd::Combo("## Font Type", &settings::visuals::esp_font, { "Tahoma", "Smallest Pixel", "Arial" });

                            ImGui::Text("Box Type");
                            ImAdd::Combo("## Box Type", &settings::visuals::box_type, { "Bounding Box", "Cornered Box" });

                            ImGui::Text("Name Display Type");
                            ImAdd::Combo("## Name Display Type", &settings::visuals::name_display_type, { "Display Name", "Username" });

                            ImGui::Text("Distance Measurement");
                            ImAdd::Combo("## Distance Measurement", &settings::visuals::distance_measurement, { "Studs", "Meters" });

                            ImGui::Text("Chams Type");
                            ImAdd::Combo("## Chams Type", &settings::visuals::chams_type, { "Cube", "Highlight", "Mesh" });

                            ImAdd::CheckBox("Visuals 3D Preview", &settings::visuals::preview_3d);

                            ImAdd::CheckBox("Dynamic Healthbar", &settings::visuals::health_based_healthbar);

                            ImAdd::CheckBox("Blend", &settings::visuals::blend);
                            if (settings::visuals::blend)
                            {
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() * 2 - style.ItemSpacing.x + style.ChildPadding.x);
                                ImAdd::ColorEdit4("## colorblend start", settings::visuals::name_color_blend_start);
                                ImGui::SameLine();
                                ImAdd::ColorEdit4("## colorblend end", settings::visuals::name_color_blend_end);
                            }

                            ImAdd::CheckBox("Knock Check", &settings::visuals::knock_check);
                            ImAdd::CheckBox("Team Color", &settings::visuals::use_team_color);
                            ImAdd::CheckBox("Ignore Whitelisted", &settings::visuals::ignore_whitelisted);

                            ImAdd::CheckBox("Max Distance", &settings::visuals::max_distance_enabled);
                            if (settings::visuals::max_distance_enabled)
                            {
                                ImAdd::SliderFloat("Max Dist", &settings::visuals::max_distance, 1.0f, 5000.0f, "%.0f studs");
                                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                                ImGui::InputFloat("## max_dist_input", &settings::visuals::max_distance, 10.0f, 100.0f, "%.0f");
                                if (settings::visuals::max_distance < 1.0f) settings::visuals::max_distance = 1.0f;
                            }

                            ImAdd::CheckBox("Debug Wallcheck", &settings::visuals::debug_wallcheck);

                            ImAdd::CheckBox("Enable Radar", &settings::visuals::radar_enabled);
                            if (settings::visuals::radar_enabled)
                            {
                                ImAdd::SliderFloat("Radar Size", &settings::visuals::radar_size, 50.0f, 300.0f, "%.0f");
                            }

                            ImAdd::SliderFloat("Fade In Speed", &settings::visuals::fade_in_speed, 0.1f, 50.0f, "%.1f");
                            ImAdd::SliderFloat("Fade Out Speed", &settings::visuals::fade_out_speed, 0.1f, 50.0f, "%.1f");
                        }
                    }
                    ImAdd::EndChild();

                    ImGui::EndGroup();
                    ImGui::SameLine();
                    ImGui::BeginGroup();

                    if (ImAdd::BeginChild("group3", ImVec2(0.0f, 0.0f)))
                    {
                        ImAdd::CheckBox("Fog", &settings::lighting::fog::enabled);
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                        float fog_color[4] = { settings::lighting::fog::fog_r, settings::lighting::fog::fog_g, settings::lighting::fog::fog_b, 1.0f };
                        ImAdd::ColorEdit4("## fog color", fog_color);
                        settings::lighting::fog::fog_r = fog_color[0];
                        settings::lighting::fog::fog_g = fog_color[1];
                        settings::lighting::fog::fog_b = fog_color[2];

                        ImAdd::CheckBox("Disable Shadows", &settings::lighting::shadows::disable);

                        ImAdd::CheckBox("Exposure", &settings::lighting::exposure::enabled);

                        ImAdd::CheckBox("Clock Time", &settings::lighting::clocktime::enabled);

                        ImAdd::CheckBox("Skybox", &settings::lighting::skybox::enabled);

                        if (settings::lighting::skybox::enabled)
                        {
                            ImAdd::Combo("Preset##skybox", &settings::lighting::skybox::preset_index, lighting::skybox::preset_names());
                        }

                        if (settings::lighting::clocktime::enabled)
                        {
                            ImAdd::SliderFloat("Time", &settings::lighting::clocktime::clock_time, 0.0f, 24.0f, "%.1f");
                        }

                        if (settings::lighting::fog::enabled)
                        {
                            ImAdd::SliderFloat("Fog Start", &settings::lighting::fog::fog_start, 0.0f, 1000.0f, "%.1f");
                            ImAdd::SliderFloat("Fog End", &settings::lighting::fog::fog_end, 0.0f, 1000.0f, "%.1f");
                        }

                        if (settings::lighting::exposure::enabled)
                        {
                            ImAdd::SliderFloat("Exposure##lighting", &settings::lighting::exposure::exposure, -10.0f, 10.0f, "%.1f");
                        }

                        ImAdd::EndChild();
                    }

                    ImGui::EndGroup();

                    esp::preview::render_visuals_3d_window();
                }
                else if (logical_tab == 3)
                {
                    ImGui::BeginGroup();
                    if (ImAdd::BeginChild("Movement", ImVec2(group_width, group_height)))
                    {
                        ImAdd::CheckBox("Speed", &settings::movement::speedhack::enabled);
                        ImGui::SameLine();
                        static ImGuiKey speedhack_key = ImGuiKey_None;
                        speedhack_key = keybind::vk_to_imgui_key(settings::movement::speedhack::keybind);
                        if (ImAdd::KeyBind("##speedhack_keybind", &speedhack_key, ImVec2(0, 0), &settings::movement::speedhack::activation_mode))
                        {
                            settings::movement::speedhack::keybind = keybind::imgui_key_to_vk(speedhack_key);
                        }

                        ImAdd::CheckBox("Jump", &settings::movement::jumphack::enabled);
                        ImGui::SameLine();
                        static ImGuiKey jumppower_key = ImGuiKey_None;
                        jumppower_key = keybind::vk_to_imgui_key(settings::movement::jumphack::keybind);
                        if (ImAdd::KeyBind("##jumppower_keybind", &jumppower_key, ImVec2(0, 0), &settings::movement::jumphack::activation_mode))
                        {
                            settings::movement::jumphack::keybind = keybind::imgui_key_to_vk(jumppower_key);
                        }

                        ImAdd::CheckBox("Fly", &settings::movement::flyhack::enabled);
                        ImGui::SameLine();
                        static ImGuiKey flyhack_key = ImGuiKey_None;
                        flyhack_key = keybind::vk_to_imgui_key(settings::movement::flyhack::keybind);
                        if (ImAdd::KeyBind("##flyhack_keybind", &flyhack_key, ImVec2(0, 0), &settings::movement::flyhack::activation_mode))
                        {
                            settings::movement::flyhack::keybind = keybind::imgui_key_to_vk(flyhack_key);
                        }

                        ImAdd::CheckBox("Tickrate", &settings::movement::tickrate::enabled);
                        ImAdd::CheckBox("Noclip", &settings::rage::noclip);
                        ImAdd::CheckBox("Gravity", &settings::movement::gravity::enabled);
                        ImAdd::CheckBox("No Jump Cooldown", &settings::movement::nojumpcooldown::enabled);
                        ImAdd::CheckBox("Desync", &settings::desync::enabled);
                        ImGui::SameLine();
                        static ImGuiKey desync_key = ImGuiKey_None;
                        desync_key = keybind::vk_to_imgui_key(settings::desync::keybind);
                        if (ImAdd::KeyBind("## Desync Keybind", &desync_key, ImVec2(0, 0), &settings::desync::keybind_mode))
                        {
                            settings::desync::keybind = keybind::imgui_key_to_vk(desync_key);
                        }
                        ImAdd::CheckBox("Freeze Player", &settings::exploits::freezeplayer::enabled);
                        ImGui::SameLine();
                        static ImGuiKey freeze_key = ImGuiKey_None;
                        freeze_key = keybind::vk_to_imgui_key(settings::exploits::freezeplayer::keybind);
                        if (ImAdd::KeyBind("## Freeze Keybind", &freeze_key, ImVec2(0, 0), &settings::exploits::freezeplayer::activation_mode))
                        {
                            settings::exploits::freezeplayer::keybind = keybind::imgui_key_to_vk(freeze_key);
                        }
                        ImAdd::EndChild();
                    }

                    ImGui::SameLine();

                    if (ImAdd::BeginChild("Movement Settings", ImVec2(group_width, group_height)))
                    {
                        if (settings::movement::speedhack::enabled)
                        {
                            ImGui::Text("Speed Mode");
                            ImAdd::Combo("##speedhack_mode", &settings::movement::speedhack::mode, { "Velocity", "WalkSpeed" });
                            ImAdd::SliderFloat("Speed Speed", &settings::movement::speedhack::speed, 1.0f, 1000.0f, "%.1f");
                        }

                        if (settings::movement::jumphack::enabled)
                        {
                            ImAdd::SliderFloat("Jump", &settings::movement::jumphack::value, 50.0f, 300.0f, "%.1f");
                        }

                        if (settings::movement::gravity::enabled)
                        {
                            ImAdd::SliderFloat("Gravity", &settings::movement::gravity::value, 0.0f, 400.0f, "%.1f");
                        }

                        if (settings::movement::flyhack::enabled)
                        {
                            ImGui::Text("Fly Mode");
                            ImAdd::Combo("##flyhack_mode", &settings::movement::flyhack::mode, { "Velocity", "Position", "CFrame" });
                            ImAdd::SliderFloat("Fly Speed", &settings::movement::flyhack::speed, 1.0f, 1000.0f, "%.1f");
                        }

                        if (settings::movement::tickrate::enabled)
                        {
                            ImAdd::SliderFloat("Tickrate Value", &settings::movement::tickrate::value, 0.0f, 1000.0f, "%.1f");
                        }

                        if (settings::desync::enabled)
                        {
                            ImAdd::CheckBox("Visualizer", &settings::desync::visualizer::enabled);
                            if (settings::desync::visualizer::enabled)
                            {
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                                ImAdd::ColorEdit4("## desync viz color", settings::desync::visualizer::color);
                            }
                        }

                        ImAdd::EndChild();
                    }

                    ImGui::EndGroup();

                    ImGui::BeginGroup();
                    float big_child_width = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2);
                    float big_child_height = ImGui::GetContentRegionAvail().y;

                    if (ImAdd::BeginChild("Rage 1", ImVec2(big_child_width, big_child_height)))
                    {
                        ImAdd::CheckBox("HitSounds", &settings::rage::hitsounds);
                        ImAdd::CheckBox("Hit Tracers", &settings::rage::hit_tracers);
                        if (settings::rage::hit_tracers)
                        {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## hit tracers color", settings::rage::hit_tracers_color);
                        }
                        ImAdd::CheckBox("RapidFire", &settings::rage::rapidfire);
                        ImAdd::CheckBox("Hip Height", &settings::rage::hipheight::enabled);
                        ImAdd::CheckBox("3rd Person View", &settings::rage::thirdperson::enabled);
                        ImAdd::CheckBox("Orbit", &settings::movement::orbit::enabled);
                        ImAdd::CheckBox("Hitbox Expander", &settings::rage::hitbox_expander::enabled);
                        ImAdd::CheckBox("Spinbot", &settings::rage::spin360::enabled);
                        ImGui::SameLine();
                        static ImGuiKey spinbot_key = ImGuiKey_None;
                        spinbot_key = keybind::vk_to_imgui_key(settings::rage::spin360::keybind);
                        if (ImAdd::KeyBind("## Spinbot Keybind", &spinbot_key, ImVec2(0, 0), &settings::rage::spin360::activation_mode))
                        {
                            settings::rage::spin360::keybind = keybind::imgui_key_to_vk(spinbot_key);
                        }
                        ImAdd::CheckBox("Magic Bullet", &settings::magicbullet::enabled);
                        ImGui::SameLine();
                        static ImGuiKey mb_key = ImGuiKey_None;
                        mb_key = keybind::vk_to_imgui_key(settings::magicbullet::keybind);
                        if (ImAdd::KeyBind("## MagicBullet Keybind", &mb_key, ImVec2(0, 0), &settings::magicbullet::activation_mode))
                        {
                            settings::magicbullet::keybind = keybind::imgui_key_to_vk(mb_key);
                        }
                        ImAdd::CheckBox("Anti AFK", &settings::exploits::antiafk::enabled);
                        ImAdd::EndChild();
                    }

                    ImGui::SameLine();

                    if (ImAdd::BeginChild("Rage 2", ImVec2(big_child_width, big_child_height)))
                    {
                        if (settings::rage::hitsounds)
                        {
                            ImGui::Text("Detection Type");
                            ImAdd::Combo("##HitsoundMethod", &settings::rage::hitsound_method, { "Health", "Click", "Ammo" });
                            ImGui::Text("Hitsound Type");
                            ImAdd::Combo("##HitSound", &settings::rage::hitsound_type, { "Among Us", "Skeet", "Beep", "Bonk", "Bubble", "COD", "CSGO", "Fairy", "Fatality", "Osu", "Neverlose" });
                        }

                        if (settings::rage::hit_tracers)
                        {
                            ImGui::Text("Detection Type");
                            ImAdd::Combo("##HitTracersMethod", &settings::visuals::hit_tracers_method, { "Health", "Click", "Ammo" });
                            ImAdd::SliderFloat("Duration", &settings::rage::hit_tracers_duration, 0.1f, 5.0f, "%.1f");
                        }

                        if (settings::rage::hitbox_expander::enabled)
                        {
                            ImAdd::SliderFloat("Size X", &settings::rage::hitbox_expander::size_x, 0.1f, 30.0f, "%.1f");
                            ImAdd::SliderFloat("Size Y", &settings::rage::hitbox_expander::size_y, 0.1f, 30.0f, "%.1f");
                            ImAdd::SliderFloat("Size Z", &settings::rage::hitbox_expander::size_z, 0.1f, 30.0f, "%.1f");
                            ImAdd::CheckBox("Knock Check", &settings::rage::hitbox_expander::knock_check);
                            ImAdd::CheckBox("View Hitbox", &settings::visuals::view_hitbox);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                            ImAdd::ColorEdit4("## view hitbox color", settings::visuals::view_hitbox_color);
                        }

                        if (settings::rage::spin360::enabled)
                        {
                            ImAdd::SliderFloat("Spin Speed", &settings::rage::spin360::speed, 1.0f, 10.0f, "%.1f");
                        }

                        if (settings::magicbullet::enabled)
                        {
                            ImGui::Text("Target Source");
                            ImAdd::Combo("##MB Target", &settings::magicbullet::target_source, { "Silent Aim", "Aimbot", "Auto" });
                            ImAdd::SliderFloat("Offset Distance", &settings::magicbullet::offset_distance, 1.0f, 15.0f, "%.1f");
                            ImAdd::SliderInt("Hold Time (ms)", &settings::magicbullet::hold_ms, 10, 200);
                            ImAdd::SliderInt("TP Iterations", &settings::magicbullet::tp_iterations, 1000, 15000);
                        }

                        if (settings::rage::hipheight::enabled)
                        {
                            ImAdd::SliderFloat("Hip Height Value", &settings::rage::hipheight::height, 2.0f, 500.0f, "%.1f");
                        }

                        if (settings::rage::thirdperson::enabled)
                        {
                            ImAdd::SliderFloat("3rd Person Distance", &settings::rage::thirdperson::distance, 2.0f, 30.0f, "%.1f");
                            ImAdd::SliderFloat("Camera Height Offset", &settings::rage::thirdperson::height_offset, -5.0f, 10.0f, "%.1f");
                        }

                        if (settings::movement::orbit::enabled)
                        {
                            ImGui::Text("Target Type");
                            ImAdd::Combo("##Target Type", &settings::movement::orbit::orbit_type, { "Aimbot Target", "Silent Target" });
                            ImAdd::SliderFloat("Orbit Speed", &settings::movement::orbit::speed, 1.0f, 100.0f, "%.1f");
                            ImAdd::SliderFloat("Orbit Radius", &settings::movement::orbit::radius, 1.0f, 50.0f, "%.1f");
                            ImAdd::SliderFloat("Orbit Height Offset", &settings::movement::orbit::height_offset, 0.0f, 50.0f, "%.1f");
                            ImAdd::CheckBox("Spectate Target", &settings::movement::orbit::spectate_target);
                            ImAdd::CheckBox("Randomize", &settings::movement::orbit::randomize);
                            if (settings::movement::orbit::randomize)
                            {
                                ImAdd::SliderFloat("Randomize X", &settings::movement::orbit::randomize_x, 0.0f, 50.0f, "%.1f");
                                ImAdd::SliderFloat("Randomize Y", &settings::movement::orbit::randomize_y, 0.0f, 50.0f, "%.1f");
                            }
                        }

                        ImAdd::EndChild();
                    }

                    ImGui::EndGroup();
                }
                else if (logical_tab == 6)
                {
                    float group_width = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2);

                    ImGui::BeginGroup();
                    const float left_child_height = ImTrunc((ImGui::GetContentRegionAvail().y - style.ItemSpacing.y) / 2);
                    if (ImAdd::BeginChild("Auto", ImVec2(group_width, left_child_height)))
                    {
                        ImAdd::CheckBox("Auto Parry", &settings::blade_ball::auto_parry);
                        ImAdd::CheckBox("Auto Spam", &settings::blade_ball::auto_spam);
                        ImAdd::EndChild();
                    }

                    if (ImAdd::BeginChild("Misc", ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        ImAdd::CheckBox("Ball ESP", &settings::blade_ball::ball_esp);
                        ImAdd::CheckBox("Look At The Ball", &settings::blade_ball::look_at_ball);
                        ImAdd::CheckBox("Target Closest Player", &settings::blade_ball::target_closest_player);
                        ImAdd::CheckBox("Anti Curve", &settings::blade_ball::anti_curve);
                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine(0, style.ItemSpacing.x);

                    ImGui::BeginGroup();
                    if (ImAdd::BeginChild("Blade Ball Settings", ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        ImAdd::SliderFloat("Parry Distance", &settings::blade_ball::parry_distance, 4.0f, 35.0f, "%.1f");
                        ImAdd::SliderFloat("Parry Height", &settings::blade_ball::parry_height, 2.0f, 20.0f, "%.1f");
                        ImAdd::SliderInt("Spam Count", &settings::blade_ball::spam_count, 1, 12);
                        ImAdd::SliderFloat("Spam Sensitivity", &settings::blade_ball::spam_sensitivity, 0.0f, 1.0f, "%.2f");
                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();
                }
                else if (logical_tab == 4)
                {
                    float group_width = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2);

                    ImGui::BeginGroup();
                    if (ImAdd::BeginChild("Settings", ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        ImGui::AlignTextToFramePadding();
						ImGui::Text("Menu Key:");

                        static ImGuiKey menu_key = ImGuiKey_None;
                        menu_key = keybind::vk_to_imgui_key(settings::menu::menu_keybind);
                        if (ImAdd::KeyBind("##menu_key", &menu_key))
                        {
                            settings::menu::menu_keybind = keybind::imgui_key_to_vk(menu_key);
                        }

                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Panic Key");

                        static ImGuiKey panic_key = ImGuiKey_None;
                        panic_key = keybind::vk_to_imgui_key(settings::menu::panic_keybind);
                        if (ImAdd::KeyBind("##panic_key", &panic_key))
                        {
                            settings::menu::panic_keybind = keybind::imgui_key_to_vk(panic_key);
                        }

                        ImAdd::CheckBox("Watermark", &settings::menu::watermark);

                        if (settings::menu::watermark)
                        {
                            ImGui::Text("Elements:");
                            ImAdd::CheckBox("Cheat Name", &settings::watermark::show_cheat_name);
                            ImAdd::CheckBox("Game Name", &settings::watermark::show_game_name);
                            ImAdd::CheckBox("Display Name", &settings::watermark::show_display_name);
                            ImAdd::CheckBox("Username", &settings::watermark::show_username);
                            ImAdd::CheckBox("FPS", &settings::watermark::show_fps);
                                ImAdd::CheckBox("Server IP", &settings::watermark::show_server_ip);

                            ImGui::Spacing();
                            ImGui::Text("Separator:");
                            ImAdd::Combo("##wm_sep", &settings::watermark::separator_type, std::vector<const char*>({ " | ", " - ", " / ", " :: ", "   ", " \xC2\xB7 " }));

                            ImGui::Spacing();
                            ImAdd::CheckBox("Rainbow", &settings::watermark::rainbow);
                            if (settings::watermark::rainbow)
                            {
                                ImAdd::SliderFloat("Speed", &settings::watermark::rainbow_speed, 0.1f, 5.0f, "%.1f");
                            }
                            else
                            {
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() + style.ChildPadding.x);
                                ImAdd::ColorEdit4("##wm_color", settings::watermark::text_color);
                            }
                            ImGui::Spacing();
                            ImGui::Separator();
                            ImGui::Spacing();
                        }

                        ImAdd::CheckBox("Features List", &settings::ui::keybinds);
                        ImAdd::CheckBox("Streamproof", &settings::menu::streamproof);
                        ImAdd::CheckBox("V-Sync", &settings::menu::vsync);
                        ImGui::TextDisabled("Console is hidden in public release.");
                        ImAdd::CheckBox("Performance Mode", &settings::menu::performance_mode);
                        ImAdd::CheckBox("Cilent uncap fps", &settings::cilent::fpscaps::enabled);

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                        ImAdd::CheckBox("Show Custom Entities", &settings::custom_entities::show_custom_entities);
                        ImAdd::CheckBox("Auto Refresh", &settings::custom_entities::auto_refresh);
                        if (settings::custom_entities::auto_refresh)
                        {
                            ImAdd::SliderFloat("Refresh Rate", &settings::custom_entities::refresh_rate, 0.001f, 1.0f, "%.3f");
                        }
                        static char path_input[256];
                        strncpy_s(path_input, settings::custom_entities::current_input.c_str(), sizeof(path_input));
                        if (ImGui::InputText("Path", path_input, sizeof(path_input)))
                        {
                            settings::custom_entities::current_input = path_input;
                        }
                        if (ImGui::Button("Set"))
                        {
                            custom_entities::set_container(settings::custom_entities::current_input);
                        }
                        ImGui::Text("Entities:");
                        std::vector<settings::custom_entities::custom_container_t> containers_snapshot;
                        {
                            std::lock_guard<std::mutex> lock(custom_entities::containers_mtx);
                            containers_snapshot = settings::custom_entities::containers;
                        }
                        for (const auto& container : containers_snapshot)
                        {
                            ImGui::Text(container.path.c_str());
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        if (ImAdd::ButtonAccent("Unload", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        {
                            exit(0);
                        }

                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine(0, style.ItemSpacing.x);

                    ImGui::BeginGroup();
                    if (ImAdd::BeginChild("Configs", ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        static char config_name[64] = "";
                        static int selected_config = -1;
                        static std::vector<config::config_info_t> config_list;
                        static bool refresh_list = true;

                        if (refresh_list)
                        {
                            config_list = config::get_config_list();
                            refresh_list = false;
                        }

                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        bool input_active = ImGui::InputText("## Config Name", config_name, sizeof(config_name));

                        if (input_active && ImGui::IsItemActive() && selected_config >= 0)
                        {
                            selected_config = -1;
                        }

                        ImGui::Spacing();

                        if (ImAdd::ButtonAccent("Save Config", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        {
                            if (strlen(config_name) > 0)
                            {
                                if (config::save_config(config_name))
                                {
                                    config_list = config::get_config_list();
                                    selected_config = -1;
                                    memset(config_name, 0, sizeof(config_name));
                                    ImGui::SetKeyboardFocusHere(-1);
                                }
                            }
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        if (ImGui::BeginChild("## Config List", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::GetFontSize() - style.ItemSpacing.y * 2 - 60), true))
                        {
                            if (config_list.empty())
                            {
                                ImGui::TextDisabled("No configs found");
                            }
                            else
                            {
                                for (size_t i = 0; i < config_list.size(); i++)
                                {
                                    bool is_selected = (selected_config == static_cast<int>(i));
                                    if (ImGui::Selectable(config_list[i].name.c_str(), is_selected))
                                    {
                                        selected_config = static_cast<int>(i);
                                        strncpy_s(config_name, sizeof(config_name), config_list[i].name.c_str(), _TRUNCATE);
                                        if (ImGui::IsItemFocused())
                                        {
                                            ImGui::SetKeyboardFocusHere(-1);
                                        }
                                    }
                                }
                            }
                        }
                        ImGui::EndChild();

                        ImGui::Spacing();

                        if (selected_config >= 0 && selected_config < static_cast<int>(config_list.size()))
                        {
                            if (ImAdd::ButtonAccent("Load Config", ImVec2((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2, 0)))
                            {
                                config::load_config(config_list[selected_config].name.c_str());
                            }

                            ImGui::SameLine();

                            if (ImAdd::ButtonAccent("Delete Config", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            {
                                if (config::delete_config(config_list[selected_config].name))
                                {
                                    config_list = config::get_config_list();
                                    selected_config = -1;
                                    memset(config_name, 0, sizeof(config_name));
                                }
                            }
                        }

                        ImGui::Spacing();

                        if (selected_config >= 0)
                        {
                            if (ImAdd::ButtonAccent("Open File Location", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            {
                                config::open_file_location();
                            }
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        if (selected_config >= 0 && selected_config < static_cast<int>(config_list.size()))
                        {
                            bool is_autoload = (external_config::autoload_config == config_list[selected_config].name);
                            if (ImAdd::ButtonAccent(is_autoload ? "Remove Autoload" : "Set Autoload", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            {
                                if (is_autoload)
                                    external_config::autoload_config = "";
                                else
                                    external_config::autoload_config = config_list[selected_config].name;
                                external_config::save();
                            }
                        }

                        if (!external_config::autoload_config.empty())
                        {
                            ImGui::TextDisabled("Autoload: %s", external_config::autoload_config.c_str());
                        }

                        ImGui::Spacing();

                        if (ImAdd::ButtonAccent("Reset to Defaults", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        {
                            config::reset_to_defaults();
                        }

                        ImAdd::EndChild();
                    }
                    ImGui::EndGroup();
                }
                else if (logical_tab == 5)
                {
                    // LT2 Game Support Tab ??? Teleport only
                    float group_width = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2);

                    ImGui::BeginGroup();

                    if (ImAdd::BeginChild("LT2 Teleport", ImVec2(group_width, ImGui::GetContentRegionAvail().y)))
                    {
                        ImGui::Text("Teleport Locations");
                        ImGui::Separator();

                        const auto& locations = lt2::get_teleport_locations();
                        for (size_t i = 0; i < locations.size(); i++)
                        {
                            if (ImGui::Button(locations[i].name, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            {
                                lt2::teleport_to(locations[i].position);
                            }
                        }
                    }
                    ImAdd::EndChild();

                    ImGui::EndGroup();
                }
            }
            ImAdd::EndChild();
        }
        ImGui::End();
    }

    if (m_bExplorerWindowOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        bool explorer_window = ImGui::Begin("Explorer", &m_bExplorerWindowOpen, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize);
        ImGui::PopStyleVar(2);

        if (explorer_window)
        {
            ImRect window_bb(ImGui::GetCurrentWindow()->Rect());

            if (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_NoBackground)
            {
                ImGuiWindow* window = ImGui::GetCurrentWindow();

                window->DrawList->AddRectFilled(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_WindowBg));

                if (style.WindowBorderSize > 0.0f)
                {
                    window->DrawList->AddRect(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);
                    window->DrawList->AddRect(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize), window_bb.Max - ImVec2(style.WindowBorderSize, style.WindowBorderSize), ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);

                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 2.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 2.0f), ImGui::GetColorU32(ImGuiCol_Header), style.WindowBorderSize);
                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 3.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 3.0f), ImGui::GetColorU32(ImGuiCol_HeaderActive), style.WindowBorderSize);
                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 4.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 4.0f), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
                }

                ImAdd::RenderText(window_bb.Min + ImVec2(style.FramePadding.x + style.WindowBorderSize * 3.0f, style.FramePadding.y + style.WindowBorderSize * 4.0f), "Explorer", NULL, false, true);
            }

            ImGui::SetCursorScreenPos(window_bb.Min + ImVec2(style.WindowPadding.x, ImGui::GetFrameHeight() + style.WindowBorderSize * 3.0f + 3.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.WindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, ImVec2(style.WindowPadding.x, 4.6f));
            if (ImAdd::BeginChild("explorer_body", ImGui::GetContentRegionAvail() - style.WindowPadding))
            {

                float bottom_height = ImGui::GetFontSize() * 12.0f;

                const float bottom_gap = 3.0f;
                float available_height = ImGui::GetContentRegionAvail().y - bottom_gap;
                float explorer_height = available_height - bottom_height - style.ItemSpacing.y;

                if (explorer_height > 0 && available_height > 0 && bottom_height > 0)
                {

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, ImVec2(style.WindowPadding.x, 3.0f));
                    if (ImAdd::BeginChild("Explorer", ImVec2(0, explorer_height)))
                    {
                        if (explorer::explorer && !explorer::explorer->is_refreshing.load())
                        {
                            if (explorer::explorer->root)
                            {
                                try
                                {
                                    explorer::explorer->render_node(explorer::explorer->root);
                                }
                                catch (...)
                                {
                                    ImGui::TextDisabled("Error rendering explorer tree");
                                }
                            }
                            else
                            {
                                ImGui::TextDisabled("No game instance found");
                            }
                        }
                        else if (explorer::explorer && explorer::explorer->is_refreshing.load())
                        {
                            ImGui::TextDisabled("Refreshing...");
                        }
                        else
                        {
                            ImGui::TextDisabled("Explorer not initialized");
                        }
                        ImAdd::EndChild();
                        ImGui::PopStyleVar();
                    }

                }

                ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, ImVec2(style.WindowPadding.x, 3.0f));
                if (ImAdd::BeginChild("Settings", ImVec2(0, bottom_height)))
                {
                    if (explorer::explorer && !explorer::explorer->is_refreshing.load())
                    {
                        try
                        {
                            explorer::explorer->render_settings();
                            explorer::explorer->render_properties();
                        }
                        catch (...)
                        {
                            ImGui::TextDisabled("Error rendering explorer settings/properties");
                        }
                    }
                    else if (explorer::explorer && explorer::explorer->is_refreshing.load())
                    {
                        ImGui::TextDisabled("Refreshing...");
                    }
                    else
                    {
                        ImGui::TextDisabled("Explorer not initialized");
                    }
                    ImAdd::EndChild();
                }
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
        }
        ImGui::End();
    }

    // ESP Preview window removed

    // === Player List Window (styled like Explorer) ===
    if (m_bPlayerListOpen)
    {
        settings::rage::playerlist::enabled = true;
        ImGui::SetNextWindowSize(ImVec2(750, 500), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f + 50, io.DisplaySize.y * 0.5f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        bool pl_window = ImGui::Begin("##PlayerListWnd", &m_bPlayerListOpen, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(2);

        if (pl_window)
        {
            ImRect window_bb(ImGui::GetCurrentWindow()->Rect());
            if (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_NoBackground)
            {
                ImGuiWindow* window = ImGui::GetCurrentWindow();
                window->DrawList->AddRectFilled(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_WindowBg));
                if (style.WindowBorderSize > 0.0f)
                {
                    window->DrawList->AddRect(window_bb.Min, window_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);
                    window->DrawList->AddRect(window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize), window_bb.Max - ImVec2(style.WindowBorderSize, style.WindowBorderSize), ImGui::GetColorU32(ImGuiCol_Border), style.WindowRounding, ImDrawFlags_None, style.WindowBorderSize);
                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 2.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 2.0f), ImGui::GetColorU32(ImGuiCol_Header), style.WindowBorderSize);
                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 3.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 3.0f), ImGui::GetColorU32(ImGuiCol_HeaderActive), style.WindowBorderSize);
                    window->DrawList->AddLine(window_bb.Min + ImVec2(style.WindowBorderSize * 2.0f, style.WindowBorderSize * 4.0f), ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize * 4.0f), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
                }
                ImAdd::RenderText(window_bb.Min + ImVec2(style.FramePadding.x + style.WindowBorderSize * 3.0f, style.FramePadding.y + style.WindowBorderSize * 4.0f), "Player List", NULL, false, true);
            }

            ImGui::SetCursorScreenPos(window_bb.Min + ImVec2(style.WindowPadding.x, ImGui::GetFrameHeight() + style.WindowBorderSize * 3.0f + 3.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.WindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, ImVec2(style.WindowPadding.x, 4.6f));
            if (ImAdd::BeginChild("pl_body", ImGui::GetContentRegionAvail() - style.WindowPadding))
            {
                std::vector<cache::entity_t> players_snapshot;
                cache::entity_t local_snapshot;
                { std::lock_guard<std::mutex> lock(cache::mtx); players_snapshot = cache::cached_players; local_snapshot = cache::cached_local_player; }

                float left_w = ImTrunc(ImGui::GetContentRegionAvail().x * 0.55f);
                float right_w = ImGui::GetContentRegionAvail().x - left_w - style.ItemSpacing.x;
                float list_h = ImGui::GetContentRegionAvail().y;

                // Re-resolve selected player by name (stable across respawns)
                if (!settings::rage::playerlist::selected_name.empty())
                {
                    bool resolved = false;
                    for (const auto& e : players_snapshot)
                    {
                        if (e.name == settings::rage::playerlist::selected_name)
                        {
                            settings::rage::playerlist::selected_address = e.instance.address;
                            resolved = true;
                            break;
                        }
                    }
                    if (!resolved) settings::rage::playerlist::selected_address = 0;
                }

                // Spectate: continuously re-apply CameraSubject for target (survives respawn)
                if (settings::rage::playerlist::is_spectating && !settings::rage::playerlist::spectate_target_name.empty() && game::camera != 0)
                {
                    for (const auto& e : players_snapshot)
                    {
                        if (e.name == settings::rage::playerlist::spectate_target_name)
                        {
                            auto hi = e.parts.find("Head");
                            if (hi == e.parts.end() || !hi->second.address)
                                hi = e.parts.find("HumanoidRootPart");
                            if (hi != e.parts.end() && hi->second.address)
                            {
                                try { memory->write<std::uint64_t>(game::camera + Offsets::Camera::CameraSubject, hi->second.address); } catch (...) {}
                            }
                            break;
                        }
                    }
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, ImVec2(style.WindowPadding.x, 3.0f));
                if (ImAdd::BeginChild("##PL_List", ImVec2(left_w, list_h)))
                {
                    if (players_snapshot.empty()) { ImGui::TextDisabled("No players found"); }
                    else
                    {
                        bool first_entry = true;
                        for (int i = 0; i < static_cast<int>(players_snapshot.size()); i++)
                        {
                            const auto& entity = players_snapshot[i];
                            if (entity.instance.address == local_snapshot.instance.address) continue;

                            if (!first_entry) ImGui::Separator();
                            first_entry = false;

                            bool wl = settings::rage::playerlist::whitelist.count(entity.name) > 0;
                            bool is_tgt = (!settings::rage::playerlist::target_name.empty() && entity.name == settings::rage::playerlist::target_name);
                            char label[300];
                            snprintf(label, sizeof(label), "%s%s%s (@%s) | HP: %.0f/%.0f",
                                is_tgt ? "[TGT] " : "",
                                wl ? "[WL] " : "",
                                entity.display_name.c_str(), entity.name.c_str(),
                                entity.health, entity.max_health);

                            bool is_selected = (!settings::rage::playerlist::selected_name.empty() && entity.name == settings::rage::playerlist::selected_name);
                            ImGui::PushID(i);
                            if (is_tgt) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                            else if (wl) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns))
                            {
                                settings::rage::playerlist::selected_index = i;
                                settings::rage::playerlist::selected_address = entity.instance.address;
                                settings::rage::playerlist::selected_name = entity.name;
                            }
                            if (is_tgt || wl) ImGui::PopStyleColor();
                            ImGui::PopID();
                        }
                    }
                    ImAdd::EndChild();
                }
                ImGui::PopStyleVar();

                ImGui::SameLine();
                ImAdd::VSeparator(2.0f);
                ImGui::SameLine();

                ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, ImVec2(style.WindowPadding.x, 3.0f));
                if (ImAdd::BeginChild("##PL_Actions", ImVec2(right_w - 6.0f, list_h)))
                {
                    cache::entity_t sel{}; bool found = false;
                    if (!settings::rage::playerlist::selected_name.empty())
                    {
                        std::lock_guard<std::mutex> lock(cache::mtx);
                        for (const auto& e : cache::cached_players)
                        {
                            if (e.name == settings::rage::playerlist::selected_name)
                            { sel = e; found = true; break; }
                        }
                    }

                    if (!found || settings::rage::playerlist::selected_name.empty()) { ImGui::TextDisabled("Select a player"); }
                    else
                    {
                        ImGui::Text("Name: %s", sel.display_name.c_str());
                        ImGui::Text("User: @%s", sel.name.c_str());
                        ImGui::Text("HP: %.0f / %.0f", sel.health, sel.max_health);
                        math::vector3 tp{}; bool has_tp = false;
                        { auto rit = sel.parts.find("HumanoidRootPart"); if (rit != sel.parts.end() && rit->second.address) { try { rbx::primitive_t p = rit->second.get_primitive(); if (p.address) { tp = p.get_position(); has_tp = true; } } catch (...) {} } }
                        if (has_tp) ImGui::Text("Pos: %.0f, %.0f, %.0f", tp.x, tp.y, tp.z);

                        bool wl = settings::rage::playerlist::whitelist.count(sel.name) > 0;
                        if (wl)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                            ImGui::Text("WHITELISTED");
                            ImGui::PopStyleColor();
                        }

                        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                        // Target / Untarget
                        bool is_targeted = (!settings::rage::playerlist::target_name.empty() && sel.name == settings::rage::playerlist::target_name);
                        if (is_targeted)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                            ImGui::Text("TARGETED");
                            ImGui::PopStyleColor();
                        }
                        if (ImAdd::ButtonAccent(is_targeted ? "Untarget" : "Target", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        {
                            if (is_targeted)
                            {
                                settings::rage::playerlist::target_name.clear();
                                settings::rage::playerlist::target_address = 0;
                            }
                            else
                            {
                                settings::rage::playerlist::target_name = sel.name;
                                settings::rage::playerlist::target_address = sel.instance.address;
                            }
                        }

                        ImGui::Spacing();

                        // Whitelist / Unwhitelist
                        if (ImAdd::ButtonAccent(wl ? "Remove Whitelist" : "Whitelist", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        {
                            if (wl) settings::rage::playerlist::whitelist.erase(sel.name);
                            else settings::rage::playerlist::whitelist.insert(sel.name);
                        }

                        ImGui::Spacing();

                        // Teleport To
                        if (ImAdd::ButtonAccent("Teleport To", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && has_tp)
                        {
                            try {
                                std::lock_guard<std::mutex> lock(cache::mtx);
                                auto ri = cache::cached_local_player.parts.find("HumanoidRootPart");
                                if (ri != cache::cached_local_player.parts.end() && ri->second.address)
                                {
                                    uintptr_t lp = memory->read<uintptr_t>(ri->second.address + Offsets::BasePart::Primitive);
                                    if (lp)
                                    {
                                        settings::rage::playerlist::saved_position = memory->read<math::vector3>(lp + Offsets::Primitive::Position);
                                        settings::rage::playerlist::has_saved_position = true;
                                        math::vector3 t = tp; t.x += 3.0f;
                                        for (int w = 0; w < 5; w++) { memory->write<math::vector3>(lp + Offsets::Primitive::Position, t); memory->write<math::vector3>(lp + Offsets::Primitive::AssemblyLinearVelocity, math::vector3(0, 0, 0)); }
                                    }
                                }
                            } catch (...) {}
                        }

                        // Teleport Back
                        if (settings::rage::playerlist::has_saved_position)
                        {
                            if (ImAdd::ButtonAccent("Teleport Back", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            {
                                try {
                                    std::lock_guard<std::mutex> lock(cache::mtx);
                                    auto ri = cache::cached_local_player.parts.find("HumanoidRootPart");
                                    if (ri != cache::cached_local_player.parts.end() && ri->second.address)
                                    {
                                        uintptr_t lp = memory->read<uintptr_t>(ri->second.address + Offsets::BasePart::Primitive);
                                        if (lp)
                                        {
                                            for (int w = 0; w < 5; w++) { memory->write<math::vector3>(lp + Offsets::Primitive::Position, settings::rage::playerlist::saved_position); memory->write<math::vector3>(lp + Offsets::Primitive::AssemblyLinearVelocity, math::vector3(0, 0, 0)); }
                                            settings::rage::playerlist::has_saved_position = false;
                                        }
                                    }
                                } catch (...) {}
                            }
                        }

                        ImGui::Spacing();

                        // Spectate / Unspectate
                        bool is_spec = settings::rage::playerlist::is_spectating;
                        if (ImAdd::ButtonAccent(is_spec ? "Unspectate" : "Spectate", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        {
                            if (is_spec)
                            {
                                if (game::camera != 0 && settings::rage::playerlist::original_camera_subject != 0)
                                    memory->write<std::uint64_t>(game::camera + Offsets::Camera::CameraSubject, settings::rage::playerlist::original_camera_subject);
                                settings::rage::playerlist::is_spectating = false;
                                settings::rage::playerlist::spectate_target_name.clear();
                                settings::rage::playerlist::original_camera_subject = 0;
                            }
                            else
                            {
                                if (game::camera != 0)
                                {
                                    settings::rage::playerlist::original_camera_subject = memory->read<std::uint64_t>(game::camera + Offsets::Camera::CameraSubject);
                                    settings::rage::playerlist::spectate_target_name = sel.name;
                                    settings::rage::playerlist::is_spectating = true;
                                }
                            }
                        }
                    }
                    ImAdd::EndChild();
                }
                ImGui::PopStyleVar();

                ImAdd::EndChild();
            }
            ImGui::PopStyleVar(2);
        }
        ImGui::End();
        if (!m_bPlayerListOpen) settings::rage::playerlist::enabled = false;
    }
    else { settings::rage::playerlist::enabled = false; }

    // --- Target leave notification ---
    {
        static std::string notif_text;
        static float notif_timer = 0.0f;
        static bool was_target_present = false;
        const float NOTIF_DURATION = 4.0f; // seconds

        // Check if targeted player is still in the server
        if (!settings::rage::playerlist::target_name.empty())
        {
            bool target_found = false;
            {
                std::lock_guard<std::mutex> lock(cache::mtx);
                for (const auto& e : cache::cached_players)
                {
                    if (e.name == settings::rage::playerlist::target_name)
                    { target_found = true; break; }
                }
            }

            if (!target_found && was_target_present)
            {
                notif_text = settings::rage::playerlist::target_name + " left the server";
                notif_timer = NOTIF_DURATION;
                settings::rage::playerlist::target_name.clear();
                settings::rage::playerlist::target_address = 0;
            }
            was_target_present = target_found;
        }
        else
        {
            was_target_present = false;
        }

        // Draw notification
        if (notif_timer > 0.0f)
        {
            notif_timer -= io.DeltaTime;
            float alpha = (notif_timer < 1.0f) ? notif_timer : 1.0f; // fade out last second

            ImVec2 text_size = ImGui::CalcTextSize(notif_text.c_str());
            float pad_x = 12.0f;
            float pad_y = 8.0f;
            float notif_w = text_size.x + pad_x * 2.0f;
            float notif_h = text_size.y + pad_y * 2.0f;
            float screen_w = io.DisplaySize.x;

            ImVec2 notif_pos(screen_w - notif_w - 20.0f, 60.0f);
            ImVec2 notif_end(notif_pos.x + notif_w, notif_pos.y + notif_h);

            ImDrawList* draw = ImGui::GetForegroundDrawList();

            // Background
            draw->AddRectFilled(notif_pos, notif_end,
                IM_COL32(24, 24, 24, static_cast<int>(220 * alpha)), 4.0f);

            // Border
            draw->AddRect(notif_pos, notif_end,
                IM_COL32(60, 60, 60, static_cast<int>(255 * alpha)), 4.0f);

            // Red accent line on top
            draw->AddLine(
                ImVec2(notif_pos.x + 1.0f, notif_pos.y + 1.0f),
                ImVec2(notif_end.x - 1.0f, notif_pos.y + 1.0f),
                IM_COL32(255, 80, 80, static_cast<int>(255 * alpha)), 2.0f);

            // Text
            draw->AddText(
                ImVec2(notif_pos.x + pad_x, notif_pos.y + pad_y),
                IM_COL32(255, 100, 100, static_cast<int>(255 * alpha)),
                notif_text.c_str());
        }
    }

}

void Menu::Shutdown()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}



