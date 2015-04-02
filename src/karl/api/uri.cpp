#include <karl/api/uri.hpp>

#include <iostream>

namespace supermarx
{

uri::uri(const std::string& uri)
	: path()
	, args()
{
	parse_uri(uri);
}

void uri::parse_uri(const std::string& uri)
{
	size_t args_start = uri.find('?');

	if(args_start == std::string::npos)
		parse_path(uri, uri.size());
	else
	{
		parse_path(uri, args_start);
		parse_args(uri, args_start+1);
	}
}

void uri::parse_path(const std::string& uri, const size_t size)
{
	for(size_t start = 0, end; start < size; start = end+1)
	{
		end = uri.find('/', start);

		if(end > size)
			end = size;

		if(end-start > 0)
			path.push_back(uri.substr(start, end-start));
	}
}

void uri::parse_args(const std::string& uri, size_t start)
{
	for(size_t end; start < uri.size(); start = end + 1)
	{
		end = uri.find('&', start);

		if(end-start > 0)
			parse_arg(uri.substr(start, end-start));

		if(end == std::string::npos)
			break;
	}
}

void uri::parse_arg(const std::string& arg)
{
	size_t i = arg.find('=');
	if(i == std::string::npos)
		args[arg] = "";
	else
		args[arg.substr(0, i)] = arg.substr(i+1);
}

bool uri::match_path(const size_t i, const std::string& x) const
{
	return(path.size() > i && path[i] == x);
}

}
