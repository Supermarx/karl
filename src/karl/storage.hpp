#pragma once

#include <map>
#include <pqxx/pqxx>
#include <boost/optional.hpp>

#include <supermarx/id_t.hpp>
#include <supermarx/token.hpp>
#include <supermarx/qualified.hpp>

#include <supermarx/message/add_product.hpp>
#include <supermarx/message/product_summary.hpp>
#include <supermarx/message/product_log.hpp>
#include <supermarx/message/product_history.hpp>
#include <supermarx/message/session.hpp>
#include <supermarx/message/tag.hpp>

#include <supermarx/data/tag.hpp>
#include <supermarx/data/tagcategory.hpp>

#include <supermarx/data/karluser.hpp>
#include <supermarx/data/session.hpp>
#include <supermarx/data/sessionticket.hpp>
#include <supermarx/data/supermarket.hpp>
#include <supermarx/data/imagecitation.hpp>

#include <supermarx/data/product.hpp>
#include <supermarx/data/productlog.hpp>
#include <supermarx/data/productclass.hpp>
#include <supermarx/data/productdetails.hpp>
#include <supermarx/data/productdetailsrecord.hpp>

namespace supermarx
{
class storage
{
public:
	class not_found_error : std::runtime_error
	{
	public:
		not_found_error();
	};

private:
	pqxx::connection conn;

public:
	storage(std::string const& host, std::string const& user, std::string const& password, const std::string& db);
	~storage();

	reference<data::karluser> add_karluser(data::karluser const& user);
	qualified<data::karluser> get_karluser(reference<data::karluser> karluser_id);
	qualified<data::karluser> get_karluser_by_name(std::string const& name);

	reference<data::sessionticket> add_sessionticket(data::sessionticket const& st);
	qualified<data::sessionticket> get_sessionticket(reference<data::sessionticket> sessionticket_id);

	reference<data::session> add_session(data::session const& s);
	qualified<data::session> get_session_by_token(message::sessiontoken const& token);

	void add_product(reference<data::supermarket> supermarket_id, message::add_product const& ap);
	message::product_summary get_product(std::string const& identifier, reference<data::supermarket> supermarket_id);
	std::vector<message::product_summary> get_products(reference<data::supermarket> supermarket_id);
	std::vector<message::product_summary> get_products_by_name(std::string const& name, reference<data::supermarket> supermarket_id);
	std::vector<message::product_log> get_recent_productlog(reference<data::supermarket> supermarket_id);
	message::product_history get_product_history(std::string const& identifier, reference<data::supermarket> supermarket_id);

	void absorb_productclass(reference<data::productclass> src_productclass_id, reference<data::productclass> dest_productclass_id);

	reference<data::tagcategory> find_add_tagcategory(std::string const& name);
	reference<data::tag> find_add_tag(std::string const& name, boost::optional<reference<data::tagcategory>> tagcategory_id = boost::none);

	void bind_tag(reference<data::productclass> productclass_id, reference<data::tag> tag_id);
	void absorb_tagcategory(reference<data::tagcategory> src_tagcategory_id, reference<data::tagcategory> dest_tagcategory_id);
	void absorb_tag(reference<data::tag> src_tag_id, reference<data::tag> dest_tag_id);

	reference<data::imagecitation> add_image_citation(data::imagecitation const& imagecitation);
	void update_product_image_citation(std::string const& product_identifier, reference<data::supermarket> supermarket_id, reference<data::imagecitation> imagecitation_id);

private:
	void update_database_schema();
	void prepare_statements();
};
}
