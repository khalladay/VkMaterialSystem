#include "stdafx.h"

#include "material.h"
#include "asset_rdata_types.h"
#include "hash.h"
#include "vkh.h"
#include <vector>
#include "texture.h"
#include <map>
#include "material_creation.h"

struct MaterialStorage
{
	//std::vector<MaterialAsset> data;
	std::map<uint32_t, MaterialAsset> data;
	MaterialAsset mat;
};

MaterialStorage matStorage;


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
	vkh::Allocation globalMem;
	VkBuffer globalBuffer;
	GlobalShaderData globalShaderData;
	void* mappedMemory;
	uint32_t globalSize;

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

	uint32_t charArrayToMaterialName(const char* name)
	{
		return hash(name);
	}

	MaterialInstance make(const char* materialPath)
	{
		uint32_t newId = reserve(materialPath);
		return make(newId, load(materialPath));
	}

	MaterialInstance makeInstance(uint32_t parentId)
	{
		return{ parentId,0,0,0 };
	}

	uint32_t reserve(const char* reserveName)
	{
		uint32_t hashedName = charArrayToMaterialName(reserveName);
		
		//if there is any collision, explode because we can't handle that - and maybe should switch to 64 bit hashing? 
		while (matStorage.data.count(hashedName) > 0)
		{
			checkf(0, "hash collision with material name");
		}

		matStorage.data[hashedName] = {};
		return hashedName;
	}

	void setPushConstantData(uint32_t matId, const char* var, void* data, uint32_t size)
	{
		MaterialRenderData& rData = Material::getRenderData(matId);

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
	void setTexture(MaterialInstance inst, const char* var, uint32_t texId)
	{
		MaterialRenderData& rData = Material::getRenderData(inst.parent);

		uint32_t varHash = hash(var);
		for (uint32_t i = 0; i < rData.numDynamicUniforms * MATERIAL_UNIFORM_LAYOUT_STRIDE; i += MATERIAL_UNIFORM_LAYOUT_STRIDE)
		{
			if (rData.dynamicUniformLayout[i] == varHash)
			{
				TextureRenderData* texData = Texture::getRenderData(texId);
				uint32_t index = rData.dynamicUniformLayout[i + TEXTUREVIEWPTR_INDEX_IDX];
				uint32_t setWriteIdx = rData.dynamicUniformLayout[i + DESCSET_WRITE_IDX];

				VkWriteDescriptorSet& setWrite = rData.instPages[inst.page].descSetWrites[setWriteIdx * inst.index]; 
				VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = texData->view; 
				imageInfo.sampler = texData->sampler;

				setWrite.pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(vkh::GContext.device, 1, &setWrite, 0, nullptr);
			}
		}
	}
	/*
	void setUniformData(uint32_t matId, const char* name, void* data)
	{
		MaterialRenderData& rData = Material::getRenderData(matId);
		uint32_t varHash = hash(name);

		for (uint32_t i = 0; i < rData.numDynamicUniforms * 4; i += 4)
		{
			if (rData.dynamicUniformLayout[i] == varHash)
			{
				VkBuffer& targetBuffer = rData.dynamic.buffers[rData.dynamicUniformLayout[i + BUFFER_INDEX_IDX]];
				uint32_t size = rData.dynamicUniformLayout[i + MEMBER_SIZE_IDX];
				uint32_t offset = rData.dynamicUniformLayout[i + MEMBER_OFFSET_IDX];

				vkh::VkhCommandBuffer scratch = vkh::beginScratchCommandBuffer(vkh::ECommandPoolType::Transfer);
				vkCmdUpdateBuffer(scratch.buffer, targetBuffer, offset, size, data);
				vkh::submitScratchCommandBuffer(scratch);
				break;
			}
		}
	}*/

	void setPushConstantVector(uint32_t matId, const char* var, glm::vec4& data)
	{
		setPushConstantData(matId, var, &data, sizeof(glm::vec4));
	}

	void setPushConstantMatrix(uint32_t matId, const char* var, glm::mat4& data)
	{
		setPushConstantData(matId, var, &data, sizeof(glm::mat4));
	}

	void setPushConstantFloat(uint32_t matId, const char* var, float data)
	{
		setPushConstantData(matId, var, &data, sizeof(float));
	}

	/*void setUniformVector4(uint32_t matId, const char* name, glm::vec4& data)
	{
		setUniformData(matId, name, &data);
	}

	void setUniformVector2(uint32_t matId, const char* name, glm::vec2& data)
	{
		setUniformData(matId, name, &data);
	}

	void setUniformFloat(uint32_t matId, const char* name, float data)
	{
		setUniformData(matId, name, &data);
	}

	void setUniformMatrix(uint32_t matId, const char* name, glm::mat4& data)
	{
		setUniformData(matId, name, &data);
	}*/

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
		return *matStorage.data[matId].rData;
	}

	void destroy()
	{
		assert(0); //unimeplemented
	}

	MaterialAsset& getMaterialAsset(uint32_t matId)
	{
		return matStorage.data[matId];
	}

}