#pragma once

#include <vector>

#include <supermarx/id_t.hpp>
#include <supermarx/product.hpp>

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
		karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db, const std::string& imagecitation_path);

		api::product_summary get_product(std::string const& identifier, id_t supermarket_id);
		std::vector<api::product_summary> get_products(std::string const& name, id_t supermarket_id);
		api::product_history get_product_history(std::string const& identifier, id_t supermarket_id);
		void add_product(product const&, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems);
		void add_product_image_citation(id_t supermarket_id, std::string const& product_identifier, std::string const& original_uri, std::string const& source_uri, const datetime &retrieved_on, raw const& image);

	private:
		storage backend;
		image_citations ic;
	};
}
