#pragma once

#include <karl/karl.hpp>

namespace supermarx
{

class api_server
{
private:
	karl& k;

public:
	api_server(karl& k);

	void run();
};

}
