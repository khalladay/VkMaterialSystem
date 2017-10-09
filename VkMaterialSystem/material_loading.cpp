#include "material_loading.h"
#include "material.h"
#include "file_utils.h"
#include "string_utils.h"
#include <vector>

namespace Material
{
	MaterialDefinition load(const char* assetPath)
	{
		MaterialDefinition def;

		std::string buffer = loadTextFile(assetPath);

		std::vector<std::string> parsedMaterial = split(buffer, ':', true);

		std::string vShaderPath = "../data/shaders/"+parsedMaterial[1] + ".vert.spv";
		std::string fShaderPath = "../data/shaders/"+parsedMaterial[3] + ".frag.spv";

		def.vShaderPath = _strdup(vShaderPath.c_str());
		def.fShaderPath = _strdup(fShaderPath.c_str());


		return def;
	}
}