/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#ifndef NODE_IPT_BUS_H
#define NODE_IPT_BUS_H

#include <NODE_project_info.h>
#include <smf/ipt/parser.h>
#include <smf/ipt/serializer.h>
#include <cyng/async/mux.h>
#include <cyng/log.h>
#include <cyng/vm/controller.h>
#include <array>

namespace node
{
	namespace ipt
	{
		class bus : public std::enable_shared_from_this<bus>
		{
		public:
			using shared_type = std::shared_ptr<bus>;

		public:
			bus(cyng::async::mux& mux
				, cyng::logging::log_ptr logger
				, boost::uuids::uuid tag
				, scramble_key const&
				, std::size_t tsk);

			bus(bus const&) = delete;
			bus& operator=(bus const&) = delete;

			void start();

			/**
			 * @return true if online with master
			 */
			bool is_online() const;

		private:
			void do_read();

		public:
			/**
			 * Interface to cyng VM
			 */
			cyng::controller vm_;

		private:
			/**
			 * connection socket
			 */
			boost::asio::ip::tcp::socket socket_;

			/**
			 * Buffer for incoming data.
			 */
			std::array<char, NODE_PREFERRED_BUFFER_SIZE> buffer_;

			cyng::async::mux& mux_;

			/**
			 * The logger instance
			 */
			cyng::logging::log_ptr logger_;

			const std::size_t task_;

			/**
			 * Parser for binary cyng data stream (from cluster master)
			 */
			parser 	parser_;

			/**
			 * wrapper class to serialize and send
			 * data and code.
			 */
			serializer		serializer_;

			/**
			 * session state
			 */
			enum {
				STATE_ERROR_,
				STATE_INITIAL_,
				STATE_AUTHORIZED_,
				STATE_SHUTDOWN_,
			} state_;

		};

		/**
		 * factory method for cluster bus
		 */
		bus::shared_type bus_factory(cyng::async::mux& mux
			, cyng::logging::log_ptr logger
			, boost::uuids::uuid tag
			, scramble_key const& sk
			, std::size_t tsk);
	}
}

#endif
