#include <karl/api/response_handler.hpp>

#include <supermarx/serialization/xml_serializer.hpp>
#include <supermarx/serialization/msgpack_serializer.hpp>
#include <supermarx/serialization/msgpack_compact_serializer.hpp>
#include <supermarx/serialization/json_serializer.hpp>

#include <supermarx/serialization/msgpack_deserializer.hpp>

#include <supermarx/serialization/serialize_fusion.hpp>
#include <supermarx/serialization/deserialize_fusion.hpp>

#include <supermarx/api/add_product.hpp>
#include <supermarx/api/add_product_image_citation.hpp>

#include <karl/api/api_exception.hpp>
#include <karl/api/uri.hpp>

#include <karl/util/log.hpp>

#include <supermarx/util/guard.hpp>

namespace supermarx
{

void init_serializer(request& r, response_handler::serializer_ptr& s)
{
	const guard g([&]()
	{
		//Fall back to XML
		if(s == nullptr)
		{
			s.reset(new xml_serializer());
			r.write_header("Content-Type", "application/xml");
		}
	});

	const auto& format = r.env().gets.find("format");
	if(format == r.env().gets.end() || format->second == "xml")
	{
		s.reset(new xml_serializer());
		r.write_header("Content-Type", "application/xml");
	}
	else if(format->second == "json")
	{
		s.reset(new json_serializer());
		r.write_header("Content-Type", "application/json");
	}
	else if(format->second == "msgpack")
		s.reset(new msgpack_serializer());
	else if(format->second == "msgpack_compact")
		s.reset(new msgpack_compact_serializer());
	else
		throw api_exception::format_unknown;
}

bool url_decode(const std::string& in, std::string& out)
{
	out.clear();
	out.reserve(in.size());

	for(std::size_t i = 0; i < in.size(); ++i)
	{
		if(in[i] == '%')
		{
			if(i + 3 <= in.size())
			{
				int value = 0;
				std::istringstream is(in.substr(i + 1, 2));

				if(is >> std::hex >> value)
				{
					out += static_cast<char>(value);
					i += 2;
				}
				else
					return false;
			}
			else
				return false;
		}
		else if(in[i] == '+')
			out += ' ';
		else
			out += in[i];
	}

	return true;
}

void write_exception(request& r, response_handler::serializer_ptr& s, api_exception e)
{
	s->clear(); //Clear any previous content

	r.write_header("Status", api_exception_status(e));

	s->write_object("exception", 3);
	s->write("code", e);
	s->write("message", api_exception_message(e));
	s->write("help", "http://supermarx.nl/docs/api_exception/" + boost::lexical_cast<std::string>(e) + "/");
}

std::string fetch_payload(const request& r)
{
	const auto payload_itr = r.env().posts.find("payload");
	if(payload_itr == r.env().posts.end())
		throw api_exception::payload_expected;

	return payload_itr->second.value;
}

template<typename T>
T deserialize_payload(const request& r, const std::string& name)
{
	std::unique_ptr<deserializer> d(new msgpack_deserializer);
	d->feed(fetch_payload(r));

	return deserialize<T>(d, name);
}

template<typename T>
void package(response_handler::serializer_ptr& s, const T& x, const std::string& name)
{
	serialize<T>(s, name, x);
}

bool process(request& r, response_handler::serializer_ptr& s, karl& k, const uri& u)
{
	if(u.match_path(0, "get_product"))
	{
		if(u.path.size() != 3)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		std::string identifier = u.path[2];

		serialize(s, "product_summary", k.get_product(identifier, supermarket_id));
		return true;
	}

	if(u.match_path(0, "find_products"))
	{
		if(u.path.size() != 3)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		std::string name = u.path[2];

		serialize(s, "products", k.get_products(name, supermarket_id));
		return true;
	}

	if(u.match_path(0, "add_product"))
	{
		if(u.path.size() != 2)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		api::add_product request = deserialize_payload<api::add_product>(r, "add_product");

		k.add_product(request.p, supermarket_id, request.retrieved_on, request.c, request.problems);
		s->write_object("response", 1);
		s->write("status", std::string("done"));
		return true;
	}

	if(u.match_path(0, "add_product_image_citation"))
	{
		if(u.path.size() != 3)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		std::string product_identifier = u.path[2];

		api::add_product_image_citation request = deserialize_payload<api::add_product_image_citation>(r, "add_product_image_citation");

		k.add_product_image_citation(supermarket_id, product_identifier, request.original_uri, request.source_uri, request.retrieved_on, request.image);
		s->write_object("response", 1);
		s->write("status", std::string("done"));
		return true;
	}

	if(u.match_path(0, "get_product_history"))
	{
		if(u.path.size() != 3)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		std::string identifier = u.path[2];

		try
		{
			serialize(s, "product_history", k.get_product_history(identifier, supermarket_id));
		} catch(storage::not_found_error)
		{
			throw api_exception::product_not_found;
		}

		return true;
	}

	return false;
}

void response_handler::respond(request& r, karl& k)
{
	r.write_header("Server", "karl/0.1");

	// Decode url to path.
	std::string request_path;
	if(!url_decode(r.env().requestUri, request_path))
	{
		r.write_header("Status", "400 Bad Request");
		r.write_endofheader();
		return;
	}

	// Request path must be absolute and not contain "..".
	if(request_path.empty() || request_path[0] != '/' || request_path.find("..") != std::string::npos)
	{
		r.write_header("Status", "400 Bad Request");
		r.write_endofheader();
		r.write_text("400 bad request");
		return;
	}

	uri u(request_path);

	serializer_ptr s(nullptr);

	try
	{
		init_serializer(r, s);

		if(process(r, s, k, u))
			r.write_header("Status", "200 OK");
		else
			throw api_exception::path_unknown;
	}
	catch(api_exception e)
	{
		log("api::response_handler", log::WARNING)() << "api_exception - " << api_exception_message(e) << " (" << e << ")";

		write_exception(r, s, e);
	}
	catch(std::exception& e)
	{
		log("api::response_handler", log::ERROR)() << "Uncaught exception: " << e.what();

		write_exception(r, s, api_exception::unknown);
	}
	catch( ... )
	{
		log("api::response_handler", log::ERROR)() << "Unknown exception";

		write_exception(r, s, api_exception::unknown);
	}

	r.write_endofheader();
	s->dump([&](const char* data, size_t size){ r.write_bytes(data, size); });
}

}
