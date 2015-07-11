#pragma once

#include <iostream>

#include <karl/karl.hpp>
#include <karl/config.hpp>
#include <karl/api/api_server.hpp>

#include <supermarx/api/session_operations.hpp>

#include <boost/program_options.hpp>

namespace supermarx
{

class cli
{
private:
	struct cli_options
	{
		std::string config;
		bool no_perms;
		std::string action;
	};

	static int read_options(cli_options& opt, int argc, char** argv)
	{
		boost::program_options::options_description o_general("Options");
		o_general.add_options()
				("help,h", "display this message")
				("config,C", boost::program_options::value(&opt.config), "path to the configfile (default: ./config.yaml)")
				("no-perms,n", "do not check permissions");

		boost::program_options::variables_map vm;
		boost::program_options::positional_options_description pos;

		pos.add("action", 1);

		boost::program_options::options_description options("Allowed options");
		options.add(o_general);
		options.add_options()
				("action", boost::program_options::value(&opt.action));

		try
		{
			boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(options).positional(pos).run(), vm);
		} catch(boost::program_options::unknown_option &e)
		{
			std::cerr << "Unknown option --" << e.get_option_name() << ", see --help." << std::endl;
			return EXIT_FAILURE;
		}

		try
		{
			boost::program_options::notify(vm);
		} catch(const boost::program_options::required_option &e)
		{
			std::cerr << "You forgot this: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}

		if(vm.count("help"))
		{
			std::cout
					<< "SuperMarx core daemon Karl. [https://github.com/SuperMarx/karl]" << std::endl
					<< "Usage: ./karl [options] action" << std::endl
					<< std::endl
					<< "Actions:" << std::endl
					<< "  create-user           create an user" << std::endl
					<< "  server [-n]           serve the REST API server via fastcgi" << std::endl
					<< "                            use a wrapper like `spawn-fcgi`" << std::endl
					<< std::endl
					<< o_general;

			return EXIT_FAILURE;
		}

		if(!vm.count("action"))
		{
			std::cerr << "Please specify an action, see --help." << std::endl;
			return EXIT_FAILURE;
		}

		if(!vm.count("config"))
			opt.config = "./config.yaml";

		opt.no_perms = vm.count("no-perms");

		return EXIT_SUCCESS;
	}

public:
	cli() = delete;

	static int exec(int argc, char** argv)
	{
		cli_options opt;

		int result = read_options(opt, argc, argv);
		if(result != EXIT_SUCCESS)
			return result;

		supermarx::config c(opt.config);
		supermarx::karl karl(c.db_host, c.db_user, c.db_password, c.db_database, c.ic_path, !opt.no_perms);

		if(opt.action == "server")
		{
			supermarx::api_server as(karl);
			as.run();
		}
		else if(opt.action == "create-user")
		{
			std::string username, password;

			while(username == "")
			{
				std::cerr << "Username: ";
				std::getline(std::cin, username);
			}

			std::cerr << "Password (leave blank for autogen): ";
			std::getline(std::cin, password);

			bool autogen_password = (password == "");
			if(autogen_password)
				password = to_string(api::random_token());

			karl.create_user(username, password);

			if(autogen_password)
				std::cerr << std::endl
						  << "Use the following password:" << std::endl
						  << password << std::endl
						  << std::endl;
		}
		else if(opt.action == "test")
		{
			karl.test();
		}
		else
		{
			std::cerr << "Unknown action '" << opt.action << "', see --help." << std::endl;
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

};

}
