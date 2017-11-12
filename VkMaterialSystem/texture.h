#pragma once

struct TextureRenderData;
struct TextureAsset;

namespace Texture
{
	uint32_t make(const char* filepath);
	

	TextureRenderData* getRenderData(uint32_t texId);

	void destroy(uint64_t texId);
}