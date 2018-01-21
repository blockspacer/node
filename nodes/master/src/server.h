/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017 Sylko Olzscher 
 * 
 */ 

#ifndef NODE_MASTER_SERVER_H
#define NODE_MASTER_SERVER_H

//#include "session.h"
//#include <cyng/compatibility/io_service.h>
#include <cyng/async/mux.h>
#include <cyng/log.h>
#include <cyng/store/db.h>
#include <boost/uuid/uuid.hpp>
#include <unordered_map>

namespace node 
{
	class server 
	{
	public:
		server(cyng::async::mux&
			, cyng::logging::log_ptr logger
			, boost::uuids::uuid
			, std::string account
			, std::string pwd
			, int monitor
			, cyng::tuple_t cfg_session);

		/**
		 * start listening
		 */
		void run(std::string const&, std::string const&);
		
		/**
		 * close acceptor 
		 */
		void close();
		
	private:
		/// Perform an asynchronous accept operation.
		void do_accept();
		
	private:
		/*
		 * task manager and running I/O context
		 */
		cyng::async::mux& mux_;

		/// The io_context used to perform asynchronous operations.
		//cyng::io_service_t& io_ctx_;
		
		/**
		 * The logger
		 */
		cyng::logging::log_ptr logger_;

		/**
		 * node tag
		 */
		//boost::uuids::uuid tag_;

		//	credentials
		const std::string account_;
		const std::string pwd_;
		const std::chrono::seconds monitor_;

		
		/// Acceptor used to listen for incoming connections.
		boost::asio::ip::tcp::acceptor acceptor_;		

#if (BOOST_VERSION < 106600)
		boost::asio::ip::tcp::socket socket_;
#endif

		/**
		 * database
		 */
		cyng::store::db	db_;

	};
}

#endif

