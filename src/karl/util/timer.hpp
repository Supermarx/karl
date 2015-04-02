#pragma once

#include <boost/chrono.hpp>

namespace supermarx
{

class timer
{
public:
	typedef boost::chrono::steady_clock clock;

private:
	clock::time_point start;

public:
	timer()
		: start(clock::now())
	{}

	clock::duration diff() const
	{
		return clock::now() - start;
	}

	boost::chrono::microseconds diff_msec() const
	{
		return boost::chrono::duration_cast<boost::chrono::microseconds>(diff());
	}
};

}
