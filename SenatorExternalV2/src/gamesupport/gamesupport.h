#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>

namespace gamesupport
{
	enum class GameKey : std::uint8_t
	{
		Unknown = 0,
		PhantomForces,
		MurderMystery2,
		LumberTycoon2,
		BladeBall,
		AnimeLeague,
		Overkill,
	};

	namespace game_ids
	{
		constexpr std::uint64_t LumberTycoon2  = 2471084ULL;
		constexpr std::uint64_t BladeBall      = 4777817887ULL;
		constexpr std::uint64_t AnimeLeague    = 7518216173ULL;
		constexpr std::uint64_t Overkill       = 8420998291ULL;
	}

	struct Detection final
	{
		GameKey key{ GameKey::Unknown };
		std::uint64_t game_id{ 0 };
		std::uint64_t place_id{ 0 };
		bool supported{ false };
		std::string_view name{ "Unknown" };
	};

	enum class SupportState : std::uint8_t
	{
		Failed = 0,
		GenericReady,
		ProfileReady,
	};

	struct RuntimeStatus final
	{
		bool has_datamodel{ false };
		bool has_workspace{ false };
		bool has_players{ false };
		bool has_camera{ false };
		std::size_t universal_feature_count{ 0 };
		std::size_t profile_feature_count{ 0 };
	};

	struct SupportReport final
	{
		SupportState state{ SupportState::Failed };
		std::string game_name{};
		std::string profile_name{};
		std::string title{ "Failed" };
		std::string status_label{ "Failed" };
		std::string detail{ "Game unsupported" };
		std::string features_label{ "No features work" };
		bool core_ready{ false };
		bool has_profile{ false };
	};

	using GameMatcher = bool(*)(std::uint64_t game_id, std::uint64_t place_id);

	struct GameModule final
	{
		GameKey key{ GameKey::Unknown };
		std::uint64_t game_id{ 0 };
		std::string_view name{ "Unknown" };
		GameMatcher matches{ nullptr };
	};

	const GameModule* modules();
	std::size_t module_count();
	const GameModule* find_module(std::uint64_t game_id, std::uint64_t place_id);

	Detection detect(std::uint64_t game_id, std::uint64_t place_id);
	Detection make_detection_result(
		GameKey key,
		std::uint64_t game_id,
		std::uint64_t place_id,
		std::string_view name,
		bool supported = true);

	const char* support_state_label(SupportState state);
	SupportReport make_support_report(
		const Detection& detection,
		std::string_view display_name,
		const RuntimeStatus& runtime);
	void log_support_status(const SupportReport& report);
	void log_support_status(const Detection& d);
}
