#include "skybox.h"

#include <thread>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>

#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <sdk/sdk.h>
#include <sdk/math/math.h>
#include <game/game.h>
#include <runtime/runtime.h>
#include <runtime/runtime_log.h>
#include <settings.h>

namespace lighting
{
	namespace skybox
	{
		namespace
		{
			struct skybox_faces_t
			{
				const char* back;
				const char* down;
				const char* front;
				const char* left;
				const char* right;
				const char* up;
			};

			struct owned_skybox_faces_t
			{
				std::string back;
				std::string down;
				std::string front;
				std::string left;
				std::string right;
				std::string up;
			};

			struct skybox_preset_t
			{
				const char* name;
				skybox_faces_t faces;
			};

			constexpr skybox_preset_t k_presets[] = {
				{
					"Nebula",
					{
						"rbxassetid://159454299",
						"rbxassetid://159454296",
						"rbxassetid://159454293",
						"rbxassetid://159454286",
						"rbxassetid://159454300",
						"rbxassetid://159454288"
					}
				},
				{
					"Vaporwave",
					{
						"rbxassetid://1417494030",
						"rbxassetid://1417494146",
						"rbxassetid://1417494253",
						"rbxassetid://1417494402",
						"rbxassetid://1417494499",
						"rbxassetid://1417494643"
					}
				},
				{
					"Night",
					{
						"rbxassetid://12064107",
						"rbxassetid://12064152",
						"rbxassetid://12064121",
						"rbxassetid://12063984",
						"rbxassetid://12064115",
						"rbxassetid://12064131"
					}
				},
				{
					"Sunset",
					{
						"rbxassetid://150939022",
						"rbxassetid://150939038",
						"rbxassetid://150939047",
						"rbxassetid://150939056",
						"rbxassetid://150939063",
						"rbxassetid://150939082"
					}
				}
			};

			struct captured_skybox_t
			{
				std::uint64_t sky{};
				owned_skybox_faces_t faces{};
				math::vector3 orientation{};
				bool captured{};
			};

			captured_skybox_t g_original{};
			int g_last_preset_index = -1;
			int g_force_refresh_ticks = 0;
			bool g_was_enabled = false;
			std::uint64_t g_last_logged_sky = 0;

			rbx::instance_t find_lighting()
			{
				if (game::datamodel.address == 0)
					return {};

				rbx::instance_t lighting = game::datamodel.find_first_child_by_class("Lighting");
				if (lighting.address == 0)
					lighting = game::datamodel.find_first_child("Lighting");

				return lighting;
			}

			rbx::instance_t find_sky(const rbx::instance_t& lighting)
			{
				if (lighting.address == 0)
					return {};

				try
				{
					const std::uint64_t sky = memory->read<std::uint64_t>(lighting.address + Offsets::Lighting::Sky);
					if (sky != 0)
						return rbx::instance_t(sky);
				}
				catch (...)
				{
				}

				rbx::instance_t sky = lighting.find_first_child_by_class("Sky");
				if (sky.address == 0)
					sky = lighting.find_first_child("Sky");

				return sky;
			}

			std::string class_name_or_unknown(const rbx::instance_t& instance)
			{
				try
				{
					const std::string class_name = instance.get_class_name();
					return class_name.empty() ? "unknown" : class_name;
				}
				catch (...)
				{
					return "unknown";
				}
			}

			std::string hex_address(std::uint64_t address)
			{
				std::ostringstream ss;
				ss << "0x" << std::hex << address;
				return ss.str();
			}

			void write_content_id(std::uint64_t address, const std::string& value)
			{
				const std::int32_t new_length = static_cast<std::int32_t>(value.length());
				if (new_length <= 0)
				{
					memory->write<std::int32_t>(address + 0x10, 0);
					return;
				}

				if (new_length < 16)
				{
					memory->write<std::int32_t>(address + 0x10, new_length);
					memory->write_buffer(address, value.c_str(), static_cast<std::size_t>(new_length + 1));
					return;
				}

				try
				{
					const std::int32_t current_length = memory->read<std::int32_t>(address + 0x10);
					const std::int32_t current_capacity = memory->read<std::int32_t>(address + 0x18);
					const std::uint64_t current_buffer = current_length >= 16 ? memory->read<std::uint64_t>(address) : 0;

					if (current_buffer != 0 && current_capacity >= new_length)
					{
						memory->write<std::int32_t>(address + 0x10, new_length);
						memory->write_buffer(current_buffer, value.c_str(), static_cast<std::size_t>(new_length + 1));
						return;
					}
				}
				catch (...)
				{
				}

				void* remote_buffer = VirtualAllocEx(
					memory->get_process_handle(),
					nullptr,
					static_cast<SIZE_T>(new_length + 1),
					MEM_COMMIT | MEM_RESERVE,
					PAGE_READWRITE);
				if (remote_buffer == nullptr)
					return;

				const std::uint64_t remote_address = reinterpret_cast<std::uint64_t>(remote_buffer);
				memory->write_buffer(remote_address, value.c_str(), static_cast<std::size_t>(new_length + 1));
				memory->write<std::uint64_t>(address, remote_address);
				memory->write<std::int32_t>(address + 0x10, new_length);
				memory->write<std::int32_t>(address + 0x18, new_length);
			}

