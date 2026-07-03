#include "game_detection.h"

#include <Offsets/Offsets.hpp>
#include <features/feature_registry.h>
#include <game/game.h>
#include <memory/memory.h>
#include <runtime/runtime_log.h>
#include <ui/support_notification/support_notification.h>
#include <utils/net/https_get.h>

#include "../../ext/json/json.hpp"

#include <cstdint>
#include <chrono>
#include <future>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
	std::string fetch_experience_name(std::uint64_t place_id)
	{
		std::string body;
		if (!netutil::https_get("https://apis.roblox.com/universes/v1/places/" + std::to_string(place_id) + "/universe", body))
			return {};

		std::uint64_t universe_id = 0;
		try
		{
			const auto json = nlohmann::json::parse(body);
			universe_id = json.value("universeId", 0ULL);
		}
		catch (...)
		{
			return {};
		}

		if (universe_id == 0)
			return {};

		body.clear();
		if (!netutil::https_get("https://games.roblox.com/v1/games?universeIds=" + std::to_string(universe_id), body))
			return {};

		try
		{
			const auto json = nlohmann::json::parse(body);
			if (!json.contains("data") || !json["data"].is_array() || json["data"].empty())
				return {};

			return json["data"][0].value("name", "");
		}
		catch (...)
		{
			return {};
		}
	}

	bool refresh_remote_display_name(const gamesupport::Detection& detection)
	{
		if (detection.key != gamesupport::GameKey::Unknown || detection.place_id == 0)
			return false;

		static std::unordered_map<std::uint64_t, std::string> cached_names;
		static std::unordered_set<std::uint64_t> attempted_places;
		static std::unordered_map<std::uint64_t, std::future<std::string>> in_flight;

		const auto cached = cached_names.find(detection.place_id);
		if (cached != cached_names.end())
		{
			if (game::get_active_game_display_name() == cached->second)
				return false;

			game::set_active_game_display_name(cached->second);
			return true;
		}

		const auto pending = in_flight.find(detection.place_id);
		if (pending != in_flight.end())
		{
			if (pending->second.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
				return false;

			std::string name;
			try
			{
				name = pending->second.get();
			}
			catch (...)
			{
			}

			in_flight.erase(pending);

			if (game::is_unknown_name(name))
				return false;

			cached_names.emplace(detection.place_id, name);
			game::set_active_game_display_name(name);

			std::ostringstream ss;
			ss << "Resolved game name: " << name << " (place_id=" << detection.place_id << ")";
			runtime_log::info("Game", ss.str());
			return true;
		}

		if (attempted_places.find(detection.place_id) != attempted_places.end())
			return false;

		attempted_places.insert(detection.place_id);
		in_flight.emplace(detection.place_id, std::async(std::launch::async, [place_id = detection.place_id]() {
			return fetch_experience_name(place_id);
		}));
		return false;
	}
}

gamesupport::RuntimeStatus cache::capture_runtime_status()
{
	gamesupport::RuntimeStatus status{};
	status.has_datamodel = game::datamodel.address != 0;
	status.has_workspace = game::workspace.address != 0;
	status.has_players = game::players.address != 0;
	status.has_camera = game::camera != 0;

	if (!status.has_camera && game::workspace.address != 0)
	{
		try
		{
			status.has_camera = memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) != 0;
		}
		catch (...)
		{
		}
	}

	status.universal_feature_count = features::universal_count();
	status.profile_feature_count = features::profile_available_count(game::active_game);
	return status;
}

gamesupport::SupportReport cache::make_support_report(const gamesupport::Detection& detection)
{
	return gamesupport::make_support_report(
		detection,
		game::get_active_game_display_name(),
		capture_runtime_status());
}

gamesupport::SupportReport cache::publish_support_report(const gamesupport::Detection& detection, bool show_popup)
{
	gamesupport::SupportReport report = make_support_report(detection);
	gamesupport::log_support_status(report);

	if (show_popup)
		ui::support_notification::push(report, 10.0f);

	return report;
}

cache::game_detection_result cache::refresh_game_detection()
{
	const std::uint64_t game_id = game::datamodel.address ? memory->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::GameId) : 0ULL;
	const std::uint64_t place_id = game::datamodel.address ? memory->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::PlaceId) : 0ULL;
	const gamesupport::Detection detection = gamesupport::detect(game_id, place_id);

	const bool was_phantom_forces = game::is_phantom_forces;
	game::set_active_detection(detection);
	const bool remote_name_updated = refresh_remote_display_name(detection);

	static gamesupport::GameKey last_key = gamesupport::GameKey::Unknown;
	static std::uint64_t last_place_id = 0;
	if ((detection.key != last_key || detection.place_id != last_place_id) && detection.game_id != 0)
	{
		std::ostringstream ss;
		ss << "Active game changed: ";
		const std::string game_name = game::get_active_game_display_name();
		if (!game_name.empty())
			ss << game_name;
		else
			ss.write(detection.name.data(), static_cast<std::streamsize>(detection.name.size()));
		ss << " (game_id=" << detection.game_id << ", place_id=" << detection.place_id << ")";
		runtime_log::info("Game", ss.str());

		last_key = detection.key;
		last_place_id = detection.place_id;

		publish_support_report(detection, true);
	}
	else if (remote_name_updated)
	{
		publish_support_report(detection, true);
	}

	cache::game_detection_result result{};
	result.detection = detection;
	result.entered_phantom_forces = game::is_phantom_forces && !was_phantom_forces;
	return result;
}
