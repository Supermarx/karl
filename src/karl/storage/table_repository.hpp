#pragma once

namespace supermarx
{

namespace detail
{

template<typename T>
struct table_resolv
{
	static inline std::string exec() = delete;
};

#define table_resolv_auto(T)\
	template<> struct table_resolv<data::T> { static inline std::string exec() { return #T; } };

table_resolv_auto(tag)
table_resolv_auto(tagalias)
table_resolv_auto(tagcategory)
table_resolv_auto(tagcategoryalias)

table_resolv_auto(product)
table_resolv_auto(productdetails)
table_resolv_auto(productdetailsrecord)
table_resolv_auto(productclass)
table_resolv_auto(productlog)

table_resolv_auto(karluser)
table_resolv_auto(sessionticket)
table_resolv_auto(session)

table_resolv_auto(imagecitation)

#undef table_resolv_auto

}

class table_repository
{
public:
	template<typename T>
	static std::string lookup()
	{
		return detail::table_resolv<T>::exec();
	}

private:
	table_repository() = delete;
	table_repository operator=(table_repository) = delete;
};

}
