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
	InputType stringToInputType(const char* str);
	ShaderStage stringToShaderStage(const char* str);
	const char* shaderExtensionForStage(ShaderStage stage);
	const char* shaderReflExtensionForStage(ShaderStage stage);
	VkShaderStageFlags shaderStageVectorToVkEnum(std::vector<ShaderStage>& vec);
	VkShaderStageFlagBits shaderStageEnumToVkEnum(ShaderStage stage);
	VkDescriptorType inputTypeEnumToVkEnum(InputType type);

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

		const Value& shaders = materialDoc["shaders"];
		materialDef.stages.reserve(shaders.Size());

		for (SizeType i = 0; i < shaders.Size(); i++)
		{
			ShaderStageDefinition stageDef = {};

			const Value& matStage = shaders[i];
			stageDef.stage = stringToShaderStage(matStage["stage"].GetString());
			
			snprintf(stageDef.shaderPath, sizeof(stageDef.shaderPath), "%s%s%s", generatedShaderPath, matStage["shader"].GetString(), shaderExtensionForStage(stageDef.stage));

			char reflPath[256];
			snprintf(reflPath, sizeof(reflPath), "%s%s%s", generatedShaderPath, matStage["shader"].GetString(), shaderReflExtensionForStage(stageDef.stage));

			const char* reflData = loadTextFile(reflPath);

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

					if (!memberAlreadyExists)
					{
						materialDef.pcBlock.blockMembers.push_back(mem);
					}
				}
			}

			if (reflDoc.HasMember("inputs"))
			{
				const Value& uniformInputsFromReflData = reflDoc["inputs"];				

				std::vector<uint32_t> blocksWithDefaultsPresent;
				const Value& defaultArray = matStage["inputs"];

				for (SizeType d = 0; d < defaultArray.Size(); ++d)
				{
					const Value& default = defaultArray[d];
					blocksWithDefaultsPresent.push_back(hash(default["name"].GetString()));
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
						materialDef.inputCount++;

						blockDef.owningStages.push_back(stageDef.stage);

						checkf(currentInputFromReflData["name"].GetStringLength() < 31, "opaque block names must be less than 32 characters");
						snprintf(blockDef.name, sizeof(blockDef.name), "%s", currentInputFromReflData["name"].GetString());

						int blockDefaultsIndex = -1;
						for (uint32_t d = 0; d < blocksWithDefaultsPresent.size(); ++d)
						{
							if (blocksWithDefaultsPresent[d] == hash(currentInputFromReflData["name"].GetString()))
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
							blockDef.blockMembers.push_back(mem);
						}


						if (blockDefaultsIndex > -1)
						{
							const Value& defaultItem = defaultArray[blockDefaultsIndex];

							if (blockDef.type == InputType::SAMPLER)
							{
								const Value& defaultValue = defaultItem["value"];
								snprintf(blockDef.defaultValue, sizeof(blockDef.defaultValue), "%s", defaultValue.GetString());
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

	void make(const char* materialPath)
	{
		make(load(materialPath));
	}

	void make(Definition def)
	{
		using vkh::GContext;

		MaterialAsset& outAsset = Material::getMaterialAsset();
		outAsset.rData = (MaterialRenderData*)calloc(1,sizeof(MaterialRenderData));

		vkh::VkhMaterial& outMaterial = outAsset.rData->vkMat;


		VkResult res;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		for (uint32_t i = 0; i < def.stages.size(); ++i)
		{
			ShaderStageDefinition& stageDef = def.stages[i];
			VkPipelineShaderStageCreateInfo shaderStageInfo = vkh::shaderPipelineStageCreateInfo(shaderStageEnumToVkEnum(stageDef.stage));
			vkh::createShaderModule(shaderStageInfo.module, stageDef.shaderPath, GContext.device);
			shaderStages.push_back(shaderStageInfo);
		}

		///////////////////////////////////////////////////////////////////////////////
		//set up descriptorSetLayout
		///////////////////////////////////////////////////////////////////////////////

		std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> uniformSetBindings;
		uint32_t inputOffset = 0;


		uint32_t curSet = 0;
		uint32_t remainingInputs = def.inputs.size();

		while (remainingInputs)
		{
			bool needsEmpty = true;
			for (auto& input : def.inputs)
			{
				if (input.first == curSet)
				{
					std::vector<DescriptorSetBinding>& setBindingCollection = input.second;

					std::sort(setBindingCollection.begin(), setBindingCollection.end(), [](const DescriptorSetBinding& lhs, const DescriptorSetBinding& rhs)
					{
						return lhs.binding < rhs.binding;
					});

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



		std::map<uint32_t, VkDescriptorSetLayout> uniformLayoutMap;
		std::vector<VkDescriptorSetLayout> uniformLayouts;
		//each key is a set

		curSet = 0;
		uint32_t bindingsLeft = uniformSetBindings.size();

		while (bindingsLeft)
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo = vkh::descriptorSetLayoutCreateInfo(nullptr, 0);

			if (uniformSetBindings.find(curSet) != uniformSetBindings.end())
			{
				std::vector<VkDescriptorSetLayoutBinding>& setBindings = uniformSetBindings[curSet];

				layoutInfo = vkh::descriptorSetLayoutCreateInfo(setBindings.data(), static_cast<uint32_t>(setBindings.size()));
				bindingsLeft--;
			}

			uniformLayoutMap[curSet] = {};
			res = vkCreateDescriptorSetLayout(GContext.device, &layoutInfo, nullptr, &uniformLayoutMap[curSet]);

			uniformLayouts.push_back(uniformLayoutMap[curSet]);
			checkf(res == VK_SUCCESS, "Error creating descriptor set layout for material");

			curSet++;
		}


		outMaterial.layoutCount = static_cast<uint32_t>(uniformLayouts.size());

		outMaterial.descriptorSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * outMaterial.layoutCount);
		memcpy(outMaterial.descriptorSetLayouts, uniformLayouts.data(), sizeof(VkDescriptorSetLayout) * outMaterial.layoutCount);

		outMaterial.descSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * outMaterial.layoutCount);
		outMaterial.numDescSets = outMaterial.layoutCount;

		///////////////////////////////////////////////////////////////////////////////
		//allocate descriptor sets
		///////////////////////////////////////////////////////////////////////////////

		for (uint32_t j = 0; j < uniformLayouts.size(); ++j)
		{
			VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&outMaterial.descriptorSetLayouts[j], 1, GContext.descriptorPool);
			res = vkAllocateDescriptorSets(GContext.device, &allocInfo, &outMaterial.descSets[j]);
			checkf(res == VK_SUCCESS, "Error allocating descriptor set");
		}

		///////////////////////////////////////////////////////////////////////////////
		//set up pipeline layout
		///////////////////////////////////////////////////////////////////////////////

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkh::pipelineLayoutCreateInfo(outMaterial.descriptorSetLayouts, outMaterial.layoutCount);

		if (def.pcBlock.sizeBytes > 0)
		{
			VkPushConstantRange pushConstantRange = {};
			pushConstantRange.offset = 0;
			pushConstantRange.size = def.pcBlock.sizeBytes;
			pushConstantRange.stageFlags = shaderStageVectorToVkEnum(def.pcBlock.owningStages);

			outAsset.rData->pushConstantStages = pushConstantRange.stageFlags;

			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
			pipelineLayoutInfo.pushConstantRangeCount = 1;

			outAsset.rData->pushConstantLayout = (uint32_t*)malloc(sizeof(uint32_t) * def.pcBlock.blockMembers.size() * 2);
			outAsset.rData->pushConstantSize = def.pcBlock.sizeBytes;

			checkf(def.pcBlock.sizeBytes < 128, "Push constant block is too large in material");

			outAsset.rData->pushConstantData = (char*)malloc(Material::getRenderData().pushConstantSize);

			for (uint32_t i = 0; i < def.pcBlock.blockMembers.size(); ++i)
			{
				BlockMember& mem = def.pcBlock.blockMembers[i];

				outAsset.rData->pushConstantLayout[i * 2] = hash(&mem.name[0]);
				outAsset.rData->pushConstantLayout[i * 2 + 1] = mem.offset;
			}
		}



		res = vkCreatePipelineLayout(GContext.device, &pipelineLayoutInfo, nullptr, &outMaterial.pipelineLayout);
		assert(res == VK_SUCCESS);

		///////////////////////////////////////////////////////////////////////////////
		//set up graphics pipeline
		///////////////////////////////////////////////////////////////////////////////
		{
			VkVertexInputBindingDescription bindingDescription = vkh::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);

			const VertexRenderData* vertexLayout = Mesh::vertexRenderData();

			VkPipelineVertexInputStateCreateInfo vertexInputInfo = vkh::pipelineVertexInputStateCreateInfo();
			vertexInputInfo.vertexBindingDescriptionCount = 1; 		//todo - what would be the reason for multiple binding?
			vertexInputInfo.vertexAttributeDescriptionCount = vertexLayout->attrCount;
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = &vertexLayout->attrDescriptions[0];

			VkPipelineInputAssemblyStateCreateInfo inputAssembly = vkh::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
			VkViewport viewport = vkh::viewport(0, 0, static_cast<float>(GContext.swapChain.extent.width), static_cast<float>(GContext.swapChain.extent.height));
			VkRect2D scissor = vkh::rect2D(0, 0, GContext.swapChain.extent.width, GContext.swapChain.extent.height);
			VkPipelineViewportStateCreateInfo viewportState = vkh::pipelineViewportStateCreateInfo(&viewport, 1, &scissor, 1);

			VkPipelineRasterizationStateCreateInfo rasterizer = vkh::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
			VkPipelineMultisampleStateCreateInfo multisampling = vkh::pipelineMultisampleStateCreateInfo();

			VkPipelineColorBlendAttachmentState colorBlendAttachment = vkh::pipelineColorBlendAttachmentState(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
			VkPipelineColorBlendStateCreateInfo colorBlending = vkh::pipelineColorBlendStateCreateInfo(colorBlendAttachment);

			def.depthTest = false;
			def.depthWrite = false;
			VkPipelineDepthStencilStateCreateInfo depthStencil = vkh::pipelineDepthStencilStateCreateInfo(
				def.depthTest ? VK_TRUE : VK_FALSE,
				def.depthWrite ? VK_TRUE : VK_FALSE,
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
		//initialize buffers
		///////////////////////////////////////////////////////////////////////////////

		//now initialize all the buffers we need
		size_t uboAlignment = GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;
		std::vector<VkWriteDescriptorSet> descSetWrites;


		//in order to deal with potential gaps in set numbers, we need to create 
		remainingInputs = def.inputs.size();
		//matStorage.mat.rData.unif = (uint32_t*)malloc(sizeof(uint32_t) * def.pcBlock.blockMembers.size() * 2);

		uint32_t curDescSet = 0;
		inputOffset = 0;

		std::vector<VkDescriptorBufferInfo> uniformBufferInfos;
		std::vector<VkDescriptorImageInfo> imageInfos;
		uint32_t lastSize = 0;
		uint32_t lastSet = 0;

		curSet = 0;
		outAsset.rData->staticBuffers = (VkBuffer*)malloc(sizeof(VkBuffer) * def.inputCount);
		//matStorage.mat.rData.staticMems = (VkDeviceMemory*)malloc(sizeof(VkDeviceMemory) * def.inputCount);


		//create the buffers needed for uniform data, but don't allocate any mem yet, since we're going to
		//allocate the mem for all of them in one chunk
		uint32_t staticUniformSize = 0;
		uint32_t curBuffer = 0;

		VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		std::vector<char*> staticData;
		std::vector<uint32_t> staticSizes;
		for (auto& input : def.inputs)
		{
			std::vector<DescriptorSetBinding>& descSets = input.second;
			for (uint32_t i = 0; i < descSets.size(); ++i)
			{
				DescriptorSetBinding& inputDef = descSets[i];
				size_t dynamicAlignment = (inputDef.sizeBytes / uboAlignment) * uboAlignment + ((inputDef.sizeBytes % uboAlignment) > 0 ? uboAlignment : 0);
				if (inputDef.type == InputType::UNIFORM)
				{
					vkh::createBuffer(outAsset.rData->staticBuffers[curBuffer++],
						dynamicAlignment,
						VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						memFlags);


					char* data = (char*)malloc(dynamicAlignment);

					for (uint32_t k = 0; k < inputDef.blockMembers.size(); ++k)
					{
						memcpy(&data[0] + inputDef.blockMembers[k].offset, inputDef.blockMembers[k].defaultValue, inputDef.blockMembers[k].size);
					}
					staticData.push_back(data);
					staticSizes.push_back(dynamicAlignment);

					staticUniformSize += dynamicAlignment;
				}
			}
		}

		if (staticUniformSize)
		{
			//coalesce all static data into single char* for easy mapping
			char* allStaticData = (char*)malloc(staticUniformSize);

			{
				uint32_t offset = 0;
				for (uint32_t d = 0; d < staticData.size(); ++d)
				{
					memcpy(&allStaticData[offset], staticData[d], staticSizes[d]);
					offset += staticSizes[d];
					free(staticData[d]);
				}
			}

			//and allocate a single VkDeviceMem that will hold all the data
			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(GContext.device, outAsset.rData->staticBuffers[0], &memRequirements);

			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = staticUniformSize;
			allocInfo.memoryTypeIndex = vkh::getMemoryType(GContext.gpu.device, memRequirements.memoryTypeBits, memFlags);

			res = vkAllocateMemory(GContext.device, &allocInfo, nullptr, &outAsset.rData->staticUniformMem);

			uint32_t bufferOffset = 0;
			for (uint32_t bufferIdx = 0; bufferIdx < staticSizes.size(); ++bufferIdx)
			{
				vkBindBufferMemory(GContext.device, outAsset.rData->staticBuffers[bufferIdx], outAsset.rData->staticUniformMem, bufferOffset);
				bufferOffset += staticSizes[bufferIdx];
			}


			//now map the default data into this memory
			{

				VkBuffer stagingBuffer;
				VkDeviceMemory stagingMemory;

				vkh::createBuffer(stagingBuffer,
					stagingMemory,
					staticUniformSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);



				void* mappedStagingBuffer;
				vkMapMemory(GContext.device, stagingMemory, 0, staticUniformSize, 0, &mappedStagingBuffer);

				memset(mappedStagingBuffer, 0, staticUniformSize);
				memcpy(mappedStagingBuffer, allStaticData, staticUniformSize);

				vkh::VkhCommandBuffer scratch = vkh::beginScratchCommandBuffer(vkh::ECommandPoolType::Transfer);
				uint32_t mapOffset = 0;
				for (uint32_t bufferIdx = 0; bufferIdx < staticSizes.size(); ++bufferIdx)
				{
					vkh::copyBuffer(stagingBuffer, outAsset.rData->staticBuffers[bufferIdx], staticSizes[bufferIdx], mapOffset, 0, scratch);
					mapOffset += staticSizes[bufferIdx];
				}

				vkh::submitScratchCommandBuffer(scratch);

				free(allStaticData);
				vkUnmapMemory(GContext.device, stagingMemory);

				vkDestroyBuffer(GContext.device, stagingBuffer, nullptr);
				vkFreeMemory(GContext.device, stagingMemory, nullptr);
			}
		}


		std::vector<VkDescriptorBufferInfo*> bufferPtrs;
		bufferPtrs.reserve(staticSizes.size());
		uniformBufferInfos.reserve(staticSizes.size());
		curBuffer = 0;
		lastSize = 0;
		//we only need to update descriptors that actually exist, and aren't empties
		for (auto& input : def.inputs)
		{
			std::vector<DescriptorSetBinding>& descSets = input.second;
			for (uint32_t i = 0; i < descSets.size(); ++i)
			{
				DescriptorSetBinding& inputDef = descSets[i];
				VkDescriptorImageInfo* imageInfoPtr = nullptr;

				if (inputDef.type == InputType::UNIFORM)
				{
					VkDescriptorBufferInfo uniformBufferInfo;
					uniformBufferInfo.buffer = outAsset.rData->staticBuffers[bufferPtrs.size()];
					uniformBufferInfo.offset = 0;
					uniformBufferInfo.range = staticSizes[bufferPtrs.size()];

					lastSize += uniformBufferInfo.range;

					uniformBufferInfos.push_back(uniformBufferInfo);
					bufferPtrs.push_back(&uniformBufferInfos[uniformBufferInfos.size() - 1]);

					curBuffer++;
				}
				else if (inputDef.type == InputType::SAMPLER)
				{
					VkDescriptorImageInfo imageInfo = {};

					uint32_t tex = Texture::make(inputDef.defaultValue);

					TextureRenderData* texData = Texture::getRenderData(tex);
					imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageInfo.imageView = texData->view;
					imageInfo.sampler = texData->sampler;

					imageInfos.push_back(imageInfo);
					imageInfoPtr = &imageInfos[imageInfos.size() - 1];
				}

				VkWriteDescriptorSet descriptorWrite = {};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = outMaterial.descSets[input.first];
				descriptorWrite.dstBinding = inputDef.binding; //refers to binding in shader
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = inputTypeEnumToVkEnum(inputDef.type);
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pBufferInfo = inputDef.type == InputType::SAMPLER ? 0 : bufferPtrs[curBuffer - 1];
				descriptorWrite.pImageInfo = inputDef.type == InputType::UNIFORM ? 0 : imageInfoPtr; // Optional
				descriptorWrite.pTexelBufferView = nullptr; // Optional
				descSetWrites.push_back(descriptorWrite);
			}
		}

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
}