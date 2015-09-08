#include <karl/storage/storage.hpp>

#include <karl/storage/storage_common.hpp>
#include <karl/storage/storage_tags.hpp>
#include <karl/storage/storage_products.hpp>
#include <karl/storage/storage_users.hpp>

namespace supermarx
{

storage::not_found_error::not_found_error()
: std::runtime_error("Row could not be found")
{}

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

void storage::check_integrity()
{
	pqxx::work txn(conn);
	check_tag_consistency(txn);
}

reference<data::imagecitation> storage::add_image_citation(data::imagecitation const& ic)
{
	return write_simple_with_id(conn, ic);
}

void storage::update_product_image_citation(const std::string &product_identifier, reference<data::supermarket> supermarket_id, reference<data::imagecitation> imagecitation_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::update_product_image_citation))
			(imagecitation_id.unseal())
			(product_identifier)
			(supermarket_id.unseal()).exec();

	txn.commit();
}

#define ADD_SCHEMA(ID)\
	schema_queries.emplace(std::make_pair(ID, std::string(reinterpret_cast<char*>(sql_schema_ ## ID), sql_schema_ ## ID ## _len)));

void storage::update_database_schema()
{
	std::map<unsigned int, std::string> schema_queries;
	ADD_SCHEMA(1);
	ADD_SCHEMA(2);
	ADD_SCHEMA(3);
	ADD_SCHEMA(4);
	ADD_SCHEMA(5);
	ADD_SCHEMA(6);
	ADD_SCHEMA(7);
	ADD_SCHEMA(8);
	ADD_SCHEMA(9);
	ADD_SCHEMA(10);
	ADD_SCHEMA(11);
	ADD_SCHEMA(12);

	const size_t target_schema_version = 12;

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
	conn.prepare(conv(statement::NAME), std::string(reinterpret_cast<char*>(sql_ ## NAME), sql_ ## NAME ## _len));

void storage::prepare_statements()
{
	PREPARE_STATEMENT(absorb_productclass_product)
	PREPARE_STATEMENT(absorb_productclass_delete_tag_productclass)
	PREPARE_STATEMENT(absorb_productclass_tag_productclass)
	PREPARE_STATEMENT(absorb_productclass_delete)

	PREPARE_STATEMENT(update_product)

	PREPARE_STATEMENT(absorb_tag)
	PREPARE_STATEMENT(absorb_tagcategory)
	PREPARE_STATEMENT(bind_tag)
	PREPARE_STATEMENT(update_tag_set_parent)

	PREPARE_STATEMENT(update_product_image_citation);

	PREPARE_STATEMENT(invalidate_productdetails)
}

#undef PREPARE_STATEMENT

}
