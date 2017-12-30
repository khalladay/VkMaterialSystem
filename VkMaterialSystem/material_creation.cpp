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

//Material Creation
namespace Material
{
	InputType stringToInputType(const char* str);
	ShaderStage stringToShaderStage(const char* str);
	const char* shaderExtensionForStage(ShaderStage stage);
	const char* shaderReflExtensionForStage(ShaderStage stage);
	VkShaderStageFlags shaderStageVectorToVkEnum(std::vector<ShaderStage>& vec);
	VkShaderStageFlagBits shaderStageEnumToVkEnum(ShaderStage stage);
	VkDescriptorType inputTypeEnumToVkEnum(InputType type);
	uint32_t allocInstancePage(uint32_t baseMaterial);
	Instance makeInstance(uint32_t baseMaterial);

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

	VkDescriptorType inputTypeEnumToVkEnum(InputType type)
	{
		if (type == InputType::SAMPLER) return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		if (type == InputType::UNIFORM) return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

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

	Definition load(const char* assetPath)
	{
		using namespace rapidjson;

		Material::Definition materialDef = {};

		const static char* generatedShaderPath = "../data/_generated/builtshaders/";

		const char* materialString = loadTextFile(assetPath);
		size_t len = strlen(materialString);

		Document materialDoc;
		materialDoc.Parse(materialString, len);
		checkf(!materialDoc.HasParseError(), "Error parsing material file");
		free((void*)materialString);

		const Value& shaders = materialDoc["shaders"];
		materialDef.stages.reserve(shaders.Size());

		/*
		this will iterate over each shader in a material file
		each shader is laid out in json like so: 
		
		{
			"stage": "fragment",
			"shader" : "fragment_passthrough",
			"inputs" :
			[
				... defined in later comment ... 
			]
		}
		*/

		for (SizeType i = 0; i < shaders.Size(); i++)
		{
			ShaderStageDefinition stageDef = {};

			const Value& matStage = shaders[i];
			stageDef.stage = stringToShaderStage(matStage["stage"].GetString());

			int printfErr = 0;
			printfErr = snprintf(stageDef.shaderPath, sizeof(stageDef.shaderPath), "%s%s%s", generatedShaderPath, matStage["shader"].GetString(), shaderExtensionForStage(stageDef.stage));
			checkf(printfErr > -1, "");

			materialDef.stages.push_back(stageDef);

			//now we need to get information about the layout of the shader inputs
			//this isn't necessarily known when writing a material file, so we have to 
			//grab it from a reflection file

			char reflPath[256];
			printfErr = snprintf(reflPath, sizeof(reflPath), "%s%s%s", generatedShaderPath, matStage["shader"].GetString(), shaderReflExtensionForStage(stageDef.stage));
			checkf(printfErr > -1, "");

			const char* reflData = loadTextFile(reflPath);
			size_t reflLen = strlen(reflData);

			std::vector<uint32_t> blocksWithDefaultsPresent;

			if (matStage.HasMember("defaults"))
			{
				const Value& arrayOfDefaultValuesFromMaterial = matStage["defaults"];

				for (SizeType d = 0; d < arrayOfDefaultValuesFromMaterial.Size(); ++d)
				{
					const Value& default = arrayOfDefaultValuesFromMaterial[d];
					blocksWithDefaultsPresent.push_back(hash(default["name"].GetString()));
				}
			}


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
					const Value& currentInputFromReflData = reflFileDescSets[dsIdx];

					DescriptorSetBinding descSetBindingDef = {};
					descSetBindingDef.sizeBytes = getGPUAlignedSize(currentInputFromReflData["size"].GetInt());
					descSetBindingDef.set = currentInputFromReflData["set"].GetInt();
					descSetBindingDef.binding = currentInputFromReflData["binding"].GetInt();
					descSetBindingDef.type = stringToInputType(currentInputFromReflData["type"].GetString());

					if (std::find(materialDef.staticSets.begin(), materialDef.staticSets.end(), descSetBindingDef.set) != materialDef.staticSets.end())
					{
						materialDef.staticSetsSize += descSetBindingDef.sizeBytes;
					}
					else if (std::find(materialDef.dynamicSets.begin(), materialDef.dynamicSets.end(), descSetBindingDef.set) != materialDef.dynamicSets.end())
					{
						materialDef.dynamicSetsSize += descSetBindingDef.sizeBytes;
					}

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
						descSetBindingDef.owningStages.push_back(stageDef.stage);

						checkf(currentInputFromReflData["name"].GetStringLength() < 31, "opaque block names must be less than 32 characters");
						snprintf(descSetBindingDef.name, sizeof(descSetBindingDef.name), "%s", currentInputFromReflData["name"].GetString());

						int blockDefaultsIndex = -1;
						uint32_t hashedBlockName = hash(currentInputFromReflData["name"].GetString());
						for (uint32_t d = 0; d < blocksWithDefaultsPresent.size(); ++d)
						{
							if (blocksWithDefaultsPresent[d] == hashedBlockName)
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
							snprintf(mem.name, sizeof(mem.name), "%s", reflBlockMember["name"].GetString());
							descSetBindingDef.blockMembers.push_back(mem);
						}

						//if the block member has a default defined in the material
						//we want to grab it. For samplers, this is the path of their texture, 
						//for uniform blocks, this is an array of floats for each member. 
						if (blockDefaultsIndex > -1)
						{
							const Value& arrayOfDefaultValuesFromMaterial = matStage["defaults"];
							const Value& defaultItem = arrayOfDefaultValuesFromMaterial[blockDefaultsIndex];

							if (descSetBindingDef.type == InputType::SAMPLER)
							{
								const Value& defaultValue = defaultItem["value"];
								
								materialDef.rootInstanceDefinition.defaultValues[hashedBlockName];

								snprintf(materialDef.rootInstanceDefinition.defaultValues[hashedBlockName].value
									, sizeof(materialDef.rootInstanceDefinition.defaultValues[hashedBlockName].value)
									, "%s", defaultValue.GetString());
							}
							else
							{
								const Value& defaultBlockMembers = defaultItem["members"];

								//remmeber, we dont' know which members have default values specified yet,
								for (SizeType m = 0; m < descSetBindingDef.blockMembers.size(); m++)
								{
									BlockMember& mem = descSetBindingDef.blockMembers[m];
									uint32_t memName = hash(mem.name);
									
									materialDef.rootInstanceDefinition.defaultValues[memName];
									memset(&materialDef.rootInstanceDefinition.defaultValues[memName], 0, sizeof(float) * 16);

									//since we might not have default values for each block member, 
									//we need to make sure we're grabbing data for our current member
									//loop over all the default blocks we have and find one that corresponds to this block member
									for (uint32_t defaultMemIdx = 0; defaultMemIdx < defaultBlockMembers.Size(); ++defaultMemIdx)
									{
										if (hash(defaultBlockMembers[defaultMemIdx]["name"].GetString()) == hash(mem.name))
										{
											const Value& defaultValues = defaultBlockMembers[defaultMemIdx]["value"];
											float* defaultFloats = (float*)materialDef.rootInstanceDefinition.defaultValues[memName].value;
											for (uint32_t dv = 0; dv < defaultValues.Size(); ++dv)
											{
												defaultFloats[dv] = defaultValues[dv].GetFloat();
											}
											break;

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

	uint32_t createSingleBufferForDescriptorSetBindingArray(std::vector<DescriptorSetBinding*>& input, VkBuffer* dst, VkMemoryPropertyFlags memFlags)
	{
		uint32_t totalSize = 0;
		for (DescriptorSetBinding* binding : input)
		{
			if (binding->type == InputType::UNIFORM)
			{
				totalSize += binding->sizeBytes;
			}
		}

		vkh::createBuffer(*dst,
			totalSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			memFlags);

		return totalSize;
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

	void fillBufferWithData(VkBuffer* buffer, uint32_t dataSize, char* data)
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
		vkh::copyBuffer(stagingBuffer,*buffer, dataSize, 0, 0, scratch);
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
	
	void buildUniformLayout(std::vector<DescriptorSetBinding*>& bindings, std::vector<uint32_t>& outLayout)
	{
		uint32_t bufferOffset = 0;
		uint32_t curImage = 0;
		uint32_t total = 0;		//meed this number to know the index into the descriptor set write array our image will be

		for (DescriptorSetBinding* binding : bindings)
		{
			if (binding->type == InputType::UNIFORM)
			{
				//since each binding has to be created with a gpu aligned size, we can't just sum buffer offsets
				for (uint32_t k = 0; k < binding->blockMembers.size(); ++k)
				{
					uint32_t hashedBlockName = hash(binding->blockMembers[k].name);
					
					outLayout.push_back(hashedBlockName);
					outLayout.push_back(bufferOffset);
					outLayout.push_back(binding->blockMembers[k].size);
					outLayout.push_back(binding->blockMembers[k].offset);
				}

				bufferOffset += binding->sizeBytes;
			}
			else
			{
				outLayout.push_back(hash(binding->name));
				outLayout.push_back(curImage++);
				outLayout.push_back(total);
				outLayout.push_back(MATERIAL_IMAGE_FLAG_VALUE);
			}
			total++;
		}
	}

	void collectDefaultValuesIntoBuffer(char* outBuffer, std::vector<DescriptorSetBinding*> bindings, InstanceDefinition& inst)
	{
		uint32_t bufferOffset = 0;
		uint32_t curBuffer = 0; //need this number to know the index into our VkBuffer array that a uniform block will be
		uint32_t curImage = 0;

		for (DescriptorSetBinding* binding : bindings)
		{
			if (binding->type == InputType::UNIFORM)
			{
				for (uint32_t k = 0; k < binding->blockMembers.size(); ++k)
				{
					uint32_t hashedBlockName = hash(binding->blockMembers[k].name);
					memcpy(&outBuffer[0] + bufferOffset + binding->blockMembers[k].offset, &(inst.defaultValues[hashedBlockName].value), binding->blockMembers[k].size);
				}

				bufferOffset += binding->sizeBytes;
				curBuffer++;
			}
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

	//making a material creates the base material object and returns the root instance
	Material::Instance make(uint32_t id, Definition def)
	{
		using vkh::GContext;

		//this is what we actually return
		Material::Instance rootInstance;

		//convenience vars
		Asset& outAsset = Material::getMaterialAsset(id);
		outAsset.rData = (MaterialRenderData*)calloc(1, sizeof(MaterialRenderData));
		MaterialRenderData& outMaterial = *outAsset.rData;

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
		////Map Bindings
		/////////////////////////////////////////////////////////////////////////////////

		//for convenience, we need to build arrays of the static and dynamic bindings
		//saves us having to iterate over the map a bunch later, we still want the map of all of the bindings though, 
		//since that makes a few things easier for us to do

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

		VkResult res;		

		/////////////////////////////////////////////////////////////////////////////////
		////set up descriptorSetLayouts
		/////////////////////////////////////////////////////////////////////////////////

		//The second step of setting up the material is getting an array of VkDescriptorSetLayouts, 
		//one for each descriptor set in the shaders used by the material. 
		std::vector<VkDescriptorSetLayout> uniformLayouts;

		//it's important to note that this array has to have no gaps in set number, so if a set
		//isn't used by the shaders, we have to add an empty VkDescriptorSetLayout. 

		//VkDescriptorSetLayouts are created from arrays of VkDescriptorSetLayoutBindings, one for each
		//binding in the set, so first we use the descSets array on our material definition to give us a 
		//map that has an array of VkDescriptorSetLayoutBindings for each descriptor set index (the key of the map) 
		std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> uniformSetBindings;

		//the rest of this logic can be wrapped in braces so we can collapse it for easier reading
		//no other vars are declared that need to be visible outside of this block
		{
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
							VkDescriptorSetLayoutBinding layoutBinding = vkh::descriptorSetLayoutBinding(inputTypeEnumToVkEnum(binding.type), shaderStageVectorToVkEnum(binding.owningStages), binding.binding, 1);
							uniformSetBindings[binding.set].push_back(layoutBinding);
						}

						remainingInputs--;
						needsEmpty = false;
					}
				}
				if (needsEmpty)
				{
					VkDescriptorSetLayoutBinding layoutBinding = vkh::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0, 0);
					uniformSetBindings[curSet].push_back(layoutBinding);
				}

				curSet++;

			}

			//now that we have our map of bindings for each set, we know how many VKDescriptorSetLayouts we're going to need:
			uniformLayouts.resize(uniformSetBindings.size());

			//and we can generate the VkDescriptorSetLayouts from the map arrays
			for (auto& bindingCollection : uniformSetBindings)
			{
				uint32_t set = bindingCollection.first;

				std::vector<VkDescriptorSetLayoutBinding>& setBindings = uniformSetBindings[bindingCollection.first];
				VkDescriptorSetLayoutCreateInfo layoutInfo = vkh::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));

				res = vkCreateDescriptorSetLayout(GContext.device, &layoutInfo, nullptr, &uniformLayouts[bindingCollection.first]);
			}

			//for sanity in storage, we want to keep MaterialAssets and MaterialRenderDatas POD structs, so we need to convert our lovely
			//containers to arrays. This might change later, if I decide to start using POD arrays. In any case, in a real application you'd
			//almost certainly want these allocations done with any allocator other than malloc
			outMaterial.numDescSetLayouts = static_cast<uint32_t>(uniformLayouts.size());

			outMaterial.descriptorSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * outMaterial.numDescSetLayouts);
			memcpy(outMaterial.descriptorSetLayouts, uniformLayouts.data(), sizeof(VkDescriptorSetLayout) * outMaterial.numDescSetLayouts);
		}
		
		///////////////////////////////////////////////////////////////////////////////
		//set up pipeline layout
		///////////////////////////////////////////////////////////////////////////////
		{
			//we also use the descriptor set layouts to set up our pipeline layout
			VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkh::pipelineLayoutCreateInfo(outMaterial.descriptorSetLayouts, outMaterial.numDescSetLayouts);

			//we need to figure out what's up with push constants here becaus the pipeline layout object needs to know
			if (def.pcBlock.sizeBytes > 0)
			{
				VkPushConstantRange pushConstantRange = {};
				pushConstantRange.offset = 0;
				pushConstantRange.size = def.pcBlock.sizeBytes;
				pushConstantRange.stageFlags = shaderStageVectorToVkEnum(def.pcBlock.owningStages);

				outAsset.rData->pushConstantLayout.visibleStages = pushConstantRange.stageFlags;

				pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
				pipelineLayoutInfo.pushConstantRangeCount = 1;

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
		}

		///////////////////////////////////////////////////////////////////////////////
		//set up graphics pipeline
		///////////////////////////////////////////////////////////////////////////////
		{
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
		}
		
		///////////////////////////////////////////////////////////////////////////////
		//allocate descriptor sets
		///////////////////////////////////////////////////////////////////////////////
		{
			//now that everything is set bup with our buffers, we need to set up the descriptor sets that will actually
			//use those buffers. the first step is allocating them

			outMaterial.descSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * uniformLayouts.size());
			outMaterial.numDescSets = outMaterial.numDescSetLayouts;

			for (uint32_t j = 0; j < uniformLayouts.size(); ++j)
			{
				VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&outMaterial.descriptorSetLayouts[j], 1, GContext.descriptorPool);
				res = vkAllocateDescriptorSets(GContext.device, &allocInfo, &outMaterial.descSets[j]);
				checkf(res == VK_SUCCESS, "Error allocating descriptor set");
			}
		}

		///////////////////////////////////////////////////////////////////////////////
		//Build Uniform Layouts
		///////////////////////////////////////////////////////////////////////////////			
		{
			outAsset.rData->dynamicUniformMemSize = def.dynamicSetsSize;
			outAsset.rData->staticUniformMemSize = def.staticSetsSize;

			std::vector<uint32_t> staticLayout;
			buildUniformLayout(staticBindings, staticLayout);
			collectDefaultValuesIntoBuffer(outAsset.rData->defaultStaticData, staticBindings, def.rootInstanceDefinition);

			std::vector<uint32_t> dynamicLayout;
			buildUniformLayout(dynamicBindings, dynamicLayout);
			collectDefaultValuesIntoBuffer(outAsset.rData->defaultDynamicData, dynamicBindings, def.rootInstanceDefinition);

			//we can convert the layout array to a pointer for storage
			outMaterial.staticLayout = (uint32_t*)malloc(sizeof(uint32_t) * staticLayout.size());
			memcpy(outMaterial.staticLayout, staticLayout.data(), sizeof(uint32_t) * staticLayout.size());

			outMaterial.dynamicLayout = (uint32_t*)malloc(sizeof(uint32_t) * dynamicLayout.size());
			memcpy(outMaterial.dynamicLayout, dynamicLayout.data(), sizeof(uint32_t) * dynamicLayout.size());
		
			//save off the descriptor set writes for dynamic data for easier updating later
			outAsset.rData->dynamicDescSetStride = def.numDynamicTextures + def.numDynamicUniforms;
			outAsset.rData->staticDescSetStride = def.numStaticTextures + def.numStaticUniforms;
		}

		///////////////////////////////////////////////////////////////////////////////
		//cleanup
		///////////////////////////////////////////////////////////////////////////////
		for (uint32_t i = 0; i < shaderStages.size(); ++i)
		{
			vkDestroyShaderModule(GContext.device, shaderStages[i].module, nullptr);
		}

		///////////////////////////////////////////////////////////////////////////////
		//Write Global Descriptor Set
		///////////////////////////////////////////////////////////////////////////////

		//if we're using global data, we pull the data from wherever our global data has been initialized
		if (def.globalSets.size() > 0)
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
			globalDescriptorWrite.dstSet = outMaterial.descSets[0];
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
		//Create Root Instance
		///////////////////////////////////////////////////////////////////////////////			
		{
			//all "materials" applied to meshes are actually material instances, which reference
			//buffer locations inside a larger allocation stored on the material. When creating a new
			//material, we need to also create the 0th, or "root" instance. 
			size_t uboAlignment = GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;

			uint32_t rootPage = allocInstancePage(id);
			checkf(rootPage == 0, "allocating first page of root instance, but page index is not zero, wtf?");

			rootInstance = makeInstance(def.rootInstanceDefinition);
		}

		return rootInstance;
	}

	//Material Instance Creation
	//return page
	uint32_t allocInstancePage(uint32_t baseMaterial)
	{
		MaterialRenderData& data = Material::getRenderData(baseMaterial);
		MaterialInstancePage page;
		data.instPages.push_back(page);

		MaterialInstancePage& newPage = *data.instPages.end();

		VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (data.staticUniformMemSize > 0)
		{
			vkh::createBuffer(newPage.staticBuffer,
				data.staticUniformMemSize * MATERIAL_INSTANCE_PAGESIZE,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				memFlags);

			allocateDeviceMemoryForBuffers(newPage.staticMem, data.staticUniformMemSize * MATERIAL_INSTANCE_PAGESIZE, &newPage.staticBuffer, memFlags);

			vkBindBufferMemory(vkh::GContext.device, newPage.staticBuffer, newPage.staticMem.handle, newPage.staticMem.offset);
		}

		if (data.dynamicUniformMemSize > 0)
		{
			vkh::createBuffer(newPage.dynamicBuffer,
				data.dynamicUniformMemSize * MATERIAL_INSTANCE_PAGESIZE,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				memFlags);

			allocateDeviceMemoryForBuffers(newPage.dynamicMem, data.dynamicUniformMemSize * MATERIAL_INSTANCE_PAGESIZE, &newPage.dynamicBuffer, memFlags);

			vkBindBufferMemory(vkh::GContext.device, newPage.dynamicBuffer, newPage.dynamicMem.handle, newPage.dynamicMem.offset);
		}

		for (uint32_t i = 0; i < MATERIAL_INSTANCE_PAGESIZE; ++i)
		{
			newPage.freeIndices.push(i);
			newPage.generation[i] = 0;
		}

		return data.instPages.size() - 1;
	}

	//without other arg, this will clone static data from root instance
	Instance makeInstance(InstanceDefinition def)
	{
		///////////////////////////////////////////////////////////////////////////////
		//Place Material Instance in a page
		///////////////////////////////////////////////////////////////////////////////			
		uint32_t parentName = charArrayToMaterialName(def.parentName);
		MaterialRenderData& data = Material::getRenderData(parentName);

		Instance inst = { 0,0,0,0 };
		inst.parent = parentName;

		bool foundSlot = false;

		for (uint32_t page = 0; page < data.instPages.size(); ++page)
		{
			MaterialInstancePage& curPage = data.instPages[page];

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
			uint32_t newPageIdx = allocInstancePage(parentName);
			MaterialInstancePage& curPage = data.instPages[newPageIdx];

			inst.page = newPageIdx;
			inst.index = curPage.freeIndices.front();
			curPage.freeIndices.pop();
		}
		
		///////////////////////////////////////////////////////////////////////////////
		//Set up buffers
		///////////////////////////////////////////////////////////////////////////////			

		char* staticData = (char*)malloc(data.staticUniformMemSize);
		memcpy(staticData, data.defaultStaticData, data.staticUniformMemSize);

		char* dynData = (char*)malloc(data.dynamicUniformMemSize);
		memcpy(dynData, data.defaultDynamicData, data.dynamicUniformMemSize);

		for (UniformValue& v : def.defaultValues)
		{
			uint32_t varHash = v.name;

			for (uint32_t i = 0; i < data.numDynamicInputs * 4; i += 4)
			{
				if (data.dynamicLayout[i] == varHash)
				{
					if (data.dynamicLayout[i + 3] == MATERIAL_IMAGE_FLAG_VALUE)
					{

					}
					else
					{

					}
				}
		}


		return inst;
	}

	void destroyInstance(Material::Instance instance)
	{

	}
}