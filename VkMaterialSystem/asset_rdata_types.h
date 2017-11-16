#pragma once
#include "vkh.h"

struct MeshRenderData
{
	VkBuffer vBuffer;
	VkBuffer iBuffer;
	VkDeviceMemory vBufferMemory;
	VkDeviceMemory iBufferMemory;

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

	// stride: 3 - hashed name / index of binding / offet
	uint32_t* layout;
	VkBuffer* buffers;
	VkDeviceMemory uniformMem;

	VkWriteDescriptorSet* descriptorSetWrites;
};

struct MaterialRenderData
{
	//general material data
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	uint32_t layoutCount;
	VkDescriptorSetLayout* descriptorSetLayouts;

	VkDescriptorSet* descSets;
	uint32_t numDescSets;

	//input data
	UniformBlockDef pushConstantLayout;
	char* pushConstantData;

	//we don't need a layout for static data since it cannot be 
	//changed after initialization
	VkBuffer* staticBuffers;
	VkDeviceMemory staticUniformMem;

	//for now, just add buffers here to modify. when this
	//is modified to support material instances, we'll change it 
	//to something more sane. 
	MaterialDynamicData dynamic;
};

struct TextureRenderData
{
	VkImage image;
	VkDeviceMemory deviceMemory;
	VkImageView view;
	VkFormat format;
	VkSampler sampler;
};

struct VertexRenderData
{
	VkVertexInputAttributeDescription* attrDescriptions;
	uint32_t attrCount;
};