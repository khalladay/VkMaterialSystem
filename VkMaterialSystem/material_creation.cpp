#include "stdafx.h"

#include "material_creation.h"
#include "material.h"
#include "file_utils.h"
#include "asset_rdata_types.h"
#include "vkh_initializers.h"
#include "vkh.h"
#include "hash.h"
#include "mesh.h"
#include "texture.h"
#include <vector>
#include <algorithm>
#include <map>

#include <rapidjson\document.h>
#include <rapidjson\filereadstream.h>

namespace Material
{
	const static char* generatedShaderPath = "../data/_generated/builtshaders/";


	InputType stringToInputType(const char* str);
	ShaderStage stringToShaderStage(const char* str);
	const char* shaderExtensionForStage(ShaderStage stage);
	const char* shaderReflExtensionForStage(ShaderStage stage);
	VkShaderStageFlags shaderStageVectorToVkEnum(std::vector<ShaderStage>& vec);
	VkShaderStageFlagBits shaderStageEnumToVkEnum(ShaderStage stage);
	VkDescriptorType inputTypeEnumToVkEnum(InputType type, bool forceDynamicUniforms = false);
	uint32_t allocInstancePage(uint32_t baseMaterial);
	MaterialInstance makeInstance(InstanceDefinition def);

	InputType stringToInputType(const char* str)
	{
		if (!strcmp(str,"UNIFORM")) return InputType::UNIFORM;
		if (!strcmp(str,"SAMPLER")) return InputType::SAMPLER;
		 
		checkf(0, "trying to convert an invalid string to input type");
		return InputType::MAX;
	}

	ShaderStage stringToShaderStage(const char* str)
	{
		if (!strcmp(str,"vertex")) return ShaderStage::VERTEX;
		if (!strcmp(str,"fragment")) return ShaderStage::FRAGMENT;

		checkf(0, "Could not parse shader stage from input string when loading material");
		return ShaderStage::MAX;
	}

	const char* shaderExtensionForStage(ShaderStage stage)
	{
		if (stage == ShaderStage::VERTEX) return ".vert.spv";
		if (stage == ShaderStage::FRAGMENT) return ".frag.spv";

		checkf(0, "Unsupported shader stage passed to function");
		return "";
	}

	const char* shaderReflExtensionForStage(ShaderStage stage)
	{
		if (stage == ShaderStage::VERTEX) return ".vert.refl";
		if (stage == ShaderStage::FRAGMENT) return ".frag.refl";

		checkf(0, "Unsupported shader stage passed to function");
		return "";
	}

	VkShaderStageFlags shaderStageVectorToVkEnum(std::vector<ShaderStage>& vec)
	{
		checkf(vec.size() > 0, "Trying to convert vector of ShaderStages to Vk Enums, but input vector is empty");
		VkShaderStageFlags outBits = shaderStageEnumToVkEnum(vec[0]);

		for (uint32_t i = 1; i < vec.size(); ++i)
		{
			outBits = static_cast<VkShaderStageFlags>(outBits | shaderStageEnumToVkEnum(vec[i]));
		}
		return outBits;
	}

	VkShaderStageFlagBits shaderStageEnumToVkEnum(ShaderStage stage)
	{
		VkShaderStageFlagBits outBits = static_cast<VkShaderStageFlagBits>(0);

		if (stage == ShaderStage::VERTEX) outBits = static_cast<VkShaderStageFlagBits>(outBits | VK_SHADER_STAGE_VERTEX_BIT);
		if (stage == ShaderStage::FRAGMENT) outBits = static_cast<VkShaderStageFlagBits>(outBits | VK_SHADER_STAGE_FRAGMENT_BIT);

		checkf((int)outBits > 0, "Error converting ShaderStage to VK Enum");
		return outBits;
	}

	VkDescriptorType inputTypeEnumToVkEnum(InputType type, bool forceDynamicUniforms)
	{
		if (type == InputType::SAMPLER) return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		if (type == InputType::UNIFORM) return forceDynamicUniforms ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		checkf(0, "trying to use an unsupported descriptor type in a material");
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}

	uint32_t getGPUAlignedSize(uint32_t unalignedSize)
	{
		size_t uboAlignment = vkh::GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;
		return static_cast<uint32_t>((unalignedSize / uboAlignment) * uboAlignment + ((unalignedSize % uboAlignment) > 0 ? uboAlignment : 0));

	}

	bool IsDynamicInput(DescriptorSetBinding binding)
	{
		return binding.set == 3;
	}

	void loadJsonDocument(rapidjson::Document& jsonDoc, const char* docPath)
	{
		const char* assetString = loadTextFile(docPath);
		size_t len = strlen(assetString);

		jsonDoc.Parse(assetString, len);
		checkf(!jsonDoc.HasParseError(), "Error parsing material file");
		free((void*)assetString);
	}

