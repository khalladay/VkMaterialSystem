#include "material.h"
#include "mesh.h"
#include "asset_rdata_types.h"
#include "vkh_initializers.h"
#include "vkh.h"
#include <vector>
#include "hash.h"
#include "texture.h"
#include <map>
#include <algorithm>
struct MaterialAsset
{
	MaterialRenderData rData;
};

struct MaterialStorage
{
	MaterialAsset mat;
};

MaterialStorage matStorage;

namespace Material
{
	VkShaderStageFlagBits shaderStageEnumToVkEnum(ShaderStage stage);
	VkShaderStageFlags shaderStageVectorToVkEnum(std::vector<ShaderStage>& vec);
	VkDescriptorType inputTypeEnumToVkEnum(InputType type);

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

	void make(MaterialDefinition def)
	{
		using vkh::GContext;

		vkh::VkhMaterial& outMaterial = matStorage.mat.rData.vkMat;

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

			matStorage.mat.rData.pushConstantStages = pushConstantRange.stageFlags;

			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
			pipelineLayoutInfo.pushConstantRangeCount = 1;

			matStorage.mat.rData.pushConstantLayout = (uint32_t*)malloc(sizeof(uint32_t) * def.pcBlock.blockMembers.size() * 2);
			matStorage.mat.rData.pushConstantSize = def.pcBlock.sizeBytes;

			checkf(def.pcBlock.sizeBytes < 128, "Push constant block is too large in material");

			matStorage.mat.rData.pushConstantData = (char*)malloc(Material::getRenderData().pushConstantSize);

			for (uint32_t i = 0; i < def.pcBlock.blockMembers.size(); ++i)
			{
				BlockMember& mem = def.pcBlock.blockMembers[i];

				matStorage.mat.rData.pushConstantLayout[i * 2] = hash(&mem.name[0]);
				matStorage.mat.rData.pushConstantLayout[i * 2 + 1] = mem.offset;
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
		matStorage.mat.rData.staticBuffers = (VkBuffer*)malloc(sizeof(VkBuffer) * def.inputCount);
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
					vkh::createBuffer(matStorage.mat.rData.staticBuffers[curBuffer++],
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
			vkGetBufferMemoryRequirements(GContext.device, matStorage.mat.rData.staticBuffers[0], &memRequirements);

			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = staticUniformSize;
			allocInfo.memoryTypeIndex = vkh::getMemoryType(GContext.gpu.device, memRequirements.memoryTypeBits, memFlags);

			res = vkAllocateMemory(GContext.device, &allocInfo, nullptr, &matStorage.mat.rData.staticUniformMem);

			uint32_t bufferOffset = 0;
			for (uint32_t bufferIdx = 0; bufferIdx < staticSizes.size(); ++bufferIdx)
			{
				vkBindBufferMemory(GContext.device, matStorage.mat.rData.staticBuffers[bufferIdx], matStorage.mat.rData.staticUniformMem, bufferOffset);
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
					vkh::copyBuffer(stagingBuffer, matStorage.mat.rData.staticBuffers[bufferIdx], staticSizes[bufferIdx], mapOffset, 0, scratch);
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
					uniformBufferInfo.buffer = matStorage.mat.rData.staticBuffers[bufferPtrs.size()];
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

	void setPushConstantVector(const char* var, glm::vec4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint32_t numVars = static_cast<uint32_t>(rData.pushConstantSize / (sizeof(uint32_t) * 2));

		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < numVars; i += 2)
		{
			if (rData.pushConstantLayout[i] == varHash)
			{
				memcpy(rData.pushConstantData + rData.pushConstantLayout[i + 1], &data, sizeof(glm::vec4));
				break;
			}
		}
	}


	void setPushConstantMatrix(const char* var, glm::mat4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint32_t numVars = static_cast<uint32_t>(rData.pushConstantSize / (sizeof(uint32_t) * 2));
		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < numVars; i += 2)
		{
			if (rData.pushConstantLayout[i] == varHash)
			{
				memcpy(rData.pushConstantData + rData.pushConstantLayout[i + 1], &data, sizeof(glm::mat4));
				break;
			}
		}
	}

	void setPushConstantFloat(const char* var, float data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint32_t numVars = static_cast<uint32_t>(rData.pushConstantSize / (sizeof(uint32_t) * 2));
		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < numVars; i += 2)
		{
			if (rData.pushConstantLayout[i] == varHash)
			{
				memcpy(rData.pushConstantData + rData.pushConstantLayout[i + 1], &data, sizeof(float));
				break;
			}
		}
	}

	void setUniformVector4(const char* name, glm::vec4& data)
	{

	}

	void setUniformVector2(const char* name, glm::vec2& data)
	{

	}

	void setUniformFloat(const char* name, float data)
	{

	}

	void setUniformMatrix(const char* name, glm::mat4& data)
	{

	}


	MaterialRenderData getRenderData()
	{
		return matStorage.mat.rData;
	}

	void destroy()
	{
		assert(0); //unimeplemented
	}

}