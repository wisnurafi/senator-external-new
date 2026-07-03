#pragma once
#include <windows.h>
#include <mutex>
#include <string>
#include <utility>
#include <gamesupport/gamesupport.h>
#include <sdk/sdk.h>

namespace game
{
	inline rbx::instance_t datamodel{};
	inline rbx::visualengine_t visengine{};
	inline rbx::instance_t workspace{};
	inline rbx::instance_t players{};
	inline rbx::instance_t local_player{};
	inline rbx::instance_t local_character{};
	inline std::uint64_t camera{};
	inline HWND roblox_window = nullptr;
	inline float window_offset_x = 0.0f;
	inline float window_offset_y = 0.0f;
	inline gamesupport::Detection active_detection{};
	inline gamesupport::GameKey active_game = gamesupport::GameKey::Unknown;
	inline std::string active_game_display_name{};
	inline std::mutex active_game_display_name_mtx;
	inline bool is_phantom_forces = false;
	inline bool is_murder_mystery_2 = false;
	inline bool is_lumber_tycoon_2 = false;
	inline bool is_blade_ball = false;
	inline bool is_anime_league = false;
	inline bool is_overkill = false;
	inline std::string binary_name = "RobloxPlayerBeta.exe";

	inline bool is(gamesupport::GameKey key)
	{
		return active_game == key;
	}

	inline std::string get_active_game_display_name()
	{
		std::lock_guard<std::mutex> lock(active_game_display_name_mtx);
		return active_game_display_name;
	}

	inline void set_active_game_display_name(std::string name)
	{
		std::lock_guard<std::mutex> lock(active_game_display_name_mtx);
		active_game_display_name = std::move(name);
	}

	inline std::string detection_name(const gamesupport::Detection& detection)
	{
		if (detection.name.empty())
			return {};

		return std::string(detection.name.data(), detection.name.size());
	}

	inline bool is_unknown_name(const std::string& name)
	{
		return name.empty() || name == "Unknown" || name == "unknown";
	}

	inline bool is_generic_datamodel_name(const std::string& name)
	{
		return is_unknown_name(name) || name == "DataModel" || name == "Game";
	}

	inline bool is_place_fallback_name(const std::string& name)
	{
		return name.rfind("Place ", 0) == 0;
	}

	inline std::string read_datamodel_display_name()
	{
		if (!datamodel.address)
			return {};

		try
		{
			const std::string name = datamodel.get_name();
			if (!is_generic_datamodel_name(name))
				return name;
		}
		catch (...)
		{
		}

		return {};
	}

	inline std::string resolve_game_display_name(const gamesupport::Detection& detection)
	{
		const std::string supported_name = detection_name(detection);
		if (!is_unknown_name(supported_name))
			return supported_name;

		const std::string datamodel_name = read_datamodel_display_name();
		if (!datamodel_name.empty())
			return datamodel_name;

		if (detection.place_id != 0)
			return "Place " + std::to_string(detection.place_id);

		return {};
	}

	inline void set_active_detection(const gamesupport::Detection& detection)
	{
		const bool same_experience = active_detection.game_id == detection.game_id && active_detection.place_id == detection.place_id;
		const std::string previous_display_name = get_active_game_display_name();

		active_detection = detection;
		active_game = detection.key;

		std::string display_name = resolve_game_display_name(detection);
		if (same_experience &&
			!previous_display_name.empty() &&
			!is_place_fallback_name(previous_display_name))
		{
			display_name = previous_display_name;
		}

		set_active_game_display_name(display_name);

		is_phantom_forces = is(gamesupport::GameKey::PhantomForces);
		is_murder_mystery_2 = is(gamesupport::GameKey::MurderMystery2);
		is_lumber_tycoon_2 = is(gamesupport::GameKey::LumberTycoon2);
		is_blade_ball = is(gamesupport::GameKey::BladeBall);
		is_anime_league = is(gamesupport::GameKey::AnimeLeague);
		is_overkill = is(gamesupport::GameKey::Overkill);
	}

	HWND get_roblox_window();
	void update_window_offset();
}
