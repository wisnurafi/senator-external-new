#pragma once

#include <gamesupport/gamesupport.h>

namespace ui::support_notification
{
	void push(const gamesupport::SupportReport& report, float duration_seconds = 10.0f);
	void render();
}
