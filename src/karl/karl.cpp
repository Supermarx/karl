#include <karl/karl.hpp>

#include <iostream>

#include <karl/util/log.hpp>

namespace supermarx {
	karl::karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db)
		: backend(host, user, password, db)
	{}

	std::vector<product> karl::get_products(std::string const& name, id_t supermarket_id)
	{
		return backend.get_products_by_name(name, supermarket_id);
	}

	boost::optional<api::product_summary> karl::get_product_summary(std::string const& identifier, id_t supermarket_id)
	{
		return backend.get_product_summary(identifier, supermarket_id);
	}

	void karl::add_product(product const& product, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems)
	{
		log("karl::karl", log::level_e::DEBUG)() << "Received product " << product.name << " [" << supermarket_id << "] [" << product.identifier << "]";
		backend.add_product(product, supermarket_id, retrieved_on, conf, problems);
	}
}
