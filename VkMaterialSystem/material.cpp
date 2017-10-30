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

	VkShaderStageFlagBits shaderStageEnumToVkEnum(ShaderStage stage)
	{
		if (stage == ShaderStage::VERTEX) return VK_SHADER_STAGE_VERTEX_BIT;
		if (stage == ShaderStage::FRAGMENT) return VK_SHADER_STAGE_FRAGMENT_BIT;

		assert(0);
		return VK_SHADER_STAGE_ALL;
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
		for (uint32_t i = 0; i < def.numShaderStages; ++i)
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


		for (uint32_t i = 0; i < def.numShaderStages; ++i)
		{
			ShaderStageDefinition& stageDef = def.stages[i];

			for (uint32_t j = 0; j < stageDef.numInputs; ++j)
			{
				ShaderInput& inputDef = def.inputs[inputOffset + j];
				VkDescriptorSetLayoutBinding layoutBinding = vkh::descriptorSetLayoutBinding(inputTypeEnumToVkEnum(inputDef.type), shaderStageEnumToVkEnum(stageDef.stage), inputDef.binding, 1);

				uniformSetBindings[inputDef.set].push_back(layoutBinding);
			}

			inputOffset += stageDef.numInputs;
		}

		std::vector<VkDescriptorSetLayout> uniformLayouts;
		std::vector<VkDescriptorType> uniformTypes;
		std::vector<VkDescriptorSetLayoutBinding> inOrderBindings;

		uniformLayouts.resize(uniformSetBindings.size());
		uniformTypes.resize(uniformSetBindings.size());

		//each key is a set
		for (auto& setPair : uniformSetBindings)
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo = vkh::descriptorSetLayoutCreateInfo(setPair.second.data(), static_cast<uint32_t>(setPair.second.size()));

			res = vkCreateDescriptorSetLayout(GContext.device, &layoutInfo, nullptr, &uniformLayouts[setPair.first]);
			uniformTypes[setPair.first] = setPair.second[0].descriptorType;

			for (uint32_t layoutBinding = 0; layoutBinding < setPair.second.size(); ++layoutBinding)
			{
				inOrderBindings.push_back(setPair.second[layoutBinding]);
			}

			checkf(res == VK_SUCCESS, "Error creating descriptor set layout for material");
		}

		uint32_t numDecsriptorSets = static_cast<uint32_t>(uniformLayouts.size());

		outMaterial.descriptorSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * numDecsriptorSets);
		memcpy(outMaterial.descriptorSetLayouts, uniformLayouts.data(), sizeof(VkDescriptorSetLayout) * numDecsriptorSets);

		outMaterial.layoutCount = numDecsriptorSets;

		outMaterial.descSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * numDecsriptorSets);
		outMaterial.numDescSets = numDecsriptorSets;

		///////////////////////////////////////////////////////////////////////////////
		//allocate descriptor sets
		///////////////////////////////////////////////////////////////////////////////

		for (uint32_t j = 0; j < uniformLayouts.size(); ++j)
		{
			VkDescriptorPool* targetPool;
			if (uniformTypes[j] == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				targetPool = &GContext.uniformBufferDescPool;
			}
			else
			{
				targetPool = &GContext.samplerDescPool;
			}

			VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&outMaterial.descriptorSetLayouts[j], 1, *targetPool);

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
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
			pipelineLayoutInfo.pushConstantRangeCount = 1;

			matStorage.mat.rData.pushConstantLayout = (uint32_t*)malloc(sizeof(uint32_t) * def.pcBlock.numBlockMembers * 2);
			matStorage.mat.rData.pushConstantSize = def.pcBlock.sizeBytes;

			checkf(def.pcBlock.sizeBytes < 128, "Push constant block is too large in material");

			matStorage.mat.rData.pushConstantData = (char*)malloc(Material::getRenderData().pushConstantSize);

			for (uint32_t i = 0; i < def.pcBlock.numBlockMembers; ++i)
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


		for (uint32_t i = 0; i < shaderStages.size(); ++i)
		{
			vkDestroyShaderModule(GContext.device, shaderStages[i].module, nullptr);

		}


		//now initialize all the buffers we need
		size_t uboAlignment = GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;



		std::vector<VkWriteDescriptorSet> descSetWrites;

		uint32_t curDescSet = 0;
		inputOffset = 0;

		std::vector<VkDescriptorBufferInfo> uniformBufferInfos;
		std::vector<VkDescriptorImageInfo> imageInfos;
		uint32_t lastSize = 0;
		uint32_t lastSet = 0;

		for (uint32_t i = 0; i < def.numInputs; ++i)
		{

			ShaderInput& inputDef = def.inputs[i + inputOffset];
			size_t dynamicAlignment = (inputDef.sizeBytes / uboAlignment) * uboAlignment + ((inputDef.sizeBytes % uboAlignment) > 0 ? uboAlignment : 0);

			VkDescriptorBufferInfo* bufferPtr = nullptr;
			VkDescriptorImageInfo* imageInfoPtr = nullptr;

			if (inputDef.type == InputType::UNIFORM)
			{

				VkDescriptorBufferInfo uniformBufferInfo;

				//for now, only allocate 1
				vkh::createBuffer(matStorage.mat.rData.staticBuffer,
					matStorage.mat.rData.staticMem,
					dynamicAlignment,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					GContext.gpu.device,
					GContext.device);

				uniformBufferInfo.buffer = matStorage.mat.rData.staticBuffer;
				uniformBufferInfo.offset = lastSet == inputDef.set ? lastSize : 0;
				uniformBufferInfo.range = dynamicAlignment;

				lastSize += dynamicAlignment;
				if (lastSet != inputDef.set)
				{
					lastSize = 0;
				}
				lastSet = inputDef.set;

				uniformBufferInfos.push_back(uniformBufferInfo);
				bufferPtr = &uniformBufferInfos[uniformBufferInfos.size() - 1];

				void* mappedStagingBuffer;

				vkMapMemory(GContext.device, matStorage.mat.rData.staticMem, 0, dynamicAlignment, 0, &mappedStagingBuffer);

				char* defaultData = (char*)malloc(inputDef.sizeBytes);

				memset(defaultData, 0, inputDef.sizeBytes);

				for (uint32_t k = 0; k < inputDef.numBlockMembers; ++k)
				{
					memcpy(&defaultData[0] + inputDef.blockMembers[k].offset, inputDef.blockMembers[k].defaultValue, inputDef.blockMembers[k].size);
				}


				memcpy(mappedStagingBuffer, defaultData, inputDef.sizeBytes);

				free(defaultData);
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
			descriptorWrite.dstSet = outMaterial.descSets[curDescSet++];
			descriptorWrite.dstBinding = inputDef.binding; //refers to binding in shader
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = inputTypeEnumToVkEnum(inputDef.type);
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = bufferPtr;
			descriptorWrite.pImageInfo = imageInfoPtr; // Optional
			descriptorWrite.pTexelBufferView = nullptr; // Optional
			descSetWrites.push_back(descriptorWrite);
		}


		//it's kinda weird that the order of desc writes has to be the order of sets. 
		vkUpdateDescriptorSets(GContext.device, descSetWrites.size(), descSetWrites.data(), 0, nullptr);
	}

	void setPushConstantVector(const char* var, glm::vec4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint32_t numVars = static_cast<uint32_t>(rData.pushConstantSize / (sizeof(uint32_t) * 2));

		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < numVars; i+=2)
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

	MaterialRenderData getRenderData()
	{
		return matStorage.mat.rData;
	}

	void destroy()
	{
		assert(0); //unimeplemented
	}

}