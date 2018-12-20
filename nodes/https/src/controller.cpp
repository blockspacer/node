/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017 Sylko Olzscher 
 * 
 */ 

#include "controller.h"
#include <NODE_project_info.h>
#include <smf/https/srv/server.h>
//#include "server_certificate.hpp"
#include <cyng/log.h>
#include <cyng/async/scheduler.h>
#include <cyng/async/signal_handler.h>
#include <cyng/factory/set_factory.h>
#include <cyng/io/serializer.h>
#include <cyng/dom/reader.h>
#include <cyng/dom/tree_walker.h>
#include <cyng/json.h>
#include <cyng/value_cast.hpp>
#include <cyng/compatibility/io_service.h>
#include <cyng/vector_cast.hpp>
#include <cyng/vm/controller.h>

#include <fstream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/filesystem.hpp>
#include <boost/assert.hpp>

namespace node 
{
	//
	//	forward declarations
	//
	bool start(cyng::async::scheduler&, cyng::logging::log_ptr, cyng::object);
	bool wait(cyng::logging::log_ptr logger);
	bool load_server_certificate(boost::asio::ssl::context& ctx
		, cyng::logging::log_ptr
		, std::string const& tls_pwd
		, std::string const& tls_certificate_chain
		, std::string const& tls_private_key
		, std::string const& tls_dh);

	controller::controller(unsigned int pool_size, std::string const& json_path)
	: pool_size_(pool_size)
	, json_path_(json_path)
	{}
	
