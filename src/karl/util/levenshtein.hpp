#pragma once

#include <string>
#include <memory>
#include <numeric>

namespace supermarx
{
	inline static size_t levenshtein(std::string const& x, std::string const& y)
	{
		const size_t m(x.size());
		const size_t n(y.size());

		if(m == 0)
			return n;

		if(n == 0)
			return m;

		std::vector<size_t> costs;
		costs.reserve(n+1);
		std::iota(costs.begin(), costs.end(), 0);

		for(size_t i = 0; i < m; ++i)
		{
			char cx = x[i];

			costs[0] = i+1;
			size_t corner = i;

			for(size_t j = 0; j < n; ++j)
			{
				char cy = y[j];
				size_t upper = costs[j+1];

				if( cx == cy )
					costs[j+1] = corner;
				else
				{
					size_t t(upper < corner ? upper : corner);
					costs[j+1] = (costs[j] < t ? costs[j] : t) + 1;
				}

				corner = upper;
			}
		}

		size_t result = costs[n];

		return result;
	}
}
