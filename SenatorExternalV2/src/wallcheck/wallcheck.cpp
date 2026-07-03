#include "wallcheck.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <game/game.h>
#include <imgui/imgui.h>
#include <memory/memory.h>

std::unique_ptr<c_wallcheck> wallcheck = std::make_unique<c_wallcheck>();

#include <offsets/offsets.hpp>
#include <settings.h>

namespace
{
	static int clamp_channel(float v)
	{
		if (v < 0.f) return 0;
		if (v > 255.f) return 255;
		return static_cast<int>(v + 0.5f);
	}

	static std::uint32_t material_to_color(std::int16_t mat)
	{
		switch (mat)
		{
		case 256:  return IM_COL32(200, 200, 200, 255);
		case 288:  return IM_COL32(255, 100, 255, 255);
		case 512:  return IM_COL32(139, 90, 43, 255);
		case 800:  return IM_COL32(100, 100, 110, 255);
		case 816:  return IM_COL32(140, 140, 145, 255);
		case 1040: return IM_COL32(80, 85, 90, 255);
		case 1056: return IM_COL32(140, 145, 150, 255);
		case 1072: return IM_COL32(180, 180, 190, 255);
		case 1088: return IM_COL32(90, 120, 60, 255);
		case 1104: return IM_COL32(180, 220, 255, 255);
		case 1120: return IM_COL32(160, 165, 170, 255);
		case 1136: return IM_COL32(130, 125, 120, 255);
		case 1152: return IM_COL32(160, 80, 60, 255);
		case 1168: return IM_COL32(130, 125, 115, 255);
		case 1184: return IM_COL32(195, 175, 130, 255);
		case 1200: return IM_COL32(200, 180, 160, 255);
		case 1280: return IM_COL32(200, 220, 255, 255);
		case 1536: return IM_COL32(100, 150, 255, 180);
		case 880:  return IM_COL32(70, 70, 75, 255);
		case 848:  return IM_COL32(55, 55, 60, 255);
		case 864:  return IM_COL32(95, 95, 100, 255);
		case 832:  return IM_COL32(120, 115, 110, 255);
		case 896:  return IM_COL32(200, 60, 40, 255);
		case 928:  return IM_COL32(220, 220, 235, 255);
		case 912:  return IM_COL32(240, 240, 250, 255);
		case 944:  return IM_COL32(200, 210, 220, 255);
		case 960:  return IM_COL32(235, 235, 240, 255);
		case 976:  return IM_COL32(245, 245, 250, 255);
		case 992:  return IM_COL32(250, 250, 255, 255);
		case 1008: return IM_COL32(255, 255, 255, 255);
		case 1024: return IM_COL32(100, 140, 180, 255);
		case 1048: return IM_COL32(85, 95, 100, 255);
		case 1064: return IM_COL32(150, 140, 120, 255);
		case 1096: return IM_COL32(190, 185, 175, 255);
		case 1112: return IM_COL32(175, 170, 160, 255);
		case 1144: return IM_COL32(140, 135, 125, 255);
		case 1176: return IM_COL32(210, 200, 175, 255);
		case 1192: return IM_COL32(225, 215, 190, 255);
		case 1216: return IM_COL32(240, 235, 220, 255);
		case 1232: return IM_COL32(255, 250, 240, 255);
		case 1248: return IM_COL32(200, 195, 185, 255);
		case 1264: return IM_COL32(180, 175, 165, 255);
		case 1288: return IM_COL32(220, 230, 245, 255);
		case 1312: return IM_COL32(100, 180, 255, 255);
		case 1328: return IM_COL32(255, 200, 120, 255);
		case 1344: return IM_COL32(255, 140, 80, 255);
		case 1360: return IM_COL32(255, 100, 60, 255);
		case 1376: return IM_COL32(200, 100, 255, 255);
		case 1392: return IM_COL32(255, 255, 200, 255);
		case 1408: return IM_COL32(180, 220, 255, 255);
		case 1424: return IM_COL32(160, 200, 255, 255);
		case 1440: return IM_COL32(140, 180, 240, 255);
		case 1456: return IM_COL32(120, 160, 220, 255);
		case 1472: return IM_COL32(100, 140, 200, 255);
		case 1488: return IM_COL32(80, 120, 180, 255);
		case 1504: return IM_COL32(60, 100, 160, 255);
		case 1520: return IM_COL32(40, 80, 140, 255);
		default:
		{
			std::uint32_t u = static_cast<std::uint32_t>(static_cast<std::int16_t>(mat)) * 0x9e3779b1u;
			int r = 85 + static_cast<int>(u & 0x4fu);
			int g = 85 + static_cast<int>((u >> 9) & 0x4fu);
			int b = 85 + static_cast<int>((u >> 18) & 0x4fu);
			return IM_COL32(r, g, b, 255);
		}
		}
	}

