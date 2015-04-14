#include <karl/storage.hpp>

#include <karl/util/log.hpp>
#include <karl/util/guard.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace supermarx
{

enum class statement : uint8_t
{
	add_product,
	get_product_by_identifier,

	add_productdetails,
	add_productdetailsrecord,

	get_all_productdetails_by_product,
	get_last_productdetails_by_product,
	get_last_productdetails_by_name,
	invalidate_productdetails
};

std::string conv(statement rhs)
{
	return std::string("PREP_STATEMENT") + boost::lexical_cast<std::string>((uint8_t)rhs);
}

template<typename T>
void rcol(pqxx::result::tuple& row, T& rhs, std::string const& key)
{
	rhs = row[key].as<T>();
}

template<>
void rcol<date>(pqxx::result::tuple& row, date& rhs, std::string const& key)
{
	rhs = to_date(row[key].as<std::string>());
}

template<>
void rcol<datetime>(pqxx::result::tuple& row, datetime& rhs, std::string const& key)
{
	rhs = to_datetime(row[key].as<std::string>());
}

#define rcoladv(row, p, col) { rcol(row, p.col, #col); }

void read_product(pqxx::result::tuple& row, product& p)
{
	rcoladv(row, p, identifier);
	rcoladv(row, p, name);
	rcoladv(row, p, orig_price);
	rcoladv(row, p, price);
	rcoladv(row, p, valid_on);
	rcoladv(row, p, discount_amount);
}

#undef rcoladv

id_t read_id(pqxx::result& result)
{
	for(auto row : result)
		return row["id"].as<id_t>();

	assert(false);
}

static std::string create_connstr(const std::string &host, const std::string &user, const std::string &password, const std::string& db)
{
	std::stringstream sstr;
	sstr << "host=" << host << " user=" << user << " password=" << password << " dbname=" << db;
	return sstr.str();
}

storage::storage(const std::string &host, const std::string &user, const std::string &password, const std::string& db)
	: conn(create_connstr(host, user, password, db))
{
	update_database_schema();
	prepare_statements();
}

storage::~storage() {}

void lock_products_read(pqxx::transaction_base& txn)
{
	txn.exec("lock table product in access share mode");
}

void lock_products_write(pqxx::transaction_base& txn)
{
	txn.exec("lock table product in access exclusive mode");
}

boost::optional<id_t> find_product_unsafe(pqxx::transaction_base& txn, std::string const& identifier, id_t supermarket_id)
{
	pqxx::result result = txn.prepared(conv(statement::get_product_by_identifier))
			(identifier)
			(supermarket_id).exec();

	for(auto row : result)
		return row["id"].as<id_t>();

	return boost::none;
}

boost::optional<std::pair<id_t, product>> fetch_last_productdetails_unsafe(pqxx::transaction_base& txn, id_t product_id)
{
	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_product))
			(product_id).exec();

	for(auto row : result)
	{
		product p;
		read_product(row, p);

		return std::make_pair(row["id"].as<id_t>(), p);
	}

	return boost::none;
}

id_t find_add_product(pqxx::connection& conn, std::string const& identifier, id_t supermarket_id)
{
	{
		pqxx::work txn(conn);
		lock_products_read(txn);
		auto product_id = find_product_unsafe(txn, identifier, supermarket_id);
		if(product_id)
			return product_id.get();
	}

	{
		pqxx::work txn(conn);
		lock_products_write(txn);

		{
			auto product_id = find_product_unsafe(txn, identifier, supermarket_id);
			if(product_id)
				return product_id.get();
		}

		pqxx::result result = txn.prepared(conv(statement::add_product))
				(identifier)
				(supermarket_id).exec();

		txn.commit();

		return read_id(result);
	}
}

void register_productdetailsrecord(pqxx::transaction_base& txn, id_t productdetails_id, datetime retrieved_on, confidence conf)
{
	txn.prepared(conv(statement::add_productdetailsrecord))
			(productdetails_id)
			(to_string(retrieved_on))
			(to_string(conf)).exec();
}

void storage::add_product(product const& p, id_t supermarket_id, datetime retrieved_on, confidence conf)
{
	id_t product_id = find_add_product(conn, p.identifier, supermarket_id);

	pqxx::work txn(conn);

	// Check if an older version of the product exactly matches what we've got
	auto p_old_opt_kvp = fetch_last_productdetails_unsafe(txn, product_id);
	if(p_old_opt_kvp)
	{
		product const& p_old = p_old_opt_kvp->second;
		bool similar = (
			p.discount_amount == p_old.discount_amount &&
			p.name == p_old.name &&
			p.orig_price == p_old.orig_price &&
			p.price == p_old.price
		);

		if(similar)
		{
			register_productdetailsrecord(txn, p_old_opt_kvp->first, retrieved_on, conf);
			txn.commit();
			return;
		}
		else
		{
			txn.prepared(conv(statement::invalidate_productdetails))
					(to_string(p.valid_on))
					(product_id).exec();
		}
	}

	//TODO check if a newer entry is already entered. Perhaps invalidate that entry in such a case.
	pqxx::result result = txn.prepared(conv(statement::add_productdetails))
			(product_id)(p.name)(p.orig_price)(p.price)
			(p.discount_amount)(to_string(p.valid_on))
			(to_string(retrieved_on)).exec();

	id_t productdetails_id = read_id(result);
	log("storage::storage", log::level_e::NOTICE)() << "Inserted new productdetails " << productdetails_id << " for product " << p.identifier << " [" << product_id << ']';

	register_productdetailsrecord(txn, productdetails_id, retrieved_on, conf);
	txn.commit();
}

