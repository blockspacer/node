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
#include <smf/ipt/config.h>
#include <smf/ipt/serializer.h>
#include <cyng/async/mux.h>
#include <cyng/log.h>
#include <cyng/vm/controller.h>
#include <array>
#include <chrono>
#include <atomic>

namespace node
{
	namespace ipt
	{
		class bus_interface
		{
		public:
			/**
			 * @brief slot [0] 0x4001/0x4002: response login
			 *
			 * sucessful network login
			 * 
			 * @param watchdog watchdog timer in minutes
			 * @param redirect URL to reconnect
			 */
			virtual void on_login_response(std::uint16_t watchdog, std::string redirect) = 0;

			/**
			 * @brief slot [1]
			 *
			 * connection lost / reconnect
			 */
			virtual void on_logout() = 0;

			/**
			 * @brief slot [2] - 0x4005: push target registered response
			 *
			 * register target response
			 *
			 * @param seq ipt sequence
			 * @param success success flag
			 * @param channel channel id
			 * @param target target name (empty when if request not triggered by bus::req_register_push_target())
			 */
			virtual void on_res_register_target(sequence_type seq
				, bool success
				, std::uint32_t channel
				, std::string target) = 0;

			/**
			 * @brief slot [3] - 0x4006: push target deregistered response
			 *
			 * deregister target response
			 */
			virtual void on_res_deregister_target(sequence_type, bool, std::string const&) = 0;

			/**
			 * @brief slot [4] - 0x1000: push channel open response
			 *
			 * open push channel response
			 * 
			 * @param seq ipt sequence
			 * @param success success flag
			 * @param channel channel id
			 * @param source source id
			 * @param status channel status
			 * @param count number of targets reached
			 */
			virtual void on_res_open_push_channel(sequence_type seq
				, bool success
				, std::uint32_t channel
				, std::uint32_t source
				, std::uint16_t status
				, std::size_t count) = 0;

			/**
			 * @brief slot [5] - 0x1001: push channel close response
			 *
			 * register consumer.
			 * open push channel response
			 * @param seq ipt sequence
			 * @param bool success flag
			 * @param channel channel id
			 */
			virtual void on_res_close_push_channel(sequence_type seq
				, bool success
				, std::uint32_t channel) = 0;

			/**
			 * @brief slot [6] - 0x9003: connection open request 
			 *
			 * incoming call
			 *
			 * @return true if request is accepted - otherwise false
			 */
			virtual bool on_req_open_connection(sequence_type, std::string const& number) = 0;

			/**
			 * @brief slot [7] - 0x1003: connection open response
			 *
			 * @param seq ipt sequence
			 * @param success true if connection open request was accepted
			 */
			virtual cyng::buffer_t on_res_open_connection(sequence_type seq, bool success) = 0;

			/**
			 * @brief slot [8] - 0x9004/0x1004: connection close request/response
			 *
			 * open connection closed
			 */
			virtual void on_req_close_connection(sequence_type) = 0;
			virtual void on_res_close_connection(sequence_type) = 0;

			/**
			 * @brief slot [9] - 0x9002: push data transfer request
			 *
			 * push data
			 */
			virtual void on_req_transfer_push_data(sequence_type, std::uint32_t, std::uint32_t, cyng::buffer_t const&) = 0;

			/**
			 * @brief slot [10] - transmit data (if connected)
			 *
			 * incoming data
			 */
			virtual cyng::buffer_t on_transmit_data(cyng::buffer_t const&) = 0;

		};

		/**
		 * Implementation of an IP-T client. 
		 * The bus send messages to it's host to signal different IP-T events.
		 * <ol>
		 * <li>0 - successful authorized</li>
		 * <li>1 - connection to master lost</li>
		 * <li>2 - incoming call (open connection request)</li>
		 * <li>3 - push data received</li>
		 * <li>4 - push target registered response</li>
		 * <li>5 - data received</li>
		 * <li>6 - push target deregistered response</li>
		 * <li>7 - open connection closed</li>
		 * <li>8 - open push channel</li>
		 * <li>9 - push channel closed</li>
		 * <li>10 - connection open response</li>
		 * </ol>
		 */
		class bus : public bus_interface //	std::enable_shared_from_this<bus>
		{
		public:
			using shared_type = std::shared_ptr<bus>;

		public:
			bus(cyng::logging::log_ptr logger
				, cyng::async::mux&
				, boost::uuids::uuid tag
				, scramble_key const&
				, std::string const& model
				, std::size_t retries);

			bus(bus const&) = delete;
			bus& operator=(bus const&) = delete;

