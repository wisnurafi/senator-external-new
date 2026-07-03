#include "cache.h"
#include "game_detection.h"
#include <thread>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <game/game.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <gamesupport/AnimeLeague/anime_league.h>
#include <gamesupport/MurderMystery2/murder_mystery_2.h>
#include <gamesupport/Overkill/overkill.h>
#include <gamesupport/PhantomForces/phantom_forces.h>
#include <settings.h>

// Roblox BrickColor ID -> RGB (0-1 float). Covers the most common team colors.
static bool brickcolor_to_rgb(int id, float out[3])
{
	switch (id)
	{
	case 1:   out[0]=0.949f; out[1]=0.953f; out[2]=0.953f; return true; // White
	case 5:   out[0]=0.843f; out[1]=0.773f; out[2]=0.604f; return true; // Brick yellow
	case 9:   out[0]=0.910f; out[1]=0.725f; out[2]=0.545f; return true; // Light orange
	case 11:  out[0]=0.498f; out[1]=0.557f; out[2]=0.392f; return true; // Pastel blue-green
	case 18:  out[0]=0.800f; out[1]=0.557f; out[2]=0.412f; return true; // Nougat
	case 21:  out[0]=0.769f; out[1]=0.157f; out[2]=0.110f; return true; // Bright red
	case 23:  out[0]=0.051f; out[1]=0.412f; out[2]=0.675f; return true; // Bright blue
	case 24:  out[0]=0.961f; out[1]=0.804f; out[2]=0.188f; return true; // Bright yellow
	case 26:  out[0]=0.106f; out[1]=0.165f; out[2]=0.208f; return true; // Black
	case 28:  out[0]=0.157f; out[1]=0.498f; out[2]=0.278f; return true; // Dark green
	case 29:  out[0]=0.631f; out[1]=0.769f; out[2]=0.549f; return true; // Medium green
	case 37:  out[0]=0.294f; out[1]=0.592f; out[2]=0.294f; return true; // Bright green
	case 38:  out[0]=0.627f; out[1]=0.373f; out[2]=0.208f; return true; // Dark orange
	case 45:  out[0]=0.678f; out[1]=0.737f; out[2]=0.835f; return true; // Light blue
	case 100: out[0]=0.933f; out[1]=0.573f; out[2]=0.573f; return true; // Light red
	case 101: out[0]=0.859f; out[1]=0.561f; out[2]=0.608f; return true; // Medium red
	case 102: out[0]=0.420f; out[1]=0.518f; out[2]=0.710f; return true; // Medium blue
	case 104: out[0]=0.420f; out[1]=0.294f; out[2]=0.502f; return true; // Bright violet
	case 105: out[0]=0.886f; out[1]=0.608f; out[2]=0.251f; return true; // Bright orange
	case 106: out[0]=0.851f; out[1]=0.522f; out[2]=0.255f; return true; // Bright orange (alt)
	case 107: out[0]=0.008f; out[1]=0.522f; out[2]=0.522f; return true; // Bright bluish green
	case 119: out[0]=0.643f; out[1]=0.741f; out[2]=0.278f; return true; // Br. yellowish green
	case 125: out[0]=0.914f; out[1]=0.722f; out[2]=0.573f; return true; // Light orange
	case 135: out[0]=0.455f; out[1]=0.525f; out[2]=0.616f; return true; // Sand blue
	case 141: out[0]=0.106f; out[1]=0.220f; out[2]=0.055f; return true; // Earth green
	case 151: out[0]=0.467f; out[1]=0.553f; out[2]=0.522f; return true; // Sand green
	case 153: out[0]=0.659f; out[1]=0.467f; out[2]=0.322f; return true; // Sand red
	case 192: out[0]=0.412f; out[1]=0.251f; out[2]=0.157f; return true; // Reddish brown
	case 194: out[0]=0.631f; out[1]=0.647f; out[2]=0.635f; return true; // Medium stone grey
	case 199: out[0]=0.388f; out[1]=0.373f; out[2]=0.384f; return true; // Dark stone grey
	case 208: out[0]=0.898f; out[1]=0.863f; out[2]=0.737f; return true; // Light stone grey
	case 226: out[0]=0.992f; out[1]=0.918f; out[2]=0.553f; return true; // Cool yellow
	case 1001: out[0]=0.992f; out[1]=0.992f; out[2]=0.992f; return true; // Institutional white
	case 1002: out[0]=0.016f; out[1]=0.016f; out[2]=0.016f; return true; // Really black
	case 1003: out[0]=1.000f; out[1]=0.000f; out[2]=0.000f; return true; // Really red
	case 1004: out[0]=1.000f; out[1]=0.690f; out[2]=0.000f; return true; // Deep orange
	case 1005: out[0]=0.000f; out[1]=0.400f; out[2]=0.800f; return true; // Really blue
	case 1006: out[0]=0.710f; out[1]=0.000f; out[2]=0.000f; return true; // Alder
	case 1007: out[0]=0.639f; out[1]=0.294f; out[2]=0.000f; return true; // Dusty Rose
	case 1008: out[0]=0.690f; out[1]=0.580f; out[2]=0.337f; return true; // Olive
	case 1009: out[0]=1.000f; out[1]=1.000f; out[2]=0.000f; return true; // New Yeller
	case 1010: out[0]=0.000f; out[1]=0.000f; out[2]=1.000f; return true; // Really blue
	case 1011: out[0]=0.004f; out[1]=0.004f; out[2]=0.004f; return true; // Really black (alt)
	case 1012: out[0]=0.067f; out[1]=0.067f; out[2]=1.000f; return true; // Deep blue
	case 1013: out[0]=0.016f; out[1]=0.686f; out[2]=0.925f; return true; // Toothpaste
	case 1014: out[0]=0.671f; out[1]=1.000f; out[2]=0.000f; return true; // Lime green
	case 1015: out[0]=0.863f; out[1]=0.086f; out[2]=0.235f; return true; // Crimson
	case 1016: out[0]=1.000f; out[1]=0.400f; out[2]=0.800f; return true; // Hot pink
	case 1017: out[0]=1.000f; out[1]=0.690f; out[2]=0.000f; return true; // Deep orange (alt)
	case 1018: out[0]=0.184f; out[1]=0.820f; out[2]=0.580f; return true; // Teal
	case 1019: out[0]=0.000f; out[1]=1.000f; out[2]=1.000f; return true; // Cyan
	case 1020: out[0]=0.518f; out[1]=0.000f; out[2]=0.678f; return true; // Royal purple
	case 1021: out[0]=0.678f; out[1]=0.000f; out[2]=1.000f; return true; // Neon orange (actually purple)
	case 1022: out[0]=0.459f; out[1]=0.000f; out[2]=0.459f; return true; // Magenta
	case 1023: out[0]=0.686f; out[1]=0.867f; out[2]=1.000f; return true; // Baby blue
	case 1024: out[0]=1.000f; out[1]=0.698f; out[2]=0.000f; return true; // Carnation pink (actually orange)
	case 1025: out[0]=1.000f; out[1]=0.600f; out[2]=0.000f; return true; // Gold
	case 1026: out[0]=0.702f; out[1]=0.533f; out[2]=1.000f; return true; // Lavender
	case 1027: out[0]=0.286f; out[1]=0.592f; out[2]=0.812f; return true; // Pastel blue
	case 1028: out[0]=0.671f; out[1]=0.953f; out[2]=0.400f; return true; // Pastel green
	case 1029: out[0]=1.000f; out[1]=1.000f; out[2]=0.596f; return true; // Pastel yellow
	case 1030: out[0]=1.000f; out[1]=0.596f; out[2]=0.863f; return true; // Pink
	case 1031: out[0]=0.749f; out[1]=0.000f; out[2]=0.000f; return true; // Maroon
	case 1032: out[0]=0.471f; out[1]=0.235f; out[2]=0.000f; return true; // Brown
	default:  return false;
	}
}

