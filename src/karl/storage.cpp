#include <karl/storage.hpp>

#include <stack>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include <karl/price_normalization.hpp>

#include <karl/util/log.hpp>
#include <karl/storage_read_fusion.hpp>
#include <karl/storage_write_fusion.hpp>

#include <supermarx/util/guard.hpp>

#include <supermarx/data/tagalias.hpp>
#include <supermarx/data/tagcategoryalias.hpp>

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

	get_productclass,
	add_productclass,
	absorb_productclass_product,
	absorb_productclass_delete_tag_productclass,
	absorb_productclass_tag_productclass,
	absorb_productclass_delete,

	add_product,
	update_product,
	get_product_by_identifier,

	add_productdetails,
	add_productdetailsrecord,
	add_productlog,

	get_tags,
	get_tags_by_productclass,
	add_tag,
	add_tagalias,
	get_tagalias_by_tagcategory_name,
	add_tagcategory,
	add_tagcategoryalias,
	get_tagcategoryalias_by_name,
	absorb_tag,
	absorb_tagcategory,
	bind_tag,
	update_tag_set_parent,

	add_imagecitation,
	update_product_image_citation,

	get_all_productdetails_by_product,
	get_last_productdetails,
	get_last_productdetails_by_product,
	get_last_productdetails_by_name,
	get_last_productdetails_by_productclass,
	invalidate_productdetails,

	get_recent_productlog
};

inline std::string conv(statement rhs)
{
	return std::string("PREP_STATEMENT") + boost::lexical_cast<std::string>(static_cast<uint32_t>(rhs));
}

id_t read_id(pqxx::result& result, std::string const& key = "id")
{
	for(auto row : result)
		return row[key].as<id_t>();

	throw std::logic_error("Storage backend did not return a single row for `read_id`");
}

template<typename T>
static inline T read_first_result(pqxx::result const& result)
{
	for(auto row : result)
		return read_result<T>(row);

	throw storage::not_found_error();
}

template<typename T, typename ARG>
static inline T fetch_simple_first(pqxx::connection& conn, statement stmt, ARG const& arg)
{
	pqxx::work txn(conn);
	pqxx::result result(txn.prepared(conv(stmt))(arg).exec());
	return read_first_result<T>(result);
}

template<typename T, typename U>
static inline reference<T> fetch_id(pqxx::work& txn, statement stmt, U const& x)
{
	pqxx::prepare::invocation invo(txn.prepared(conv(stmt)));
	write_invo<U>(invo, x);
	pqxx::result result(invo.exec());
	return reference<T>(read_id(result));
}

template<typename T>
static inline pqxx::result write(pqxx::transaction_base& txn, statement stmt, T const& x)
{
	pqxx::prepare::invocation invo(txn.prepared(conv(stmt)));
	write_invo<T>(invo, x);
	pqxx::result result(invo.exec());
	return result;
}

template<typename T>
static inline reference<T> write_with_id(pqxx::transaction_base& txn, statement stmt, T const& x)
{
	pqxx::result result(write(txn, stmt, x));
	return reference<T>(read_id(result));
}

template<typename T>
static inline pqxx::result write_simple(pqxx::connection& conn, statement stmt, T const& x)
{
	pqxx::work txn(conn);
	pqxx::result result(write(txn, stmt, x));
	txn.commit();
	return result;
}

template<typename T>
static inline reference<T> write_simple_with_id(pqxx::connection& conn, statement stmt, T const& x)
{
	pqxx::result result(write_simple(conn, stmt, x));
	return reference<T>(read_id(result));
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

inline static message::product_summary merge(data::product const& p, data::productdetails const& pd)
{
	return message::product_summary({
		p.identifier,
		p.supermarket_id,
		p.name,
		p.productclass_id,
		p.volume,
		p.volume_measure,
		pd.orig_price,
		pd.price,
		pd.discount_amount,
		price_normalization::exec(pd.orig_price, p.volume, p.volume_measure),
		price_normalization::exec(pd.price, p.volume, p.volume_measure),
		pd.valid_on,
		p.imagecitation_id
	});
}

qualified<data::product> find_product_unsafe(pqxx::transaction_base& txn, reference<data::supermarket> supermarket_id, std::string const& identifier)
{
	pqxx::result result = txn.prepared(conv(statement::get_product_by_identifier))
			(identifier)
			(supermarket_id.unseal()).exec();

	return read_first_result<qualified<data::product>>(result);
}

qualified<data::productdetails> fetch_last_productdetails_unsafe(pqxx::transaction_base& txn, reference<data::product> product_id)
{
	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_product))
			(product_id.unseal()).exec();
	return read_first_result<qualified<data::productdetails>>(result);
}

