#include "noclip.h"

#include <game/game.h>
#include <cache/cache.h>
#include <sdk/sdk.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <settings.h>

#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace {
	// Per-primitive saved flags so we can restore exactly what we modified.
	std::unordered_map<std::uint64_t, std::uint8_t> g_saved_flags;

	void restore_all_flags()
	{
		for (const auto& [prim, original] : g_saved_flags)
		{
			try
			{
				memory->write<std::uint8_t>(prim + Offsets::Primitive::Flags, original);
			}
			catch (...) {}
		}
		g_saved_flags.clear();
	}
}

void rage::noclip::run()
{
	bool was_enabled = false;

	while (runtime::alive())
	{
		try
		{
			if (!game::datamodel.address)
			{
				if (was_enabled)
				{
					restore_all_flags();
					was_enabled = false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			cache::entity_t local;
			{
				std::lock_guard<std::mutex> lock(cache::mtx);
				local = cache::cached_local_player;
			}

			if (!local.instance.address)
			{
				if (was_enabled)
				{
					restore_all_flags();
					was_enabled = false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			if (settings::rage::noclip)
			{
				was_enabled = true;

				for (const auto& part_pair : local.parts)
				{
					if (!part_pair.second.address)
						continue;

					rbx::part_t part = part_pair.second;
					std::uint64_t primitive_address = memory->read<std::uint64_t>(part.address + Offsets::BasePart::Primitive);
					if (!primitive_address)
						continue;

					try
					{
						std::uint8_t primitive_flags = memory->read<std::uint8_t>(primitive_address + Offsets::Primitive::Flags);

						// Remember the unmodified value once per primitive.
						if (g_saved_flags.find(primitive_address) == g_saved_flags.end())
							g_saved_flags[primitive_address] = primitive_flags;

						primitive_flags &= ~0x08;
						memory->write<std::uint8_t>(primitive_address + Offsets::Primitive::Flags, primitive_flags);
					}
					catch (const std::exception& e)
					{
						(void)e;
						continue;
					}
				}
			}
			else if (was_enabled)
			{
				restore_all_flags();
				was_enabled = false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		catch (const std::exception& e)
		{
			(void)e;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	restore_all_flags();
}
