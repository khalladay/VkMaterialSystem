#include "material_loading.h"
#include "material.h"
#include "file_utils.h"
#include "string_utils.h"
#include <vector>
#include <algorithm>

#include <rapidjson\document.h>
#include <rapidjson\filereadstream.h>

namespace Material
{
	struct BlockDefaults
	{
		std::string blockName;
		std::vector<std::string> memberNames;

	};

	InputType stringToInputType(std::string str)
	{
		if (str == "UNIFORM") return InputType::UNIFORM;
		if (str == "SAMPLER") return InputType::SAMPLER;
		
		checkf(0, "trying to convert an invalid string to input type");
		return InputType::MAX;
	}

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

		checkf(0, "Unsupported shader stage passed to function");
		return "";
	}

	std::string shaderReflExtensionForStage(ShaderStage stage)
	{
		if (stage == ShaderStage::VERTEX) return ".vert.refl";
		if (stage == ShaderStage::FRAGMENT) return ".frag.refl";

		checkf(0, "Unsupported shader stage passed to function");
		return "";
	}

	void copyCStrAndNullTerminate(char* dst, const char* src)
	{
		uint32_t len = static_cast<uint32_t>(strlen(src)) + 1;
		checkf(len < 256, "Material loading is trying to create a string of length > 256, which is larger than the path array in the shaderstagedefinition struct");
		memcpy(dst, src, len);
		dst[len - 1] = '\0';
	}

	MaterialDefinition load(const char* assetPath)
	{
		using namespace rapidjson;

		MaterialDefinition materialDef = {};

		const static std::string generatedShaderPath = "../data/_generated/builtshaders/";

		const char* materialString = loadTextFile(assetPath);
		size_t len = strlen(materialString);

		Document materialDoc;
		materialDoc.Parse(materialString, len);
		checkf(!materialDoc.HasParseError(), "Error parsing material file");

		const Value& shaders = materialDoc["shaders"];
		materialDef.stages.reserve(shaders.Size());

		for (SizeType i = 0; i < shaders.Size(); i++)
		{
			ShaderStageDefinition stageDef = {};

			const Value& matStage = shaders[i];

			stageDef.stage = stringToShaderStage(matStage["stage"].GetString());
			
			std::string shaderName = std::string(matStage["shader"].GetString());
			std::string shaderPath = generatedShaderPath + shaderName + shaderExtensionForStage(stageDef.stage);
			copyCStrAndNullTerminate(stageDef.shaderPath, shaderPath.c_str());


			//parse shader reflection file
			std::string reflPath = generatedShaderPath + shaderName + shaderReflExtensionForStage(stageDef.stage);
			const char* reflData = loadTextFile(reflPath.c_str());

			size_t reflLen = strlen(reflData);
			materialDef.stages.push_back(stageDef);

			Document reflDoc;
			reflDoc.Parse(reflData, reflLen);

			checkf(!reflDoc.HasParseError(), "Error parsing reflection file");

			if (reflDoc.HasMember("push_constants"))
			{
				const Value& pushConstants = reflDoc["push_constants"];

				checkf(materialDef.pcBlock.sizeBytes == 0 || materialDef.pcBlock.sizeBytes == pushConstants["size"].GetInt(), "Error loading material: multiple stages use push constant block but expect block of different size");
				
				materialDef.pcBlock.owningStages.push_back(stageDef.stage);				
				materialDef.pcBlock.sizeBytes = pushConstants["size"].GetInt();

				const Value& elements = pushConstants["elements"];
				assert(elements.IsArray());

				for (SizeType elem = 0; elem < elements.Size(); elem++)
				{
					const Value& element = elements[elem];
					BlockMember mem;

					checkf(element["name"].GetStringLength() < 31, "opaque block member names must be less than 32 characters");

					copyCStrAndNullTerminate(&mem.name[0], element["name"].GetString());
					mem.offset = element["offset"].GetInt();
					mem.size = element["size"].GetInt();

					bool memberAlreadyExists = false;
					for (uint32_t member = 0; member < materialDef.pcBlock.blockMembers.size(); ++member)
					{
						BlockMember& existing = materialDef.pcBlock.blockMembers[member];
						if (existing.name == mem.name)
						{
							memberAlreadyExists = true;
						}
					}

					if (!memberAlreadyExists)
					{
						materialDef.pcBlock.blockMembers.push_back(mem);
					}
				}
			}

			if (reflDoc.HasMember("inputs"))
			{
				const Value& uniformInputsFromReflData = reflDoc["inputs"];				

				std::vector<std::string> blocksWithDefaultsPresent;
				const Value& defaultArray = matStage["inputs"];
				for (SizeType d = 0; d < defaultArray.Size(); ++d)
				{
					const Value& default = defaultArray[d];
					blocksWithDefaultsPresent.push_back(default["name"].GetString());
				}

				for (SizeType uni = 0; uni < uniformInputsFromReflData.Size(); uni++)
				{
					const Value& currentInputFromReflData = uniformInputsFromReflData[uni];

					DescriptorSetBinding blockDef	= {};
					uint32_t set = currentInputFromReflData["set"].GetInt();
					blockDef.binding = currentInputFromReflData["binding"].GetInt();
					blockDef.set = set;
					blockDef.sizeBytes = currentInputFromReflData["size"].GetInt();
					blockDef.type = stringToInputType(currentInputFromReflData["type"].GetString()); 

					//if set already exists, just update this uniform if it already exists

					bool alreadyExists = false;
					if (materialDef.inputs.find(set) != materialDef.inputs.end())
					{
						for (auto& uniform : materialDef.inputs[set])
						{
							if (uniform.binding == blockDef.binding)
							{
								checkf(uniform.sizeBytes == blockDef.sizeBytes, "A DescriptorSet binding is shared between stages but each stage expects a different size");
								checkf(uniform.type == blockDef.type, "A DescriptorSet binding is shared between stages but each stage expects a different type");
								uniform.owningStages.push_back(stageDef.stage);
								alreadyExists = true;
							}
						}
					}
					
					if (!alreadyExists)
					{
						bool isGlobalData = (set == 0 && blockDef.binding == 0);
						materialDef.inputCount++;

						blockDef.owningStages.push_back(stageDef.stage);

						checkf(currentInputFromReflData["name"].GetStringLength() < 31, "opaque block names must be less than 32 characters");
						copyCStrAndNullTerminate(&blockDef.name[0], currentInputFromReflData["name"].GetString());

						int blockDefaultsIndex = -1;
						for (uint32_t d = 0; d < blocksWithDefaultsPresent.size(); ++d)
						{
							if (!blocksWithDefaultsPresent[d].compare(currentInputFromReflData["name"].GetString()))
							{
								blockDefaultsIndex = d;
							}
						}

						//add all members to block definition
						const Value& reflBlockMembers = currentInputFromReflData["members"];
						for (SizeType m = 0; m < reflBlockMembers.Size(); m++)
						{
							const Value& reflBlockMember = reflBlockMembers[m];

							BlockMember mem = {};
							mem.offset = reflBlockMember["offset"].GetInt();
							mem.size = reflBlockMember["size"].GetInt();

							checkf(reflBlockMember["name"].GetStringLength() < 31, "opaque block member names must be less than 32 characters");
							copyCStrAndNullTerminate(&mem.name[0], reflBlockMember["name"].GetString());
							blockDef.blockMembers.push_back(mem);
						}


						if (blockDefaultsIndex > -1)
						{
							const Value& defaultItem = defaultArray[blockDefaultsIndex];

							if (blockDef.type == InputType::SAMPLER)
							{
								const Value& defaultValue = defaultItem["value"];
								copyCStrAndNullTerminate(&blockDef.defaultValue[0], defaultValue.GetString());
							}
							else
							{
								const Value& defaultBlockMembers = defaultItem["members"];

								for (SizeType m = 0; m < blockDef.blockMembers.size(); m++)
								{
									const Value& reflBlockMember = reflBlockMembers[m];
									uint32_t offset = reflBlockMember["offset"].GetInt();
									uint32_t size = reflBlockMember["size"].GetInt();

									auto iter = std::find_if(blockDef.blockMembers.begin(), blockDef.blockMembers.end(), [size, offset](BlockMember& mem) {
										return mem.offset == offset && mem.size == size;
									});

									if (iter._Ptr)
									{
										BlockMember& mem = *iter;

										memset(&mem.defaultValue[0], 0, sizeof(float) * 16);

										const Value& defaultValues = defaultBlockMembers[m]["value"];

										float* defaultFloats = (float*)mem.defaultValue;

										for (uint32_t dv = 0; dv < defaultValues.Size(); ++dv)
										{

											defaultFloats[dv] = defaultValues[dv].GetFloat();
										}
									}
								}

							}
						}

						materialDef.inputs[blockDef.set].push_back(blockDef);
					}
				}
			}
		}

		free((void*)materialString);

		return materialDef;
	}
}