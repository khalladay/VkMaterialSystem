#pragma once
#include <string>

struct BinaryBuffer
{
	char* data;
	size_t size;
};

BinaryBuffer*	loadBinaryFile(const char* filepath);
std::string		loadTextFile(const char* filepath);
void			freeBinaryBuffer(BinaryBuffer* buffer);