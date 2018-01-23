#include "stdafx.h"
#include "vkh.h"
#include "file_utils.h"
#include "vkh_allocator_passthrough.h"
#include "vkh_allocator_pool.h"
namespace vkh
{
	VkhContext GContext;

	//initialization functions, don't need to be publically viewable
	void createWindowsInstance(VkInstance& outInstance, const char* applicationName);
	void createWin32Surface(VkhSurface& outSurface, VkInstance& vkInstance, HINSTANCE win32Instance, HWND wndHdl);
	void getDiscretePhysicalDevice(VkhPhysicalDevice& outDevice, VkInstance& inInstance, const VkhSurface& surface);
	void createLogicalDevice(VkDevice& outDevice, VkhDeviceQueues& outqueues, const VkhPhysicalDevice& physDevice);
	void createSwapchainForSurface(VkhSwapChain& outSwapChain, VkhPhysicalDevice& physDevice, const VkDevice& lDevice, const VkhSurface& surface);
	void createCommandPool(VkCommandPool& outPool, const VkDevice& lDevice, const VkhPhysicalDevice& physDevice, uint32_t queueFamilyIdx);
	uint32_t getMemoryType(const VkPhysicalDevice& device, uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void createImageView(VkImageView& outView, VkFormat imageFormat, VkImageAspectFlags aspectMask, uint32_t mipCount, const VkImage& imageHdl, const VkDevice& device);

	VkDebugReportCallbackEXT callback;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData)
	{
		printf("[VALIDATION LAYER] %s \n", msg);
		return VK_FALSE;
	}

	void createWin32Context(VkhContext& outContext, uint32_t width, uint32_t height, HINSTANCE Instance, HWND wndHdl, const char* applicationName)
	{
		createWindowsInstance(outContext.instance, applicationName);
		createWin32Surface(outContext.surface, outContext.instance, Instance, wndHdl);
		getDiscretePhysicalDevice(outContext.gpu, outContext.instance, outContext.surface);
		createLogicalDevice(outContext.device, outContext.deviceQueues, outContext.gpu);
		
		vkh::allocators::pool::activate(&outContext);
		
		createSwapchainForSurface(outContext.swapChain, outContext.gpu, outContext.device, outContext.surface);
		createCommandPool(outContext.gfxCommandPool, outContext.device, outContext.gpu, outContext.gpu.graphicsQueueFamilyIdx);
		createCommandPool(outContext.transferCommandPool, outContext.device, outContext.gpu, outContext.gpu.transferQueueFamilyIdx);
		createCommandPool(outContext.presentCommandPool, outContext.device, outContext.gpu, outContext.gpu.presentQueueFamilyIdx);

		//since we're only creating one of these for each material, this means we'll support 128 materials

		std::vector<VkDescriptorType> types;
		types.reserve(3);
		types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
		types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		types.push_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		types.push_back(VK_DESCRIPTOR_TYPE_SAMPLER);

		std::vector<uint32_t> counts;
		counts.reserve(3);
		counts.push_back(128);
		counts.push_back(128);
		counts.push_back(128);
		counts.push_back(128);
		counts.push_back(128);

		createDescriptorPool(outContext.descriptorPool, outContext.device, types, counts);

		createVkSemaphore(outContext.imageAvailableSemaphore, outContext.device);
		createVkSemaphore(outContext.renderFinishedSemaphore, outContext.device);

		outContext.frameFences.resize(outContext.swapChain.imageViews.size());
		for (uint32_t i = 0; i < outContext.frameFences.size(); ++i)
		{
			createFence(outContext.frameFences[i], outContext.device);
		}
	}

	void waitForFence(VkFence& fence, const VkDevice& device)
	{
		if (fence)
		{
			if (vkGetFenceStatus(device, fence) == VK_SUCCESS)
			{
				vkWaitForFences(device, 1, &fence, true, 0);
			}
		}
	}

	void createFence(VkFence& outFence, VkDevice& device)
	{
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.pNext = NULL;
		fenceInfo.flags = 0;
		VkResult vk_res = vkCreateFence(device, &fenceInfo, NULL, &outFence);
		assert(vk_res == VK_SUCCESS);
	}