	int controller::run(bool console)
	{
		//
		//	to calculate uptime
		//
		const std::chrono::system_clock::time_point tp_start = std::chrono::system_clock::now();
		
		//
		//	controller loop
		//
		try 
		{
			//
			//	controller loop
			//
			bool shutdown {false};
			while (!shutdown)
			{
				//
				//	establish I/O context
				//
				cyng::async::scheduler scheduler{this->pool_size_};
				
				//
				//	read configuration file
				//
				cyng::object config = cyng::json::read_file(json_path_);
				
				if (config)
				{
					cyng::vector_t vec;
					vec = cyng::value_cast(config, vec);

					if (vec.empty())
					{
						std::cerr
							<< "use option -D to generate a configuration file"
							<< std::endl;
						shutdown = true;
					}
					else
					{
						//
						//	initialize logger
						//
#if BOOST_OS_LINUX
						auto logger = cyng::logging::make_sys_logger("HTTPS", true);
						// 	auto logger = cyng::logging::make_console_logger(ioc, "HTTPS");
#else
						auto logger = cyng::logging::make_console_logger(scheduler.get_io_service(), "HTTPS");
#endif

						CYNG_LOG_TRACE(logger, cyng::io::to_str(config));
						CYNG_LOG_INFO(logger, "pool size: " << this->pool_size_);

						//
						//	start application
						//
						shutdown = start(scheduler, logger, vec.at(0));

						//
						//	print uptime
						//
						const auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - tp_start);
						CYNG_LOG_INFO(logger, "uptime " << cyng::io::to_str(cyng::make_object(duration)));
					}
				}
				else 
				{
// 					CYNG_LOG_FATAL(logger, "no configuration data");
					std::cout
						<< "use option -D to generate a configuration file"
						<< std::endl;
					
					//
					//	no configuration found
					//
					shutdown = true;
				}
				
				//
				//	stop scheduler
				//
				scheduler.stop();
				
			}
			
			
			return EXIT_SUCCESS;
		}
		catch (std::exception& ex)
		{
// 			CYNG_LOG_FATAL(logger, ex.what());
			std::cerr 
			<< ex.what()
			<< std::endl;
		}
		return EXIT_FAILURE;
	}

	int controller::create_config(std::string const& type) const
	{
		return (boost::algorithm::iequals(type, "XML"))
		? create_xml_config()
		: create_json_config()
		;
	}
	
	int controller::create_xml_config() const
	{
		return EXIT_SUCCESS;
	}
	
	int controller::create_json_config() const
	{
		std::fstream fout(json_path_, std::ios::trunc | std::ios::out);
		if (fout.is_open())
		{
			//
			//	get default values
			//
			const boost::filesystem::path tmp = boost::filesystem::temp_directory_path();
			const boost::filesystem::path pwd = boost::filesystem::current_path();
			boost::uuids::random_generator uidgen;
			
			//
            //  to send emails from cgp see https://cloud.google.com/compute/docs/tutorials/sending-mail/
            //
            
			const auto conf = cyng::vector_factory({
				cyng::tuple_factory(cyng::param_factory("log-dir", tmp.string())
					, cyng::param_factory("log-level", "INFO")
					, cyng::param_factory("tag", uidgen())
					, cyng::param_factory("generated", std::chrono::system_clock::now())
					, cyng::param_factory("version", cyng::version(NODE_VERSION_MAJOR, NODE_VERSION_MINOR))
					//, cyng::param_factory("favicon", "")
					//, cyng::param_factory("mime-config", (pwd / "mime.xml").string())
					, cyng::param_factory("https", cyng::tuple_factory(
						cyng::param_factory("address", "0.0.0.0"),
						cyng::param_factory("service", "8443"),	//	default is 443
						cyng::param_factory("document-root", (pwd / "htdocs").string()),
						cyng::param_factory("tls-pwd", "test"),
						cyng::param_factory("tls-certificate-chain", "demo.cert"),
						cyng::param_factory("tls-private-key", "priv.key"),
						cyng::param_factory("tls-dh", "demo.dh"),	//	diffie-hellman
						cyng::param_factory("auth", cyng::vector_factory({
							//	directory: /
							//	authType:
							//	user:
							//	pwd:
							cyng::tuple_factory(
								cyng::param_factory("directory", "/"),
								cyng::param_factory("authType", "Basic"),
								cyng::param_factory("realm", "Restricted Content"),
								cyng::param_factory("name", "auth@example.com"),
								cyng::param_factory("pwd", "secret")
							),
							cyng::tuple_factory(
								cyng::param_factory("directory", "/temp"),
								cyng::param_factory("authType", "Basic"),
								cyng::param_factory("realm", "Restricted Content"),
								cyng::param_factory("name", "auth@example.com"),
								cyng::param_factory("pwd", "secret")
							)}
						)),	//	auth
						//185.244.25.187
						cyng::param_factory("blacklist", cyng::vector_factory({
							//	https://bl.isx.fr/raw
							cyng::make_address("185.244.25.187"),	//	KV Solutions B.V. scans for "login.cgi"
							cyng::make_address("139.219.100.104")	//	ISP Microsoft (China) Co. Ltd. - 2018-07-31T21:14
						})),	//	blacklist
						cyng::param_factory("redirect", cyng::vector_factory({
							cyng::param_factory("/", "/index.html")
						}))
					))
				)
			});
			
			cyng::json::write(std::cout, cyng::make_object(conf));
			std::cout << std::endl;
			cyng::json::write(fout, cyng::make_object(conf));
			return EXIT_SUCCESS;
		}
		return EXIT_FAILURE;
	}

	/**
	 * Start application - simplified
	 */
	bool start(cyng::async::scheduler& scheduler, cyng::logging::log_ptr logger, cyng::object cfg)
	{
		CYNG_LOG_TRACE(logger, cyng::dom_counter(cfg) << " configuration nodes found" );	
		auto dom = cyng::make_reader(cfg);
		
 		const auto doc_root = cyng::io::to_str(dom["https"].get("document-root"));
		const auto host = cyng::io::to_str(dom["https"].get("address"));
		const auto service = cyng::io::to_str(dom["https"].get("service"));
		const auto port = static_cast<unsigned short>(std::stoi(service));
 		
 		CYNG_LOG_TRACE(logger, "document root: " << doc_root);	


		// This holds the self-signed certificate used by the server
		//load_server_certificate(ctx);

		auto const address = cyng::make_address(host);
		BOOST_ASSERT_MSG(scheduler.is_running(), "scheduler not running");
		
		//
		//	get user credentials
		//
		auth_dirs ad;
		init(dom["https"].get("auth"), ad);
		for (auto const& dir : ad) {
			CYNG_LOG_INFO(logger, "restricted access to [" << dir.first << "]");
		}

		//
		//	get blacklisted addresses
		//
		const auto blacklist_str = cyng::vector_cast<std::string>(dom["https"].get("blacklist"), "");
		CYNG_LOG_INFO(logger, blacklist_str.size() << " adresses are blacklisted");
		std::set<boost::asio::ip::address>	blacklist;
		for (auto const& a : blacklist_str) {
			auto r = blacklist.insert(boost::asio::ip::make_address(a));
			if (r.second) {
				CYNG_LOG_TRACE(logger, *r.first);
			}
			else {
				CYNG_LOG_WARNING(logger, "cannot insert " << a);
			}
		}
		
		// The SSL context is required, and holds certificates
		boost::asio::ssl::context ctx{ boost::asio::ssl::context::sslv23 };

		//
		//	get SSL configuration
		//
		auto tls_pwd = cyng::value_cast<std::string>(dom["https"].get("tls-pwd"), "test");
		auto tls_certificate_chain = cyng::value_cast<std::string>(dom["https"].get("tls-certificate-chain"), "demo.cert");
		auto tls_private_key = cyng::value_cast<std::string>(dom["https"].get("tls-private-key"), "priv.key");
		auto tls_dh = cyng::value_cast<std::string>(dom["https"].get("tls-dh"), "demo.dh");

		//
		// This holds the self-signed certificate used by the server
		//
		if (load_server_certificate(ctx, logger, tls_pwd, tls_certificate_chain, tls_private_key, tls_dh)) {

			//
			//	create VM controller
			//
			boost::uuids::random_generator uidgen;
			cyng::controller vm(scheduler.get_io_service(), uidgen(), std::cout, std::cerr);

			vm.register_function("http.session.launch", 3, [&](cyng::context& ctx) {
				//	[849a5b98-429c-431e-911d-18a467a818ca,false,127.0.0.1:61383]
				const cyng::vector_t frame = ctx.get_frame();
				CYNG_LOG_INFO(logger, "http.session.launch " << cyng::io::to_str(frame));
			});

			// Create and launch a listening port
			auto srv = std::make_shared<https::server>(logger
				, scheduler.get_io_service()
				, ctx
				, boost::asio::ip::tcp::endpoint{ address, port }
				, doc_root
				, ad
				, blacklist
				, vm);

			if (srv->run())
			{
				//
				//	wait for system signals
				//
				const bool shutdown = wait(logger);

				//
				//	close acceptor
				//
				CYNG_LOG_INFO(logger, "close acceptor");
				srv->close();

				return shutdown;
			}
		}
		//
		//	shutdown
		//
		return true;
	}
	
	
	bool wait(cyng::logging::log_ptr logger)
	{
		//
		//	wait for system signals
		//
		bool shutdown = false;
		cyng::signal_mgr signal;
		switch (signal.wait())
		{
#if BOOST_OS_WINDOWS
		case CTRL_BREAK_EVENT:
#else
		case SIGHUP:
#endif
 			CYNG_LOG_INFO(logger, "SIGHUP received");
			break;
		default:
 			CYNG_LOG_WARNING(logger, "SIGINT received");
			shutdown = true;
			break;
		}
		return shutdown;
	}

	bool load_server_certificate(boost::asio::ssl::context& ctx
		, cyng::logging::log_ptr logger
		, std::string const& tls_pwd
		, std::string const& tls_certificate_chain
		, std::string const& tls_private_key
		, std::string const& tls_dh)
	{

		//
		//	generate files with (see https://www.adfinis-sygroup.ch/blog/de/openssl-x509-certificates/):
		//

		//	openssl genrsa -out solostec.com.key 4096
		//	openssl req -new -sha256 -key solostec.com.key -out solostec.com.csr
		//	openssl req -new -sha256 -nodes -newkey rsa:4096 -keyout solostec.com.key -out solostec.com.csr
		//	openssl req -x509 -sha256 -nodes -newkey rsa:4096 -keyout solostec.com.key -days 730 -out solostec.com.pem



		ctx.set_password_callback([&tls_pwd](std::size_t, boost::asio::ssl::context_base::password_purpose) {
			return "test";
			//return tls_pwd;
		});

		ctx.set_options(
			boost::asio::ssl::context::default_workarounds |
			boost::asio::ssl::context::no_sslv2 |
			//boost::asio::ssl::context::no_sslv3 |
			boost::asio::ssl::context::single_dh_use);

		try {
			ctx.use_certificate_chain_file(tls_certificate_chain);
			ctx.use_private_key_file(tls_private_key, boost::asio::ssl::context::pem);
			ctx.use_tmp_dh_file(tls_dh);

		}
		catch (std::exception const& ex) {
			CYNG_LOG_FATAL(logger, ex.what());
			return false;

		}
		return true;
	}

}
