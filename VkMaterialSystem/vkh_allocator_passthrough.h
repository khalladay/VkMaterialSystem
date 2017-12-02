#pragma once
#include "stdafx.h"

struct VkhContext;

//perhaps the only good part of c++17: nested namespaces, woohoo!
namespace vkh::allocators::passthrough
{
	void activate(VkhContext* context);
	void deactivate(VkhContext* context);
}