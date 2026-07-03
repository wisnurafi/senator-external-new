#include "thirdperson.h"

#include <cache/cache.h>
#include <game/game.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <runtime/runtime.h>
#include <settings.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace
{
	struct thirdperson_state_t
	{
		std::uint64_t player{ 0 };
		std::uint64_t humanoid{ 0 };

		bool has_player_original{ false };
		bool has_humanoid_original{ false };

		std::int32_t camera_mode{ 0 };
		float min_zoom{ 0.0f };
		float max_zoom{ 0.0f };
		math::vector3 camera_offset{};
	};

	thirdperson_state_t g_state{};

	void reset_state()
	{
		g_state = {};
	}

	void restore_player()
	{
		if (!g_state.player || !g_state.has_player_original)
			return;

		try
		{
			memory->write<std::int32_t>(g_state.player + Offsets::Player::CameraMode, g_state.camera_mode);
			memory->write<float>(g_state.player + Offsets::Player::MinZoomDistance, g_state.min_zoom);
			memory->write<float>(g_state.player + Offsets::Player::MaxZoomDistance, g_state.max_zoom);
		}
		catch (...) {}

		g_state.has_player_original = false;
	}

	void restore_humanoid()
	{
		if (!g_state.humanoid || !g_state.has_humanoid_original)
			return;

		try
		{
			memory->write<math::vector3>(g_state.humanoid + Offsets::Humanoid::CameraOffset, g_state.camera_offset);
		}
		catch (...) {}

		g_state.has_humanoid_original = false;
	}

	void restore_all()
	{
		restore_player();
		restore_humanoid();
		reset_state();
	}

	void capture_player_original(std::uint64_t player)
	{
		if (g_state.player == player && g_state.has_player_original)
			return;

		restore_player();

		g_state.player = player;
		g_state.camera_mode = memory->read<std::int32_t>(player + Offsets::Player::CameraMode);
		g_state.min_zoom = memory->read<float>(player + Offsets::Player::MinZoomDistance);
		g_state.max_zoom = memory->read<float>(player + Offsets::Player::MaxZoomDistance);
		g_state.has_player_original = true;
	}

	void capture_humanoid_original(std::uint64_t humanoid)
	{
		if (g_state.humanoid == humanoid && g_state.has_humanoid_original)
			return;

		restore_humanoid();

		g_state.humanoid = humanoid;
		g_state.camera_offset = memory->read<math::vector3>(humanoid + Offsets::Humanoid::CameraOffset);
		g_state.has_humanoid_original = true;
	}

	std::uint64_t local_player_address()
	{
		if (!game::players.address)
			return 0;

		try
		{
			return memory->read<std::uint64_t>(game::players.address + Offsets::Player::LocalPlayer);
		}
		catch (...) {}

		return 0;
	}

	std::uint64_t local_humanoid_address()
	{
		cache::entity_t local{};
		{
			std::lock_guard<std::mutex> lock(cache::mtx);
			local = cache::cached_local_player;
		}

		return local.humanoid.address;
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

			const std::uint64_t player = local_player_address();
			if (!player)
			{
				if (was_enabled)
					restore_all();

				was_enabled = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}

			capture_player_original(player);
			was_enabled = true;

			const float distance = std::clamp(settings::rage::thirdperson::distance, 2.0f, 30.0f);
			const float max_distance = (std::max)(distance, distance + 0.1f);

			memory->write<std::int32_t>(player + Offsets::Player::CameraMode, 0);
			memory->write<float>(player + Offsets::Player::MinZoomDistance, distance);
			memory->write<float>(player + Offsets::Player::MaxZoomDistance, max_distance);

			const std::uint64_t humanoid = local_humanoid_address();
			if (humanoid)
			{
				capture_humanoid_original(humanoid);

				const float height = std::clamp(settings::rage::thirdperson::height_offset, -5.0f, 10.0f);
				memory->write<math::vector3>(humanoid + Offsets::Humanoid::CameraOffset, math::vector3{ 0.0f, height, 0.0f });
			}
			else
			{
				restore_humanoid();
				g_state.humanoid = 0;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(25));
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