qualified<data::product> find_add_product(pqxx::connection& conn, reference<data::supermarket> supermarket_id, message::product_base const& pb)
{
	{
		pqxx::work txn(conn);
		lock_products_read(txn);

		try
		{
			return find_product_unsafe(txn, supermarket_id, pb.identifier);
		} catch(storage::not_found_error)
		{}
	}

	{
		pqxx::work txn(conn);
		lock_products_write(txn);

		try
		{
			return find_product_unsafe(txn, supermarket_id, pb.identifier);
		} catch(storage::not_found_error)
		{}

		reference<data::productclass> productclass_id(
			write_with_id(txn, statement::add_productclass, data::productclass({
				pb.name
		})));

		data::product p({
			pb.identifier,
			supermarket_id,
			boost::none,
			productclass_id,
			pb.name,
			pb.volume,
			pb.volume_measure
		});

		reference<data::product> product_id(write_with_id(txn, statement::add_product, p));
		txn.commit();

		return qualified<data::product>(product_id, p);
	}
}

void register_productdetailsrecord(pqxx::transaction_base& txn, data::productdetailsrecord const& pdr, std::vector<std::string> const& problems)
{
	reference<data::productdetailsrecord> pdn_id(write_with_id(txn, statement::add_productdetailsrecord, pdr));

	for(std::string const& p_str : problems)
		write(txn, statement::add_productlog, data::productlog({pdn_id, p_str}));
}

reference<data::karluser> storage::add_karluser(data::karluser const& user)
{
	return write_simple_with_id(conn, statement::add_karluser, user);
}

qualified<data::karluser> storage::get_karluser(reference<data::karluser> karluser_id)
{
	return fetch_simple_first<qualified<data::karluser>>(conn, statement::get_karluser, karluser_id.unseal());
}

qualified<data::karluser> storage::get_karluser_by_name(const std::string &name)
{
	return fetch_simple_first<qualified<data::karluser>>(conn, statement::get_karluser_by_name, name);
}

reference<data::sessionticket> storage::add_sessionticket(data::sessionticket const& st)
{
	return write_simple_with_id(conn, statement::add_sessionticket, st);
}

qualified<data::sessionticket> storage::get_sessionticket(reference<data::sessionticket> sessionticket_id)
{
	return fetch_simple_first<qualified<data::sessionticket>>(conn, statement::get_sessionticket, sessionticket_id.unseal());
}

reference<data::session> storage::add_session(data::session const& s)
{
	return write_simple_with_id(conn, statement::add_session, s);
}

qualified<data::session> storage::get_session_by_token(const message::sessiontoken &token)
{
	pqxx::binarystring token_bs(token.data(), token.size());
	return fetch_simple_first<qualified<data::session>>(conn, statement::get_session_by_token, token_bs);
}

void storage::add_product(reference<data::supermarket> supermarket_id, message::add_product const& ap_new)
{
	message::product_base const& p_new = ap_new.p;

	qualified<data::product> p_canonical(find_add_product(conn, supermarket_id, ap_new.p));

	pqxx::work txn(conn);
	if(
		p_canonical.data.name != p_new.name ||
		p_canonical.data.volume != p_new.volume ||
		p_canonical.data.volume_measure != p_new.volume_measure
	)
	{
		p_canonical = read_first_result<qualified<data::product>>(
			txn.prepared(conv(statement::update_product))
				(p_new.name)
				(p_new.volume)
				(to_string(p_new.volume_measure))
				(p_new.identifier)
				(supermarket_id.unseal()).exec()
		);
		log("storage::storage", log::level_e::NOTICE)() << "Updated product " << supermarket_id << ":" << p_canonical.data.identifier << " [" << p_canonical.id << ']';
	}

	// Check if an older version of the product exactly matches what we've got
	try
	{
		qualified<data::productdetails> pd_old(fetch_last_productdetails_unsafe(txn, p_canonical.id));

		bool similar = (
			p_new.discount_amount == pd_old.data.discount_amount &&
			p_new.orig_price == pd_old.data.orig_price &&
			p_new.price == pd_old.data.price
		);

		if(similar)
		{
			data::productdetailsrecord pdr({
				pd_old.id,
				ap_new.retrieved_on,
				ap_new.c
			});

			register_productdetailsrecord(txn, pdr, ap_new.problems);
			txn.commit();
			return;
		}
		else
		{
			txn.prepared(conv(statement::invalidate_productdetails))
					(to_string(p_new.valid_on))
					(p_canonical.id.unseal()).exec();
		}
	} catch(storage::not_found_error)
	{
		// Not found, continue
	}

	data::productdetails pd_new({
		p_canonical.id,
		p_new.orig_price,
		p_new.price,
		p_new.discount_amount,
		p_new.valid_on,
		boost::none, // Valid until <NULL> (still valid in future)
		ap_new.retrieved_on
	});

	reference<data::productdetails> productdetails_id(write_with_id(txn, statement::add_productdetails, pd_new));
	log("storage::storage", log::level_e::NOTICE)() << "Inserted new productdetails " << productdetails_id << " for product " << p_new.identifier << " [" << p_canonical.id << ']';

	data::productdetailsrecord pdr({
		productdetails_id,
		ap_new.retrieved_on,
		ap_new.c
	});

	register_productdetailsrecord(txn, pdr, ap_new.problems);
	txn.commit();
}

