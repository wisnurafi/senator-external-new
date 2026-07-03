#pragma once

#include <d3d11.h>
#include <imgui/imgui.h>

namespace branding
{
	enum class brand_asset_t
	{
		icon,
		wordmark
	};

	ID3D11ShaderResourceView* logo_texture(ID3D11Device* device);
	ID3D11ShaderResourceView* logo_texture(ID3D11Device* device, brand_asset_t asset);
	void draw_logo(ID3D11Device* device, ImDrawList* draw, ImVec2 pos, float size, float alpha = 1.0f);
	void draw_logo_fit(ID3D11Device* device, ImDrawList* draw, brand_asset_t asset, ImVec2 min, ImVec2 max, float alpha = 1.0f);
	void release_logo_textures();
}
