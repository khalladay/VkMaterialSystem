#include "material.h"
#include "mesh.h"
#include "asset_rdata_types.h"
#include "vkh_initializers.h"
#include "vkh.h"
#include <vector>
#include "hash.h"
#include "texture.h"
#include <map>

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

	//	std::vector<VkDescriptorSetLayoutBinding> bindings;

		std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> uniformSetBindings;
		std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> samplerSetBindings;


		for (uint32_t i = 0; i < def.numShaderStages; ++i)
		{
			ShaderStageDefinition& stageDef = def.stages[i];

			if (stageDef.numUniformBlocks > 0)
			{
				for (uint32_t j = 0; j < stageDef.numUniformBlocks; ++j)
				{
					OpaqueBlockDefinition& blockDef = stageDef.uniformBlocks[j];
					VkDescriptorSetLayoutBinding layoutBinding = {};
					layoutBinding.binding = blockDef.binding;
					layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					layoutBinding.descriptorCount = 1;
					layoutBinding.stageFlags = shaderStageEnumToVkEnum(stageDef.stage);
					layoutBinding.pImmutableSamplers = nullptr; // Optional

					uniformSetBindings[blockDef.set].push_back(layoutBinding);
				}
			}
			if (stageDef.numSamplers > 0)
			{

				for (uint32_t j = 0; j < stageDef.numSamplers; ++j)
				{
					SamplerDefinition& sampDef = stageDef.samplers[j];

					VkDescriptorSetLayoutBinding layoutBinding = {};
					layoutBinding.binding = sampDef.binding;
					layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					layoutBinding.pImmutableSamplers = nullptr;
					layoutBinding.descriptorCount = 1;
					layoutBinding.stageFlags = shaderStageEnumToVkEnum(stageDef.stage);

					samplerSetBindings[sampDef.set].push_back(layoutBinding);

				}
			}
		}

		std::vector<VkDescriptorSetLayout> uniformLayouts;
		uniformLayouts.resize(uniformSetBindings.size());

		std::vector<VkDescriptorSetLayout> samplerLayouts;
		samplerLayouts.resize(samplerSetBindings.size());


		//each key is a set
		for (auto& setPair : uniformSetBindings) 
		{
			
			VkDescriptorSetLayoutCreateInfo layoutInfo = vkh::descriptorSetLayoutCreateInfo(setPair.second.data(), static_cast<uint32_t>(setPair.second.size()));
			
			res = vkCreateDescriptorSetLayout(GContext.device, &layoutInfo, nullptr, &uniformLayouts[setPair.first]);
			assert(res == VK_SUCCESS);

		}

		for (auto& setPair : samplerSetBindings)
		{

			VkDescriptorSetLayoutCreateInfo layoutInfo = vkh::descriptorSetLayoutCreateInfo(setPair.second.data(), static_cast<uint32_t>(setPair.second.size()));

			res = vkCreateDescriptorSetLayout(GContext.device, &layoutInfo, nullptr, &samplerLayouts[0]);
			assert(res == VK_SUCCESS);

		}
		uint32_t numDecsriptorSets = static_cast<uint32_t>(uniformLayouts.size() + samplerLayouts.size());

		outMaterial.descriptorSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * numDecsriptorSets);
		outMaterial.layoutCount = numDecsriptorSets;

		memcpy(outMaterial.descriptorSetLayouts, uniformLayouts.data(), sizeof(VkDescriptorSetLayout) * uniformLayouts.size());
		memcpy(&outMaterial.descriptorSetLayouts[uniformLayouts.size()], samplerLayouts.data(), sizeof(VkDescriptorSetLayout) * samplerLayouts.size());

		outMaterial.descSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * numDecsriptorSets);
		outMaterial.numDescSets = numDecsriptorSets;

		///////////////////////////////////////////////////////////////////////////////
		//allocate descriptor sets
		///////////////////////////////////////////////////////////////////////////////

		uint32_t descSetOffset = 0;

		if (uniformLayouts.size() > 0)
		{
			VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(outMaterial.descriptorSetLayouts, static_cast<uint32_t>(uniformLayouts.size()), GContext.uniformBufferDescPool);

			res = vkAllocateDescriptorSets(GContext.device, &allocInfo, &outMaterial.descSets[descSetOffset]);
			descSetOffset += static_cast<uint32_t>(uniformLayouts.size());
			assert(res == VK_SUCCESS);
		}
		if (samplerLayouts.size() > 0)
		{
			VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&outMaterial.descriptorSetLayouts[uniformLayouts.size()], static_cast<uint32_t>(samplerLayouts.size()), GContext.samplerDescPool);

			res = vkAllocateDescriptorSets(GContext.device, &allocInfo, &outMaterial.descSets[descSetOffset]);
			descSetOffset += static_cast<uint32_t>(samplerLayouts.size());

			assert(res == VK_SUCCESS);

		}
		///////////////////////////////////////////////////////////////////////////////
		//set up pipeline layout
		///////////////////////////////////////////////////////////////////////////////

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkh::pipelineLayoutCreateInfo(outMaterial.descriptorSetLayouts, outMaterial.layoutCount);		

		if (def.pcBlock.size > 0)
		{
			VkPushConstantRange pushConstantRange = {};
			pushConstantRange.offset = 0;
			pushConstantRange.size = def.pcBlock.size;
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
			pipelineLayoutInfo.pushConstantRangeCount = 1;

			matStorage.mat.rData.pushConstantLayout = (uint32_t*)malloc(sizeof(uint32_t) * def.pcBlock.num * 2);
			matStorage.mat.rData.pushConstantSize = def.pcBlock.size;
			
			assert(def.pcBlock.size < 128);

			matStorage.mat.rData.pushConstantData = (char*)malloc(Material::getRenderData().pushConstantSize);

			for (uint32_t i = 0; i < def.pcBlock.num; ++i)
			{
				BlockMember& mem = def.pcBlock.blockMembers[i];

				matStorage.mat.rData.pushConstantLayout[i * 2] = HASH(&mem.name[0]);
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
		for (uint32_t i = 0; i < def.numShaderStages; ++i)
		{
			ShaderStageDefinition& stageDef = def.stages[i];

			if (stageDef.numUniformBlocks > 0)
			{
				std::vector<VkBuffer> staticBuffers;

				size_t lastSize = 0;
				for (uint32_t j = 0; j < stageDef.numUniformBlocks; ++j)
				{
					OpaqueBlockDefinition& blockDef = stageDef.uniformBlocks[j];
					size_t dynamicAlignment = (blockDef.size / uboAlignment) * uboAlignment + ((blockDef.size % uboAlignment) > 0 ? uboAlignment : 0);


					//for now, only allocate 1
					vkh::createBuffer(matStorage.mat.rData.staticBuffer,
						matStorage.mat.rData.staticMem,
						dynamicAlignment,
						VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						GContext.gpu.device,
						GContext.device);

					VkDescriptorBufferInfo bufferInfo = {};
					bufferInfo.buffer = matStorage.mat.rData.staticBuffer;
					bufferInfo.offset = lastSize;
					bufferInfo.range = dynamicAlignment;
					lastSize = dynamicAlignment;


					void* mappedStagingBuffer;

					vkMapMemory(GContext.device, matStorage.mat.rData.staticMem, 0, dynamicAlignment, 0, &mappedStagingBuffer);

					char* defaultData = (char*)malloc(blockDef.size);
	
					memset(defaultData, 0, blockDef.size);

					for (uint32_t k = 0; k < blockDef.num; ++k)
					{
						memcpy(&defaultData[0] + blockDef.blockMembers[k].offset, blockDef.blockMembers[k].defaultValue, blockDef.blockMembers[k].size);
					}


					memcpy(mappedStagingBuffer, defaultData, blockDef.size);
					free(defaultData);


					VkWriteDescriptorSet descriptorWrite = {};
					descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					descriptorWrite.dstSet = outMaterial.descSets[curDescSet++];
					descriptorWrite.dstBinding = blockDef.binding; //refers to binding in shader
					descriptorWrite.dstArrayElement = 0;
					descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					descriptorWrite.descriptorCount = 1;
					descriptorWrite.pBufferInfo = &bufferInfo;
					descriptorWrite.pImageInfo = nullptr; // Optional
					descriptorWrite.pTexelBufferView = nullptr; // Optional
					descSetWrites.push_back(descriptorWrite);

				}
			}


		}
		for (uint32_t i = 0; i < def.numShaderStages; ++i)
		{
			ShaderStageDefinition& stageDef = def.stages[i];

			if (stageDef.numSamplers > 0)
			{
				for (uint32_t j = 0; j < stageDef.numSamplers; ++j)
				{
					SamplerDefinition& sampDef = stageDef.samplers[j];

					TextureRenderData* tex = Texture::getRenderData();

					VkDescriptorImageInfo imageInfo = {};
					imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageInfo.imageView = tex->view;
					imageInfo.sampler = tex->sampler;

					//imageInfo.imageView = textureImageView;
					//	imageInfo.sampler = textureSampler;


					VkWriteDescriptorSet descriptorWrite = {};
					descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					descriptorWrite.dstSet = outMaterial.descSets[curDescSet++];
					descriptorWrite.dstBinding = sampDef.binding; //refers to binding in shader
					descriptorWrite.dstArrayElement = 0;
					descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					descriptorWrite.descriptorCount = 1;
					descriptorWrite.pBufferInfo = nullptr;
					descriptorWrite.pImageInfo = &imageInfo; // Optional
					descriptorWrite.pTexelBufferView = nullptr; // Optional
					descSetWrites.push_back(descriptorWrite);
				}

			}
		}

		vkUpdateDescriptorSets(GContext.device, descSetWrites.size(), descSetWrites.data(), 0, nullptr);





	}

	void setPushConstantVector(const char* var, glm::vec4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint32_t numVars = static_cast<uint32_t>(rData.pushConstantSize / (sizeof(uint32_t) * 2));

		uint32_t varHash = HASH(var);

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
		uint32_t varHash = HASH(var);

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
		uint32_t varHash = HASH(var);

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