#pragma once
#include <vector>
#include <string>

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
	std::string outStr = "";
	if (data.pushConstants.size > 0)
	{
		outStr += "push_constants:" + std::to_string(data.pushConstants.size);
		outStr += "\n";

		for (uint32_t i = 0; i < data.pushConstants.members.size(); ++i)
		{
			outStr += "\t" + data.pushConstants.members[i].name + ":";
			outStr += "" + std::to_string(data.pushConstants.members[i].size);
			outStr += ":" + std::to_string(data.pushConstants.members[i].offset);
			outStr += "\n";
		}
	}

	for (auto& block : data.uniformBlocks)
	{
		if (block.members.size() > 0)
		{
			outStr += block.name + ":";
			outStr += std::to_string(block.size);
			outStr += +":";
			outStr += std::to_string(block.set);
			outStr += ":";
			outStr += std::to_string(block.binding);
			outStr += "\n";

			for (uint32_t i = 0; i < block.members.size(); ++i)
			{
				outStr += "\t" + block.members[i].name + ":";
				outStr += std::to_string(block.members[i].size);
				outStr += ":" + std::to_string(block.members[i].offset);
				outStr += "\n";
			}
			outStr += "\n";
		}
	}

	return outStr;
}