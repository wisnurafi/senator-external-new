#include "visuals_preview.h"

#include <Offsets/Offsets.hpp>
#include <cache/cache.h>
#include <features/avatarmanager/avatarmanager.h>
#include <memory/memory.h>
#include <render/render.h>
#include <settings.h>

#include "../../../../ext/imgui/addons/imgui_addons.h"
#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace
{
    ImU32 color_from_setting(const float color[4], float alpha_multiplier = 1.0f)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(
            color[0],
            color[1],
            color[2],
            std::clamp(color[3] * alpha_multiplier, 0.0f, 1.0f)));
    }

    void draw_preview_corner_box(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float thickness)
    {
        const float width = max.x - min.x;
        const float height = max.y - min.y;
        const float line_w = width * 0.28f;
        const float line_h = height * 0.20f;
        const ImU32 backing = IM_COL32(0, 0, 0, 220);
        const float backing_thickness = thickness + 2.0f;

        draw->AddLine(min, ImVec2(min.x + line_w, min.y), backing, backing_thickness);
        draw->AddLine(min, ImVec2(min.x, min.y + line_h), backing, backing_thickness);
        draw->AddLine(ImVec2(max.x, min.y), ImVec2(max.x - line_w, min.y), backing, backing_thickness);
        draw->AddLine(ImVec2(max.x, min.y), ImVec2(max.x, min.y + line_h), backing, backing_thickness);
        draw->AddLine(ImVec2(min.x, max.y), ImVec2(min.x + line_w, max.y), backing, backing_thickness);
        draw->AddLine(ImVec2(min.x, max.y), ImVec2(min.x, max.y - line_h), backing, backing_thickness);
        draw->AddLine(max, ImVec2(max.x - line_w, max.y), backing, backing_thickness);
        draw->AddLine(max, ImVec2(max.x, max.y - line_h), backing, backing_thickness);
        draw->AddLine(min, ImVec2(min.x + line_w, min.y), col, thickness);
        draw->AddLine(min, ImVec2(min.x, min.y + line_h), col, thickness);
        draw->AddLine(ImVec2(max.x, min.y), ImVec2(max.x - line_w, min.y), col, thickness);
        draw->AddLine(ImVec2(max.x, min.y), ImVec2(max.x, min.y + line_h), col, thickness);
        draw->AddLine(ImVec2(min.x, max.y), ImVec2(min.x + line_w, max.y), col, thickness);
        draw->AddLine(ImVec2(min.x, max.y), ImVec2(min.x, max.y - line_h), col, thickness);
        draw->AddLine(max, ImVec2(max.x - line_w, max.y), col, thickness);
        draw->AddLine(max, ImVec2(max.x, max.y - line_h), col, thickness);
    }

    ImVec2 preview_add(ImVec2 lhs, ImVec2 rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
    ImVec2 preview_sub(ImVec2 lhs, ImVec2 rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
    ImVec2 preview_mul(ImVec2 value, float scalar) { return ImVec2(value.x * scalar, value.y * scalar); }

    void draw_preview_limb_part(ImDrawList* draw, ImVec2 from, ImVec2 to, float width, ImU32 fill, ImU32 outline, ImU32 shadow)
    {
        ImVec2 delta = preview_sub(to, from);
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length <= 0.01f) return;

        const ImVec2 normal(-delta.y / length, delta.x / length);
        const float outline_width = width + 3.0f;
        const ImVec2 fill_offset = preview_mul(normal, width * 0.5f);
        const ImVec2 outline_offset = preview_mul(normal, outline_width * 0.5f);
        const ImVec2 shadow_offset(2.0f, 3.0f);

        ImVec2 shadow_points[4] = {
            preview_add(preview_add(from, outline_offset), shadow_offset),
            preview_add(preview_add(to, outline_offset), shadow_offset),
            preview_add(preview_sub(to, outline_offset), shadow_offset),
            preview_add(preview_sub(from, outline_offset), shadow_offset)
        };
        draw->AddConvexPolyFilled(shadow_points, 4, shadow);

        ImVec2 outline_points[4] = {
            preview_add(from, outline_offset),
            preview_add(to, outline_offset),
            preview_sub(to, outline_offset),
            preview_sub(from, outline_offset)
        };
        draw->AddConvexPolyFilled(outline_points, 4, outline);

        ImVec2 fill_points[4] = {
            preview_add(from, fill_offset),
            preview_add(to, fill_offset),
            preview_sub(to, fill_offset),
            preview_sub(from, fill_offset)
        };
        draw->AddConvexPolyFilled(fill_points, 4, fill);
        draw->AddPolyline(fill_points, 4, outline, ImDrawFlags_Closed, 1.0f);
    }

    void draw_preview_mesh_part(ImDrawList* draw, const ImVec2* points, int count, ImU32 fill, ImU32 outline, ImU32 shadow)
    {
        ImVec2 shadow_points[8]{};
        if (count > 8) return;

        for (int i = 0; i < count; ++i)
            shadow_points[i] = ImVec2(points[i].x + 2.0f, points[i].y + 3.0f);

        draw->AddConvexPolyFilled(shadow_points, count, shadow);
        draw->AddConvexPolyFilled(points, count, fill);
        draw->AddPolyline(points, count, outline, ImDrawFlags_Closed, 1.4f);
    }

    void draw_preview_mesh_edge(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 color, float thickness)
    {
        draw->AddLine(ImVec2(a.x + 1.0f, a.y + 1.0f), ImVec2(b.x + 1.0f, b.y + 1.0f), IM_COL32(0, 0, 0, 150), thickness + 1.0f);
        draw->AddLine(a, b, color, thickness);
    }

    std::uint64_t get_visuals_preview_user_id(bool client_preview)
    {
        std::uint64_t user_id = 0;

        try
        {
            std::lock_guard<std::mutex> lock(cache::mtx);
            const cache::entity_t* preview_entity = nullptr;

            if (client_preview && cache::cached_local_player.instance.address)
            {
                preview_entity = &cache::cached_local_player;
            }
            else
            {
                for (const auto& player : cache::cached_players)
                {
                    if (!player.instance.address || player.instance.address == cache::cached_local_player.instance.address)
                        continue;

                    preview_entity = &player;
                    break;
                }
            }

            if (memory && preview_entity && preview_entity->instance.address)
                user_id = memory->read<std::uint64_t>(preview_entity->instance.address + Offsets::Player::UserId);
        }
        catch (...)
        {
            user_id = 0;
        }

        return (user_id != 0 && user_id != 0xFFFFFFFFFFFFFFFF) ? user_id : 1ULL;
    }

    ImTextureID get_visuals_preview_avatar(bool client_preview)
    {
        if (!g_avatar_manager)
            return ImTextureID{};

        const std::uint64_t user_id = get_visuals_preview_user_id(client_preview);
        g_avatar_manager->requestAvatar(user_id);
        return g_avatar_manager->getAvatarTexture(user_id);
    }

    void draw_preview_avatar_image(ImDrawList* draw, ImTextureID avatar_texture, ImVec2 min, ImVec2 max, bool chams_enabled, const float* chams_fill, const float* chams_outline)
    {
        if (!avatar_texture)
            return;

        if (chams_enabled)
        {
            const ImU32 outline = color_from_setting(chams_outline, 0.88f);
            const ImU32 fill = color_from_setting(chams_fill, 0.72f);
            const ImU32 base = IM_COL32(255, 255, 255, 92);

            draw->AddImage(avatar_texture, min, max, ImVec2(0, 0), ImVec2(1, 1), base);
            draw->AddImage(avatar_texture, ImVec2(min.x - 1.5f, min.y), ImVec2(max.x - 1.5f, max.y), ImVec2(0, 0), ImVec2(1, 1), outline);
            draw->AddImage(avatar_texture, ImVec2(min.x + 1.5f, min.y), ImVec2(max.x + 1.5f, max.y), ImVec2(0, 0), ImVec2(1, 1), outline);
            draw->AddImage(avatar_texture, ImVec2(min.x, min.y - 1.5f), ImVec2(max.x, max.y - 1.5f), ImVec2(0, 0), ImVec2(1, 1), outline);
            draw->AddImage(avatar_texture, ImVec2(min.x, min.y + 1.5f), ImVec2(max.x, max.y + 1.5f), ImVec2(0, 0), ImVec2(1, 1), outline);
            draw->AddImage(avatar_texture, min, max, ImVec2(0, 0), ImVec2(1, 1), fill);
            return;
        }

        draw->AddImage(avatar_texture, min, max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 245));
    }

    void draw_visuals_preview_model(ImDrawList* draw, const ImVec2& center, float scale, bool client_preview, ImTextureID avatar_texture)
    {
        const bool box_enabled = client_preview ? settings::visuals::client_box : settings::visuals::box;
        const bool box_fill_enabled = client_preview ? settings::visuals::client_box_fill : settings::visuals::box_fill;
        const bool skeleton_enabled = client_preview ? settings::visuals::client_skeleton : settings::visuals::skeleton;
        const bool name_enabled = client_preview ? settings::visuals::client_name : settings::visuals::name;
        const bool healthbar_enabled = client_preview ? settings::visuals::client_healthbar : settings::visuals::healthbar;
        const bool health_percent_enabled = client_preview ? settings::visuals::client_health_percent : settings::visuals::health_percent;
        const bool armorbar_enabled = client_preview ? settings::visuals::client_armorbar : settings::visuals::armorbar;
        const bool distance_enabled = client_preview ? settings::visuals::client_distance : settings::visuals::distance;
        const bool tool_enabled = client_preview ? settings::visuals::client_tool : settings::visuals::tool;
        const bool flags_enabled = client_preview ? settings::visuals::client_flags : settings::visuals::flags;
        const bool chams_enabled = client_preview ? settings::visuals::client_chams : settings::visuals::chams;

        const float* box_color = client_preview ? settings::visuals::client_box_color : settings::visuals::box_color;
        const float* box_fill_color = client_preview ? settings::visuals::client_box_fill_color : settings::visuals::box_fill_color;
        const float* skeleton_color = client_preview ? settings::visuals::client_skeleton_color : settings::visuals::skeleton_color;
        const float* name_color = client_preview ? settings::visuals::client_name_color : settings::visuals::name_color;
        const float* healthbar_color = client_preview ? settings::visuals::client_healthbar_color : settings::visuals::healthbar_color;
        const float* health_percent_color = client_preview ? settings::visuals::client_health_percent_color : settings::visuals::health_percent_color;
        const float* armorbar_color = client_preview ? settings::visuals::client_armorbar_color : settings::visuals::armorbar_color;
        const float* distance_color = client_preview ? settings::visuals::client_distance_color : settings::visuals::distance_color;
        const float* tool_color = client_preview ? settings::visuals::client_tool_color : settings::visuals::tool_color;
        const float* flags_color = client_preview ? settings::visuals::client_flags_state_colour : settings::visuals::flags_state_colour;
        const float* chams_fill = client_preview ? settings::visuals::client_chams_fill_color : settings::visuals::chams_fill_color;
        const float* chams_outline = client_preview ? settings::visuals::client_chams_outline_color : settings::visuals::chams_outline_color;

        const ImVec2 box_min(center.x - 42.0f * scale, center.y - 92.0f * scale);
        const ImVec2 box_max(center.x + 42.0f * scale, center.y + 86.0f * scale);
        const ImVec2 head(center.x, center.y - 62.0f * scale);
        const ImVec2 neck(center.x, center.y - 36.0f * scale);
        const ImVec2 pelvis(center.x, center.y + 26.0f * scale);
        const ImVec2 left_shoulder(center.x - 26.0f * scale, center.y - 24.0f * scale);
        const ImVec2 right_shoulder(center.x + 26.0f * scale, center.y - 24.0f * scale);
        const ImVec2 left_hand(center.x - 43.0f * scale, center.y + 22.0f * scale);
        const ImVec2 right_hand(center.x + 43.0f * scale, center.y + 22.0f * scale);
        const ImVec2 left_foot(center.x - 24.0f * scale, center.y + 82.0f * scale);
        const ImVec2 right_foot(center.x + 24.0f * scale, center.y + 82.0f * scale);
        const ImVec2 avatar_min(center.x - 72.0f * scale, center.y - 104.0f * scale);
        const ImVec2 avatar_max(center.x + 72.0f * scale, center.y + 94.0f * scale);
        const bool has_avatar_texture = avatar_texture != ImTextureID{};

        if (box_fill_enabled)
            draw->AddRectFilled(box_min, box_max, color_from_setting(box_fill_color), 2.0f);

        const ImU32 base_fill = chams_enabled ? color_from_setting(chams_fill, 0.78f) : IM_COL32(42, 45, 50, 210);
        const ImU32 base_outline = chams_enabled ? color_from_setting(chams_outline) : IM_COL32(210, 215, 224, 230);
        const ImU32 subtle_shadow = IM_COL32(0, 0, 0, 80);

        if (has_avatar_texture)
        {
            draw_preview_avatar_image(draw, avatar_texture, avatar_min, avatar_max, chams_enabled, chams_fill, chams_outline);
        }
        else if (chams_enabled)
        {
            const ImU32 part_fill = color_from_setting(chams_fill, 0.62f);
            const ImU32 part_outline = color_from_setting(chams_outline, 0.98f);
            const ImU32 inner_line = color_from_setting(chams_outline, 0.32f);

            const ImVec2 head_part[4] = {
                ImVec2(center.x - 12.0f * scale, center.y - 78.0f * scale),
                ImVec2(center.x + 14.0f * scale, center.y - 75.0f * scale),
                ImVec2(center.x + 12.0f * scale, center.y - 51.0f * scale),
                ImVec2(center.x - 13.0f * scale, center.y - 53.0f * scale)
            };
            const ImVec2 torso_part[4] = {
                ImVec2(center.x - 25.0f * scale, center.y - 49.0f * scale),
                ImVec2(center.x + 27.0f * scale, center.y - 44.0f * scale),
                ImVec2(center.x + 33.0f * scale, center.y + 23.0f * scale),
                ImVec2(center.x - 31.0f * scale, center.y + 29.0f * scale)
            };
            const ImVec2 left_arm_part[4] = {
                ImVec2(center.x - 29.0f * scale, center.y - 43.0f * scale),
                ImVec2(center.x - 50.0f * scale, center.y - 30.0f * scale),
                ImVec2(center.x - 58.0f * scale, center.y + 22.0f * scale),
                ImVec2(center.x - 34.0f * scale, center.y + 15.0f * scale)
            };
            const ImVec2 right_arm_part[4] = {
                ImVec2(center.x + 28.0f * scale, center.y - 42.0f * scale),
                ImVec2(center.x + 53.0f * scale, center.y - 27.0f * scale),
                ImVec2(center.x + 57.0f * scale, center.y + 21.0f * scale),
                ImVec2(center.x + 35.0f * scale, center.y + 15.0f * scale)
            };
            const ImVec2 left_leg_part[4] = {
                ImVec2(center.x - 25.0f * scale, center.y + 24.0f * scale),
                ImVec2(center.x - 3.0f * scale, center.y + 23.0f * scale),
                ImVec2(center.x - 13.0f * scale, center.y + 74.0f * scale),
                ImVec2(center.x - 39.0f * scale, center.y + 72.0f * scale)
            };
            const ImVec2 right_leg_part[4] = {
                ImVec2(center.x + 6.0f * scale, center.y + 23.0f * scale),
                ImVec2(center.x + 27.0f * scale, center.y + 21.0f * scale),
                ImVec2(center.x + 42.0f * scale, center.y + 69.0f * scale),
                ImVec2(center.x + 17.0f * scale, center.y + 74.0f * scale)
            };

            draw_preview_mesh_part(draw, left_arm_part, 4, part_fill, part_outline, subtle_shadow);
            draw_preview_mesh_part(draw, right_arm_part, 4, part_fill, part_outline, subtle_shadow);
            draw_preview_mesh_part(draw, left_leg_part, 4, part_fill, part_outline, subtle_shadow);
            draw_preview_mesh_part(draw, right_leg_part, 4, part_fill, part_outline, subtle_shadow);
            draw_preview_mesh_part(draw, torso_part, 4, part_fill, part_outline, subtle_shadow);
            draw_preview_mesh_part(draw, head_part, 4, part_fill, part_outline, subtle_shadow);

            draw_preview_mesh_edge(draw, ImVec2(center.x, center.y - 75.0f * scale), ImVec2(center.x, center.y + 58.0f * scale), inner_line, 1.0f);
            draw_preview_mesh_edge(draw, torso_part[0], torso_part[2], inner_line, 1.0f);
            draw_preview_mesh_edge(draw, torso_part[1], torso_part[3], inner_line, 1.0f);
            draw_preview_mesh_edge(draw, ImVec2(center.x - 14.0f * scale, center.y + 25.0f * scale), ImVec2(center.x - 30.0f * scale, center.y + 69.0f * scale), inner_line, 1.0f);
            draw_preview_mesh_edge(draw, ImVec2(center.x + 16.0f * scale, center.y + 24.0f * scale), ImVec2(center.x + 32.0f * scale, center.y + 68.0f * scale), inner_line, 1.0f);
        }
        else
        {
            const ImVec2 torso_top_left(center.x - 21.0f * scale, center.y - 32.0f * scale);
            const ImVec2 torso_top_right(center.x + 21.0f * scale, center.y - 32.0f * scale);
            const ImVec2 torso_bottom_right(center.x + 27.0f * scale, center.y + 34.0f * scale);
            const ImVec2 torso_bottom_left(center.x - 18.0f * scale, center.y + 42.0f * scale);
            const float limb_width = 10.0f * scale;

            draw_preview_limb_part(draw, left_shoulder, left_hand, limb_width, base_fill, base_outline, subtle_shadow);
            draw_preview_limb_part(draw, right_shoulder, right_hand, limb_width, base_fill, base_outline, subtle_shadow);
            draw_preview_limb_part(draw, ImVec2(center.x - 10.0f * scale, center.y + 32.0f * scale), left_foot, limb_width, base_fill, base_outline, subtle_shadow);
            draw_preview_limb_part(draw, ImVec2(center.x + 12.0f * scale, center.y + 31.0f * scale), right_foot, limb_width, base_fill, base_outline, subtle_shadow);

            draw->AddQuadFilled(ImVec2(torso_top_left.x + 2.0f, torso_top_left.y + 3.0f), ImVec2(torso_top_right.x + 2.0f, torso_top_right.y + 3.0f), ImVec2(torso_bottom_right.x + 2.0f, torso_bottom_right.y + 3.0f), ImVec2(torso_bottom_left.x + 2.0f, torso_bottom_left.y + 3.0f), subtle_shadow);
            draw->AddQuadFilled(ImVec2(torso_top_left.x - 2.0f, torso_top_left.y - 2.0f), ImVec2(torso_top_right.x + 2.0f, torso_top_right.y - 2.0f), ImVec2(torso_bottom_right.x + 2.0f, torso_bottom_right.y + 2.0f), ImVec2(torso_bottom_left.x - 2.0f, torso_bottom_left.y + 2.0f), base_outline);
            draw->AddQuadFilled(torso_top_left, torso_top_right, torso_bottom_right, torso_bottom_left, base_fill);
            draw->AddQuad(torso_top_left, torso_top_right, torso_bottom_right, torso_bottom_left, base_outline, 1.0f);

            const ImVec2 head_min(head.x - 15.0f * scale, head.y - 15.0f * scale);
            const ImVec2 head_max(head.x + 15.0f * scale, head.y + 15.0f * scale);
            draw->AddRectFilled(ImVec2(head_min.x + 2.0f, head_min.y + 3.0f), ImVec2(head_max.x + 2.0f, head_max.y + 3.0f), subtle_shadow, 3.0f);
            draw->AddRectFilled(ImVec2(head_min.x - 2.0f, head_min.y - 2.0f), ImVec2(head_max.x + 2.0f, head_max.y + 2.0f), base_outline, 3.0f);
            draw->AddRectFilled(head_min, head_max, base_fill, 2.0f);
            draw->AddRect(head_min, head_max, base_outline, 2.0f, 0, 1.0f);
        }

        if (skeleton_enabled)
        {
            const ImU32 skel = color_from_setting(skeleton_color);
            draw->AddLine(head, neck, skel, 1.5f);
            draw->AddLine(neck, pelvis, skel, 1.5f);
            draw->AddLine(left_shoulder, right_shoulder, skel, 1.5f);
            draw->AddLine(left_shoulder, left_hand, skel, 1.5f);
            draw->AddLine(right_shoulder, right_hand, skel, 1.5f);
            draw->AddLine(pelvis, left_foot, skel, 1.5f);
            draw->AddLine(pelvis, right_foot, skel, 1.5f);
        }

        if (box_enabled)
        {
            if (settings::visuals::box_type == 1)
                draw_preview_corner_box(draw, box_min, box_max, color_from_setting(box_color), 1.5f);
            else
                draw->AddRect(box_min, box_max, color_from_setting(box_color), 1.0f, 0, 1.5f);
        }

        if (healthbar_enabled)
        {
            const float bar_height = box_max.y - box_min.y;
            const ImVec2 bar_min(box_min.x - 8.0f, box_min.y);
            const ImVec2 bar_max(box_min.x - 4.0f, box_max.y);
            draw->AddRectFilled(bar_min, bar_max, IM_COL32(18, 18, 18, 220));
            draw->AddRectFilled(ImVec2(bar_min.x, bar_max.y - bar_height * 0.72f), bar_max, color_from_setting(healthbar_color));
        }

        if (armorbar_enabled)
        {
            const float bar_height = box_max.y - box_min.y;
            const ImVec2 bar_min(box_max.x + 4.0f, box_min.y);
            const ImVec2 bar_max(box_max.x + 8.0f, box_max.y);
            draw->AddRectFilled(bar_min, bar_max, IM_COL32(18, 18, 18, 220));
            draw->AddRectFilled(ImVec2(bar_min.x, bar_max.y - bar_height * 0.48f), bar_max, color_from_setting(armorbar_color));
        }

        if (name_enabled)
        {
            const char* label = client_preview ? "LocalPlayer" : "EnemyPlayer";
            const ImVec2 text_size = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(center.x - text_size.x * 0.5f, box_min.y - 20.0f), color_from_setting(name_color), label);
        }

        if (health_percent_enabled)
            draw->AddText(ImVec2(box_max.x + 12.0f, box_min.y + 8.0f), color_from_setting(health_percent_color), "72%");

        if (distance_enabled)
            draw->AddText(ImVec2(box_min.x, box_max.y + 8.0f), color_from_setting(distance_color), client_preview ? "0m" : "34m");

        if (tool_enabled)
            draw->AddText(ImVec2(box_min.x, box_max.y + 24.0f), color_from_setting(tool_color), client_preview ? "Tool: Sword" : "Tool: Rifle");

        if (flags_enabled)
            draw->AddText(ImVec2(box_max.x + 12.0f, box_min.y + 28.0f), color_from_setting(flags_color), client_preview ? "[LOCAL]" : "[VISIBLE]");

        if (!client_preview && settings::visuals::target_warning_icon)
        {
            const float radius = (std::max)(8.0f, settings::visuals::target_warning_icon_size * 0.5f);
            const ImVec2 warning(box_max.x + 20.0f + radius, box_min.y - 8.0f);
            draw->AddTriangleFilled(ImVec2(warning.x, warning.y - radius), ImVec2(warning.x - radius, warning.y + radius), ImVec2(warning.x + radius, warning.y + radius), IM_COL32(255, 190, 45, 230));
            draw->AddText(ImVec2(warning.x - 3.0f, warning.y - 5.0f), IM_COL32(30, 30, 30, 255), "!");
        }
    }
}

