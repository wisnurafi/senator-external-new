#pragma once

#include <sdk/math/math.h>

namespace rage { namespace desync {

struct scene_t
{
	bool active;
	math::vector3 position;
};

void initialize();
void shutdown();
bool is_active();
scene_t get_scene_snapshot();
void render_visualizer();

} }
