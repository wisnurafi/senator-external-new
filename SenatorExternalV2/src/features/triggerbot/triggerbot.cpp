#include "triggerbot.h"

#include <thread>
#include <chrono>
#include <random>
#include <cmath>
#include <windows.h>
#include <game/game.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <settings.h>
#include <menu/keybind/keybind.h>
#include <cache/cache.h>
#include <features/aimbot/aimbot.h>
#include <features/silentaim/silentaim.h>
#include <wallcheck/wallcheck.h>
#include <render/render.h>
#include <mutex>

static bool is_player_knocked(const cache::entity_t& player)
{
	if (player.instance.address == 0)
		return false;

	rbx::model_instance_t model_instance = rbx::player_t(player.instance.address).get_model_instance();
	if (model_instance.address == 0)
		return false;

	rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
	if (body_effects.address == 0)
	{
		std::vector<rbx::instance_t> children = model_instance.get_children();
		for (const auto& child : children)
		{
			if (child.get_name() == "BodyEffects")
			{
				body_effects = child;
				break;
			}
		}
		if (body_effects.address == 0)
			return false;
	}

	rbx::instance_t ko = body_effects.find_first_child("K.O");
	if (ko.address == 0)
		return false;

	std::string ko_class = ko.get_class_name();
	if (ko_class != "BoolValue")
		return false;

	bool value = false;
	try {
		value = memory->read<bool>(ko.address + Offsets::Misc::Value);
	} catch (...) {
		value = false;
	}
	return value;
}

static void mouse_left_down()
{
	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	SendInput(1, &input, sizeof(INPUT));
}

static void mouse_left_up()
{
	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(1, &input, sizeof(INPUT));
}

static void mouse_left_click()
{
	mouse_left_down();
	Sleep(15);
	mouse_left_up();
}

// Per-source fire state (0 = aimbot, 1 = silentaim)
struct fire_state_t
{
	float click_timer = 0.f;
	float hold_timer = 0.f;
	bool holding = false;
	std::chrono::high_resolution_clock::time_point last_fire_time{};
	bool first_shot = true;
};
static fire_state_t g_fire_state[2];

static bool mouse_fire_helper(int source, bool on_target, int fire_mode, float clicks_per_second, float hold_duration_val)
{
	auto& state = g_fire_state[source];

	if (!on_target)
	{
		state.click_timer = 0.f;
		state.hold_timer = 0.f;
		state.first_shot = true;
		if (state.holding)
		{
			mouse_left_up();
			state.holding = false;
		}
		return false;
	}

	// Calculate delta from last fire call
	auto now = std::chrono::high_resolution_clock::now();
	float delta = std::chrono::duration<float>(now - state.last_fire_time).count();
	state.last_fire_time = now;

	// Clamp delta to avoid huge jumps
	if (delta > 0.5f) delta = 0.0f;

	switch (fire_mode)
	{
	case 0: // Click mode
	{
		if (state.first_shot)
		{
			mouse_left_click();
			state.first_shot = false;
			state.click_timer = 0.f;
			return true;
		}
		float interval = 1.f / clicks_per_second;
		state.click_timer += delta;
		if (state.click_timer >= interval)
		{
			mouse_left_click();
			state.click_timer -= interval;
			return true;
		}
		break;
	}
	case 1: // Hold mode
	{
		if (!state.holding)
		{
			mouse_left_down();
			state.holding = true;
			state.hold_timer = 0.f;
		}
		state.hold_timer += delta;
		if (state.hold_timer >= hold_duration_val)
		{
			mouse_left_up();
			state.holding = false;
			state.hold_timer = 0.f;
			state.first_shot = true;
		}
		return true;
	}
	}

	return false;
}

