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

inline static query_builder last_productdetails()
{
	query_builder qb("product");
	qb.add_join("productdetails", { query_builder::condition_t("product.id", "productdetails.product_id") });

	qb.add_fields<data::product>("product");
	qb.add_fields<qualified<data::productdetails>>("productdetails");

	qb.add_cond("productdetails.valid_until", "null", query_builder::comp_e::IS);
	return qb;
}

qualified<data::product> find_product_unsafe(pqxx::transaction_base& txn, reference<data::supermarket> supermarket_id, std::string const& identifier)
{
	static std::string q = query_gen::simple_select<qualified<data::product>>("product", {{"product.identifier"}, {"product.supermarket_id"}});

	pqxx::result result = txn.parameterized(q)
			(identifier)
			(supermarket_id.unseal()).exec();

	return read_first_result<qualified<data::product>>(result);
}

qualified<data::productdetails> fetch_last_productdetails_unsafe(pqxx::transaction_base& txn, reference<data::product> product_id)
{
	static std::string q = ([]() {
		query_builder qb(last_productdetails());
		qb.add_cond("productdetails.product_id");
		return qb.select_str();
	})();

	pqxx::result result = txn.parameterized(q)
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

		reference<data::productclass> productclass_id(write_with_id(txn, data::productclass({pb.name})));

		data::product p({
			pb.identifier,
			supermarket_id,
			boost::none,
			productclass_id,
			pb.name,
			pb.volume,
			pb.volume_measure
		});

		reference<data::product> product_id(write_with_id(txn, p));
		txn.commit();

		return qualified<data::product>(product_id, p);
	}
}

void register_productdetailsrecord(pqxx::transaction_base& txn, data::productdetailsrecord const& pdr, std::vector<std::string> const& problems)
{
	reference<data::productdetailsrecord> pdn_id(write_with_id(txn, pdr));

	for(std::string const& p_str : problems)
		write(txn, data::productlog({pdn_id, p_str}));
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

	reference<data::productdetails> productdetails_id(write_with_id(txn, pd_new));
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
	static std::string q_get_relevant_productdetails_by_product = ([]() {
		query_builder qb("productdetails");
		qb.add_field("productdetails.price");
		qb.add_field("productdetails.valid_on");
		qb.add_field("productdetailsrecord.retrieved_on");
		qb.add_join("productdetailsrecord", {{"productdetails.id", "productdetailsrecord.productdetails_id"}});
		qb.add_cond("productdetails.product_id");
		qb.add_order_by({"productdetailsrecord.id", true});
		return qb.select_str();
	})();

	pqxx::work txn(conn);

	qualified<data::product> p(find_product_unsafe(txn, supermarket_id, identifier));

	message::product_history history({
		p.data.identifier,
		p.data.name,
		{}
	});

	pqxx::result result = txn.parameterized(q_get_relevant_productdetails_by_product)
			(p.id.unseal()).exec();

	for(auto row : result)
	{
		datetime retrieved_on(detail::rcol<datetime>::exec(row, "retrieved_on"));

		datetime valid_on(detail::rcol<datetime>::exec(row, "valid_on"));
		if(valid_on < retrieved_on)
			valid_on = retrieved_on;

		history.pricehistory.emplace_back(valid_on, row["price"].as<int>());
	}

	return history;
}

std::vector<message::product_summary> storage::get_products(reference<data::supermarket> supermarket_id)
{
	static std::string q = ([]() {
		query_builder qb(last_productdetails());
		qb.add_cond("product.supermarket_id");
		return qb.select_str();
	})();

	pqxx::work txn(conn);
	pqxx::result result = txn.parameterized(q)
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
	static std::string q = ([]() {
		query_builder qb(last_productdetails());
		qb.add_cond("lower(product.name)", query_builder::comp_e::LIKE);
		qb.add_cond("product.supermarket_id");
		return qb.select_str();
	})();

	pqxx::work txn(conn);
	pqxx::result result = txn.parameterized(q)
			(std::string("%") + txn.esc(name) + "%")
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

	static std::string q = ([](){
		query_builder qb("product");

		qb.add_join("productdetails", {{"productdetails.product_id", "product.id"}});
		qb.add_join("productdetailsrecord", {{"productdetailsrecord.productdetails_id", "productdetails.id"}});
		qb.add_join("productlog", {{"productlog.productdetailsrecord_id", "productdetailsrecord.id"}});

		qb.add_fields({"product.identifier", "product.name", "productlog.description", "productdetailsrecord.retrieved_on"});

		qb.add_cond("product.supermarket_id");
		qb.add_cond("productdetails.valid_until", "null", query_builder::comp_e::IS);
		qb.add_cond("productdetailsrecord.id", "(select max(pdr2.id) from productdetailsrecord as pdr2 group by pdr2.productdetails_id)", query_builder::comp_e::IN);

		return qb.select_str(true);
	})();

	pqxx::result result = txn.parameterized(q)
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

	static std::string q_productclass = query_gen::simple_select<data::productclass>("productclass", {{"productclass"}});
	static std::string q_last_productdetails = ([]() {
		query_builder qb(last_productdetails());
		qb.add_cond("product.productclass_id");
		return qb.select_str();
	})();
	static std::string q_tags = ([]() {
		query_builder qb("tag");
		qb.add_join("tag_productclass", { query_builder::condition_t("tag.id", "tag_productclass.tag_id") });
		qb.add_fields<data::tag>("tag");
		qb.add_cond("tag_productclass.productclass_id");

		return qb.select_str();
	})();

	pqxx::work txn(conn);
	pqxx::result result_productclass = txn.parameterized(q_productclass)
			(productclass_id.unseal()).exec();

	auto pc(read_first_result<data::productclass>(result_productclass));
	result.name = pc.name;

	pqxx::result result_last_productdetails = txn.parameterized(q_last_productdetails)
			(productclass_id.unseal()).exec();

	for(auto row : result_last_productdetails)
	{
		auto p(read_result<data::product>(row));
		auto pd(read_result<data::productdetails>(row));

		result.products.emplace_back(merge(p, pd));
	}

	pqxx::result result_tags = txn.parameterized(q_tags)
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
