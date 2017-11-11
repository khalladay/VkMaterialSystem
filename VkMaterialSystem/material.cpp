#include "material.h"
#include "asset_rdata_types.h"
#include "hash.h"

struct MaterialStorage
{
	MaterialAsset mat;
};

MaterialStorage matStorage;

//
namespace Material
{
	void setPushConstantVector(const char* var, glm::vec4& data)
	{
		MaterialRenderData& rData = Material::getRenderData();
		uint32_t numVars = static_cast<uint32_t>(rData.pushConstantSize / (sizeof(uint32_t) * 2));

		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < numVars; i += 2)
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
		uint32_t varHash = hash(var);

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
		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < numVars; i += 2)
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
