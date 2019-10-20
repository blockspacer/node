/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylko Olzscher
 *
 */

#ifndef NODE_SEGW_SERVER_H
#define NODE_SEGW_SERVER_H

#include <cyng/async/mux.h>
#include <cyng/log.h>

namespace node
{
	class cache;
	class server
	{
	public:
		server(cyng::async::mux&
			, cyng::logging::log_ptr logger
			, cache& c
			, std::string account
			, std::string pwd);

		/**
		 * start listening
		 */
		void run(std::string const&, std::string const&);

		/**
		* close acceptor
		*/
		void close();

	private:
		/**
		 * Perform an asynchronous accept operation.
		 */
		void do_accept();

	private:
		/*
		 * task manager and running I/O context
		 */
		cyng::async::mux& mux_;

		/**
		 * The logger
		 */
		cyng::logging::log_ptr logger_;

		/**
		 * configuration cache
		 */
		cache& cache_;


		/**
		 * credentials
		 */
		std::string const account_;
		std::string const pwd_;

		/** 
		 * Acceptor used to listen for incoming connections.
		 */
		boost::asio::ip::tcp::acceptor acceptor_;

#if (BOOST_VERSION < 106600)
		boost::asio::ip::tcp::socket socket_;
#endif

	};
}

#endif

