#pragma once

#include <string>

namespace supermarx
{

class config
{
public:
	std::string db_host, db_user, db_password, db_database;
	std::string ic_path;

	config(std::string const& filename);
};

}
