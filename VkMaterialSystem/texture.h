#pragma once
#include "stdafx.h"

struct TextureRenderData;
struct TextureAsset;

namespace Texture
{
	void make(const char* filepath);
	TextureRenderData* getRenderData();
	void destroy();
}