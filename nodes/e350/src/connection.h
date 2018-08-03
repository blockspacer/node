/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 

#ifndef NODE_E350_CONNECTION_H
#define NODE_E350_CONNECTION_H

#include "session.h"
#include <NODE_project_info.h>
#include <smf/imega/serializer.h>
#include <smf/cluster/bus.h>
#include <cyng/log.h>
#include <array>
#include <memory>

namespace node 
{
	namespace imega
	{
		/**
		 * Cluster member connection
		 */
		class server;
		class connection 
		{
			friend class server;

		public:
			connection(connection const&) = delete;
			connection& operator=(connection const&) = delete;

			/**
			 * Construct a connection with the specified socket
			 */
			explicit connection(boost::asio::ip::tcp::socket&&
				, cyng::async::mux& mux
				, cyng::logging::log_ptr logger
				, bus::shared_type
				, boost::uuids::uuid
				, std::chrono::seconds const& timeout);

            virtual ~connection();

			/**
			 * Start the first asynchronous operation for the connection.
			 */
			void start();

			/**
			 * Stop all asynchronous operations associated with the connection.
			 */
			void stop();

			/**
			 * @return connection specific hash based in internal tag
			 */
			std::size_t hash() const noexcept;

		private:
			/**
			 * Perform an asynchronous read operation.
			 */
			void do_read();

			/**
			 * Perform an asynchronous write operation.
			 */
			void do_write();

		private:
			/**
			 * connection socket
			 */
			boost::asio::ip::tcp::socket socket_;

			/**
			 * The logger instance
			 */
			cyng::logging::log_ptr logger_;

			const boost::uuids::uuid tag_;

			/**
			 * Buffer for incoming data.
			 */
			std::array<char, NODE::PREFERRED_BUFFER_SIZE> buffer_;

			/**
			 * Implements the session logic
			 */
			session	session_;

			/**
			 * wrapper class to serialize and send
			 * data and code.
			 */
			serializer		serializer_;

            /**
             * system shutdown flag - supress all futher
             * communication
             */
            bool shutdown_;
		};

		cyng::object make_connection(boost::asio::ip::tcp::socket&&
			, cyng::async::mux& mux
			, cyng::logging::log_ptr logger
			, bus::shared_type
			, boost::uuids::uuid
			, std::chrono::seconds const& timeout);

	}
}

#include <cyng/intrinsics/traits.hpp>
namespace cyng
{
	namespace traits
	{
		template <>
		struct type_tag<node::imega::connection>
		{
			using type = node::imega::connection;
			using tag = std::integral_constant<std::size_t, PREDEF_CONNECTION>;
#if defined(CYNG_LEGACY_MODE_ON)
			const static char name[];
#else
			constexpr static char name[] = "ipt:connection";
#endif
		};

		template <>
		struct reverse_type < PREDEF_CONNECTION >
		{
			using type = node::imega::connection;
		};
	}
}

#include <functional>
#include <boost/functional/hash.hpp>

namespace std
{
	template<>
	struct hash<node::imega::connection>
	{
		inline size_t operator()(node::imega::connection const& conn) const noexcept
		{
			return conn.hash();
		}
	};
	template<>
	struct equal_to<node::imega::connection>
	{
		using result_type = bool;
		using first_argument_type = node::imega::connection;
		using second_argument_type = node::imega::connection;

		inline bool operator()(node::imega::connection const& conn1, node::imega::connection const& conn2) const noexcept
		{
			return conn1.hash() == conn2.hash();
		}
	};
}

#endif



