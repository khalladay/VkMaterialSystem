#pragma once
#include "stdafx.h"
#include <vector>
#include <map>
#include "material.h"

//this is in it's own file so that unless you want to manually 
//create a Material Definition, you don't need to know about any of 
//the enums / structs required to load a material. 
namespace Material
{
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
		uint32_t set;
		uint32_t binding;
		uint32_t sizeBytes;
		char name[32];
		std::vector<ShaderStage> owningStages;
		std::vector<BlockMember> blockMembers;
	};

	struct UniformValue
	{
		uint32_t name;
		char value[64];
	};

	struct InstanceDefinition
	{
		char parentName[256];
		std::vector<UniformValue> defaultValues;
	};

	struct ShaderStageDefinition
	{
		ShaderStage stage;
		char shaderPath[256];
	};

	struct Definition
	{
		PushConstantBlock pcBlock;
		std::vector<ShaderStageDefinition> stages;
		std::map<uint32_t, std::vector<DescriptorSetBinding>> descSets;

		InstanceDefinition rootInstanceDefinition;

		std::vector<uint32_t> dynamicSets;
		std::vector<uint32_t> staticSets;
		std::vector<uint32_t> globalSets;

		uint32_t numStaticUniforms;
		uint32_t numStaticTextures;
		uint32_t numDynamicUniforms;
		uint32_t numDynamicTextures;
		uint32_t staticSetsSize;
		uint32_t dynamicSetsSize;
	};

	//if you're manually specifying a material definition instead of loading it, 
	//you need to manually request a key from Material Storage with reserve()
	//since we don't have a path to hash to use as the (potential) map key
	Definition load(const char* assetPath);
	Material::Instance make(uint32_t matId, Material::Definition def);

	InstanceDefinition loadInstance(const char* assetPath);
	Material::Instance makeInstance(InstanceDefinition def);

	void destroyInstance(Material::Instance instance);

}