#include "overkill.h"

#include <initializer_list>
#include <string>
#include <Offsets/Offsets.hpp>
#include <memory/memory.h>

namespace
{
	bool is_part_like_class(const std::string& class_name)
	{
		return class_name.find("Part") != std::string::npos ||
			class_name == "UnionOperation" ||
			class_name == "NegateOperation" ||
			class_name == "PartOperation";
	}

	void populate_entity_from_model_recursive(cache::entity_t& entity, const rbx::instance_t& root, int depth, int& visited)
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
				const std::string child_class = child.get_class_name();

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

	void populate_entity_from_model(cache::entity_t& entity, const rbx::model_instance_t& model)
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

	bool entity_has_usable_parts(const cache::entity_t& entity)
	{
		return entity.parts.find("HumanoidRootPart") != entity.parts.end() ||
			entity.parts.find("RootPart") != entity.parts.end() ||
			entity.parts.find("Head") != entity.parts.end() ||
			entity.parts.find("UpperTorso") != entity.parts.end() ||
			entity.parts.find("Torso") != entity.parts.end();
	}

	void add_part_aliases(cache::entity_t& entity)
	{
		if (entity.parts.empty())
			return;

		auto ensure_alias = [&](const char* alias, std::initializer_list<const char*> candidates) {
			if (entity.parts.find(alias) != entity.parts.end())
				return;

			for (const char* candidate : candidates)
			{
				auto it = entity.parts.find(candidate);
				if (it != entity.parts.end() && it->second.address)
				{
					entity.parts[alias] = it->second;
					return;
				}
			}
		};

		ensure_alias("HumanoidRootPart", { "HumanoidRootPart", "RootPart", "Root", "Torso", "UpperTorso", "LowerTorso", "Body", "Chest", "Hips" });
		ensure_alias("Head", { "Head", "head", "Helmet" });
		ensure_alias("UpperTorso", { "UpperTorso", "Torso", "Body", "Chest" });
		ensure_alias("LowerTorso", { "LowerTorso", "Torso", "Body", "Hips" });
		ensure_alias("LeftUpperArm", { "LeftUpperArm", "Left Arm", "LeftArm", "LeftHand", "L_Arm" });
		ensure_alias("RightUpperArm", { "RightUpperArm", "Right Arm", "RightArm", "RightHand", "R_Arm" });
		ensure_alias("LeftUpperLeg", { "LeftUpperLeg", "Left Leg", "LeftLeg", "L_Leg" });
		ensure_alias("RightUpperLeg", { "RightUpperLeg", "Right Leg", "RightLeg", "R_Leg" });

		if (entity.parts.find("HumanoidRootPart") == entity.parts.end())
		{
			for (const auto& part : entity.parts)
			{
				if (part.second.address)3
				{
					entity.parts["HumanoidRootPart"] = part.second;
					break;
				}
			}
		}
	}

	void collect_entities_recursive(const rbx::instance_t& root, std::vector<cache::entity_t>& out, int depth, int& visited)
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

				if (gamesupport::overkill::is_excluded_entity_name(child_name) || child_name.empty())
					continue;

				if (child_class == "Model")
				{
					cache::entity_t entity{};
					entity.instance = { child.address };
					entity.name = child_name;
					entity.display_name = entity.name;

					populate_entity_from_model(entity, { child.address });
					add_part_aliases(entity);
					if (entity_has_usable_parts(entity))
					{
						entity.rig_type = 1;
						out.push_back(entity);
						continue;
					}
				}

				if (depth > 0)
					collect_entities_recursive(child, out, depth - 1, visited);
			}
		}
		catch (...)
		{
		}
	}
}

bool gamesupport::overkill::is_excluded_entity_name(const std::string& name)
{
	return name == "ViewModel";
}

bool gamesupport::overkill::collect_entities(const rbx::instance_t& workspace, std::vector<cache::entity_t>& out)
{
	if (!workspace.address)
		return false;

	try
	{
		rbx::instance_t world = workspace.find_first_child("World");
		if (!world.address)
			return false;

		rbx::instance_t entities = world.find_first_child("Entities");
		if (!entities.address)
			return false;

		int visited = 0;
		collect_entities_recursive(entities, out, 4, visited);
	}
	catch (...)
	{
	}

	return !out.empty();
}
