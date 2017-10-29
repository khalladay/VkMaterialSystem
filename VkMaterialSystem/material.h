#pragma once
#include "stdafx.h"

#if 0
Still remaining to do: 
	* support for samplers at a lower set number than an opaque block
	* support for multiple uniform blocks at different bindings in same set
	* support for multiple uniform blocks in different snets
	* support for multiple unifomr blocks in same set but different binding
	* support for multiple samplers
	* support for push constant in fragment and vertex shader
	* support for setting images at runtime

	* write a material validation step that ensures
		* push constants are the same in all stages if used
		* all default values are formatted correctly, and if textures, that those textures exist

	* first post about this - just put everything in static uniforms 
	* second post - support for multiple objects, dynamic uniform buffers 

#endif 

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
	float defaultValue[16];
};

struct OpaqueBlockDefinition
{
	char name[32];
	uint32_t size;
	uint32_t binding;
	uint32_t set;

	uint32_t num;
	BlockMember* blockMembers;
};

struct SamplerDefinition
{
	char name[32];
	uint32_t set;
	uint32_t binding;
	char defaultTexName[64];
};


struct ShaderStageDefinition
{
	ShaderStage stage;
	char* shaderPath;
	
	OpaqueBlockDefinition* uniformBlocks;
	uint32_t numUniformBlocks;

	SamplerDefinition* samplers;
	uint32_t numSamplers;
};

struct MaterialDefinition
{
	bool depthTest;
	bool depthWrite;

	OpaqueBlockDefinition pcBlock;

	uint32_t numShaderStages;
	ShaderStageDefinition* stages;
};

namespace Material
{
	void make(MaterialDefinition def);

	MaterialRenderData getRenderData();
	
	void setPushConstantVector(const char*, glm::vec4& data);
	void setPushConstantFloat(const char*, float data);
	void setPushConstantMatrix(const char*, glm::mat4& data);

	void destroy();
}