message::product_summary storage::get_product(const std::string &identifier, reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	lock_products_read(txn);

	qualified<data::product> p(find_product_unsafe(txn, supermarket_id, identifier));

	try
	{
		qualified<data::productdetails> pd(fetch_last_productdetails_unsafe(txn, p.id));
		return merge(p.data, pd.data);
	} catch(storage::not_found_error)
	{
		throw std::logic_error(std::string("Inconsistency: found product ") + boost::lexical_cast<std::string>(p.id.unseal()) + " but has no (latest) productdetails entry");
	}
}

message::product_history storage::get_product_history(std::string const& identifier, reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	qualified<data::product> p(find_product_unsafe(txn, supermarket_id, identifier));

	message::product_history history({
		p.data.identifier,
		p.data.name,
		{}
	});

	pqxx::result result = txn.prepared(conv(statement::get_all_productdetails_by_product))
			(p.id.unseal()).exec();

	for(auto row : result)
	{
		auto pd(read_result<data::productdetails>(row));

		datetime valid_on(pd.valid_on);
		if(valid_on < pd.retrieved_on)
			valid_on = pd.retrieved_on;

		history.pricehistory.emplace_back(valid_on, pd.price);
	}

	return history;
}

std::vector<message::product_summary> storage::get_products(reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails))
			(supermarket_id.unseal()).exec();

	std::vector<message::product_summary> products;
	for(auto row : result)
	{
		auto p(read_result<data::product>(row));
		auto pd(read_result<data::productdetails>(row));

		products.emplace_back(merge(p, pd));
	}

	return products;
}

std::vector<message::product_summary> storage::get_products_by_name(std::string const& name, reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_name))
			(std::string("%") + name + "%")
			(supermarket_id.unseal()).exec();

	std::vector<message::product_summary> products;
	for(auto row : result)
	{
		auto p(read_result<data::product>(row));
		auto pd(read_result<data::productdetails>(row));

		products.emplace_back(merge(p, pd));
	}

	return products;
}

std::vector<message::product_log> storage::get_recent_productlog(reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_recent_productlog))
			(supermarket_id.unseal()).exec();

	std::map<std::string, message::product_log> log_map;

	for(auto row : result)
	{
		std::string identifier(row["identifier"].as<std::string>());
		std::string message(row["description"].as<std::string>());

		auto it = log_map.find(identifier);
		if(it == log_map.end())
		{
			message::product_log pl;
			pl.identifier = identifier;
			pl.name = row["name"].as<std::string>();
			pl.messages.emplace_back(message);
			pl.retrieved_on = to_datetime(row["retrieved_on"].as<std::string>());

			log_map.insert(std::make_pair(identifier, pl));
		}
		else
		{
			it->second.messages.emplace_back(message);
		}
	}

	std::vector<message::product_log> log;
	for(auto& p : log_map)
		log.emplace_back(p.second);

	return log;
}

message::productclass_summary storage::get_productclass(reference<data::productclass> productclass_id)
{
	message::productclass_summary result;

	pqxx::work txn(conn);
	pqxx::result result_productclass = txn.prepared(conv(statement::get_productclass))
			(productclass_id.unseal()).exec();

	auto pc(read_first_result<data::productclass>(result_productclass));
	result.name = pc.name;

	pqxx::result result_last_productdetails = txn.prepared(conv(statement::get_last_productdetails_by_productclass))
			(productclass_id.unseal()).exec();

	for(auto row : result_last_productdetails)
	{
		auto p(read_result<data::product>(row));
		auto pd(read_result<data::productdetails>(row));

		result.products.emplace_back(merge(p, pd));
	}

	pqxx::result result_tags = txn.prepared(conv(statement::get_tags_by_productclass))
			(productclass_id.unseal()).exec();

	for(auto row : result_tags)
		result.tags.emplace_back(read_result<qualified<data::tag>>(row));

	return result;
}

