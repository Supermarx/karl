#pragma once

#include <karl/storage/storage_common.hpp>

namespace supermarx
{

reference<data::karluser> storage::add_karluser(data::karluser const& user)
{
	return write_simple_with_id(conn, user);
}

qualified<data::karluser> storage::get_karluser(reference<data::karluser> karluser_id)
{
	static std::string q = query_gen::simple_select<qualified<data::karluser>>("karluser", {{"karluser.id"}});
	return fetch_simple_first<qualified<data::karluser>>(conn, q, karluser_id.unseal());
}

qualified<data::karluser> storage::get_karluser_by_name(const std::string &name)
{
	static std::string q = query_gen::simple_select<qualified<data::karluser>>("karluser", {{"karluser.name"}});
	return fetch_simple_first<qualified<data::karluser>>(conn, q, name);
}

reference<data::sessionticket> storage::add_sessionticket(data::sessionticket const& st)
{
	return write_simple_with_id(conn, st);
}

qualified<data::sessionticket> storage::get_sessionticket(reference<data::sessionticket> sessionticket_id)
{
	static std::string q = query_gen::simple_select<qualified<data::sessionticket>>("sessionticket", {{"sessionticket.id"}});
	return fetch_simple_first<qualified<data::sessionticket>>(conn, q, sessionticket_id.unseal());
}

reference<data::session> storage::add_session(data::session const& s)
{
	return write_simple_with_id(conn, s);
}

qualified<data::session> storage::get_session_by_token(const message::sessiontoken &token)
{
	static std::string q = query_gen::simple_select<qualified<data::session>>("session", {{"session.token"}});
	pqxx::binarystring token_bs(token.data(), token.size());
	return fetch_simple_first<qualified<data::session>>(conn, q, token_bs);
}

}
