#pragma once
#include "vkh.h"

namespace vkh
{
	inline VkPipelineShaderStageCreateInfo shaderPipelineStageCreateInfo(VkShaderStageFlagBits stage)
	{
		VkPipelineShaderStageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.stage = stage;
		info.pName = "main";
		return info;
	}

	inline VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount = 1)
	{
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.descriptorCount = descriptorCount;
		layoutBinding.binding = binding;
		layoutBinding.stageFlags = stageFlags;
		layoutBinding.descriptorType = type;
		layoutBinding.pImmutableSamplers = nullptr;
		return layoutBinding;
	}

	inline VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount)
	{
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = bindingCount;
		layoutInfo.pBindings = bindings;
		return layoutInfo;
	}
	
	inline VkDescriptorSetAllocateInfo descriptorSetAllocateInfo(const VkDescriptorSetLayout* layouts, uint32_t layoutCount, VkDescriptorPool pool)
	{
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = layoutCount;
		allocInfo.pSetLayouts = layouts;
		return allocInfo;

	}

	inline VkVertexInputBindingDescription vertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate)
	{
		VkVertexInputBindingDescription vInputBindDescription = {};
		vInputBindDescription.binding = binding;
		vInputBindDescription.stride = stride;
		vInputBindDescription.inputRate = inputRate;
		return vInputBindDescription;
	}

	inline VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo()
	{
		VkPipelineVertexInputStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		return info;
	}

	inline VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnabled)
	{
		VkPipelineInputAssemblyStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		info.topology = topology;
		info.primitiveRestartEnable = primitiveRestartEnabled;
		return info;
	}

	inline VkViewport viewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 0.0f)
	{
		VkViewport vp = {};
		vp.x = x;
		vp.y = y;
		vp.width = width;
		vp.height = height;
		vp.maxDepth = maxDepth;
		vp.minDepth = minDepth;
		return vp;
	}

	inline VkRect2D rect2D(int32_t x, int32_t y, uint32_t w, uint32_t h)
	{
		VkRect2D r = {};
		r.extent.width = w;
		r.extent.height = h;
		r.offset.x = x;
		r.offset.y = y;
		return r;
	}

	inline VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(VkViewport* viewports, uint32_t viewportCount, VkRect2D* scissors, uint32_t scissorCount)
	{
		VkPipelineViewportStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		info.pScissors = scissors;
		info.pViewports = viewports;
		info.scissorCount = scissorCount;
		info.viewportCount = viewportCount;
		return info;
	}

	inline VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, float lineWidth = 1.0f)
	{
		VkPipelineRasterizationStateCreateInfo outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		outInfo.depthClampEnable = VK_FALSE; //if values are outside depth, they are clamped to min/max
		outInfo.rasterizerDiscardEnable = VK_FALSE; //this disables output to the fb
		outInfo.polygonMode = polygonMode;
		outInfo.lineWidth = lineWidth;
		outInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		outInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		outInfo.depthBiasEnable = VK_FALSE;
		outInfo.depthBiasConstantFactor = 0.0f; // Optional
		outInfo.depthBiasClamp = 0.0f; // Optional
		outInfo.depthBiasSlopeFactor = 0.0f; // Optional

		return outInfo;
	}

	inline VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT)
	{
		VkPipelineMultisampleStateCreateInfo outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		outInfo.sampleShadingEnable = VK_FALSE;
		outInfo.rasterizationSamples = samples;
		outInfo.minSampleShading = 1.0f; // Optional
		outInfo.pSampleMask = nullptr; // Optional
		outInfo.alphaToCoverageEnable = VK_FALSE; // Optional
		outInfo.alphaToOneEnable = VK_FALSE; // Optional

		return outInfo;
	}

	inline VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(VkColorComponentFlags colorMask, VkBool32 blendEnabled)
	{
		VkPipelineColorBlendAttachmentState outState = {};
		outState.colorWriteMask = colorMask;
		outState.blendEnable = blendEnabled;
		return outState;
	}

	inline VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState& blendState)
	{
		VkPipelineColorBlendStateCreateInfo outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		outInfo.logicOpEnable = VK_FALSE; //can be used for bitwise blending
		outInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
		outInfo.attachmentCount = 1;
		outInfo.pAttachments = &blendState;
		outInfo.blendConstants[0] = 0.0f; // Optional
		outInfo.blendConstants[1] = 0.0f; // Optional
		outInfo.blendConstants[2] = 0.0f; // Optional
		outInfo.blendConstants[3] = 0.0f; // Optional
		return outInfo;
	}

	inline VkPushConstantRange pushConstantRange(uint32_t offset, uint32_t size, VkShaderStageFlags shaderStageFlags)
	{
		VkPushConstantRange outRange = {};
		outRange.offset = offset;
		outRange.size = size;
		outRange.stageFlags = shaderStageFlags;
		return outRange;
	}

	inline VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(const VkDescriptorSetLayout* layouts, uint32_t layoutCounts)
	{
		VkPipelineLayoutCreateInfo outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		outInfo.setLayoutCount = layoutCounts;
		outInfo.pSetLayouts = layouts;
		return outInfo;
	}

	inline VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp)
	{
		VkPipelineDepthStencilStateCreateInfo outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		outInfo.depthTestEnable = depthTestEnable;
		outInfo.depthWriteEnable = depthWriteEnable;
		outInfo.depthCompareOp = depthCompareOp;
		//	outInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;

		//	outInfo.front = outInfo.back;

		return outInfo;
	}


}