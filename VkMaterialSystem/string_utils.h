#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

std::vector<std::string> split(std::string str, char delim, bool splitOnWhitespace)
{
	using std::string;

	if (splitOnWhitespace)
	{
		std::replace(str.begin(), str.end(), '\n', delim);
	}


	std::stringstream ss(str);
	std::vector<string> result;

	while (ss.good())
	{
		string substr;
		getline(ss, substr, delim);
		result.push_back(substr);
	}

	return result;
}