static float get_world_distance(const cache::entity_t& entity, const cache::entity_t& local)
{
	auto target_it = entity.parts.find("HumanoidRootPart");
	if (target_it == entity.parts.end() || !target_it->second.address)
		target_it = entity.parts.find("Torso");
	if (target_it == entity.parts.end() || !target_it->second.address)
		target_it = entity.parts.find("Head");
	if (target_it == entity.parts.end() || !target_it->second.address)
		return FLT_MAX;

	auto local_it = local.parts.find("HumanoidRootPart");
	if (local_it == local.parts.end() || !local_it->second.address)
		local_it = local.parts.find("Torso");
	if (local_it == local.parts.end() || !local_it->second.address)
		return FLT_MAX;

	rbx::part_t target_part{ target_it->second };
	rbx::primitive_t target_prim = target_part.get_primitive();
	if (!target_prim.address) return FLT_MAX;

	rbx::part_t local_part{ local_it->second };
	rbx::primitive_t local_prim = local_part.get_primitive();
	if (!local_prim.address) return FLT_MAX;

	try
	{
		math::vector3 target_pos = target_prim.get_position();
		math::vector3 local_pos = local_prim.get_position();
		return local_pos.distance(target_pos);
	}
	catch (...)
	{
		return FLT_MAX;
	}
}

static bool get_entity_world_pos(const cache::entity_t& entity, math::vector3& out_pos)
{
	auto it = entity.parts.find("HumanoidRootPart");
	if (it == entity.parts.end() || !it->second.address)
		it = entity.parts.find("Head");
	if (it == entity.parts.end() || !it->second.address)
		return false;

	rbx::part_t part{ it->second };
	rbx::primitive_t prim = part.get_primitive();
	if (!prim.address) return false;

	try
	{
		out_pos = prim.get_position();
		return !(std::isnan(out_pos.x) || out_pos.x == 0.0f && out_pos.y == 0.0f && out_pos.z == 0.0f);
	}
	catch (...)
	{
		return false;
	}
}

static bool get_camera_position(math::vector3& out_pos)
{
	if (!game::visengine.address) return false;

	try
	{
		math::matrix4 view = game::visengine.get_viewmatrix();
		float ix = -(view.m[0][0] * view.m[0][3] + view.m[1][0] * view.m[1][3] + view.m[2][0] * view.m[2][3]);
		float iy = -(view.m[0][1] * view.m[0][3] + view.m[1][1] * view.m[1][3] + view.m[2][1] * view.m[2][3]);
		float iz = -(view.m[0][2] * view.m[0][3] + view.m[1][2] * view.m[1][3] + view.m[2][2] * view.m[2][3]);
		out_pos = { ix, iy, iz };
		return true;
	}
	catch (...)
	{
		return false;
	}
}

static bool is_crosshair_on_target(
	const cache::entity_t& entity,
	const math::matrix4& view,
	const math::vector2& dims,
	math::vector2& out_screen)
{
	POINT cursor{};
	if (!GetCursorPos(&cursor))
		return false;

	const math::vector2 cursor_pos{ static_cast<float>(cursor.x), static_cast<float>(cursor.y) };

	static const char* part_names[] = { "Head", "HumanoidRootPart", "LeftArm", "RightArm", "LeftLeg", "RightLeg", "Torso", "LeftUpperArm", "RightUpperArm", "LeftUpperLeg", "RightUpperLeg" };

	float closest_dist = FLT_MAX;
	bool found = false;

	for (const char* name : part_names)
	{
		auto it = entity.parts.find(name);
		if (it == entity.parts.end() || !it->second.address)
			continue;

		rbx::part_t part{ it->second };
		rbx::primitive_t prim = part.get_primitive();
		if (!prim.address) continue;

		math::vector3 pos_3d;
		try
		{
			pos_3d = prim.get_position();
			if (std::isnan(pos_3d.x) || std::isnan(pos_3d.y) || std::isnan(pos_3d.z))
				continue;
			if (pos_3d.x == 0.0f && pos_3d.y == 0.0f && pos_3d.z == 0.0f)
				continue;
		}
		catch (...)
		{
			continue;
		}

		math::vector2 pos_2d{};
		if (!game::visengine.world_to_screen(pos_3d, pos_2d, dims, view))
			continue;

		float dx = cursor_pos.x - pos_2d.x;
		float dy = cursor_pos.y - pos_2d.y;
		float dist = std::sqrtf(dx * dx + dy * dy);

		if (dist < 20.0f && dist < closest_dist)
		{
			closest_dist = dist;
			out_screen = pos_2d;
			found = true;
		}
	}

	return found;
}