			owned_skybox_faces_t read_faces(std::uint64_t sky)
			{
				return {
					memory->read_string(sky + Offsets::Sky::SkyboxBk),
					memory->read_string(sky + Offsets::Sky::SkyboxDn),
					memory->read_string(sky + Offsets::Sky::SkyboxFt),
					memory->read_string(sky + Offsets::Sky::SkyboxLf),
					memory->read_string(sky + Offsets::Sky::SkyboxRt),
					memory->read_string(sky + Offsets::Sky::SkyboxUp)
				};
			}

			void write_faces(std::uint64_t sky, const skybox_faces_t& faces)
			{
				write_content_id(sky + Offsets::Sky::SkyboxBk, faces.back);
				write_content_id(sky + Offsets::Sky::SkyboxDn, faces.down);
				write_content_id(sky + Offsets::Sky::SkyboxFt, faces.front);
				write_content_id(sky + Offsets::Sky::SkyboxLf, faces.left);
				write_content_id(sky + Offsets::Sky::SkyboxRt, faces.right);
				write_content_id(sky + Offsets::Sky::SkyboxUp, faces.up);
			}

			void write_faces(std::uint64_t sky, const owned_skybox_faces_t& faces)
			{
				write_content_id(sky + Offsets::Sky::SkyboxBk, faces.back);
				write_content_id(sky + Offsets::Sky::SkyboxDn, faces.down);
				write_content_id(sky + Offsets::Sky::SkyboxFt, faces.front);
				write_content_id(sky + Offsets::Sky::SkyboxLf, faces.left);
				write_content_id(sky + Offsets::Sky::SkyboxRt, faces.right);
				write_content_id(sky + Offsets::Sky::SkyboxUp, faces.up);
			}

			std::vector<std::uint64_t> render_views()
			{
				std::vector<std::uint64_t> views;

				auto push_unique = [&views](std::uint64_t value)
					{
						if (value == 0)
							return;

						for (const std::uint64_t existing : views)
						{
							if (existing == value)
								return;
						}

						views.push_back(value);
					};

				try
				{
					if (game::visengine.address != 0)
					{
						const std::uint64_t rv = memory->read<std::uint64_t>(game::visengine.address + Offsets::VisualEngine::RenderView);
						push_unique(rv);
					}

					if (game::datamodel.address != 0)
					{
						const std::uint64_t first = memory->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::ToRenderView1);
						const std::uint64_t second = first ? memory->read<std::uint64_t>(first + Offsets::DataModel::ToRenderView2) : 0;
						push_unique(second ? memory->read<std::uint64_t>(second + Offsets::DataModel::ToRenderView3) : 0);
					}
				}
				catch (...)
				{
				}

				return views;
			}

			std::string render_view_summary()
			{
				const std::vector<std::uint64_t> views = render_views();
				std::ostringstream ss;
				ss << "[";
				for (std::size_t i = 0; i < views.size(); ++i)
				{
					if (i > 0)
						ss << ", ";
					ss << hex_address(views[i]);
				}
				ss << "]";
				return ss.str();
			}

			void invalidate_sky()
			{
				for (const std::uint64_t rv : render_views())
				{
					try
					{
						memory->write<std::uint8_t>(rv + Offsets::RenderView::LightingValid, 0);
						memory->write<std::uint8_t>(rv + Offsets::RenderView::SkyValid, 0);
					}
					catch (...)
					{
					}
				}
			}

