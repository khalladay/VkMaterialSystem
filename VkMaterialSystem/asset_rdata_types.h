#pragma once
#include "vkh.h"
#include "stdafx.h"
#include <queue>

#define MATERIAL_LAYOUT_STRIDE 4
#define MATERIAL_IMAGE_FLAG_VALUE 0xFFFFFFFF

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

struct MaterialInstancePage
{
	VkBuffer staticBuffer;
	VkBuffer dynamicBuffer;
	std::vector<VkWriteDescriptorSet> descSetWrites;

	//stores the generation of material instances
	//0 is not a valid generation, empty slots are 0.
	uint8_t generation[MATERIAL_INSTANCE_PAGESIZE];
	std::queue<uint8_t> freeIndices; 

	vkh::Allocation staticMem;
	vkh::Allocation dynamicMem;
};


/* For the dynamicLayout array: (removed comment from struct for easier reading) 
// stride: 4 - hashed name / offset of binding start / member size / member offset
// for images- hasehd name / textureViewPtr index / desc set write idx / imageFlagValue
*/
struct MaterialRenderData
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	uint32_t numDescSetLayouts;
	VkDescriptorSetLayout* descriptorSetLayouts;

	UniformBlockDef pushConstantLayout;
	char* pushConstantData;

	VkDescriptorSet* descSets;
	uint32_t numDescSets;

	uint32_t* dynamicLayout;
	uint32_t numDynamicInputs;

	uint32_t* staticLayout;
	uint32_t numStaticInputs;

	VkDeviceSize staticUniformMemSize;
	VkDeviceSize dynamicUniformMemSize;

	uint32_t dynamicDescSetStride;
	uint32_t staticDescSetStride;

	char* defaultStaticData;
	char* defaultDynamicData;

	std::vector<MaterialInstancePage> instPages;
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