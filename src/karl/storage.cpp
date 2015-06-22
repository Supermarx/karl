#include <karl/storage.hpp>

#include <karl/util/log.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <supermarx/util/guard.hpp>

#include "sql.cc"

namespace supermarx
{

storage::not_found_error::not_found_error()
: std::runtime_error("Row could not be found")
{}

enum class statement : uint8_t
{
	add_karluser,
	get_karluser,
	get_karluser_by_name,

	add_sessionticket,
	get_sessionticket,

	add_session,
	get_session_by_token,

	add_productclass,
	absorb_productclass,

	add_product,
	get_product_by_identifier,

	add_productdetails,
	add_productdetailsrecord,
	add_productlog,

	add_tag,
	get_tag_by_name,
	add_tagcategory,
	bind_tag,

	add_imagecitation,
	update_product_image_citation,

	get_all_productdetails_by_product,
	get_last_productdetails_by_product,
	get_last_productdetails_by_name,
	invalidate_productdetails,

	get_recent_productlog
};

std::string conv(statement rhs)
{
	return std::string("PREP_STATEMENT") + boost::lexical_cast<std::string>((uint32_t)rhs);
}

template<typename T>
struct rcol
{
	static inline void exec(pqxx::result::tuple& row, T& rhs, std::string const& key)
	{
		rhs = row[key].as<T>();
	}
};

template<typename T>
struct rcol<boost::optional<T>>
{
	static inline void exec(pqxx::result::tuple& row, boost::optional<T>& rhs, std::string const& key)
	{
		if(row[key].is_null())
			rhs = boost::none;
		else
			rhs = row[key].as<T>();
	}
};

template<>
struct rcol<date>
{
	static inline void exec(pqxx::result::tuple& row, date& rhs, std::string const& key)
	{
		rhs = to_date(row[key].as<std::string>());
	}
};

template<>
struct rcol<datetime>
{
	static inline void exec(pqxx::result::tuple& row, datetime& rhs, std::string const& key)
	{
		rhs = to_datetime(row[key].as<std::string>());
	}
};

template<>
struct rcol<measure>
{
	static inline void exec(pqxx::result::tuple& row, measure& rhs, std::string const& key)
	{
		rhs = to_measure(row[key].as<std::string>());
	}
};

template<>
struct rcol<token>
{
	static inline void exec(pqxx::result::tuple& row, token& rhs, std::string const& key)
	{
		pqxx::binarystring bs(row[key]);
		if(rhs.size() != bs.size())
			throw std::runtime_error("Binarystring does not have correct size to fit into token");

		memcpy(rhs.data(), bs.data(), rhs.size());
	}
};

#define rcoladv(row, p, col) { rcol<decltype(p.col)>::exec(row, p.col, #col); }

void read_karluser(pqxx::result::tuple& row, karluser& u)
{
	rcoladv(row, u, name);
	rcoladv(row, u, password_salt);
	rcoladv(row, u, password_hashed);
}

void read_session(pqxx::result::tuple& row, session& s)
{
	rcoladv(row, s, karluser_id);
	rcoladv(row, s, token);
	rcoladv(row, s, creation);
}

void read_sessionticket(pqxx::result::tuple& row, sessionticket& st)
{
	rcoladv(row, st, karluser_id);
	rcoladv(row, st, nonce);
	rcoladv(row, st, creation);
}

template<typename P>
void read_product(pqxx::result::tuple& row, P& p)
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

void read_product_summary(pqxx::result::tuple& row, api::product_summary& p)
{
	read_product(row, p);

	rcoladv(row, p, productclass_id);
	rcoladv(row, p, imagecitation_id);
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

boost::optional<std::pair<id_t, api::product_summary>> fetch_last_productdetails_unsafe(pqxx::transaction_base& txn, id_t product_id)
{
	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_product))
			(product_id).exec();

	for(auto row : result)
	{
		api::product_summary p;
		read_product_summary(row, p);

		return std::make_pair(row["id"].as<id_t>(), p);
	}

	return boost::none;
}

id_t find_add_product(pqxx::connection& conn, std::string const& identifier, id_t supermarket_id, std::string const& name)
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

		pqxx::result result_productclass = txn.prepared(conv(statement::add_productclass))
				(name).exec();

		id_t productclass_id = read_id(result_productclass);

		pqxx::result result_product = txn.prepared(conv(statement::add_product))
				(identifier)
				(supermarket_id)
				(productclass_id).exec();

		txn.commit();

		return read_id(result_product);
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

id_t storage::add_karluser(karluser const& user)
{
	pqxx::work txn(conn);

	pqxx::binarystring pw_salt_bs(user.password_salt.data(), user.password_salt.size());
	pqxx::binarystring pw_hashed_bs(user.password_hashed.data(), user.password_hashed.size());

	pqxx::result result(txn.prepared(conv(statement::add_karluser))
		(user.name)(pw_salt_bs)(pw_hashed_bs).exec());

	id_t karluser_id = read_id(result);

	txn.commit();

	return karluser_id;
}

