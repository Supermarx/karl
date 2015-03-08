#include "karl.hpp"

#include <supermarx/product.hpp>

int main(){
	supermarx::Karl karl("/tmp/karl/");
	karl.add_product(supermarx::product{"Appleflaps", 2000});
	karl.add_product(supermarx::product{"Mudcrab Sticks", 1337});

	auto products = karl.get_products("Appleflaps");
	for(auto const& p : products){
		std::cout << "OMFG! " << p.name << " VOOR SLECHTS " << p.price_in_cents << std::endl;
	}
}
