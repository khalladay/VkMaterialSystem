#pragma once
#include "vkh.h"
#include <queue>

#define MATERIAL_INSTANCE_PAGESIZE 32
#define MATERIAL_UNIFORM_LAYOUT_STRIDE 4

#define MATERIAL_IMAGE_UNIFORM_FLAG 0xFFFFFFFF

#define HASHED_NAME_IDX				0
#define BUFFER_START_OFFSET_IDX		1
#define TEXTUREVIEWPTR_INDEX_IDX	1
#define MEMBER_SIZE_IDX				2
#define DESCSET_WRITE_IDX			2
#define MEMBER_OFFSET_IDX			3
#define IMAGE_FLAG_IDX				3

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

//	stride: 4 - hashed name / buffer index / member size / member offset
//	for images- hasehd name / textureViewPtr index / desc set write idx / image flag value 

	VkBuffer* buffers;
	vkh::Allocation uniformMem;

	VkWriteDescriptorSet* descriptorSetWrites;
};

struct MaterialInstancePage
{
	VkBuffer staticBuffer;
	VkBuffer dynamicBuffer;
	
	VkDescriptorSet* descSets;

	//stores the generation of material instances
	//0 is not a valid generation, empty slots are 0.
	uint8_t generation[MATERIAL_INSTANCE_PAGESIZE];
	std::queue<uint8_t> freeIndices;

	// each texture is 2 uints - one for tex Id and one for sampler id
	//if a material uses 2 textures, stride is 4. count is 4 * pagesize;
	uint32_t* textures;

	vkh::Allocation staticMem;
	vkh::Allocation dynamicMem;

	uint32_t numPageDescSets;
	uint32_t numPageDynamicBuffers;
};

struct MaterialRenderData
{
	//general material data
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	uint32_t numDescSets;

	uint32_t numDescSetLayouts;
	VkDescriptorSetLayout* descriptorSetLayouts;

	UniformBlockDef pushConstantLayout;
	char* pushConstantData;

	std::vector<MaterialInstancePage> instPages;
	
	VkWriteDescriptorSet* defaultDescWrites;
	VkDescriptorBufferInfo* defaultBufferInfos;
	VkDescriptorImageInfo* defaultImageInfos;

	uint32_t numDefaultStaticWrites;
	uint32_t numDefaultDynamicWrites;
	uint32_t numDefaultBufferInfos;
	uint32_t numDefaultImageInfos;

	char* defaultStaticData;
	char* defaultDynamicData;

	uint32_t numStaticUniforms;
	uint32_t numDynamicUniforms;
	uint32_t numStaticTextures;
	uint32_t numDynamicTextures;

	uint32_t staticUniformMemSize;
	uint32_t dynamicUniformMemSize;

	uint32_t* staticUniformLayout;
	uint32_t* dynamicUniformLayout;

	uint8_t usesGlobalData : 1;
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