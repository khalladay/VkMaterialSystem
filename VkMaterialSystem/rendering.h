#pragma once
#include "material.h"

namespace Rendering
{
	struct DrawCall
	{
		uint32_t meshIdx;
		Material::Instance mat;
	};

	void init();
	void draw(DrawCall* drawCalls, uint32_t count);
}