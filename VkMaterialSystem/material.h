#pragma once
#include "stdafx.h"

struct MaterialAsset;
struct MaterialRenderData;

struct MaterialDefinition
{
	const char* vShaderPath;
	const char* fShaderPath;
	bool depthTest;
	bool depthWrite;
};

namespace Material
{
	void make(MaterialDefinition def);

	MaterialRenderData getRenderData();
	void destroy();
}