static bool is_in_fov(const cache::entity_t& entity, const math::matrix4& view, const math::vector2& dims, float fov)
{
	POINT cursor{};
	if (!GetCursorPos(&cursor))
		return false;

	const math::vector2 cursor_pos{ static_cast<float>(cursor.x), static_cast<float>(cursor.y) };

	static const char* check_parts[] = { "Head", "HumanoidRootPart" };
	for (const char* name : check_parts)
	{
		auto it = entity.parts.find(name);
		if (it == entity.parts.end() || !it->second.address) continue;

		rbx::part_t part{ it->second };
		rbx::primitive_t prim = part.get_primitive();
		if (!prim.address) continue;

		math::vector3 pos_3d;
		try
		{
			pos_3d = prim.get_position();
			if (std::isnan(pos_3d.x)) continue;
		}
		catch (...) { continue; }

		math::vector2 pos_2d{};
		if (!game::visengine.world_to_screen(pos_3d, pos_2d, dims, view))
			continue;

		float dx = cursor_pos.x - pos_2d.x;
		float dy = cursor_pos.y - pos_2d.y;
		float dist = std::sqrtf(dx * dx + dy * dy);

		if (dist <= fov)
			return true;
	}

	return false;
}

// Shared triggerbot logic with delta-time fire modes
static void triggerbot_tick(
	int source,
	bool enabled,
	float reaction_ms,
	bool max_dist_enabled,
	float max_dist,
	bool use_teamcheck,
	bool use_knockcheck,
	bool use_healthcheck,
	float min_health,
	bool use_wallcheck,
	bool use_fov,
	float fov,
	int fire_mode,
	float clicks_per_second,
	float hold_duration_val)
{
	if (!enabled) return;

	// don't fire if Roblox isn't the foreground window or if the cheat menu is open
	HWND fg = GetForegroundWindow();
	HWND roblox = game::get_roblox_window();
	if (!roblox || fg != roblox) return;
	if (render->running) return;

	math::vector2 dims = game::visengine.get_dimensions();
	math::matrix4 view = game::visengine.get_viewmatrix();
	if (dims.x == 0 || dims.y == 0) return;

	std::vector<cache::entity_t> entities_snapshot;
	cache::entity_t local_snapshot;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		entities_snapshot = cache::cached_players;
		local_snapshot = cache::cached_local_player;
	}

	math::vector3 camera_pos{};
	bool has_camera = false;
	if (use_wallcheck)
		has_camera = get_camera_position(camera_pos);

	bool found_target = false;

	for (cache::entity_t& entity : entities_snapshot)
	{
		if (!entity.instance.address) continue;
		if (entity.instance.address == local_snapshot.instance.address) continue;

		if (use_teamcheck && entity.team != 0 && entity.team == local_snapshot.team)
			continue;

		if (settings::rage::is_whitelisted(entity.name))
			continue;

		// always skip knocked/dead players
		if (entity.health <= 0.f || is_player_knocked(entity))
			continue;

		if (use_healthcheck && entity.health < min_health)
			continue;

		if (max_dist_enabled && local_snapshot.instance.address != 0)
		{
			float dist = get_world_distance(entity, local_snapshot);
			if (dist > max_dist) continue;
		}

		bool fov_passed = false;
		if (use_fov && fov > 0.0f)
		{
			if (!is_in_fov(entity, view, dims, fov))
				continue;
			fov_passed = true;
		}

		if (use_wallcheck && has_camera)
		{
			math::vector3 entity_pos{};
			if (get_entity_world_pos(entity, entity_pos))
			{
				if (!wallcheck->is_visible(camera_pos, entity_pos))
					continue;
			}
		}

		// If FOV check passed (silent aim triggerbot), fire without needing crosshair on target.
		// If no FOV check (aimbot triggerbot), require crosshair directly on target.
		math::vector2 target_screen{};
		bool should_fire = false;

		if (fov_passed)
		{
			// Target is within FOV — get screen pos for display but fire regardless
			auto it = entity.parts.find("HumanoidRootPart");
			if (it == entity.parts.end() || !it->second.address)
				it = entity.parts.find("Head");
			if (it != entity.parts.end() && it->second.address)
			{
				try {
					rbx::part_t part{ it->second };
					rbx::primitive_t prim = part.get_primitive();
					if (prim.address)
					{
						math::vector3 pos_3d = prim.get_position();
						game::visengine.world_to_screen(pos_3d, target_screen, dims, view);
					}
				} catch (...) {}
			}
			should_fire = true;
		}
		else
		{
			should_fire = is_crosshair_on_target(entity, view, dims, target_screen);
		}

		if (should_fire)
		{
			triggerbot::current_target = entity;
			triggerbot::current_target_screen = target_screen;
			triggerbot::has_target = true;
			found_target = true;

			// Reaction delay with small random jitter (+/- 20%)
			if (reaction_ms > 0.0f)
			{
				static std::mt19937 rng(std::random_device{}());
				float jitter = reaction_ms * 0.2f;
				std::uniform_real_distribution<float> dist_r(-jitter, jitter);
				int delay = static_cast<int>(reaction_ms + dist_r(rng));
				if (delay > 0) Sleep(delay);
			}

			mouse_fire_helper(source, true, fire_mode, clicks_per_second, hold_duration_val);
			break;
		}
	}

	if (!found_target)
	{
		mouse_fire_helper(source, false, 0, 0, 0);
		triggerbot::has_target = false;
	}
}

