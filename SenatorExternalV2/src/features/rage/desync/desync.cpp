#include "desync.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cmath>
#include <Windows.h>
#include <memory/memory.h>
#include <Offsets/FFlags.hpp>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <imgui/imgui.h>

static std::atomic<bool> s_running{ false };
static std::thread s_thread;
static std::atomic<bool> s_scene_active{ false };
static math::vector3 s_scene_pos{ 0.0f, 0.0f, 0.0f };
static std::mutex s_scene_mtx;

static void s_thread_fn()
{
    bool held          = false;
    bool toggle_state  = false;
    bool was_pressed   = false;
    int  old_kb        = 0;
    auto chill_out     = std::chrono::steady_clock::time_point{};

    while (s_running.load()) {
        try {
            if (!settings::desync::enabled) {
                if (held || s_scene_active.load()) {
                    held = false;
                    toggle_state = false; was_pressed = false;
                    try { memory->write<bool>(memory->get_module_address() + FFlagOffsets::FFlags::NextGenReplicatorEnabledWrite4, false); } catch (...) {}
                    std::lock_guard<std::mutex> lk(s_scene_mtx);
                    s_scene_active.store(false);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            // Cooldown when keybind changes
            if (settings::desync::keybind != old_kb) {
                old_kb       = settings::desync::keybind;
                chill_out    = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
                toggle_state = false; was_pressed = false;
            }

            bool key = false;
            int  kb_key  = settings::desync::keybind;
            int  kb_mode = settings::desync::keybind_mode;

            if (kb_key != 0 && std::chrono::steady_clock::now() >= chill_out) {
                bool is_down = (GetAsyncKeyState(kb_key) & 0x8000) != 0;
                if (kb_mode == 1) { // Hold
                    key = is_down;
                } else if (kb_mode == 0) { // Toggle
                    if (is_down && !was_pressed) toggle_state = !toggle_state;
                    was_pressed = is_down;
                    key = toggle_state;
                } else { // Always
                    key = true;
                }
            }

            if (key && !held) {
                held = true;
                try {
                    std::lock_guard<std::mutex> lk(cache::mtx);
                    auto& lp = cache::cached_local_player;
                    if (lp.instance.address) {
                        auto it = lp.parts.find("HumanoidRootPart");
                        if (it != lp.parts.end() && it->second.address) {
                            try {
                                math::vector3 pos = it->second.get_primitive().get_position();
                                std::lock_guard<std::mutex> lk2(s_scene_mtx);
                                s_scene_pos = pos;
                                s_scene_active.store(true);
                            } catch (...) {}
                        }
                    }
                } catch (...) {}
                try { memory->write<bool>(memory->get_module_address() + FFlagOffsets::FFlags::NextGenReplicatorEnabledWrite4, true); } catch (...) {}
            } else if (!key && held) {
                held = false;
                try { memory->write<bool>(memory->get_module_address() + FFlagOffsets::FFlags::NextGenReplicatorEnabledWrite4, false); } catch (...) {}
                std::lock_guard<std::mutex> lk(s_scene_mtx);
                s_scene_active.store(false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    try { memory->write<bool>(memory->get_module_address() + FFlagOffsets::FFlags::NextGenReplicatorEnabledWrite4, false); } catch (...) {}
    std::lock_guard<std::mutex> lk(s_scene_mtx);
    s_scene_active.store(false);
}

namespace rage { namespace desync {

void initialize()
{
    if (s_running.load()) return;
    s_running.store(true);
    s_thread = std::thread(s_thread_fn);
}

void shutdown()
{
    if (!s_running.load()) return;
    s_running.store(false);
    if (s_thread.joinable()) s_thread.join();
    std::lock_guard<std::mutex> lk(s_scene_mtx);
    s_scene_active.store(false);
    s_scene_pos = { 0.0f, 0.0f, 0.0f };
}

bool is_active()
{
    return s_running.load();
}

scene_t get_scene_snapshot()
{
    scene_t out{ false, {0.0f, 0.0f, 0.0f} };
    std::lock_guard<std::mutex> lk(s_scene_mtx);
    out.active   = s_scene_active.load();
    out.position = s_scene_pos;
    return out;
}

void render_visualizer()
{
    if (!settings::desync::visualizer::enabled) return;
    if (!s_scene_active.load()) return;

    math::vector3 frozen_pos;
    {
        std::lock_guard<std::mutex> lk(s_scene_mtx);
        frozen_pos = s_scene_pos;
    }

    math::vector2 dims = game::visengine.get_dimensions();
    math::matrix4 view = game::visengine.get_viewmatrix();
    math::vector2 screen{};
    if (!game::visengine.world_to_screen(frozen_pos, screen, dims, view))
        return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float* c = settings::desync::visualizer::color;
    ImU32 col = IM_COL32((int)(c[0]*255),(int)(c[1]*255),(int)(c[2]*255),(int)(c[3]*255));
    ImU32 col_bg = IM_COL32((int)(c[0]*255),(int)(c[1]*255),(int)(c[2]*255),(int)(c[3]*80));
    ImU32 black = IM_COL32(0, 0, 0, 180);

    ImVec2 sp(screen.x, screen.y);

    // small filled circle with outline
    dl->AddCircleFilled(sp, 5.f, black);
    dl->AddCircleFilled(sp, 4.f, col_bg);
    dl->AddCircle(sp, 4.f, col, 16, 1.5f);
}

} }
