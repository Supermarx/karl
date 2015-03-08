#pragma once

#include <vector>

#include <supermarx/product.hpp>

#include "storage.hpp"

namespace supermarx {
	//! The man who keeps an eye on all the prices.
	//! Karl abstracts from how products and prices are stored, and provides an interface for fetching, adding and removing products.
	struct Karl {
		//! Create Karl with a directory where it'll write its data.
		Karl(std::string const& writable_directory);

		//! Returns producs with this name
		std::vector<product> get_products(std::string const& name);
		void add_product(product const&);

	private:
		storage backend;
	};
}
