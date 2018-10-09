/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_LIB_HTTPS_SRV_DETECTOR_H
#define NODE_LIB_HTTPS_SRV_DETECTOR_H

#include <smf/https/srv/https.h>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/logic/tribool.hpp>

namespace node
{
	namespace https
	{
		//	forward declaration(s):
		class connections;

		/**
		 * Handles an boost::beast::http server connection
		 */
		class detector : public std::enable_shared_from_this<detector>
		{
		public:
			detector(cyng::logging::log_ptr
				, connections&
				, boost::uuids::uuid tag
				, boost::asio::ip::tcp::socket socket
				, boost::asio::ssl::context& ctx
				, std::string const& doc_root);

			// Launch the detector
			void run();

		private:
			void on_detect(boost::system::error_code ec, boost::tribool result);

		private:
			cyng::logging::log_ptr logger_;
			connections& connection_manager_;
			const boost::uuids::uuid tag_;
			boost::asio::ip::tcp::socket socket_;
			boost::asio::ssl::context& ctx_;
			boost::asio::strand<boost::asio::io_context::executor_type> strand_;
			std::string const& doc_root_;
			boost::beast::flat_buffer buffer_;

		};
	}
}

#endif