karluser storage::get_karluser(id_t karluser_id)
{
	pqxx::work txn(conn);

	pqxx::result result(txn.prepared(conv(statement::get_karluser))(karluser_id).exec());

	for(auto row : result)
	{
		karluser u;
		read_karluser(row, u);
		return u;
	}

	throw storage::not_found_error();
}

std::pair<id_t, karluser> storage::get_karluser_by_name(const std::string &name)
{
	pqxx::work txn(conn);

	pqxx::result result(txn.prepared(conv(statement::get_karluser_by_name))(name).exec());

	for(auto row : result)
	{
		karluser u;
		read_karluser(row, u);

		return std::make_pair(row["id"].as<id_t>(), u);
	}

	throw storage::not_found_error();
}

id_t storage::add_sessionticket(sessionticket const& st)
{
	pqxx::work txn(conn);

	pqxx::binarystring nonce_bs(st.nonce.data(), st.nonce.size());

	pqxx::result result(txn.prepared(conv(statement::add_sessionticket))
		(st.karluser_id)
		(nonce_bs)
		(to_string(st.creation)).exec());

	id_t sessionticket_id = read_id(result);

	txn.commit();

	return sessionticket_id;
}

std::pair<id_t, sessionticket> storage::get_sessionticket(id_t sessionticket_id)
{
	pqxx::work txn(conn);

	pqxx::result result(txn.prepared(conv(statement::get_sessionticket))(sessionticket_id).exec());

	for(auto row : result)
	{
		sessionticket st;
		read_sessionticket(row, st);

		return std::make_pair(row["id"].as<id_t>(), st);
	}

	throw storage::not_found_error();
}

id_t storage::add_session(session const& s)
{
	pqxx::work txn(conn);

	pqxx::binarystring token_bs(s.token.data(), s.token.size());

	pqxx::result result(txn.prepared(conv(statement::add_session))
		(s.karluser_id)
		(token_bs)
		(to_string(s.creation)).exec());

	id_t session_id = read_id(result);

	txn.commit();

	return session_id;
}

std::pair<id_t, session> storage::get_session_by_token(const api::sessiontoken &token)
{
	pqxx::work txn(conn);

	pqxx::binarystring token_bs(token.data(), token.size());
	pqxx::result result(txn.prepared(conv(statement::get_session_by_token))(token_bs).exec());

	for(auto row : result)
	{
		session s;
		read_session(row, s);

		return std::make_pair(row["id"].as<id_t>(), s);
	}

	throw storage::not_found_error();
}

void storage::add_product(product const& p, id_t supermarket_id, datetime retrieved_on, confidence conf, std::vector<std::string> const& problems)
{
	id_t product_id = find_add_product(conn, p.identifier, supermarket_id, p.name);

	pqxx::work txn(conn);

	// Check if an older version of the product exactly matches what we've got
	auto p_old_opt_kvp = fetch_last_productdetails_unsafe(txn, product_id);
	if(p_old_opt_kvp)
	{
		api::product_summary const& p_old = p_old_opt_kvp->second;
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

api::product_summary storage::get_product(const std::string &identifier, id_t supermarket_id)
{
	pqxx::work txn(conn);

	lock_products_read(txn);

	auto product_id_opt = find_product_unsafe(txn, identifier, supermarket_id);
	if(!product_id_opt)
		throw storage::not_found_error();

	auto productdetails_opt_kvp = fetch_last_productdetails_unsafe(txn, *product_id_opt);
	if(!productdetails_opt_kvp)
		throw std::logic_error(std::string("Inconsistency: found product ") + boost::lexical_cast<std::string>(*product_id_opt) + " but has no (latest) productdetails entry");

	return productdetails_opt_kvp->second;
}

api::product_history storage::get_product_history(std::string const& identifier, id_t supermarket_id)
{
	pqxx::work txn(conn);

	auto product_id_opt = find_product_unsafe(txn, identifier, supermarket_id);
	if(!product_id_opt)
		throw storage::not_found_error();

	api::product_history history;
	history.identifier = identifier;

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

		history.name = p.name; // Last set will be used
		history.pricehistory.emplace_back(valid_on, p.price);
	}

	return history;
}

std::vector<api::product_summary> storage::get_products_by_name(std::string const& name, id_t supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_name))
			(std::string("%") + name + "%")
			(supermarket_id).exec();

	std::vector<api::product_summary> products;
	for(auto row : result)
	{
		api::product_summary p;
		read_product_summary(row, p);
		products.emplace_back(p);
	}

	return products;
}

