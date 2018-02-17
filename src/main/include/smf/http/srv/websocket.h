/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_LIB_HTTP_SRV_WEBSOCKET_H
#define NODE_LIB_HTTP_SRV_WEBSOCKET_H

#include <boost/beast/websocket.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <cyng/log.h>

namespace node
{

	class websocket_session : public std::enable_shared_from_this<websocket_session>
	{
		boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
		boost::asio::strand<boost::asio::io_context::executor_type> strand_;
		boost::asio::steady_timer timer_;
		boost::beast::multi_buffer buffer_;
		char ping_state_ = 0;

	public:
		// Take ownership of the socket
		explicit
			websocket_session(boost::asio::ip::tcp::socket socket);

		// Start the asynchronous operation
		template<class Body, class Allocator>
		void do_accept(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> req)
		{
			// Set the control callback. This will be called
			// on every incoming ping, pong, and close frame.
			ws_.control_callback(
				std::bind(
					&websocket_session::on_control_callback,
					this,
					std::placeholders::_1,
					std::placeholders::_2));

			// Run the timer. The timer is operated
			// continuously, this simplifies the code.
			on_timer({});

			// Set the timer
			timer_.expires_after(std::chrono::seconds(15));

			// Accept the websocket handshake
			ws_.async_accept(
				req,
				boost::asio::bind_executor(
					strand_,
					std::bind(
						&websocket_session::on_accept,
						shared_from_this(),
						std::placeholders::_1)));
		}

		void on_accept(boost::system::error_code ec);

		// Called when the timer expires.
		void on_timer(boost::system::error_code ec);
		// Called to indicate activity from the remote peer
		void activity();

		// Called after a ping is sent.
		void on_ping(boost::system::error_code ec);

		void on_control_callback(boost::beast::websocket::frame_type kind,
			boost::beast::string_view payload);

		void do_read();

		void on_read(boost::system::error_code ec,
			std::size_t bytes_transferred);

		void on_write(boost::system::error_code ec,
			std::size_t bytes_transferred);
	};
}

#endif
