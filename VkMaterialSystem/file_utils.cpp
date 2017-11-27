#include "stdafx.h"

#include "file_utils.h"
#include <cstring>
#include <stdio.h>

BinaryBuffer* loadBinaryFile(const char* filepath)
{
	BinaryBuffer* outBuf = (BinaryBuffer*)malloc(sizeof(BinaryBuffer));

	FILE* inFile;
	fopen_s(&inFile,filepath, "rb");
	assert(inFile);
	
	fseek(inFile, 0, SEEK_END);
	outBuf->size = ftell(inFile);
	rewind(inFile);

	outBuf->data = (char *)calloc(1,(outBuf->size + 1)); // Enough memory for file + \0

	checkf(outBuf->data, "Allocation failed in LoadBinaryFile, calloc returned null");
	fread(outBuf->data, outBuf->size, 1, inFile); // Read in the entire file
	fclose(inFile); // Close the file

	return outBuf;
}

const char* loadTextFile(const char* filepath)
{
	FILE* inFile;
	fopen_s(&inFile, filepath, "r");
	checkf(inFile, "Trying to load text file: %s, file not found", filepath);

	fseek(inFile, 0, SEEK_END);
	long size = ftell(inFile);
	rewind(inFile);

	char* outString = (char *)calloc(1, size + 1); // Enough memory for file + \0

	fread(outString, size, 1, inFile); // Read in the entire file
	fclose(inFile); // Close the file

	return outString;
}

void freeBinaryBuffer(BinaryBuffer* buffer)
{
	checkf(buffer, "Passed a nullptr to freeBinaryBuffer");

	if (buffer->data)
	{
		free(buffer->data);
	}

	free(buffer);
}