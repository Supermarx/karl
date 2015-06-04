#pragma once

#include <supermarx/id_t.hpp>
#include <supermarx/token.hpp>
#include <supermarx/datetime.hpp>

namespace supermarx
{

struct session
{
	id_t karluser_id;
	token token;
	datetime creation;
};

}
