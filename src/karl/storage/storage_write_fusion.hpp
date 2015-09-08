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

template<typename T, typename INVO>
struct wcol
{
	static inline void exec(INVO& invo, T const& x)
	{
		invo(x);
	}
};

template<typename T, typename INVO>
struct wcol<reference<T>, INVO>
{
	static inline void exec(INVO& invo, reference<T> const& x)
	{
		invo(x.unseal());
	}
};

template<typename T, typename INVO>
struct wcol<boost::optional<T>, INVO>
{
	static inline void exec(INVO& invo, boost::optional<T> const& x)
	{
		if(x)
			wcol<T, INVO>::exec(invo, *x);
		else
			invo();
	}
};

template<typename INVO>
struct wcol<date, INVO>
{
	static inline void exec(INVO& invo, date const& x)
	{
		wcol<std::string, INVO>::exec(invo, to_string(x));
	}
};

template<typename INVO>
struct wcol<datetime, INVO>
{
	static inline void exec(INVO& invo, datetime const& x)
	{
		wcol<std::string, INVO>::exec(invo, to_string(x));
	}
};

template<typename INVO>
struct wcol<confidence, INVO>
{
	static inline void exec(INVO& invo, confidence const& x)
	{
		wcol<std::string, INVO>::exec(invo, to_string(x));
	}
};

template<typename INVO>
struct wcol<measure, INVO>
{
	static inline void exec(INVO& invo, measure const& x)
	{
		wcol<std::string, INVO>::exec(invo, to_string(x));
	}
};

template<typename INVO>
struct wcol<token, INVO>
{
	static inline void exec(INVO& invo, token const& x)
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
template<typename T, typename INVO, typename N>
struct write_itr
{
	static inline void exec(INVO& invo, T const& x)
	{
		using current_t = type_t<T, N>;

		wcol<current_t, INVO>::exec(invo, boost::fusion::at<N>(x));
		write_itr<T, INVO, next_t<N>>::exec(invo, x);
	}
};

template<typename T, typename INVO>
struct write_itr<T, INVO, size_t<T>>
{
	static inline void exec(INVO&, T const&)
	{
		return;
	}
};

} // End of detail

template<typename T, typename INVO>
inline void write_invo(INVO& invo, T const& x)
{
	detail::write_itr<T, INVO, boost::mpl::int_<0>>::exec(invo, x);
}

}
