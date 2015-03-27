#include "karl.hpp"

#include <iostream>

namespace supermarx {
	Karl::Karl(const std::string &writable_directory)
		: backend(writable_directory)
	{
	}

	Karl::Karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db)
		: backend(host, user, password, db)
	{}

	std::vector<product> Karl::get_products(std::string const& name){
		return backend.get_products_by_name(name);
	}

	void Karl::add_product(product const& product){
		std::cout << "Adding " << product.name << " for " << product.price << std::endl;
		backend.add_product(product);
	}
}
