#include "material_loading.h"
#include "material.h"
#include "file_utils.h"
#include "string_utils.h"
#include <vector>

#include <rapidjson\document.h>
#include <rapidjson\filereadstream.h>

namespace Material
{
	ShaderStage stringToShaderStage(std::string str)
	{
		if (str == "vertex") return ShaderStage::VERTEX;
		if (str == "fragment") return ShaderStage::FRAGMENT;

		checkf(0, "Could not parse shader stage from input string when loading material");
		return ShaderStage::MAX;
	}

	std::string shaderExtensionForStage(ShaderStage stage)
	{
		if (stage == ShaderStage::VERTEX) return ".vert.spv";
		if (stage == ShaderStage::FRAGMENT) return ".frag.spv";
	}

	std::string shaderReflExtensionForStage(ShaderStage stage)
	{
		if (stage == ShaderStage::VERTEX) return ".vert.refl";
		if (stage == ShaderStage::FRAGMENT) return ".frag.refl";
	}

	void copyCStrAndNullTerminate(char* dst, const char* src)
	{
		uint32_t len = strlen(src) + 1;
		memcpy(dst, src, len);
		dst[len - 1] = '\0';
	}


	MaterialDefinition load(const char* assetPath)
	{
		using namespace rapidjson;

		const static std::string generatedShaderPath = "../data/_generated/builtshaders/";

		MaterialDefinition def = {};
		std::vector<ShaderStageDefinition> shaderStages;

		const char* materialString = loadTextFile(assetPath); //(char *)calloc(1, size + 1); // Enough memory for file + \0

		size_t len = strlen(materialString);
		Document materialDoc;
		materialDoc.Parse(materialString, len);

		checkf(!materialDoc.HasParseError(), "Error parsing material file");

		const Value& shaders = materialDoc["shaders"];
		for (SizeType i = 0; i < shaders.Size(); i++)
		{
			const Value& stage = shaders[i];
			ShaderStageDefinition stageDef = {};
			stageDef.stage = stringToShaderStage(stage["stage"].GetString());

			std::string shaderName = std::string(stage["shader"].GetString());

			std::string shaderPath = generatedShaderPath + shaderName + shaderExtensionForStage(stageDef.stage);
			
			stageDef.shaderPath = (char*)malloc(shaderPath.length() + 1);
			copyCStrAndNullTerminate(stageDef.shaderPath, shaderPath.c_str());


			//parse shader reflection file

			std::string reflPath = generatedShaderPath + shaderName + shaderReflExtensionForStage(stageDef.stage);
			const char* reflData = loadTextFile(reflPath.c_str());
			size_t reflLen = strlen(reflData);

			Document reflDoc;
			reflDoc.Parse(reflData, reflLen);
			checkf(!reflDoc.HasParseError(), "Error parsing reflection file");

			if (reflDoc.HasMember("push_constants"))
			{
				const Value& pushConstants = reflDoc["push_constants"];

				def.pcBlock.binding = 0;
				def.pcBlock.set = 0;
				def.pcBlock.size = pushConstants["size"].GetInt();

				const Value& elements = pushConstants["elements"];
				assert(elements.IsArray());

				std::vector<BlockMember> members;
				for (SizeType elem = 0; elem < elements.Size(); elem++)
				{
					const Value& element = elements[elem];
					BlockMember mem;

					memcpy(mem.name, element["name"].GetString(), element["name"].GetStringLength());
					mem.offset = element["offset"].GetInt();
					mem.size = element["size"].GetInt();
					members.push_back(mem);
				}

				def.pcBlock.num = members.size();

				def.pcBlock.blockMembers = (BlockMember*)malloc(sizeof(BlockMember) * def.pcBlock.num);
				memcpy(def.pcBlock.blockMembers, members.data(), sizeof(BlockMember) * def.pcBlock.num);
				
			}

			if (reflDoc.HasMember("uniforms"))
			{
				const Value& uniforms = reflDoc["uniforms"];

				std::vector<OpaqueBlockDefinition> blockDefs;

				for (SizeType uni = 0; uni < uniforms.Size(); uni++)
				{
					const Value& uniform = uniforms[uni];

					OpaqueBlockDefinition blockDef = {};
					blockDef.binding = uniform["binding"].GetInt();
					blockDef.set = uniform["set"].GetInt();
					blockDef.size = uniform["size"].GetInt();
					memcpy(blockDef.name, uniform["name"].GetString(), uniform["name"].GetStringLength());

					const Value& elements = uniform["elements"];

					std::vector<BlockMember> members;
					for (SizeType elem = 0; elem < elements.Size(); elem++)
					{
						const Value& element = elements[elem];
						BlockMember mem;
						mem.offset = element["offset"].GetInt();
						mem.size = element["size"].GetInt();
						members.push_back(mem);
					}

					blockDef.num = members.size();

					blockDef.blockMembers = (BlockMember*)malloc(sizeof(BlockMember) * blockDef.num);
					memcpy(blockDef.blockMembers, members.data(), sizeof(BlockMember) * blockDef.num);
					blockDefs.push_back(blockDef);

				}

				stageDef.numUniformBlocks = blockDefs.size();
				stageDef.uniformBlocks = (OpaqueBlockDefinition*)malloc(sizeof(OpaqueBlockDefinition) * blockDefs.size());
				memcpy(stageDef.uniformBlocks, blockDefs.data(), sizeof(OpaqueBlockDefinition) * blockDefs.size());
			}

			//parse shader uniform defaults 


			shaderStages.push_back(stageDef);
		}



		
		def.numShaderStages = shaderStages.size();
		def.stages = (ShaderStageDefinition*)malloc(sizeof(ShaderStageDefinition) * def.numShaderStages);
		memcpy(def.stages, shaderStages.data(), sizeof(ShaderStageDefinition) * def.numShaderStages);
		free((void*)materialString);
		
		return def;
	}
}