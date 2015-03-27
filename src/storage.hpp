#pragma once

#include <map>
#include <boost/optional.hpp>
#include <rusql/database.hpp>

#include <supermarx/product.hpp>

namespace supermarx
{
class storage
{
public:
	typedef uint64_t id_t;

private:
	std::shared_ptr<rusql::Database> database;

	enum class statement
	{
		add_product,
		get_product_by_identifier,

		add_productrecord,
		get_productrecord_by_name,
		invalidate_productrecords
	};

	std::map<statement, std::shared_ptr<rusql::PreparedStatement>> prepared_statements;

public:
	storage(std::string const& embedded_dir);
	storage(std::string const& host, std::string const& user, std::string const& password, const std::string& db);
	~storage();

	void add_product(product const& p);
	std::vector<product> get_products_by_name(std::string const& name);

private:
	void lock_products_read();
	void lock_products_write();
	void unlock_all();

	boost::optional<id_t> find_product_unsafe(std::string const& identifier, id_t supermarket_id);
	id_t find_add_product(std::string const& identifier, id_t supermarket_id);

private:
	// This function is called after the connection to the database is made,
	// and can be used to update tables to a new version, that kind of stuff.
	void update_database_schema();
	void prepare_statements();
};
}
