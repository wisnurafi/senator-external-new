#include "thirdperson.h"

#include <cache/cache.h>
#include <game/game.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <runtime/runtime.h>
#include <settings.h>
#include <sdk/math/math.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <cmath>

namespace
{
	struct thirdperson_state_t
	{
		std::uint64_t camera{ 0 };
		bool has_camera_original{ false };
		std::int32_t original_camera_type{ 0 };
	};

	thirdperson_state_t g_state{};

	void reset_state()
	{
		g_state = {};
	}

	void restore_camera()
	{
		if (!g_state.camera || !g_state.has_camera_original)
			return;

		try
		{
			memory->write<std::int32_t>(g_state.camera + Offsets::Camera::CameraType, g_state.original_camera_type);
		}
		catch (...) {}

		g_state.has_camera_original = false;
	}

	void restore_all()
	{
		restore_camera();
		reset_state();
	}

	void capture_camera_original()
	{
		if (g_state.has_camera_original)
			return;

		if (!game::camera || game::camera == 0)
			return;

		g_state.camera = game::camera;
		g_state.original_camera_type = memory->read<std::int32_t>(game::camera + Offsets::Camera::CameraType);
		g_state.has_camera_original = true;
	}

	math::vector3 get_local_head_position()
	{
		cache::entity_t local{};
		{
			std::lock_guard<std::mutex> lock(cache::mtx);
			local = cache::cached_local_player;
		}

		auto head_it = local.parts.find("Head");
		if (head_it != local.parts.end() && head_it->second.address != 0)
		{
			try
			{
				return memory->read<math::vector3>(head_it->second.address + Offsets::BasePart::Primitive + Offsets::Primitive::Position);
			}
			catch (...) {}
		}

		auto root_it = local.parts.find("HumanoidRootPart");
		if (root_it != local.parts.end() && root_it->second.address != 0)
		{
			try
			{
				return memory->read<math::vector3>(root_it->second.address + Offsets::BasePart::Primitive + Offsets::Primitive::Position);
			}
			catch (...) {}
		}

		return { 0.0f, 0.0f, 0.0f };
	}
}

void rage::thirdperson::run()
{
	bool was_enabled = false;

	while (runtime::alive())
	{
		try
		{
			if (!game::datamodel.address || !settings::rage::thirdperson::enabled)
			{
				if (was_enabled)
				{
					restore_all();
					was_enabled = false;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			if (!game::camera || game::camera == 0)
			{
				if (was_enabled)
				{
					restore_all();
					was_enabled = false;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}

			capture_camera_original();
			was_enabled = true;

			const float distance = std::clamp(settings::rage::thirdperson::distance, 2.0f, 30.0f);
			const float height_offset = std::clamp(settings::rage::thirdperson::height_offset, -5.0f, 10.0f);

			// Set camera to Scriptable mode (1) — we want full control
			memory->write<std::int32_t>(game::camera + Offsets::Camera::CameraType, 1);

			// Get camera rotation & forward vector
			math::matrix3 cam_rot = memory->read<math::matrix3>(game::camera + Offsets::Camera::Rotation);
			math::vector3 cam_forward = cam_rot.forward();

			// Get local character head/root position
			math::vector3 head_pos = get_local_head_position();

			// Calculate camera position: head - (forward * distance) + (up * height_offset)
			math::vector3 cam_position = head_pos - (cam_forward * distance);
			cam_position.y += height_offset;

			// Keep camera rotation same as player camera
			math::matrix3 cam_rotation = cam_rot;

			// Write camera position & rotation directly (like CS2)
			memory->write<math::vector3>(game::camera + Offsets::Camera::Position, cam_position);
			memory->write<math::matrix3>(game::camera + Offsets::Camera::Rotation, cam_rotation);

			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
		catch (...)
		{
			if (was_enabled)
				restore_all();

			was_enabled = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	restore_all();
}
