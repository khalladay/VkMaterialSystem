#pragma once
#include <vector>
#include <string>
#include <Windows.h>

std::vector<std::string> getFilesInDirectory(std::string folder)
{
	using std::vector;
	using std::string;

	vector<string> names;
	string search_path = folder + "/*.*";

	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(search_path.c_str(), &fd);

	if (hFind != INVALID_HANDLE_VALUE) 
	{
		do {
			// read all (real) files in current folder
			// , delete '!' read other 2 default folder . and ..
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				names.push_back(fd.cFileName);
			}
		} while (FindNextFile(hFind, &fd));
		FindClose(hFind);
	}
	return names;
}

bool makeDirectory(std::string folderPath)
{
	return true;
}