	Definition load(const char* assetPath)
	{
		using namespace rapidjson;

		Material::Definition materialDef = {};
		
		Document materialDoc;
		loadJsonDocument(materialDoc, assetPath);

		/////////////////////////////////////////////////////////////////////////////////
		////Build array of block names with defaults present
		/////////////////////////////////////////////////////////////////////////////////

		std::vector<uint32_t> defaultNamesFromMaterialFile;

		if (materialDoc.HasMember("defaults"))
		{
			const Value& defaults = materialDoc["defaults"];

			for (SizeType i = 0; i < defaults.Size(); ++i)
			{
				const Value& default = defaults[i];
				uint32_t hashedName = hash(default["name"].GetString());

				for (uint32_t def = 0; def < defaultNamesFromMaterialFile.size(); ++def)
				{
					if (defaultNamesFromMaterialFile[def] == hashedName)
					{
						checkf(0, "hash collision with material uniform names");
					}
				}

				defaultNamesFromMaterialFile.push_back(hashedName);
			}
		}


		/////////////////////////////////////////////////////////////////////////////////
		////Build shader stage definitions
		/////////////////////////////////////////////////////////////////////////////////

		const Value& shaders = materialDoc["shaders"];
		materialDef.stages.reserve(shaders.Size());

		for (SizeType i = 0; i < shaders.Size(); i++)
		{
			ShaderStageDefinition stageDef = {};

			const Value& matStage = shaders[i];
			stageDef.stage = stringToShaderStage(matStage["stage"].GetString());

			int printfErr = 0;
			printfErr = snprintf(stageDef.shaderPath, sizeof(stageDef.shaderPath), "%s%s%s", generatedShaderPath, matStage["shader"].GetString(), shaderExtensionForStage(stageDef.stage));
			checkf(printfErr > -1, "");

			materialDef.stages.push_back(stageDef);
		

			/////////////////////////////////////////////////////////////////////////////////
			////Parse Shader Inputs
			/////////////////////////////////////////////////////////////////////////////////

			//now we need to get information about the layout of the shader inputs
			//this isn't necessarily known when writing a material file, so we have to 
			//grab it from a reflection file
			char reflPath[256];
			printfErr = snprintf(reflPath, sizeof(reflPath), "%s%s%s", generatedShaderPath, matStage["shader"].GetString(), shaderReflExtensionForStage(stageDef.stage));
			checkf(printfErr > -1, "");

			const char* reflData = loadTextFile(reflPath);
			size_t reflLen = strlen(reflData);

			Document reflDoc;
			reflDoc.Parse(reflData, reflLen);
			checkf(!reflDoc.HasParseError(), "Error parsing reflection file");
			free((void*)reflData);

			materialDef.numDynamicTextures += reflDoc["num_dynamic_textures"].GetInt();
			materialDef.numDynamicUniforms += reflDoc["num_dynamic_uniforms"].GetInt();
			materialDef.numStaticTextures += reflDoc["num_static_textures"].GetInt();
			materialDef.numStaticUniforms += reflDoc["num_static_uniforms"].GetInt();

			for (SizeType setIdx = 0; setIdx < reflDoc["static_sets"].Size(); setIdx++)
			{
				uint32_t set = reflDoc["static_sets"][setIdx].GetInt();
				if (std::find(materialDef.staticSets.begin(), materialDef.staticSets.end(), set) == materialDef.staticSets.end())
				{
					materialDef.staticSets.push_back(reflDoc["static_sets"][setIdx].GetInt());
				}
			}

			for (SizeType setIdx = 0; setIdx < reflDoc["global_sets"].Size(); setIdx++)
			{
				uint32_t set = reflDoc["global_sets"][setIdx].GetInt();
				if (std::find(materialDef.globalSets.begin(), materialDef.globalSets.end(), set) == materialDef.globalSets.end())
				{
					materialDef.globalSets.push_back(reflDoc["global_sets"][setIdx].GetInt());
				}
			}
			
			for (SizeType setIdx = 0; setIdx < reflDoc["dynamic_sets"].Size(); setIdx++)
			{
				uint32_t set = reflDoc["dynamic_sets"][setIdx].GetInt();

				if (std::find(materialDef.dynamicSets.begin(), materialDef.dynamicSets.end(), set) == materialDef.dynamicSets.end())
				{
					materialDef.dynamicSets.push_back(reflDoc["dynamic_sets"][setIdx].GetInt());
				}
			}

			if (reflDoc.HasMember("push_constants"))
			{
				const Value& pushConstants = reflDoc["push_constants"];

				checkf(materialDef.pcBlock.sizeBytes == 0 || materialDef.pcBlock.sizeBytes == pushConstants["size"].GetInt(), "Error loading material: multiple stages use push constant block but expect block of different size");

				materialDef.pcBlock.owningStages.push_back(stageDef.stage);
				materialDef.pcBlock.sizeBytes = pushConstants["size"].GetInt();

				const Value& elements = pushConstants["elements"];
				checkf(elements.IsArray(), "push constant block in reflection file does not contain a member array");

				for (SizeType elem = 0; elem < elements.Size(); elem++)
				{
					const Value& element = elements[elem];
					checkf(element["name"].GetStringLength() < 31, "opaque block member names must be less than 32 characters");

					BlockMember mem;
					snprintf(mem.name, sizeof(mem.name), "%s", element["name"].GetString());
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

					//if we've already created the push constant block from an earlier stage's shader
					//we might already have info about this block member. 
					if (!memberAlreadyExists)
					{
						materialDef.pcBlock.blockMembers.push_back(mem);
					}
				}
			}

			//reflection files also contain information about any uniform or sampler inputs
			//this array is called the "descriptor_sets" array in the json file

			//Now we need to go through all the inputs we have and build a representation of them
			//for our material definition

			if (reflDoc.HasMember("descriptor_sets"))
			{
				const Value& reflFileDescSets = reflDoc["descriptor_sets"];

				for (SizeType dsIdx = 0; dsIdx < reflFileDescSets.Size(); dsIdx++)
				{
					const Value& curDescBlockFromReflData = reflFileDescSets[dsIdx];

					DescriptorSetBinding descSetBindingDef = {};
					descSetBindingDef.sizeBytes = getGPUAlignedSize(curDescBlockFromReflData["size"].GetInt());
					descSetBindingDef.set = curDescBlockFromReflData["set"].GetInt();
					descSetBindingDef.binding = curDescBlockFromReflData["binding"].GetInt();
					descSetBindingDef.type = stringToInputType(curDescBlockFromReflData["type"].GetString());

					//we might already know about this descriptor set binding if it was also in a previous stage,
					//in that case, all we need to do is add the current shader's stage to the set binding's stages array
					bool alreadyExists = false;
					if (materialDef.descSets.find(descSetBindingDef.set) != materialDef.descSets.end())
					{
						for (auto& descSetItem : materialDef.descSets[descSetBindingDef.set])
						{
							if (descSetItem.binding == descSetBindingDef.binding)
							{
								checkf(descSetItem.sizeBytes == descSetBindingDef.sizeBytes, "A DescriptorSet binding is shared between stages but each stage expects a different size");
								checkf(descSetItem.type == descSetBindingDef.type, "A DescriptorSet binding is shared between stages but each stage expects a different type");
								descSetItem.owningStages.push_back(stageDef.stage);
								alreadyExists = true;
							}
						}
					}

					//if not, we have to create it
					if (!alreadyExists)
					{
						if (std::find(materialDef.staticSets.begin(), materialDef.staticSets.end(), descSetBindingDef.set) != materialDef.staticSets.end())
						{
							materialDef.staticSetsSize += descSetBindingDef.sizeBytes;
						}
						else if (std::find(materialDef.dynamicSets.begin(), materialDef.dynamicSets.end(), descSetBindingDef.set) != materialDef.dynamicSets.end())
						{
							materialDef.dynamicSetsSize += descSetBindingDef.sizeBytes;
						}

						descSetBindingDef.owningStages.push_back(stageDef.stage);

						checkf(curDescBlockFromReflData["name"].GetStringLength() <= 31, "opaque block names must be less than 32 characters");
						snprintf(descSetBindingDef.name, sizeof(descSetBindingDef.name), "%s", curDescBlockFromReflData["name"].GetString());

						//add all members to block definition
						const Value& reflBlockMembers = curDescBlockFromReflData["members"];
						for (SizeType m = 0; m < reflBlockMembers.Size(); m++)
						{
							const Value& reflBlockMember = reflBlockMembers[m];

							BlockMember mem = {};
							mem.offset = reflBlockMember["offset"].GetInt();
							mem.size = reflBlockMember["size"].GetInt();

							checkf(reflBlockMember["name"].GetStringLength() < 31, "opaque block member names must be less than 32 characters");
							snprintf(mem.name, sizeof(mem.name), "%s", reflBlockMember["name"].GetString());
							descSetBindingDef.blockMembers.push_back(mem);
						}

						//if the block member has a default defined in the material
						//we want to grab it. For samplers, this is the path of their texture, 
						//for uniform blocks, this is an array of floats for each member. 
						{
							const Value& arrayOfDefaultValuesFromMaterial = materialDoc["defaults"];

							if (descSetBindingDef.type == InputType::SAMPLER)
							{
								int32_t index = -1;
								for (uint32_t idx = 0; idx < defaultNamesFromMaterialFile.size(); ++idx)
								{
									if (defaultNamesFromMaterialFile[idx] == hash(curDescBlockFromReflData["name"].GetString()))
									{
										index = idx;
									}
								}

								if (index > 0)
								{
									const Value& defaultItem = arrayOfDefaultValuesFromMaterial[index];
									const Value& defaultValue = defaultItem["value"];
									snprintf(descSetBindingDef.defaultValue, sizeof(descSetBindingDef.defaultValue), "%s", defaultValue.GetString());
								}
							}
							//the material file doesn't know where in the descriptor sets a particular value lives, 
							//so we have to loop over all the sets until we find our match. 
							else
							{
								//const Value& defaultBlockMembers = defaultItem["members"];

								//remmeber, we dont' know which members have default values specified yet,
								for (SizeType m = 0; m < descSetBindingDef.blockMembers.size(); m++)
								{
									BlockMember& mem = descSetBindingDef.blockMembers[m];
									memset(&mem.defaultValue[0], 0, sizeof(float) * 16);

									uint32_t defIndex = 0;
									int32_t index = -1;
									for (uint32_t idx = 0; idx < defaultNamesFromMaterialFile.size(); ++idx)
									{
										if (defaultNamesFromMaterialFile[idx] == hash(mem.name))
										{
											index = idx;
										}
									}

									if (index > -1)
									{
										const Value& defaultItem = arrayOfDefaultValuesFromMaterial[index];
										
										const Value& defaultValue = defaultItem["value"];
										float* defaultFloats = (float*)mem.defaultValue;
									
										for (uint32_t dv = 0; dv < defaultValue.Size(); ++dv)
										{
											defaultFloats[dv] = defaultValue[dv].GetFloat();
										}

									
									}
								}
							}
						}

						//finally, add our bindingDef to the material, and continue to the next input in the reflection file 
						materialDef.descSets[descSetBindingDef.set].push_back(descSetBindingDef);
					}
				}
			}
		}

		return materialDef;
	}

