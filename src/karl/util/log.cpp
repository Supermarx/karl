#include <karl/util/log.hpp>

#include <iostream>
#include <ctime>

namespace supermarx
{

log::log(const std::string& _facility, const level_e _l)
	: facility(_facility)
	, l(_l)
	, os()
{}

log::~log()
{
	std::cerr << os.str() << std::endl;
}

std::ostream& log::operator()()
{
	char time_str[80];
	std::time_t t = std::time(NULL);
	std::strftime(time_str, 80, "%F %T", std::localtime(&t));

	os << time_str << " [" << facility << "] " << to_string(l) << ": ";
	return os;
}

std::string to_string(const log::level_e l)
{
	switch(l)
	{
	case log::DEBUG:
		return "debug";
	case log::NOTICE:
		return "notice";
	case log::WARNING:
		return "warning";
	case log::ERROR:
		return "error";
	default:
		throw std::exception();
	}
}

}
