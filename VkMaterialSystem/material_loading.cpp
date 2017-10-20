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
		assert(!materialDoc.HasParseError());
		assert(materialDoc["shaders"].IsArray());

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

			Document reflDoc;
			reflDoc.Parse(reflData, strlen(reflData));

			assert(!reflDoc.HasParseError());

			if (reflDoc.HasMember("push_constants"))
			{
				const Value& pushConstants = reflDoc["push_constants"];

				OpaqueBlockDefinition block = {};

				block.binding = 0;
				block.set = 0;
				block.size = pushConstants["size"].GetInt();

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

				block.num = members.size();

				block.blockMembers = (BlockMember*)malloc(sizeof(BlockMember) * block.num);
				memcpy(block.blockMembers, members.data(), sizeof(BlockMember) * block.num);

			}

			if (reflDoc.HasMember("uniforms"))
			{
				const Value& uniforms = reflDoc["uniforms"];

				std::vector<OpaqueBlockDefinition> blockDefs;

				for (SizeType uni = 0; uni < uniforms.Size(); uni++)
				{
					const Value& uniform = uniforms[uni];
					OpaqueBlockDefinition blockDef = {};

				}
			}

			//parse shader uniform defaults 


			shaderStages.push_back(stageDef);
		}



		//std::vector<std::string> parsedMaterial = split(buffer, ':', true);

		//std::vector<ShaderStageDefinition> shaderStages;

		////each line of the material file is a different shader stage, increment by two to get next line
		//for (uint32_t i = 0; i < parsedMaterial.size(); i += 2)
		//{
		//	if (parsedMaterial[i].length() < 1) continue;
		//	ShaderStageDefinition stageDef = {};
		//	stageDef.stage = stringToShaderStage(parsedMaterial[i]);

		//	std::string shaderPath = generatedShaderPath + parsedMaterial[i+1] + shaderExtensionForStage(stageDef.stage);

		//	stageDef.shaderPath = (char*)malloc(shaderPath.length() + 1);
		//	
		//	copyCStrAndNullTerminate(stageDef.shaderPath, shaderPath.c_str());
		//	
		//	std::string reflPath = generatedShaderPath + parsedMaterial[i+1] + shaderReflExtensionForStage(stageDef.stage);
		//	const char* reflData = loadTextFile(reflPath.c_str());

		//	//we need to get data on every opaque block visible to the shader stage.
		//	//each line in the refl file is either a block declaration or member info
		//	std::vector<std::string> lines = split(reflData, '\n', true);

		//	//we can only have one active block at a time
		//	//so we just need a pointer to it, and a vector to hold members until we know
		//	//how many there are
		//	OpaqueBlockDefinition* currentBlock = nullptr;
		//	std::vector<BlockMember> blockMembers;

		//	//we need to track all the block definitions though, to write out at the end
		//	std::vector<OpaqueBlockDefinition> blockDefinitions;

		//	for (uint32_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx)
		//	{
		//		std::string line = lines[lineIdx];
		//		if (line.length() < 1) continue;

		//		//lines that don't start with a tab declare a new block
		//		if (line[0] != '\t')
		//		{

		//			//if we already have an active block, we need to end it, and start setting
		//			//up a new one
		//			if (currentBlock)
		//			{
		//				//which means we know how many members there are, and we can allocate the memory finally 
		//				currentBlock->blockMembers = (BlockMember*)malloc(sizeof(BlockMember) * blockMembers.size());
		//				memcpy(currentBlock->blockMembers, blockMembers.data(), sizeof(BlockMember) * blockMembers.size());
		//				currentBlock->num = blockMembers.size();

		//				currentBlock = nullptr;
		//				blockMembers.clear();
		//			}

		//			std::vector<std::string> blockDecl = split(line, ':', false);
		//			
		//			char* namePtr;

		//			if (blockDecl[0] == "push_constants")
		//			{
		//				//there can only be one push constant layout for all shaders used in a pipeline
		//				//so use the first pushconstant block you find as the layout for that. 

		//				//TODO: flag when important a material with conflicting push constant layouts
		//				currentBlock = &def.pcBlock;

		//			}
		//			else
		//			{
		//				OpaqueBlockDefinition block = {};
		//				blockDefinitions.push_back(block);
		//				currentBlock = &blockDefinitions[blockDefinitions.size() - 1];
		//				currentBlock->binding = atoi(blockDecl[3].c_str());
		//				currentBlock->set = atoi(blockDecl[2].c_str());

		//			}

		//			namePtr = &currentBlock->name[0];
		//			
		//			copyCStrAndNullTerminate(namePtr, blockDecl[0].c_str());
		//			currentBlock->size = atoi(blockDecl[1].c_str());

		//		}
		//		else
		//		{
		//			//if we don't know what block this belongs to, crash because we're fucked. 
		//			assert(currentBlock);

		//			BlockMember mem;
		//			line = line.substr(1, line.length() - 1);
		//			std::vector<std::string> blockInfo = split(line, ':', false);

		//			uint32_t nameLen = strlen(blockInfo[0].c_str()) + 1;
		//			memcpy(mem.name, blockInfo[0].c_str(), nameLen);

		//			//format name:size:offset
		//			mem.name[nameLen - 1] = '\0';
		//			mem.size = atoi(blockInfo[1].c_str());
		//			mem.offset = atoi(blockInfo[2].c_str());

		//			blockMembers.push_back(mem);


		//		}
		//	}
		//	if (currentBlock)
		//	{
		//		//which means we know how many members there are, and we can allocate the memory finally 
		//		currentBlock->blockMembers = (BlockMember*)malloc(sizeof(BlockMember) * blockMembers.size());
		//		memcpy(currentBlock->blockMembers, blockMembers.data(), sizeof(BlockMember) * blockMembers.size());
		//		currentBlock->num = blockMembers.size();

		//		currentBlock = nullptr;
		//		blockMembers.clear();
		//	}


		//	//we now have all the block definitions we need

		//	stageDef.numUniformBlocks = blockDefinitions.size();
		//	stageDef.uniformBlocks = (OpaqueBlockDefinition*)malloc(sizeof(OpaqueBlockDefinition) * stageDef.numUniformBlocks);
		//	memcpy(stageDef.uniformBlocks, blockDefinitions.data(), sizeof(OpaqueBlockDefinition) * stageDef.numUniformBlocks);

		//	shaderStages.push_back(stageDef);
		//	free((void*)reflData);
		//}

		//
		//def.numShaderStages = shaderStages.size();
		//def.stages = (ShaderStageDefinition*)malloc(sizeof(ShaderStageDefinition) * def.numShaderStages);
		//memcpy(def.stages, shaderStages.data(), sizeof(ShaderStageDefinition) * def.numShaderStages);
		//free((void*)buffer);
		return def;
	}
}