	InstanceDefinition loadInstance(const char* instancePath)
	{
		using namespace rapidjson;

		Material::InstanceDefinition instanceDef = {};

		Document instanceDoc;
		loadJsonDocument(instanceDoc, instancePath);

		/////////////////////////////////////////////////////////////////////////////////
		////Build array of block names with defaults present
		/////////////////////////////////////////////////////////////////////////////////

		instanceDef.parent = charArrayToMaterialName(instanceDoc["material"].GetString());

		if (instanceDoc.HasMember("defaults"))
		{
			const Value& defaults = instanceDoc["defaults"];

			for (SizeType i = 0; i < defaults.Size(); ++i)
			{
				const Value& default = defaults[i];

				ShaderInputDefinition inputDef;
				memset(inputDef.value, 0, sizeof(inputDef.value));
				memset(inputDef.name, 0, sizeof(inputDef.name));
				snprintf(inputDef.name, sizeof(inputDef.name), "%s", default["name"].GetString());

				const Value& defaultValue = default["value"];
				
				if (defaultValue.IsArray())
				{
					float* defaultFloats = (float*)inputDef.value;

					for (uint32_t dv = 0; dv < defaultValue.Size(); ++dv)
					{
						float f = defaultValue[dv].GetFloat();
						defaultFloats[dv] = defaultValue[dv].GetFloat();
					}
				}
				else
				{
					snprintf(inputDef.value, defaultValue.GetStringLength(), "%s", defaultValue.GetString());
				}

				instanceDef.defaults.push_back(inputDef);
			}
		}

		return instanceDef;
	}

	uint32_t createBuffersForDescriptorSetBindingArray(std::vector<DescriptorSetBinding*>& input, VkBuffer* dst, VkMemoryPropertyFlags memFlags)
	{
		uint32_t curBuffer = 0;
		VkMemoryRequirements memRequirements;

		for (DescriptorSetBinding* binding : input)
		{
			if (binding->type == InputType::UNIFORM)
			{
				vkh::createBuffer(dst[curBuffer],
					binding->sizeBytes,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					memFlags);

				vkGetBufferMemoryRequirements(vkh::GContext.device, dst[curBuffer], &memRequirements);
				curBuffer++;
			}
		}
		
		//return the number of buffers created
		return curBuffer;
	}

	void allocateDeviceMemoryForBuffers(vkh::Allocation& dst, size_t size, VkBuffer* buffers, VkMemoryPropertyFlags memFlags)
	{
		if (size > 0)
		{
			//all our buffers use the same memory flags and properties, so we can re-use the memory type bits
			//from the first element in the array for all of them. 
			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(vkh::GContext.device, buffers[0], &memRequirements);

			vkh::AllocationCreateInfo createInfo;
			createInfo.size = size;
			createInfo.memoryTypeIndex = vkh::getMemoryType(vkh::GContext.gpu.device, memRequirements.memoryTypeBits, memFlags);
			createInfo.usage = memFlags;
			vkh::allocateDeviceMemory(dst,createInfo );
		}

	}