static bool is_part_like_class(const std::string& class_name)
{
	return class_name.find("Part") != std::string::npos ||
		class_name == "UnionOperation" ||
		class_name == "NegateOperation" ||
		class_name == "PartOperation";
}

static int count_part_like_descendants(const rbx::instance_t& root, int depth, int& visited)
{
	if (!root.address || depth < 0 || visited > 500)
		return 0;

	int count = 0;
	try
	{
		std::vector<rbx::instance_t> children = root.get_children();
		for (rbx::instance_t& child : children)
		{
			if (++visited > 500)
				return count;

			const std::string child_name = child.get_name();
			const std::string child_class = child.get_class_name();
			if (child_name == "ViewModel" || child_class == "Camera")
				continue;

			if (is_part_like_class(child_class))
				++count;

			if (depth > 0)
				count += count_part_like_descendants(child, depth - 1, visited);
		}
	}
	catch (...)
	{
	}

	return count;
}

static bool instance_has_character_parts(const rbx::instance_t& instance)
{
	if (!instance.address)
		return false;

	try
	{
		if (instance.find_first_child("HumanoidRootPart").address)
			return true;
		if (instance.find_first_child("RootPart").address)
			return true;
		if (instance.find_first_child("Torso").address)
			return true;
		if (instance.find_first_child("UpperTorso").address)
			return true;
		if (instance.find_first_child("Head").address)
			return true;
		if (instance.find_first_child_by_class("Humanoid").address)
			return true;

		int visited = 0;
		if (count_part_like_descendants(instance, 4, visited) >= 4)
			return true;
	}
	catch (...)
	{
	}

	return false;
}

