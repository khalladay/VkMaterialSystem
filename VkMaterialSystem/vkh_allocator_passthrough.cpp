#include "stdafx.h"
#include "vkh_allocator_passthrough.h"
#include "vkh.h"
#include "vkh_initializers.h"

namespace vkh::allocators::passthrough
{
	struct AllocatorState
	{
		size_t* memTypeAllocSizes;
		uint32_t totalAllocs;

		VkhContext* context;
	};

	AllocatorState state;

	//ALLOCATOR INTERFACE / INSTALLATION 
	void activate(VkhContext* context);
	void alloc(Allocation& outHandle, VkDeviceSize size, uint32_t memoryType);
	void free(Allocation& handle);
	size_t allocatedSize(uint32_t memoryType);
	uint32_t numAllocs();

	AllocatorInterface allocImpl = { activate, alloc, free, allocatedSize, numAllocs };

	void activate(VkhContext* context)
	{
		context->allocator = allocImpl;
		state.context = context;

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(context->gpu.device, &memProperties);

		state.memTypeAllocSizes = (size_t*)calloc(1, sizeof(size_t) * memProperties.memoryTypeCount);
	}

	void deactivate(VkhContext* context)
	{
		::free(state.memTypeAllocSizes);
	}

	//IMPLEMENTATION


	void alloc(Allocation& outAlloc, VkDeviceSize size, uint32_t memoryType)
	{
		state.totalAllocs++;
		state.memTypeAllocSizes[memoryType] += size;

		VkMemoryAllocateInfo allocInfo = vkh::memoryAllocateInfo(size, memoryType);
		VkResult res = vkAllocateMemory(state.context->device, &allocInfo, nullptr, &(outAlloc.handle));

		outAlloc.size = size;
		outAlloc.type = memoryType;
		outAlloc.offset = 0;

		checkf(res != VK_ERROR_OUT_OF_DEVICE_MEMORY, "Out of device memory");
		checkf(res != VK_ERROR_TOO_MANY_OBJECTS, "Attempting to create too many allocations")
		checkf(res == VK_SUCCESS, "Error allocating memory in passthrough allocator");
	}

	void free(Allocation& allocation)
	{
		state.totalAllocs--;
		state.memTypeAllocSizes[allocation.type] -= allocation.size;
		vkFreeMemory(state.context->device, (allocation.handle), nullptr);
	}

	size_t allocatedSize(uint32_t memoryType)
	{
		return state.memTypeAllocSizes[memoryType];
	}

	uint32_t numAllocs()
	{
		return state.totalAllocs;
	}
}