	void fillBufferWithData(VkBuffer* buffer, uint32_t dataSize, uint32_t dstOffset, char* data)
	{
		VkBuffer stagingBuffer;
		vkh::Allocation stagingMemory;

		vkh::createBuffer(stagingBuffer,
			stagingMemory,
			dataSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* mappedStagingBuffer;
		vkMapMemory(vkh::GContext.device, stagingMemory.handle, stagingMemory.offset, dataSize, 0, &mappedStagingBuffer);

		memset(mappedStagingBuffer, 0, dataSize);
		memcpy(mappedStagingBuffer, data, dataSize);
		vkUnmapMemory(vkh::GContext.device, stagingMemory.handle);

		vkh::VkhCommandBuffer scratch = vkh::beginScratchCommandBuffer(vkh::ECommandPoolType::Transfer);
		vkh::copyBuffer(stagingBuffer, *buffer, dataSize, 0, dstOffset, scratch);
		vkh::submitScratchCommandBuffer(scratch);
		vkh::freeDeviceMemory(stagingMemory);

	}

	void fillBuffersWithDefaultValues(VkBuffer* buffers, uint32_t dataSize, char* defaultData, std::vector<DescriptorSetBinding*> bindings)
	{
		if (dataSize <= 0) return;

		VkBuffer stagingBuffer;
		vkh::Allocation stagingMemory;

		vkh::createBuffer(stagingBuffer,
			stagingMemory,
			dataSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* mappedStagingBuffer;
		vkMapMemory(vkh::GContext.device, stagingMemory.handle, stagingMemory.offset, dataSize, 0, &mappedStagingBuffer);

		memset(mappedStagingBuffer, 0, dataSize);
		memcpy(mappedStagingBuffer, defaultData, dataSize);

		vkUnmapMemory(vkh::GContext.device, stagingMemory.handle);
		vkh::VkhCommandBuffer scratch = vkh::beginScratchCommandBuffer(vkh::ECommandPoolType::Transfer);

		uint32_t curBuffer = 0;
		uint32_t bufferOffset = 0;

		for (DescriptorSetBinding* binding : bindings)
		{
			if (binding->type == InputType::UNIFORM)
			{
				VkBuffer& bindingBuffer = buffers[curBuffer++];
				vkh::copyBuffer(stagingBuffer, bindingBuffer, binding->sizeBytes, bufferOffset, 0, scratch);
				bufferOffset += binding->sizeBytes;
			}
		}

		vkh::submitScratchCommandBuffer(scratch);
		vkh::freeDeviceMemory(stagingMemory);
	}
	
	void collectDefaultValuesIntoBufferAndBuildLayout(char* outBuffer, std::vector<DescriptorSetBinding*> bindings, std::vector<uint32_t>* optionalOutLayout = nullptr)
	{
		uint32_t bufferOffset = 0;
		uint32_t curImage = 0;
		uint32_t total = 0; //eed this number to know the index into the descriptor set write array our image will be
		
		for (DescriptorSetBinding* binding : bindings)
		{
			if (binding->type == InputType::UNIFORM)
			{
				//since each binding has to be created with a gpu aligned size, we can't just sum buffer offsets
				for (uint32_t k = 0; k < binding->blockMembers.size(); ++k)
				{
					if (optionalOutLayout)
					{
						optionalOutLayout->push_back(hash(binding->blockMembers[k].name));
						optionalOutLayout->push_back(bufferOffset);
						optionalOutLayout->push_back(binding->blockMembers[k].size);
						optionalOutLayout->push_back(binding->blockMembers[k].offset);
					}
					memcpy(&outBuffer[0] + bufferOffset + binding->blockMembers[k].offset, binding->blockMembers[k].defaultValue, binding->blockMembers[k].size);
				}
				bufferOffset += binding->sizeBytes;
			}
			else if (optionalOutLayout)
			{
				optionalOutLayout->push_back(hash(binding->name));
				optionalOutLayout->push_back(curImage++);
				optionalOutLayout->push_back(total);
				optionalOutLayout->push_back(MATERIAL_IMAGE_UNIFORM_FLAG);
			}
			total++;

		}
	}

	void bindBuffersToMemory(vkh::Allocation& memoryToBind, VkBuffer* buffers,std::vector<DescriptorSetBinding*> bindings)
	{
		VkDeviceSize bufferOffset = memoryToBind.offset;
		uint32_t curBuffer = 0;
		for (DescriptorSetBinding* binding : bindings)
		{
			if (binding->type == InputType::UNIFORM)
			{
				vkBindBufferMemory(vkh::GContext.device, buffers[curBuffer++], memoryToBind.handle, bufferOffset);
				bufferOffset += binding->sizeBytes;
			}
		}
	}

	MaterialInstance make(uint32_t id, Definition def)
	{
		using vkh::GContext;
		VkResult res;

		MaterialAsset& outAsset = Material::getMaterialAsset(id);

		outAsset.rData = (MaterialRenderData*)calloc(1, sizeof(MaterialRenderData));
		MaterialRenderData& outMaterial = *outAsset.rData;

		/////////////////////////////////////////////////////////////////////////////////
		////Build Arrays of Bindings
		/////////////////////////////////////////////////////////////////////////////////
		
		//for convenience, the first thing we want to do is to built arrays of the static and dynamic bindings
		//saves us having to iterate over the map a bunch later, we still want the map of all of the bindings though, 

		std::vector<DescriptorSetBinding*> staticBindings;
		std::vector<DescriptorSetBinding*> dynamicBindings;

		for (uint32_t idx : def.staticSets)
		{
			for (DescriptorSetBinding& binding : def.descSets[idx])
			{
				staticBindings.push_back(&binding);
			}
		}

		for (uint32_t idx : def.dynamicSets)
		{
			for (DescriptorSetBinding& binding : def.descSets[idx])
			{
				dynamicBindings.push_back(&binding);
			}
		}

		outMaterial.numDynamicUniforms = def.numDynamicUniforms;
		outMaterial.numStaticUniforms = def.numStaticUniforms;
		outMaterial.numStaticTextures = def.numStaticTextures;
		outMaterial.numDynamicTextures = def.numDynamicTextures;

		/////////////////////////////////////////////////////////////////////////////////
		////build shader stages
		/////////////////////////////////////////////////////////////////////////////////

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		
		for (uint32_t i = 0; i < def.stages.size(); ++i)
		{
			ShaderStageDefinition& stageDef = def.stages[i];
			VkPipelineShaderStageCreateInfo shaderStageInfo = vkh::shaderPipelineStageCreateInfo(shaderStageEnumToVkEnum(stageDef.stage));
			vkh::createShaderModule(shaderStageInfo.module, stageDef.shaderPath, GContext.device);
			shaderStages.push_back(shaderStageInfo);
		}
		
		/////////////////////////////////////////////////////////////////////////////////
		////set up descriptorSetLayouts
		/////////////////////////////////////////////////////////////////////////////////

		//Now we build an array of VkDescriptorSetLayouts, 
		//one for each descriptor set in the shaders used by the material.
		//it's important to note that this array has to have no gaps in set number, so if a set
		//isn't used by the shaders, we have to add an empty VkDescriptorSetLayout.
		std::vector<VkDescriptorSetLayout> uniformLayouts;

		//VkDescriptorSetLayouts are created from arrays of VkDescriptorSetLayoutBindings, one for each
		//binding in the set, so first we use the descSets array in our material definition to give us a 
		//map that has an array of VkDescriptorSetLayoutBindings for each descriptor set index (the key of the map) 
		std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> descSetBindingMap;

		//if there is a gap in the descriptor sets our material uses, we need
		//to add an empty one, so we keep going until we've added an entry for each
		//of the bindings we know we have. If we hit an empty set, we don't decrement the
		//remaining inputs var
		uint32_t remainingInputs = static_cast<uint32_t>(def.descSets.size());
		uint32_t curSet = 0;

		while (remainingInputs)
		{
			bool needsEmpty = true;

			for (auto& descSetBindings : def.descSets)
			{
				if (descSetBindings.first == curSet)
				{
					//this is a vector of all the bindings for the curSet
					std::vector<DescriptorSetBinding>& setBindingCollection = descSetBindings.second;
					
					for (auto& binding : setBindingCollection)
					{
						VkDescriptorSetLayoutBinding layoutBinding = vkh::descriptorSetLayoutBinding(inputTypeEnumToVkEnum(binding.type, curSet != 0), shaderStageVectorToVkEnum(binding.owningStages), binding.binding, 1);
						descSetBindingMap[binding.set].push_back(layoutBinding);
					}

					remainingInputs--;
					needsEmpty = false;
				}
			}

			if (needsEmpty)
			{
				VkDescriptorSetLayoutBinding layoutBinding = vkh::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0, 0);
				descSetBindingMap[curSet].push_back(layoutBinding);
			}

			curSet++;
		}

		//now that we have our map of bindings for each set, we know how many VKDescriptorSetLayouts we're going to need:
		uniformLayouts.resize(descSetBindingMap.size());

		//and we can generate the VkDescriptorSetLayouts from the map arrays
		for (auto& bindingCollection : descSetBindingMap)
		{
			uint32_t set = bindingCollection.first;

			std::vector<VkDescriptorSetLayoutBinding>& setBindings = descSetBindingMap[bindingCollection.first];
			VkDescriptorSetLayoutCreateInfo layoutInfo = vkh::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

			res = vkCreateDescriptorSetLayout(GContext.device, &layoutInfo, nullptr, &uniformLayouts[bindingCollection.first]);
		}

		//for sanity in storage, we want to keep MaterialAssets and MaterialRenderDatas POD structs, so we need to convert our lovely
		//containers to arrays. This might change later, if I decide to start using POD arrays. In any case, in a real application you'd
		//almost certainly want these allocations done with any allocator other than malloc
		outMaterial.numDescSetLayouts = static_cast<uint32_t>(uniformLayouts.size());


		outMaterial.descriptorSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * outMaterial.numDescSetLayouts);
		memcpy(outMaterial.descriptorSetLayouts, uniformLayouts.data(), sizeof(VkDescriptorSetLayout) * outMaterial.numDescSetLayouts);

		outMaterial.numDescSets = static_cast<uint32_t>(uniformLayouts.size());

		///////////////////////////////////////////////////////////////////////////////
		//set up pipeline layout
		///////////////////////////////////////////////////////////////////////////////
		
		//we also use the descriptor set layouts to set up our pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkh::pipelineLayoutCreateInfo(uniformLayouts.data(), static_cast<uint32_t>(uniformLayouts.size()));

		//we need to figure out what's up with push constants here becaus the pipeline layout object needs to know
		if (def.pcBlock.sizeBytes > 0)
		{
			//the first part of this block just sets up the relevant fields on the pipelineLayoutInfo from above
			VkPushConstantRange pushConstantRange = {};
			pushConstantRange.offset = 0;
			pushConstantRange.size = def.pcBlock.sizeBytes;
			pushConstantRange.stageFlags = shaderStageVectorToVkEnum(def.pcBlock.owningStages);

			outAsset.rData->pushConstantLayout.visibleStages = pushConstantRange.stageFlags;

			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
			pipelineLayoutInfo.pushConstantRangeCount = 1;

			//now we need to build the layout structure for the push constants, so that we can set these at runtime layer
			outAsset.rData->pushConstantLayout.layout = (uint32_t*)malloc(sizeof(uint32_t) * def.pcBlock.blockMembers.size() * 2);
			outAsset.rData->pushConstantLayout.blockSize = def.pcBlock.sizeBytes;
			outAsset.rData->pushConstantLayout.memberCount = static_cast<uint32_t>(def.pcBlock.blockMembers.size());
			outAsset.rData->pushConstantData = (char*)malloc(def.pcBlock.sizeBytes);

			checkf(def.pcBlock.sizeBytes < 128, "Push constant block is too large in material");

			for (uint32_t i = 0; i < def.pcBlock.blockMembers.size(); ++i)
			{
				BlockMember& mem = def.pcBlock.blockMembers[i];

				outAsset.rData->pushConstantLayout.layout[i * 2] = hash(&mem.name[0]);
				outAsset.rData->pushConstantLayout.layout[i * 2 + 1] = mem.offset;
			}
		}

		res = vkCreatePipelineLayout(GContext.device, &pipelineLayoutInfo, nullptr, &outMaterial.pipelineLayout);
		assert(res == VK_SUCCESS);

		///////////////////////////////////////////////////////////////////////////////
		//set up fixed function graphics pipeline
		///////////////////////////////////////////////////////////////////////////////

		//with the pipeline layout all set up, it's time to actually make the pipeline
		VkVertexInputBindingDescription bindingDescription = vkh::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);

		const VertexRenderData* vertexLayout = Mesh::vertexRenderData();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = vkh::pipelineVertexInputStateCreateInfo();
		vertexInputInfo.vertexBindingDescriptionCount = 1; 		//todo - what would be the reason for multiple binding?
		vertexInputInfo.vertexAttributeDescriptionCount = vertexLayout->attrCount;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = &vertexLayout->attrDescriptions[0];

		//most of this is all boilerplate that will never change over the lifetime of the application
		//but some of it could conceivably be set by the material
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = vkh::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
		VkViewport viewport = vkh::viewport(0, 0, static_cast<float>(GContext.swapChain.extent.width), static_cast<float>(GContext.swapChain.extent.height));
		VkRect2D scissor = vkh::rect2D(0, 0, GContext.swapChain.extent.width, GContext.swapChain.extent.height);
		VkPipelineViewportStateCreateInfo viewportState = vkh::pipelineViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		
		VkPipelineRasterizationStateCreateInfo rasterizer = vkh::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
		VkPipelineMultisampleStateCreateInfo multisampling = vkh::pipelineMultisampleStateCreateInfo();

