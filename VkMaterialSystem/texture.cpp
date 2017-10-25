#include "texture.h"
#include "asset_rdata_types.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb\stb_image.h>

struct TextureAsset
{
	TextureRenderData rData;
	uint32_t width;
	uint32_t height;
	uint32_t numChannels;
};

struct TextureStorage
{
	TextureAsset tex;
};

TextureStorage texStorage;


namespace Texture
{
	TextureRenderData* getRenderData()
	{
		return &texStorage.tex.rData;
	}

	void make(const char* filepath)
	{
		TextureAsset t;

		int texWidth, texHeight, texChannels;

		//STBI_rgb_alpha forces an alpha even if the image doesn't have one
		stbi_uc* pixels = stbi_load(filepath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		checkf(pixels, "Could not load image");

		VkDeviceSize imageSize = texWidth * texHeight * 4;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		vkh::createBuffer(stagingBuffer,
			stagingBufferMemory,
			imageSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* data;
		vkMapMemory(vkh::GContext.device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(vkh::GContext.device, stagingBufferMemory);

		stbi_image_free(pixels);

		t.width = texWidth;
		t.height = texHeight;
		t.numChannels = texChannels;
		t.rData.format = VK_FORMAT_R8G8B8A8_UNORM;

		//VK image format must match buffer
		vkh::createImage(t.rData.vulkanTexture.image,
			t.width, t.height,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

		vkh::allocBindImageToMem(t.rData.vulkanTexture.deviceMemory,
			t.rData.vulkanTexture.image,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		vkh::transitionImageLayout(t.rData.vulkanTexture.image, t.rData.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkh::copyBufferToImage(stagingBuffer, t.rData.vulkanTexture.image, t.width, t.height);

		vkh::transitionImageLayout(t.rData.vulkanTexture.image, t.rData.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkh::createImageView(t.rData.view, 1, t.rData.vulkanTexture.image, t.rData.format);
		vkh::createTexSampler(t.rData.sampler);

		vkDestroyBuffer(vkh::GContext.device, stagingBuffer, nullptr);
		vkFreeMemory(vkh::GContext.device, stagingBufferMemory, nullptr);

		texStorage.tex = t;

	}


	void destroy()
	{

	}

}