			void start();
            void stop();

			/**
			 * @return true if online with master
			 */
			bool is_online() const;
			bool is_connected() const;

			/**
			 * @return true if watchdog is requested
			 */
			bool has_watchdog() const;
			std::uint16_t get_watchdog() const;

			boost::asio::ip::tcp::endpoint local_endpoint() const;
			boost::asio::ip::tcp::endpoint remote_endpoint() const;

			/**
			 * Send a login request (public/scrambled)
			 * 
			 * @return true if bus/session is not authorized yet and login request
			 * was emitted.
			 */
			bool req_login(master_record const&);

			/**
			 * send connection open request and starts a monitor tasks
			 * to detect timeouts.
			 *
			 * @return false if bus is in authorized state
			 */
			bool req_connection_open(std::string const& number, std::chrono::seconds d);

			/**
			 * send connection close request and starts a monitor tasks
			 * to detect timeouts.
			 *
			 * @return false if bus is not in connected state
			 */
			bool req_connection_close(std::chrono::seconds d);

			/**
			 * send connection open response.
			 * @param seq ipt sequence
			 * @param accept true if request is accepted
			 */
			void res_connection_open(sequence_type seq, bool accept);

			/**
			 * send a register push target request with an timeout
			 */
			void req_register_push_target(std::string const& name
				, std::uint16_t packet_size
				, std::uint8_t window_size
				, std::chrono::seconds);

			/**
			 * @return a textual description of the bus/connection state
			 */
			std::string get_state() const;

		private:
			void do_read();

			void ipt_res_login(cyng::context& ctx, bool scrambled);
			void ipt_res_register_push_target(cyng::context& ctx);
			void ipt_res_deregister_push_target(cyng::context& ctx);
			void ipt_req_register_push_target(cyng::context& ctx);
			void ipt_req_deregister_push_target(cyng::context& ctx);
			void ipt_res_open_channel(cyng::context& ctx);
			void ipt_res_close_channel(cyng::context& ctx);
			void ipt_res_transfer_push_data(cyng::context& ctx);
			void ipt_req_transmit_data(cyng::context& ctx);
			void ipt_req_open_connection(cyng::context& ctx);
			void ipt_res_open_connection(cyng::context& ctx);
			void ipt_req_close_connection(cyng::context& ctx);
			void ipt_res_close_connection(cyng::context& ctx);

			void ipt_req_protocol_version(cyng::context& ctx);
			void ipt_req_software_version(cyng::context& ctx);
			void ipt_req_device_id(cyng::context& ctx);
			void ipt_req_net_stat(cyng::context& ctx);
			void ipt_req_ip_statistics(cyng::context& ctx);
			void ipt_req_dev_auth(cyng::context& ctx);
			void ipt_req_dev_time(cyng::context& ctx);

			void ipt_req_transfer_pushdata(cyng::context& ctx);

			void store_relation(cyng::context& ctx);
			void remove_relation(cyng::context& ctx);

		protected:
			/**
			 * The logger instance
			 */
			cyng::logging::log_ptr logger_;

			/**
			 * Interface to cyng VM
			 */
			cyng::controller vm_;

		private:
			/**
			 * task scheduler
			 */
			cyng::async::mux& mux_;

			/**
			 * connection socket
			 */
			boost::asio::ip::tcp::socket socket_;

			/**
			 * Buffer for incoming data.
			 */
			std::array<char, NODE::PREFERRED_BUFFER_SIZE> buffer_;

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
			 * device id - response device id request
			 */
			const std::string model_;

			/**
			 * connection open retries
			 */
			const std::size_t retries_;

			/** 
			 * watchdog in minutes
			 */
			std::uint16_t watchdog_;

			/**
			 * session state
			 */
			enum state {
				STATE_ERROR_,
				STATE_INITIAL_,
				STATE_AUTHORIZED_,
				STATE_CONNECTED_,
				STATE_WAIT_FOR_OPEN_RESPONSE_,
				STATE_WAIT_FOR_CLOSE_RESPONSE_,
				STATE_SHUTDOWN_,
				STATE_HALTED_
			};
			std::atomic<state>	state_;

			/**
			 * bookkeeping of ip-t sequence to task relation
			 */
			std::map<sequence_type, std::size_t>	task_db_;

			/**
			 * Check if transition to new state is valid.
			 */
			bool transition(state evt);
		};

		/**
		 * Combine two 32-bit integers into one 64-bit integer
		 */
		std::uint64_t build_line(std::uint32_t channel, std::uint32_t source);

	}
}

#endif