		VkPipelineColorBlendAttachmentState colorBlendAttachment = vkh::pipelineColorBlendAttachmentState(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlending = vkh::pipelineColorBlendStateCreateInfo(colorBlendAttachment);

		VkPipelineDepthStencilStateCreateInfo depthStencil = vkh::pipelineDepthStencilStateCreateInfo(
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS);

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr; // Optional
		pipelineInfo.layout = outMaterial.pipelineLayout;
		pipelineInfo.renderPass = GContext.mainRenderPass;
		pipelineInfo.pDepthStencilState = &depthStencil;

		pipelineInfo.subpass = 0;

		//can use this to create new pipelines by deriving from old ones
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		//if you get an error about push constant ranges not being defined for offset, you have too many things defined in the push
		//constant in the shader itself
		res = vkCreateGraphicsPipelines(GContext.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outMaterial.pipeline);
		assert(res == VK_SUCCESS);

		///////////////////////////////////////////////////////////////////////////////
		//set up uniform layouts for shader inputs 
		///////////////////////////////////////////////////////////////////////////////

		//the initial "base material defined" values for every input are stored on the material
		//instances can optionally override some or all of these values

		//this is also where we set up the data for the layout of our inputs (on a per binding member basis)
		//for easy setting of individual uniform values at runtime

		outAsset.rData->staticUniformMemSize = def.staticSetsSize;
		outAsset.rData->dynamicUniformMemSize = def.dynamicSetsSize;

		outAsset.rData->defaultStaticData = (char*)malloc(def.staticSetsSize);
		outAsset.rData->defaultDynamicData = (char*)malloc(def.dynamicSetsSize);

		std::vector<uint32_t> staticLayout;
		collectDefaultValuesIntoBufferAndBuildLayout(outAsset.rData->defaultStaticData, staticBindings, &staticLayout);

		std::vector<uint32_t> dynamicLayout;
		collectDefaultValuesIntoBufferAndBuildLayout(outAsset.rData->defaultDynamicData, dynamicBindings, &dynamicLayout);

		//we can convert the layout array to a bare uint32_t array for storage
		outMaterial.staticUniformLayout = (uint32_t*)malloc(sizeof(uint32_t) * staticLayout.size());
		outMaterial.dynamicUniformLayout = (uint32_t*)malloc(sizeof(uint32_t) * dynamicLayout.size());

		memcpy(outMaterial.staticUniformLayout, staticLayout.data(), sizeof(uint32_t) * staticLayout.size());
		memcpy(outMaterial.dynamicUniformLayout, dynamicLayout.data(), sizeof(uint32_t) * dynamicLayout.size());

		///////////////////////////////////////////////////////////////////////////////
		//Write Global Descriptor Set
		///////////////////////////////////////////////////////////////////////////////

		//while each instance page will have it's own set of descriptor sets, the base material will store the global descriptor set
		//todo: move this to a single desc set for the whole application? 

		//global is always set 0, to make this easier
		VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&outMaterial.descriptorSetLayouts[0], 1, GContext.descriptorPool);
		
		res = vkAllocateDescriptorSets(GContext.device, &allocInfo, &outMaterial.globalDescSet);
		checkf(res == VK_SUCCESS, "Error allocating descriptor set");

		//if we're using global data, we pull the data from wherever our global data has been initialized
		outMaterial.usesGlobalData = def.globalSets.size() > 0;

		if (outMaterial.usesGlobalData)
		{
			checkf(def.globalSets.size() == 1, "using more than 1 global buffer isn't supported right now");

			std::vector<DescriptorSetBinding> globalBindings = def.descSets[def.globalSets[0]];

			extern VkBuffer globalBuffer;
			extern uint32_t globalSize;

			VkDescriptorBufferInfo globalBufferInfo = {};
			globalBufferInfo.offset = 0;
			globalBufferInfo.buffer = globalBuffer;
			globalBufferInfo.range = globalSize;

			VkWriteDescriptorSet globalDescriptorWrite = {};
			globalDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			globalDescriptorWrite.dstSet = outMaterial.globalDescSet;
			globalDescriptorWrite.dstBinding = 0; //refers to binding in shader
			globalDescriptorWrite.dstArrayElement = 0;
			globalDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			globalDescriptorWrite.descriptorCount = 1;
			globalDescriptorWrite.pBufferInfo = &globalBufferInfo;
			globalDescriptorWrite.pImageInfo = 0;
			globalDescriptorWrite.pTexelBufferView = nullptr; // Optional

			vkUpdateDescriptorSets(GContext.device, 1, &globalDescriptorWrite, 0, nullptr);
		}

		///////////////////////////////////////////////////////////////////////////////
		//Create Default VkWriteDescriptorSet Objects
		///////////////////////////////////////////////////////////////////////////////

		std::vector<VkWriteDescriptorSet> descSetWrites;

		std::vector<VkDescriptorBufferInfo> uniformBufferInfos;
		uniformBufferInfos.reserve(def.numStaticUniforms + def.numDynamicUniforms);

		std::vector<VkDescriptorImageInfo> imageInfos;
		imageInfos.reserve(def.numDynamicTextures + def.numStaticTextures);


		//each page will need their own, but this simplifies setting things up, since we don't have
		//to keep around the whole material definition once these are built once. 
		//static info is a bit more compliated because it might be images as well

		uint32_t* curLayoutEntry = &outMaterial.staticUniformLayout[0];
		outMaterial.numDefaultStaticWrites = static_cast<uint32_t>(staticBindings.size());

		for (DescriptorSetBinding* bindingPtr : staticBindings)
		{
			DescriptorSetBinding& binding = *bindingPtr;

			VkWriteDescriptorSet descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstBinding = binding.binding;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = inputTypeEnumToVkEnum(binding.type, true);
			descriptorWrite.descriptorCount = binding.set;
			descriptorWrite.pBufferInfo = 0; //all going to point to the same buffer
			descriptorWrite.pImageInfo = 0;
			
			if (binding.type == InputType::UNIFORM)
			{
				VkDescriptorBufferInfo uniformBufferInfo = {};
				uniformBufferInfo.offset = curLayoutEntry[BUFFER_START_OFFSET_IDX];
				uniformBufferInfo.range = binding.sizeBytes;
				uniformBufferInfos.push_back(uniformBufferInfo);
			}
			else if (binding.type == InputType::SAMPLER)
			{
				VkDescriptorImageInfo imageInfo = {};
				uint32_t tex = Texture::make(binding.defaultValue);

				TextureRenderData* texData = Texture::getRenderData(tex);
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = texData->view;
				imageInfo.sampler = texData->sampler;

				imageInfos.push_back(imageInfo);
			//	descriptorWrite.pImageInfo = &imageInfos[imageInfos.size() - 1];
			}

			descriptorWrite.pTexelBufferView = nullptr; // Optional
			descSetWrites.push_back(descriptorWrite);
			curLayoutEntry += MATERIAL_UNIFORM_LAYOUT_STRIDE;

		}

		curLayoutEntry = &outMaterial.dynamicUniformLayout[0];
		for (DescriptorSetBinding* bindingPtr : dynamicBindings)
		{
			DescriptorSetBinding& binding = *bindingPtr;

			VkWriteDescriptorSet descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstBinding = binding.binding; //refers to binding in shader
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = inputTypeEnumToVkEnum(binding.type, true);
			descriptorWrite.descriptorCount = binding.set;
			descriptorWrite.pBufferInfo = 0;
			descriptorWrite.pImageInfo = 0;

			if (binding.type == InputType::UNIFORM)
			{
				VkDescriptorBufferInfo uniformBufferInfo = {};
				uniformBufferInfo.offset = curLayoutEntry[BUFFER_START_OFFSET_IDX];
				uniformBufferInfo.range = binding.sizeBytes;

				uniformBufferInfos.push_back(uniformBufferInfo);
				descriptorWrite.pBufferInfo = &uniformBufferInfos[uniformBufferInfos.size() - 1];

			}
			else if (binding.type == InputType::SAMPLER)
			{
				uint32_t tex = Texture::make(binding.defaultValue);
				TextureRenderData* texData = Texture::getRenderData(tex);

				VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = texData->view;
				imageInfo.sampler = texData->sampler;

				imageInfos.push_back(imageInfo);

			}

			descriptorWrite.pTexelBufferView = nullptr; // Optional

			descSetWrites.push_back(descriptorWrite);
			curLayoutEntry += MATERIAL_UNIFORM_LAYOUT_STRIDE;

		}