void storage::absorb_productclass(reference<data::productclass> src_productclass_id, reference<data::productclass> dest_productclass_id)
{
	pqxx::work txn(conn);

	id_t src_productclass_idu(src_productclass_id.unseal());
	id_t dest_productclass_idu(dest_productclass_id.unseal());

	txn.prepared(conv(statement::absorb_productclass_product))
			(src_productclass_idu)
			(dest_productclass_idu).exec();

	txn.prepared(conv(statement::absorb_productclass_delete_tag_productclass))
			(src_productclass_idu)
			(dest_productclass_idu).exec();

	txn.prepared(conv(statement::absorb_productclass_tag_productclass))
			(src_productclass_idu)
			(dest_productclass_idu).exec();

	txn.prepared(conv(statement::absorb_productclass_delete))
			(src_productclass_idu).exec();

	txn.commit();
}

reference<data::tagcategory> storage::find_add_tagcategory(std::string const& name)
{
	pqxx::work txn(conn);

	pqxx::result result_tagcategoryalias = txn.prepared(conv(statement::get_tagcategoryalias_by_name))
			(name).exec();

	if(result_tagcategoryalias.size() > 0)
		return reference<data::tagcategory>(read_id(result_tagcategoryalias, "tagcategory_id"));

	reference<data::tagcategory> tagcategory_id(write_with_id(txn, statement::add_tagcategory, data::tagcategory({name})));
	write(txn, statement::add_tagcategoryalias, data::tagcategoryalias({tagcategory_id, name}));

	txn.commit();

	return tagcategory_id;
}

reference<data::tag> storage::find_add_tag(std::string const& name, reference<data::tagcategory> tagcategory_id)
{
	pqxx::work txn(conn);

	pqxx::result result_tagalias = txn.prepared(conv(statement::get_tagalias_by_tagcategory_name))
			(tagcategory_id.unseal())
			(name).exec();

	if(result_tagalias.size() > 0)
		return reference<data::tag>(read_id(result_tagalias, "tag_id"));

	reference<data::tag> tag_id(write_with_id(txn, statement::add_tag, data::tag({boost::none, tagcategory_id, name})));
	write(txn, statement::add_tagalias, data::tagalias({tag_id, tagcategory_id, name}));

	txn.commit();

	return tag_id;
}

void storage::absorb_tagcategory(reference<data::tagcategory> src_tagcategory_id, reference<data::tagcategory> dest_tagcategory_id)
{
	pqxx::work txn(conn);
	txn.prepared(conv(statement::absorb_tagcategory))
			(src_tagcategory_id.unseal())
			(dest_tagcategory_id.unseal()).exec();
	txn.commit();
}

void check_tag_consistency(pqxx::transaction_base& txn)
{
	pqxx::result result_tags(txn.prepared(conv(statement::get_tags)).exec());

	std::stack<reference<data::tag>> todo;
	std::set<reference<data::tag>> tag_ids;
	std::map<reference<data::tag>, std::vector<reference<data::tag>>> tag_tree; //tag_tree[parent] = children
	for(pqxx::tuple const& tup : result_tags)
	{
		qualified<data::tag> tag = read_result<qualified<data::tag>>(tup);

		tag_ids.emplace(tag.id);
		if(tag.data.parent_id)
		{
			auto tree_it = tag_tree.find(*tag.data.parent_id);
			if(tree_it == std::end(tag_tree))
				tag_tree.emplace(*tag.data.parent_id, std::vector<reference<data::tag>>({ tag.id }));
			else
				tree_it->second.emplace_back(tag.id);
		}
		else
			todo.push(tag.id);
	}

	std::set<reference<data::tag>> visited;
	while(!todo.empty())
	{
		reference<data::tag> tag_id = todo.top();
		todo.pop();

		if(!visited.emplace(tag_id).second)
			throw std::runtime_error(std::string("Tag tree is not consistent (cycle detected with id: ") + boost::lexical_cast<std::string>(tag_id.unseal()) + ")");

		for(auto child_tag_id : tag_tree[tag_id])
			todo.push(child_tag_id);
	}

	if(visited.size() < tag_ids.size())
	{
		std::set<reference<data::tag>> diff;
		std::set_difference(tag_ids.begin(), tag_ids.end(), visited.begin(), visited.end(), std::inserter(diff, diff.begin()));

		std::stringstream sstr;
		sstr << "Tag tree is not consistent (closed cycles detected with ids:";
		for(reference<data::tag> tag_id : diff)
			sstr << " " << tag_id.unseal();
		sstr << ")";

		throw std::runtime_error(sstr.str());
	}
}

