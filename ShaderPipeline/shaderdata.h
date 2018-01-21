#pragma once
#include <vector>
#include <string>
#include <rapidjson\prettywriter.h>
#include <rapidjson\stringbuffer.h>

struct BlockMember
{
	std::string name;
	uint32_t size;
	uint32_t offset;
};

enum class BlockType : uint8_t
{
	UNIFORM = 0,
	TEXTURE = 1,
	SEPARATETEXTURE = 2,
	SAMPLER = 3,
	TEXTUREARRAY = 4,
	SAMPLERARRAY = 5
};

std::string BlockTypeNames[] =
{
	"UNIFORM",
	"TEXTURE",
	"SEPARATETEXTURE"
	"SAMPLER",
	"TEXTUREARRAY",
	"SAMPELRARRAY"
};

struct InputBlock
{
	std::string name;
	uint32_t size;
	std::vector<BlockMember> members;
	uint32_t set;
	uint32_t binding;
	BlockType type;
	uint32_t arrayLen;
};

struct ShaderData
{
	InputBlock pushConstants;
	std::vector<InputBlock> descriptorSets;

	std::vector<uint32_t> dynamicSets;
	std::vector<uint32_t> globalSets;
	std::vector<uint32_t> staticSets;

	uint32_t dynamicSetSize;
	uint32_t staticSetSize;
	uint32_t numDynamicUniforms;
	uint32_t numDynamicTextures;
	uint32_t numStaticUniforms;
	uint32_t numStaticTextures;
};


void writeInputGroup(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, std::vector<InputBlock>& descSet, std::string inputGroupName)
{
	writer.Key(inputGroupName.c_str());
	writer.StartArray();
	
	for (InputBlock& block : descSet)
	{
		writer.StartObject();
		writer.Key("set");
		writer.Int(block.set);
		writer.Key("binding");
		writer.Int(block.binding);
		writer.Key("name");
		writer.String(block.name.c_str());
		writer.Key("size");
		writer.Int(block.size);
		writer.Key("arraylen");
		writer.Int(block.arrayLen);
		writer.Key("type");
		writer.String(BlockTypeNames[(uint8_t)block.type].c_str());

		writer.Key("members");
		writer.StartArray();
		for (uint32_t i = 0; i < block.members.size(); ++i)
		{
			writer.StartObject();
			writer.Key("name");
			writer.String(block.members[i].name.c_str());
			writer.Key("size");
			writer.Int(block.members[i].size);
			writer.Key("offset");
			writer.Int(block.members[i].offset);
			writer.EndObject();

		}
		writer.EndArray();
		writer.EndObject();
	}

	writer.EndArray();
}

std::string getReflectionString(ShaderData& data)
{

	using namespace rapidjson;
	StringBuffer s;
	PrettyWriter<StringBuffer> writer(s);

	writer.StartObject();

	if (data.pushConstants.size > 0)
	{
		writer.Key("push_constants");
		writer.StartObject();
		writer.Key("size");
		writer.Int(data.pushConstants.size);

		writer.Key("elements");
		writer.StartArray();

		for (uint32_t i = 0; i < data.pushConstants.members.size(); ++i)
		{
			writer.StartObject();
			writer.Key("name");
			writer.String(data.pushConstants.members[i].name.c_str());
			writer.Key("size");
			writer.Int(data.pushConstants.members[i].size);
			writer.Key("offset");
			writer.Int(data.pushConstants.members[i].offset);
			writer.EndObject();
		}
		writer.EndArray();
		writer.EndObject();
	}

	writeInputGroup(writer, data.descriptorSets, "descriptor_sets");

	writer.Key("global_sets");
	writer.StartArray();
	for (uint32_t i : data.globalSets) writer.Int(i);
	writer.EndArray();

	writer.Key("static_sets");
	writer.StartArray();
	for (uint32_t i : data.staticSets) writer.Int(i);
	writer.EndArray();

	writer.Key("dynamic_sets");
	writer.StartArray();
	for (uint32_t i : data.dynamicSets) writer.Int(i);
	writer.EndArray();

	writer.Key("static_set_size");
	writer.Int(data.staticSetSize);
	writer.Key("dynamic_set_size");
	writer.Int(data.dynamicSetSize);
	writer.Key("num_static_uniforms");
	writer.Int(data.numStaticUniforms);
	writer.Key("num_static_textures");
	writer.Int(data.numStaticTextures);

	writer.Key("num_dynamic_uniforms");
	writer.Int(data.numDynamicUniforms);
	writer.Key("num_dynamic_textures");
	writer.Int(data.numDynamicTextures);

	writer.EndObject();
	return s.GetString();
}