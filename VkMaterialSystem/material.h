#pragma once
#include "stdafx.h"

#if 0
Still remaining to do: 
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

enum class InputType : uint8_t
{
	UNIFORM,
	SAMPLER,
	PUSH_CONSTANT
};

struct BlockMember
{
	char name[32];
	uint32_t size;
	uint32_t offset;
	float defaultValue[16];
};

struct ShaderInput
{
	ShaderStage owningStage;
	InputType type;
	uint8_t set;
	uint8_t binding;
	uint8_t numBlockMembers;
	uint32_t sizeBytes;

	//null for sampler
	BlockMember* blockMembers;

	char name[32];
	char defaultValue[64];
};

struct ShaderStageDefinition
{
	ShaderStage stage;
	char shaderPath[256];
	uint8_t numInputs;
};

struct MaterialDefinition
{
	bool depthTest;
	bool depthWrite;

	ShaderInput pcBlock;

	uint32_t numShaderStages;
	ShaderStageDefinition* stages;

	//in order based on set
	ShaderInput* inputs;
	uint32_t numInputs;

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