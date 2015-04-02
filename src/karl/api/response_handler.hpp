#pragma once

#include <karl/karl.hpp>
#include <karl/api/request.hpp>

#include <supermarx/serialization/serializer.hpp>

namespace supermarx
{

class response_handler
{
public:
	typedef std::unique_ptr<serializer> serializer_ptr;

	response_handler() = delete;
	response_handler(response_handler&) = delete;
	void operator=(response_handler&) = delete;

	static void respond(request& r, karl& k);
};

}
