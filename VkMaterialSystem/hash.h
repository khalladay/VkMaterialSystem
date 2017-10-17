#pragma once
#include <cstdint>

#define HASH(x) djb2a(x)

uint32_t djb2a(const char *str)
{
	uint32_t hash = 5381;
	int c;

	while (c = *str++)
		hash = hash * 33 ^ c;

	return hash;
}
