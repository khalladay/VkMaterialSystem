#pragma once

namespace vkh
{
	class Allocator
	{
	public:
		virtual void allocate(size_t size) = 0;
		virtual void deallocate() = 0;
		virtual size_t allocated_size() = 0;

	};
}