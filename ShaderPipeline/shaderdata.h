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

struct UniformBlock
{
	std::string name;
	uint32_t size;
	std::vector<BlockMember> members;
	uint32_t set;
	uint32_t binding;
};

struct ShaderData
{
	UniformBlock pushConstants;
	std::vector<UniformBlock> uniformBlocks;
};

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

	if (data.uniformBlocks.size() > 0)
	{
		writer.Key("uniforms");
		writer.StartArray();


		for (auto& block : data.uniformBlocks)
		{
			if (block.members.size() > 0)
			{
				writer.StartObject();
				writer.Key("name");
				writer.String(block.name.c_str());
				writer.Key("size");
				writer.Int(block.size);
				writer.Key("set");
				writer.Int(block.set);
				writer.Key("binding");
				writer.Int(block.binding);

				writer.Key("elements");
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
		}

		writer.EndArray();
	}

	writer.EndObject();
	return s.GetString();
}