static rbx::model_instance_t find_character_model_by_name(
	const rbx::instance_t& root,
	const std::string& name,
	const std::string& display_name,
	int depth,
	int& visited)
{
	if (!root.address || depth < 0 || visited > 700)
		return {};

	try
	{
		std::vector<rbx::instance_t> children = root.get_children();
		for (rbx::instance_t& child : children)
		{
			if (++visited > 700)
				return {};

			const std::string child_name = child.get_name();
			const std::string child_class = child.get_class_name();
			const bool name_matches =
				(!name.empty() && child_name == name) ||
				(!display_name.empty() && child_name == display_name);

			if (name_matches && child_class == "Model" && instance_has_character_parts(child))
				return { child.address };

			if (depth > 0)
			{
				rbx::model_instance_t nested = find_character_model_by_name(child, name, display_name, depth - 1, visited);
				if (nested.address)
					return nested;
			}
		}
	}
	catch (...)
	{
	}

	return {};
}

static rbx::model_instance_t resolve_player_model(rbx::player_t& player, const std::string& name, const std::string& display_name)
{
	try
	{
		rbx::model_instance_t model = player.get_model_instance();
		if (model.address && instance_has_character_parts(model))
			return model;
	}
	catch (...)
	{
	}

	int visited = 0;
	return find_character_model_by_name(game::workspace, name, display_name, 5, visited);
}

static void populate_entity_from_model_recursive(cache::entity_t& entity, const rbx::instance_t& root, int depth, int& visited)
{
	if (!root.address || depth < 0 || visited > 1200)
		return;

	try
	{
		std::vector<rbx::instance_t> children = root.get_children();
		for (rbx::instance_t& child : children)
		{
			if (++visited > 1200)
				return;

			std::string child_name = child.get_name();
			std::string child_class = child.get_class_name();

			if (is_part_like_class(child_class))
			{
				if (child_name.empty())
					child_name = child_class + "_" + std::to_string(child.address);
				entity.parts[child_name] = rbx::part_t(child.address);
			}
			else if (child_class == "Humanoid")
			{
				entity.humanoid = { child.address };
			}
			else if (child_class == "Tool")
			{
				entity.tool_name = child_name;
			}

			if (depth > 0)
				populate_entity_from_model_recursive(entity, child, depth - 1, visited);
		}
	}
	catch (...)
	{
	}
}

static void populate_entity_from_model(cache::entity_t& entity, const rbx::model_instance_t& model)
{
	if (!model.address)
		return;

	try
	{
		int visited = 0;
		populate_entity_from_model_recursive(entity, { model.address }, 6, visited);

		if (entity.humanoid.address != 0)
		{
			entity.rig_type = entity.humanoid.get_rig_type();
			entity.health = memory->read<float>(entity.humanoid.address + Offsets::Humanoid::Health);
			entity.max_health = memory->read<float>(entity.humanoid.address + Offsets::Humanoid::MaxHealth);
		}
		else
		{
			entity.health = 100.0f;
			entity.max_health = 100.0f;
		}
	}
	catch (...)
	{
	}
}

static bool entity_has_usable_parts(const cache::entity_t& entity)
{
	return entity.parts.find("HumanoidRootPart") != entity.parts.end() ||
		entity.parts.find("RootPart") != entity.parts.end() ||
		entity.parts.find("Head") != entity.parts.end() ||
		entity.parts.find("UpperTorso") != entity.parts.end() ||
		entity.parts.find("Torso") != entity.parts.end();
}

static bool cache_has_usable_parts(const std::vector<cache::entity_t>& entities)
{
	for (const auto& entity : entities)
	{
		if (entity_has_usable_parts(entity))
			return true;
	}
	return false;
}

