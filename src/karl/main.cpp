#include <karl/karl.hpp>
#include <karl/config.hpp>
#include <karl/api/api_server.hpp>

#include <supermarx/product.hpp>

/*supermarx::product create_stub(std::string const& name, unsigned int price)
{
	auto now = supermarx::datetime_now();

	return supermarx::product{
		name+"id",
		name,
		price,
		price,
		supermarx::condition::ALWAYS,
		now.date(),
		now
	};
}*/

int main()
{
	supermarx::config c("config.yaml");
	supermarx::karl karl(c.db_host, c.db_user, c.db_password, c.db_database);

	supermarx::api_server as(karl);
	as.run();

	//karl.add_product(create_stub("Appleflaps", 2000));
	//karl.add_product(create_stub("Mudcrab Sticks", 1337));

	//auto products = karl.get_products("Appelsien");
	//for(auto const& p : products){
	//	std::cout << "OMFG! " << p.name << " VOOR SLECHTS " << p.price << std::endl;
	//}
}