	static std::uint32_t read_part_color_rgba(std::uint64_t part_address, std::uint64_t prim_address)
	{
		if (!part_address)
			return material_to_color(256);

		const std::uintptr_t color_offset = part_address + Offsets::BasePart::Color3;

		std::uint64_t ptr = memory->read<std::uint64_t>(color_offset);
		if (ptr && ptr < 0x7FFFFFFFFFFFull)
		{
			math::vector3 rgb = memory->read<math::vector3>(ptr);
			if (std::isfinite(rgb.x) && std::isfinite(rgb.y) && std::isfinite(rgb.z))
			{
				float mx = (std::max)((std::max)(rgb.x, rgb.y), rgb.z);
				float mn = (std::min)((std::min)(rgb.x, rgb.y), rgb.z);
				if (mx <= 1.2f && mn >= -0.02f)
					return IM_COL32(clamp_channel(rgb.x * 255.f), clamp_channel(rgb.y * 255.f), clamp_channel(rgb.z * 255.f), 255);
			}
		}

		math::vector3 rgb = memory->read<math::vector3>(color_offset);
		if (std::isfinite(rgb.x) && std::isfinite(rgb.y) && std::isfinite(rgb.z))
		{
			float mx = (std::max)((std::max)(rgb.x, rgb.y), rgb.z);
			float mn = (std::min)((std::min)(rgb.x, rgb.y), rgb.z);
			if (mx <= 1.2f && mn >= -0.02f)
				return IM_COL32(clamp_channel(rgb.x * 255.f), clamp_channel(rgb.y * 255.f), clamp_channel(rgb.z * 255.f), 255);
			if (mx <= 260.f && mn >= 0.f && mx > 1.5f)
				return IM_COL32(clamp_channel(rgb.x), clamp_channel(rgb.y), clamp_channel(rgb.z), 255);
		}

		std::uint32_t p = memory->read<std::uint32_t>(color_offset);
		int r = static_cast<int>(p & 0xFFu);
		int g = static_cast<int>((p >> 8) & 0xFFu);
		int b = static_cast<int>((p >> 16) & 0xFFu);
		if (r != 0 || g != 0 || b != 0)
			return IM_COL32(r, g, b, 255);

		r = static_cast<int>((p >> 16) & 0xFFu);
		g = static_cast<int>((p >> 8) & 0xFFu);
		b = static_cast<int>(p & 0xFFu);
		if (r != 0 || g != 0 || b != 0)
			return IM_COL32(r, g, b, 255);

		if (prim_address)
		{
			std::int16_t mat0 = memory->read<std::int16_t>(prim_address + Offsets::Primitive::Material);
			if (mat0 != 0 && mat0 >= 128 && mat0 <= 4096)
				return material_to_color(mat0);
			std::int16_t mat1 = memory->read<std::int16_t>(prim_address + 0x246);
			if (mat1 != 0 && mat1 >= 128 && mat1 <= 4096)
				return material_to_color(mat1);
		}
		return material_to_color(256);
	}

	static bool is_basepart_class(const std::string& className)
	{
		return className == "BasePart" ||
			className == "Part" ||
			className == "MeshPart" ||
			className == "WedgePart" ||
			className == "CornerWedgePart" ||
			className == "Ball" ||
			className == "Cylinder" ||
			className == "UnionOperation" ||
			className == "TrussPart" ||
			className == "TriangleMeshPart" ||
			className == "SmoothVoxelPart" ||
			className == "PartOperation";
	}
}

