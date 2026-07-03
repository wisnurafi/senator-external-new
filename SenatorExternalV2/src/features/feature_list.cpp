#include "feature_list.h"

#include "feature_registry.h"

#include <settings.h>
#include <menu/menu.h>
#include <menu/keybind/keybind.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <string>
#include <string_view>
#include <vector>

namespace
{
	const char* mode_name(int mode)
	{
		switch (mode)
		{
		case 0: return "Toggle";
		case 1: return "Hold";
		case 2: return "Always";
		default: return "Key";
		}
	}

	std::string keybind_text(const features::FeatureDescriptor& feature)
	{
		if (feature.get_keybind == nullptr)
			return "Always";

		const int key = feature.get_keybind();
		const int mode = feature.get_activation_mode != nullptr ? feature.get_activation_mode() : 1;
		const char* key_name = keybind::get_key_name(key);

		if (key == 0 || key_name == nullptr || key_name[0] == '\0')
		{
			if (mode == 2)
				return "Always";

			return std::string("Not Bound (") + mode_name(mode) + ")";
		}

		return std::string(key_name) + " (" + mode_name(mode) + ")";
	}

	int feature_key(const features::FeatureDescriptor& feature)
	{
		if (feature.get_keybind == nullptr)
			return 0;

		return feature.get_keybind();
	}

	bool has_keybind_conflict(
		const features::FeatureDescriptor& feature,
		const std::vector<const features::FeatureDescriptor*>& active_features)
	{
		const int key = feature_key(feature);
		if (key == 0)
			return false;

		int matches = 0;
		for (const features::FeatureDescriptor* active_feature : active_features)
		{
			if (active_feature != nullptr && feature_key(*active_feature) == key)
				++matches;
		}

		return matches > 1;
	}

	void text_view(std::string_view text)
	{
		ImGui::TextUnformatted(text.data(), text.data() + text.size());
	}

	void text_colored_view(const ImVec4& color, std::string_view text)
	{
		ImGui::TextColored(color, "%.*s", static_cast<int>(text.size()), text.data());
	}
}

void features::render_active_feature_list()
{
	if (!settings::ui::keybinds)
		return;

	std::vector<const FeatureDescriptor*> active_features;
	for (std::size_t i = 0; i < registry_count(); ++i)
	{
		const FeatureDescriptor& feature = registry()[i];
		if (is_available(feature) && feature.is_enabled != nullptr && feature.is_enabled())
			active_features.push_back(&feature);
	}

	if (active_features.empty())
		return;

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav;

	if (!Menu::m_bMenuVisible)
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

	ImGui::SetNextWindowSize(ImVec2(305.0f, 0.0f), ImGuiCond_Always);
	if (!settings::feature_list::pos_initialized)
	{
		ImGui::SetNextWindowPos(ImVec2(settings::feature_list::pos_x, settings::feature_list::pos_y), ImGuiCond_Always);
		settings::feature_list::pos_initialized = true;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(11.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.11f, 0.92f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.52f, 0.52f, 0.52f, 1.0f));

	const bool opened = ImGui::Begin("##active_feature_list", nullptr, flags);
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(3);

	if (opened)
	{
		const ImVec2 pos = ImGui::GetWindowPos();
		settings::feature_list::pos_x = pos.x;
		settings::feature_list::pos_y = pos.y;

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImRect window_bb(window->Rect());
		const ImGuiStyle& style = ImGui::GetStyle();
		const ImU32 header = ImGui::GetColorU32(ImGuiCol_Header);
		const ImU32 header_active = ImGui::GetColorU32(ImGuiCol_HeaderActive);
		const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
		const ImU32 row_even = ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.13f, 0.48f));
		const ImU32 row_odd = ImGui::GetColorU32(ImVec4(0.10f, 0.10f, 0.10f, 0.25f));

		window->DrawList->AddLine(
			window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize),
			ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize),
			header,
			1.0f);
		window->DrawList->AddLine(
			window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize + 1.0f),
			ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize + 1.0f),
			header_active,
			1.0f);
		window->DrawList->AddLine(
			window_bb.Min + ImVec2(style.WindowBorderSize, style.WindowBorderSize + 2.0f),
			ImVec2(window_bb.Max.x - style.WindowBorderSize, window_bb.Min.y + style.WindowBorderSize + 2.0f),
			border,
			1.0f);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));
		ImGui::TextUnformatted("Active Features");
		ImGui::SameLine();
		const std::string active_count = std::to_string(active_features.size()) + " active";
		const float count_width = ImGui::CalcTextSize(active_count.c_str()).x;
		ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - count_width);
		ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "%s", active_count.c_str());
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 2.0f));

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 5.0f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 0.48f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.10f, 0.10f, 0.10f, 0.25f));
		if (ImGui::BeginTable("##active_feature_table", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Feature", ImGuiTableColumnFlags_WidthStretch, 1.15f);
			ImGui::TableSetupColumn("Keybind", ImGuiTableColumnFlags_WidthStretch, 0.85f);

			for (std::size_t row = 0; row < active_features.size(); ++row)
			{
				const FeatureDescriptor* feature = active_features[row];
				const std::string bind_text = keybind_text(*feature);
				const bool conflict = has_keybind_conflict(*feature, active_features);

				ImGui::TableNextRow();
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, (row % 2 == 0) ? row_even : row_odd);
				ImGui::TableSetColumnIndex(0);
				if (conflict)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.20f, 1.0f), "[!]");
					ImGui::SameLine(0.0f, 5.0f);
				}
				text_view(feature->label);
				ImGui::SameLine(0.0f, 6.0f);
				text_colored_view(ImVec4(0.52f, 0.52f, 0.52f, 1.0f), feature->category);

				ImGui::TableSetColumnIndex(1);
				if (conflict)
					ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.20f, 1.0f), "%s", bind_text.c_str());
				else
					ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.72f, 1.0f), "%s", bind_text.c_str());
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}
	ImGui::End();
}
