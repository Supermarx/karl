#pragma once

#include <vector>

#include <supermarx/product.hpp>

#include "storage.hpp"

namespace supermarx
{
	/* The man who keeps an eye on all the prices.
	 * Karl abstracts from how products and prices are stored, and provides an interface for fetching, adding and removing products.
	 */
	class karl
	{
	public:
		karl(std::string const& writable_directory);
		karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db);

		std::vector<product> get_products(std::string const& name);
		void add_product(product const&);

	private:
		storage backend;
	};
}
