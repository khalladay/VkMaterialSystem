#pragma once
#include "stdafx.h"

struct MaterialAsset;
struct MaterialRenderData;

struct MaterialDefinition
{
	char* vShaderPath;
	char* fShaderPath;
	bool depthTest;
	bool depthWrite;
};

namespace Material
{
	void make(MaterialDefinition def);

	MaterialRenderData getRenderData();
	void destroy();
}