		//save off the descriptor set writes for dynamic data for easier updating later
		outMaterial.numDefaultDynamicWrites = def.numDynamicTextures + def.numDynamicUniforms;
		outMaterial.numDefaultStaticWrites = def.numStaticTextures + def.numStaticUniforms;

		uint32_t totalSetWrites = outMaterial.numDefaultDynamicWrites + outMaterial.numDefaultStaticWrites;

		outMaterial.defaultBufferInfos = (VkDescriptorBufferInfo*)malloc(sizeof(VkDescriptorBufferInfo) * uniformBufferInfos.size());
		outMaterial.numDefaultBufferInfos = uniformBufferInfos.size();
		memcpy(outMaterial.defaultBufferInfos, uniformBufferInfos.data(), sizeof(VkDescriptorBufferInfo) * uniformBufferInfos.size());

		outMaterial.defaultImageInfos = (VkDescriptorImageInfo*)malloc(sizeof(VkDescriptorImageInfo) * imageInfos.size());
		outMaterial.numDefaultImageInfos = imageInfos.size();
		memcpy(outMaterial.defaultImageInfos, imageInfos.data(), sizeof(VkDescriptorImageInfo) * imageInfos.size());

		outMaterial.defaultDescWrites = (VkWriteDescriptorSet*)malloc(sizeof(VkWriteDescriptorSet) * totalSetWrites);
		memcpy(outAsset.rData->defaultDescWrites, descSetWrites.data(), sizeof(VkWriteDescriptorSet) * totalSetWrites);

		///////////////////////////////////////////////////////////////////////////////
		//Set up first page of instance memory
		///////////////////////////////////////////////////////////////////////////////
		
		uint32_t instPageIdx = allocInstancePage(id);
		checkf(instPageIdx == 0, "Attempting to allocate root instance page for material, but a page is already present");

		InstanceDefinition rootInstanceDef;
		rootInstanceDef.parent = id;

		MaterialInstance rootInstance = makeInstance(rootInstanceDef);
		return rootInstance;
	}

#if 0
	void make(uint32_t id, Definition def)
	{
		

		///////////////////////////////////////////////////////////////////////////////
		//initialize buffers
		///////////////////////////////////////////////////////////////////////////////	
		{
			//we need to actually write to our descriptor sets, but to do that, we need to get all
			//the uniform buffers allocated / bound first. So we'll do that here. This is also a convenient 
			//place to initialize things to default values from the material
			size_t uboAlignment = GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;

			//global buffers come from elsewhere in the application

			//static buffers never change, so we don't need to keep any information around about them
			outAsset.rData->staticBuffers = (VkBuffer*)malloc(sizeof(VkBuffer) * def.numStaticUniforms);
			outAsset.rData->numStaticUniforms = def.numStaticTextures + def.numStaticUniforms;

			//dynamic buffers are a pain in the ass and we need to track a lot of information about them. 
			outAsset.rData->dynamic.buffers = (VkBuffer*)malloc(sizeof(VkBuffer) * def.numDynamicUniforms);
			outAsset.rData->dynamicUniformLayout = (uint32_t*)malloc(sizeof(uint32_t) * def.numDynamicUniforms * 3);
			outAsset.rData->numDynamicUniforms = def.numDynamicUniforms + def.numDynamicTextures;

			//all material mem should be device local, for perf
			VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			//we'll skip global buffers until we're writing out descriptor sets, and start with allocating / binding
			//the memory for the static buffers

			//start by writing out the default data our material file is providing for these bindings
			//this can be done basically at any point in the process before we write this data to the buffers
			std::vector<uint32_t> staticLayout;
			outAsset.rData->defaultStaticData = (char*)malloc(def.staticSetsSize);
			collectDefaultValuesIntoBufferAndBuildLayout(outAsset.rData->defaultStaticData, staticBindings, &staticLayout);

			//we can convert the layout array to a pointer for storage in our POD MaterialRenderData
			outMaterial.staticUniformLayout = (uint32_t*)malloc(sizeof(uint32_t) * staticLayout.size());
			memcpy(outMaterial.staticUniformLayout, staticLayout.data(), sizeof(uint32_t) * staticLayout.size());


			//then create buffers for each of those bindings
			createBuffersForDescriptorSetBindingArray(staticBindings, &outAsset.rData->staticBuffers[0], memFlags);	

			//allocate memory for those buffers
			//we need to pass in one buffer to grab the memory type bits for our device memory. 
			//since all our buffers have the same memory flags / properties, this will be the same for all of them 
			allocateDeviceMemoryForBuffers(outAsset.rData->staticUniformMem, def.staticSetsSize, &outAsset.rData->staticBuffers[0], memFlags);
			bindBuffersToMemory(outAsset.rData->staticUniformMem, outAsset.rData->staticBuffers, staticBindings);

			//now we have all our default data written in the staticDefaultData array, we can map it to the device memory bound to the static
			//buffers in one shot
			fillBuffersWithDefaultValues(outAsset.rData->staticBuffers, def.staticSetsSize, outAsset.rData->defaultStaticData, staticBindings);


			//next we do the same for dynamic uniform memory, except we also need to create the layout structure so we can edit this later. 
			std::vector<uint32_t> dynLayout;
			outAsset.rData->defaultDynamicData = (char*)malloc(def.dynamicSetsSize);
			collectDefaultValuesIntoBufferAndBuildLayout(outAsset.rData->defaultDynamicData, dynamicBindings, &dynLayout);

			//we can convert the layout array to a pointer for storage in our POD MaterialRenderData
			outMaterial.dynamicUniformLayout = (uint32_t*)malloc(sizeof(uint32_t) * dynLayout.size());
			memcpy(outMaterial.dynamicUniformLayout, dynLayout.data(), sizeof(uint32_t) * dynLayout.size());

			//same as before, we need to create buffers, alloc memory, bind it to the buffers
			createBuffersForDescriptorSetBindingArray(dynamicBindings, &outAsset.rData->dynamic.buffers[0], memFlags);
			allocateDeviceMemoryForBuffers(outAsset.rData->dynamic.uniformMem, def.dynamicSetsSize, &outAsset.rData->dynamic.buffers[0], memFlags);
			bindBuffersToMemory(outAsset.rData->dynamic.uniformMem, outAsset.rData->dynamic.buffers, dynamicBindings);
			fillBuffersWithDefaultValues(outAsset.rData->dynamic.buffers, def.dynamicSetsSize, outAsset.rData->defaultDynamicData, dynamicBindings);
		}

		

		///////////////////////////////////////////////////////////////////////////////
		//allocate descriptor sets
		///////////////////////////////////////////////////////////////////////////////
		{
			//now that everything is set bup with our buffers, we need to set up the descriptor sets that will actually
			//use those buffers. the first step is allocating them

			outMaterial.descSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * uniformLayouts.size());
			outMaterial.numDescSets = uniformLayouts.size();

			for (uint32_t j = 0; j < uniformLayouts.size(); ++j)
			{
				VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&uniformLayouts[j], 1, GContext.descriptorPool);
				res = vkAllocateDescriptorSets(GContext.device, &allocInfo, &outMaterial.descSets[j]);
				checkf(res == VK_SUCCESS, "Error allocating descriptor set");
			}
		}

		///////////////////////////////////////////////////////////////////////////////
		//Write descriptor sets
		///////////////////////////////////////////////////////////////////////////////

		//now we have everything we need to update our VkDescriptorSets with information about what buffers to use 
		//for their data sources. 
		std::vector<VkWriteDescriptorSet> descSetWrites;

		//we're going to queue up a bunch of descriptor write objects, and those objects
		//will store pointers to bufferInfo structs. To make sure those buffer info structs are still
		//around, we need to make sure this vector has reserved enough size at the beginning to never 
		//realloc
		std::vector<VkDescriptorBufferInfo> uniformBufferInfos;
		uniformBufferInfos.reserve(def.numStaticUniforms + def.numDynamicUniforms); //+1 in case we have global data

		//same deal here
		std::vector<VkDescriptorImageInfo> imageInfos;
		imageInfos.reserve(def.numDynamicTextures + def.numStaticTextures);

		//if we're using global data, we pull the data from wherever our global data has been initialized
		VkDescriptorBufferInfo globalBufferInfo;
		if (def.globalSets.size() > 0)
		{
			checkf(def.globalSets.size() == 1, "using more than 1 global buffer isn't supported right now");

			std::vector<DescriptorSetBinding> globalBindings = def.descSets[def.globalSets[0]];
		
			extern VkBuffer globalBuffer;
			extern uint32_t globalSize;

			globalBufferInfo.offset = 0;
			globalBufferInfo.buffer = globalBuffer;
			globalBufferInfo.range = globalSize;

			VkWriteDescriptorSet descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = outMaterial.descSets[0];
			descriptorWrite.dstBinding = 0; //refers to binding in shader
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = &globalBufferInfo;
			descriptorWrite.pImageInfo = 0;
			descriptorWrite.pTexelBufferView = nullptr; // Optional
			descSetWrites.push_back(descriptorWrite);

		}

		//static info is a bit more compliated because it might be images as well
		for (DescriptorSetBinding* bindingPtr : staticBindings)
		{
			DescriptorSetBinding& binding = *bindingPtr;

			VkWriteDescriptorSet descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = outMaterial.descSets[binding.set];
			descriptorWrite.dstBinding = binding.binding; //refers to binding in shader
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = inputTypeEnumToVkEnum(binding.type);
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = 0;
			descriptorWrite.pImageInfo = 0;

			if (binding.type == InputType::UNIFORM)
			{
				VkDescriptorBufferInfo uniformBufferInfo;
				uniformBufferInfo.offset = 0;
				uniformBufferInfo.buffer = outAsset.rData->staticBuffers[uniformBufferInfos.size()];
				uniformBufferInfo.range = binding.sizeBytes;

				uniformBufferInfos.push_back(uniformBufferInfo);
				descriptorWrite.pBufferInfo = &uniformBufferInfos[uniformBufferInfos.size() - 1];
			}
			else if (binding.type == InputType::SAMPLER)
			{
				VkDescriptorImageInfo imageInfo = {};
				uint32_t tex = Texture::make(binding.defaultValue);

				TextureRenderData* texData = Texture::getRenderData(tex);
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = texData->view;
				imageInfo.sampler = texData->sampler;

				imageInfos.push_back(imageInfo);
				descriptorWrite.pImageInfo = &imageInfos[imageInfos.size() - 1];

			}

			descriptorWrite.pTexelBufferView = nullptr; // Optional
			descSetWrites.push_back(descriptorWrite);
		}

		uint32_t dynamicBufferTotal = 0;
		uint32_t firstDynamicWriteIdx = 0;
		uint32_t dynamicTextureTotal = 0;

		for (DescriptorSetBinding* bindingPtr : dynamicBindings)
		{
			DescriptorSetBinding& binding = *bindingPtr;

			VkWriteDescriptorSet descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = outMaterial.descSets[binding.set];
			descriptorWrite.dstBinding = binding.binding; //refers to binding in shader
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = inputTypeEnumToVkEnum(binding.type);
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = 0;
			descriptorWrite.pImageInfo = 0;

			if (binding.type == InputType::UNIFORM)
			{
				VkDescriptorBufferInfo uniformBufferInfo;
				uniformBufferInfo.offset = 0;
				uniformBufferInfo.buffer = outAsset.rData->dynamic.buffers[dynamicBufferTotal++];
				uniformBufferInfo.range = binding.sizeBytes;

				uniformBufferInfos.push_back(uniformBufferInfo);
				descriptorWrite.pBufferInfo = &uniformBufferInfos[uniformBufferInfos.size() - 1];
			}
			else if (binding.type == InputType::SAMPLER)
			{
				VkDescriptorImageInfo imageInfo = {};
				uint32_t tex = Texture::make(binding.defaultValue);

				TextureRenderData* texData = Texture::getRenderData(tex);
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				//these have to be pointers to the material, not to a specific tex data? 
				imageInfo.imageView = texData->view;
				imageInfo.sampler = texData->sampler;

				imageInfos.push_back(imageInfo);
				descriptorWrite.pImageInfo = &imageInfos[imageInfos.size() - 1];

			}

			descriptorWrite.pTexelBufferView = nullptr; // Optional
			
			//this is a bit gross if we only have dynamic inputs, 
			//but it will still work. 
			if (firstDynamicWriteIdx == 0)
			{
				firstDynamicWriteIdx = descSetWrites.size();
			}

			descSetWrites.push_back(descriptorWrite);
		}

		//save off the descriptor set writes for dynamic data for easier updating later
		uint32_t numDynamicSetWrites = def.numDynamicTextures + def.numDynamicUniforms;
		uint32_t numStaticSetWrites = def.numStaticTextures + def.numStaticUniforms;

		uint32_t indexOfFirstDynamicSetWrite = def.globalSets.size() + numStaticSetWrites;

		outAsset.rData->dynamic.descriptorSetWrites = (VkWriteDescriptorSet*)malloc(sizeof(VkWriteDescriptorSet) * numDynamicSetWrites);
		memcpy(outAsset.rData->dynamic.descriptorSetWrites, &descSetWrites.data()[indexOfFirstDynamicSetWrite], sizeof(VkWriteDescriptorSet) * numDynamicSetWrites);

		//it's kinda weird that the order of desc writes has to be the order of sets. 
		vkUpdateDescriptorSets(GContext.device, descSetWrites.size(), descSetWrites.data(), 0, nullptr);

		///////////////////////////////////////////////////////////////////////////////
		//cleanup
		///////////////////////////////////////////////////////////////////////////////
		for (uint32_t i = 0; i < shaderStages.size(); ++i)
		{
			vkDestroyShaderModule(GContext.device, shaderStages[i].module, nullptr);
		}

	}
