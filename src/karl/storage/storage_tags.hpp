#pragma once

#include <karl/storage/storage_common.hpp>

namespace supermarx
{

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

}