void c_wallcheck::find_valid_parts(std::vector<rbx::instance_t> instances, std::vector<rbx::primitive_t>& valid, std::int32_t depth) {
	if (depth > 96)
		return;

	for (rbx::instance_t child : instances) {
		std::string className = child.get_class_name();

		if (is_basepart_class(className)) {
			rbx::part_t part(child.address);
			rbx::primitive_t prim = part.get_primitive();
			valid.push_back(prim);

			math::vector3 center = prim.get_position();
			math::vector3 size = prim.get_size();
			math::matrix3 rotation = prim.get_rotation();
			math::cframe cf(center, rotation);

			const float mn = (std::min)((std::min)(size.x, size.y), size.z);
			const float mx = (std::max)((std::max)(size.x, size.y), size.z);
			if (!std::isfinite(mn) || !std::isfinite(mx) || mn < 0.08f || mx > 8000.f)
				continue;

			const bool for_raycast = mx <= 200.f && mn >= 1.f;

			rbx::obb obb(center, size, cf);
			const std::uint32_t col = read_part_color_rgba(child.address, prim.address);

			if (for_raycast)
			{
				obstacles.push_back(obb);
				obstacle_colors.push_back(col);
			}
			radar_geometry.push_back(obb);
			radar_geometry_colors.push_back(col);
		}
		if (className == "Folder") {
			find_valid_parts(child.get_children<rbx::instance_t>(), valid, depth + 1);
		}
		else if (className == "Model") {
			rbx::instance_t humanoid = child.find_first_child_by_class("Humanoid");
			if (humanoid.address != 0) {
				continue;
			}
			find_valid_parts(child.get_children<rbx::instance_t>(), valid, depth + 1);
		}
		else if (className == "Actor" || className == "WorldRoot" || className == "WorldModel") {
			find_valid_parts(child.get_children<rbx::instance_t>(), valid, depth + 1);
		}
	}
}

bool c_wallcheck::cache_workspace() {
	if (game::datamodel.address == 0) {
		return false;
	}

	rbx::instance_t workspace = game::datamodel.find_first_child_by_class("Workspace");
	if (workspace.address == 0) {
		return false;
	}

	obstacles.clear();
	obstacle_colors.clear();
	radar_geometry.clear();
	radar_geometry_colors.clear();
	parts.clear();

	std::vector<rbx::instance_t> children = workspace.get_children<rbx::instance_t>();

	std::vector<rbx::primitive_t> valid;
	find_valid_parts(children, valid, -1);

	if (valid.empty()) {
		return false;
	}

	parts = valid;
	return true;
}

bool c_wallcheck::is_visible(const math::vector3& origin, const math::vector3& target) {
	math::vector3 dir = (target - origin).normalized();
	float distance = (target - origin).length();

	const float max_obb_size = 5.f;

	for (const rbx::obb& box : get_obstacles()) {
		float max_obb_extent = max(max(box.half_size.x, box.half_size.y), box.half_size.z);

		float dist_to_center = (box.center - origin).length();
		if (dist_to_center > distance + max_obb_extent) continue;

		if (box.intersects(origin, dir, distance)) {
			return false;
		}
	}

	return true;
}

void c_wallcheck::draw_debug() {
	return;

	const math::matrix4 view = game::visengine.get_viewmatrix();
	const math::vector2 dims = game::visengine.get_dimensions();
	ImDrawList* draw = ImGui::GetBackgroundDrawList();

	const ImU32 box_color = IM_COL32(255, 0, 255, 255);

	for (const rbx::obb& box : obstacles) {
		math::vector3 corners[8];

		for (int i = 0; i < 8; ++i) {
			float sign_x = ((i & 1) != 0) ? 1.0f : -1.0f;
			float sign_y = ((i & 2) != 0) ? 1.0f : -1.0f;
			float sign_z = ((i & 4) != 0) ? 1.0f : -1.0f;

			corners[i] = box.center
				+ box.axes[0] * (box.half_size.x * sign_x)
				+ box.axes[1] * (box.half_size.y * sign_y)
				+ box.axes[2] * (box.half_size.z * sign_z);
		}

		math::vector2 screen_corners[8];
		bool valid_corners[8] = { false };
		int valid_count = 0;

		for (int i = 0; i < 8; ++i) {
			if (game::visengine.world_to_screen(corners[i], screen_corners[i], dims, view)) {
				valid_corners[i] = true;
				valid_count++;
			}
		}

		if (valid_count < 2) {
			continue;
		}

		const int edges[12][2] = {
			{0, 1}, {1, 3}, {3, 2}, {2, 0},
			{4, 5}, {5, 7}, {7, 6}, {6, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}
		};

		for (int i = 0; i < 12; ++i) {
			int idx1 = edges[i][0];
			int idx2 = edges[i][1];

			if (valid_corners[idx1] && valid_corners[idx2]) {
				ImVec2 p1(screen_corners[idx1].x, screen_corners[idx1].y);
				ImVec2 p2(screen_corners[idx2].x, screen_corners[idx2].y);
				draw->AddLine(p1, p2, box_color, 1.0f);
			}
		}
	}
}

const std::vector<rbx::obb>& c_wallcheck::get_obstacles() {
	return obstacles;
}

const std::vector<rbx::obb>& c_wallcheck::get_radar_geometry() const {
	return radar_geometry;
}

const std::vector<std::uint32_t>& c_wallcheck::get_radar_geometry_colors() const {
	return radar_geometry_colors;
}