#include "support_notification.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

namespace
{
	struct Notification final
	{
		gamesupport::SupportReport report{};
		float duration_seconds{ 10.0f };
		std::chrono::steady_clock::time_point start_time{};
	};

	std::mutex g_mutex;
	std::vector<Notification> g_notifications;

	bool same_report(const gamesupport::SupportReport& a, const gamesupport::SupportReport& b)
	{
		return a.state == b.state &&
			a.title == b.title &&
			a.status_label == b.status_label &&
			a.detail == b.detail &&
			a.features_label == b.features_label;
	}

	ImVec4 accent_color(gamesupport::SupportState state)
	{
		switch (state)
		{
		case gamesupport::SupportState::ProfileReady:
			return ImVec4(0.33f, 0.27f, 0.39f, 1.0f);
		case gamesupport::SupportState::GenericReady:
			return ImVec4(0.40f, 0.35f, 0.46f, 1.0f);
		case gamesupport::SupportState::Failed:
		default:
			return ImVec4(0.52f, 0.22f, 0.24f, 1.0f);
		}
	}
}

void ui::support_notification::push(const gamesupport::SupportReport& report, float duration_seconds)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (!g_notifications.empty() && same_report(g_notifications.back().report, report))
		return;

	g_notifications.clear();
	g_notifications.push_back(Notification{ report, std::max(duration_seconds, 1.0f), std::chrono::steady_clock::now() });
}

void ui::support_notification::render()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_notifications.empty())
		return;

	Notification& notification = g_notifications.back();
	const auto now = std::chrono::steady_clock::now();
	const float elapsed = std::chrono::duration<float>(now - notification.start_time).count();
	if (elapsed >= notification.duration_seconds)
	{
		g_notifications.clear();
		return;
	}

	const float remaining = notification.duration_seconds - elapsed;
	const float alpha = std::clamp(remaining / 0.35f, 0.0f, 1.0f);
	const ImVec4 accent = accent_color(notification.report.state);
	const ImGuiIO& io = ImGui::GetIO();

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove;

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 18.0f, io.DisplaySize.y - 86.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
	ImGui::SetNextWindowBgAlpha(0.92f * alpha);
	ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Always);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 11.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.11f, 0.92f * alpha));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent.x, accent.y, accent.z, 0.95f * alpha));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.52f, 0.52f, 0.52f, alpha));

	if (ImGui::Begin("##support_notification", nullptr, flags))
	{
		const ImVec2 min = ImGui::GetWindowPos();
		const ImVec2 max = ImVec2(min.x + 4.0f, min.y + ImGui::GetWindowHeight());
		ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, alpha)));

		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 268.0f);
		ImGui::TextUnformatted(notification.report.title.c_str());

		if (notification.report.state == gamesupport::SupportState::Failed)
		{
			ImGui::TextUnformatted(notification.report.detail.c_str());
			ImGui::TextDisabled("%s", notification.report.features_label.c_str());
		}
		else
		{
			ImGui::TextUnformatted(notification.report.status_label.c_str());
			ImGui::TextDisabled("Features: %s", notification.report.features_label.c_str());
		}

		ImGui::PopTextWrapPos();
	}
	ImGui::End();

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);
}
