#include "material.h"
#include "mesh.h"
#include "asset_rdata_types.h"
#include "vkh_initializers.h"
#include "vkh.h"
#include <vector>

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

		std::vector<VkDescriptorSetLayoutBinding> bindings;


		VkDescriptorSetLayoutCreateInfo layoutInfo = vkh::descriptorSetLayoutCreateInfo(0, static_cast<uint32_t>(bindings.size()));

		res = vkCreateDescriptorSetLayout(GContext.device, &layoutInfo, nullptr, &outMaterial.descriptorSetLayout);
		assert(res == VK_SUCCESS);


		///////////////////////////////////////////////////////////////////////////////
		//allocate descriptor sets
		///////////////////////////////////////////////////////////////////////////////

		VkDescriptorSetLayout layouts[] = { outMaterial.descriptorSetLayout };

		VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(layouts, 1, GContext.uniformBufferDescPool);

		res = vkAllocateDescriptorSets(GContext.device, &allocInfo, &outMaterial.descSet);
		assert(res == VK_SUCCESS);

		///////////////////////////////////////////////////////////////////////////////
		//set up pipeline layout
		///////////////////////////////////////////////////////////////////////////////

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkh::pipelineLayoutCreateInfo(&outMaterial.descriptorSetLayout, 1);		

		if (def.pcBlock.size > 0)
		{
			VkPushConstantRange pushConstantRange = {};
			pushConstantRange.offset = 0;
			pushConstantRange.size = def.pcBlock.size;
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
			pipelineLayoutInfo.pushConstantRangeCount = 1;

			matStorage.mat.rData.pushConstantLayout = (uint8_t*)malloc(sizeof(uint8_t) * def.pcBlock.num * 2);
			matStorage.mat.rData.pushConstantSize = def.pcBlock.size;
			matStorage.mat.rData.pushConstantData = (char*)malloc(Material::getRenderData().pushConstantSize);

			for (uint32_t i = 0; i < def.pcBlock.num; ++i)
			{
				BlockMember& mem = def.pcBlock.blockMembers[i];
				
				if (strcmp(&mem.name[0], "col") == 0)
				{
					matStorage.mat.rData.pushConstantLayout[i * 2] = static_cast<uint8_t>(PushConstant::Col);
				}
				if (strcmp(&mem.name[0], "time") == 0)
				{
					matStorage.mat.rData.pushConstantLayout[i * 2] = static_cast<uint8_t>(PushConstant::Time);
				}

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

		VkPipelineDepthStencilStateCreateInfo depthStencil = vkh::pipelineDepthStencilStateCreateInfo(
			def.depthTest ? VK_TRUE : VK_FALSE,
			def.depthWrite ? VK_TRUE : VK_FALSE, 
			VK_COMPARE_OP_LESS);

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = shaderStages.size();
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
	}

	void setPushConstantVector(PushConstant var, glm::vec4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint8_t numVars = rData.pushConstantSize / (sizeof(uint8_t) * 2);

		for (uint32_t i = 0; i < numVars; i+=2)
		{
			if (rData.pushConstantLayout[i] == (uint8_t)var)
			{
				memcpy(rData.pushConstantData + rData.pushConstantLayout[i + 1], &data, sizeof(glm::vec4));
				break;
			}
		}
	}


	void setPushConstantMatrix(PushConstant var, glm::mat4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint8_t numVars = rData.pushConstantSize / (sizeof(uint8_t) * 2);

		for (uint32_t i = 0; i < numVars; i += 2)
		{
			if (rData.pushConstantLayout[i] == (uint8_t)var)
			{
				memcpy(rData.pushConstantData + rData.pushConstantLayout[i + 1], &data, sizeof(glm::mat4));
				break;
			}
		}
	}

	void setPushConstantFloat(PushConstant var, float data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint8_t numVars = rData.pushConstantSize / (sizeof(uint8_t) * 2);

		for (uint32_t i = 0; i < numVars; i += 2)
		{
			if (rData.pushConstantLayout[i] == (uint8_t)var)
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