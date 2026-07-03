#include "diagnostics.h"

#include <mutex>

#include <cache/cache.h>
#include <cache/game_detection.h>
#include <features/feature_registry.h>
#include <game/game.h>

diagnostics::Snapshot diagnostics::capture()
{
	Snapshot snapshot{};

	snapshot.active_game = game::active_game;
	snapshot.game_id = game::active_detection.game_id;
	snapshot.place_id = game::active_detection.place_id;
	snapshot.game_name = game::get_active_game_display_name();
	const gamesupport::SupportReport report = cache::make_support_report(game::active_detection);
	snapshot.support_state = report.state;
	snapshot.has_profile = report.has_profile;
	snapshot.features_label = report.features_label;

	snapshot.has_datamodel = game::datamodel.address != 0;
	snapshot.has_workspace = game::workspace.address != 0;
	snapshot.has_players = game::players.address != 0;
	snapshot.has_camera = game::camera != 0;

	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		snapshot.has_local_player = game::local_player.address != 0 || cache::cached_local_player.instance.address != 0;
		snapshot.cached_player_count = cache::cached_players.size();
	}

	snapshot.registered_feature_count = features::registry_count();
	snapshot.enabled_feature_count = features::enabled_count();

	return snapshot;
}
