/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017 Sylko Olzscher 
 * 
 */ 

#include "set_start_options.h"
#include <NODE_project_info.h>
#include <boost/filesystem.hpp>
#include <boost/config.hpp>
#include <boost/version.hpp>
#include <thread>

namespace node 
{
	void set_start_options(boost::program_options::options_description& opt
	, std::string const& name
	, std::string& json_path
    , unsigned int& pool_size
#if BOOST_OS_LINUX
    , struct rlimit& rl
#endif
	)
	{
		//	get the working directory
		const boost::filesystem::path cwd = boost::filesystem::current_path();
// 		json = /etc/opt/node/http_v0.1.json
		opt.add_options()

			("setup.json,J", boost::program_options::value<std::string>(&json_path)->default_value((cwd / (name + "_" + NODE_SUFFIX + ".json")).string()), "JSON configuration file")
			("setup.pool-size,P", boost::program_options::value<unsigned int>(&pool_size)->default_value(std::thread::hardware_concurrency() * 2), "Thread pool size")
#if BOOST_OS_LINUX
			("RLIMIT_NOFILE.soft", boost::program_options::value< rlim_t >()->default_value(rl.rlim_cur), "RLIMIT_NOFILE soft")
			("RLIMIT_NOFILE.hard", boost::program_options::value< rlim_t >()->default_value(rl.rlim_max), "RLIMIT_NOFILE hard")
#else
			("service.enabled,S", boost::program_options::value<bool>()->default_value(false), "run as NT service")
			("service.name", boost::program_options::value< std::string >()->default_value(("node_"+ name) + std::string(NODE_VERSION)), "NT service name")
#endif
			;
	}
	
}

