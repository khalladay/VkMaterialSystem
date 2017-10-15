#pragma once
#include "vkh.h"

struct MeshRenderData
{
	vkh::VkhMesh vkMesh;
};

struct MaterialRenderData
{
	vkh::VkhMaterial vkMat;
	
	//every odd entry maps to a PushConstant enum, every even entry is the offset into the pc buffer
	uint8_t* pushConstantLayout;
	uint32_t pushConstantSize;

	//should use a stack allocator here
	char* pushConstantData;

};

struct VertexRenderData
{
	VkVertexInputAttributeDescription* attrDescriptions;
	uint32_t attrCount;
};