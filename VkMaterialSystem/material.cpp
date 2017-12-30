#include "stdafx.h"

#include "material.h"
#include "asset_rdata_types.h"
#include "hash.h"
#include "vkh.h"
#include <vector>
#include "texture.h"
#include <map>
#include "material_creation.h"

struct GlobalShaderData
{
	__declspec(align(16)) glm::float32 time;
	__declspec(align(16)) glm::vec4 mouse;
	__declspec(align(16)) glm::vec2 resolution;
	__declspec(align(16)) glm::mat4 viewMatrix;
	__declspec(align(16)) glm::vec4 worldSpaceCameraPos;
};
//
namespace Material
{
	struct MaterialStorage
	{
		//std::vector<MaterialAsset> data;
		std::map<uint32_t, Asset> baseMaterials;

		Asset mat;
	};

	MaterialStorage matStorage;

	vkh::Allocation globalMem;
	VkBuffer globalBuffer;
	GlobalShaderData globalShaderData;
	void* mappedMemory;
	uint32_t globalSize;

	uint32_t charArrayToMaterialName(const char* name)
	{
		return hash(name);
	}

	void initGlobalShaderData()
	{
		static bool isInitialized = false;
		if (!isInitialized)
		{
			uint32_t structSize = static_cast<uint32_t>(sizeof(GlobalShaderData));
			size_t uboAlignment = vkh::GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;
			size_t s = sizeof(glm::float32);
			size_t s2 = sizeof(glm::vec4);
			size_t s3 = sizeof(glm::mat4);

			float* t = (&globalShaderData.time);
			glm::vec4* mp = (&globalShaderData.mouse);
			glm::vec2* res = (&globalShaderData.resolution);

			globalSize = (structSize / uboAlignment) * uboAlignment + ((structSize % uboAlignment) > 0 ? uboAlignment : 0);
			
			vkh::createBuffer(globalBuffer, 
				globalMem,
				globalSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			vkMapMemory(vkh::GContext.device, globalMem.handle, globalMem.offset, globalSize, 0, &mappedMemory);
			isInitialized = true;
		}
	}

	Instance make(const char* materialPath)
	{
		uint32_t baseMaterial = charArrayToMaterialName(materialPath);
		
		if (matStorage.baseMaterials.count(baseMaterial) > 0)
		{
			checkf(0, "material already exists, or hash collision");
		}

		
		baseMaterial = reserve(materialPath);
		return make(baseMaterial, load(materialPath));
	}

	Instance make(Instance baseInstance)
	{
		return{ 0,0,0,0 };
	}

	uint32_t reserve(const char* reserveName)
	{
		uint32_t hashedName = hash(reserveName);
		
		//if there is any collision, increment the hashed value until we find an empty slot
		while (matStorage.baseMaterials.count(hashedName) > 0)
		{
			hashedName++;
		}

		matStorage.baseMaterials[hashedName] = {};
		return hashedName;
	}

	void setPushConstantData(Instance instance, const char* var, void* data, uint32_t size)
	{
		MaterialRenderData& rData = Material::getRenderData(instance.parent);

		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < rData.pushConstantLayout.memberCount * 2; i += 2)
		{
			if (rData.pushConstantLayout.layout[i] == varHash)
			{
				uint32_t offset = rData.pushConstantLayout.layout[i + 1];
				memcpy(rData.pushConstantData + rData.pushConstantLayout.layout[i + 1], data, size);
				break;
			}
		}
	}

	//note: this cannot be done from within a command buffer
	void setTexture(Instance instance, const char* var, uint32_t texId)
	{
		MaterialRenderData& rData = Material::getRenderData(instance.parent);

		uint32_t varHash = hash(var);
		for (uint32_t i = 0; i < rData.numDynamicInputs * 4; i += 4)
		{
			if (rData.dynamicLayout[i] == varHash)
			{
				TextureRenderData* texData = Texture::getRenderData(texId);
				uint32_t index = rData.dynamicLayout[i + 1];
				uint32_t setWriteIdx = rData.dynamicLayout[i + 2];

				VkWriteDescriptorSet& setWrite = rData.descSetWrites[setWriteIdx];
				VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = texData->view; 
				imageInfo.sampler = texData->sampler;

				setWrite.pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(vkh::GContext.device, 1, &setWrite, 0, nullptr);
			}
		}
	}

	void setUniformData(Instance instance, uint32_t name, void* data)
	{
		MaterialRenderData& rData = Material::getRenderData(instance.parent);
		uint32_t varHash = name;

		for (uint32_t i = 0; i < rData.numDynamicInputs * 4; i += 4)
		{
			if (rData.dynamicLayout[i] == varHash)
			{
				VkBuffer& targetBuffer = rData.instPages[instance.page].staticBuffer;
				uint32_t size = rData.dynamicLayout[i + 2];
				uint32_t offset = rData.dynamicUniformMemSize * instance.index + rData.dynamicLayout[i + 1] + rData.dynamicLayout[i + 3];

				vkh::VkhCommandBuffer scratch = vkh::beginScratchCommandBuffer(vkh::ECommandPoolType::Transfer);
				vkCmdUpdateBuffer(scratch.buffer, targetBuffer, offset, size, data);
				vkh::submitScratchCommandBuffer(scratch);
				break;
			}
		}

	}
	
	void setUniformData(Instance instance, const char* name, void* data)
	{
		uint32_t varHash = hash(name);
		setUniformData(instance, varHash, data);
	}

	void setPushConstantVector(Instance instance, const char* var, glm::vec4& data)
	{
		setPushConstantData(instance, var, &data, sizeof(glm::vec4));
	}

	void setPushConstantMatrix(Instance instance, const char* var, glm::mat4& data)
	{
		setPushConstantData(instance, var, &data, sizeof(glm::mat4));
	}

	void setPushConstantFloat(Instance instance, const char* var, float data)
	{
		setPushConstantData(instance, var, &data, sizeof(float));
	}

	void setUniformVector4(Instance instance, const char* name, glm::vec4& data)
	{
		setUniformData(instance, name, &data);
	}

	void setUniformVector2(Instance instance, const char* name, glm::vec2& data)
	{
		setUniformData(instance, name, &data);
	}

	void setUniformFloat(Instance instance, const char* name, float data)
	{
		setUniformData(instance, name, &data);
	}

	void setUniformMatrix(Instance instance, const char* name, glm::mat4& data)
	{
		setUniformData(instance, name, &data);
	}

	void setGlobalFloat(const char* name, float data)
	{
		initGlobalShaderData();
		globalShaderData.time = data;
		memcpy(mappedMemory, &globalShaderData, globalSize);
	}

	void setGlobalVector4(const char* name, glm::vec4& data)
	{
		initGlobalShaderData();
		globalShaderData.mouse = data;
		memcpy(mappedMemory, &globalShaderData, globalSize);
	}

	void setGlobalVector2(const char* name, glm::vec2& data)
	{
		initGlobalShaderData();
		globalShaderData.resolution = data;
		memcpy(mappedMemory, &globalShaderData, globalSize);
	}


	MaterialRenderData& getRenderData(uint32_t matId)
	{
		return *matStorage.baseMaterials[matId].rData;
	}

	void destroy()
	{
		assert(0); //unimeplemented
	}

	Asset& getMaterialAsset(uint32_t matId)
	{
		return matStorage.baseMaterials[matId];
	}

	void destroy(uint32_t asset)
	{
		
	}

	void destroy(Instance instance)
	{
		destroyInstance(instance);
	}

}