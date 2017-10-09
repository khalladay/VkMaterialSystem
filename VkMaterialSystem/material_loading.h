#pragma once
#include "stdafx.h"

struct MaterialDefinition;

namespace Material
{
	MaterialDefinition load(const char* assetPath);
}