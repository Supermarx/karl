#pragma once

#include <supermarx/normalized_price.hpp>
#include <supermarx/measure.hpp>

namespace supermarx
{

class price_normalization
{
private:
	price_normalization() = delete;

	static inline uint64_t canonical_volume(measure volume_measure)
	{
		switch(volume_measure)
		{
		case measure::UNITS:
			return 1;
		case measure::MILLILITRES:
			return 1000;
		case measure::MILLIGRAMS:
			return 1000000;
		case measure::MILLIMETRES:
			return 1000;
		}
	}

public:
	static inline normalized_price exec(uint64_t price, uint64_t volume, measure volume_measure)
	{
		uint64_t ca = canonical_volume(volume_measure);

		if(volume == 0)
			return normalized_price({
				price,
				1,
				measure::UNITS
			});

		return normalized_price({
			price * ca / volume,
			ca,
			volume_measure
		});
	}
};

}
