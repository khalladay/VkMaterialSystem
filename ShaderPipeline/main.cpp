#include <spirv_glsl.hpp>
#include <argh.h>
#include <string>
#include <vector>
#include "filesystem_utils.h"


int main(int argc, const char** argv)
{
	argh::parser cmdl(argv);
	if (!cmdl(3)) printf("ShaderPipeline: usage: ShaderPipeline <path to shader folder> <path to output shader folder> <path to output reflection folder> \n");

	std::string shaderInPath = cmdl[1];
	std::string shaderOutPath = cmdl[2];
	std::string reflOutPath = cmdl[3];

	std::vector<std::string> files = getFilesInDirectory(shaderInPath);

	return 0;
}