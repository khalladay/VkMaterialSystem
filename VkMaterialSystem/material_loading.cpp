#include "material_loading.h"
#include "material.h"
#include "file_utils.h"
#include "string_utils.h"
#include <vector>

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
		return InputType::PUSH_CONSTANT;
	}

	ShaderStage stringToShaderStage(std::string str)
	{
		if (str == "vertex") return ShaderStage::VERTEX;
		if (str == "fragment") return ShaderStage::FRAGMENT;

		checkf(0, "Could not parse shader stage from input string when loading material");
		return ShaderStage::NONE;
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

	template <typename T>
	bool getValueFromArray(rapidjson::Value& outValue, const rapidjson::Value& array, const char* targetKey, T& targetValue)
	{
		using namespace rapidjson;
		
		for (SizeType elem = 0; elem < array.Size(); elem++)
		{
			const Value& arrayItem = array[elem];
			if (arrayItem.HasMember(targetKey))
			{
				if (arrayItem[targetKey].Is<T>())
				{
					if (arrayItem[targetKey].Get<T>() == targetValue)
					{
						outValue = arrayItem[targetKey];
						return true;
					}

				}
			}
		}

		return false;
	}


	MaterialDefinition load(const char* assetPath)
	{
		using namespace rapidjson;

		const static std::string generatedShaderPath = "../data/_generated/builtshaders/";

		const char* materialString = loadTextFile(assetPath);
		size_t len = strlen(materialString);

		Document materialDoc;
		materialDoc.Parse(materialString, len);
		checkf(!materialDoc.HasParseError(), "Error parsing material file");

		const Value& shaders = materialDoc["shaders"];

		MaterialDefinition def = {};
		def.stages.reserve(shaders.Size());

		for (SizeType i = 0; i < shaders.Size(); i++)
		{
			const Value& matStage = shaders[i];

			ShaderStageDefinition stageDef = {};
			stageDef.stage = stringToShaderStage(matStage["stage"].GetString());
			
			std::string shaderName = std::string(matStage["shader"].GetString());
			std::string shaderPath = generatedShaderPath + shaderName + shaderExtensionForStage(stageDef.stage);
			
			copyCStrAndNullTerminate(stageDef.shaderPath, shaderPath.c_str());


			//parse shader reflection file

			std::string reflPath = generatedShaderPath + shaderName + shaderReflExtensionForStage(stageDef.stage);
			const char* reflData = loadTextFile(reflPath.c_str());
			size_t reflLen = strlen(reflData);
			def.stages.push_back(stageDef);

			Document reflDoc;
			reflDoc.Parse(reflData, reflLen);

			checkf(!reflDoc.HasParseError(), "Error parsing reflection file");

			if (reflDoc.HasMember("push_constants"))
			{
				const Value& pushConstants = reflDoc["push_constants"];

				def.pcBlock.binding = 0;

				checkf(def.pcBlock.sizeBytes == 0 || def.pcBlock.sizeBytes == pushConstants["size"].GetInt(), "Error loading material: multiple stages use push constant block but expect block of different size");
				
				def.pcBlock.owningStages.push_back(stageDef.stage);				
				def.pcBlock.sizeBytes = pushConstants["size"].GetInt();

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
					for (uint32_t member = 0; member < def.pcBlock.blockMembers.size(); ++member)
					{
						BlockMember& existing = def.pcBlock.blockMembers[member];
						if (existing.name == mem.name)
						{
							memberAlreadyExists = true;
						}
					}

					if (!memberAlreadyExists)
					{
						def.pcBlock.blockMembers.push_back(mem);
					}
				}
			}

			if (reflDoc.HasMember("inputs"))
			{
				const Value& uniforms = reflDoc["inputs"];				

				std::vector<std::string> blocksWithDefaultsPresent;
				const Value& defaultArray = matStage["uniforms"];
				for (SizeType d = 0; d < defaultArray.Size(); ++d)
				{
					const Value& default = defaultArray[d];
					blocksWithDefaultsPresent.push_back(default["name"].GetString());
				}

				for (SizeType uni = 0; uni < uniforms.Size(); uni++)
				{
					const Value& uniform = uniforms[uni];

					ShaderInput blockDef	= {};
					uint32_t set = uniform["set"].GetInt();
					blockDef.binding = uniform["binding"].GetInt();
					blockDef.set = set;
					blockDef.sizeBytes = uniform["size"].GetInt();
					blockDef.type = stringToInputType(uniform["type"].GetString()); 

					//if set already exists, just update this uniform if it already exists
					if (def.inputs.find(set) != def.inputs.end())
					{
						for (auto& uniform : def.inputs[set])
						{
							if (uniform.binding == blockDef.binding)
							{
								checkf(uniform.sizeBytes == blockDef.sizeBytes, "A DescriptorSet binding is shared between stages but each stage expects a different size");
								checkf(uniform.type == blockDef.type, "A DescriptorSet binding is shared between stages but each stage expects a different type");
								uniform.owningStages.push_back(stageDef.stage);
							}
						}
					}
					else
					{
						blockDef.owningStages.push_back(stageDef.stage);

						checkf(uniform["name"].GetStringLength() < 31, "opaque block names must be less than 32 characters");
						copyCStrAndNullTerminate(&blockDef.name[0], uniform["name"].GetString());

						int blockDefaultsIndex = -1;
						for (uint32_t d = 0; d < blocksWithDefaultsPresent.size(); ++d)
						{
							if (!blocksWithDefaultsPresent[d].compare(uniform["name"].GetString()))
							{
								blockDefaultsIndex = d;
							}
						}

						if (blockDefaultsIndex > -1 && blockDef.type == InputType::SAMPLER)
						{
							const Value& defaultItem = matStage["inputs"][blockDefaultsIndex];
							const Value& defaultName = defaultItem["value"];
							copyCStrAndNullTerminate(&blockDef.defaultValue[0], defaultName.GetString());
						}
						else if (blockDef.type == InputType::UNIFORM)
						{
							const Value& elements = uniform["elements"];

							for (SizeType elem = 0; elem < elements.Size(); elem++)
							{
								const Value& element = elements[elem];

								BlockMember mem = {};
								mem.offset = element["offset"].GetInt();
								mem.size = element["size"].GetInt();

								checkf(element["name"].GetStringLength() < 31, "opaque block member names must be less than 32 characters");
								copyCStrAndNullTerminate(&mem.name[0], element["name"].GetString());

								if (blockDefaultsIndex > -1 && blockDef.type == InputType::UNIFORM)
								{
									bool memberHasDefault = false;
									const Value& defaultItem = matStage["inputs"][blockDefaultsIndex];
									const Value& defaultMembers = defaultItem["members"];

									size_t curSize = defaultMembers.Size();
									memset(&mem.defaultValue[0], 0, sizeof(float) * 16);


									const Value& defaultValues = defaultMembers[elem]["value"];
									for (uint32_t dv = 0; dv < defaultValues.Size(); ++dv)
									{
										mem.defaultValue[dv] = defaultValues[dv].GetFloat();
									}
								}


								blockDef.blockMembers.push_back(mem);
							}
						}

						def.inputs[blockDef.set].push_back(blockDef);
					}
				}
			}
		}

		free((void*)materialString);

		return def;
	}
}