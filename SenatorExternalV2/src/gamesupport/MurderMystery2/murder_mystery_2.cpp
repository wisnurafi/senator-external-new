#include "murder_mystery_2.h"

#include <string>
#include <vector>

namespace
{
	int detect_role_from_tool(const std::string& tool)
	{
		if (tool.empty())
			return 0;
		if (tool == "Knife" || tool == "knife")
			return 3;
		if (tool == "Gun" || tool == "gun" || tool == "Revolver" || tool == "revolver")
			return 2;
		return 0;
	}

	void set_role_color(cache::entity_t& entity)
	{
		if (entity.mm2_role == 3)
		{
			entity.team_color[0] = 1.0f;
			entity.team_color[1] = 0.15f;
			entity.team_color[2] = 0.15f;
		}
		else if (entity.mm2_role == 2)
		{
			entity.team_color[0] = 0.15f;
			entity.team_color[1] = 0.4f;
			entity.team_color[2] = 1.0f;
		}
		else
		{
			entity.team_color[0] = 0.6f;
			entity.team_color[1] = 0.6f;
			entity.team_color[2] = 0.6f;
		}

		entity.has_team_color = true;
	}
}

namespace gamesupport::murder_mystery_2
{
	bool matches(std::uint64_t current_game_id, std::uint64_t place_id)
	{
		(void)place_id;
		return current_game_id == game_id;
	}

	Detection make_detection(std::uint64_t game_id, std::uint64_t place_id)
	{
		return make_detection_result(GameKey::MurderMystery2, game_id, place_id, name);
	}

	void apply_role(cache::entity_t& entity, rbx::player_t& player)
	{
		entity.mm2_role = 1;

		int role = detect_role_from_tool(entity.tool_name);
		if (role == 0)
		{
			rbx::instance_t backpack = player.find_first_child("Backpack");
			if (backpack.address != 0)
			{
				std::vector<rbx::instance_t> items = backpack.get_children();
				for (rbx::instance_t& item : items)
				{
					role = detect_role_from_tool(item.get_name());
					if (role != 0)
						break;
				}
			}
		}

		if (role != 0)
			entity.mm2_role = role;

		set_role_color(entity);
	}
}
