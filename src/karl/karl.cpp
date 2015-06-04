#include <karl/karl.hpp>

#include <iostream>
#include <sodium.h>

#include <karl/util/log.hpp>

#include <karl/api/api_exception.hpp>
#include <supermarx/api/session_operations.hpp>

namespace supermarx {
	karl::karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db, const std::string& imagecitation_path, bool _check_perms)
		: backend(host, user, password, db)
		, ic(imagecitation_path)
		, check_perms(_check_perms)
	{}

	bool karl::check_permissions() const
	{
		return check_perms;
	}

	void karl::create_user(const std::string &name, const std::string &password)
	{
		log("karl::create_user", log::level_e::DEBUG)() << "Generating token";
		token password_salt(api::random_token());
		log("karl::create_user", log::level_e::DEBUG)() << "Hashing password";
		token password_hashed(api::hash(password, password_salt));
		log("karl::create_user", log::level_e::DEBUG)() << "Creating user";

		id_t user_id = backend.add_karluser({name, password_salt, password_hashed});

		log("karl::create_user", log::level_e::NOTICE)() << "Added user '" << name << "' [id:" << user_id << "]";
	}

	api::sessionticket karl::generate_sessionticket(const std::string &user)
	{
		std::pair<id_t, karluser> pair([&]() { try {
			return backend.get_karluser_by_name(user);
		} catch(storage::not_found_error) {
			throw api_exception::authentication_error; // User does not exist
		}}());

		log("karl::generate_sessionticket", log::level_e::DEBUG)() << "Generating nonce";
		token nonce(api::random_token());
		sessionticket st({pair.first, nonce, datetime_now()});

		id_t ticket_id = backend.add_sessionticket(st);
		log("karl::generate_sessionticket", log::level_e::DEBUG)() << "Created sessionticket [id: " << ticket_id << "]";

		return {ticket_id, nonce, pair.second.password_salt};
	}

	api::sessiontoken karl::create_session(id_t sessionticket_id, token const& ticket_password)
	{
		std::pair<id_t, sessionticket> sessionticket_p(backend.get_sessionticket(sessionticket_id));
		karluser user(backend.get_karluser(sessionticket_p.second.karluser_id));

		if((sessionticket_p.second.creation + time(0, 5, 0, 0)) < datetime_now()) // Max. 5 minutes have passed
			throw std::runtime_error("Sessionticket no longer valid"); // TODO proper exception

		if(check_perms)
		{
			log("karl::create_session", log::level_e::DEBUG)() << "Checking password";
			token ticket_password_known(api::hash(user.password_hashed, sessionticket_p.second.nonce));

			if(ticket_password != ticket_password_known)
				throw api_exception::authentication_error;
		}
		else
			log("karl::create_session", log::level_e::DEBUG)() << "Not checking password, as no-perms has been enabled";

		api::sessiontoken st(api::random_token());

		id_t session_id = backend.add_session({sessionticket_p.second.karluser_id, st, datetime_now()});

		log("karl::create_session", log::level_e::DEBUG)() << "Session created, access granted [id: " << session_id << "]";

		return st;
	}

	void karl::check_session(api::sessiontoken const& token)
	{
		if(!check_perms)
			return;

		std::pair<id_t, session> s_p([&]() { try {
			return backend.get_session_by_token(token);
		} catch(storage::not_found_error) {
			throw api_exception::session_invalid;
		}}());

		assert(s_p.second.token == token);

		if(s_p.second.creation + time(6, 0, 0, 0) < datetime_now())
			throw api_exception::session_invalid; // Session timeout

		log("karl::check_session", log::level_e::DEBUG)() << "Validated session [id: " << s_p.first << "]";
	}

	api::product_summary karl::get_product(const std::string &identifier, id_t supermarket_id)
	{
		return backend.get_product(identifier, supermarket_id);
	}

	std::vector<api::product_summary> karl::get_products(std::string const& name, id_t supermarket_id)
	{
		return backend.get_products_by_name(name, supermarket_id);
	}

	api::product_history karl::get_product_history(std::string const& identifier, id_t supermarket_id)
	{
		return backend.get_product_history(identifier, supermarket_id);
	}

	std::vector<api::product_log> karl::get_recent_productlog(id_t supermarket_id)
	{
		return backend.get_recent_productlog(supermarket_id);
	}

	void karl::add_product(product const& product, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems)
	{
		log("karl::karl", log::level_e::DEBUG)() << "Received product " << product.name << " [" << supermarket_id << "] [" << product.identifier << "]";
		backend.add_product(product, supermarket_id, retrieved_on, conf, problems);
	}

	void karl::add_product_image_citation(id_t supermarket_id, const std::string &product_identifier, const std::string &original_uri, const std::string &source_uri, const datetime &retrieved_on, raw const& image)
	{
		std::pair<size_t, size_t> orig_geo(ic.get_size(image));
		std::pair<size_t, size_t> new_geo(150, 150);

		id_t ic_id = backend.add_image_citation(
			supermarket_id,
			original_uri,
			source_uri,
			orig_geo.first,
			orig_geo.second,
			retrieved_on
		);

		ic.commit(ic_id, image, new_geo);

		backend.update_product_image_citation(product_identifier, supermarket_id, ic_id);
	}
}