#endif

	//Material Instance Creation
	//return page
	uint32_t allocInstancePage(uint32_t baseMaterial)
	{
		MaterialRenderData& data = Material::getRenderData(baseMaterial);
		MaterialInstancePage page;
		data.instPages.push_back(page);

		MaterialInstancePage& newPage = data.instPages[data.instPages.size() - 1];

		///////////////////////////////////////////////////////////////////////////////
		//Allocate buffers for page
		///////////////////////////////////////////////////////////////////////////////

		VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		VkDeviceSize uboAlignment = vkh::GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;
		data.staticUniformMemSize = (data.staticUniformMemSize / uboAlignment) * uboAlignment + ((data.staticUniformMemSize % uboAlignment) > 0 ? uboAlignment : 0);
		data.dynamicUniformMemSize = (data.dynamicUniformMemSize / uboAlignment) * uboAlignment + ((data.dynamicUniformMemSize % uboAlignment) > 0 ? uboAlignment : 0);

		if (data.staticUniformMemSize > 0)
		{
			vkh::createBuffer(newPage.staticBuffer,
				data.staticUniformMemSize * MATERIAL_INSTANCE_PAGESIZE,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				memFlags);

			allocateDeviceMemoryForBuffers(newPage.staticMem, data.staticUniformMemSize * MATERIAL_INSTANCE_PAGESIZE, &newPage.staticBuffer, memFlags);

			vkBindBufferMemory(vkh::GContext.device, newPage.staticBuffer, newPage.staticMem.handle, newPage.staticMem.offset);
		}

		if (data.dynamicUniformMemSize > 0)
		{
			vkh::createBuffer(newPage.dynamicBuffer,
				data.dynamicUniformMemSize * MATERIAL_INSTANCE_PAGESIZE,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				memFlags);

			allocateDeviceMemoryForBuffers(newPage.dynamicMem, data.dynamicUniformMemSize * MATERIAL_INSTANCE_PAGESIZE, &newPage.dynamicBuffer, memFlags);

			vkBindBufferMemory(vkh::GContext.device, newPage.dynamicBuffer, newPage.dynamicMem.handle, newPage.dynamicMem.offset);
		}

		for (uint32_t i = 0; i < MATERIAL_INSTANCE_PAGESIZE; ++i)
		{
			newPage.freeIndices.push(i);
			newPage.generation[i] = 0;
		}

		///////////////////////////////////////////////////////////////////////////////
		//Allocate / set up descriptor sets for page
		///////////////////////////////////////////////////////////////////////////////

		//each page only needs a single set of descriptor sets, since we'll bind them and use offsets to get to 
		//individual page members when needed.

		newPage.numPageDescSets = data.numDescSets - data.usesGlobalData;
		newPage.descSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * newPage.numPageDescSets);

		VkDescriptorBufferInfo* bufferInfos = (VkDescriptorBufferInfo*)malloc(sizeof(VkDescriptorBufferInfo) * data.numDefaultBufferInfos);
		memcpy(bufferInfos, data.defaultBufferInfos, sizeof(VkDescriptorBufferInfo) * data.numDefaultBufferInfos);
		
		VkDescriptorImageInfo* imageInfos = (VkDescriptorImageInfo*)malloc(sizeof(VkDescriptorImageInfo) * data.numDefaultImageInfos);
		memcpy(imageInfos, data.defaultImageInfos, sizeof(VkDescriptorImageInfo) * data.numDefaultImageInfos);

		VkWriteDescriptorSet* descSetWrites = (VkWriteDescriptorSet*)malloc(sizeof(VkWriteDescriptorSet) * (data.numDefaultStaticWrites + data.numDefaultDynamicWrites));
		memcpy(descSetWrites, data.defaultDescWrites, sizeof(VkWriteDescriptorSet) * (data.numDefaultStaticWrites + data.numDefaultDynamicWrites));

		for (uint32_t i = 0; i < newPage.numPageDescSets; ++i)
		{
			VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&data.descriptorSetLayouts[i+data.usesGlobalData], 1, vkh::GContext.descriptorPool);
			VkResult res = vkAllocateDescriptorSets(vkh::GContext.device, &allocInfo, &newPage.descSets[i]);
			checkf(res == VK_SUCCESS, "Error allocating descriptor set");
		}

		uint32_t totalDescWrites = data.numDefaultStaticWrites + data.numDefaultDynamicWrites;

		newPage.numPageDynamicBuffers = 0;

		uint32_t curBuffer = 0;
		uint32_t curImage = 0;

		for (uint32_t i = 0; i < totalDescWrites; ++i)
		{
			if (data.defaultDescWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				descSetWrites[i].pImageInfo = &imageInfos[curImage++];

			}
			else if (data.defaultDescWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			{
				bufferInfos[curBuffer].buffer = (i >= data.numDefaultStaticWrites) ? newPage.dynamicBuffer : newPage.staticBuffer;
				descSetWrites[i].pBufferInfo = &bufferInfos[curBuffer++];
				newPage.numPageDynamicBuffers++;
			}

			//HACK - store the set number in the count since we're always using 1 descriptor per write 
			uint32_t targetSet = data.defaultDescWrites[i].descriptorCount;
			uint32_t binding = data.defaultDescWrites[i].dstBinding;
			
			descSetWrites[i].descriptorCount = 1;
			descSetWrites[i].dstSet = newPage.descSets[targetSet-data.usesGlobalData];
		}

		vkUpdateDescriptorSets(vkh::GContext.device, totalDescWrites, descSetWrites, 0, nullptr);

		free(descSetWrites);
		free(bufferInfos);
		free(imageInfos);

		return static_cast<uint32_t>(data.instPages.size()) - 1;
	}

	MaterialInstance getFreeInstance(uint32_t parent)
	{
		MaterialRenderData& rData = Material::getRenderData(parent);

		MaterialInstance inst = { 0,0,0,0 };
		inst.parent = parent;

		bool foundSlot = false;

		for (uint32_t page = 0; page < rData.instPages.size(); ++page)
		{
			MaterialInstancePage& curPage = rData.instPages[page];

			if (curPage.freeIndices.size() > 0)
			{
				uint8_t slot = curPage.freeIndices.front();
				curPage.freeIndices.pop();

				inst.page = page;
				inst.index = slot;

				curPage.generation[slot]++;
				inst.generation = curPage.generation[slot];

				foundSlot = true;
			}
		}

		if (!foundSlot)
		{
			//if code reaches here, there's no room in the parent material's
			//instance pages for this instance, and we need to alloc a new page
			uint32_t newPageIdx = allocInstancePage(inst.parent);
			MaterialInstancePage& curPage = rData.instPages[newPageIdx];

			inst.page = newPageIdx;
			inst.index = curPage.freeIndices.front();
			curPage.freeIndices.pop();
		}
		return inst;

	}

	MaterialInstance duplicateInstance(MaterialInstance copyFrom)
	{
		///////////////////////////////////////////////////////////////////////////////
		//Place Material Instance in a page
		///////////////////////////////////////////////////////////////////////////////			
		MaterialRenderData& rData = Material::getRenderData(copyFrom.parent);
		MaterialInstance inst = getFreeInstance(copyFrom.parent);

		///////////////////////////////////////////////////////////////////////////////
		//Set up buffers
		///////////////////////////////////////////////////////////////////////////////			

		vkh::VkhCommandBuffer scratch = vkh::beginScratchCommandBuffer(vkh::ECommandPoolType::Transfer);
	
		if (rData.staticUniformMemSize > 0)
		{
			vkh::copyBuffer(rData.instPages[copyFrom.page].staticBuffer, rData.instPages[inst.page].staticBuffer, rData.staticUniformMemSize, copyFrom.index * rData.staticUniformMemSize, inst.index * rData.staticUniformMemSize, scratch);
		}

		if (rData.dynamicUniformMemSize > 0)
		{
			vkh::copyBuffer(rData.instPages[copyFrom.page].dynamicBuffer, rData.instPages[inst.page].dynamicBuffer, rData.dynamicUniformMemSize, copyFrom.index * rData.dynamicUniformMemSize, inst.index * rData.dynamicUniformMemSize, scratch);
		}

		vkh::submitScratchCommandBuffer(scratch);
		return inst;
	}


	MaterialInstance makeInstance(InstanceDefinition def)
	{
		///////////////////////////////////////////////////////////////////////////////
		//Place Material Instance in a page
		///////////////////////////////////////////////////////////////////////////////			
		MaterialRenderData& rData = Material::getRenderData(def.parent);
		MaterialInstance inst = getFreeInstance(def.parent);
		
		///////////////////////////////////////////////////////////////////////////////
		//Set up buffers
		///////////////////////////////////////////////////////////////////////////////			

		char* staticData = (char*)malloc(rData.staticUniformMemSize);
		memcpy(staticData, rData.defaultStaticData, rData.staticUniformMemSize);

		char* dynData = (char*)malloc(rData.dynamicUniformMemSize);
		memcpy(dynData, rData.defaultDynamicData, rData.dynamicUniformMemSize);
		size_t uboAlignment = vkh::GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;

		for (Material::ShaderInputDefinition& inputDef : def.defaults)
		{
			uint32_t varHash = hash(inputDef.name);

			for (uint32_t i = 0; i < rData.numStaticUniforms * MATERIAL_UNIFORM_LAYOUT_STRIDE; i += MATERIAL_UNIFORM_LAYOUT_STRIDE)
			{
				if (rData.staticUniformLayout[i] == varHash)
				{
					uint32_t size = rData.staticUniformLayout[i + MEMBER_SIZE_IDX];
					uint32_t offset = rData.staticUniformLayout[i + MEMBER_OFFSET_IDX];
					
					memcpy(staticData + offset, inputDef.value, size);
				}
			}


			for (uint32_t i = 0; i < rData.numDynamicUniforms * MATERIAL_UNIFORM_LAYOUT_STRIDE; i += MATERIAL_UNIFORM_LAYOUT_STRIDE)
			{
				if (rData.dynamicUniformLayout[i] == varHash)
				{
					uint32_t size = rData.dynamicUniformLayout[i + MEMBER_SIZE_IDX];
					uint32_t offset = rData.dynamicUniformLayout[i + MEMBER_OFFSET_IDX];
					memcpy(dynData + offset, inputDef.value, size);
				}
			}
		}

		if (rData.staticUniformMemSize)
		{
			fillBufferWithData(&rData.instPages[inst.page].staticBuffer, rData.staticUniformMemSize, inst.index * rData.staticUniformMemSize, staticData);
		}

		if (rData.dynamicUniformMemSize)
		{
			fillBufferWithData(&rData.instPages[inst.page].dynamicBuffer, rData.dynamicUniformMemSize, inst.index * rData.dynamicUniformMemSize, dynData);
		}

		free(dynData);
		free(staticData);

		return inst;
	}
}