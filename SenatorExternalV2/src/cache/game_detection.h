#pragma once

#include <gamesupport/gamesupport.h>

namespace cache
{
	struct game_detection_result final
	{
		gamesupport::Detection detection{};
		bool entered_phantom_forces{ false };
	};

	game_detection_result refresh_game_detection();
	gamesupport::RuntimeStatus capture_runtime_status();
	gamesupport::SupportReport make_support_report(const gamesupport::Detection& detection);
	gamesupport::SupportReport publish_support_report(const gamesupport::Detection& detection, bool show_popup = true);
}
