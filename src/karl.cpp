#include "karl.hpp"

#include <iostream>

#include <supermarx/product.hpp>

namespace supermarx {
	void Karl::add_product(Product const& product){
		std::cout << "Adding " << product.name << " for " << product.price_in_cents << std::endl;
		products.push_back(product);
	}

	namespace karl {
		void do_stuff(){
			std::cout << "Love your inner communist." << std::endl;
		}
	}
}