	void createWindowsInstance(VkInstance& outInstance, const char* applicationName)
	{
		VkApplicationInfo app_info;

		//stype and pnext are the same usage as below
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = NULL;
		app_info.pApplicationName = applicationName;
		app_info.applicationVersion = 1;
		app_info.pEngineName = applicationName;
		app_info.engineVersion = 1;
		app_info.apiVersion = VK_API_VERSION_1_0;

		std::vector<const char*> validationLayers;
		std::vector<bool> layersAvailable;

#if _DEBUG
		printf("Starting up with validation layers enabled: \n");

		const char* layerNames = "VK_LAYER_LUNARG_standard_validation";

		validationLayers.push_back(layerNames);
		printf("Looking for: %s\n", layerNames);

		layersAvailable.push_back(false);

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers;
		availableLayers.resize(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (uint32_t i = 0; i < validationLayers.size(); ++i)
		{
			for (uint32_t j = 0; j < availableLayers.size(); ++j)
			{
				if (strcmp(validationLayers[i], availableLayers[j].layerName) == 0)
				{
					printf("Found layer: %s\n", validationLayers[i]);
					layersAvailable[i] = true;
				}
			}
		}

		bool foundAllLayers = true;
		for (uint32_t i = 0; i < layersAvailable.size(); ++i)
		{
			foundAllLayers &= layersAvailable[i];
		}

		checkf(foundAllLayers, "Not all required validation layers were found.");
#endif

		//Get available extensions:
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions;
		extensions.resize(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::vector<const char*> requiredExtensions;
		std::vector<bool> extensionsPresent;

		requiredExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		extensionsPresent.push_back(false);

		requiredExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		extensionsPresent.push_back(false);

#if _DEBUG
		requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		extensionsPresent.push_back(false);
#endif
		

		printf("Available Vulkan Extensions: \n");

		for (uint32_t i = 0; i < extensions.size(); ++i)
		{
			auto& prop = extensions[i];
			printf("* %s ", prop.extensionName);
			bool found = false;
			for (uint32_t i = 0; i < requiredExtensions.size(); i++)
			{
				if (strcmp(prop.extensionName, requiredExtensions[i]) == 0)
				{
					printf(" - Enabled\n");
					found = true;
					extensionsPresent[i] = true;
				}
			}

			if (!found) printf("\n");
		}

		bool allExtensionsFound = true;
		for (uint32_t i = 0; i < extensionsPresent.size(); i++)
		{
			allExtensionsFound &= extensionsPresent[i];
		}

		checkf(allExtensionsFound, "Failed to find all required vulkan extensions");

		//create instance with all extensions

		VkInstanceCreateInfo inst_info;

		//useful for driver validation and when passing as void*
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

		//used to pass extension info when stype is extension defined
		inst_info.pNext = NULL;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		inst_info.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		inst_info.ppEnabledExtensionNames = requiredExtensions.data(); 

		//validation layers / other layers
		inst_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		inst_info.ppEnabledLayerNames = validationLayers.data();

		VkResult res;
		res = vkCreateInstance(&inst_info, NULL, &outInstance);

		checkf(res != VK_ERROR_INCOMPATIBLE_DRIVER, "Cannot create VkInstance - driver incompatible");
		checkf(res == VK_SUCCESS, "Cannot create VkInstance - unknown error");

#if _DEBUG
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;

		PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
		CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(outInstance, "vkCreateDebugReportCallbackEXT");
		CreateDebugReportCallback(outInstance, &createInfo, NULL, &callback);
#endif
	}

	void createWin32Surface(VkhSurface& outSurface, VkInstance& vkInstance, HINSTANCE win32Instance, HWND wndHdl)
	{
		VkWin32SurfaceCreateInfoKHR createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = wndHdl;
		createInfo.hinstance = win32Instance;
		createInfo.pNext = NULL;
		createInfo.flags = 0;
		VkResult res = vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, &outSurface.surface);
		assert(res == VK_SUCCESS);
	}

	void getDiscretePhysicalDevice(VkhPhysicalDevice& outDevice, VkInstance& inInstance, const VkhSurface& surface)
	{
		uint32_t gpu_count;
		VkResult res = vkEnumeratePhysicalDevices(inInstance, &gpu_count, NULL);

		// Allocate space and get the list of devices.
		std::vector<VkPhysicalDevice> gpus;
		gpus.resize(gpu_count);

		res = vkEnumeratePhysicalDevices(inInstance, &gpu_count, gpus.data());

		bool found = false;
		int curScore = 0;

		std::vector<const char*> deviceExtensions;
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		for (uint32_t i = 0; i < gpus.size(); ++i)
		{
			const auto& gpu = gpus[i];
		
			auto props = VkPhysicalDeviceProperties();
			vkGetPhysicalDeviceProperties(gpu, &props);

			if (props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				VkPhysicalDeviceProperties deviceProperties;
				VkPhysicalDeviceFeatures deviceFeatures;
				vkGetPhysicalDeviceProperties(gpu, &deviceProperties);
				vkGetPhysicalDeviceFeatures(gpu, &deviceFeatures);

				int score = 1000;
				score += props.limits.maxImageDimension2D;
				score += props.limits.maxFragmentInputComponents;
				score += deviceFeatures.geometryShader ? 1000 : 0;
				score += deviceFeatures.tessellationShader ? 1000 : 0;

				if (!deviceFeatures.shaderSampledImageArrayDynamicIndexing)
				{
					continue;
				}

				//make sure the device supports presenting

				uint32_t extensionCount;
				vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);

				std::vector<VkExtensionProperties> availableExtensions;
				availableExtensions.resize(extensionCount);
				vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, availableExtensions.data());

				uint32_t foundExtensionCount = 0;
				for (uint32_t extIdx = 0; extIdx < availableExtensions.size(); ++extIdx)
				{
					for (uint32_t reqIdx = 0; reqIdx < deviceExtensions.size(); ++reqIdx)
					{
						if (strcmp(availableExtensions[extIdx].extensionName,deviceExtensions[i]) == 0)
						{
							foundExtensionCount++;
						}
					}
				}

				bool supportsAllRequiredExtensions = deviceExtensions.size() == foundExtensionCount;
				if (!supportsAllRequiredExtensions)
				{
					continue;
				}

				//make sure the device supports at least one valid image format for our surface
				VkhSwapChainSupportInfo scSupport;
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface.surface, &scSupport.capabilities);

				uint32_t formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface.surface, &formatCount, nullptr);

