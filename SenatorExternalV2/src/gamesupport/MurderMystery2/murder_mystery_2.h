#pragma once

#include <cstdint>
#include <string_view>

#include <cache/cache.h>
#include <gamesupport/gamesupport.h>
#include <sdk/sdk.h>

namespace gamesupport::murder_mystery_2
{
	inline constexpr std::uint64_t game_id = 66654135ULL;
	inline constexpr std::string_view name = "Murder Mystery 2";

	bool matches(std::uint64_t game_id, std::uint64_t place_id);
	Detection make_detection(std::uint64_t game_id, std::uint64_t place_id);

	void apply_role(cache::entity_t& entity, rbx::player_t& player);
}
