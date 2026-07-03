#include "magicbullet.h"

#include <thread>
#include <chrono>
#include <mutex>
#include <cmath>
#include <windows.h>

#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <sdk/sdk.h>
#include <sdk/math/math.h>
#include <features/aimbot/aimbot.h>
#include <features/silentaim/silentaim.h>
#include <menu/keybind/keybind.h>

namespace rage { namespace magicbullet {

static math::vector3 get_target_position()
{
    // try silent aim target
    if (settings::magicbullet::target_source == 0 || settings::magicbullet::target_source == 2)
    {
        if (silentaim::state.data_ready && silentaim::state.target.instance.address)
        {
            auto it = silentaim::state.target.parts.find("HumanoidRootPart");
            if (it != silentaim::state.target.parts.end() && it->second.address)
            {
                try {
                    rbx::primitive_t prim = it->second.get_primitive();
                    if (prim.address)
                        return prim.get_position();
                } catch (...) {}
            }
        }
    }

    // try aimbot target
    if (settings::magicbullet::target_source == 1 || settings::magicbullet::target_source == 2)
    {
        if (aimbot::player.instance.address)
        {
            auto it = aimbot::player.parts.find("HumanoidRootPart");
            if (it != aimbot::player.parts.end() && it->second.address)
            {
                try {
                    rbx::primitive_t prim = it->second.get_primitive();
                    if (prim.address)
                        return prim.get_position();
                } catch (...) {}
            }
        }
    }

    return { 0.f, 0.f, 0.f };
}

static void tp_write(std::uint64_t prim_addr, const math::vector3& pos, int iterations)
{
    math::vector3 zero_vel = { 0.f, 0.f, 0.f };
    for (int i = 0; i < iterations; i++)
    {
        memory->write<math::vector3>(prim_addr + Offsets::Primitive::Position, pos);
        memory->write<math::vector3>(prim_addr + Offsets::Primitive::AssemblyLinearVelocity, zero_vel);
    }
}

void run()
{
    bool toggle_state = false;
    bool was_pressed = false;
    bool was_mouse_down = false;

    while (runtime::alive())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        if (!settings::magicbullet::enabled || !game::datamodel.address)
        {
            toggle_state = false;
            was_pressed = false;
            was_mouse_down = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // keybind check
        bool key_active = true;
        int kb = settings::magicbullet::keybind;
        int kb_mode = settings::magicbullet::activation_mode;
        if (kb != 0)
        {
            bool is_down = (GetAsyncKeyState(kb) & 0x8000) != 0;
            if (kb_mode == 1) {
                key_active = is_down;
            } else if (kb_mode == 0) {
                if (is_down && !was_pressed) toggle_state = !toggle_state;
                was_pressed = is_down;
                key_active = toggle_state;
            } else {
                key_active = true;
            }
        }

        if (!key_active)
        {
            was_mouse_down = false;
            continue;
        }

        // detect mouse click
        bool mouse_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool just_clicked = mouse_down && !was_mouse_down;
        was_mouse_down = mouse_down;

        if (!just_clicked) continue;

        // get target
        math::vector3 target_pos = get_target_position();
        if (target_pos.x == 0.f && target_pos.y == 0.f && target_pos.z == 0.f)
            continue;

        // get local player primitive
        std::uint64_t local_prim_addr = 0;
        math::vector3 local_pos{};
        {
            std::lock_guard<std::mutex> lock(cache::mtx);
            auto it = cache::cached_local_player.parts.find("HumanoidRootPart");
            if (it == cache::cached_local_player.parts.end() || !it->second.address)
                continue;
            try {
                rbx::primitive_t prim = it->second.get_primitive();
                if (!prim.address) continue;
                local_prim_addr = prim.address;
                local_pos = prim.get_position();
            } catch (...) { continue; }
        }

        // calculate position near target (offset_distance studs away, towards local player)
        math::vector3 dir = {
            local_pos.x - target_pos.x,
            local_pos.y - target_pos.y,
            local_pos.z - target_pos.z
        };
        float dist = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (dist < 0.5f) continue;

        float offset = settings::magicbullet::offset_distance;
        dir.x /= dist;
        dir.y /= dist;
        dir.z /= dist;

        math::vector3 shoot_pos = {
            target_pos.x + dir.x * offset,
            target_pos.y + dir.y * offset,
            target_pos.z + dir.z * offset
        };

        int tp_iters = settings::magicbullet::tp_iterations;
        int hold = settings::magicbullet::hold_ms;

        try
        {
            // teleport to target
            tp_write(local_prim_addr, shoot_pos, tp_iters);

            // hold at target for the shot raycast to register
            std::this_thread::sleep_for(std::chrono::milliseconds(hold));

            // teleport back to original position
            tp_write(local_prim_addr, local_pos, tp_iters);
        }
        catch (...)
        {
            // always try to return home
            try { tp_write(local_prim_addr, local_pos, tp_iters); } catch (...) {}
        }
    }
}

} }
