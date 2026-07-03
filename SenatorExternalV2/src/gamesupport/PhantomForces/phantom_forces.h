#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <cache/cache.h>
#include <gamesupport/gamesupport.h>
#include <sdk/sdk.h>

namespace gamesupport::phantom_forces
{
	inline constexpr std::uint64_t game_id = 113491250ULL;
	inline constexpr std::string_view name = "Phantom Forces";

	using brickcolor_resolver_t = bool(*)(int id, float out[3]);

	bool matches(std::uint64_t game_id, std::uint64_t place_id);
	Detection make_detection(std::uint64_t game_id, std::uint64_t place_id);

	void collect_entities(
		const rbx::instance_t& workspace,
		const rbx::instance_t& players,
		cache::entity_t& local_entity,
		std::vector<cache::entity_t>& out,
		brickcolor_resolver_t resolve_brickcolor);
}
