#include "phantom_forces.h"

#include <string>
#include <unordered_map>

#include <Offsets/Offsets.hpp>
#include <memory/memory.h>

namespace gamesupport::phantom_forces
{
	bool matches(std::uint64_t current_game_id, std::uint64_t place_id)
	{
		(void)place_id;
		return current_game_id == game_id;
	}

	Detection make_detection(std::uint64_t game_id, std::uint64_t place_id)
	{
		return make_detection_result(GameKey::PhantomForces, game_id, place_id, name);
	}

	void collect_entities(
		const rbx::instance_t& workspace,
		const rbx::instance_t& players,
		cache::entity_t& local_entity,
		std::vector<cache::entity_t>& out,
		brickcolor_resolver_t resolve_brickcolor)
	{
		if (!workspace.address || !players.address)
			return;

		rbx::player_t local_player_obj{ memory->read<std::uint64_t>(players.address + Offsets::Player::LocalPlayer) };
		std::string local_player_name;
		if (local_player_obj.address != 0)
			local_player_name = local_player_obj.get_name();

		std::unordered_map<std::string, int> player_teamcolor_map;
		int local_team_color = -1;
		{
			std::vector<rbx::player_t> service_players = players.get_children<rbx::player_t>();
			for (rbx::player_t& sp : service_players)
			{
				std::string pname = sp.get_name();
				int tc = memory->read<int>(sp.address + Offsets::Player::TeamColor);
				if (!pname.empty())
					player_teamcolor_map[pname] = tc;
			}
			if (local_player_obj.address != 0)
				local_team_color = memory->read<int>(local_player_obj.address + Offsets::Player::TeamColor);
		}

		rbx::instance_t pf_players_folder = workspace.find_first_child("Players");
		if (pf_players_folder.address != 0)
		{
			std::vector<rbx::instance_t> team_folders = pf_players_folder.get_children();
			for (rbx::instance_t& team_folder : team_folders)
			{
				std::vector<rbx::instance_t> player_models = team_folder.get_children();
				for (rbx::instance_t& model : player_models)
				{
					cache::entity_t entity{};
					entity.instance = { model.address };
					entity.health = 100.f;
					entity.max_health = 100.f;

					std::vector<rbx::instance_t> parts = model.get_children();
					int limb_index = 0;

					for (rbx::instance_t& part : parts)
					{
						std::string part_class = part.get_class_name();
						if (part_class.find("Part") == std::string::npos)
							continue;

						rbx::instance_t billboard = part.find_first_child_by_class("BillboardGui");
						if (billboard.address != 0)
						{
							rbx::instance_t text_label = billboard.find_first_child_by_class("TextLabel");
							if (text_label.address != 0)
							{
								entity.name = memory->read_string(text_label.address + Offsets::GuiObject::Text);
								entity.display_name = entity.name;
							}
							entity.parts["Head"] = rbx::part_t(part.address);
							continue;
						}

						rbx::instance_t spotlight = part.find_first_child_by_class("SpotLight");
						if (spotlight.address != 0)
						{
							entity.parts["HumanoidRootPart"] = rbx::part_t(part.address);
							entity.parts["UpperTorso"] = rbx::part_t(part.address);
							continue;
						}

						static const char* limb_names[] = {
							"LeftUpperArm", "RightUpperArm",
							"LeftUpperLeg", "RightUpperLeg",
							"LowerTorso"
						};
						if (limb_index < 5)
							entity.parts[limb_names[limb_index]] = rbx::part_t(part.address);
						limb_index++;
					}

					if (entity.parts.find("Head") == entity.parts.end() ||
						entity.parts.find("HumanoidRootPart") == entity.parts.end())
						continue;

					if (entity.name.empty())
						continue;

					auto tc_it = player_teamcolor_map.find(entity.name);
					if (tc_it != player_teamcolor_map.end())
					{
						entity.team = static_cast<std::uint64_t>(tc_it->second);
						if (resolve_brickcolor != nullptr)
							entity.has_team_color = resolve_brickcolor(tc_it->second, entity.team_color);
					}
					else
					{
						entity.team = team_folder.address;
					}

					entity.rig_type = 1;
					entity.humanoid = { 0 };

					out.push_back(entity);
				}
			}
		}

		if (local_player_obj.address != 0)
		{
			local_entity.name = local_player_name;
			local_entity.instance = { local_player_obj.address };
			local_entity.team = (local_team_color >= 0) ? static_cast<std::uint64_t>(local_team_color) : 0;
		}
	}
}
