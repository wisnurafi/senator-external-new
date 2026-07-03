#include "feature_registry.h"

#include <game/game.h>
#include <settings.h>

namespace
{
#define FEATURE(ID, LABEL, CATEGORY, ENABLED) \
	{ ID, LABEL, CATEGORY, features::kAllGames, [] { return (ENABLED); }, nullptr, nullptr }

#define FEATURE_KEY(ID, LABEL, CATEGORY, ENABLED, KEYBIND, MODE) \
	{ ID, LABEL, CATEGORY, features::kAllGames, [] { return (ENABLED); }, [] { return (KEYBIND); }, [] { return (MODE); } }

#define FEATURE_GAME(ID, LABEL, CATEGORY, GAME_MASK, ENABLED) \
	{ ID, LABEL, CATEGORY, GAME_MASK, [] { return (ENABLED); }, nullptr, nullptr }

	const features::FeatureDescriptor k_features[] = {
		FEATURE_KEY("aimbot", "Aimbot", "Combat", settings::aimbot::enabled, settings::aimbot::keybind, settings::aimbot::activation_mode),
		FEATURE_KEY("aimbot_triggerbot", "Aimbot Triggerbot", "Combat", settings::aimbot::triggerbot::enabled, settings::aimbot::triggerbot::keybind, settings::aimbot::triggerbot::activation_mode),
		FEATURE_KEY("silentaim", "Silent Aim", "Combat", settings::silentaim::enabled, settings::silentaim::keybind, settings::silentaim::activation_mode),
		FEATURE_KEY("silentaim_triggerbot", "Silent Aim Triggerbot", "Combat", settings::silentaim::triggerbot::enabled, settings::silentaim::triggerbot::keybind, settings::silentaim::triggerbot::activation_mode),
		FEATURE("visuals_enemies", "Enemy Visuals", "Visuals", settings::visuals::enable_enemies),
		FEATURE("visuals_client", "Client Visuals", "Visuals", settings::visuals::enable_client),
		FEATURE("radar", "Radar", "Visuals", settings::visuals::radar_enabled),
		FEATURE_KEY("speedhack", "Speed", "Movement", settings::movement::speedhack::enabled, settings::movement::speedhack::keybind, settings::movement::speedhack::activation_mode),
		FEATURE_KEY("jumphack", "Jump", "Movement", settings::movement::jumphack::enabled, settings::movement::jumphack::keybind, settings::movement::jumphack::activation_mode),
		FEATURE_KEY("flyhack", "Fly", "Movement", settings::movement::flyhack::enabled, settings::movement::flyhack::keybind, settings::movement::flyhack::activation_mode),
		FEATURE("tickrate", "Tickrate", "Movement", settings::movement::tickrate::enabled),
		FEATURE("orbit", "Orbit", "Movement", settings::movement::orbit::enabled),
		FEATURE("gravity", "Gravity", "Movement", settings::movement::gravity::enabled),
		FEATURE("rapidfire", "Rapid Fire", "Rage", settings::rage::rapidfire),
		FEATURE("noclip", "Noclip", "Rage", settings::rage::noclip),
		FEATURE("hit_tracers", "Hit Tracers", "Rage", settings::rage::hit_tracers || settings::visuals::hit_tracers_enabled),
		FEATURE("hipheight", "Hip Height", "Rage", settings::rage::hipheight::enabled),
		FEATURE("thirdperson", "3rd Person View", "Rage", settings::rage::thirdperson::enabled),
		FEATURE("hitbox_expander", "Hitbox Expander", "Rage", settings::rage::hitbox_expander::enabled),
		FEATURE_KEY("spin360", "Spin 360", "Rage", settings::rage::spin360::enabled, settings::rage::spin360::keybind, settings::rage::spin360::activation_mode),
		FEATURE_KEY("desync", "Desync", "Rage", settings::desync::enabled, settings::desync::keybind, settings::desync::keybind_mode),
		FEATURE_KEY("magicbullet", "Magic Bullet", "Rage", settings::magicbullet::enabled, settings::magicbullet::keybind, settings::magicbullet::activation_mode),
		FEATURE_GAME("blade_ball_auto_parry", "Blade Ball Auto Parry", "Game Support", features::game_mask(gamesupport::GameKey::BladeBall), settings::blade_ball::auto_parry),
		FEATURE_GAME("blade_ball_auto_spam", "Blade Ball Auto Spam", "Game Support", features::game_mask(gamesupport::GameKey::BladeBall), settings::blade_ball::auto_spam),
		FEATURE_GAME("blade_ball_esp", "Blade Ball ESP", "Game Support", features::game_mask(gamesupport::GameKey::BladeBall), settings::blade_ball::ball_esp),
		FEATURE("antiafk", "Anti AFK", "Exploits", settings::exploits::antiafk::enabled),
		FEATURE_KEY("freezeplayer", "Freeze Player", "Exploits", settings::exploits::freezeplayer::enabled, settings::exploits::freezeplayer::keybind, settings::exploits::freezeplayer::activation_mode),
		FEATURE("fog", "Fog", "Lighting", settings::lighting::fog::enabled),
		FEATURE("shadows", "Shadows", "Lighting", settings::lighting::shadows::disable),
		FEATURE("clocktime", "Clock Time", "Lighting", settings::lighting::clocktime::enabled),
		FEATURE("skybox", "Skybox", "Lighting", settings::lighting::skybox::enabled),
		FEATURE("exposure", "Exposure", "Lighting", settings::lighting::exposure::enabled),
	};

#undef FEATURE_KEY
#undef FEATURE_GAME
#undef FEATURE
}

features::GameMask features::game_mask(gamesupport::GameKey key)
{
	if (key == gamesupport::GameKey::Unknown)
		return kAllGames;

	const auto bit = static_cast<std::uint8_t>(key) - 1U;
	if (bit >= 31U)
		return kAllGames;

	return static_cast<GameMask>(1U << bit);
}

bool features::is_universal(const FeatureDescriptor& feature)
{
	return feature.game_mask == kAllGames;
}

bool features::is_available(const FeatureDescriptor& feature, gamesupport::GameKey active_game)
{
	if (is_universal(feature))
		return true;

	return (feature.game_mask & game_mask(active_game)) != 0;
}

bool features::is_available(const FeatureDescriptor& feature)
{
	return is_available(feature, game::active_game);
}

const features::FeatureDescriptor* features::registry()
{
	return k_features;
}

std::size_t features::registry_count()
{
	return sizeof(k_features) / sizeof(k_features[0]);
}

std::size_t features::enabled_count()
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < registry_count(); ++i)
	{
		if (is_available(k_features[i]) && k_features[i].is_enabled != nullptr && k_features[i].is_enabled())
			++count;
	}
	return count;
}

std::size_t features::universal_count()
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < registry_count(); ++i)
	{
		if (is_universal(k_features[i]))
			++count;
	}
	return count;
}

std::size_t features::profile_available_count(gamesupport::GameKey active_game)
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < registry_count(); ++i)
	{
		if (!is_universal(k_features[i]) && is_available(k_features[i], active_game))
			++count;
	}
	return count;
}

std::size_t features::available_count()
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < registry_count(); ++i)
	{
		if (is_available(k_features[i]))
			++count;
	}
	return count;
}
