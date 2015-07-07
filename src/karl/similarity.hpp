#pragma once

#include <functional>
#include <boost/algorithm/string.hpp>

#include <supermarx/message/product_summary.hpp>

#include <karl/util/levenshtein.hpp>

namespace supermarx
{

class similarity
{
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
		boost::split(xs, x, boost::is_any_of("\t "));
		boost::split(ys, y, boost::is_any_of("\t "));

		if(xs.size() > ys.size())
			std::swap(xs, ys);

		float similarity = 0.0f;
		for(auto const& xe : xs)
			similarity += max_set<std::string>(ys, [&](std::string const& ye) {
				return 1.0f - (float)levenshtein(xe, ye) / (float)std::max(xe.size(), ye.size());
			});

		similarity /= (float)xs.size();
		return similarity;
	}

	template<typename T>
	static inline float numeric_compare(T x, T y)
	{
		return 1.0f - (float)std::abs(x - y) / (float)std::max(x, y);
	}

public:
	similarity() = delete;

	static inline float exec(message::product_summary const& x, message::product_summary const& y)
	{
		float similarity = 0.0;

		//similarity += 1.0f - (float)levenshtein(boost::to_lower_copy(x.name), boost::to_lower_copy(y.name)) / (float)std::max(x.name.size(), y.name.size()); // TODO wordwise
		similarity += 0.3f * textual_compare(boost::to_lower_copy(x.name), boost::to_lower_copy(y.name));
		similarity += 0.2f * numeric_compare(x.orig_price, y.orig_price);

		if(x.volume_measure == y.volume_measure)
			similarity += 0.5f * numeric_compare(x.volume, y.volume);

		return similarity;
	}
};

}
