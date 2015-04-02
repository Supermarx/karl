#include <karl/api/api_server.hpp>

#include <fastcgi++/manager.hpp>
#include <karl/api/request.hpp>
#include <karl/util/log.hpp>

namespace supermarx
{

api_server::api_server(karl &k)
	: k(k)
{}

void api_server::run()
{
	log("api::api_server", log::NOTICE)() << "Starting fCGI manager";

	try
	{
		Fastcgipp::GenManager<fcgi_request> m([&](){
			return boost::shared_ptr<fcgi_request>(new fcgi_request(k));
		});
		m.handler();
	}
	catch(std::exception& e)
	{
		std::cerr << "exception: " << e.what() << std::endl;
	}

	log("api::api_server", log::NOTICE)() << "fCGI manager terminated";
}

}
