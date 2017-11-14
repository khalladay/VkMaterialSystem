#include "stdafx.h"

#include "material.h"
#include "asset_rdata_types.h"
#include "hash.h"
#include "vkh.h"

struct MaterialStorage
{
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
	VkDeviceMemory globalMem;
	VkBuffer globalBuffer;
	GlobalShaderData globalShaderData;
	void* mappedMemory;
	uint32_t globalSize;

	void initGlobalShaderData()
	{
		static bool isInitialized = false;
		if (!isInitialized)
		{
			uint32_t structSize = sizeof(GlobalShaderData);
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

			vkMapMemory(vkh::GContext.device, globalMem, 0, globalSize,0, &mappedMemory);
			isInitialized = true;
		}
	}

	void setPushConstantVector(const char* var, glm::vec4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();

		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < rData.pushConstantCount*2; i += 2)
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
		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < rData.pushConstantCount*2; i += 2)
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
		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < rData.pushConstantCount*2; i += 2)
		{
			if (rData.pushConstantLayout[i] == varHash)
			{
				memcpy(rData.pushConstantData + rData.pushConstantLayout[i + 1], &data, sizeof(float));
				break;
			}
		}
	}

	void setUniformVector4(const char* name, glm::vec4& data)
	{

	}

	void setUniformVector2(const char* name, glm::vec2& data)
	{

	}

	void setUniformFloat(const char* name, float data)
	{

	}

	void setUniformMatrix(const char* name, glm::mat4& data)
	{

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


	MaterialRenderData getRenderData()
	{
		return *matStorage.mat.rData;
	}

	void destroy()
	{
		assert(0); //unimeplemented
	}

	//temporary - only support 1 material until our import system is bullet proof
	MaterialAsset& getMaterialAsset()
	{
		return matStorage.mat;
	}

}
