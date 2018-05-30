/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 

#ifndef NODE_IPT_MASTER_SESSION_H
#define NODE_IPT_MASTER_SESSION_H

#include <smf/cluster/bus.h>
#include <smf/ipt/parser.h>
#include <cyng/async/mux.h>
#include <cyng/log.h>
#include <cyng/vm/controller.h>

namespace node 
{
	namespace ipt
	{
		class connection;
		class server;
		class session
		{
			friend class connection;
			friend class server;

			struct connect_state
			{
				connect_state();
				bool connected_local_;
			};

		public:
			session(cyng::async::mux& mux
				, cyng::logging::log_ptr logger
				, bus::shared_type
				, boost::uuids::uuid tag
				, scramble_key const& sk
				, std::uint16_t watchdog
				, std::chrono::seconds const& timeout);

			session(session const&) = delete;
			session& operator=(session const&) = delete;

			virtual ~session();

			void stop(cyng::object);
			void stop(boost::system::error_code ec);

		private:
			void ipt_req_login_public(cyng::context& ctx);
			void ipt_req_login_scrambled(cyng::context& ctx);
			void ipt_req_logout(cyng::context& ctx);
			void ipt_res_logout(cyng::context& ctx);

			void ipt_req_open_push_channel(cyng::context& ctx);

			/**
			 * Normally this function should not be used, because the open
			 * push channel request is answered by the IP-T master.
			 * There is a bug in the VARIOSafe Manager to answer an open
			 * connection request with an open push channel response.
			 */
			void ipt_res_open_push_channel(cyng::context& ctx);
			void ipt_req_close_push_channel(cyng::context& ctx);
			void ipt_req_open_connection(cyng::context& ctx);
			void ipt_req_close_connection(cyng::context& ctx);
			void ipt_res_open_connection(cyng::context& ctx);
			void ipt_req_transfer_pushdata(cyng::context& ctx);
			void ipt_res_close_connection(cyng::context& ctx);
			void ipt_req_transmit_data(cyng::context& ctx);
			void ipt_req_watchdog(cyng::context& ctx);
			void ipt_res_watchdog(cyng::context& ctx);

			void ipt_req_protocol_version(cyng::context& ctx);
			void ipt_res_protocol_version(cyng::context& ctx);
			void ipt_req_software_version(cyng::context& ctx);
			void ipt_res_software_version(cyng::context& ctx);
			void ipt_res_dev_id(cyng::context& ctx);
			void ipt_req_device_id(cyng::context& ctx);
			void ipt_req_net_stat(cyng::context& ctx);
			void ipt_res_network_stat(cyng::context& ctx);
			void ipt_req_ip_statistics(cyng::context& ctx);
			void ipt_res_ip_statistics(cyng::context& ctx);
			void ipt_req_dev_auth(cyng::context& ctx);
			void ipt_res_dev_auth(cyng::context& ctx);
			void ipt_req_dev_time(cyng::context& ctx);
			void ipt_res_dev_time(cyng::context& ctx);

			void ipt_unknown_cmd(cyng::context& ctx);

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

			void ipt_req_register_push_target(cyng::context& ctx);
			void client_res_register_push_target(cyng::context& ctx);
			void ipt_req_deregister_push_target(cyng::context& ctx);
			void client_res_deregister_push_target(cyng::context& ctx);

			void client_res_open_connection(cyng::context& ctx);

			void store_relation(cyng::context& ctx);
			void update_connection_state(cyng::context& ctx);

		private:
			cyng::async::mux& mux_;
			cyng::logging::log_ptr logger_;
			bus::shared_type bus_;	//!< cluster bus
			cyng::controller vm_;	//!< ipt device
			const scramble_key sk_;
			const std::uint16_t watchdog_;
			const std::chrono::seconds timeout_;

			/**
			 * ipt parser
			 */
			parser 	parser_;

			/**
			 * bookkeeping of ip-t sequence to task relation
			 */
			std::map<sequence_type, std::size_t>	task_db_;

			/**
			 * gatekeeper task
			 */
			std::size_t gate_keeper_;

			/**
			 * contains state of local connections
			 */
			connect_state	connect_state_;

#ifdef SMF_IO_LOG
			std::size_t log_counter_;
#endif
		};
	}
}

#endif

