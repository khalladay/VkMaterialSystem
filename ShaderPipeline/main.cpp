#include <spirv_glsl.hpp>
#include <argh.h>
#include <string>
#include <vector>
#include <stdint.h>
#include "filesystem_utils.h"
#include "string_utils.h"
#include "shaderdata.h"
#include "config.h"
#include <cstdio>
#include <codecvt>

std::string baseTypeToString(spirv_cross::SPIRType::BaseType type);

void createUniformBlockForResource(InputBlock* outBlock, spirv_cross::Resource res, spirv_cross::CompilerGLSL& compiler)
{	
	uint32_t id = res.id;
	std::vector<spirv_cross::BufferRange> ranges = compiler.get_active_buffer_ranges(id);
	const spirv_cross::SPIRType& ub_type = compiler.get_type(res.base_type_id);

	outBlock->name = res.name;
	for (auto& range : ranges)
	{
		BlockMember mem;
		mem.name = compiler.get_member_name(res.base_type_id, range.index);
		mem.size = range.range;
		mem.offset = range.offset;

		auto type = compiler.get_type(res.type_id);

		auto baseType = type.basetype;
		std::string baseTypeString = baseTypeToString(baseType);
		std::string mName = compiler.get_member_name(res.base_type_id, range.index);

		outBlock->members.push_back(mem);
	}

	outBlock->size = compiler.get_declared_struct_size(ub_type);
	outBlock->binding = compiler.get_decoration(res.id, spv::DecorationBinding);
	outBlock->set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
	outBlock->arrayLen = 0;
	outBlock->type = BlockType::UNIFORM;
}

void createSeparateSamplerBlockForResource(InputBlock* outBlock, spirv_cross::Resource res, spirv_cross::CompilerGLSL& compiler)
{
	uint32_t id = res.id;

	outBlock->name = res.name;
	outBlock->set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
	outBlock->binding = compiler.get_decoration(res.id, spv::DecorationBinding);

	//.array is an array of uint32_t array sizes (for cases of more than one array)
	outBlock->arrayLen = compiler.get_type(res.type_id).array[0];

	outBlock->type = outBlock->arrayLen == 1 ? BlockType::SAMPLER : BlockType::SAMPLERARRAY;
}

void createSeparateTextureBlockForResource(InputBlock* outBlock, spirv_cross::Resource res, spirv_cross::CompilerGLSL& compiler)
{
	uint32_t id = res.id;

	outBlock->name = res.name;
	outBlock->set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
	outBlock->binding = compiler.get_decoration(res.id, spv::DecorationBinding);
	
	
	outBlock->arrayLen = compiler.get_type(res.type_id).array[0];
	outBlock->type = outBlock->arrayLen == 1 ? BlockType::SEPARATETEXTURE : BlockType::TEXTUREARRAY;
}

void createTextureBlockForResource(InputBlock* outBlock, spirv_cross::Resource res, spirv_cross::CompilerGLSL& compiler)
{
	uint32_t id = res.id;

	outBlock->name = res.name;
	outBlock->set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
	outBlock->binding = compiler.get_decoration(res.id, spv::DecorationBinding);
	outBlock->arrayLen = 0;
	outBlock->type = BlockType::TEXTURE;
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

	uint32_t compileErr = 0;

	//next, compile all input shaders into spv
	{
		std::vector<std::string> inputShaders = getFilesInDirectory(shaderInPath);
		std::string pathToShaderCompile = "../third_party/glslangValidator";
		pathToShaderCompile = makeFullPath(pathToShaderCompile);

		//write out config file - need to specify UTF-8 because
		//otherwise we end up with UTF-16 by default and glslang can't read
		//a config file with that encoding
		const unsigned int initial_cp = GetConsoleOutputCP();
		SetConsoleOutputCP(CP_UTF8);

		FILE* configOut;
		fopen_s(&configOut, "myconf.conf", "w");
		fputs(gpuConfig, configOut);
		fclose(configOut);

		SetConsoleOutputCP(initial_cp);


		std::string configPath = makeFullPath("myconf.conf");

		for (uint32_t i = 0; i < inputShaders.size(); ++i)
		{
			std::string relPath = shaderInPath + inputShaders[i];
			std::string fullPath = makeFullPath(relPath);
			
			std::string shaderOutFull = makeFullPath(shaderOutPath);
			std::string fileOut = shaderOutFull +"/"+ inputShaders[i] + ".spv";

			std::string compilecommand = pathToShaderCompile + " -V -o " + fileOut + " " + fullPath + " " + configPath;
			printf("%s\n", compilecommand.c_str());
			compileErr = system(compilecommand.c_str());
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
			findReplace(reflPath, std::string(".spv"), std::string(".refl"));
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

			data.descriptorSets.resize(resources.uniform_buffers.size() + resources.sampled_images.size() + resources.separate_images.size() + resources.separate_samplers.size());

			uint32_t idx = 0;
			for (spirv_cross::Resource res : resources.uniform_buffers)
			{
				createUniformBlockForResource(&data.descriptorSets[idx++], res, glsl);
			}

			for (spirv_cross::Resource res : resources.sampled_images)
			{
				createTextureBlockForResource(&data.descriptorSets[idx++], res, glsl);
			}

			for (spirv_cross::Resource res : resources.separate_images)
			{
				createSeparateTextureBlockForResource(&data.descriptorSets[idx++], res, glsl);
			}

			for (spirv_cross::Resource res : resources.separate_samplers)
			{
				createSeparateSamplerBlockForResource(&data.descriptorSets[idx++], res, glsl);
			}

			std::sort(data.descriptorSets.begin(), data.descriptorSets.end(), [](const InputBlock& lhs, const InputBlock& rhs)
			{
				if (lhs.set != rhs.set) return lhs.set < rhs.set;
				return lhs.binding < rhs.binding;
			});


			for (uint32_t blockIdx = 0; blockIdx < data.descriptorSets.size(); ++blockIdx)
			{
				InputBlock& b = data.descriptorSets[blockIdx];

				if (b.set == GLOBAL_SET) data.globalSets.push_back(b.set);
				else if (b.set == DYNAMIC_SET)
				{
					if (b.type == BlockType::TEXTURE) data.numDynamicTextures++;
					else data.numDynamicUniforms++;

					if (std::find(data.dynamicSets.begin(), data.dynamicSets.end(), b.set) == data.dynamicSets.end())
					{
						data.dynamicSets.push_back(b.set);
					}
					data.dynamicSetSize += b.size;
				}
				else
				{
					if (b.type == BlockType::TEXTURE) data.numStaticTextures++;
					else data.numStaticUniforms++;

					if (std::find(data.staticSets.begin(), data.staticSets.end(), b.set) == data.staticSets.end())
					{
						data.staticSets.push_back(b.set);
					}

					data.staticSetSize += b.size;
				}
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