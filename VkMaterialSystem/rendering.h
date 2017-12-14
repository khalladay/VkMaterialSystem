#pragma once


namespace Rendering
{
	struct DrawCall
	{
		uint32_t meshIdx;
		uint32_t matIdx;
	};

	void init();
	void draw(DrawCall* drawCalls, uint32_t count);
}