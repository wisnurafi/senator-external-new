#pragma once

#include <imgui/imgui.h>
#include <algorithm>

namespace branding
{
	inline constexpr const char* product_name = "Senator";
	inline constexpr const char* product_full_name = "Senator External";
	inline constexpr const char* loader_title = "Senator | Loader";
	inline constexpr const char* product_type = "External Overlay";
	inline constexpr const char* target_name = "Roblox Player";
	inline constexpr const char* platform_name = "Windows x64";
	inline constexpr const char* version = "v1.0.0";
	inline constexpr const char* version_short = "v1.0.0";
	inline constexpr const char* channel = "BETA";

	inline ImVec4 accent()
	{
		return ImVec4(0.45f, 0.25f, 1.0f, 1.0f);
	}

	inline ImVec4 accent_dark()
	{
		return ImVec4(0.24f, 0.16f, 0.45f, 1.0f);
	}

	inline ImU32 accent_u32(float alpha = 1.0f)
	{
		ImVec4 color = accent();
		color.w = std::clamp(alpha, 0.0f, 1.0f);
		return ImGui::ColorConvertFloat4ToU32(color);
	}

	inline ImU32 accent_dark_u32(float alpha = 1.0f)
	{
		ImVec4 color = accent_dark();
		color.w = std::clamp(alpha, 0.0f, 1.0f);
		return ImGui::ColorConvertFloat4ToU32(color);
	}

	inline void draw_mark(ImDrawList* draw, ImVec2 pos, float size, float alpha = 1.0f)
	{
		if (draw == nullptr || size <= 0.0f)
			return;

		const ImU32 purple = accent_u32(alpha);
		const ImU32 purple_dark = accent_dark_u32(alpha);
		const float s = size;
		const float stroke = (std::max)(2.0f, s * 0.115f);
		const auto p = [&](float x, float y) {
			return ImVec2(pos.x + s * x, pos.y + s * y);
		};

		ImVec2 crown[] = {
			p(0.08f, 0.26f),
			p(0.30f, 0.34f),
			p(0.50f, 0.08f),
			p(0.70f, 0.34f),
			p(0.92f, 0.26f),
			p(0.82f, 0.46f),
			p(0.50f, 0.38f),
			p(0.18f, 0.46f),
		};
		draw->AddPolyline(crown, IM_ARRAYSIZE(crown), purple, ImDrawFlags_Closed, stroke);

		ImVec2 upper[] = {
			p(0.76f, 0.47f),
			p(0.22f, 0.47f),
			p(0.66f, 0.64f),
			p(0.24f, 0.81f),
		};
		draw->AddPolyline(upper, IM_ARRAYSIZE(upper), purple, 0, stroke);

		ImVec2 lower[] = {
			p(0.24f, 0.61f),
			p(0.76f, 0.61f),
			p(0.34f, 0.79f),
			p(0.52f, 0.92f),
			p(0.52f, 0.82f),
		};
		draw->AddPolyline(lower, IM_ARRAYSIZE(lower), purple_dark, 0, stroke);
	}
}
