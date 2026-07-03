#include "branding_assets.h"

#include "branding.h"
#include <render/textures/texture.h>

#include <windows.h>
#include "../../ext/imgui/include/D3DX11tex.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace branding
{
	namespace
	{
		struct cached_logo_t
		{
			ID3D11Device* device{};
			brand_asset_t asset{};
			ID3D11ShaderResourceView* texture{};
			float width{};
			float height{};
		};

		std::vector<cached_logo_t> g_logo_cache;

		std::filesystem::path exe_directory()
		{
			char path[MAX_PATH]{};
			const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
			if (length == 0 || length >= MAX_PATH)
				return {};

			return std::filesystem::path(path).parent_path();
		}

		const char* asset_filename(brand_asset_t asset)
		{
			switch (asset)
			{
			case brand_asset_t::icon:
				return "new_icon.png";
			case brand_asset_t::wordmark:
			default:
				return "icon.png";
			}
		}

		std::filesystem::path find_logo_path(brand_asset_t asset)
		{
			const std::filesystem::path exe_dir = exe_directory();
			const char* filename = asset_filename(asset);
			const std::filesystem::path candidates[] = {
				std::filesystem::path("assets") / filename,
				exe_dir / "assets" / filename,
				exe_dir / ".." / "SenatorExternalV2" / "assets" / filename,
				exe_dir / ".." / "assets" / filename,
			};

			for (const auto& candidate : candidates)
			{
				try
				{
					if (!candidate.empty() && std::filesystem::exists(candidate))
						return candidate;
				}
				catch (...) {}
			}

			return {};
		}

		cached_logo_t* find_cached(ID3D11Device* device, brand_asset_t asset)
		{
			for (cached_logo_t& cached : g_logo_cache)
			{
				if (cached.device == device && cached.asset == asset)
					return &cached;
			}

			return nullptr;
		}

		bool read_file(const std::filesystem::path& path, std::vector<unsigned char>& bytes)
		{
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file.is_open())
				return false;

			const std::streamsize size = file.tellg();
			if (size <= 0)
				return false;

			file.seekg(0, std::ios::beg);
			bytes.resize(static_cast<std::size_t>(size));
			return file.read(reinterpret_cast<char*>(bytes.data()), size).good();
		}
	}

	ID3D11ShaderResourceView* logo_texture(ID3D11Device* device)
	{
		return logo_texture(device, brand_asset_t::wordmark);
	}

	ID3D11ShaderResourceView* logo_texture(ID3D11Device* device, brand_asset_t asset)
	{
		if (device == nullptr)
			return nullptr;

		if (cached_logo_t* cached = find_cached(device, asset))
			return cached->texture;

		std::vector<unsigned char> bytes;
		const std::filesystem::path path = find_logo_path(asset);
		if (path.empty() || !read_file(path, bytes))
			return nullptr;

		D3DX11_IMAGE_INFO image_info{};
		D3DX11GetImageInfoFromMemory(bytes.data(), bytes.size(), nullptr, &image_info, nullptr);

		ID3D11ShaderResourceView* texture = D3D11CreateTextureFromBytes(device, bytes.data(), bytes.size());
		if (texture == nullptr)
			return nullptr;

		g_logo_cache.push_back({
			device,
			asset,
			texture,
			static_cast<float>(image_info.Width),
			static_cast<float>(image_info.Height)
		});
		return texture;
	}

	void draw_logo(ID3D11Device* device, ImDrawList* draw, ImVec2 pos, float size, float alpha)
	{
		draw_logo_fit(device, draw, brand_asset_t::wordmark, pos, ImVec2(pos.x + size, pos.y + size), alpha);
	}

	void draw_logo_fit(ID3D11Device* device, ImDrawList* draw, brand_asset_t asset, ImVec2 min, ImVec2 max, float alpha)
	{
		ID3D11ShaderResourceView* texture = logo_texture(device, asset);
		cached_logo_t* cached = find_cached(device, asset);
		const float box_width = std::max(1.0f, max.x - min.x);
		const float box_height = std::max(1.0f, max.y - min.y);

		if (texture == nullptr || cached == nullptr || cached->width <= 0.0f || cached->height <= 0.0f)
		{
			const float fallback_size = std::min(box_width, box_height);
			const ImVec2 fallback_pos(
				min.x + (box_width - fallback_size) * 0.5f,
				min.y + (box_height - fallback_size) * 0.5f);
			draw_mark(draw, fallback_pos, fallback_size, alpha);
			return;
		}

		const float texture_aspect = cached->width / cached->height;
		float draw_width = box_width;
		float draw_height = draw_width / texture_aspect;
		if (draw_height > box_height)
		{
			draw_height = box_height;
			draw_width = draw_height * texture_aspect;
		}

		const ImVec2 image_min(
			min.x + (box_width - draw_width) * 0.5f,
			min.y + (box_height - draw_height) * 0.5f);
		const ImVec2 image_max(image_min.x + draw_width, image_min.y + draw_height);
		const int alpha_byte = static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
		const ImU32 tint = IM_COL32(255, 255, 255, alpha_byte);
		draw->AddImage(reinterpret_cast<ImTextureID>(texture), image_min, image_max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
	}

	void release_logo_textures()
	{
		for (cached_logo_t& cached : g_logo_cache)
		{
			if (cached.texture != nullptr)
				cached.texture->Release();
		}
		g_logo_cache.clear();
	}
}
