#include <karl/karl.hpp>

#include <iostream>

#include <karl/util/log.hpp>

namespace supermarx {
	karl::karl(std::string const& host, std::string const& user, std::string const& password, const std::string& db, const std::string& imagecitation_path)
		: backend(host, user, password, db)
		, ic(imagecitation_path)
	{}

	api::product_summary karl::get_product(const std::string &identifier, id_t supermarket_id)
	{
		return backend.get_product(identifier, supermarket_id);
	}

	std::vector<api::product_summary> karl::get_products(std::string const& name, id_t supermarket_id)
	{
		return backend.get_products_by_name(name, supermarket_id);
	}

	api::product_history karl::get_product_history(std::string const& identifier, id_t supermarket_id)
	{
		return backend.get_product_history(identifier, supermarket_id);
	}

	std::vector<api::product_log> karl::get_recent_productlog(id_t supermarket_id)
	{
		return backend.get_recent_productlog(supermarket_id);
	}

	void karl::add_product(product const& product, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems)
	{
		log("karl::karl", log::level_e::DEBUG)() << "Received product " << product.name << " [" << supermarket_id << "] [" << product.identifier << "]";
		backend.add_product(product, supermarket_id, retrieved_on, conf, problems);
	}

	void karl::add_product_image_citation(id_t supermarket_id, const std::string &product_identifier, const std::string &original_uri, const std::string &source_uri, const datetime &retrieved_on, raw const& image)
	{
		std::pair<size_t, size_t> orig_geo(ic.get_size(image));
		std::pair<size_t, size_t> new_geo(150, 150);

		id_t ic_id = backend.add_image_citation(
			supermarket_id,
			original_uri,
			source_uri,
			orig_geo.first,
			orig_geo.second,
			retrieved_on
		);

		ic.commit(ic_id, image, new_geo);

		backend.update_product_image_citation(product_identifier, supermarket_id, ic_id);
	}
}
