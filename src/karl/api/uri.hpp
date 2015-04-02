#pragma once

#include <string>
#include <vector>
#include <map>

namespace supermarx
{

class uri
{
public:
	std::vector<std::string> path;
	std::map<std::string, std::string> args;

	uri(const std::string& uri);
	bool match_path(const size_t i, const std::string& x) const;

private:
	void parse_uri(const std::string& uri);
	void parse_path(const std::string& uri, size_t size);
	void parse_args(const std::string& uri, size_t i);
	void parse_arg(const std::string& arg);
};

}
