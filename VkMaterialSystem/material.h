#pragma once
#include "stdafx.h"

struct MaterialAsset;
struct MaterialRenderData;

struct BlockMember
{
	char name[32];
	uint32_t size;
	uint32_t offset;
};

struct OpaqueBlockDefinition
{
	char name[32];
	uint32_t size;
	uint32_t num;
	BlockMember* blockMembers;
};

struct MaterialDefinition
{
	char* vShaderPath;
	char* fShaderPath;
	bool depthTest;
	bool depthWrite;
	OpaqueBlockDefinition pcDefinition;

};

namespace Material
{
	void make(MaterialDefinition def);

	MaterialRenderData getRenderData();
	void destroy();
}