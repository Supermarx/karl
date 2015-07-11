#pragma once

#include <vector>

namespace supermarx
{

template<typename T>
class matrix
{
public:
	const size_t size_i, size_j;

private:
	std::vector<T> data;

public:
	matrix(const size_t size_i, const size_t size_j, const T default_value)
	: size_i(size_i)
	, size_j(size_j)
	, data(size_i * size_j, default_value)
	{}

	T& operator()(const size_t i, const size_t j)
	{
		return data[i*size_j + j];
	};

	const T& operator()(const size_t i, const size_t j) const
	{
		return data[i*size_j + j];
	}
};

}
