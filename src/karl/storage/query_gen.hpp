#pragma once

#include <string>
#include <array>

#include <karl/storage/name_iter.hpp>

namespace supermarx
{

class query_builder
{
public:
	enum class comp_e
	{
		EQUAL,
		NOTEQUAL,
		LIKE,
		IN,
		IS
	};

	struct condition_t
	{
		std::string x, y;
		comp_e c;

		condition_t(std::string const& x, std::string const& y)
			: x(x)
			, y(y)
			, c(comp_e::EQUAL)
		{}

		condition_t(std::string const& x, std::string const& y, comp_e c)
			: x(x)
			, y(y)
			, c(c)
		{}
	};

	struct condition_opt_t
	{
		std::string x;
		boost::optional<std::string> y;
		comp_e c;

		condition_opt_t(std::string const& x)
			: x(x)
			, y(boost::none)
			, c(comp_e::EQUAL)
		{}

		condition_opt_t(std::string const& x, std::string const& y)
			: x(x)
			, y(y)
			, c(comp_e::EQUAL)
		{}

		condition_opt_t(std::string const& x, comp_e c)
			: x(x)
			, y(boost::none)
			, c(c)
		{}

		condition_opt_t(std::string const& x, std::string const& y, comp_e c)
			: x(x)
			, y(y)
			, c(c)
		{}
	};

	struct assignment_t
	{
		std::string column;
		std::string value;
	};

	struct join_clause_t
	{
		std::string table;
		std::vector<condition_t> conditions;
	};

private:
	std::string table;
	std::vector<std::string> fields;
	std::vector<assignment_t> assignments;
	std::vector<join_clause_t> joins;
	std::vector<condition_t> conds;

	size_t arg_i;

	static inline void write_condition(condition_t const& c, std::stringstream& sstr)
	{
		sstr << c.x;

		switch(c.c)
		{
		case comp_e::EQUAL:
			sstr << " = ";
		break;
		case comp_e::NOTEQUAL:
			sstr << " != ";
		break;
		case comp_e::LIKE:
			sstr << " like ";
		break;
		case comp_e::IN:
			sstr << " in ";
		break;
		case comp_e::IS:
			sstr << " is ";
		break;
		}

		sstr << c.y;
	}

	static inline void write_conditions(std::vector<condition_t> const& cs, std::stringstream& sstr)
	{
		sstr << "1=1";

		for(condition_t const& c : cs)
		{
			sstr << " and ";
			write_condition(c, sstr);
		}
	}

	static inline std::string warning_str(std::string const& what, std::string const& type)
	{
		return std::string("Do not understand what to do with ") + what + " in an " + type + "-statement";
	}

public:
	query_builder(std::string const& table)
		: table(table)
		, fields()
		, assignments()
		, joins()
		, conds()
		, arg_i(1)
	{}

	size_t fresh_arg()
	{
		return arg_i++;
	}

	std::string fresh_arg_str()
	{
		return std::string("$") + boost::lexical_cast<std::string>(fresh_arg());
	}

	void add_fields(std::initializer_list<std::string> const& _fields)
	{
		fields.insert(fields.end(), _fields.begin(), _fields.end());
	}

	template<typename T>
	void add_fields()
	{
		name_itr<T>([&](std::string const& name) {
			fields.emplace_back(name);
		});
	}

	template<typename T>
	void add_fields(std::string const& table)
	{
		name_itr<T>([&](std::string const& name) {
			fields.emplace_back(table + "." + name);
		});
	}

	template<typename T>
	void add_assignments()
	{
		name_itr<T>([&](std::string const& name) {
			assignments.emplace_back(assignment_t({name, fresh_arg_str()}));
		});
	}

	void add_join(std::string const& table, std::vector<condition_t> const& conditions)
	{
		joins.emplace_back(join_clause_t({table, conditions}));
	}

	template<typename... Args>
	void add_cond(Args... args)
	{
		condition_opt_t cond(std::forward<Args>(args)...);

		if(cond.y)
			conds.emplace_back(cond.x, *cond.y, cond.c);
		else
			conds.emplace_back(cond.x, fresh_arg_str(), cond.c);
	}

	std::string select_str(bool distinct = false) const
	{
		std::stringstream sstr;

		if(!assignments.empty())
			throw std::logic_error(warning_str("assignments", "select"));

		if(distinct)
			sstr << "select distinct" << std::endl;
		else
			sstr << "select" << std::endl;

		bool first = true;
		for(std::string const& field : fields)
		{
			if(first)
				first = false;
			else
				sstr << ", ";
			sstr << field;
		}
		sstr << std::endl;

		sstr << "from " << table << std::endl;

		for(join_clause_t const& j : joins)
		{
			sstr << "inner join " << j.table << " on (";
			write_conditions(j.conditions, sstr);
			sstr << ")" << std::endl;
		}

		sstr << "where" << std::endl;

		write_conditions(conds, sstr);

		return sstr.str();
	}

	std::string insert_str() const
	{
		std::stringstream sstr;

		if(!fields.empty())
			throw std::logic_error(warning_str("fields", "insert"));

		if(!joins.empty())
			throw std::logic_error(warning_str("joins", "insert"));

		sstr << "insert into " << table << " (";
		{
			bool first = true;
			for(assignment_t const& ass : assignments)
			{
				if(first)
					first = false;
				else
					sstr << ", ";
				sstr << ass.column;
			}
		}
		sstr << ")" << std::endl;

		sstr << "values (";
		{
			bool first = true;
			for(assignment_t const& ass : assignments)
			{
				if(first)
					first = false;
				else
					sstr << ", ";

				sstr << ass.value;
			}
		}
		sstr << ")" << std::endl;

		return sstr.str();
	}

	std::string update_str() const
	{
		std::stringstream sstr;

		if(!fields.empty())
			throw std::logic_error(warning_str("fields", "update"));

		if(!joins.empty())
			throw std::logic_error(warning_str("joins", "update"));

		sstr << "update " << table << std::endl;
		sstr << "set" << std::endl;

		{
			bool first = true;
			for(assignment_t const& ass : assignments)
			{
				if(first)
					first = false;
				else
					sstr << ", ";
				sstr << ass.column << " = " << ass.value;
			}
		}

		sstr << std::endl;
		sstr << "where" << std::endl;

		write_conditions(conds, sstr);

		return sstr.str();
	}

	std::string delete_str() const
	{
		std::stringstream sstr;

		if(!fields.empty())
			throw std::logic_error(warning_str("fields", "delete"));

		if(!assignments.empty())
			throw std::logic_error(warning_str("assignments", "delete"));

		if(!joins.empty())
			throw std::logic_error(warning_str("joins", "delete"));

		sstr << "delete " << table << std::endl;
		sstr << "where" << std::endl;

		write_conditions(conds, sstr);

		return sstr.str();
	}
};

class query_gen
{
public:
	template<typename T>
	static inline std::string simple_select(std::string const& table, std::initializer_list<query_builder::condition_opt_t> const& conds = {})
	{
		query_builder qb(table);
		qb.add_fields<T>(table);

		for(query_builder::condition_opt_t const& c : conds)
			qb.add_cond(c);

		return qb.select_str();
	}

	template<typename T>
	static inline std::string simple_insert(std::string const& table)
	{
		query_builder qb(table);

		qb.add_assignments<T>();

		return qb.insert_str();
	}

	template<typename T>
	static inline std::string simple_insert_with_id(std::string const& table)
	{
		return simple_insert<T>(table) + " returning id";
	}

private:
	query_gen() = delete;
	query_gen& operator=(query_gen) = delete;

};

}
