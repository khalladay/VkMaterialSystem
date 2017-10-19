#pragma once
#include <vector>
#include <string>
#include <Windows.h>
#include "string_utils.h"

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

bool doesDirectoryExist(std::string folderPath)
{
	DWORD dwAttrib = GetFileAttributes(folderPath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool deleteFile(std::string filepath)
{
	if (!DeleteFile(filepath.c_str()))
	{
		DWORD err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND)
		{
			return false;
		}

		if (err == ERROR_FILE_READ_ONLY)
		{
			return false;
		}
	}

	return true;
}

bool deleteDirectory(std::string dirPath)
{
	return false;
}

bool makeDirectory(std::string directoryPath)
{
	return	static_cast<bool>(CreateDirectory(directoryPath.c_str(), 0));
}

std::string makeFullPath(std::string path)
{
	char buf[1024];
	TCHAR** lppPart = { NULL };

	GetFullPathName(path.c_str(), 1024, buf, lppPart);
	return buf;
}

bool makeDirectoryRecursive(std::string directoryPath)
{
	std::vector<std::string> pathSplit;
	splitString(directoryPath, pathSplit, "\\");

	std::string curPath = pathSplit[0] + "\\";
	for (uint32_t i = 1; i < pathSplit.size(); ++i)
	{
		curPath += pathSplit[i] + "\\";
		if (!doesDirectoryExist(curPath))
		{
			makeDirectory(curPath);
		}
	}

	return true;
}

bool doesFilExist(std::string folderPath)
{
	DWORD dwAttrib = GetFileAttributes(folderPath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

