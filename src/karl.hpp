#pragma once

#include <vector>

#include <supermarx/product.hpp>

namespace supermarx {
	// The man who keeps an eye on all the prices. Basically the database.
	struct Karl {
		void add_product(Product const&);

		std::vector<Product> products;
	};

	namespace karl {
		void do_stuff();
	}
}
