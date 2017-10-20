#pragma once
#include "stdafx.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>
#include <vector>

namespace vkh
{
	const uint32_t INVALID_QUEUE_FAMILY_IDX = -1;

	enum ECommandPoolType
	{
		Graphics,
		Transfer,
		Present
	};

	struct VkhSurface
	{
		VkSurfaceKHR surface;
		VkFormat format;
	};

	struct VkhCommandBuffer
	{
		VkCommandBuffer buffer;
		ECommandPoolType owningPool;
	};

	struct VkhSwapChainSupportInfo
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct VkhSwapChain
	{
		VkSwapchainKHR				swapChain;
		VkFormat					imageFormat;
		VkExtent2D					extent;
		std::vector<VkImage>		imageHandles;
		std::vector<VkImageView>	imageViews;
	};

	struct VkhRenderBuffer
	{
		VkImage			handle;
		VkImageView		view;
		VkDeviceMemory	imageMemory;
	};

	struct VkhLogicalDevice
	{
		VkDevice	device;
		VkQueue		graphicsQueue;
		VkQueue		transferQueue;
		VkQueue		presentQueue;
	};

	struct VkhPhysicalDevice
	{
		VkPhysicalDevice					device;
		VkPhysicalDeviceProperties			deviceProps;
		VkPhysicalDeviceMemoryProperties	memProps;
		VkPhysicalDeviceFeatures			features;
		VkhSwapChainSupportInfo				swapChainSupport;
		uint32_t							queueFamilyCount;
		uint32_t							presentQueueFamilyIdx;
		uint32_t							graphicsQueueFamilyIdx;
		uint32_t							transferQueueFamilyIdx;
	};

	struct VkhDeviceQueues
	{
		VkQueue		graphicsQueue;
		VkQueue		transferQueue;
		VkQueue		presentQueue;
	};

	struct VkhContext
	{
		VkInstance				instance;
		VkhSurface				surface;
		VkhPhysicalDevice		gpu;
		VkDevice				device;
		VkhDeviceQueues			deviceQueues;
		VkhSwapChain			swapChain;
		VkCommandPool			gfxCommandPool;
		VkCommandPool			transferCommandPool;
		VkCommandPool			presentCommandPool;
		std::vector<VkFence>	frameFences;
		VkSemaphore				imageAvailableSemaphore;
		VkSemaphore				renderFinishedSemaphore;

		VkDescriptorPool		uniformBufferDescPool;
		VkDescriptorPool		dynamicUniformBufferDescPool;

		//hate this being here, but if material can create itself
		//this is where it has to live, otherwise rendering has to return
		//a vulkan type from a function and that means adding vkh.h to renderer.h
		VkRenderPass		mainRenderPass;
	};

	extern vkh::VkhContext GContext;

	struct VkhLoadStoreOp
	{
		VkAttachmentLoadOp load;
		VkAttachmentStoreOp store;
	};

	struct VkhUniformBuffer
	{
		VkBuffer* buffer;
		uint32_t* bufferLayout;
		uint32_t numElements;
	};

	struct VkhMaterial
	{
		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;

	//	FixedArray<VkDescriptorSetLayout> descriptorSetLayoutArray;

		uint32_t layoutCount;
		VkDescriptorSetLayout* descriptorSetLayouts;

		//since we're using dynamic uniform buffers, 
		//only the parent material needs a descriptor set 
		VkDescriptorSet* descSets;

	};

	struct VkhMesh
	{
		VkBuffer vBuffer;
		VkBuffer iBuffer;
		VkDeviceMemory vBufferMemory;
		VkDeviceMemory iBufferMemory;

		uint32_t vCount;
		uint32_t iCount;
	};

	struct VkhTexture
	{
		VkImage image;
		VkDeviceMemory deviceMemory;
	};

	VkFormat depthFormat();

	void createWin32Context(VkhContext& outContext, uint32_t width, uint32_t height, HINSTANCE Instance, HWND wndHdl, const char* applicationName);

	void createCommandBuffer(VkCommandBuffer& outBuffers, VkCommandPool& pool, const VkDevice& lDevice);
	void createFrameBuffers(std::vector<VkFramebuffer>& outBuffers, const VkhSwapChain& swapChain, const VkImageView* depthBufferView, const VkRenderPass& renderPass, const VkDevice& device);

	void createBuffer(VkBuffer& outBuffer, VkDeviceMemory& bufferMemory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void createBuffer(VkBuffer& outBuffer, VkDeviceMemory& bufferMemory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const VkPhysicalDevice& gpu, const VkDevice& device);

	void createImage(VkImage& outImage, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);
	void createImage(VkImage& outImage, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, const VkDevice& device);

	void createDepthBuffer(VkhRenderBuffer& outBuffer, uint32_t width, uint32_t height, const VkDevice& device, const VkPhysicalDevice& gpu);
	void createVkSemaphore(VkSemaphore& outSemaphore, const VkDevice& device);

	void createRenderPass(VkRenderPass& outPass, std::vector<VkAttachmentDescription>& colorAttachments, VkAttachmentDescription* depthAttachment, const VkDevice& device);
	void createDescriptorPool(VkDescriptorPool& outPool, const VkDevice& device, VkDescriptorType descriptorType, uint32_t maxDescriptors);
	void createFence(VkFence& outFence, VkDevice& device);

	void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
	void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size, VkhCommandBuffer& buffer);

	void allocateDeviceMemory(VkDeviceMemory& outMem, size_t size, uint32_t memoryType);
	void allocateDeviceMemory(VkDeviceMemory& outMem, size_t size, uint32_t memoryType, const VkDevice& device);

	void allocBindImageToMem(VkDeviceMemory& outMem, const VkImage& image, VkMemoryPropertyFlags properties);
	void allocBindImageToMem(VkDeviceMemory& outMem, const VkImage& image, VkMemoryPropertyFlags properties, const VkDevice& device, const VkPhysicalDevice& gpu);

	void waitForFence(VkFence& fence, const VkDevice& device);

	void createShaderModule(VkShaderModule& outModule, const char* binaryData, size_t dataSize, const VkDevice& lDevice);
	void createShaderModule(VkShaderModule& outModule, const char* filepath, const VkDevice& lDevice);

	void createDefaultViewportForSwapChain(VkViewport& outViewport, const VkhSwapChain& swapChain);
	void createDefaultColorBlendStateCreateInfo(VkPipelineColorBlendStateCreateInfo& outInfo, const VkPipelineColorBlendAttachmentState& blendState);
	void createMultisampleStateCreateInfo(VkPipelineMultisampleStateCreateInfo& outInfo, uint32_t sampleCount);
	void createDefaultPipelineRasterizationStateCreateInfo(VkPipelineRasterizationStateCreateInfo& outInfo);
	void createOpaqueColorBlendAttachState(VkPipelineColorBlendAttachmentState& outState);


	VkhCommandBuffer beginScratchCommandBuffer(ECommandPoolType type);
	void submitScratchCommandBuffer(VkhCommandBuffer& buffer);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	//assumes VkImage is in format _OPTIMZAL
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	void createImageView(VkImageView& outView, uint32_t mipCount, const VkImage& image, VkFormat format);
	void createTexSampler(VkSampler& outSampler);

	size_t getUniformBufferAlignment();
}



