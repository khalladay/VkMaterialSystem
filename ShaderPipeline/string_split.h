#pragma once
#include <vector>
#include <string>

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

void FindReplace(std::string& line, std::string& oldString, std::string& newString) 
{
	using std::string;

	const size_t oldSize = oldString.length();


	// do nothing if line is shorter than the string to find
	if (oldSize > line.length()) return;

	const size_t newSize = newString.length();
	for (size_t pos = 0; ; pos += newSize) {
		// Locate the substring to replace
		pos = line.find(oldString, pos);
		if (pos == string::npos) return;
		if (oldSize == newSize) {
			// if they're same size, use std::string::replace
			line.replace(pos, oldSize, newString);
		}
		else {
			// if not same size, replace by erasing and inserting
			line.erase(pos, oldSize);
			line.insert(pos, newString);
		}
	}
}
