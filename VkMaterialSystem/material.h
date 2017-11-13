#pragma once
#include "stdafx.h"

#if 0
TODO: 
	- support a global dynamic uniform buffer - set once per frame
		- have shaderpipeline add that block to each shader before compilation
		- pass in time, resolution, mouse, etc to that uniform block
	- support changing uniforms on a material (only ones on the right set of course)

	- set 0 == global
	- set 2 == static
	- set 3 == dynamic
#endif 

struct MaterialRenderData;

struct MaterialAsset
{
	MaterialRenderData* rData;
};

namespace Material
{
	MaterialRenderData getRenderData();
	MaterialAsset& getMaterialAsset();

	void initGlobalShaderData();

	void setPushConstantVector(const char* name, glm::vec4& data);
	void setPushConstantFloat(const char* name, float data);
	void setPushConstantMatrix(const char* name, glm::mat4& data);

	void setUniformVector4(const char* name, glm::vec4& data);
	void setUniformVector2(const char* name, glm::vec2& data);
	void setUniformFloat(const char* name, float data);
	void setUniformMatrix(const char* name, glm::mat4& data);

	void setGlobalFloat(const char* name, float data);
	void setGlobalVector4(const char* name, glm::vec4& data);

	void destroy();
}