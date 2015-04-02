#include <karl/storage.hpp>

#include <karl/util/guard.hpp>

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

boost::optional<id_t> storage::find_product_unsafe(std::string const& identifier, id_t supermarket_id)
{
	auto& qselect = *prepared_statements.at(statement::get_product_by_identifier);
	qselect.execute(identifier, supermarket_id);

	id_t product_id;
	qselect.bind_results(product_id);

	if(qselect.fetch())
		return product_id;

	return boost::none;
}

boost::optional<std::pair<id_t, product>> storage::fetch_last_productdetails_unsafe(id_t product_id)
{
	auto& qolder = *prepared_statements.at(statement::get_last_productdetails_by_product);
	qolder.execute(product_id);

	id_t id;
	product p;
	std::string raw_discount_condition, raw_valid_on, raw_retrieved_on;

	qolder.bind_results(id, p.identifier, p.name, p.orig_price, p.price, raw_discount_condition, raw_valid_on, raw_retrieved_on);
	if(!qolder.fetch())
		return boost::none;

	p.discount_condition = to_condition(raw_discount_condition);
	p.valid_on = to_date(raw_valid_on);
	p.retrieved_on = to_datetime(raw_retrieved_on);

	return std::make_pair(id, p);
}

id_t storage::find_add_product(std::string const& identifier, id_t supermarket_id)
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

void storage::register_productdetailsrecord(id_t productdetails_id, datetime retrieved_on)
{
	auto& q = *prepared_statements.at(statement::add_productdetailsrecord);
	q.execute(productdetails_id, to_string(retrieved_on));
}

void storage::add_product(product const& p, id_t supermarket_id)
{
	id_t product_id = find_add_product(p.identifier, supermarket_id);

	// TODO Does not need locks, but does require a transaction

	// Check if an older version of the product exactly matches what we've got
	auto p_old_opt_kvp = fetch_last_productdetails_unsafe(product_id);
	if(p_old_opt_kvp)
	{
		product const& p_old = p_old_opt_kvp->second;
		bool similar = (
			p.discount_condition == p_old.discount_condition &&
			p.name == p_old.name &&
			p.orig_price == p_old.orig_price &&
			p.price == p_old.price
		);

		if(similar)
		{
			register_productdetailsrecord(p_old_opt_kvp->first, p.retrieved_on);
			return;
		}
		else
		{
			auto& qinvalidate = *prepared_statements.at(statement::invalidate_productdetails);
			qinvalidate.execute(to_string(p.valid_on), product_id);
		}
	}

	//TODO check if a newer entry is already entered. Perhaps invalidate that entry in such a case.

	auto& qadd = *prepared_statements.at(statement::add_productdetails);
	qadd.execute(product_id, p.name, p.orig_price, p.price, to_string(p.discount_condition), to_string(p.valid_on), to_string(p.retrieved_on));
	id_t productdetails_id = qadd.insert_id();
	std::cerr << "Inserted new productdetails " << productdetails_id << " for product " << p.identifier << " [" << product_id << ']' << std::endl;

	register_productdetailsrecord(productdetails_id, p.retrieved_on);
}

std::vector<product> storage::get_products_by_name(std::string const& name, id_t supermarket_id)
{
	lock_products_read();
	guard unlocker([&]() {
		unlock_all();
	});

	rusql::PreparedStatement& query = *prepared_statements.at(statement::get_last_productdetails_by_name);
	query.execute(std::string("%") + name + "%", supermarket_id);

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
	bool productdetails_found = false;
	bool productdetailsrecord_found = false;
	bool supermarket_found = false;

	auto result = database->select_query("show tables");
	while(result)
	{
		std::string tablename = result.get<std::string>(0);
		if(tablename == "product")
			product_found = true;
		else if(tablename == "productdetails")
			productdetails_found = true;
		else if(tablename == "productdetailsrecord")
			productdetailsrecord_found = true;
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
			") character set utf8 collate utf8_bin");

	if(!productdetails_found)
		database->execute(std::string() +
			"create table `productdetails` (" +
			"`id` int(10) unsigned not null auto_increment," +
			"`product_id` int(10) unsigned not null," +
			"`name` varchar(1024) character set utf8 not null," +
			"`orig_price` int not null," +
			"`price` int not null," +
			"`discount_condition` enum('ALWAYS','AT_TWO','AT_THREE') not null," +
			"`valid_on` date not null," +
			"`valid_until` date," +
			"`retrieved_on` datetime not null," +
			"primary key (`id`)," +
			"key `product_id` (`product_id`)" +
			") character set utf8 collate utf8_bin");

	if(!productdetailsrecord_found)
		database->execute(std::string() +
			"create table `productdetailsrecord` (" +
			"`id` int(10) unsigned not null auto_increment," +
			"`productdetails_id` int(10) unsigned not null," +
			"`retrieved_on` datetime not null," +
			"primary key (`id`)," +
			"key `productdetails_id` (`productdetails_id`)" +
			") character set utf8 collate utf8_bin");

	if(!supermarket_found)
		database->execute(std::string() +
			"create table `supermarket` (" +
			"`id` int(11) unsigned not null auto_increment," +
			"`name` varchar(1024) not null," +
			"primary key (`id`)" +
			") character set utf8 collate utf8_bin");
}

void storage::prepare_statements()
{
	auto create_statement =
			[&](statement const& s, std::string const& query)
	{
		prepared_statements[s] = std::make_shared<rusql::PreparedStatement>(database->prepare(query));
	};

	database->query("SET NAMES 'utf8'");
	database->query("SET CHARACTER SET utf8");

	create_statement(statement::add_product, "insert into product (identifier, supermarket_id) values (?, ?)");
	create_statement(statement::get_product_by_identifier, "select product.id from product where product.identifier = ? and product.supermarket_id = ?");

	create_statement(statement::add_productdetails, "insert into productdetails (product_id, name, orig_price, price, discount_condition, valid_on, valid_until, retrieved_on) values(?, ?, ?, ?, ?, ?, NULL, ?)");
	create_statement(statement::add_productdetailsrecord, "insert into productdetailsrecord (productdetails_id, retrieved_on) values(?, ?)");

	create_statement(statement::get_last_productdetails_by_product, "select productdetails.id, product.identifier, productdetails.name, productdetails.orig_price, productdetails.price, productdetails.discount_condition, productdetails.valid_on, productdetails.retrieved_on from product inner join productdetails on (product.id = productdetails.product_id) where productdetails.product_id = ? AND productdetails.valid_until is NULL");
	create_statement(statement::get_last_productdetails_by_name, "select product.identifier, productdetails.name, productdetails.orig_price, productdetails.price, productdetails.discount_condition, productdetails.valid_on, productdetails.retrieved_on from product inner join productdetails on (product.id = productdetails.product_id) where productdetails.name LIKE ? AND productdetails.valid_until is NULL AND product.supermarket_id = ?");
	create_statement(statement::invalidate_productdetails, "update productdetails set productdetails.valid_until = ? where productdetails.valid_until is null and productdetails.product_id = ?");
}

}
