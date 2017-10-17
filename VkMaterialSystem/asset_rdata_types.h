#pragma once
#include "vkh.h"

struct MeshRenderData
{
	vkh::VkhMesh vkMesh;
};

struct MaterialRenderData
{
	vkh::VkhMaterial vkMat;
	
	//every odd entry is a hashed string name, every even entry is a value
	uint32_t* pushConstantLayout;
	uint32_t pushConstantSize;

	//should use a stack allocator here
	char* pushConstantData;

};

struct VertexRenderData
{
	VkVertexInputAttributeDescription* attrDescriptions;
	uint32_t attrCount;
};