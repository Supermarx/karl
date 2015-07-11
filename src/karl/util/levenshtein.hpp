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

		std::vector<size_t> costs(n+1);
		std::iota(costs.begin(), costs.end(), 0);

		for(size_t i = 0; i < m; ++i)
		{
			char cx = x.at(i);

			costs[0] = i+1;
			size_t corner = i;

			for(size_t j = 0; j < n; ++j)
			{
				char cy = y.at(j);
				size_t upper = costs.at(j+1);

				if(cx == cy)
					costs.at(j+1) = corner;
				else
				{
					size_t t(upper < corner ? upper : corner);
					costs.at(j+1) = (costs.at(j) < t ? costs.at(j) : t) + 1;
				}

				corner = upper;
			}
		}

		size_t result = costs[n];

		return result;
	}
}
