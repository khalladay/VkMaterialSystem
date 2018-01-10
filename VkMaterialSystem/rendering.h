#pragma once

#include "material.h"

struct DrawCall
{
	uint32_t meshIdx;
	MaterialInstance mat;
};

namespace Rendering
{
	void init();
	void draw(DrawCall* drawCalls, uint32_t count);
}