boost::optional<api::product_summary> storage::get_product_summary(std::string const& identifier, id_t supermarket_id)
{
	pqxx::work txn(conn);

	auto product_id_opt = find_product_unsafe(txn, identifier, supermarket_id);
	if(!product_id_opt)
		return boost::none;

	api::product_summary summary;
	summary.identifier = identifier;

	pqxx::result result = txn.prepared(conv(statement::get_all_productdetails_by_product))
			(*product_id_opt).exec();

	for(auto row : result)
	{
		product p;
		read_product(row, p);

		summary.name = p.name; // Last set will be used
		summary.pricehistory.emplace_back(p.valid_on, p.price);
	}

	return summary;
}

std::vector<product> storage::get_products_by_name(std::string const& name, id_t supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_name))
			(std::string("%") + name + "%")
			(supermarket_id).exec();

	std::vector<product> products;
	for(auto row : result)
	{
		product p;
		read_product(row, p);
		products.emplace_back(p);
	}

	return products;
}

void storage::update_database_schema()
{
	auto try_create = [&](std::string const& q)
	{
		try
		{
			pqxx::work txn(conn);
			txn.exec(q);
			txn.commit();
		} catch(pqxx::sql_error e)
		{
			if(!boost::algorithm::ends_with(e.what(), "already exists\n"))
				throw e;
		}
	};

	try_create(std::string() +
			"create table product ("+
			"id serial primary key,"+
			"identifier varchar(1024) not null,"+
			"supermarket_id integer not null"+
			")");

	try_create(std::string() +
			 "create index product_supermarket_idx on product(supermarket_id)");
	try_create(std::string() +
			 "create index product_identifierx on product(identifier)");

	try_create(std::string() +
			 "create table productdetails (" +
			 "id serial primary key," +
			 "product_id int not null," +
			 "name varchar(1024) not null," +
			 "orig_price int not null," +
			 "price int not null," +
			 "discount_amount int not null," +
			 "valid_on timestamp not null," +
			 "valid_until timestamp," +
			 "retrieved_on timestamp not null" +
			 ")");

	try_create(std::string() +
			 "create index productdetails_product_idx on productdetails(product_id)");

	try_create(std::string() +
			 "create type confidence_t as enum ('LOW','NEUTRAL','HIGH', 'PERFECT')");

	try_create(std::string() +
			 "create table productdetailsrecord (" +
			 "id serial primary key," +
			 "productdetails_id int not null," +
			 "retrieved_on timestamp not null," +
			 "confidence confidence_t not null default 'LOW'"
			 ")");

	try_create(std::string() +
			 "create index productdetailsrecord_productdetails_idx on productdetailsrecord(productdetails_id)");

	try_create(std::string() +
			 "create table supermarket (" +
			 "id serial primary key," +
			 "name varchar(1024) not null"
			 ")");
}

void storage::prepare_statements()
{
	conn.prepare(conv(statement::add_product), "insert into product (identifier, supermarket_id) values ($1, $2) returning id");
	conn.prepare(conv(statement::get_product_by_identifier), "select product.id from product where product.identifier = $1 and product.supermarket_id = $2");

	conn.prepare(conv(statement::add_productdetails), "insert into productdetails (product_id, name, orig_price, price, discount_amount, valid_on, valid_until, retrieved_on) values($1, $2, $3, $4, $5, $6, NULL, $7) returning id");
	conn.prepare(conv(statement::add_productdetailsrecord), "insert into productdetailsrecord (productdetails_id, retrieved_on, confidence) values($1, $2, $3) returning id");

	conn.prepare(conv(statement::get_all_productdetails_by_product), "select productdetails.id, product.identifier, productdetails.name, productdetails.orig_price, productdetails.price, productdetails.discount_amount, productdetails.valid_on, productdetails.retrieved_on from product inner join productdetails on (product.id = productdetails.product_id) where productdetails.product_id = $1 order by productdetails.id asc");
	conn.prepare(conv(statement::get_last_productdetails_by_product), "select productdetails.id, product.identifier, productdetails.name, productdetails.orig_price, productdetails.price, productdetails.discount_amount, productdetails.valid_on, productdetails.retrieved_on from product inner join productdetails on (product.id = productdetails.product_id) where productdetails.product_id = $1 AND productdetails.valid_until is NULL");
	conn.prepare(conv(statement::get_last_productdetails_by_name), "select product.identifier, productdetails.name, productdetails.orig_price, productdetails.price, productdetails.discount_amount, productdetails.valid_on, productdetails.retrieved_on from product inner join productdetails on (product.id = productdetails.product_id) where lower(productdetails.name) like lower($1) AND productdetails.valid_until is NULL AND product.supermarket_id = $2");
	conn.prepare(conv(statement::invalidate_productdetails), "update productdetails set valid_until = $1 where productdetails.valid_until is null and productdetails.product_id = $2");
}

}
