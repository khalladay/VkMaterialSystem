#pragma once
#include <vector>

void splitString(std::string& str, std::vector<std::string>& splitTarget, std::string delim)
{
	size_t last = 0;
	size_t next = 0;

	while ((next = str.find(delim, last)) != std::string::npos)
	{
		splitTarget.push_back(str.substr(last, next - last));
		last = next + 1;
	} 
	
	splitTarget.push_back(str.substr(last));
}