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

enum class PushConstant : uint8_t
{
	MVP = 1,
	Col,
	Tint,
	
	//new items should be added above max

	MAX
};

namespace Material
{
	void make(MaterialDefinition def);

	MaterialRenderData getRenderData();
	void setPushConstantVector(PushConstant var, glm::vec4& data);
	void setPushConstantMatrix(PushConstant var, glm::mat4& data);
	void destroy();
}