#pragma once
#include <cstdint>

#if _DEBUG
#include <type_traits>
#endif

template <class T>
struct Array
{
	T* data;
	uint32_t num;
	uint32_t capacity;
};

namespace array
{
	template<typename T> inline void resize(Array<T>& arr, uint32_t newCount)
	{

#if _DEBUG
		static_assert(std::is_pod<T>::value);
#endif

		if (newCount > a.capacity)
		{
			grow(a, newCount);
		}
		
		a.size = newCount;
	}

	template<typename T> void setCapacity(Array<T> &a, uint32_t newCapacity)
	{
#if _DEBUG
		static_assert(std::is_pod<T>::value);
#endif

		if (newCapacity == a.capacity) return;

		if (newCapacity < a.size) resize(a, newCapacity);

		T *newData = 0;
		
		if (newCapacity > 0)
		{
			newData = (T *)_aligned_malloc(sizeof(T)*new_capacity, alignof(T));
			memcpy(newData, a.data, sizeof(T)*a.num);
		}

		free(a.data);
		a.data = newData;
		a.capacity = newCapacity;
	}

	template<typename T> void grow(Array<T> &a, uint32_t minCapacity)
	{
#if _DEBUG
		static_assert(std::is_pod<T>::value);
#endif

		uint32_t newCapacity = a.capacity * 2 + 8;

		if (newCapacity < minCapacity)
		{
			newCapacity = minCapacity;
		}

		setCapacity(a, newCapacity);
	}

	template<typename T> inline void push_back(Array<T> &a, const T &item)
	{
		if (a.size + 1 > a.capacity)
		{
			grow(a);
		}

		a.data[a.size++] = item;
	}

	template<typename T> inline void pop_back(Array<T> &a)
	{
		a.size--;
	}

	template<typename T> inline void free(Array<T> &a)
	{
		free(a.data);
	}
}