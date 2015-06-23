#pragma once

#include <pqxx/pqxx>

#include <cstddef>
#include <vector>

#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/adapted.hpp>

namespace supermarx
{

namespace detail
{

template<typename T>
struct wcol
{
	static inline void exec(pqxx::prepare::invocation& invo, T const& x)
	{
		invo(x);
	}
};

template<typename T>
struct wcol<reference<T>>
{
	static inline void exec(pqxx::prepare::invocation& invo, reference<T> const& x)
	{
		invo(x.unseal());
	}
};

template<typename T>
struct wcol<boost::optional<T>>
{
	static inline void exec(pqxx::prepare::invocation& invo, boost::optional<T> const& x)
	{
		if(x)
			wcol<T>::exec(invo, *x);
		else
			invo();
	}
};

template<>
struct wcol<date>
{
	static inline void exec(pqxx::prepare::invocation& invo, date const& x)
	{
		wcol<std::string>::exec(invo, to_string(x));
	}
};

template<>
struct wcol<datetime>
{
	static inline void exec(pqxx::prepare::invocation& invo, datetime const& x)
	{
		wcol<std::string>::exec(invo, to_string(x));
	}
};

template<>
struct wcol<confidence>
{
	static inline void exec(pqxx::prepare::invocation& invo, confidence const& x)
	{
		wcol<std::string>::exec(invo, to_string(x));
	}
};

template<>
struct wcol<measure>
{
	static inline void exec(pqxx::prepare::invocation& invo, measure const& x)
	{
		wcol<std::string>::exec(invo, to_string(x));
	}
};

template<>
struct wcol<token>
{
	static inline void exec(pqxx::prepare::invocation& invo, token const& x)
	{
		pqxx::binarystring bs(x.data(), x.size());
		invo(bs);
	}
};

template<typename T, typename N>
using type_t = typename boost::fusion::result_of::value_at<T, N>::type;

template<typename N>
using next_t = typename boost::mpl::next<N>::type;

template<typename T>
using size_t = typename boost::fusion::result_of::size<T>::type;

/* Mechanics */
template<typename T, typename N>
struct write_itr
{
	static inline void exec(pqxx::prepare::invocation& invo, T const& x)
	{
		using current_t = type_t<T, N>;

		wcol<current_t>::exec(invo, boost::fusion::at<N>(x));
		write_itr<T, next_t<N>>::exec(invo, x);
	}
};

template<typename T>
struct write_itr<T, size_t<T>>
{
	static inline void exec(pqxx::prepare::invocation&, T const&)
	{
		return;
	}
};

} // End of detail

template<typename T>
inline void write_invo(pqxx::prepare::invocation& invo, T const& x)
{
	detail::write_itr<T, boost::mpl::int_<0>>::exec(invo, x);
}

}
