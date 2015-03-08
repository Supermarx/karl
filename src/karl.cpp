#include "karl.hpp"

#include <iostream>

#include <supermarx/product.hpp>

namespace supermarx {
	Karl::Karl(const std::string &writable_directory)
		: backend(writable_directory)
	{
	}

	std::vector<product> Karl::get_products(std::string const& name){
		return backend.get_products_by_name(name);
	}

	void Karl::add_product(product const& product){
		std::cout << "Adding " << product.name << " for " << product.price_in_cents << std::endl;
		backend.add_product(product);
	}
}
