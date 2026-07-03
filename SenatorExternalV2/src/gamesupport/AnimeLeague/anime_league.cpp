#include "anime_league.h"

bool gamesupport::anime_league::should_use_workspace_fallback(bool has_usable_cache)
{
	return !has_usable_cache;
}

int gamesupport::anime_league::workspace_scan_depth()
{
	return 5;
}
