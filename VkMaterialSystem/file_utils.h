#pragma once

struct BinaryBuffer
{
	char* data;
	size_t size;
};

BinaryBuffer*	loadBinaryFile(const char* filepath);
void			freeBinaryBuffer(BinaryBuffer* buffer);