#include "hitbox_expander.h"

#include <game/game.h>
#include <cache/cache.h>
#include <cache/bodyparts/bodyparts.h>
#include <sdk/sdk.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <settings.h>

#include <thread>
#include <chrono>
#include <mutex>
#include <initializer_list>

static bool is_player_knocked(const cache::entity_t& player)
{
	if (game::is_overkill)
		return false;

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

struct saved_primitive_t
{
	std::uint64_t prim_addr;
	math::vector3 original_size;
	std::uint8_t original_flags;
};

static std::vector<saved_primitive_t> expanded_parts;

static void restore_all()
{
	for (const auto& s : expanded_parts)
	{
		try
		{
			memory->write<math::vector3>(s.prim_addr + Offsets::Primitive::Size, s.original_size);
			memory->write<std::uint8_t>(s.prim_addr + Offsets::Primitive::Flags, s.original_flags);
		}
		catch (...) {}
	}
	expanded_parts.clear();
}

void rage::hitbox_expander::run()
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
					restore_all();
					was_enabled = false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			if (!settings::rage::hitbox_expander::enabled)
			{
				if (was_enabled)
				{
					restore_all();
					was_enabled = false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			was_enabled = true;

			if (settings::rage::hitbox_expander::size_x <= 0.0f ||
				settings::rage::hitbox_expander::size_y <= 0.0f ||
				settings::rage::hitbox_expander::size_z <= 0.0f)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			std::vector<cache::entity_t> snapshot;
			std::uint64_t local_address = 0;
			std::string local_name;
			std::uint64_t local_target_address = 0;
			std::uint64_t local_primitive_address = 0;
			{
				std::lock_guard<std::mutex> lock(cache::mtx);
				snapshot = cache::cached_players;
				local_address = cache::cached_local_player.instance.address;
				local_name = cache::cached_local_player.name;

				auto local_part_it = cache::cached_local_player.parts.find("HumanoidRootPart");
				if (local_part_it != cache::cached_local_player.parts.end() && local_part_it->second.address != 0)
				{
					local_target_address = local_part_it->second.address;
					local_primitive_address = memory->read<std::uint64_t>(local_target_address + Offsets::BasePart::Primitive);
				}
			}

			// build set of current player primitive addresses for tracking
			std::vector<saved_primitive_t> new_expanded;

			for (auto& entity : snapshot)
			{
				if (!entity.instance.address)
					continue;

				if ((local_address != 0 && entity.instance.address == local_address) ||
					(!local_name.empty() && entity.name == local_name))
					continue;

				if (settings::rage::hitbox_expander::knock_check && is_player_knocked(entity))
					continue;

				math::vector3 ignored_position{};
				if (!bodyparts::get_part_position(entity, "HumanoidRootPart", ignored_position))
					continue;

				rbx::part_t target_part{};
				for (const char* part_name : { "HumanoidRootPart", "RootPart", "Torso", "UpperTorso", "LowerTorso", "Body", "Chest", "Hips" })
				{
					auto part_it = entity.parts.find(part_name);
					if (part_it != entity.parts.end() && part_it->second.address)
					{
						target_part = part_it->second;
						break;
					}
				}

				if (!target_part.address)
					continue;

				if (local_target_address != 0 && target_part.address == local_target_address)
					continue;

				std::uint64_t primitive_address = memory->read<std::uint64_t>(target_part.address + Offsets::BasePart::Primitive);
				if (!primitive_address)
					continue;

				if (local_primitive_address != 0 && primitive_address == local_primitive_address)
					continue;

				try
				{
					// check if we already have this primitive saved
					bool already_saved = false;
					for (const auto& s : expanded_parts)
					{
						if (s.prim_addr == primitive_address)
						{
							already_saved = true;
							new_expanded.push_back(s);
							break;
						}
					}

					if (!already_saved)
					{
						// save original size and flags before expanding
						math::vector3 orig_size = memory->read<math::vector3>(primitive_address + Offsets::Primitive::Size);
						std::uint8_t orig_flags = memory->read<std::uint8_t>(primitive_address + Offsets::Primitive::Flags);
						new_expanded.push_back({ primitive_address, orig_size, orig_flags });
					}

					math::vector3 new_size = {
						settings::rage::hitbox_expander::size_x,
						settings::rage::hitbox_expander::size_y,
						settings::rage::hitbox_expander::size_z
					};

					memory->write<math::vector3>(primitive_address + Offsets::Primitive::Size, new_size);

					std::uint8_t primitive_flags = memory->read<std::uint8_t>(primitive_address + Offsets::Primitive::Flags);
					primitive_flags &= ~0x08;
					memory->write<std::uint8_t>(primitive_address + Offsets::Primitive::Flags, primitive_flags);
				}
				catch (...)
				{
					continue;
				}
			}

			// restore any primitives that are no longer in current player list
			for (const auto& old : expanded_parts)
			{
				bool still_active = false;
				for (const auto& cur : new_expanded)
				{
					if (cur.prim_addr == old.prim_addr)
					{
						still_active = true;
						break;
					}
				}
				if (!still_active)
				{
					try
					{
						memory->write<math::vector3>(old.prim_addr + Offsets::Primitive::Size, old.original_size);
						memory->write<std::uint8_t>(old.prim_addr + Offsets::Primitive::Flags, old.original_flags);
					}
					catch (...) {}
				}
			}

			expanded_parts = new_expanded;

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		catch (const std::exception& e)
		{
			(void)e;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}
