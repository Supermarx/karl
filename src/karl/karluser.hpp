#pragma once

#include <string>

#include <supermarx/token.hpp>

namespace supermarx
{

struct karluser
{
	std::string name;
	token password_salt;
	token password_hashed;
};

}
