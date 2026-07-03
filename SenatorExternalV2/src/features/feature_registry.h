#pragma once

#include <cstddef>
#include <cstdint>
#include <gamesupport/gamesupport.h>
#include <string_view>

namespace features
{
	using GameMask = std::uint32_t;
	constexpr GameMask kAllGames = 0;

	struct FeatureDescriptor final
	{
		std::string_view id;
		std::string_view label;
		std::string_view category;
		GameMask game_mask{ kAllGames };
		bool (*is_enabled)();
		int (*get_keybind)();
		int (*get_activation_mode)();
	};

	GameMask game_mask(gamesupport::GameKey key);
	bool is_universal(const FeatureDescriptor& feature);
	bool is_available(const FeatureDescriptor& feature, gamesupport::GameKey active_game);
	bool is_available(const FeatureDescriptor& feature);
	const FeatureDescriptor* registry();
	std::size_t registry_count();
	std::size_t enabled_count();
	std::size_t available_count();
	std::size_t universal_count();
	std::size_t profile_available_count(gamesupport::GameKey active_game);
}
