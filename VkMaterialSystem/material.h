#pragma once
#include "stdafx.h"

struct MaterialAsset;
struct MaterialRenderData;

enum class ShaderStage : uint8_t
{
	VERTEX,
	FRAGMENT,
	GEOMETRY,
	COMPUTE,
	MAX
};

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
	uint32_t binding;
	uint32_t set;
	BlockMember* blockMembers;
};

struct ShaderStageDefinition
{
	ShaderStage stage;
	char* shaderPath;
	
	OpaqueBlockDefinition* uniformBlocks;
	uint32_t numUniformBlocks;
};

struct MaterialDefinition
{
	bool depthTest;
	bool depthWrite;

	OpaqueBlockDefinition pcBlock;

	uint32_t numShaderStages;
	ShaderStageDefinition* stages;
};

enum class PushConstant : uint8_t
{
	MVP = 1,
	Col,
	Time,
	MAX
};

namespace Material
{
	void make(MaterialDefinition def);

	MaterialRenderData getRenderData();
	
	void setPushConstantVector(PushConstant var, glm::vec4& data);
	void setPushConstantFloat(PushConstant var, float data);
	void setPushConstantMatrix(PushConstant var, glm::mat4& data);

	void destroy();
}