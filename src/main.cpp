#include "karl.hpp"

#include <supermarx/product.hpp>

int main(){
	supermarx::karl::do_stuff();

	supermarx::Karl karl;
	karl.add_product(supermarx::Product{"Appleflaps", 2000});
	karl.add_product(supermarx::Product{"Mudcrab Sticks", 1337});
}
