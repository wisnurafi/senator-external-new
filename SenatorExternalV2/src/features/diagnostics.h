#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <gamesupport/gamesupport.h>

namespace diagnostics
{
	struct Snapshot final
	{
		gamesupport::GameKey active_game{ gamesupport::GameKey::Unknown };
		std::uint64_t game_id{ 0 };
		std::uint64_t place_id{ 0 };
		std::string game_name{};
		gamesupport::SupportState support_state{ gamesupport::SupportState::Failed };
		bool has_profile{ false };
		std::string features_label{};

		bool has_datamodel{ false };
		bool has_workspace{ false };
		bool has_players{ false };
		bool has_camera{ false };
		bool has_local_player{ false };
		std::size_t cached_player_count{ 0 };

		std::size_t registered_feature_count{ 0 };
		std::size_t enabled_feature_count{ 0 };
	};

	Snapshot capture();
}
