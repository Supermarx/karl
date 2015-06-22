#pragma once

#include <vector>

#include <supermarx/id_t.hpp>
#include <supermarx/product.hpp>

#include <supermarx/api/product_summary.hpp>
#include <supermarx/api/product_log.hpp>
#include <supermarx/api/product_history.hpp>
#include <supermarx/api/session.hpp>

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
		api::sessionticket generate_sessionticket(std::string const& user);
		api::sessiontoken create_session(id_t sessionticket_id, token const& ticket_password);
		void check_session(api::sessiontoken const& token);

		api::product_summary get_product(std::string const& identifier, id_t supermarket_id);
		std::vector<api::product_summary> get_products(std::string const& name, id_t supermarket_id);
		api::product_history get_product_history(std::string const& identifier, id_t supermarket_id);
		std::vector<api::product_log> get_recent_productlog(id_t supermarket_id);
		void add_product(product const&, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems);
		void add_product_image_citation(id_t supermarket_id, std::string const& product_identifier, std::string const& original_uri, std::string const& source_uri, const datetime &retrieved_on, raw const& image);

		void absorb_productclass(id_t src_productclass_id, id_t dest_productclass_id);

		id_t find_add_tag(api::tag const& t);
		void bind_tag(id_t productclass_id, id_t tag_id);

	private:
		storage backend;
		image_citations ic;
		bool check_perms;
	};
}
