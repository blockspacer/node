/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 

#ifndef NODE_IPT_STORE_CONTROLLER_H
#define NODE_IPT_STORE_CONTROLLER_H

#include <string>
#include <cstdint>

namespace node 
{
	class controller 
	{
	public:
		/**
		 * @param pool_size thread pool size 
		 * @param json_path path to JSON configuration file 
		 */
		controller(unsigned int pool_size, std::string const& json_path);
		
		/**
		 * Start controller loop.
		 * The controller loop controls startup and shutdown of the server.
		 * 
		 * @return EXIT_FAILURE in case of an error, otherwise EXIT_SUCCESS.
		 */
		int run(bool console);
		
		/**
		 * create a configuration file with default values.

		 * @return EXIT_FAILURE in case of an error, otherwise EXIT_SUCCESS.
		 */
		int create_config() const;

		/**
		 * Read the JSON configuration, connects to database and create all
		 * required (empty) tables.
		 *
		 * @return EXIT_FAILURE in case of an error, otherwise EXIT_SUCCESS.
		 */
		int init_db();

	private:
		const unsigned int pool_size_;
		const std::string json_path_;
	};
}

#endif
