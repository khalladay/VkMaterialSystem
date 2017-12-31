#pragma once
#include "vkh.h"

#define MATERIAL_IMAGE_UNIFORM_FLAG 0xFFFFFFFF

struct MeshRenderData
{
	VkBuffer vBuffer;
	VkBuffer iBuffer;
	vkh::Allocation vBufferMemory;
	vkh::Allocation iBufferMemory;

	uint32_t vCount;
	uint32_t iCount;
};

struct UniformBlockDef
{
	//every odd element is a hashed member name, every even element is that member's offset
	uint32_t* layout;
	uint32_t blockSize;
	uint32_t memberCount;
	VkShaderStageFlags visibleStages;
};

struct MaterialDynamicData
{
	uint32_t numInputs;

	// stride: 4 - hashed name / buffer index / member size / member offset
	// for images- hasehd name / textureViewPtr index / desc set write idx / padding 
	uint32_t* layout;
	VkBuffer* buffers;
	vkh::Allocation uniformMem;

	VkWriteDescriptorSet* descriptorSetWrites;
};

struct MaterialRenderData
{
	//general material data
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	//needs information about stride in various arrays for instances
	// ie/ material instance ID = 2, where does that start in the descSets array? the static buffers array? 
	VkDescriptorSet* descSets;
	uint32_t numDescSets;

	UniformBlockDef pushConstantLayout;
	char* pushConstantData;

	//we don't need a layout for static data since it cannot be 
	//changed after initialization
	VkBuffer* staticBuffers;
	vkh::Allocation staticUniformMem;
	uint32_t numStaticBuffers;

	//for now, just add buffers here to modify. when this
	//is modified to support material instances, we'll change it 
	//to something more sane. 
	MaterialDynamicData dynamic;
};

struct TextureRenderData
{
	VkImage image;
	vkh::Allocation deviceMemory;
	VkImageView view;
	VkFormat format;
	VkSampler sampler;
};

struct VertexRenderData
{
	VkVertexInputAttributeDescription* attrDescriptions;
	uint32_t attrCount;
};