				if (formatCount != 0)
				{
					scSupport.formats.resize(formatCount);
					vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface.surface, &formatCount, scSupport.formats.data());
				}
				else
				{
					continue;
				}

				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface.surface, &presentModeCount, nullptr);

				if (presentModeCount != 0)
				{
					scSupport.presentModes.resize(presentModeCount);
					vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface.surface, &presentModeCount, scSupport.presentModes.data());
				}

				bool worksWithSurface = scSupport.formats.size() > 0 && scSupport.presentModes.size() > 0;

				if (score > curScore && supportsAllRequiredExtensions && worksWithSurface)
				{
					found = true;

					outDevice.device = gpu;
					outDevice.swapChainSupport = scSupport;
					curScore = score;
				}
			}
		}

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(outDevice.device, &deviceProperties);
		printf("Selected GPU: %s\n", deviceProperties.deviceName);

		assert(found);
		assert(res == VK_SUCCESS);

		vkGetPhysicalDeviceFeatures(outDevice.device, &outDevice.features);
		vkGetPhysicalDeviceMemoryProperties(outDevice.device, &outDevice.memProps);
		vkGetPhysicalDeviceProperties(outDevice.device, &outDevice.deviceProps);
		printf("Max mem allocations: %i\n", outDevice.deviceProps.limits.maxMemoryAllocationCount);

		//get queue families while we're here
		vkGetPhysicalDeviceQueueFamilyProperties(outDevice.device, &outDevice.queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies;
		queueFamilies.resize(outDevice.queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(outDevice.device, &outDevice.queueFamilyCount, queueFamilies.data());

		std::vector<VkQueueFamilyProperties> queueVec;
		queueVec.resize(outDevice.queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(outDevice.device, &outDevice.queueFamilyCount, &queueVec[0]);


		// Iterate over each queue to learn whether it supports presenting:
		VkBool32 *pSupportsPresent = (VkBool32 *)malloc(outDevice.queueFamilyCount * sizeof(VkBool32));
		for (uint32_t i = 0; i < outDevice.queueFamilyCount; i++)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(outDevice.device, i, surface.surface, &pSupportsPresent[i]);
		}


		outDevice.graphicsQueueFamilyIdx = INVALID_QUEUE_FAMILY_IDX;
		outDevice.transferQueueFamilyIdx = INVALID_QUEUE_FAMILY_IDX;
		outDevice.presentQueueFamilyIdx = INVALID_QUEUE_FAMILY_IDX;

		bool foundGfx = false;
		bool foundTransfer = false;
		bool foundPresent = false;

		for (uint32_t queueIdx = 0; queueIdx < queueFamilies.size(); ++queueIdx)
		{ 
			const auto& queueFamily = queueFamilies[queueIdx];
			const auto& queueFamily2 = queueVec[queueIdx];

			if (!foundGfx && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				outDevice.graphicsQueueFamilyIdx = queueIdx;
				foundGfx = true;
			}

			if (!foundTransfer && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				outDevice.transferQueueFamilyIdx = queueIdx;
				foundTransfer = true;

			}

			if (!foundPresent && queueFamily.queueCount > 0 && pSupportsPresent[queueIdx])
			{
				outDevice.presentQueueFamilyIdx = queueIdx;
				foundPresent = true;

			}

			if (foundGfx && foundTransfer && foundPresent) break;
		}

		assert(foundGfx && foundPresent && foundTransfer);
	}

	void createLogicalDevice(VkDevice& outDevice, VkhDeviceQueues& outqueues, const VkhPhysicalDevice& physDevice)
	{
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::vector<uint32_t> uniqueQueueFamilies;

		uniqueQueueFamilies.push_back(physDevice.graphicsQueueFamilyIdx);
		if (physDevice.transferQueueFamilyIdx != physDevice.graphicsQueueFamilyIdx)
		{
			uniqueQueueFamilies.push_back(physDevice.transferQueueFamilyIdx);
		}

		if (physDevice.presentQueueFamilyIdx != physDevice.graphicsQueueFamilyIdx &&
			physDevice.presentQueueFamilyIdx != physDevice.transferQueueFamilyIdx)
		{
			uniqueQueueFamilies.push_back(physDevice.presentQueueFamilyIdx);
		}

		for (uint32_t familyIdx = 0; familyIdx < uniqueQueueFamilies.size(); ++familyIdx)
		{
			uint32_t queueFamily = uniqueQueueFamilies[familyIdx];
		
			VkDeviceQueueCreateInfo queueCreateInfo = {};

			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;

			float queuePriority[] = { 1.0f };
			queueCreateInfo.pQueuePriorities = queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);

		}

		//we don't need anything fancy right now, but this is where you require things
		// like geo shader support

		std::vector<const char*> deviceExtensions;
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data(); 

		std::vector<const char*> validationLayers;

#ifndef _DEBUG
		const bool enableValidationLayers = false;
#else
		const bool enableValidationLayers = true;
		validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");

		//don't do anything else here because we already know the validation layer is available, 
		//else we would have asserted earlier
#endif

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data(); 
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkResult res = vkCreateDevice(physDevice.device, &createInfo, nullptr, &outDevice);
		assert(res == VK_SUCCESS);

		vkGetDeviceQueue(outDevice, physDevice.graphicsQueueFamilyIdx, 0, &outqueues.graphicsQueue);
		vkGetDeviceQueue(outDevice, physDevice.transferQueueFamilyIdx, 0, &outqueues.transferQueue);
		vkGetDeviceQueue(outDevice, physDevice.presentQueueFamilyIdx, 0, &outqueues.presentQueue);

	}

	void createSwapchainForSurface(VkhSwapChain& outSwapChain, VkhPhysicalDevice& physDevice, const VkDevice& lDevice, const VkhSurface& surface)
	{
		//choose the surface format to use
		VkSurfaceFormatKHR desiredFormat;
		VkPresentModeKHR desiredPresentMode;
		VkExtent2D swapExtent;

		bool foundFormat = false;

		//if there is no preferred format, the formats array only contains VK_FORMAT_UNDEFINED
		if (physDevice.swapChainSupport.formats.size() == 1 && physDevice.swapChainSupport.formats[0].format == VK_FORMAT_UNDEFINED)
		{
			desiredFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			desiredFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
			foundFormat = true;
		}

		//otherwise we can't just choose any format we want, but still let's try to grab one that we know will work for us first
		if (!foundFormat)
		{
			for (uint32_t i = 0; i < physDevice.swapChainSupport.formats.size(); ++i)
			{
				const auto& availableFormat = physDevice.swapChainSupport.formats[i];

				if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					desiredFormat = availableFormat;
				}
			}
		}

		//if our preferred format isn't available, let's just grab the first available because yolo
		if (!foundFormat)
		{
			desiredFormat = physDevice.swapChainSupport.formats[0];
		}

		//present mode - VK_PRESENT_MODE_MAILBOX_KHR is for triple buffering, VK_PRESENT_MODE_FIFO_KHR is double, VK_PRESENT_MODE_IMMEDIATE_KHR is single
		//VK_PRESENT_MODE_FIFO_KHR  is guaranteed to be available.
		//let's prefer triple buffering, and fall back to double if it isn't supported

		desiredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (uint32_t i = 0; i < physDevice.swapChainSupport.presentModes.size(); ++i)
		{ 
			const auto& availablePresentMode = physDevice.swapChainSupport.presentModes[i];

			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				desiredPresentMode = availablePresentMode;
			}
		}

		//update physdevice for new surface size
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice.device, surface.surface, &physDevice.swapChainSupport.capabilities);

		//swap extent is the resolution of the swapchain
		swapExtent = physDevice.swapChainSupport.capabilities.currentExtent;

		//need 1 more than minimum image count for triple buffering
		uint32_t imageCount = physDevice.swapChainSupport.capabilities.minImageCount + 1;
		if (physDevice.swapChainSupport.capabilities.maxImageCount > 0 && imageCount > physDevice.swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = physDevice.swapChainSupport.capabilities.maxImageCount;
		}

		//now that everything is set up, we need to actually create the swap chain
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface.surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = desiredFormat.format;
		createInfo.imageColorSpace = desiredFormat.colorSpace;
		createInfo.imageExtent = swapExtent;
		createInfo.imageArrayLayers = 1; //always 1 unless a stereoscopic app

		 //here, we're rendering directly to the swap chain, but if we were using post processing, this might be VK_IMAGE_USAGE_TRANSFER_DST_BIT 
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


		if (physDevice.graphicsQueueFamilyIdx != physDevice.presentQueueFamilyIdx)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			uint32_t queueFamilyIndices[] = { physDevice.graphicsQueueFamilyIdx, physDevice.presentQueueFamilyIdx };
			createInfo.pQueueFamilyIndices = queueFamilyIndices;

		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional	
		}

		createInfo.preTransform = physDevice.swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = desiredPresentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.pNext = NULL;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		VkResult res = vkCreateSwapchainKHR(lDevice, &createInfo, nullptr, &outSwapChain.swapChain);
		assert(res == VK_SUCCESS);

		//get images for swap chain
		vkGetSwapchainImagesKHR(lDevice, outSwapChain.swapChain, &imageCount, nullptr);
		outSwapChain.imageHandles.resize(imageCount);
		outSwapChain.imageViews.resize(imageCount);

		vkGetSwapchainImagesKHR(lDevice, outSwapChain.swapChain, &imageCount, &outSwapChain.imageHandles[0]);

		outSwapChain.imageFormat = desiredFormat.format;
		outSwapChain.extent = swapExtent;

		//create image views
		for (uint32_t i = 0; i < outSwapChain.imageHandles.size(); i++)
		{
			createImageView(outSwapChain.imageViews[i], outSwapChain.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, outSwapChain.imageHandles[i], lDevice);
		}
	}

	void createImageView(VkImageView& outView, uint32_t mipCount, const VkImage& image, VkFormat format)
	{
		createImageView(outView, format, VK_IMAGE_ASPECT_COLOR_BIT, mipCount, image, GContext.device);
	}

	void createImageView(VkImageView& outView, VkFormat imageFormat, VkImageAspectFlags aspectMask, uint32_t mipCount, const VkImage& imageHdl, const VkDevice& device)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = imageHdl;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = imageFormat;

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = aspectMask;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = mipCount;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;


		VkResult res = vkCreateImageView(device, &createInfo, nullptr, &outView);
		assert(res == VK_SUCCESS);

	}

	void createCommandPool(VkCommandPool& outPool, const VkDevice& lDevice, const VkhPhysicalDevice& physDevice, uint32_t queueFamilyIdx)
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIdx;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional

		VkResult res = vkCreateCommandPool(lDevice, &poolInfo, nullptr, &outPool);
		assert(res == VK_SUCCESS);
	}

	void createVkSemaphore(VkSemaphore& outSemaphore, const VkDevice& device)
	{
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkResult res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &outSemaphore);
		assert(res == VK_SUCCESS);
	}

	void createCommandBuffer(VkCommandBuffer& outBuffer, VkCommandPool& pool, const VkDevice& lDevice)
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = pool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkResult res = vkAllocateCommandBuffers(lDevice, &allocInfo, &outBuffer);
		assert(res == VK_SUCCESS);
	}

	void createDescriptorPool(VkDescriptorPool& outPool, const VkDevice& device, std::vector<VkDescriptorType>& descriptorTypes, std::vector<uint32_t>& maxDescriptors)
	{
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.reserve(descriptorTypes.size());
		uint32_t summedDescCount = 0;

		for (uint32_t i = 0; i < descriptorTypes.size(); ++i)
		{
			VkDescriptorPoolSize poolSize = {};
			poolSize.type = descriptorTypes[i];
			poolSize.descriptorCount = maxDescriptors[i];
			poolSizes.push_back(poolSize);

			summedDescCount += poolSize.descriptorCount;
		}

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = &poolSizes[0];
		poolInfo.maxSets = summedDescCount;

		VkResult res = vkCreateDescriptorPool(device, &poolInfo, nullptr, &outPool);
		assert(res == VK_SUCCESS);
	}

	void createRenderPass(VkRenderPass& outPass, std::vector<VkAttachmentDescription>& colorAttachments, VkAttachmentDescription* depthAttachment, const VkDevice& device)
	{
		std::vector<VkAttachmentReference> attachRefs;

		std::vector<VkAttachmentDescription> allAttachments;
		allAttachments = colorAttachments;

		uint32_t attachIdx = 0;
		while (attachIdx < colorAttachments.size())
		{
			VkAttachmentReference ref = { 0 };
			ref.attachment = attachIdx++;
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachRefs.push_back(ref);
		}

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
		subpass.pColorAttachments = &attachRefs[0];

		VkAttachmentReference depthRef = { 0 };

		if (depthAttachment)
		{
			VkAttachmentReference depthRef = { 0 };
			depthRef.attachment = attachIdx;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			subpass.pDepthStencilAttachment = &depthRef;
			allAttachments.push_back(*depthAttachment);
		}
		

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(allAttachments.size());
		renderPassInfo.pAttachments = &allAttachments[0];
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		//we need a subpass dependency for transitioning the image to the right format, because by default, vulkan
		//will try to do that before we have acquired an image from our fb
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL; //External means outside of the render pipeline, in srcPass, it means before the render pipeline
		dependency.dstSubpass = 0; //must be higher than srcSubpass
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		//add the dependency to the renderpassinfo
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VkResult res = vkCreateRenderPass(device, &renderPassInfo, nullptr, &outPass);
		assert(res == VK_SUCCESS);
	}

	void createFrameBuffers(std::vector<VkFramebuffer>& outBuffers, const VkhSwapChain& swapChain, const VkImageView* depthBufferView, const VkRenderPass& renderPass, const VkDevice& device)
	{
		outBuffers.resize(swapChain.imageViews.size());

		for (uint32_t i = 0; i < outBuffers.size(); i++)
		{
			std::vector<VkImageView> attachments;
			attachments.push_back(swapChain.imageViews[i]);

			if (depthBufferView)
			{
				attachments.push_back(*depthBufferView);
			}

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = &attachments[0];
			framebufferInfo.width = swapChain.extent.width;
			framebufferInfo.height = swapChain.extent.height;
			framebufferInfo.layers = 1;

			VkResult r = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &outBuffers[i]);
			assert(r == VK_SUCCESS);
		}
	}

	void createBuffer(VkBuffer& outBuffer, Allocation& bufferMemory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		createBuffer(outBuffer, bufferMemory, size, usage, properties, GContext.gpu.device, GContext.device);
	}

	void createBuffer(VkBuffer& outBuffer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;

		std::vector<uint32_t> queues;
		queues.reserve(2);

		queues.push_back(GContext.gpu.graphicsQueueFamilyIdx);

		if (GContext.gpu.graphicsQueueFamilyIdx != GContext.gpu.transferQueueFamilyIdx)
		{
			queues.push_back(GContext.gpu.transferQueueFamilyIdx);
			bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		}
		else bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		bufferInfo.pQueueFamilyIndices = &queues[0];
		bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queues.size());

		VkResult res = vkCreateBuffer(GContext.device, &bufferInfo, nullptr, &outBuffer);
		assert(res == VK_SUCCESS);
	}

	void createBuffer(VkBuffer& outBuffer, Allocation& bufferMemory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const VkPhysicalDevice& gpu, const VkDevice& device)
	{
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;

		//concurrent so it can be used by the graphics and transfer queues

		std::vector<uint32_t> queues;
		queues.push_back(GContext.gpu.graphicsQueueFamilyIdx);

		if (GContext.gpu.graphicsQueueFamilyIdx != GContext.gpu.transferQueueFamilyIdx)
		{
			queues.push_back(GContext.gpu.transferQueueFamilyIdx);
			bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		}
		else bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


		bufferInfo.pQueueFamilyIndices = &queues[0];
		bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queues.size());

		VkResult res = vkCreateBuffer(device, &bufferInfo, nullptr, &outBuffer);
		assert(res == VK_SUCCESS);

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, outBuffer, &memRequirements);

		AllocationCreateInfo allocInfo = {};
		allocInfo.size = memRequirements.size;
		allocInfo.memoryTypeIndex = getMemoryType(gpu, memRequirements.memoryTypeBits, properties);
		allocInfo.usage = properties;

		GContext.allocator.alloc(bufferMemory, allocInfo);
		vkBindBufferMemory(device, outBuffer, bufferMemory.handle, bufferMemory.offset);
	}

	void createImage(VkImage& outImage, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
	{
		createImage(outImage, width, height, format, tiling, usage, GContext.device);
	}

	void createImage(VkImage& outImage, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, const VkDevice& device)
	{
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkResult res = vkCreateImage(device, &imageInfo, nullptr, &outImage);
		assert(res == VK_SUCCESS);
	}

	void allocBindImageToMem(Allocation& outMem, const VkImage& image, VkMemoryPropertyFlags properties)
	{
		allocBindImageToMem(outMem, image, properties, GContext.device, GContext.gpu.device);
	}

	void allocBindImageToMem(Allocation& outMem, const VkImage& image, VkMemoryPropertyFlags properties, const VkDevice& device, const VkPhysicalDevice& gpu)
	{
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		AllocationCreateInfo createInfo;
		createInfo.size = memRequirements.size;
		createInfo.memoryTypeIndex = getMemoryType(gpu, memRequirements.memoryTypeBits, properties);
		createInfo.usage = properties;

		allocateDeviceMemory(outMem, createInfo);
		vkBindImageMemory(device, image, outMem.handle, outMem.offset);
	}

	VkFormat depthFormat()
	{
		return VK_FORMAT_D32_SFLOAT;
	}

	void createDepthBuffer(VkhRenderBuffer& outBuffer, uint32_t width, uint32_t height, const VkDevice& device, const VkPhysicalDevice& gpu)
	{
		createImage(outBuffer.handle,
			width,
			height,
			depthFormat(),
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			device);

		allocBindImageToMem(outBuffer.imageMemory, outBuffer.handle, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device, gpu);

		createImageView(outBuffer.view, depthFormat(), VK_IMAGE_ASPECT_DEPTH_BIT, 1, outBuffer.handle, device);
	}

	uint32_t getMemoryType(const VkPhysicalDevice& device, uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

		//The VkPhysicalDeviceMemoryProperties structure has two arraysL memoryTypes and memoryHeaps.
		//Memory heaps are distinct memory resources like dedicated VRAM and swap space in RAM 
		//for when VRAM runs out.The different types of memory exist within these heaps.Right now 
		//we'll only concern ourselves with the type of memory and not the heap it comes from, 
		//but you can imagine that this can affect performance.

		for (uint32_t memoryIndex = 0; memoryIndex < memProperties.memoryTypeCount; memoryIndex++)
		{
			const uint32_t memoryTypeBits = (1 << memoryIndex);
			const bool isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;

			const VkMemoryPropertyFlags properties = memProperties.memoryTypes[memoryIndex].propertyFlags;
			const bool hasRequiredProperties = (properties & requiredProperties) == requiredProperties;

			if (isRequiredMemoryType && hasRequiredProperties)
			{
				return static_cast<int32_t>(memoryIndex);
			}

		}

		assert(0);
		return 0;
	}

	void freeDeviceMemory(Allocation& mem)
	{
		GContext.allocator.free(mem);
	}

	void allocateDeviceMemory(Allocation& outMem, AllocationCreateInfo info)
	{
		GContext.allocator.alloc(outMem, info);
	}

	size_t getUniformBufferAlignment()
	{
		return (size_t)GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;
	}

	void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size, VkhCommandBuffer& buffer)
	{
		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(buffer.buffer, srcBuffer, dstBuffer, 1, &copyRegion);
	}

	void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size, uint32_t srcOffset, uint32_t dstOffset, VkhCommandBuffer& buffer)
	{
		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = srcOffset; // Optional
		copyRegion.dstOffset = dstOffset; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(buffer.buffer, srcBuffer, dstBuffer, 1, &copyRegion);
	}


	void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size)
	{
		VkhCommandBuffer scratch = beginScratchCommandBuffer(ECommandPoolType::Transfer);

		copyBuffer(srcBuffer, dstBuffer, size, scratch);

		submitScratchCommandBuffer(scratch);
	}

	void createShaderModule(VkShaderModule& outModule, const char* binaryData, size_t dataSize, const VkDevice& lDevice)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = dataSize;
		
		//data for vulkan is stored in uint32_t  -  so we have to temporarily copy it to a container that respects that alignment

		std::vector<uint32_t> codeAligned;
		codeAligned.resize((uint32_t)(dataSize / sizeof(uint32_t) + 1));

		memcpy(&codeAligned[0], binaryData, dataSize);
		createInfo.pCode = &codeAligned[0];

		VkResult res = vkCreateShaderModule(lDevice, &createInfo, nullptr, &outModule);
		assert(res == VK_SUCCESS);

	}

	void createShaderModule(VkShaderModule& outModule, const char* filepath, const VkDevice& lDevice)
	{
		BinaryBuffer* data = loadBinaryFile(filepath);
		vkh::createShaderModule(outModule, data->data, data->size, lDevice);
		delete data;

	}

	void createDefaultViewportForSwapChain(VkViewport& outViewport, const VkhSwapChain& swapChain)
	{
		outViewport = {};
		outViewport.x = 0.0f;
		outViewport.y = 0.0f;
		outViewport.width = (float)swapChain.extent.width;
		outViewport.height = (float)swapChain.extent.height;
		outViewport.minDepth = 0.0f;
		outViewport.maxDepth = 1.0f;
	}

	void createOpaqueColorBlendAttachState(VkPipelineColorBlendAttachmentState& outState)
	{
		outState = {};
		outState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		outState.blendEnable = VK_FALSE;
		outState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		outState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		outState.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		outState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		outState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		outState.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
	}

	void createDefaultPipelineRasterizationStateCreateInfo(VkPipelineRasterizationStateCreateInfo& outInfo)
	{
		outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		outInfo.depthClampEnable = VK_FALSE; //if values are outside depth, they are clamped to min/max
		outInfo.rasterizerDiscardEnable = VK_FALSE; //this disables output to the fb
		outInfo.polygonMode = VK_POLYGON_MODE_FILL;
		outInfo.lineWidth = 1.0f;
		outInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		outInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		outInfo.depthBiasEnable = VK_FALSE;
		outInfo.depthBiasConstantFactor = 0.0f; // Optional
		outInfo.depthBiasClamp = 0.0f; // Optional
		outInfo.depthBiasSlopeFactor = 0.0f; // Optional
	}

	void createMultisampleStateCreateInfo(VkPipelineMultisampleStateCreateInfo& outInfo, uint32_t sampleCount)
	{
		if (sampleCount != 1) printf("WARNING: CreateMultiSampleStateCreateInfo cannot support a sample count that isn't 1 right now\n");

		outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		outInfo.sampleShadingEnable = VK_FALSE;
		outInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		outInfo.minSampleShading = 1.0f; // Optional
		outInfo.pSampleMask = nullptr; // Optional
		outInfo.alphaToCoverageEnable = VK_FALSE; // Optional
		outInfo.alphaToOneEnable = VK_FALSE; // Optional


	}

	void createDefaultColorBlendStateCreateInfo(VkPipelineColorBlendStateCreateInfo& outInfo, const VkPipelineColorBlendAttachmentState& blendState)
	{
		outInfo = {};
		outInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		outInfo.logicOpEnable = VK_FALSE; //can be used for bitwise blending
		outInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
		outInfo.attachmentCount = 1;
		outInfo.pAttachments = &blendState;
		outInfo.blendConstants[0] = 0.0f; // Optional
		outInfo.blendConstants[1] = 0.0f; // Optional
		outInfo.blendConstants[2] = 0.0f; // Optional
		outInfo.blendConstants[3] = 0.0f; // Optional

	}

	VkhCommandBuffer beginScratchCommandBuffer(ECommandPoolType type)
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (type == ECommandPoolType::Graphics)
		{
			allocInfo.commandPool = GContext.gfxCommandPool;
		}
		else if (type == ECommandPoolType::Transfer)
		{
			allocInfo.commandPool = GContext.transferCommandPool;
		}
		else
		{
			allocInfo.commandPool = GContext.presentCommandPool;
		}

		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(GContext.device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkhCommandBuffer outBuf;
		outBuf.buffer = commandBuffer;
		outBuf.owningPool = type;

		return outBuf;

	}

	void submitScratchCommandBuffer(VkhCommandBuffer& commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer.buffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer.buffer;

		VkQueue queue;
		VkCommandPool pool;
		if (commandBuffer.owningPool == ECommandPoolType::Graphics)
		{
			queue = GContext.deviceQueues.graphicsQueue;
			pool = GContext.gfxCommandPool;

		}
		else if (commandBuffer.owningPool == ECommandPoolType::Transfer)
		{
			queue = GContext.deviceQueues.transferQueue;
			pool = GContext.transferCommandPool;
		}
		else
		{
			queue = GContext.deviceQueues.presentQueue;
			pool = GContext.presentCommandPool;
		}


		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue);

		vkFreeCommandBuffers(GContext.device, pool, 1, &commandBuffer.buffer);
	}

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkhCommandBuffer commandBuffer = beginScratchCommandBuffer(ECommandPoolType::Graphics);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;

		//these are used to transfer queue ownership, which we aren't doing
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else 
		{
			//Unsupported layout transition
			assert(0);
		}

		//

		vkCmdPipelineBarrier(
			commandBuffer.buffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);


		submitScratchCommandBuffer(commandBuffer);

	}

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkhCommandBuffer commandBuffer = beginScratchCommandBuffer(ECommandPoolType::Transfer);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1};


		vkCmdCopyBufferToImage(
			commandBuffer.buffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		submitScratchCommandBuffer(commandBuffer);


	}


	void createTexSampler(VkSampler& outSampler)
	{
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;

		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		//mostly for pcf? 
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		VkResult res = vkCreateSampler(GContext.device, &samplerInfo, nullptr, &outSampler);
		assert(res == VK_SUCCESS);
	}
}