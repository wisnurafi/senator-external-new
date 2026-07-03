#define IMGUI_DEFINE_MATH_OPERATORS
#include "loader.h"

#include <branding/branding.h>
#include <branding/branding_assets.h>

#include <d3d11.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <windows.h>
#include <windowsx.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace loader
{
	namespace
	{
		constexpr int k_width = 700;
		constexpr int k_height = 450;

		struct loader_context_t
		{
			HWND window{};
			WNDCLASSEXA window_class{};
			ID3D11Device* device{};
			ID3D11DeviceContext* device_context{};
			IDXGISwapChain* swap_chain{};
			ID3D11RenderTargetView* render_target{};
			bool done{};
			bool accepted{};
		};

		loader_context_t* g_context{};

		bool process_exists(const wchar_t* process_name)
		{
			HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (snapshot == INVALID_HANDLE_VALUE)
				return false;

			PROCESSENTRY32W entry{};
			entry.dwSize = sizeof(entry);

			bool found = false;
			if (Process32FirstW(snapshot, &entry))
			{
				do
				{
					if (_wcsicmp(entry.szExeFile, process_name) == 0)
					{
						found = true;
						break;
					}
				} while (Process32NextW(snapshot, &entry));
			}

			CloseHandle(snapshot);
			return found;
		}

		bool shell_open(const wchar_t* target)
		{
			const HINSTANCE result = ShellExecuteW(nullptr, L"open", target, nullptr, nullptr, SW_SHOWNORMAL);
			return reinterpret_cast<INT_PTR>(result) > 32;
		}

		bool launch_local_roblox()
		{
			wchar_t local_app_data[MAX_PATH]{};
			const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
			if (len == 0 || len >= MAX_PATH)
				return false;

			const std::filesystem::path versions_root =
				std::filesystem::path(local_app_data) / L"Roblox" / L"Versions";

			if (!std::filesystem::exists(versions_root))
				return false;

			std::filesystem::path newest_player;
			std::filesystem::file_time_type newest_time{};

			try
			{
				for (const auto& entry : std::filesystem::directory_iterator(versions_root))
				{
					if (!entry.is_directory())
						continue;

					const std::filesystem::path player = entry.path() / L"RobloxPlayerBeta.exe";
					if (!std::filesystem::exists(player))
						continue;

					const auto write_time = std::filesystem::last_write_time(player);
					if (newest_player.empty() || write_time > newest_time)
					{
						newest_player = player;
						newest_time = write_time;
					}
				}
			}
			catch (...)
			{
				return false;
			}

			if (newest_player.empty())
				return false;

			return shell_open(newest_player.c_str());
		}

		void launch_roblox_if_needed()
		{
			if (process_exists(L"RobloxPlayerBeta.exe") || process_exists(L"Windows10Universal.exe"))
				return;

			if (shell_open(L"roblox:"))
				return;

			if (shell_open(L"roblox-player:"))
				return;

			(void)launch_local_roblox();
		}

		void create_render_target(loader_context_t& ctx)
		{
			ID3D11Texture2D* back_buffer{};
			if (ctx.swap_chain && SUCCEEDED(ctx.swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))) && back_buffer)
			{
				ctx.device->CreateRenderTargetView(back_buffer, nullptr, &ctx.render_target);
				back_buffer->Release();
			}
		}

		void cleanup_render_target(loader_context_t& ctx)
		{
			if (ctx.render_target)
			{
				ctx.render_target->Release();
				ctx.render_target = nullptr;
			}
		}

		LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
		{
			if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, w_param, l_param))
				return true;

			switch (msg)
			{
			case WM_NCHITTEST:
			{
				const LRESULT hit = DefWindowProcA(hwnd, msg, w_param, l_param);
				if (hit != HTCLIENT)
					return hit;

				POINT cursor{ GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param) };
				ScreenToClient(hwnd, &cursor);
				if (cursor.y >= 0 && cursor.y < 56)
				{
					if (cursor.x >= k_width - 44)
						return HTCLIENT;
					return HTCAPTION;
				}
				return HTCLIENT;
			}
			case WM_SIZE:
				if (g_context && g_context->device && w_param != SIZE_MINIMIZED)
				{
					cleanup_render_target(*g_context);
					g_context->swap_chain->ResizeBuffers(0, LOWORD(l_param), HIWORD(l_param), DXGI_FORMAT_UNKNOWN, 0);
					create_render_target(*g_context);
				}
				return 0;
			case WM_SYSCOMMAND:
				if ((w_param & 0xfff0) == SC_KEYMENU)
					return 0;
				break;
			case WM_CLOSE:
				if (g_context)
					g_context->done = true;
				DestroyWindow(hwnd);
				return 0;
			case WM_DESTROY:
				if (!g_context || !g_context->accepted)
					PostQuitMessage(0);
				return 0;
			}

			return DefWindowProcA(hwnd, msg, w_param, l_param);
		}

		bool create_window(loader_context_t& ctx)
		{
			ctx.window_class = {
				sizeof(WNDCLASSEXA),
				CS_CLASSDC,
				wnd_proc,
				0L,
				0L,
				GetModuleHandleA(nullptr),
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				"SenatorLoaderWindow",
				nullptr
			};

			RegisterClassExA(&ctx.window_class);

			const int screen_x = GetSystemMetrics(SM_CXSCREEN);
			const int screen_y = GetSystemMetrics(SM_CYSCREEN);
			const int x = (screen_x - k_width) / 2;
			const int y = (screen_y - k_height) / 2;

			ctx.window = CreateWindowExA(
				WS_EX_APPWINDOW,
				ctx.window_class.lpszClassName,
				branding::loader_title,
				WS_POPUP,
				x,
				y,
				k_width,
				k_height,
				nullptr,
				nullptr,
				ctx.window_class.hInstance,
				nullptr);

			if (!ctx.window)
				return false;

			ShowWindow(ctx.window, SW_SHOWDEFAULT);
			UpdateWindow(ctx.window);
			return true;
		}

		bool create_device(loader_context_t& ctx)
		{
			DXGI_SWAP_CHAIN_DESC desc{};
			desc.BufferCount = 2;
			desc.BufferDesc.Width = 0;
			desc.BufferDesc.Height = 0;
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.BufferDesc.RefreshRate.Numerator = 60;
			desc.BufferDesc.RefreshRate.Denominator = 1;
			desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.OutputWindow = ctx.window;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Windowed = TRUE;
			desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
			D3D_FEATURE_LEVEL feature_level{};

			HRESULT result = D3D11CreateDeviceAndSwapChain(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				0,
				feature_levels,
				2,
				D3D11_SDK_VERSION,
				&desc,
				&ctx.swap_chain,
				&ctx.device,
				&feature_level,
				&ctx.device_context);

			if (result == DXGI_ERROR_UNSUPPORTED)
			{
				result = D3D11CreateDeviceAndSwapChain(
					nullptr,
					D3D_DRIVER_TYPE_WARP,
					nullptr,
					0,
					feature_levels,
					2,
					D3D11_SDK_VERSION,
					&desc,
					&ctx.swap_chain,
					&ctx.device,
					&feature_level,
					&ctx.device_context);
			}

			if (FAILED(result))
				return false;

			create_render_target(ctx);
			return ctx.render_target != nullptr;
		}

		void apply_style()
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::StyleColorsDark();

			style.WindowRounding = 0.0f;
			style.ChildRounding = 0.0f;
			style.FrameRounding = 3.0f;
			style.PopupRounding = 0.0f;
			style.GrabRounding = 0.0f;
			style.ScrollbarRounding = 0.0f;
			style.WindowBorderSize = 1.0f;
			style.FrameBorderSize = 1.0f;
			style.WindowPadding = ImVec2(0.0f, 0.0f);
			style.FramePadding = ImVec2(10.0f, 8.0f);
			style.ItemSpacing = ImVec2(8.0f, 8.0f);
			style.ScrollbarSize = 9.0f;

			style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
			style.Colors[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.075f, 0.085f, 1.0f);
			style.Colors[ImGuiCol_PopupBg] = style.Colors[ImGuiCol_WindowBg];
			style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
			style.Colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.95f, 1.0f);
			style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.52f, 0.58f, 1.0f);
			style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
			style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.17f, 0.16f, 0.22f, 1.0f);
			style.Colors[ImGuiCol_ButtonActive] = branding::accent_dark();
			style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
			style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.13f, 0.13f, 0.15f, 1.0f);
			style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.14f, 0.22f, 1.0f);
			style.Colors[ImGuiCol_Header] = branding::accent_dark();
			style.Colors[ImGuiCol_HeaderHovered] = branding::accent();
			style.Colors[ImGuiCol_HeaderActive] = branding::accent_dark();
			style.Colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
		}

		void draw_text(ImDrawList* draw, ImVec2 pos, ImU32 color, const char* text)
		{
			draw->AddText(pos + ImVec2(1.0f, 1.0f), IM_COL32(0, 0, 0, 180), text);
			draw->AddText(pos, color, text);
		}

		void draw_panel_header(const char* label)
		{
			const ImVec2 cursor = ImGui::GetCursorScreenPos();
			ImDrawList* draw = ImGui::GetWindowDrawList();
			draw->AddRectFilled(cursor + ImVec2(0.0f, 1.0f), cursor + ImVec2(3.0f, 18.0f), branding::accent_u32(), 2.0f);
			ImGui::SetCursorScreenPos(cursor + ImVec2(14.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.82f, 1.0f));
			ImGui::TextUnformatted(label);
			ImGui::PopStyleColor();
			ImGui::Dummy(ImVec2(0.0f, 2.0f));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0.0f, 6.0f));
		}

		void label_value(const char* label, const char* value, bool positive = false)
		{
			ImGui::TextDisabled("%s", label);
			ImGui::SameLine(158.0f);
			if (positive)
				ImGui::TextColored(ImVec4(0.30f, 1.0f, 0.45f, 1.0f), "%s", value);
			else
				ImGui::TextUnformatted(value);
		}

		void render_loader(loader_context_t& ctx)
		{
			ImGuiIO& io = ImGui::GetIO();
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

			const ImGuiWindowFlags flags =
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse;

			ImGui::Begin("##senator_loader", nullptr, flags);

			ImDrawList* draw = ImGui::GetWindowDrawList();
			const ImVec2 win_min = ImGui::GetWindowPos();
			const ImVec2 win_max = win_min + io.DisplaySize;

			draw->AddRectFilled(win_min, win_max, IM_COL32(10, 10, 12, 255));
			draw->AddRectFilledMultiColor(
				win_min,
				ImVec2(win_max.x, win_min.y + 64.0f),
				IM_COL32(18, 18, 22, 255),
				IM_COL32(14, 13, 18, 255),
				IM_COL32(10, 10, 12, 255),
				IM_COL32(10, 10, 12, 255));
			draw->AddLine(win_min, ImVec2(win_max.x, win_min.y), branding::accent_u32(), 2.0f);
			draw->AddLine(win_min + ImVec2(0.0f, 2.0f), ImVec2(win_max.x, win_min.y + 2.0f), branding::accent_dark_u32(), 1.0f);
			draw->AddLine(win_min + ImVec2(24.0f, 64.0f), win_min + ImVec2(k_width - 24.0f, 64.0f), IM_COL32(31, 31, 36, 255), 1.0f);
			draw->AddRect(win_min, win_max, IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);

			branding::draw_logo_fit(ctx.device, draw, branding::brand_asset_t::icon, win_min + ImVec2(28.0f, 19.0f), win_min + ImVec2(66.0f, 43.0f));
			draw_text(draw, win_min + ImVec2(78.0f, 26.0f), IM_COL32(240, 240, 246, 255), branding::product_name);
			draw_text(draw, win_min + ImVec2(134.0f, 26.0f), IM_COL32(118, 118, 128, 255), "| Loader");

			draw->AddCircleFilled(win_min + ImVec2(586.0f, 35.0f), 3.0f, IM_COL32(46, 255, 93, 255));
			draw->AddRectFilled(win_min + ImVec2(602.0f, 22.0f), win_min + ImVec2(657.0f, 48.0f), IM_COL32(22, 22, 28, 255), 7.0f);
			draw->AddRect(win_min + ImVec2(602.0f, 22.0f), win_min + ImVec2(657.0f, 48.0f), IM_COL32(76, 68, 95, 255), 7.0f);
			draw_text(draw, win_min + ImVec2(612.0f, 29.0f), IM_COL32(180, 180, 190, 255), branding::version_short);

			ImGui::SetCursorScreenPos(win_min + ImVec2(670.0f, 22.0f));
			if (ImGui::InvisibleButton("##close_loader", ImVec2(20.0f, 20.0f)))
			{
				ctx.done = true;
				ctx.accepted = false;
			}
			draw_text(draw, win_min + ImVec2(677.0f, 28.0f), IM_COL32(130, 130, 140, 255), "x");

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.073f, 0.073f, 0.085f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));

			ImGui::SetCursorScreenPos(win_min + ImVec2(26.0f, 84.0f));
			if (ImGui::BeginChild("##product_panel", ImVec2(308.0f, 314.0f), true, ImGuiWindowFlags_NoScrollbar))
			{
				draw_panel_header("Product");

				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.095f, 0.095f, 0.112f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.15f, 0.19f, 1.0f));
				ImGui::BeginChild("##product_card", ImVec2(0.0f, 78.0f), true, ImGuiWindowFlags_NoScrollbar);
				const ImVec2 product_min = ImGui::GetWindowPos();
				const ImVec2 product_max = product_min + ImGui::GetWindowSize();
				branding::draw_logo_fit(ctx.device, ImGui::GetWindowDrawList(), branding::brand_asset_t::wordmark, product_min + ImVec2(14.0f, 17.0f), product_min + ImVec2(72.0f, 58.0f));
				ImGui::SetCursorPos(ImVec2(84.0f, 15.0f));
				ImGui::TextUnformatted(branding::product_full_name);
				ImGui::SetCursorPosX(84.0f);
				ImGui::TextDisabled("Status:");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.30f, 1.0f, 0.45f, 1.0f), "Ready");
				const ImVec2 pill_min(product_max.x - 55.0f, product_min.y + 26.0f);
				const ImVec2 pill_max(product_max.x - 16.0f, product_min.y + 48.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(pill_min, pill_max, IM_COL32(31, 28, 43, 255), 5.0f);
				ImGui::GetWindowDrawList()->AddText(pill_min + ImVec2(8.0f, 4.0f), IM_COL32(180, 176, 196, 255), branding::channel);
				ImGui::EndChild();

				ImGui::Dummy(ImVec2(0.0f, 10.0f));
				ImGui::BeginChild("##meta_card", ImVec2(0.0f, 100.0f), true, ImGuiWindowFlags_NoScrollbar);
				label_value("Platform", branding::platform_name);
				label_value("Target", branding::target_name);
				label_value("Type", branding::product_type);
				ImGui::EndChild();

				ImGui::Dummy(ImVec2(0.0f, 10.0f));
				ImGui::BeginChild("##license_card", ImVec2(0.0f, 60.0f), true, ImGuiWindowFlags_NoScrollbar);
				label_value("Expires", "Never", true);
				label_value("Last update", "Today");
				ImGui::EndChild();
				ImGui::PopStyleColor(2);
			}
			ImGui::EndChild();

			ImGui::SetCursorScreenPos(win_min + ImVec2(352.0f, 84.0f));
			if (ImGui::BeginChild("##changelog_panel", ImVec2(320.0f, 268.0f), true))
			{
				draw_panel_header("Changelog");
				ImGui::TextColored(ImVec4(0.78f, 0.78f, 0.82f, 1.0f), "v1.0.0");
				ImGui::TextDisabled("  - initial release");
				ImGui::TextDisabled("  - notification system added");
				ImGui::TextDisabled("  - loader flow added");
				ImGui::TextDisabled("  - branding refresh");
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(0.78f, 0.78f, 0.82f, 1.0f), "[Note]");
				ImGui::TextDisabled("  - beta version");
				ImGui::TextDisabled("  - there are likely still many bugs\n    and it is not yet stable.");
			}
			ImGui::EndChild();

			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar(2);

			ImGui::SetCursorScreenPos(win_min + ImVec2(352.0f, 370.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.13f, 0.16f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.17f, 0.17f, 0.21f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.19f, 0.24f, 1.0f));
			if (ImGui::Button("Exit", ImVec2(78.0f, 40.0f)))
			{
				ctx.done = true;
				ctx.accepted = false;
			}
			ImGui::PopStyleColor(3);

			ImGui::SameLine(0.0f, 12.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.15f, 0.46f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.33f, 0.20f, 0.62f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, branding::accent_dark());
			if (ImGui::Button("Launch", ImVec2(242.0f, 40.0f)))
			{
				ctx.accepted = true;
				ctx.done = true;
				launch_roblox_if_needed();
			}
			ImGui::PopStyleColor(3);

			ImGui::End();
		}

		void cleanup(loader_context_t& ctx)
		{
			if (ImGui::GetCurrentContext())
			{
				ImGui_ImplDX11_Shutdown();
				ImGui_ImplWin32_Shutdown();
				ImGui::DestroyContext();
			}

			cleanup_render_target(ctx);
			branding::release_logo_textures();
			if (ctx.swap_chain) ctx.swap_chain->Release();
			if (ctx.device_context) ctx.device_context->Release();
			if (ctx.device) ctx.device->Release();
			if (ctx.window && IsWindow(ctx.window)) DestroyWindow(ctx.window);
			if (ctx.window_class.lpszClassName) UnregisterClassA(ctx.window_class.lpszClassName, ctx.window_class.hInstance);
		}
	}

	bool run()
	{
		loader_context_t ctx{};
		g_context = &ctx;

		if (!create_window(ctx))
			return true;

		if (!create_device(ctx))
		{
			cleanup(ctx);
			return true;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		apply_style();

		if (!ImGui_ImplWin32_Init(ctx.window) || !ImGui_ImplDX11_Init(ctx.device, ctx.device_context))
		{
			cleanup(ctx);
			return true;
		}

		while (!ctx.done)
		{
			MSG msg{};
			while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
				{
					ctx.done = true;
					ctx.accepted = false;
				}
			}

			if (ctx.done)
				break;

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			render_loader(ctx);

			ImGui::Render();
			const float clear_color[4]{ 0.06f, 0.06f, 0.07f, 1.0f };
			ctx.device_context->OMSetRenderTargets(1, &ctx.render_target, nullptr);
			ctx.device_context->ClearRenderTargetView(ctx.render_target, clear_color);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			ctx.swap_chain->Present(1, 0);

			std::this_thread::sleep_for(std::chrono::milliseconds(8));
		}

		cleanup(ctx);
		g_context = nullptr;
		return ctx.accepted;
	}
}
