#include <spirv_glsl.hpp>
#include <argh.h>
#include <string>
#include <vector>
#include "filesystem_utils.h"
#include <stdint.h>
#include "string_split.h"
#include "material.h"

std::string baseTypeToString(spirv_cross::SPIRType::BaseType type);

void createUniformBlockForResource(UniformBlock* outBlock, spirv_cross::Resource res, spirv_cross::CompilerGLSL& compiler)
{	
	uint32_t id = res.id;
	std::vector<spirv_cross::BufferRange> ranges = compiler.get_active_buffer_ranges(id);

	outBlock->name = res.name;

	uint32_t totalSize = 0;
	for (auto& range : ranges)
	{
		BlockMember mem;
		mem.name = compiler.get_member_name(res.base_type_id, range.index);
		mem.size = range.range;
		mem.offset = range.offset;

		totalSize += mem.size;

		auto type = compiler.get_type(res.type_id);

		auto baseType = type.basetype;
		std::string baseTypeString = baseTypeToString(baseType);
		std::string mName = compiler.get_member_name(res.base_type_id, range.index);

		outBlock->members.push_back(mem);
	}

	outBlock->size = totalSize;
	outBlock->binding = compiler.get_decoration(res.id, spv::DecorationBinding);
	outBlock->set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
}

int main(int argc, const char** argv)
{
	argh::parser cmdl(argv);
	if (!cmdl(3)) printf("ShaderPipeline: usage: ShaderPipeline <path to shader folder> <path to output shader folder> <path to output reflection folder> \n");

	std::string shaderInPath = cmdl[1];
	std::string shaderOutPath = cmdl[2];
	std::string reflOutPath = cmdl[3];

	makeDirectoryRecursive(makeFullPath(shaderOutPath));
	makeDirectoryRecursive(makeFullPath(reflOutPath));

	std::vector<std::string> files = getFilesInDirectory(shaderOutPath);

	//first - delete all files in built shader directory if they exist
	{
		std::vector<std::string> builtshaders = getFilesInDirectory(shaderOutPath);
		if (builtshaders.size() > 0)
		{
			for (uint32_t i = 0; i < builtshaders.size(); ++i)
			{
				std::string relPath = shaderOutPath + files[i];
				std::string fullPath = makeFullPath(relPath);
				(deleteFile(fullPath));
			}
		}
	}


	//next, compile all input shaders into spv
	{
		std::vector<std::string> inputShaders = getFilesInDirectory(shaderInPath);
		std::string pathToShaderCompile = "../third_party/glslangValidator";
		pathToShaderCompile = makeFullPath(pathToShaderCompile);


		for (uint32_t i = 0; i < inputShaders.size(); ++i)
		{
			std::string relPath = shaderInPath + inputShaders[i];
			std::string fullPath = makeFullPath(relPath);
			
			std::string shaderOutFull = makeFullPath(shaderOutPath);
			std::string fileOut = shaderOutFull +"/"+ inputShaders[i] + ".spv";

			std::string compilecommand = pathToShaderCompile+" -V -o " + fileOut + " " + fullPath; 
			printf("%s\n", compilecommand.c_str());
			system(compilecommand.c_str());
		}
	}

	//finally, generate reflection files for all built shaders
	{

		std::vector<std::string> builtFiles = getFilesInDirectory(shaderOutPath);
		for (uint32_t i = 0; i < builtFiles.size(); ++i)
		{

			if (builtFiles[i].find(".refl") != std::string::npos) continue;

			ShaderData data = {};

			std::string relPath = shaderOutPath + "\\" +builtFiles[i];
			std::string fullPath = makeFullPath(relPath);

			std::string reflPath = reflOutPath + "\\" + builtFiles[i];
			FindReplace(reflPath, std::string(".spv"), std::string(".refl"));
			std::string reflFullPath = makeFullPath(reflPath);

			FILE* shaderFile;
			fopen_s(&shaderFile, fullPath.c_str(), "rb");
			assert(shaderFile);

			fseek(shaderFile, 0, SEEK_END);
			size_t filesize = ftell(shaderFile);
			size_t wordSize = sizeof(uint32_t);
			size_t wordCount = filesize / wordSize;
			rewind(shaderFile);


			uint32_t* ir = (uint32_t*)malloc(sizeof(uint32_t) * wordCount);

			fread(ir, filesize, 1, shaderFile);
			fclose(shaderFile);

			spirv_cross::CompilerGLSL glsl(ir, wordCount);

			spirv_cross::ShaderResources resources = glsl.get_shader_resources();

			for (spirv_cross::Resource res : resources.push_constant_buffers)
			{
				createUniformBlockForResource(&data.pushConstants, res, glsl);
			}

			data.uniformBlocks.resize(resources.uniform_buffers.size());
			uint32_t idx = 0;
			for (spirv_cross::Resource res : resources.uniform_buffers)
			{
				createUniformBlockForResource(&data.uniformBlocks[idx++], res, glsl);
			}

			for (spirv_cross::Resource res : resources.sampled_images)
			{

			}

			//write out material
			std::string shader = getReflectionString(data);
			FILE *file;
			fopen_s(&file, reflFullPath.c_str(), "w");
			int results = fputs(shader.c_str(), file);
			assert(results != EOF);
			fclose(file);
		}
	}

	getchar();
	return 0;
}

std::string baseTypeToString(spirv_cross::SPIRType::BaseType type)
{
	std::string names[] = {
		"Unknown",
		"Void",
		"Boolean",
		"Char",
		"Int",
		"UInt",
		"Int64",
		"UInt64",
		"AtomicCounter",
		"Float",
		"Double",
		"Struct",
		"Image",
		"SampledImage",
		"Sampler"
	};

	return names[type];

}