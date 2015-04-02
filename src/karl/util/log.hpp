#pragma once

#include <string>
#include <sstream>

namespace supermarx
{

class log
{
public:
	enum level_e
	{
		DEBUG,
		NOTICE,
		WARNING,
		ERROR
	};

	log(const std::string& facility, const level_e l);
	~log();

	std::ostream& operator()();

private:
	std::string facility;
	level_e l;

	std::ostringstream os;
};

std::string to_string(const log::level_e l);

}
