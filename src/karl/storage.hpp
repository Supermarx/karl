#pragma once

#include <map>
#include <pqxx/pqxx>
#include <boost/optional.hpp>

#include <supermarx/id_t.hpp>
#include <supermarx/product.hpp>
#include <supermarx/api/product_summary.hpp>

namespace supermarx
{
class storage
{
private:
	pqxx::connection conn;

public:
	storage(std::string const& host, std::string const& user, std::string const& password, const std::string& db);
	~storage();

	void add_product(product const& p, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems);
	std::vector<product> get_products_by_name(std::string const& name, id_t supermarket_id);
	boost::optional<api::product_summary> get_product_summary(std::string const& identifier, id_t supermarket_id);

private:
	void update_database_schema();
	void prepare_statements();
};
}