static void append_unique_entities(std::vector<cache::entity_t>& target, const std::vector<cache::entity_t>& source)
{
	for (const auto& entity : source)
	{
		if (!entity.instance.address)
			continue;

		bool exists = false;
		for (const auto& existing : target)
		{
			if (existing.instance.address == entity.instance.address)
			{
				exists = true;
				break;
			}
		}

		if (!exists)
			target.push_back(entity);
	}
}

static void collect_workspace_character_models(
	const rbx::instance_t& root,
	std::vector<cache::entity_t>& out,
	cache::entity_t& local_entity,
	const std::string& local_name,
	const std::string& local_display_name,
	int depth,
	int& visited)
{
	if (!root.address || depth < 0 || visited > 1200)
		return;

	try
	{
		std::vector<rbx::instance_t> children = root.get_children();
		for (rbx::instance_t& child : children)
		{
			if (++visited > 1200)
				return;

			const std::string child_name = child.get_name();
			const std::string child_class = child.get_class_name();
			if (child_name == "ViewModel" || child_class == "Camera")
				continue;

			if (child_class == "Model" && instance_has_character_parts(child))
			{
				cache::entity_t entity{};
				entity.instance = { child.address };
				entity.name = child_name;
				entity.display_name = entity.name;

				populate_entity_from_model(entity, { child.address });
				if (entity_has_usable_parts(entity))
				{
					const bool is_local =
						(!local_name.empty() && entity.name == local_name) ||
						(!local_display_name.empty() && entity.name == local_display_name);

					if (is_local)
					{
						local_entity = entity;
						game::local_character = { child.address };
					}
					else
					{
						out.push_back(entity);
					}
				}

				continue;
			}

			if (depth > 0)
				collect_workspace_character_models(child, out, local_entity, local_name, local_display_name, depth - 1, visited);
		}
	}
	catch (...)
	{
	}
}

