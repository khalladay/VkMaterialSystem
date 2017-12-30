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
	//could probably get away with packing this into a single uint64 like
	//bitsquid, but that seems like overkill, and way harder to debug. 
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
	Instance make(const char* assetPath);
	Instance make(Instance baseInstance);

	//used to create an empty material in material storage, 
	//only needed if you're creating a material in a way other than
	//loading the definition file from a path (as above)
	uint32_t reserve(const char* reserveName);

	void setPushConstantVector(Instance instance, const char* name, glm::vec4& data);
	void setPushConstantFloat(Instance instance, const char* name, float data);
	void setPushConstantMatrix(Instance instance, const char* name, glm::mat4& data);
	
	void setUniformData(Instance instance, uint32_t name, void* data);
	void setUniformVector4(Instance instance, const char* name, glm::vec4& data);
	void setUniformVector2(Instance instance, const char* name, glm::vec2& data);
	void setUniformFloat(Instance instance, const char* name, float data);
	void setUniformMatrix(Instance instance, const char* name, glm::mat4& data);

	void setTexture(Instance instance, const char* name, uint32_t texId);

	void setGlobalFloat(const char* name, float data);
	void setGlobalVector4(const char* name, glm::vec4& data);
	void setGlobalVector2(const char* name, glm::vec2& data);

	uint32_t charArrayToMaterialName(const char* name);

	void destroy(uint32_t asset);
	void destroy(Instance instance);
}