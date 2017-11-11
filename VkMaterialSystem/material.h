#pragma once
#include "stdafx.h"

struct MaterialRenderData;

struct MaterialAsset
{
	MaterialRenderData* rData;
};

namespace Material
{
	MaterialRenderData getRenderData();
	MaterialAsset& getMaterialAsset();

	void setPushConstantVector(const char* name, glm::vec4& data);
	void setPushConstantFloat(const char* name, float data);
	void setPushConstantMatrix(const char* name, glm::mat4& data);

	void setUniformVector4(const char* name, glm::vec4& data);
	void setUniformVector2(const char* name, glm::vec2& data);
	void setUniformFloat(const char* name, float data);
	void setUniformMatrix(const char* name, glm::mat4& data);

	void destroy();
}