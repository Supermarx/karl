#pragma once

#include <karl/storage/storage_common.hpp>

namespace supermarx
{

inline static message::product_summary merge(data::product const& p, data::productdetails const& pd)
{
	return message::product_summary({
		p.identifier,
		p.supermarket_id,
		p.name,
		p.productclass_id,
		p.volume,
		p.volume_measure,
		pd.orig_price,
		pd.price,
		pd.discount_amount,
		price_normalization::exec(pd.orig_price, p.volume, p.volume_measure),
		price_normalization::exec(pd.price, p.volume, p.volume_measure),
		pd.valid_on,
		p.imagecitation_id
	});
}

qualified<data::product> find_product_unsafe(pqxx::transaction_base& txn, reference<data::supermarket> supermarket_id, std::string const& identifier)
{
	pqxx::result result = txn.prepared(conv(statement::get_product_by_identifier))
			(identifier)
			(supermarket_id.unseal()).exec();

	return read_first_result<qualified<data::product>>(result);
}

qualified<data::productdetails> fetch_last_productdetails_unsafe(pqxx::transaction_base& txn, reference<data::product> product_id)
{
	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_product))
			(product_id.unseal()).exec();
	return read_first_result<qualified<data::productdetails>>(result);
}

qualified<data::product> find_add_product(pqxx::connection& conn, reference<data::supermarket> supermarket_id, message::product_base const& pb)
{
	{
		pqxx::work txn(conn);
		lock_products_read(txn);

		try
		{
			return find_product_unsafe(txn, supermarket_id, pb.identifier);
		} catch(storage::not_found_error)
		{}
	}

	{
		pqxx::work txn(conn);
		lock_products_write(txn);

		try
		{
			return find_product_unsafe(txn, supermarket_id, pb.identifier);
		} catch(storage::not_found_error)
		{}

		reference<data::productclass> productclass_id(
			write_with_id(txn, statement::add_productclass, data::productclass({
				pb.name
		})));

		data::product p({
			pb.identifier,
			supermarket_id,
			boost::none,
			productclass_id,
			pb.name,
			pb.volume,
			pb.volume_measure
		});

		reference<data::product> product_id(write_with_id(txn, statement::add_product, p));
		txn.commit();

		return qualified<data::product>(product_id, p);
	}
}

void register_productdetailsrecord(pqxx::transaction_base& txn, data::productdetailsrecord const& pdr, std::vector<std::string> const& problems)
{
	reference<data::productdetailsrecord> pdn_id(write_with_id(txn, statement::add_productdetailsrecord, pdr));

	for(std::string const& p_str : problems)
		write(txn, statement::add_productlog, data::productlog({pdn_id, p_str}));
}

void storage::add_product(reference<data::supermarket> supermarket_id, message::add_product const& ap_new)
{
	message::product_base const& p_new = ap_new.p;

	qualified<data::product> p_canonical(find_add_product(conn, supermarket_id, ap_new.p));

	pqxx::work txn(conn);
	if(
		p_canonical.data.name != p_new.name ||
		p_canonical.data.volume != p_new.volume ||
		p_canonical.data.volume_measure != p_new.volume_measure
	)
	{
		p_canonical = read_first_result<qualified<data::product>>(
			txn.prepared(conv(statement::update_product))
				(p_new.name)
				(p_new.volume)
				(to_string(p_new.volume_measure))
				(p_new.identifier)
				(supermarket_id.unseal()).exec()
		);
		log("storage::storage", log::level_e::NOTICE)() << "Updated product " << supermarket_id << ":" << p_canonical.data.identifier << " [" << p_canonical.id << ']';
	}

	// Check if an older version of the product exactly matches what we've got
	try
	{
		qualified<data::productdetails> pd_old(fetch_last_productdetails_unsafe(txn, p_canonical.id));

		bool similar = (
			p_new.discount_amount == pd_old.data.discount_amount &&
			p_new.orig_price == pd_old.data.orig_price &&
			p_new.price == pd_old.data.price
		);

		if(similar)
		{
			data::productdetailsrecord pdr({
				pd_old.id,
				ap_new.retrieved_on,
				ap_new.c
			});

			register_productdetailsrecord(txn, pdr, ap_new.problems);
			txn.commit();
			return;
		}
		else
		{
			txn.prepared(conv(statement::invalidate_productdetails))
					(to_string(p_new.valid_on))
					(p_canonical.id.unseal()).exec();
		}
	} catch(storage::not_found_error)
	{
		// Not found, continue
	}

	data::productdetails pd_new({
		p_canonical.id,
		p_new.orig_price,
		p_new.price,
		p_new.discount_amount,
		p_new.valid_on,
		boost::none, // Valid until <NULL> (still valid in future)
		ap_new.retrieved_on
	});

	reference<data::productdetails> productdetails_id(write_with_id(txn, statement::add_productdetails, pd_new));
	log("storage::storage", log::level_e::NOTICE)() << "Inserted new productdetails " << productdetails_id << " for product " << p_new.identifier << " [" << p_canonical.id << ']';

	data::productdetailsrecord pdr({
		productdetails_id,
		ap_new.retrieved_on,
		ap_new.c
	});

	register_productdetailsrecord(txn, pdr, ap_new.problems);
	txn.commit();
}

