#pragma once

#include <karl/storage/storage_common.hpp>

namespace supermarx
{

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

}