void cache::run()
{
	while (runtime::alive())
	{
		std::vector<cache::entity_t> temp_cache;

		rbx::instance_t workspace = game::datamodel.find_first_child_by_class("Workspace");
		if (workspace.address != 0)
		{
			game::workspace = workspace;
			game::camera = memory->read<std::uint64_t>(workspace.address + Offsets::Workspace::CurrentCamera);
		}

		cache::entity_t local_entity{};

		bool used_custom_cache = false;
		const auto detection_result = cache::refresh_game_detection();

		// Auto-switch to Silent Aim when entering PF
		if (detection_result.entered_phantom_forces)
			settings::aimbot::mode = 2;

		if (game::is_phantom_forces)
		{
			if (!game::players.address || !game::workspace.address)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			gamesupport::phantom_forces::collect_entities(
				game::workspace,
				game::players,
				local_entity,
				temp_cache,
				brickcolor_to_rgb);

			used_custom_cache = true;
		}

		if (game::is_overkill && !used_custom_cache)
		{
			if (game::workspace.address)
				gamesupport::overkill::collect_entities(game::workspace, temp_cache);
		}

		if (!used_custom_cache)
		{
			// Re-resolve players if address is stale (teleport/rejoin)
			if (!game::players.address && game::datamodel.address)
			{
				game::players = game::datamodel.find_first_child_by_class("Players");
				if (game::players.address)
				{
					game::local_player = memory->read<rbx::instance_t>(game::players.address + Offsets::Player::LocalPlayer);
					if (game::local_player.address)
					{
						rbx::player_t lp_obj = { game::local_player.address };
						game::local_character = { resolve_player_model(lp_obj, lp_obj.get_name(), lp_obj.get_display_name()).address };
					}
				}
			}

			if (!game::players.address)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			std::vector<rbx::player_t> players = game::players.get_children<rbx::player_t>();

			// If players list is empty, datamodel might have changed ??? re-resolve
			if (players.empty() && game::datamodel.address)
			{
				game::players = game::datamodel.find_first_child_by_class("Players");
				if (game::players.address)
					players = game::players.get_children<rbx::player_t>();
			}

			rbx::player_t local_player_obj{ memory->read<std::uint64_t>(game::players.address + Offsets::Player::LocalPlayer) };

			for (rbx::player_t& player : players)
			{
				cache::entity_t entity{};

				entity.instance = { player.address };
				entity.name = player.get_name();
				entity.display_name = player.get_display_name();
				entity.team = memory->read<std::uint64_t>(player.address + Offsets::Player::Team);

				// Read team color from Team instance's BrickColor
				if (entity.team != 0 && entity.team != 0xFFFFFFFFFFFFFFFF)
				{
					try {
						int brick_color_id = memory->read<int>(entity.team + Offsets::Team::BrickColor);
						entity.has_team_color = brickcolor_to_rgb(brick_color_id, entity.team_color);
					} catch (...) {}
				}

				rbx::model_instance_t model_instance = resolve_player_model(player, entity.name, entity.display_name);

				if (model_instance.address != 0)
				{
					std::vector<rbx::instance_t> children = model_instance.get_children();
					for (rbx::instance_t& child : children)
					{
						std::string child_name = child.get_name();
						std::string child_class = child.get_class_name();

						if (child_class.find("Part") != std::string::npos)
						{
							entity.parts[child_name] = rbx::part_t(child.address);
						}
						else if (child_class == "Humanoid")
						{
							entity.humanoid = { child.address };
						}
						else if (child_class == "Tool")
						{
							entity.tool_name = child_name;
						}
						else if (child_name == "BodyEffects")
						{
							static std::vector<const char*> armor_candidates = { "Armor", "Armour", "Defense", "Defence" };
							for (const char* nm : armor_candidates)
							{
								rbx::instance_t armor_node = child.find_first_child(nm);
								if (armor_node.address == 0)
									continue;

								try {
									entity.armor_value = memory->read<int>(armor_node.address + Offsets::Misc::Value);
								} catch (...) {
									try {
										entity.armor_value = static_cast<int>(memory->read<double>(armor_node.address + Offsets::Misc::Value));
									} catch (...) {}
								}
								if (entity.armor_value >= 0) {
									entity.armor_percent = std::clamp(entity.armor_value / 100.0f, 0.0f, 1.0f);
									break;
								}
							}
						}
					}
				}

				if (model_instance.address != 0 && !entity_has_usable_parts(entity))
				{
					populate_entity_from_model(entity, model_instance);
				}

				if (entity.humanoid.address != 0)
				{
					entity.rig_type = entity.humanoid.get_rig_type();
					entity.health = memory->read<float>(entity.humanoid.address + Offsets::Humanoid::Health);
					entity.max_health = memory->read<float>(entity.humanoid.address + Offsets::Humanoid::MaxHealth);
				}

				if (game::is_murder_mystery_2 && settings::visuals::mm2_esp)
					gamesupport::murder_mystery_2::apply_role(entity, player);

				if (local_player_obj.address != 0 && player.address == local_player_obj.address)
				{
					local_entity = entity;
					game::local_character = { model_instance.address };
				}

				if (!(game::is_overkill && (gamesupport::overkill::is_excluded_entity_name(entity.name) || gamesupport::overkill::is_excluded_entity_name(entity.display_name))))
					temp_cache.push_back(entity);
			}

			if (((game::is_anime_league && gamesupport::anime_league::should_use_workspace_fallback(cache_has_usable_parts(temp_cache))) || game::is_overkill) && game::workspace.address)
			{
				std::string local_name;
				std::string local_display_name;
				try
				{
					if (local_player_obj.address)
					{
						local_name = local_player_obj.get_name();
						local_display_name = local_player_obj.get_display_name();
					}
				}
				catch (...)
				{
				}

				if (game::is_overkill)
				{
					std::vector<cache::entity_t> overkill_cache;
					if (gamesupport::overkill::collect_entities(game::workspace, overkill_cache))
					{
						append_unique_entities(temp_cache, overkill_cache);
						local_entity = {};
					}

					std::vector<cache::entity_t> workspace_cache;
					cache::entity_t workspace_local{};
					int visited = 0;
					collect_workspace_character_models(game::workspace, workspace_cache, workspace_local, local_name, local_display_name, 6, visited);
					append_unique_entities(temp_cache, workspace_cache);
					if (workspace_local.instance.address)
					{
						local_entity = workspace_local;
					}
				}
				else
				{
					temp_cache.clear();
					local_entity = {};
					int visited = 0;
					collect_workspace_character_models(game::workspace, temp_cache, local_entity, local_name, local_display_name, gamesupport::anime_league::workspace_scan_depth(), visited);
				}
			}
		}

		{
			std::lock_guard<std::mutex> lock(mtx);
			cached_players = std::move(temp_cache);
			// For PF: only persist old local player when cache is empty (dead/loading).
			// If cache has entities but local wasn't found, reset ??? teams may have changed.
			if (local_entity.instance.address != 0)
				cached_local_player = local_entity;
			else if (!used_custom_cache || !cached_players.empty())
				cached_local_player = local_entity;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

