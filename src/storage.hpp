#pragma once

#include <map>

#include <rusql/database.hpp>

#include <supermarx/product.hpp>

namespace supermarx
{
class storage
{
private:
	std::shared_ptr<rusql::Database> database;

	enum class statement
	{
		add_product,
		get_product_by_name
	};

	std::map<statement, std::shared_ptr<rusql::PreparedStatement>> prepared_statements;

public:
	storage(std::string const& embedded_dir);
	~storage();

	void add_product(product const& p);
	std::vector<product> get_products_by_name(std::string const& name);

private:
	// This function is called after the connection to the database is made,
	// and can be used to update tables to a new version, that kind of stuff.
	void update_database_schema();
	void prepare_statements();
};
}
