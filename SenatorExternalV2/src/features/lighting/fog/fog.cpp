#include "fog.h"

#include <thread>
#include <chrono>

#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <sdk/math/math.h>
#include <game/game.h>
#include <settings.h>

namespace lighting
{
	namespace fog
	{
		static bool fog_captured = false;
		static float old_fog_start, old_fog_end;
		static math::vector3 old_fog_color;

		static void fog_thread()
		{
			while (runtime::alive())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

				try
				{
					if (game::datamodel.address == 0) continue;

					auto lighting = game::datamodel.find_first_child_by_class("Lighting");
					if (lighting.address == 0) continue;

					if (!settings::lighting::fog::enabled)
					{
						if (fog_captured)
						{
							memory->write<float>(lighting.address + Offsets::Lighting::FogStart, old_fog_start);
							memory->write<float>(lighting.address + Offsets::Lighting::FogEnd, old_fog_end);
							memory->write<math::vector3>(lighting.address + Offsets::Lighting::FogColor, old_fog_color);
							fog_captured = false;
						}
						continue;
					}

					if (!fog_captured)
					{
						old_fog_start = memory->read<float>(lighting.address + Offsets::Lighting::FogStart);
						old_fog_end = memory->read<float>(lighting.address + Offsets::Lighting::FogEnd);
						old_fog_color = memory->read<math::vector3>(lighting.address + Offsets::Lighting::FogColor);
						fog_captured = true;
					}

					memory->write<float>(lighting.address + Offsets::Lighting::FogStart, settings::lighting::fog::fog_start);
					memory->write<float>(lighting.address + Offsets::Lighting::FogEnd, settings::lighting::fog::fog_end);
					memory->write<math::vector3>(lighting.address + Offsets::Lighting::FogColor,
						math::vector3{ settings::lighting::fog::fog_r, settings::lighting::fog::fog_g, settings::lighting::fog::fog_b });
				}
				catch (...) {}
			}
		}

		void run()
		{
			static bool initialized = false;
			if (!initialized)
			{
				std::thread(fog_thread).detach();
				initialized = true;
			}
		}
	}
}
