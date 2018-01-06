#pragma once
#include "stdafx.h"

struct MaterialRenderData;

struct MaterialInstance
{
	uint32_t parent;
	uint32_t page;
	uint8_t index;
	uint8_t generation;
};



struct MaterialAsset
{
	uint32_t set;
	uint32_t binding;
	uint32_t sizeBytes;

	uint32_t numDescSetBindings;

	MaterialRenderData* rData;
};

//right now, all the calls that take a material Id argument do a lookup into
//a map to get the material. This is obviously terrible. You probably want to 
//store materials in some sort of array, and return the index of the array 
//using a lookup or storing it as part of the material ID, so that you 
//can just access the element directly in all these calls
namespace Material
{
	MaterialRenderData& getRenderData(uint32_t matId);
	MaterialAsset& getMaterialAsset(uint32_t matId);

	void initGlobalShaderData();
	MaterialInstance make(const char* assetPath);
	MaterialInstance makeInstance(uint32_t parentId);

	//used to create an empty material in material storage, 
	//only needed if you're creating a material in a way other than
	//loading the definition file from a path (as above)
	uint32_t reserve(const char* reserveName);
	uint32_t charArrayToMaterialName(const char* name);

	void setPushConstantVector(uint32_t matId, const char* name, glm::vec4& data);
	void setPushConstantFloat(uint32_t matId, const char* name, float data);
	void setPushConstantMatrix(uint32_t matId, const char* name, glm::mat4& data);

	//void setUniformVector4(uint32_t matId, const char* name, glm::vec4& data);
	//void setUniformVector2(uint32_t matId, const char* name, glm::vec2& data);
	//void setUniformFloat(uint32_t matId, const char* name, float data);
	//void setUniformMatrix(uint32_t matId, const char* name, glm::mat4& data);

	//void setTexture(uint32_t matId, const char* name, uint32_t texId);

	void setGlobalFloat(const char* name, float data);
	void setGlobalVector4(const char* name, glm::vec4& data);
	void setGlobalVector2(const char* name, glm::vec2& data);

	void destroy();
}