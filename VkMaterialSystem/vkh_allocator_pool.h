#pragma once
struct VkhContext;

//perhaps the only good part of c++17: nested namespaces, woohoo!
namespace vkh::allocators::pool
{
	void activate(VkhContext* context);
	void deactivate(VkhContext* context);
}