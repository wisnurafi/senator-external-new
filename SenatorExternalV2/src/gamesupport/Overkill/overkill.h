#pragma once

#include <string>
#include <vector>
#include <cache/cache.h>
#include <sdk/sdk.h>

namespace gamesupport::overkill
{
	bool collect_entities(const rbx::instance_t& workspace, std::vector<cache::entity_t>& out);
	bool is_excluded_entity_name(const std::string& name);
}
