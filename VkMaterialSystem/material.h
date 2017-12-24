#pragma once
#include "stdafx.h"

struct MaterialRenderData;

//right now, all the calls that take a material Id argument do a lookup into
//a map to get the material. This is obviously terrible. You probably want to 
//store materials in some sort of array, and return the index of the array 
//using a lookup or storing it as part of the material ID, so that you 
//can just access the element directly in all these calls
namespace Material
{
	struct Instance
	{
		uint32_t parent;
		uint32_t page;
		uint8_t index;
		uint8_t generation;
	};

	struct Asset
	{
		MaterialRenderData* rData;
	};


	MaterialRenderData& getRenderData(uint32_t matId);
	Asset& getMaterialAsset(uint32_t matId);

	void initGlobalShaderData();
	uint32_t make(const char* assetPath);

	//used to create an empty material in material storage, 
	//only needed if you're creating a material in a way other than
	//loading the definition file from a path (as above)
	uint32_t reserve(const char* reserveName);

	void setPushConstantVector(uint32_t matId, const char* name, glm::vec4& data);
	void setPushConstantFloat(uint32_t matId, const char* name, float data);
	void setPushConstantMatrix(uint32_t matId, const char* name, glm::mat4& data);

	void setUniformVector4(uint32_t matId, const char* name, glm::vec4& data);
	void setUniformVector2(uint32_t matId, const char* name, glm::vec2& data);
	void setUniformFloat(uint32_t matId, const char* name, float data);
	void setUniformMatrix(uint32_t matId, const char* name, glm::mat4& data);

	void setTexture(uint32_t matId, const char* name, uint32_t texId);

	void setGlobalFloat(const char* name, float data);
	void setGlobalVector4(const char* name, glm::vec4& data);
	void setGlobalVector2(const char* name, glm::vec2& data);

	void destroy();
}