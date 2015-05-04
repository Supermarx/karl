#include <karl/storage.hpp>

#include <karl/util/log.hpp>
#include <karl/util/guard.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "sql.cc"

namespace supermarx
{

enum class statement : uint8_t
{
	add_product,
	get_product_by_identifier,

	add_productdetails,
	add_productdetailsrecord,
	add_productlog,

	get_all_productdetails_by_product,
	get_last_productdetails_by_product,
	get_last_productdetails_by_name,
	invalidate_productdetails
};

std::string conv(statement rhs)
{
	return std::string("PREP_STATEMENT") + boost::lexical_cast<std::string>((uint32_t)rhs);
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

template<>
void rcol<measure>(pqxx::result::tuple& row, measure& rhs, std::string const& key)
{
	rhs = to_measure(row[key].as<std::string>());
}

#define rcoladv(row, p, col) { rcol(row, p.col, #col); }

void read_product(pqxx::result::tuple& row, product& p)
{
	rcoladv(row, p, identifier);
	rcoladv(row, p, name);
	rcoladv(row, p, volume);
	rcoladv(row, p, volume_measure);
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

void register_productdetailsrecord(pqxx::transaction_base& txn, id_t productdetails_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems)
{
	pqxx::result result = txn.prepared(conv(statement::add_productdetailsrecord))
			(productdetails_id)
			(to_string(retrieved_on))
			(to_string(conf)).exec();

	size_t id = read_id(result);
	for(std::string p : problems)
	{
		txn.prepared(conv(statement::add_productlog))
				(id)
				(p).exec();
	}
}

void storage::add_product(product const& p, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems)
{
	id_t product_id = find_add_product(conn, p.identifier, supermarket_id);

	pqxx::work txn(conn);

	// Check if an older version of the product exactly matches what we've got
	auto p_old_opt_kvp = fetch_last_productdetails_unsafe(txn, product_id);
	if(p_old_opt_kvp)
	{
		product const& p_old = p_old_opt_kvp->second;
		bool similar = (
			p.volume == p_old.volume &&
			p.volume_measure == p_old.volume_measure &&
			p.discount_amount == p_old.discount_amount &&
			p.name == p_old.name &&
			p.orig_price == p_old.orig_price &&
			p.price == p_old.price
		);

		if(similar)
		{
			register_productdetailsrecord(txn, p_old_opt_kvp->first, retrieved_on, conf, problems);
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
			(product_id)(p.name)(p.volume)(to_string(p.volume_measure))
			(p.orig_price)(p.price)
			(p.discount_amount)(to_string(p.valid_on))
			(to_string(retrieved_on)).exec();

	id_t productdetails_id = read_id(result);
	log("storage::storage", log::level_e::NOTICE)() << "Inserted new productdetails " << productdetails_id << " for product " << p.identifier << " [" << product_id << ']';

	register_productdetailsrecord(txn, productdetails_id, retrieved_on, conf, problems);
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

		datetime valid_on = p.valid_on;
		datetime retrieved_on = to_datetime(row["retrieved_on"].as<std::string>());

		if(valid_on < retrieved_on)
			valid_on = retrieved_on;

		summary.name = p.name; // Last set will be used
		summary.pricehistory.emplace_back(valid_on, p.price);
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

#define ADD_SCHEMA(ID)\
	schema_queries.emplace(std::make_pair(ID, std::string((char*) sql_schema_ ## ID, sql_schema_ ## ID ## _len)));

void storage::update_database_schema()
{
	std::map<unsigned int, std::string> schema_queries;
	ADD_SCHEMA(1);
	ADD_SCHEMA(2);
	ADD_SCHEMA(3);

	const size_t target_schema_version = 3;

	unsigned int schema_version = 0;
	try
	{
		pqxx::work txn(conn);
		pqxx::result result(txn.exec("select value from karlinfo where key = 'schemaversion'"));

		if(result.begin() == result.end())
			throw std::runtime_error("Could not fetch schemaversion");

		schema_version = boost::lexical_cast<unsigned int>(result.begin()["value"].as<std::string>());
	} catch(pqxx::sql_error e)
	{
		if(!boost::algorithm::starts_with(e.what(), "ERROR:  relation \"karlinfo\" does not exist"))
			throw e;

		pqxx::work txn(conn);
		txn.exec(R"prefix(
				 create table karlinfo (
					 key varchar not null,
					 value varchar not null
					 )
				 )prefix");

				txn.exec("insert into karlinfo (key, value) values ('schemaversion', 0)");
		txn.commit();
	}

	conn.prepare("UPGRADE_SCHEMA", "update karlinfo set value = $1 where key = 'schemaversion'");
	while(schema_version < target_schema_version)
	{
		schema_version++;

		log("storage::update_database_schema", log::level_e::NOTICE)() << "Upgrading to schema version " << schema_version;

		pqxx::work txn(conn);
		auto pair_it = schema_queries.find(schema_version);

		if(pair_it == schema_queries.end())
			throw std::runtime_error("Could not find appropriate schema upgrade query");

		try
		{
			txn.exec(pair_it->second);
			txn.prepared("UPGRADE_SCHEMA")(schema_version).exec();
			txn.commit();
		} catch(pqxx::sql_error e)
		{
			log("storage::update_database_schema", log::level_e::ERROR)() << "Failed to upgrade to schema version " << schema_version << ":\n" << e.what();
			throw std::runtime_error("Failed to upgrade schema version");
		}

		log("storage::update_database_schema", log::level_e::NOTICE)() << "Updated to schema version " << schema_version;
	}

	log("storage::update_database_schema", log::level_e::NOTICE)() << "Storage engine started for schema version " << schema_version;
}

#undef ADD_SCHEMA

#define PREPARE_STATEMENT(NAME)\
	conn.prepare(conv(statement::NAME), std::string((char*) sql_ ## NAME, sql_ ## NAME ## _len));

void storage::prepare_statements()
{
	PREPARE_STATEMENT(add_product)
	PREPARE_STATEMENT(get_product_by_identifier)

	PREPARE_STATEMENT(add_productdetails)
	PREPARE_STATEMENT(add_productdetailsrecord)
	PREPARE_STATEMENT(add_productlog)

	PREPARE_STATEMENT(get_all_productdetails_by_product)
	PREPARE_STATEMENT(get_last_productdetails_by_product)
	PREPARE_STATEMENT(get_last_productdetails_by_name)
	PREPARE_STATEMENT(invalidate_productdetails)
}

#undef PREPARE_STATEMENT

}
