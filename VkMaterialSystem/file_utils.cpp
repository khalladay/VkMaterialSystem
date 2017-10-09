#include "file_utils.h"
#include <string.h>
#include <fstream>
#include <cassert>
#include <sstream>

void readAllBytes(std::ifstream& file, BinaryBuffer* outBlob, bool addNull)
{
	assert(file.is_open());
	outBlob->size = (size_t)file.tellg();
	if (addNull) outBlob->size++;

	outBlob->data = (char*)malloc(outBlob->size);

	file.seekg(0);

	file.read(outBlob->data, outBlob->size);
	if (addNull) outBlob->data[outBlob->size - 1] = '\0';
	
	file.close();

	assert(!file.is_open());

}

BinaryBuffer* loadBinaryFile(const char* filepath)
{
	BinaryBuffer* outBlob = (BinaryBuffer*)malloc(sizeof(BinaryBuffer));

	std::ifstream file(filepath, std::ios::ate | std::ios::binary);
	readAllBytes(file, outBlob, false);

	return outBlob;
}

std::string loadTextFile(const char* filepath)
{
	std::ifstream t(filepath);

	std::stringstream buffer;
	buffer << t.rdbuf();
	
	return buffer.str();
}

void freeBinaryBuffer(BinaryBuffer* buffer)
{
	assert(buffer);
	assert(buffer->data);
	free(buffer->data);
	free(buffer);
}