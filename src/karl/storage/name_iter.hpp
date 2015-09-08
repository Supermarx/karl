#pragma once

#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/adapted.hpp>

namespace supermarx
{

namespace detail
{

template<typename T, typename N>
using name_t = boost::fusion::extension::struct_member_name<T, N::value>;

template<typename T, typename N>
using type_t = typename boost::fusion::result_of::value_at<T, N>::type;

template<typename N>
using next_t = typename boost::mpl::next<N>::type;

template<typename T>
using size_t = typename boost::fusion::result_of::size<T>::type;

/* Mechanics */
template<typename T, typename F, typename N>
struct name_itr
{
	static inline void exec(F f)
	{
		using current_t = type_t<T, N>;

		f(name_t<T, N>::call());
		name_itr<T, F, next_t<N>>::exec(f);
	}
};

template<typename T, typename F>
struct name_itr<T, F, size_t<T>>
{
	static inline void exec(F)
	{
		return;
	}
};

template<typename T, typename F>
struct name_resolv
{
	static inline void exec(F f)
	{
		detail::name_itr<T, F, boost::mpl::int_<0>>::exec(f);
	}
};

template<typename U, typename F>
struct name_resolv<reference<U>, F>
{
	static inline void exec(F f)
	{
		f("id");
	}
};

template<typename U, typename F>
struct name_resolv<qualified<U>, F>
{
	static inline void exec(F f)
	{
		f("id");
		detail::name_resolv<U, F>::exec(f);
	}
};

} // End of detail

template<typename T, typename F>
inline void name_itr(F f)
{
	detail::name_resolv<T, F>::exec(f);
}

}
