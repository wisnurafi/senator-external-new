#include "hipheight.h"

#include <game/game.h>
#include <cache/cache.h>
#include <sdk/sdk.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <settings.h>

#include <thread>
#include <chrono>
#include <mutex>

void rage::hipheight::run()
{
	bool was_enabled = false;
	float original_height = 0.f;
	bool has_original = false;
	std::uint64_t saved_humanoid = 0;

	while (runtime::alive())
	{
		try
		{
			if (!game::datamodel.address)
			{
				was_enabled = false;
				has_original = false;
				saved_humanoid = 0;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			cache::entity_t local{};
			{
				std::lock_guard<std::mutex> lock(cache::mtx);
				local = cache::cached_local_player;
			}

			if (!local.humanoid.address)
			{
				was_enabled = false;
				has_original = false;
				saved_humanoid = 0;
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			// if humanoid changed (respawn), re-capture original
			if (local.humanoid.address != saved_humanoid)
			{
				has_original = false;
				saved_humanoid = local.humanoid.address;
			}

			if (settings::rage::hipheight::enabled)
			{
				// save original before first write
				if (!has_original)
				{
					original_height = memory->read<float>(local.humanoid.address + Offsets::Humanoid::HipHeight);
					has_original = true;
				}

				was_enabled = true;
				const float height = settings::rage::hipheight::height;

				for (int i = 0; i < 1500; ++i)
				{
					memory->write<float>(local.humanoid.address + Offsets::Humanoid::HipHeight, height);
				}
			}
			else if (was_enabled && has_original)
			{
				// restore original hip height
				memory->write<float>(local.humanoid.address + Offsets::Humanoid::HipHeight, original_height);
				was_enabled = false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		catch (...)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

