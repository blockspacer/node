/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 
#ifndef NODE_HTTPS_CONNECTIONS_H
#define NODE_HTTPS_CONNECTIONS_H

#include <smf/http/srv/cm_interface.h>
#include <cyng/compatibility/io_service.h>
#include <cyng/compatibility/async.h>
#include <cyng/log.h>
#include <cyng/object.h>
#include <cyng/vm/controller_fwd.h>

#include <map>
#include <array>

#include <boost/uuid/random_generator.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/experimental/core/ssl_stream.hpp>

namespace node 
{
	namespace https
	{
		class connections : public connection_manager_interface
		{
			using container_t = std::map<boost::uuids::uuid, cyng::object>;

		public:
			connections(cyng::logging::log_ptr
				, cyng::controller& vm
				, std::string const& doc_root);

			void create_session(boost::asio::ip::tcp::socket socket
				, boost::asio::ssl::context& ctx);

			void add_ssl_session(boost::asio::ip::tcp::socket, boost::asio::ssl::context&, boost::beast::flat_buffer);
			void add_plain_session(boost::asio::ip::tcp::socket, boost::beast::flat_buffer);

			void upgrade(boost::uuids::uuid, boost::asio::ip::tcp::socket socket, boost::beast::http::request<boost::beast::http::string_body> req);
			void upgrade(boost::uuids::uuid, boost::beast::ssl_stream<boost::asio::ip::tcp::socket> stream, boost::beast::http::request<boost::beast::http::string_body> req);

			/**
			 * Provide access to vm controller
			 */
			cyng::controller& vm();

			/**
			 * deliver a message to a websocket
			 */
			virtual bool ws_msg(boost::uuids::uuid tag, std::string const&) override;

			/**
			 * Send HTTP 302 moved response
			 */
			virtual bool http_moved(boost::uuids::uuid tag, std::string const& target) override;

			/**
			 * A channel is a named data source dhat cann be subsribed by web-sockets
			 */
			virtual bool add_channel(boost::uuids::uuid tag, std::string const& channel) override;

			/**
			 * Push data to all subscribers of the specified channel
			 */
			virtual void push_event(std::string const& channel, std::string const&) override;

		private:
			enum session_type {
				HTTP_PLAIN,
				HTTP_SSL,
				SOCKET_PLAIN,
				SOCKET_SSL,
				SESSION_TYPES,
				NO_SESSION	//	no session found
			};

			cyng::object create_wsocket(boost::uuids::uuid, boost::asio::ip::tcp::socket socket);
			cyng::object create_wsocket(boost::uuids::uuid, boost::beast::ssl_stream<boost::asio::ip::tcp::socket> stream);

			/**
			 * Precondition: session container is locked.
			 * 
			 */
			std::pair<cyng::object, session_type> find_http(boost::uuids::uuid tag) const;
			std::pair<cyng::object, session_type> find_socket(boost::uuids::uuid tag) const;

		private:
			cyng::logging::log_ptr logger_;

			/**
			 * execution engine
			 */
			cyng::controller& vm_;

			/**
			 * document root
			 */
			const std::string doc_root_;

			/**
			 * Generate unique session tags
			 */
			boost::uuids::random_generator uidgen_;


			/**
			 * Running HTTP/1.x sessions
			 */
			std::array<container_t, SESSION_TYPES> sessions_;

			/**
			 * Running web-sockets
			 */
			mutable std::array<cyng::async::shared_mutex, SESSION_TYPES> mutex_;


			/** @brief channel list
			 * 
			 * Each channel can be subscribed by a multitude of channels
			 */
			std::multimap<std::string, cyng::object>	listener_plain_, listener_ssl_;

		};
	}
}

#endif