			void nudge_orientation(std::uint64_t sky)
			{
				try
				{
					math::vector3 orientation = memory->read<math::vector3>(sky + Offsets::Sky::SkyboxOrientation);
					orientation.y += 0.01f;
					memory->write<math::vector3>(sky + Offsets::Sky::SkyboxOrientation, orientation);
				}
				catch (...)
				{
				}
			}

			void pulse_lighting_sky(std::uint64_t lighting, std::uint64_t sky)
			{
				if (lighting == 0 || sky == 0)
					return;

				try
				{
					memory->write<std::uint64_t>(lighting + Offsets::Lighting::Sky, 0);
					invalidate_sky();
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					memory->write<std::uint64_t>(lighting + Offsets::Lighting::Sky, sky);
					invalidate_sky();
					runtime_log::info("Skybox", "Pulsed Lighting::Sky pointer to force renderer refresh.");
				}
				catch (...)
				{
				}
			}

			void skybox_thread()
			{
				while (runtime::alive())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(250));

					try
					{
						const rbx::instance_t lighting = find_lighting();
						const rbx::instance_t sky = find_sky(lighting);
						if (sky.address == 0)
							continue;

						if (settings::lighting::skybox::enabled && g_last_logged_sky != sky.address)
						{
							std::ostringstream ss;
							ss << "Sky candidate: lighting=" << hex_address(lighting.address)
								<< " sky=" << hex_address(sky.address)
								<< " class=" << class_name_or_unknown(sky)
								<< " bk='" << memory->read_string(sky.address + Offsets::Sky::SkyboxBk) << "'";
							runtime_log::info("Skybox", ss.str());
							g_last_logged_sky = sky.address;
						}

						if (!settings::lighting::skybox::enabled)
						{
							if (g_was_enabled && g_original.captured && g_original.sky == sky.address)
							{
								write_faces(sky.address, g_original.faces);
								memory->write<math::vector3>(sky.address + Offsets::Sky::SkyboxOrientation, g_original.orientation);
								pulse_lighting_sky(lighting.address, sky.address);
								g_force_refresh_ticks = 20;
								invalidate_sky();
							}

							g_original = {};
							g_last_preset_index = -1;
							g_force_refresh_ticks = 0;
							g_was_enabled = false;
							g_last_logged_sky = 0;
							continue;
						}

						const int preset_count = static_cast<int>(preset_names().size());
						if (settings::lighting::skybox::preset_index < 0)
							settings::lighting::skybox::preset_index = 0;
						if (settings::lighting::skybox::preset_index >= preset_count)
							settings::lighting::skybox::preset_index = preset_count - 1;

						if (!g_original.captured || g_original.sky != sky.address)
						{
							g_original.sky = sky.address;
							g_original.faces = read_faces(sky.address);
							g_original.orientation = memory->read<math::vector3>(sky.address + Offsets::Sky::SkyboxOrientation);
							g_original.captured = true;
							g_last_preset_index = -1;
						}

						if (!g_was_enabled || g_last_preset_index != settings::lighting::skybox::preset_index)
						{
							write_faces(sky.address, k_presets[settings::lighting::skybox::preset_index].faces);
							nudge_orientation(sky.address);
							pulse_lighting_sky(lighting.address, sky.address);
							invalidate_sky();
							g_force_refresh_ticks = 20;
							const std::string readback = memory->read_string(sky.address + Offsets::Sky::SkyboxBk);
							std::ostringstream ss;
							ss << "Applied preset '" << k_presets[settings::lighting::skybox::preset_index].name
								<< "' readback_bk='" << readback
								<< "' render_views=" << render_view_summary();
							runtime_log::info("Skybox", ss.str());
							g_last_preset_index = settings::lighting::skybox::preset_index;
							g_was_enabled = true;
						}

						if (g_force_refresh_ticks > 0)
						{
							invalidate_sky();
							--g_force_refresh_ticks;
						}
					}
					catch (...)
					{
					}
				}
			}
		}

		const std::vector<const char*>& preset_names()
		{
			static const std::vector<const char*> names = [] {
				std::vector<const char*> result;
				constexpr int preset_count = static_cast<int>(sizeof(k_presets) / sizeof(k_presets[0]));
				result.reserve(preset_count);
				for (int i = 0; i < preset_count; ++i)
					result.push_back(k_presets[i].name);
				return result;
			}();

			return names;
		}

		void run()
		{
			static bool initialized = false;
			if (!initialized)
			{
				std::thread(skybox_thread).detach();
				initialized = true;
			}
		}
	}
}
