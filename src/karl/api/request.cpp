#include <karl/api/request.hpp>

#include <karl/api/response_handler.hpp>
#include <karl/api/api_exception.hpp>

#include <karl/util/log.hpp>

#include <supermarx/util/timer.hpp>

namespace supermarx
{

fcgi_request::fcgi_request(karl& _k)
	: Request()
	, k(_k)
{}

bool fcgi_request::response()
{
	timer t;

	request r(*this);
	try
	{
		response_handler::respond(r, k);
	}
	catch(api::exception e)
	{
		log("api::fcgi_request", log::WARNING)() << "api_exception - " << api_exception_message(e) << " (" << e << ")";
	}
	catch(std::exception& e)
	{
		log("api::fcgi_request", log::ERROR)() << "Uncaught exception: " << e.what();
	}
	catch ( ... )
	{
		log("api::fcgi_request", log::ERROR)() << "Uncaught unexpected object";
	}

	log("api::fcgi_request", log::NOTICE)() << environment().requestUri << " [" << t.diff_msec().count() << "µs]";

	return true;
}

request::request(fcgi_request &parent)
	: fcgi(parent)
	, state(state_e::init)
{}

void request::write_header(const std::string key, const std::string value)
{
	if(state == state_e::init)
		state = state_e::header;
	else if(state == state_e::header)
		fcgi.out << "\n";
	else
		throw api::exception::state_unexpected;

	fcgi.out << key << ": " << value;
}

void request::write_endofheader()
{
	if(state != state_e::header)
		throw api::exception::state_unexpected;

	state = state_e::body;
	fcgi.out << "\r\n\r\n";
}

void request::write_text(const std::string& str) const
{
	if(state != state_e::body)
		throw api::exception::state_unexpected;

	fcgi.out << str;
}

void request::write_bytes(const char *data, size_t size) const
{
	if(state != state_e::body)
		throw api::exception::state_unexpected;

	fcgi.out.dump(data, size);
}

const request::env_t& request::env() const
{
	return fcgi.environment();
}

}
