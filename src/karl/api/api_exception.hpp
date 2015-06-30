#pragma once

#include <string>
#include <supermarx/api/exception.hpp>

namespace supermarx
{

inline std::string api_exception_message(api::exception e)
{
	using namespace api;

	switch(e)
	{
	case unknown:
	case state_unexpected:
		return "Something unexpected happened; please file a bug report.";
	case backend_down:
		return "Unable to provide the requested service (backend down).";
	case path_unknown:
		return "Path not found.";
	case format_unknown:
		return "Format not supported.";
	case product_not_found:
		return "Product not found.";
	case payload_expected:
		return "Expected a payload in the POST request, but did not receive any.";
	case session_expected:
		return "Expected a session token in the POST request, but did not receive any.";
	case authentication_error:
		return "Your username or password is invalid; please contact the service provider.";
	case session_invalid:
		return "Session does not exist (here), or is no longer valid.";
	}
}

inline std::string api_exception_status(api::exception e)
{
	using namespace api;

	switch(e)
	{
	case unknown:
	case backend_down:
	case state_unexpected:
		return "500 Internal Server Error";
	case path_unknown:
	case product_not_found:
		return "404 Not Found";
	case format_unknown:
	case payload_expected:
	case session_expected:
		return "400 Bad Request";
	case authentication_error:
	case session_invalid:
		return "403 Forbidden";
	default:
		return "500 Internal Server Error";
	}
}

}
