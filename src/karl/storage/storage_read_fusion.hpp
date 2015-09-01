#pragma once

#include <pqxx/pqxx>

#include <cstddef>
#include <vector>

#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/adapted.hpp>

namespace supermarx
{

template<typename T>
inline T read_result(pqxx::result::tuple const& row);

namespace detail
{

template<typename T>
struct rcol
{
	static inline T exec(pqxx::result::tuple const& row, std::string const& key)
	{
		return row[key].as<T>();
	}
};

template<typename T>
struct rcol<reference<T>>
{
	static inline reference<T> exec(pqxx::result::tuple const& row, std::string const& key)
	{
		return reference<T>(row[key].as<id_t>());
	}
};

template<typename T>
struct rcol<boost::optional<T>>
{
	static inline boost::optional<T> exec(pqxx::result::tuple const& row, std::string const& key)
	{
		if(row[key].is_null())
			return boost::none;
		else
			return rcol<T>::exec(row, key);
	}
};

template<>
struct rcol<date>
{
	static inline date exec(pqxx::result::tuple const& row, std::string const& key)
	{
		return supermarx::to_date(row[key].as<std::string>());
	}
};

template<>
struct rcol<datetime>
{
	static inline datetime exec(pqxx::result::tuple const& row, std::string const& key)
	{
		return supermarx::to_datetime(row[key].as<std::string>());
	}
};

template<>
struct rcol<measure>
{
	static inline measure exec(pqxx::result::tuple const& row, std::string const& key)
	{
		return to_measure(row[key].as<std::string>());
	}
};

template<>
struct rcol<token>
{
	static inline token exec(pqxx::result::tuple const& row, std::string const& key)
	{
		token rhs;

		pqxx::binarystring bs(row[key]);
		if(rhs.size() != bs.size())
			throw std::runtime_error("Binarystring does not have correct size to fit into token");

		memcpy(rhs.data(), bs.data(), rhs.size());
		return rhs;
	}
};

template<typename T, typename N, typename... XS>
struct read_itr;

template<typename T, typename N>
using name_t = boost::fusion::extension::struct_member_name<T, N::value>;

template<typename T, typename N>
using type_t = typename boost::fusion::result_of::value_at<T, N>::type;

template<typename N>
using next_t = typename boost::mpl::next<N>::type;

template<typename T>
using size_t = typename boost::fusion::result_of::size<T>::type;

/* Mechanics */
template<typename T, typename N, typename... XS>
struct read_itr
{
	static inline T exec(pqxx::result::tuple const& row, XS&&... xs)
	{
		using current_t = type_t<T, N>;
		return read_itr<T, next_t<N>, XS..., current_t>::exec(row, std::forward<XS>(xs)..., rcol<current_t>::exec(row, name_t<T, N>::call()));
	}
};

template<typename T, typename... XS>
struct read_itr<T, size_t<T>, XS...>
{
	static inline T exec(pqxx::result::tuple const&, XS&&... xs)
	{
		return T({std::move(xs)...});
	}
};

template<typename T>
struct read_obj
{
	static inline T exec(pqxx::result::tuple const& row)
	{
		return read_itr<T, boost::mpl::int_<0>>::exec(row);
	}
};

template<typename T>
struct read_obj<qualified<T>>
{
	static inline qualified<T> exec(pqxx::result::tuple const& row)
	{
		reference<T> id(rcol<reference<T>>::exec(row, "id"));
		return qualified<T>(id, read_obj<T>::exec(row));
	}
};

} // End of detail

template<typename T>
inline T read_result(pqxx::result::tuple const& row)
{
	return detail::read_obj<T>::exec(row);
}

}
