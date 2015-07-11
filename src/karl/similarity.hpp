#pragma once

#include <functional>
#include <boost/algorithm/string.hpp>

#include <supermarx/message/product_summary.hpp>

#include <karl/util/levenshtein.hpp>
#include <karl/util/hungarian_fast.hpp>

namespace supermarx
{

class similarity
{
public:
	struct valuation
	{
		static constexpr size_t N = 3;

		std::array<float, N> data;

		valuation(decltype(data) _data)
		: data(_data)
		{}

		float collapse() const
		{
			static constexpr std::array<float, N> weights = {
				0.6f, 0.2f, 0.2f
			};

			float similarity = 0.0f;

			for(size_t i = 0; i < N; ++i)
				similarity += weights[i] * data[i];

			return similarity;
		}
	};

private:
	template<typename T>
	static inline float max_set(std::vector<T> const& xs, std::function<float(T const&)> f)
	{
		float result = std::numeric_limits<float>::min();

		for(auto const& x : xs)
			result = std::max(result, f(x));

		return result;
	}

	static inline float textual_compare(std::string const& x, std::string const& y)
	{
		std::vector<std::string> xs, ys;
		boost::split(xs, x, boost::is_any_of(" "));
		boost::split(ys, y, boost::is_any_of(" "));

		if(xs.size() > ys.size())
			std::swap(xs, ys);

		typedef float sim_t;
		similarity_matrix<sim_t> sm(ys.size(), xs.size());
		for(size_t yi = 0; yi < ys.size(); ++yi)
		{
			for(size_t xi = 0; xi < xs.size(); ++xi)
			{
				std::string const& ye = ys[yi];
				std::string const& xe = xs[xi];

				size_t distance_yx(levenshtein(ye, xe));
				assert(distance_yx >= 0 && distance_yx <= std::numeric_limits<sim_t>::max());

				sm(yi, xi) = static_cast<sim_t>(std::max(ye.size(), xe.size()) - distance_yx) / (sim_t)std::max(ye.size(), xe.size());
			}
		}

		hungarian_fast<decltype(sm)::similarity> h(sm);
		auto matching = h.produce();

		float similarity = 0.0f;
		for(auto matching_e : matching)
		{
			size_t yi = matching_e.first;
			size_t xi = matching_e.second;

			similarity += (float)sm(yi, xi);
		}

		float sim_min = std::min(ys.size(), xs.size()); // Due to std::swap ys.size() will always be bigger, Hungarian will thus always yield xs.size() elements
		float sim_max = std::max(ys.size(), xs.size());

		return 0.9f * similarity / sim_min + 0.1f * similarity / sim_max;
	}

	static inline float numeric_compare(float x, float y)
	{
		float result = 1.0f - std::abs(x - y) / std::max(x, y);
		assert(result >= 0.0f && result <= 1.0f);

		return result;
	}

public:
	similarity() = delete;

	static inline valuation exec(message::product_summary const& x, message::product_summary const& y)
	{
		return valuation({
			textual_compare(boost::to_lower_copy(x.name), boost::to_lower_copy(y.name)),
			numeric_compare(x.orig_price, y.orig_price),
			(x.volume_measure == y.volume_measure && x.volume == y.volume) ? 1.0f : 0.0f
		});
	}
};

}
