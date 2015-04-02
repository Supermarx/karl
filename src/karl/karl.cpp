#include <karl/karl.hpp>

#include <iostream>

#include <karl/util/log.hpp>

namespace supermarx {
	karl::karl(const std::string &writable_directory)
		: backend(writable_directory)
	{}

	karl::karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db)
		: backend(host, user, password, db)
	{}

	std::vector<product> karl::get_products(std::string const& name, id_t supermarket_id)
	{
		return backend.get_products_by_name(name, supermarket_id);
	}

	void karl::add_product(product const& product, id_t supermarket_id)
	{
		log("karl::karl", log::level_e::DEBUG)() << "Received product " << product.name << " [" << supermarket_id << "] [" << product.identifier << "]";
		backend.add_product(product, supermarket_id);
	}
}
