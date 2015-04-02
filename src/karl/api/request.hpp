#pragma once

#include <fastcgi++/request.hpp>
#include <karl/karl.hpp>

namespace supermarx
{

class fcgi_request : public Fastcgipp::Request<char>
{
	karl& k;

public:
	fcgi_request(fcgi_request&) = delete;
	void operator=(fcgi_request&) = delete;

	fcgi_request(karl& k);

	bool response();
};

class request
{
public:
	typedef Fastcgipp::Http::Environment<char> env_t;

private:
	enum class state_e
	{
		init,
		header,
		body,
		done
	};

	fcgi_request& fcgi;
	state_e state;

public:
	request(request&) = delete;
	void operator=(request&) = delete;

	request(fcgi_request& parent);

	void write_header(const std::string key, const std::string value);
	void write_endofheader();
	void write_text(const std::string& str) const;
	void write_bytes(const char* data, size_t size) const;

	const env_t& env() const;
};

}
