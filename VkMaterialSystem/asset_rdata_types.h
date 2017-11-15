#pragma once
#include "vkh.h"

struct MeshRenderData
{
	vkh::VkhMesh vkMesh;
};

struct UniformBlockDef
{
	//every odd element is a hashed member name, every even element is that member's offset
	uint32_t* layout;
	uint32_t blockSize;
	uint32_t memberCount;
	VkShaderStageFlags visibleStages;
};

struct MaterialRenderData
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	uint32_t layoutCount;
	VkDescriptorSetLayout* descriptorSetLayouts;

	VkDescriptorSet* descSets;
	uint32_t numDescSets;

	UniformBlockDef pushConstantBlockDef;
	char* pushConstantData;

	//we don't need a layout for static data since it cannot be 
	//changed after initialization
	VkBuffer* staticBuffers;
	VkDeviceMemory staticUniformMem;
};

struct TextureRenderData
{
	vkh::VkhTexture vulkanTexture;
	VkImageView view;
	VkFormat format;
	VkSampler sampler;
};

struct VertexRenderData
{
	VkVertexInputAttributeDescription* attrDescriptions;
	uint32_t attrCount;
};