#include <karl/karl.hpp>
#include <karl/config.hpp>
#include <karl/api/api_server.hpp>

#include <supermarx/product.hpp>

int main()
{
	supermarx::config c("config.yaml");
	supermarx::karl karl(c.db_host, c.db_user, c.db_password, c.db_database, c.ic_path);

	supermarx::api_server as(karl);
	as.run();
}
