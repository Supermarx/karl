#include "config.hpp"

#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace supermarx
{

config::config(std::string const& filename)
{
	YAML::Node doc(YAML::LoadFile(filename));
	const YAML::Node& db = doc["db"];

	db_host = db["host"].as<std::string>();
	db_user = db["user"].as<std::string>();
	db_password = db["password"].as<std::string>();
	db_database = db["database"].as<std::string>();

	const YAML::Node& ic = doc["imagecitations"];

	ic_path = ic["path"].as<std::string>();
}

}
