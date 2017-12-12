#include "stdafx.h"
#include "vkh_allocator_pool.h"
#include "vkh.h"
#include "vkh_initializers.h"
#include <vector>
#include "array.h"

namespace vkh::allocators::pool
{
	struct OffsetSize { uint64_t offset; uint64_t size; };
	struct BlockSpanIndexPair { uint32_t blockIdx; uint32_t spanIdx; };

	struct DeviceMemoryBlock
	{
		Allocation mem;
		std::vector<OffsetSize> layout;
	};

	struct MemoryPool
	{
		std::vector<DeviceMemoryBlock> blocks;
	};

	struct AllocatorState
	{
		VkhContext* context;

		std::vector<size_t> memTypeAllocSizes;
		uint32_t totalAllocs;

		uint32_t pageSize;
		VkDeviceSize memoryBlockMinSize;

		std::vector<MemoryPool> memPools;
	};

	AllocatorState state;

	//ALLOCATOR INTERFACE / INSTALLATION 
	void activate(VkhContext* context);
	void alloc(Allocation& outAlloc, AllocationCreateInfo createInfo);
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
		
		state.memTypeAllocSizes.resize(memProperties.memoryTypeCount); 
		state.memPools.resize(memProperties.memoryTypeCount);
		
		state.pageSize = context->gpu.deviceProps.limits.bufferImageGranularity;
		state.memoryBlockMinSize = state.pageSize * 10;
	}

	uint32_t addBlockToPool(VkDeviceSize size, uint32_t memoryType, bool fitToAlloc)
	{
		VkDeviceSize newPoolSize = size * (fitToAlloc ? 1 : 2);
		newPoolSize = newPoolSize < state.memoryBlockMinSize ? state.memoryBlockMinSize : newPoolSize;
		
		VkMemoryAllocateInfo info = vkh::memoryAllocateInfo(newPoolSize, memoryType);

		DeviceMemoryBlock newBlock = {};
		VkResult res = vkAllocateMemory(state.context->device, &info, nullptr, &newBlock.mem.handle);
	
		checkf(res != VK_ERROR_OUT_OF_DEVICE_MEMORY, "Out of device memory");
		checkf(res != VK_ERROR_TOO_MANY_OBJECTS, "Attempting to create too many allocations")
		checkf(res == VK_SUCCESS, "Error allocating memory in passthrough allocator");

		newBlock.mem.type = memoryType;
		newBlock.mem.size = newPoolSize;

		MemoryPool& pool = state.memPools[memoryType];
		pool.blocks.push_back(newBlock);
		
		pool.blocks[pool.blocks.size() - 1].layout.push_back({ 0, newPoolSize });

		state.totalAllocs++;
		
		return pool.blocks.size() - 1;
	}

	void markChunkOfMemoryBlockUsed(uint32_t memoryType, BlockSpanIndexPair indices, VkDeviceSize size)
	{
		MemoryPool& pool = state.memPools[memoryType];

		pool.blocks[indices.blockIdx].layout[indices.spanIdx].offset += size;
		pool.blocks[indices.blockIdx].layout[indices.spanIdx].size -= size;
	}

	bool findFreeChunkForAllocation(BlockSpanIndexPair& outIndexPair, uint32_t memoryType, VkDeviceSize size, bool needsWholePage)
	{
		MemoryPool& pool = state.memPools[memoryType];
		
		for (uint32_t i = 0; i < pool.blocks.size(); ++i)
		{
			for (uint32_t j = 0; j < pool.blocks[i].layout.size(); ++j)
			{
				bool validOffset = needsWholePage ? pool.blocks[i].layout[j].offset == 0 : true;
				if (pool.blocks[i].layout[j].size >= size && validOffset)
				{
					outIndexPair.blockIdx = i;
					outIndexPair.spanIdx = j;
					return true;
				}
			}
		}
		return false;
	}

	void alloc(Allocation& outAlloc, AllocationCreateInfo createInfo)
	{
		uint32_t memoryType = createInfo.memoryTypeIndex;
		VkDeviceSize size = createInfo.size;

		MemoryPool& pool = state.memPools[memoryType];
		//make sure we always alloc a multiple of pageSize
		VkDeviceSize requestedAllocSize = ((size / state.pageSize) + 1) * state.pageSize;
		state.memTypeAllocSizes[memoryType] += requestedAllocSize;

		BlockSpanIndexPair location;

		bool needsOwnPage = createInfo.usage == AllocationUsage::PersistentMapped;
		bool found = findFreeChunkForAllocation(location, memoryType, requestedAllocSize, needsOwnPage);

		if (found && needsOwnPage)
		{
			OffsetSize& curLoc = pool.blocks[location.blockIdx].layout[location.spanIdx];
			curLoc.size = pool.blocks[location.blockIdx].mem.size;
			
		}

		if (!found)
		{
			location = { addBlockToPool(requestedAllocSize, memoryType, needsOwnPage), 0 };
		}

		outAlloc.handle = pool.blocks[location.blockIdx].mem.handle;
		outAlloc.size = size;
		outAlloc.offset = pool.blocks[location.blockIdx].layout[location.spanIdx].offset;
		outAlloc.type = memoryType;
		outAlloc.id = location.blockIdx;

		markChunkOfMemoryBlockUsed(memoryType, location, requestedAllocSize);
	}

	void free(Allocation& allocation)
	{
		VkDeviceSize requestedAllocSize = ((allocation.size / state.pageSize) + 1) * state.pageSize;

		OffsetSize span = {allocation.offset, requestedAllocSize };

		MemoryPool& pool = state.memPools[allocation.type];
		bool found = false;

		uint32_t numLayoutMems = pool.blocks[allocation.id].layout.size();
		for (uint32_t j = 0; j < numLayoutMems; ++j)
		{
			if (pool.blocks[allocation.id].layout[j].offset == requestedAllocSize +allocation.offset)
			{
				pool.blocks[allocation.id].layout[j].offset = allocation.offset;

				if (numLayoutMems == 1)
				{
					pool.blocks[allocation.id].layout[j].size = pool.blocks[allocation.id].mem.size;
				}
				else pool.blocks[allocation.id].layout[j].size += requestedAllocSize;
				found = true;
				break;
			}
		}

		if (!found)
		{
			state.memPools[allocation.type].blocks[allocation.id].layout.push_back(span);
			state.memTypeAllocSizes[allocation.type] -= requestedAllocSize;
		}
	}

	size_t allocatedSize(uint32_t memoryType)
	{
		return state.memTypeAllocSizes[memoryType];
	}

	uint32_t numAllocs()
	{
		return state.totalAllocs;
	}

	void deactivate(VkhContext* context)
	{
	}

}