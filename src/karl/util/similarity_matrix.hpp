#pragma once

#include <karl/util/matrix.hpp>

namespace supermarx
{

template<typename T>
class similarity_matrix
{
public:
	typedef T similarity;

private:
	matrix<similarity> data;

public:
	similarity_matrix(const size_t height, const size_t width, const similarity default_value = 0)
	: data(height, width, default_value)
	{}

	similarity& operator()(const size_t y, const size_t x)
	{
		return data(y, x);
	};

	similarity operator()(const size_t y, const size_t x) const
	{
		return data(y, x);
	}

	similarity max() const
	{
		similarity result = 0;
		for(size_t y = 0; y < height(); y++)
			for(size_t x = 0; x < width(); x++)
				if(result < data(y, x))
					result = data(y, x);

		return result;
	}

	size_t width() const
	{
		return data.size_j;
	}

	size_t height() const
	{
		return data.size_i;
	}
};

}
