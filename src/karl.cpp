#include "karl.hpp"

#include <iostream>

#include <supermarx/product.hpp>

namespace supermarx { namespace karl {
	void do_stuff(){
		std::cout << "Love your inner communist." << std::endl;
	}

	void add_product(Product const& product){
		std::cout << "Adding " << product.name << " for " << product.price_in_cents << std::endl;
	}
}}
