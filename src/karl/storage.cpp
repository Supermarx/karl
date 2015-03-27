#include "storage.hpp"

#include "guard.hpp"

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

		auto result = db->select_query("show databases");
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

storage::storage(const std::string &host, const std::string &user, const std::string &password, const std::string& db)
	: database(std::make_shared<rusql::Database>(rusql::Database::ConstructionInfo(host, 3306, user, password, db)))
	, prepared_statements()
{
	update_database_schema();
	prepare_statements();
}

storage::~storage()
{
	prepared_statements.clear();
	database.reset();
	mysql_library_end();
}

inline std::string to_string(condition cond)
{
	switch(cond)
	{
	case condition::ALWAYS:
		return "ALWAYS";
	case condition::AT_TWO:
		return "AT_TWO";
	case condition::AT_THREE:
		return "AT_THREE";
	}
}

inline condition to_condition(std::string const& str)
{
	if(str == "ALWAYS")
		return condition::ALWAYS;
	else if(str == "AT_TWO")
		return condition::AT_TWO;
	else if(str == "AT_THREE")
		return condition::AT_THREE;

	throw std::runtime_error("Could not parse condition");
}

void storage::lock_products_read()
{
	// LibRUSQL does not support grabbing a specific connection (yet), do not lock for now
	//database->query("lock tables product read");
}

void
storage::lock_products_write()
{
	// LibRUSQL does not support grabbing a specific connection (yet), do not lock for now
	//database->query("lock tables product write");
}

void storage::unlock_all()
{
	// LibRUSQL does not support grabbing a specific connection (yet), do not lock for now
	//database->query("unlock tables");
}

boost::optional<storage::id_t> storage::find_product_unsafe(std::string const& identifier, id_t supermarket_id)
{
	auto& qselect = *prepared_statements.at(statement::get_product_by_identifier);
	qselect.execute(identifier, supermarket_id);

	id_t product_id;
	qselect.bind_results(product_id);

	if(qselect.fetch())
		return product_id;

	return boost::none;
}

storage::id_t storage::find_add_product(std::string const& identifier, id_t supermarket_id)
{
	{
		lock_products_read();
		guard unlocker([&]() {
			unlock_all();
		});

		auto product_id = find_product_unsafe(identifier, supermarket_id);
		if(product_id)
			return product_id.get();
	}

	{
		lock_products_write();
		guard unlocker([&]() {
			unlock_all();
		});

		auto product_id = find_product_unsafe(identifier, supermarket_id);
		if(product_id)
			return product_id.get();

		auto& qinsert = *prepared_statements.at(statement::add_product);
		qinsert.execute(identifier, supermarket_id);
		return qinsert.insert_id();
	}
}

void storage::add_product(product const& p)
{
	id_t product_id = find_add_product(p.identifier, 1); //TODO supermarket_id stub

	//TODO check if a newer entry is already entered. Perhaps invalidate that entry in such a case.
	auto& qinvalidate = *prepared_statements.at(statement::invalidate_productrecords);
	qinvalidate.execute(to_string(p.valid_on), product_id);

	auto& qadd = *prepared_statements.at(statement::add_productrecord);
	qadd.execute(product_id, p.name, p.orig_price, p.price, to_string(p.discount_condition), to_string(p.valid_on), to_string(p.retrieved_on));
	std::cerr << "Inserted productrecord " << qadd.insert_id() << std::endl;
}

std::vector<product> storage::get_products_by_name(std::string const& name)
{
	database->get_thread_handle();
	lock_products_read();
	guard unlocker([&]() {
		unlock_all();
	});

	rusql::PreparedStatement& query = *prepared_statements.at(statement::get_productrecord_by_name);
	query.execute(name);

	std::vector<product> products;
	product row;
	std::string raw_discount_condition, raw_valid_on, raw_retrieved_on;

	query.bind_results(row.identifier, row.name, row.orig_price, row.price, raw_discount_condition, raw_valid_on, raw_retrieved_on);
	while(query.fetch())
	{
		row.discount_condition = to_condition(raw_discount_condition);
		row.valid_on = to_date(raw_valid_on);
		row.retrieved_on = to_datetime(raw_retrieved_on);

		products.push_back(row);
	}

	return products;
}

void storage::update_database_schema()
{
	bool product_found = false;
	bool productrecord_found = false;
	bool supermarket_found = false;

	auto result = database->select_query("show tables");
	while(result)
	{
		std::string tablename = result.get<std::string>(0);
		if(tablename == "product")
			product_found = true;
		else if(tablename == "productrecord")
			productrecord_found = true;
		else if(tablename == "supermarket")
			supermarket_found = true;

		result.next();
	}

	if(!product_found)
		database->execute(std::string() +
			"create table `product` ("+
			"`id` int(11) unsigned not null auto_increment,"+
			"`identifier` varchar(1024) not null,"+
			"`supermarket_id` int(11) unsigned null,"+
			"primary key (`id`),"+
			"key `supermarket_id` (`supermarket_id`)"+
			")");

	if(!productrecord_found)
		database->execute(std::string() +
			"create table `productrecord` (" +
			"`id` int(10) unsigned not null auto_increment," +
			"`product_id` int(10) unsigned not null," +
			"`name` varchar(1024) not null," +
			"`orig_price` decimal(10,2) not null," +
			"`price` decimal(10,2) not null," +
			"`discount_condition` enum('ALWAYS','AT_TWO','AT_THREE') not null," +
			"`valid_on` date not null," +
			"`valid_until` date," +
			"`retrieved_on` datetime not null," +
			"primary key (`id`)," +
			"key `product_id` (`product_id`)" +
			")");

	if(!supermarket_found)
		database->execute(std::string() +
			"create table `supermarket` (" +
			"`id` int(11) unsigned not null auto_increment," +
			"`name` varchar(1024) not null," +
			"primary key (`id`)" +
			")");
}

void storage::prepare_statements()
{
	auto create_statement =
			[&](statement const& s, std::string const& query)
	{
		prepared_statements[s] = std::make_shared<rusql::PreparedStatement>(database->prepare(query));
	};

	create_statement(statement::add_product, "insert into product (identifier, supermarket_id) values (?, ?)");
	create_statement(statement::get_product_by_identifier, "select product.id from product where product.identifier = ? and product.supermarket_id = ?");

	create_statement(statement::add_productrecord, "insert into productrecord (product_id, name, orig_price, price, discount_condition, valid_on, valid_until, retrieved_on) values(?, ?, ?, ?, ?, ?, NULL, ?)");
	create_statement(statement::get_productrecord_by_name, "select product.identifier, productrecord.name, productrecord.orig_price, productrecord.price, productrecord.discount_condition, productrecord.valid_on, productrecord.retrieved_on from product inner join productrecord on (product.id = productrecord.product_id) where productrecord.name = ? AND productrecord.valid_until is NULL");
	create_statement(statement::invalidate_productrecords, "update productrecord set productrecord.valid_until = ? where productrecord.valid_until is null and productrecord.product_id = ?");
}

}
