#include <karl/api/response_handler.hpp>

#include <supermarx/serialization/xml_serializer.hpp>
#include <supermarx/serialization/msgpack_serializer.hpp>
#include <supermarx/serialization/msgpack_compact_serializer.hpp>
#include <supermarx/serialization/json_serializer.hpp>

#include <supermarx/serialization/msgpack_deserializer.hpp>

#include <supermarx/serialization/serialize_fusion.hpp>
#include <supermarx/serialization/deserialize_fusion.hpp>

#include <supermarx/message/add_product.hpp>
#include <supermarx/message/add_product_image_citation.hpp>
#include <supermarx/message/exception.hpp>

#include <supermarx/api/exception.hpp>

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
			r.write_header("Content-Type", "application/xml; charset=UTF-8");
		}
	});

	const auto& format = r.env().gets.find("format");
	if(format == r.env().gets.end() || format->second == "xml")
	{
		s.reset(new xml_serializer());
		r.write_header("Content-Type", "application/xml; charset=UTF-8");
	}
	else if(format->second == "json")
	{
		s.reset(new json_serializer());
		r.write_header("Content-Type", "application/json; charset=UTF-8");
	}
	else if(format->second == "msgpack")
		s.reset(new msgpack_serializer());
	else if(format->second == "msgpack_compact")
		s.reset(new msgpack_compact_serializer());
	else
		throw api::exception::format_unknown;
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

std::string fetch_payload(const request& r)
{
	const auto payload_itr = r.env().posts.find("payload");
	if(payload_itr == r.env().posts.end())
		throw api::exception::payload_expected;

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

void write_exception(request& r, response_handler::serializer_ptr& s, api::exception e)
{
	s->clear(); //Clear any previous content

	r.write_header("Status", api::exception_status(e));

	package(s, message::exception{
		e,
		api::exception_message(e),
		"http://supermarx.nl/docs/api_exception/" + boost::lexical_cast<std::string>(e) + "/"
	}, "exception");
}

void require_permissions(request const& r, karl& k)
{
	if(!k.check_permissions())
		return;

	auto const& stok(r.env().posts.find("sessiontoken"));

	if(stok == r.env().posts.end())
		throw api::exception::session_expected;

	k.check_session(supermarx::to_token(stok->second.value));
}

bool process(request& r, response_handler::serializer_ptr& s, karl& k, const uri& u)
{
	if(u.match_path(0, "get_tags"))
	{
		if(u.path.size() != 1)
			return false;

		serialize(s, "tags", k.get_tags());

		return true;
	}

	if(u.match_path(0, "get_productclass"))
	{
		if(u.path.size() != 2)
			return false;

		id_t productclass_id = boost::lexical_cast<id_t>(u.path[1]);

		try
		{
			serialize(s, "productclass_summary", k.get_productclass(productclass_id));
		} catch(storage::not_found_error)
		{
			throw api::exception::productclass_not_found;
		}
		return true;
	}

	if(u.match_path(0, "get_product"))
	{
		if(u.path.size() != 3)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		std::string identifier = u.path[2];

		try
		{
			serialize(s, "product_summary", k.get_product(identifier, supermarket_id));
		} catch(storage::not_found_error)
		{
			throw api::exception::product_not_found;
		}
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
		require_permissions(r, k);

		if(u.path.size() != 2)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		message::add_product request = deserialize_payload<message::add_product>(r, "add_product");

		k.add_product(supermarket_id, request);
		s->write_object("response", 1);
		s->write("status", std::string("done"));
		return true;
	}

	if(u.match_path(0, "add_product_image_citation"))
	{
		require_permissions(r, k);

		if(u.path.size() != 3)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		std::string product_identifier = u.path[2];

		message::add_product_image_citation request = deserialize_payload<message::add_product_image_citation>(r, "add_product_image_citation");

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
			throw api::exception::product_not_found;
		}

		return true;
	}

	if(u.match_path(0, "get_recent_productlog"))
	{
		if(u.path.size() != 2)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);

		serialize(s, "products", k.get_recent_productlog(supermarket_id));
		return true;
	}

	if(u.match_path(0, "bind_tag"))
	{
		require_permissions(r, k);

		if(u.path.size() != 3)
			return false;

		id_t supermarket_id = boost::lexical_cast<id_t>(u.path[1]);
		std::string product_identifier = u.path[2];

		message::tag request = deserialize_payload<message::tag>(r, "tag");

		message::product_summary ps(k.get_product(product_identifier, supermarket_id));
		reference<data::tag> tag_id(k.find_add_tag(request));
		k.bind_tag(ps.productclass_id, tag_id);

		s->write_object("response", 1);
		s->write("status", std::string("done"));

		return true;
	}

	if(u.match_path(0, "create_sessionticket"))
	{
		if(u.path.size() != 2)
			return false;

		std::string username = u.path[1];
		serialize(s, "sessionticket", k.generate_sessionticket(username));
		return true;
	}

	if(u.match_path(0, "login"))
	{
		if(u.path.size() != 2)
			return false;

		id_t sessiontoken_id = boost::lexical_cast<id_t>(u.path[1]);
		auto const& password_hashed_post(r.env().posts.find("password_hashed"));

		if(password_hashed_post == r.env().posts.end())
			return false;

		token password_hashed(to_token(password_hashed_post->second.value));
		message::sessiontoken stok(k.create_session(sessiontoken_id, password_hashed));

		serialize(s, "sessiontoken", stok);
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
			throw api::exception::path_unknown;
	}
	catch(api::exception e)
	{
		log("api::response_handler", log::WARNING)() << "api_exception - " << api::exception_message(e) << " (" << e << ")";

		write_exception(r, s, e);
	}
	catch(std::exception& e)
	{
		log("api::response_handler", log::ERROR)() << "Uncaught exception: " << e.what();

		write_exception(r, s, api::exception::unknown);
	}
	catch(storage::not_found_error)
	{
		log("api::response_handler", log::ERROR)() << "Uncaught storage::not_found_error";

		write_exception(r, s, api::exception::unknown);
	}

	catch( ... )
	{
		log("api::response_handler", log::ERROR)() << "Unknown exception";

		write_exception(r, s, api::exception::unknown);
	}

	r.write_endofheader();
	s->dump([&](const char* data, size_t size){ r.write_bytes(data, size); });
}

}
