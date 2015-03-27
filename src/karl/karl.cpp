#include "karl.hpp"

#include <iostream>

namespace supermarx {
	karl::karl(const std::string &writable_directory)
		: backend(writable_directory)
	{
	}

	karl::karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db)
		: backend(host, user, password, db)
	{}

	std::vector<product> karl::get_products(std::string const& name){
		return backend.get_products_by_name(name);
	}

	void karl::add_product(product const& product){
		backend.add_product(product);
	}
}
