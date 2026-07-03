#include "gamesupport.h"

#include <gamesupport/MurderMystery2/murder_mystery_2.h>
#include <gamesupport/PhantomForces/phantom_forces.h>
#include <runtime/runtime_log.h>

#include <string>
#include <sstream>

namespace
{
	constexpr gamesupport::GameModule k_modules[] = {
		{ gamesupport::GameKey::PhantomForces, gamesupport::phantom_forces::game_id, gamesupport::phantom_forces::name, gamesupport::phantom_forces::matches },
		{ gamesupport::GameKey::MurderMystery2, gamesupport::murder_mystery_2::game_id, gamesupport::murder_mystery_2::name, gamesupport::murder_mystery_2::matches },
		{ gamesupport::GameKey::LumberTycoon2, gamesupport::game_ids::LumberTycoon2, "Lumber Tycoon 2" },
		{ gamesupport::GameKey::BladeBall, gamesupport::game_ids::BladeBall, "Blade Ball" },
		{ gamesupport::GameKey::AnimeLeague, gamesupport::game_ids::AnimeLeague, "Anime League" },
		{ gamesupport::GameKey::Overkill, gamesupport::game_ids::Overkill, "Overkill" },
	};

	std::string string_from_view(std::string_view value)
	{
		return std::string(value.data(), value.size());
	}

	bool is_unknown_name(const std::string& name)
	{
		return name.empty() || name == "Unknown" || name == "unknown";
	}
}

const gamesupport::GameModule* gamesupport::modules()
{
	return k_modules;
}

std::size_t gamesupport::module_count()
{
	return sizeof(k_modules) / sizeof(k_modules[0]);
}

const gamesupport::GameModule* gamesupport::find_module(std::uint64_t game_id, std::uint64_t place_id)
{
	for (const GameModule& module : k_modules)
	{
		const bool matched = module.matches != nullptr
			? module.matches(game_id, place_id)
			: module.game_id == game_id;

		if (matched)
			return &module;
	}

	return nullptr;
}

gamesupport::Detection gamesupport::detect(std::uint64_t game_id, std::uint64_t place_id)
{
	if (const GameModule* module = find_module(game_id, place_id))
		return make_detection_result(module->key, game_id, place_id, module->name);

	return make_detection_result(GameKey::Unknown, game_id, place_id, "Unknown", false);
}

gamesupport::Detection gamesupport::make_detection_result(
	GameKey key,
	std::uint64_t game_id,
	std::uint64_t place_id,
	std::string_view name,
	bool supported)
{
	Detection d{};
	d.key = key;
	d.game_id = game_id;
	d.place_id = place_id;
	d.supported = supported;
	d.name = name;
	return d;
}

const char* gamesupport::support_state_label(SupportState state)
{
	switch (state)
	{
	case SupportState::ProfileReady:
		return "ProfileReady";
	case SupportState::GenericReady:
		return "GenericReady";
	case SupportState::Failed:
	default:
		return "Failed";
	}
}

gamesupport::SupportReport gamesupport::make_support_report(
	const Detection& detection,
	std::string_view display_name,
	const RuntimeStatus& runtime)
{
	SupportReport report{};

	const bool has_profile = detection.key != GameKey::Unknown && detection.supported;
	const bool core_ready = runtime.has_datamodel && runtime.has_workspace && runtime.has_players && runtime.has_camera;
	const bool has_universal_features = runtime.universal_feature_count > 0;
	const bool has_profile_features = has_profile && runtime.profile_feature_count > 0;

	report.core_ready = core_ready;
	report.has_profile = has_profile;
	report.profile_name = has_profile ? string_from_view(detection.name) : "Generic";

	report.game_name = string_from_view(display_name);
	if (is_unknown_name(report.game_name))
		report.game_name = has_profile ? report.profile_name : "Roblox Experience";

	if (!core_ready || (!has_universal_features && !has_profile_features))
	{
		report.state = SupportState::Failed;
		report.title = "Failed";
		report.status_label = "Failed";
		report.detail = "Game unsupported";
		report.features_label = "No features work";
		return report;
	}

	report.title = report.game_name;
	report.status_label = "Ready";
	report.detail = has_profile ? "Game profile loaded" : "Universal mode ready";

	if (has_profile)
	{
		report.state = SupportState::ProfileReady;
		report.features_label = has_universal_features
			? "Universal + " + report.profile_name
			: report.profile_name;
	}
	else
	{
		report.state = SupportState::GenericReady;
		report.features_label = "Universal";
	}

	return report;
}

void gamesupport::log_support_status(const SupportReport& report)
{
	if (report.state == SupportState::Failed)
	{
		printf("Game: %s\n", report.status_label.c_str());
		printf("Status: %s\n", report.detail.c_str());
		printf("Features: %s\n", report.features_label.c_str());

		std::string message = report.title + ": " + report.detail + " (" + report.features_label + ")";
		runtime_log::warning("Game", message);
		return;
	}

	printf("Game: %s\n", report.game_name.c_str());
	printf("Status: %s\n", report.status_label.c_str());
	printf("Features: %s\n", report.features_label.c_str());

	std::ostringstream ss;
	ss << "Game support: " << report.game_name
		<< " [" << support_state_label(report.state) << "]"
		<< " Features: " << report.features_label;
	runtime_log::info("Game", ss.str());
}

void gamesupport::log_support_status(const Detection& d)
{
	RuntimeStatus runtime{};
	runtime.has_datamodel = true;
	runtime.has_workspace = true;
	runtime.has_players = true;
	runtime.has_camera = true;
	runtime.universal_feature_count = 1;
	runtime.profile_feature_count = d.key != GameKey::Unknown ? 1 : 0;

	log_support_status(make_support_report(d, d.name, runtime));
}