void triggerbot::run_aimbot()
{
	keybind::keybind_t kb{};

	for (; runtime::alive(); )
	{
		if (!settings::aimbot::triggerbot::enabled)
		{
			mouse_fire_helper(0, false, 0, 0, 0);
			triggerbot::has_target = false;
			Sleep(5);
			continue;
		}

		// Keybind check
		kb.key = settings::aimbot::triggerbot::keybind;
		kb.mode = static_cast<keybind::activation_mode>(settings::aimbot::triggerbot::activation_mode);
		if (kb.key != 0 && !keybind::is_active(kb))
		{
			mouse_fire_helper(0, false, 0, 0, 0);
			triggerbot::has_target = false;
			Sleep(5);
			continue;
		}

		triggerbot::active_source = 0;

		triggerbot_tick(
			0,
			settings::aimbot::triggerbot::enabled,
			settings::aimbot::triggerbot::reaction_ms,
			settings::aimbot::triggerbot::max_distance_enabled,
			settings::aimbot::triggerbot::max_distance,
			settings::aimbot::teamcheck,
			settings::aimbot::knock_check,
			settings::aimbot::health_check_enabled,
			settings::aimbot::min_health,
			settings::aimbot::triggerbot::wallcheck,
			false,
			0.0f,
			settings::aimbot::triggerbot::fire_mode,
			settings::aimbot::triggerbot::clicks_per_second,
			settings::aimbot::triggerbot::hold_duration
		);

		Sleep(5);
	}
}

void triggerbot::run_silentaim()
{
	keybind::keybind_t kb{};

	for (; runtime::alive(); )
	{
		if (!settings::silentaim::enabled || !settings::silentaim::triggerbot::enabled)
		{
			mouse_fire_helper(1, false, 0, 0, 0);
			triggerbot::has_target = false;
			Sleep(5);
			continue;
		}

		// Keybind check
		kb.key = settings::silentaim::triggerbot::keybind;
		kb.mode = static_cast<keybind::activation_mode>(settings::silentaim::triggerbot::activation_mode);
		if (kb.key != 0 && !keybind::is_active(kb))
		{
			mouse_fire_helper(1, false, 0, 0, 0);
			triggerbot::has_target = false;
			Sleep(5);
			continue;
		}

		triggerbot::active_source = 1;

		triggerbot_tick(
			1,
			settings::silentaim::triggerbot::enabled,
			settings::silentaim::triggerbot::reaction_ms,
			settings::silentaim::triggerbot::max_distance_enabled,
			settings::silentaim::triggerbot::max_distance,
			settings::silentaim::teamcheck,
			settings::silentaim::knock_check,
			settings::silentaim::health_check_enabled,
			settings::silentaim::min_health,
			settings::silentaim::triggerbot::wallcheck,
			settings::silentaim::use_fov,
			settings::silentaim::fov,
			settings::silentaim::triggerbot::fire_mode,
			settings::silentaim::triggerbot::clicks_per_second,
			settings::silentaim::triggerbot::hold_duration
		);

		Sleep(5);
	}
}