void storage::check_integrity()
{
	pqxx::work txn(conn);
	check_tag_consistency(txn);
}

void storage::absorb_tag(reference<data::tag> src_tag_id, reference<data::tag> dest_tag_id)
{
	pqxx::work txn(conn);
	txn.prepared(conv(statement::absorb_tag))
			(src_tag_id.unseal())
			(dest_tag_id.unseal()).exec();

	check_tag_consistency(txn);

	txn.commit();
}

std::vector<qualified<data::tag>> storage::get_tags()
{
	pqxx::work txn(conn);
	pqxx::result result_tags(txn.prepared(conv(statement::get_tags)).exec());

	std::vector<qualified<data::tag>> result;
	for(pqxx::tuple const& tup : result_tags)
		result.emplace_back(read_result<qualified<data::tag>>(tup));

	return result;
}

void storage::bind_tag(reference<data::productclass> productclass_id, reference<data::tag> tag_id)
{
	pqxx::work txn(conn);

	txn.prepared(conv(statement::bind_tag))
			(tag_id.unseal())
			(productclass_id.unseal()).exec();

	txn.commit();
}

void storage::update_tag_set_parent(reference<data::tag> tag_id, boost::optional<reference<data::tag>> parent_tag_id)
{
	pqxx::work txn(conn);

	if(parent_tag_id)
		txn.prepared(conv(statement::update_tag_set_parent))
				(tag_id.unseal())
				(parent_tag_id->unseal()).exec();
	else
		txn.prepared(conv(statement::update_tag_set_parent))
				(tag_id.unseal())
				().exec();

	check_tag_consistency(txn);

	txn.commit();
}

reference<data::imagecitation> storage::add_image_citation(data::imagecitation const& ic)
{
	return write_simple_with_id(conn, statement::add_imagecitation, ic);
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
	PREPARE_STATEMENT(add_karluser)
	PREPARE_STATEMENT(get_karluser)
	PREPARE_STATEMENT(get_karluser_by_name)

	PREPARE_STATEMENT(add_sessionticket)
	PREPARE_STATEMENT(get_sessionticket)

	PREPARE_STATEMENT(add_session)
	PREPARE_STATEMENT(get_session_by_token)

	PREPARE_STATEMENT(get_productclass)
	PREPARE_STATEMENT(add_productclass)
	PREPARE_STATEMENT(absorb_productclass_product)
	PREPARE_STATEMENT(absorb_productclass_delete_tag_productclass)
	PREPARE_STATEMENT(absorb_productclass_tag_productclass)
	PREPARE_STATEMENT(absorb_productclass_delete)

	PREPARE_STATEMENT(add_product)
	PREPARE_STATEMENT(update_product)
	PREPARE_STATEMENT(get_product_by_identifier)

	PREPARE_STATEMENT(get_tags)
	PREPARE_STATEMENT(get_tags_by_productclass)
	PREPARE_STATEMENT(add_tag)
	PREPARE_STATEMENT(add_tagalias)
	PREPARE_STATEMENT(get_tagalias_by_tagcategory_name)
	PREPARE_STATEMENT(add_tagcategory)
	PREPARE_STATEMENT(add_tagcategoryalias)
	PREPARE_STATEMENT(get_tagcategoryalias_by_name)
	PREPARE_STATEMENT(absorb_tag)
	PREPARE_STATEMENT(absorb_tagcategory)
	PREPARE_STATEMENT(bind_tag)
	PREPARE_STATEMENT(update_tag_set_parent)

	PREPARE_STATEMENT(add_productdetails)
	PREPARE_STATEMENT(add_productdetailsrecord)
	PREPARE_STATEMENT(add_productlog)

	PREPARE_STATEMENT(add_imagecitation)
	PREPARE_STATEMENT(update_product_image_citation);

	PREPARE_STATEMENT(get_all_productdetails_by_product)
	PREPARE_STATEMENT(get_last_productdetails)
	PREPARE_STATEMENT(get_last_productdetails_by_product)
	PREPARE_STATEMENT(get_last_productdetails_by_name)
	PREPARE_STATEMENT(get_last_productdetails_by_productclass)
	PREPARE_STATEMENT(invalidate_productdetails)

	PREPARE_STATEMENT(get_recent_productlog)
}

#undef PREPARE_STATEMENT

}
