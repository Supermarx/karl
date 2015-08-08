#include <karl/karl.hpp>

#include <iostream>

#include <karl/util/log.hpp>
#include <karl/similarity.hpp>

#include <supermarx/api/exception.hpp>
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

		reference<data::karluser> user_id(backend.add_karluser({name, password_salt, password_hashed}));

		log("karl::create_user", log::level_e::NOTICE)() << "Added user '" << name << "' [id:" << user_id << "]";
	}

	message::sessionticket karl::generate_sessionticket(const std::string &user)
	{
		qualified<data::karluser> ku([&]() { try {
			return backend.get_karluser_by_name(user);
		} catch(storage::not_found_error) {
			throw api::exception::authentication_error; // User does not exist
		}}());

		log("karl::generate_sessionticket", log::level_e::DEBUG)() << "Generating nonce";
		token nonce(api::random_token());
		data::sessionticket st({ku.id, nonce, datetime_now()});

		reference<data::sessionticket> ticket_id = backend.add_sessionticket(st);
		log("karl::generate_sessionticket", log::level_e::DEBUG)() << "Created sessionticket [id: " << ticket_id << "]";

		return {ticket_id, nonce, ku.data.password_salt};
	}

	message::sessiontoken karl::create_session(reference<data::sessionticket> sessionticket_id, token const& ticket_password)
	{
		qualified<data::sessionticket> sessionticket(backend.get_sessionticket(sessionticket_id));
		data::karluser user(backend.get_karluser(sessionticket.data.karluser_id));

		if((sessionticket.data.creation + time(0, 5, 0, 0)) < datetime_now()) // Max. 5 minutes have passed
			throw std::runtime_error("Sessionticket no longer valid"); // TODO proper exception

		if(check_perms)
		{
			log("karl::create_session", log::level_e::DEBUG)() << "Checking password";
			token ticket_password_known(api::hash(user.password_hashed, sessionticket.data.nonce));

			if(ticket_password != ticket_password_known)
				throw api::exception::authentication_error;
		}
		else
			log("karl::create_session", log::level_e::DEBUG)() << "Not checking password, as no-perms has been enabled";

		message::sessiontoken st(api::random_token());

		reference<data::session> session_id(backend.add_session({sessionticket.data.karluser_id, st, datetime_now()}));

		log("karl::create_session", log::level_e::DEBUG)() << "Session created, access granted [id: " << session_id << "]";

		return st;
	}

	void karl::check_session(message::sessiontoken const& token)
	{
		if(!check_perms)
			return;

		qualified<data::session> session([&]() { try {
			return backend.get_session_by_token(token);
		} catch(storage::not_found_error) {
			throw api::exception::session_invalid;
		}}());

		assert(session.data.token == token);

		if(session.data.creation + time(12, 0, 0, 0) < datetime_now())
			throw api::exception::session_invalid; // Session timeout

		log("karl::check_session", log::level_e::DEBUG)() << "Validated session [id: " << session.id << "]";
	}

	message::product_summary karl::get_product(const std::string &identifier, reference<data::supermarket> supermarket_id)
	{
		return backend.get_product(identifier, supermarket_id);
	}

	std::vector<message::product_summary> karl::get_products(std::string const& name, reference<data::supermarket> supermarket_id)
	{
		return backend.get_products_by_name(name, supermarket_id);
	}

	message::product_history karl::get_product_history(std::string const& identifier, reference<data::supermarket> supermarket_id)
	{
		return backend.get_product_history(identifier, supermarket_id);
	}

	std::vector<message::product_log> karl::get_recent_productlog(reference<data::supermarket> supermarket_id)
	{
		return backend.get_recent_productlog(supermarket_id);
	}

	void karl::add_product(reference<data::supermarket> supermarket_id, message::add_product const& ap)
	{
		log("karl::karl", log::level_e::DEBUG)() << "Received product " << ap.p.name << " [" << supermarket_id << "] [" << ap.p.identifier << "]";
		backend.add_product(supermarket_id, ap);
	}

	void karl::add_product_image_citation(reference<data::supermarket> supermarket_id, const std::string &product_identifier, const std::string &original_uri, const std::string &source_uri, const datetime &retrieved_on, raw const& image)
	{
		std::pair<int, int> orig_geo(ic.get_size(image));
		std::pair<int, int> new_geo(150, 150);

		data::imagecitation ic_obj({
			supermarket_id,
			original_uri,
			source_uri,
			orig_geo.first,
			orig_geo.second,
			retrieved_on
		});

		reference<data::imagecitation> ic_id(backend.add_image_citation(ic_obj));

		ic.commit(ic_id.unseal(), image, new_geo);

		backend.update_product_image_citation(product_identifier, supermarket_id, ic_id);
	}

	reference<data::tag> karl::find_add_tag(message::tag const& t)
	{
		if(t.category)
		{
			reference<data::tagcategory> tc_id(backend.find_add_tagcategory(*t.category));
			return backend.find_add_tag(t.name, tc_id);
		}
		else
			return backend.find_add_tag(t.name);
	}

	void karl::bind_tag(reference<data::productclass> productclass_id, reference<data::tag> tag_id)
	{
		backend.bind_tag(productclass_id, tag_id);
	}

	void karl::test()
	{
		id_t base_supermarket = 1;
		std::vector<id_t> slave_supermarkets = {2, 3, 4, 5};

		std::map<reference<data::supermarket>, std::vector<message::product_summary>> map_supermarket_products;

		auto fetch_supermarket_f([&](reference<data::supermarket> supermarket_id)
		{
			map_supermarket_products.emplace(std::make_pair(supermarket_id, backend.get_products(supermarket_id)));
		});

		fetch_supermarket_f(base_supermarket);
		for(reference<data::supermarket> slave_supermarket_id : slave_supermarkets)
			fetch_supermarket_f(slave_supermarket_id);

		auto const x_tup(map_supermarket_products.at(base_supermarket));
		for(message::product_summary const& xps : x_tup)
		{
			//message::product_summary xps(backend.get_product("wi210145", 1));

			std::cout << "Comparing " << xps.name << " " << xps.orig_price << " " << xps.volume << std::endl;

			for(id_t supermarket_id : slave_supermarkets)
			{
				std::cout << "Supermarket " << supermarket_id << std::endl;

				std::vector<message::product_summary> vps(backend.get_products(supermarket_id));

				typedef std::tuple<size_t, similarity::valuation, float> tup_t;
				std::vector<tup_t> rps;
				rps.reserve(vps.size());

				{
					size_t i = 0;
					for(auto const& yps : vps)
					{
						similarity::valuation v(similarity::exec(xps, yps));
						rps.emplace_back(i, v, v.collapse());
						++i;
					}
				}

				std::sort(rps.begin(), rps.end(), [](tup_t a, tup_t b) {
					return std::get<2>(a) > std::get<2>(b);
				});

				size_t i = 0;

				auto const& tup(rps.at(i));
				auto const& yps(vps.at(std::get<0>(tup)));

				if(xps.productclass_id == yps.productclass_id)
					continue;

				if(std::get<2>(tup) <= 0.5)
				{
					std::cout << "No match" << std::endl;
					continue;
				}

				std::cout << yps.name << " " << yps.orig_price << " " << yps.volume << " [" << std::get<2>(tup) << "]";

				for(float v : std::get<1>(tup).data)
					std::cout << " " << std::round(100.0f*v)/100.0f;

				std::cout << std::endl;

				backend.absorb_productclass(yps.productclass_id, xps.productclass_id);
			}
		}
	}
}
