#pragma once
#include <cstdint>

#if _DEBUG
#include <type_traits>
#endif

template <class T>
struct FixedArray
{
	T* data;
	uint32_t num;
};

namespace fixed_array
{
	template<typename T> inline void resize(FixedArray<T>& arr, uint32_t newCount)
	{

#if _DEBUG
		static_assert(std::is_pod<T>::value);
#endif

		if (arr.data)
		{
			free(arr.data);
		}

		arr.num = newCount;
		arr.data = (T*)malloc(sizeof(T) * newCount);
	}
}