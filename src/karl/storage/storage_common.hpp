#pragma once

#include <stack>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include <karl/price_normalization.hpp>

#include <karl/util/log.hpp>

#include <karl/storage/storage.hpp>
#include <karl/storage/storage_read_fusion.hpp>
#include <karl/storage/storage_write_fusion.hpp>

#include <supermarx/util/guard.hpp>

#include <supermarx/data/tagalias.hpp>
#include <supermarx/data/tagcategoryalias.hpp>

#include "sql.cc"

namespace supermarx
{

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

void lock_products_read(pqxx::transaction_base& txn)
{
	txn.exec("lock table product in access share mode");
}

void lock_products_write(pqxx::transaction_base& txn)
{
	txn.exec("lock table product in access exclusive mode");
}

}