void esp::preview::render_visuals_3d_window()
{
    if (!settings::visuals::preview_3d) return;

    ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImAdd::HexToColorVec4(0x151515));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImAdd::HexToColorVec4(0x171717));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImAdd::HexToColorVec4(0x2c2236));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImAdd::HexToColorVec4(0x171717));
    ImGui::PushStyleColor(ImGuiCol_Border, ImAdd::HexToColorVec4(0x303030));
    ImGui::PushStyleColor(ImGuiCol_Text, ImAdd::HexToColorVec4(0xf2f2f2));
    if (!ImGui::Begin("Visuals 3D Preview", &settings::visuals::preview_3d, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(3);
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    canvas_size.x = (std::max)(canvas_size.x, 320.0f);
    canvas_size.y = (std::max)(canvas_size.y, 240.0f);

    const ImVec2 canvas_max(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);
    draw->AddRectFilled(canvas_pos, canvas_max, ImGui::GetColorU32(ImAdd::HexToColorVec4(0x101010, 1.0f)), 4.0f);
    draw->AddRect(canvas_pos, canvas_max, ImGui::GetColorU32(ImAdd::HexToColorVec4(0x2c2c2c, 1.0f)), 4.0f);
    draw->AddLine(
        ImVec2(canvas_pos.x + 1.0f, canvas_pos.y + 1.0f),
        ImVec2(canvas_max.x - 1.0f, canvas_pos.y + 1.0f),
        ImGui::GetColorU32(ImAdd::HexToColorVec4(0x3c2d49, 0.75f)),
        1.0f);

    const bool show_enemy = settings::visuals::enable_enemies || !settings::visuals::enable_client;
    const bool show_client = settings::visuals::enable_client;

    if (show_enemy && show_client)
    {
        draw_visuals_preview_model(draw, ImVec2(canvas_pos.x + canvas_size.x * 0.32f, canvas_pos.y + canvas_size.y * 0.55f), 0.92f, false, get_visuals_preview_avatar(false));
        draw_visuals_preview_model(draw, ImVec2(canvas_pos.x + canvas_size.x * 0.70f, canvas_pos.y + canvas_size.y * 0.55f), 0.92f, true, get_visuals_preview_avatar(true));
    }
    else
    {
        draw_visuals_preview_model(draw, ImVec2(canvas_pos.x + canvas_size.x * 0.50f, canvas_pos.y + canvas_size.y * 0.55f), 1.0f, show_client, get_visuals_preview_avatar(show_client));
    }

    ImGui::Dummy(canvas_size);
    ImGui::End();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);
}