message::product_summary storage::get_product(const std::string &identifier, reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	lock_products_read(txn);

	qualified<data::product> p(find_product_unsafe(txn, supermarket_id, identifier));

	try
	{
		qualified<data::productdetails> pd(fetch_last_productdetails_unsafe(txn, p.id));
		return merge(p.data, pd.data);
	} catch(storage::not_found_error)
	{
		throw std::logic_error(std::string("Inconsistency: found product ") + boost::lexical_cast<std::string>(p.id.unseal()) + " but has no (latest) productdetails entry");
	}
}

message::product_history storage::get_product_history(std::string const& identifier, reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	qualified<data::product> p(find_product_unsafe(txn, supermarket_id, identifier));

	message::product_history history({
		p.data.identifier,
		p.data.name,
		{}
	});

	pqxx::result result = txn.prepared(conv(statement::get_all_productdetails_by_product))
			(p.id.unseal()).exec();

	for(auto row : result)
	{
		auto pd(read_result<data::productdetails>(row));

		datetime valid_on(pd.valid_on);
		if(valid_on < pd.retrieved_on)
			valid_on = pd.retrieved_on;

		history.pricehistory.emplace_back(valid_on, pd.price);
	}

	return history;
}

std::vector<message::product_summary> storage::get_products(reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails))
			(supermarket_id.unseal()).exec();

	std::vector<message::product_summary> products;
	for(auto row : result)
	{
		auto p(read_result<data::product>(row));
		auto pd(read_result<data::productdetails>(row));

		products.emplace_back(merge(p, pd));
	}

	return products;
}

std::vector<message::product_summary> storage::get_products_by_name(std::string const& name, reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_last_productdetails_by_name))
			(std::string("%") + name + "%")
			(supermarket_id.unseal()).exec();

	std::vector<message::product_summary> products;
	for(auto row : result)
	{
		auto p(read_result<data::product>(row));
		auto pd(read_result<data::productdetails>(row));

		products.emplace_back(merge(p, pd));
	}

	return products;
}

std::vector<message::product_log> storage::get_recent_productlog(reference<data::supermarket> supermarket_id)
{
	pqxx::work txn(conn);

	pqxx::result result = txn.prepared(conv(statement::get_recent_productlog))
			(supermarket_id.unseal()).exec();

	std::map<std::string, message::product_log> log_map;

	for(auto row : result)
	{
		std::string identifier(row["identifier"].as<std::string>());
		std::string message(row["description"].as<std::string>());

		auto it = log_map.find(identifier);
		if(it == log_map.end())
		{
			message::product_log pl;
			pl.identifier = identifier;
			pl.name = row["name"].as<std::string>();
			pl.messages.emplace_back(message);
			pl.retrieved_on = to_datetime(row["retrieved_on"].as<std::string>());

			log_map.insert(std::make_pair(identifier, pl));
		}
		else
		{
			it->second.messages.emplace_back(message);
		}
	}

	std::vector<message::product_log> log;
	for(auto& p : log_map)
		log.emplace_back(p.second);

	return log;
}

message::productclass_summary storage::get_productclass(reference<data::productclass> productclass_id)
{
	message::productclass_summary result;

	pqxx::work txn(conn);
	pqxx::result result_productclass = txn.prepared(conv(statement::get_productclass))
			(productclass_id.unseal()).exec();

	auto pc(read_first_result<data::productclass>(result_productclass));
	result.name = pc.name;

	pqxx::result result_last_productdetails = txn.prepared(conv(statement::get_last_productdetails_by_productclass))
			(productclass_id.unseal()).exec();

	for(auto row : result_last_productdetails)
	{
		auto p(read_result<data::product>(row));
		auto pd(read_result<data::productdetails>(row));

		result.products.emplace_back(merge(p, pd));
	}

	pqxx::result result_tags = txn.prepared(conv(statement::get_tags_by_productclass))
			(productclass_id.unseal()).exec();

	for(auto row : result_tags)
		result.tags.emplace_back(read_result<qualified<data::tag>>(row));

	return result;
}

void storage::absorb_productclass(reference<data::productclass> src_productclass_id, reference<data::productclass> dest_productclass_id)
{
	pqxx::work txn(conn);

	id_t src_productclass_idu(src_productclass_id.unseal());
	id_t dest_productclass_idu(dest_productclass_id.unseal());

	txn.prepared(conv(statement::absorb_productclass_product))
			(src_productclass_idu)
			(dest_productclass_idu).exec();

	txn.prepared(conv(statement::absorb_productclass_delete_tag_productclass))
			(src_productclass_idu)
			(dest_productclass_idu).exec();

	txn.prepared(conv(statement::absorb_productclass_tag_productclass))
			(src_productclass_idu)
			(dest_productclass_idu).exec();

	txn.prepared(conv(statement::absorb_productclass_delete))
			(src_productclass_idu).exec();

	txn.commit();
}

}
