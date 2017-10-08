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
	void make(MaterialDefinition def)
	{
		using vkh::GContext;
		
		vkh::VkhMaterial& outMaterial = matStorage.mat.rData.vkMat;

		VkResult res;

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = vkh::shaderPipelineStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT);
		vkh::createShaderModule(vertShaderStageInfo.module, def.vShaderPath, GContext.device);

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = vkh::shaderPipelineStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT);
		vkh::createShaderModule(fragShaderStageInfo.module, def.fShaderPath, GContext.device);

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
	
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
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
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

		vkDestroyShaderModule(GContext.device, vertShaderStageInfo.module, nullptr);
		vkDestroyShaderModule(GContext.device, fragShaderStageInfo.module, nullptr);
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