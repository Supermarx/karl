#pragma once

#include <functional>

namespace supermarx
{

class guard
{
public:
	typedef std::function<void()> f_t;

private:
	f_t _f;

public:
	guard(guard&) = delete;
	void operator=(guard&) = delete;

	guard(f_t const& f)
		: _f(f)
	{}

	~guard()
	{
		_f();
	}
};

}
