#include "material_loading.h"
#include "material.h"
#include "file_utils.h"
#include "string_utils.h"
#include <vector>

namespace Material
{
	MaterialDefinition load(const char* assetPath)
	{
		MaterialDefinition def;
		
		const char* buffer = loadTextFile(assetPath);
		
		const std::string generatedShaderPath = "../data/_generated/builtshaders/";

		std::vector<std::string> parsedMaterial = split(buffer, ':', true);

		std::string vShaderPath = generatedShaderPath +parsedMaterial[1] + ".vert.spv";
		std::string vShaderRefPath = generatedShaderPath + parsedMaterial[1] + ".vert.refl";

		std::string fShaderPath = generatedShaderPath +parsedMaterial[3] + ".frag.spv";
		std::string fShaderReflPath = generatedShaderPath + parsedMaterial[3] + ".frag.refl";


		const char* vReflData = loadTextFile(vShaderRefPath.c_str());

		std::vector<std::string> lines = split(vReflData, '\n', true);
		std::vector<BlockMember> blockMembers;


		OpaqueBlockDefinition* currentBlock = nullptr;

		for (uint32_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx)
		{
			std::string line = lines[lineIdx];
			if (line[0] == '\t')
			{
				assert(currentBlock);

				BlockMember mem;
				line = line.substr(1, line.length() - 1);
				std::vector<std::string> blockInfo = split(line, ':', false);


				uint32_t nameLen = strlen(blockInfo[0].c_str()) + 1;
				memcpy(mem.name, blockInfo[0].c_str(), nameLen);
				
				mem.name[nameLen - 1] = '\0';
				mem.size = atoi(blockInfo[1].c_str());
				mem.offset = atoi(blockInfo[2].c_str());
				blockMembers.push_back(mem);

			}
			else if (currentBlock != nullptr)
			{
				currentBlock->blockMembers = (BlockMember*)malloc(sizeof(BlockMember) * blockMembers.size());
				memcpy(currentBlock->blockMembers, blockMembers.data(), sizeof(BlockMember) * blockMembers.size());
				currentBlock->num = blockMembers.size();
				currentBlock = nullptr;
			}
			else
			{
				std::vector<std::string> blockDecl = split(line, ' ', false);
				
				if (blockDecl[0] == "push_constants")
				{

					uint32_t size = atoi(blockDecl[1].c_str());

					uint32_t nameLen = strlen(blockDecl[0].c_str()) + 1;
					
					memcpy(def.pcDefinition.name, blockDecl[0].c_str(), nameLen);
					def.pcDefinition.name[nameLen - 1] = '\0';

					//free(cName);
					def.pcDefinition.size = size;
					currentBlock = &def.pcDefinition;

				}

			}

		}



		def.vShaderPath = _strdup(vShaderPath.c_str());
		def.fShaderPath = _strdup(fShaderPath.c_str());

		free((void*)buffer);
		return def;
	}
}