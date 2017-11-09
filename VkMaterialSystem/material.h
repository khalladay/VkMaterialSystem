#pragma once
#include "stdafx.h"
#include <map>
#include <vector>

#if 0
Still remaining to do: 
	* support for multiple samplers
	* support for setting images at runtime
	* support for setting uniforms at runtime. 

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
	MAX
};

enum class InputType : uint8_t
{
	UNIFORM,
	SAMPLER,
	MAX
};

struct BlockMember
{
	char name[32];
	uint32_t size;
	uint32_t offset;
	char defaultValue[64];
};

struct PushConstantBlock
{
	uint32_t sizeBytes;
	std::vector<ShaderStage> owningStages;
	std::vector<BlockMember> blockMembers;
};

struct DescriptorSetBinding
{
	InputType type;
	uint8_t set;
	uint8_t binding;
	uint32_t sizeBytes;
	char name[32];
	char defaultValue[64];

	std::vector<ShaderStage> owningStages;
	std::vector<BlockMember> blockMembers;
};

struct ShaderStageDefinition
{
	ShaderStage stage;
	char shaderPath[256];
};

struct MaterialDefinition
{
	bool depthTest;
	bool depthWrite;

	PushConstantBlock pcBlock;

	std::vector<ShaderStageDefinition> stages;
	std::map<uint32_t, std::vector<DescriptorSetBinding>> inputs;
	uint32_t inputCount;
};

namespace Material
{
	void make(MaterialDefinition def);

	MaterialRenderData getRenderData();
	
	void setPushConstantVector(const char* name, glm::vec4& data);
	void setPushConstantFloat(const char* name, float data);
	void setPushConstantMatrix(const char* name, glm::mat4& data);

	void setUniformVector4(const char* name, glm::vec4& data);
	void setUniformVector2(const char* name, glm::vec2& data);
	void setUniformFloat(const char* name, float data);
	void setUniformMatrix(const char* name, glm::mat4& data);

	void destroy();
}