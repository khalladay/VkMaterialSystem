#include "stdafx.h"
#include "vkh_allocator_pool.h"
#include "vkh.h"
#include "vkh_initializers.h"
#include <vector>
#include "array.h"

namespace vkh::allocators::pool
{
	struct FreeSpan
	{
		uint64_t offset;
		uint64_t size;
	};

	struct DeviceMemoryBlock
	{
		Allocation mem;
		std::vector<FreeSpan> layout;
	};

	struct MemoryPool
	{
		std::vector<DeviceMemoryBlock> blocks;
	};

	struct AllocatorState
	{
		size_t* memTypeAllocSizes;
		uint32_t totalAllocs;

	//	MemoryPool* memPools;
		std::vector<MemoryPool> memPools;

		VkhContext* context;
		uint32_t memTypeCount;
		
		uint32_t pageSize;
		VkDeviceSize memoryBlockMinSize;
	};

	AllocatorState state;

	//ALLOCATOR INTERFACE / INSTALLATION 
	void alloc(Allocation& outHandle, VkDeviceSize size, uint32_t memoryType);
	void free(Allocation& handle);
	size_t allocatedSize(uint32_t memoryType);
	uint32_t numAllocs();

	AllocatorInterface allocImpl = { alloc, free, allocatedSize, numAllocs };
	void initializeMemoryPools(uint32_t memoryType);

	void activate(VkhContext* context) 
	{
		context->allocator = allocImpl;
		state.context = context;

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(context->gpu.device, &memProperties);
		
		state.memTypeCount = memProperties.memoryTypeCount;
		state.memTypeAllocSizes = (size_t*)calloc(1, sizeof(size_t) * memProperties.memoryTypeCount);
		state.memPools.resize(memProperties.memoryTypeCount);
		
		state.pageSize = context->gpu.deviceProps.limits.bufferImageGranularity;
		state.memoryBlockMinSize = state.pageSize * 10;

		for (uint32_t i = 0; i < state.memTypeCount; i++)
		{
			initializeMemoryPools(i);
		}
	}

	void initializeMemoryPools(uint32_t memoryType)
	{
		DeviceMemoryBlock firstBlock;
		firstBlock.mem = {};
		state.memPools[memoryType].blocks.push_back(firstBlock);

		DeviceMemoryBlock& rootBlock = state.memPools[memoryType].blocks[0];
		
		VkMemoryAllocateInfo info = vkh::memoryAllocateInfo(state.memoryBlockMinSize, memoryType);

		vkAllocateMemory(vkh::GContext.device, &info, nullptr, &(rootBlock.mem.handle));
		rootBlock.layout.push_back( { 0, state.memoryBlockMinSize });
	}

	void deactivate(VkhContext* context)
	{
		::free(state.memTypeAllocSizes);
	}

	void alloc(Allocation& outAlloc, VkDeviceSize size, uint32_t memoryType)
	{
		MemoryPool& pool = state.memPools[memoryType];
		
		VkDeviceSize requestedAllocSize = ((size / state.pageSize) + 1) * state.pageSize;

		for (uint32_t i = 0; i < pool.blocks.size(); ++i)
		{
			for (uint32_t j = 0; j < pool.blocks[i].layout.size(); ++j)
			{
				if (pool.blocks[i].layout[j].size >= requestedAllocSize)
				{
					outAlloc.handle = pool.blocks[i].mem.handle;
					outAlloc.size = size;
					outAlloc.offset = pool.blocks[i].layout[j].offset;
					outAlloc.type = memoryType;
					outAlloc.id = i;

					pool.blocks[i].layout[j].offset += requestedAllocSize;
					pool.blocks[i].layout[j].size -= requestedAllocSize;

					return;
				}
			}
		}

		VkDeviceSize newPoolSize = requestedAllocSize * 2;
		newPoolSize = newPoolSize < state.memoryBlockMinSize ? state.memoryBlockMinSize : newPoolSize;

		VkMemoryAllocateInfo info = vkh::memoryAllocateInfo(newPoolSize, memoryType);

		DeviceMemoryBlock newBlock = {};
		vkAllocateMemory(state.context->device, &info, nullptr, &newBlock.mem.handle);
		newBlock.mem.type = memoryType;
		newBlock.mem.size = newPoolSize;
	
		pool.blocks.push_back(newBlock);
		pool.blocks[pool.blocks.size() - 1].layout.push_back( { requestedAllocSize, newPoolSize - requestedAllocSize });

		outAlloc.handle = newBlock.mem.handle;
		outAlloc.size = size;
		outAlloc.offset = 0;
		outAlloc.type = memoryType;
		outAlloc.id = pool.blocks.size() - 1;
	}

	void free(Allocation& allocation)
	{

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