#pragma once

#include <vector>

#include <supermarx/id_t.hpp>

#include <supermarx/message/add_product.hpp>
#include <supermarx/message/session.hpp>

#include <karl/storage.hpp>
#include <karl/image_citations.hpp>

namespace supermarx
{
	/* The man who keeps an eye on all the prices.
	 * Karl abstracts from how products and prices are stored, and provides an interface for fetching, adding and removing products.
	 */
	class karl
	{
	public:
		karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db, const std::string& imagecitation_path, bool check_perms);

		bool check_permissions() const;

		void create_user(std::string const& name, std::string const& password);
		message::sessionticket generate_sessionticket(std::string const& user);
		message::sessiontoken create_session(reference<data::sessionticket> sessionticket_id, token const& ticket_password);
		void check_session(message::sessiontoken const& token);

		message::product_summary get_product(std::string const& identifier, reference<data::supermarket> supermarket_id);
		std::vector<message::product_summary> get_products(std::string const& name, reference<data::supermarket> supermarket_id);
		message::product_history get_product_history(std::string const& identifier, reference<data::supermarket> supermarket_id);
		std::vector<message::product_log> get_recent_productlog(reference<data::supermarket> supermarket_id);

		void add_product(reference<data::supermarket> supermarket_id, message::add_product const& ap);
		void add_product_image_citation(reference<data::supermarket> supermarket_id, std::string const& product_identifier, std::string const& original_uri, std::string const& source_uri, const datetime &retrieved_on, raw const& image);

		void absorb_productclass(reference<data::productclass> src_productclass_id, reference<data::productclass> dest_productclass_id);

		reference<data::tag> find_add_tag(message::tag const& t);
		void bind_tag(reference<data::productclass> productclass_id, reference<data::tag> tag_id);

	private:
		storage backend;
		image_citations ic;
		bool check_perms;
	};
}
