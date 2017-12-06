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

	T &operator[](uint32_t i);
	const T &operator[](uint32_t i) const;

	//Array &operator=(const Array &other);
};

template <typename T> inline T & Array<T>::operator[](uint32_t i)
{
#if _DEBUG
	static_assert(std::is_pod<T>::value, "T must be POD");
	checkf(i < num, "Attempting to read past end of array");
#endif
	return data[i];
}

template <typename T> inline const T & Array<T>::operator[](uint32_t i) const
{
#if _DEBUG
	static_assert(std::is_pod<T>::value, "T must be POD");
	checkf(i < num, "Attempting to read past end of array");
#endif
	return data[i];
}

namespace array
{
	template <typename T> inline void initialize(Array<T>& arr)
	{
		arr.capacity = 0;
		arr.data = 0;
		arr.num = 0;
	}

	template <typename T> inline void copy(const Array<T> &src, Array<T>& dst)
	{
		const uint32_t n = src.size;
		array::resize(dst, n);
		memcpy(src.data, dst.data, sizeof(T)*n);
	}


	template<typename T> inline void resize(Array<T>& arr, uint32_t newCount)
	{

#if _DEBUG
		static_assert(std::is_pod<T>::value, "T must be POD");
#endif

		if (newCount > arr.capacity)
		{
			grow(arr, newCount);
		}
		
		arr.num = newCount;
	}

	template<typename T> void setCapacity(Array<T> &a, uint32_t newCapacity)
	{
#if _DEBUG
		static_assert(std::is_pod<T>::value, "T must be POD");
#endif

		if (newCapacity == a.capacity) return;

		if (newCapacity < a.num) resize(a, newCapacity);

		T *newData = 0;
		
		if (newCapacity > 0)
		{
			newData = (T *)_aligned_malloc(sizeof(T)*newCapacity, alignof(T));
			memcpy(newData, a.data, sizeof(T)*a.num);
		}
	
		::free(a.data);
		a.data = newData;
		a.capacity = newCapacity;
	}

	template <typename T> inline void reserve(Array<T> &a, uint32_t new_capacity)
	{
#if _DEBUG
		static_assert(std::is_pod<T>::value, "T must be POD");
#endif
		if (new_capacity > a.capacity)
			setCapacity(a, new_capacity);
	}

	template<typename T> void grow(Array<T> &a, uint32_t minCapacity)
	{
#if _DEBUG
		static_assert(std::is_pod<T>::value, "T must be POD");
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
		if (a.num + 1 > a.capacity)
		{
			grow(a, a.num+1);
		}

		a.data[a.num++] = item;
	}

	template<typename T> inline void pop_back(Array<T> &a)
	{
		a.num--;
	}

	template<typename T> inline void free(Array<T> &a)
	{
		::free(a.data);
	}
}