#include "file_utils.h"
#include "stdafx.h"

#include <string.h>
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
	fread(outBuf->data, outBuf->size, 1, inFile); // Read in the entire file
	fclose(inFile); // Close the file

	return outBuf;
}

const char* loadTextFile(const char* filepath)
{
	FILE* inFile;
	fopen_s(&inFile, filepath, "r");
	assert(inFile);

	fseek(inFile, 0, SEEK_END);
	long size = ftell(inFile);
	rewind(inFile);

	char* outString = (char *)calloc(1, size + 1); // Enough memory for file + \0

	fread(outString, size, 1, inFile); // Read in the entire file
	//outString[size] = '\0';
	fclose(inFile); // Close the file

	return outString;
}

void freeBinaryBuffer(BinaryBuffer* buffer)
{
	assert(buffer);
	assert(buffer->data);
	free(buffer->data);
	free(buffer);
}