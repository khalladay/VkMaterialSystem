#include "file_utils.h"
#include <string.h>
#include <fstream>
#include <cassert>

BinaryBuffer* loadBinaryFile(const char* filepath)
{
	BinaryBuffer* outBlob = new BinaryBuffer();

	std::ifstream file(filepath, std::ios::ate | std::ios::binary);
	assert(file.is_open());

	outBlob->size = (size_t)file.tellg();
	outBlob->data = (char*)malloc(outBlob->size);

	file.seekg(0);
	file.read(outBlob->data, outBlob->size);

	file.close();

	assert(!file.is_open());

	return outBlob;
}

void freeBinaryBuffer(BinaryBuffer* buffer)
{
	assert(buffer);
	assert(buffer->data);
	delete[] buffer->data;
	delete buffer;
}