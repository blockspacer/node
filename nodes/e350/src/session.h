/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 

#ifndef NODE_E350_SESSION_H
#define NODE_E350_SESSION_H

#include <smf/cluster/bus.h>
#include <smf/imega/parser.h>
#include <cyng/async/mux.h>
#include <cyng/log.h>
#include <cyng/vm/controller.h>

namespace node 
{
	namespace imega
	{
		class connection;
		class server;
		class session
		{
			friend class connection;
			friend class server;

			struct connect_state
			{
				enum state {
					NOT_CONNECTED_,
					LOCAL_,
					NON_LOCAL_,

				} state_;

				connect_state();
				void set_connected(bool);
				void set_disconnected();
				bool is_local() const;
				bool is_connected() const;
			};

		public:
			session(cyng::async::mux& mux
				, cyng::logging::log_ptr logger
				, bus::shared_type
				, boost::uuids::uuid tag
				, std::chrono::seconds);

			session(session const&) = delete;
			session& operator=(session const&) = delete;

			virtual ~session();

			void stop(boost::system::error_code ec);

		private:

			void imega_req_login(cyng::context& ctx);
			//void ipt_req_logout(cyng::context& ctx);
			//void ipt_res_logout(cyng::context& ctx);

			//void ipt_req_open_push_channel(cyng::context& ctx);
			//void ipt_req_close_push_channel(cyng::context& ctx);
			void modem_req_open_connection(cyng::context& ctx);
			void modem_req_close_connection(cyng::context& ctx);
			//void ipt_res_open_connection(cyng::context& ctx);
			//void ipt_req_transfer_pushdata(cyng::context& ctx);
			//void ipt_res_close_connection(cyng::context& ctx);
			void imega_req_transmit_data(cyng::context& ctx);
			//void ipt_res_watchdog(cyng::context& ctx);

			//void ipt_res_protocol_version(cyng::context& ctx);
			//void ipt_res_software_version(cyng::context& ctx);
			//void ipt_res_dev_id(cyng::context& ctx);
			//void ipt_res_network_stat(cyng::context& ctx);
			//void ipt_res_ip_statistics(cyng::context& ctx);
			//void ipt_res_dev_auth(cyng::context& ctx);
			//void ipt_res_dev_time(cyng::context& ctx);

			//void ipt_unknown_cmd(cyng::context& ctx);

			//void client_res_login(std::uint64_t, bool, std::string msg);
			void client_res_login(cyng::context& ctx);
			void client_res_open_push_channel(cyng::context& ctx);
			void client_res_close_push_channel(cyng::context& ctx);
			void client_req_open_connection_forward(cyng::context& ctx);
			void client_res_open_connection_forward(cyng::context& ctx);
			void client_req_transmit_data_forward(cyng::context& ctx);
			void client_res_transfer_pushdata(cyng::context& ctx);
			void client_req_transfer_pushdata_forward(cyng::context& ctx);
			void client_req_close_connection_forward(cyng::context& ctx);
			void client_res_close_connection_forward(cyng::context& ctx);

			//void ipt_req_register_push_target(cyng::context& ctx);
			void client_res_register_push_target(cyng::context& ctx);

			void client_res_open_connection(cyng::context& ctx);

			void store_relation(cyng::context& ctx);
			void update_connection_state(cyng::context& ctx);

		private:
			cyng::async::mux& mux_;
			cyng::logging::log_ptr logger_;
			bus::shared_type bus_;	//!< cluster bus
			cyng::controller vm_;	//!< iMEGA device

			/**
			 * iMEGA parser
			 */
			parser 	parser_;

			/**
			 * gatekeeper task
			 */
			std::size_t gate_keeper_;

			/**
			 * contains state of local connections
			 */
			connect_state	connect_state_;
		};
	}
}

#endif