std::vector<api::product_log> storage::get_recent_productlog(id_t supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_recent_productlog))
			(supermarket_id).exec();

	std::map<std::string, api::product_log> log_map;

	for(auto row : result)
	{
		std::string identifier(row["identifier"].as<std::string>());
		std::string message(row["description"].as<std::string>());

		auto it = log_map.find(identifier);
		if(it == log_map.end())
		{
			api::product_log pl;
			pl.identifier = identifier;
			pl.name = row["name"].as<std::string>();
			pl.messages.emplace_back(message);

			log_map.insert(std::make_pair(identifier, pl));
		}
		else
		{
			it->second.messages.emplace_back(message);
		}
	}

	std::vector<api::product_log> log;
	for(auto& p : log_map)
		log.emplace_back(p.second);

	return log;
}

void storage::absorb_productclass(id_t src_productclass_id, id_t dest_productclass_id)
{
	pqxx::work txn(conn);

	txn.prepared(conv(statement::absorb_productclass))
			(src_productclass_id)
			(dest_productclass_id).exec();

	txn.commit();
}

id_t storage::find_add_tag(const api::tag &t)
{
	pqxx::work txn(conn);

	{
		pqxx::result result_tag_get = txn.prepared(conv(statement::get_tag_by_name))
				(t.name).exec();

		if(result_tag_get.size() > 0)
			return read_id(result_tag_get);
	}

	id_t id;
	if(t.category)
	{
		pqxx::result result_tagcategory_add = txn.prepared(conv(statement::add_tagcategory))
				(*t.category).exec();

		id_t tagcategory_id = read_id(result_tagcategory_add);

		pqxx::result result_tag_add = txn.prepared(conv(statement::add_tag))
				() // NULL -> no parent
				(tagcategory_id)
				(t.name).exec();

		id = read_id(result_tag_add);
	}
	else
	{
		pqxx::result result_tag_add = txn.prepared(conv(statement::add_tag))
				() // NULL -> no parent
				() // NULL -> no tag category
				(t.name).exec();

		id = read_id(result_tag_add);
	}

	txn.commit();

	return id;
}

void storage::bind_tag(id_t productclass_id, id_t tag_id)
{
	pqxx::work txn(conn);

	txn.prepared(conv(statement::bind_tag))
			(tag_id)
			(productclass_id).exec();

	txn.commit();
}

id_t storage::add_image_citation(id_t supermarket_id, const std::string &original_uri, const std::string &source_uri, size_t original_width, size_t original_height, const datetime &retrieved_on)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::add_imagecitation))
			(supermarket_id)
			(original_uri)
			(source_uri)
			(original_width)
			(original_height)
			(to_string(retrieved_on)).exec();

	id_t id = read_id(result);
	txn.commit();
	return id;
}

void storage::update_product_image_citation(const std::string &product_identifier, id_t supermarket_id, id_t image_citation_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::update_product_image_citation))
			(image_citation_id)
			(product_identifier)
			(supermarket_id).exec();

	txn.commit();
}

#define ADD_SCHEMA(ID)\
	schema_queries.emplace(std::make_pair(ID, std::string((char*) sql_schema_ ## ID, sql_schema_ ## ID ## _len)));

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

	const size_t target_schema_version = 7;

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
	PREPARE_STATEMENT(add_karluser)
	PREPARE_STATEMENT(get_karluser)
	PREPARE_STATEMENT(get_karluser_by_name)

	PREPARE_STATEMENT(add_sessionticket)
	PREPARE_STATEMENT(get_sessionticket)

	PREPARE_STATEMENT(add_session)
	PREPARE_STATEMENT(get_session_by_token)

	PREPARE_STATEMENT(add_productclass)
	PREPARE_STATEMENT(absorb_productclass)

	PREPARE_STATEMENT(add_product)
	PREPARE_STATEMENT(get_product_by_identifier)

	PREPARE_STATEMENT(add_tag)
	PREPARE_STATEMENT(get_tag_by_name)
	PREPARE_STATEMENT(add_tagcategory)
	PREPARE_STATEMENT(bind_tag)

	PREPARE_STATEMENT(add_productdetails)
	PREPARE_STATEMENT(add_productdetailsrecord)
	PREPARE_STATEMENT(add_productlog)

	PREPARE_STATEMENT(add_imagecitation)
	PREPARE_STATEMENT(update_product_image_citation);

	PREPARE_STATEMENT(get_all_productdetails_by_product)
	PREPARE_STATEMENT(get_last_productdetails_by_product)
	PREPARE_STATEMENT(get_last_productdetails_by_name)
	PREPARE_STATEMENT(invalidate_productdetails)

	PREPARE_STATEMENT(get_recent_productlog)
}

#undef PREPARE_STATEMENT

}
