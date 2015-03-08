#include "storage.hpp"

namespace supermarx
{

storage::storage(std::string const& embedded_dir)
{
	const char *server_options[] = \
	{"test", "--innodb=OFF", "-h", embedded_dir.c_str(), NULL};
	int num_options = (sizeof(server_options)/sizeof(char*)) - 1;
	mysql_library_init(num_options, const_cast<char**>(server_options), NULL);

	{
		auto db = std::make_shared<rusql::Database>(rusql::Database::ConstructionInfo());
		auto result = db->query("show databases");
		bool found_karl = false;
		while(result){
			if(result.get<std::string>("Database") == "karl"){
				found_karl = true;
			}
			result.next();
		}
		if(!found_karl){
			db->execute("create database karl");
		}
	}

	database = std::make_shared<rusql::Database>(rusql::Database::ConstructionInfo("karl"));

	update_database_schema();
	prepare_statements();
}

storage::~storage()
{
	prepared_statements.clear();
	database.reset();
	mysql_library_end();
}

void storage::add_product(product const& p)
{
	prepared_statements.at(statement::add_product)->execute(p.name, p.price_in_cents);
}

std::vector<product> storage::get_products_by_name(std::string const& name)
{
	rusql::PreparedStatement& query = *prepared_statements.at(statement::get_product_by_name);
	query.execute(name);

	std::vector<product> products;
	product row;
	query.bind_results(row.name, row.price_in_cents);
	while(query.fetch()){
		products.push_back(row);
	}

	return products;
}

void storage::update_database_schema()
{
	bool products_found = false;
	auto result = database->query("show tables");
	while(result){
		if(result.get<std::string>(0) == "products"){
			products_found = true;
			break;
		}
		result.next();
	}

	if(!products_found){
		database->execute("create table products (product_name varchar(1024), price_in_cents decimal(10,2))");
	}
}

void storage::prepare_statements()
{
	auto create_statement =
			[&](statement const& s, std::string const& query)
	{
		prepared_statements[s] = std::make_shared<rusql::PreparedStatement>(database->prepare(query));
	};

	create_statement(statement::add_product, "insert into products (product_name, price_in_cents) values(?, ?)");
	create_statement(statement::get_product_by_name, "select * from products where product_name = ?");
}

}
