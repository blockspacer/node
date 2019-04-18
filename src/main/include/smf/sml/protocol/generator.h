/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_LIB_SML_GENERATOR_H
#define NODE_LIB_SML_GENERATOR_H

#include <smf/sml/defs.h>
#include <smf/sml/intrinsics/obis.h>
#include <smf/ipt/config.h>
#include <smf/sml/protocol/message.h>
#include <smf/sml/protocol/value.hpp>

#include <cyng/intrinsics/sets.h>
#include <cyng/intrinsics/mac.h>
#include <cyng/store/table.h>
#include <random>
#include <chrono>

namespace node
{
	namespace sml
	{
		/**
		 * Generate SML transaction is with the following format:
		 * @code
		 * [7 random numbers] [1 number in ascending order].
		 * nnnnnnnnnnnnnnnnn-m
		 * @endcode
		 *
		 * The numbers as the ASCII codes from 48 up to 57.
		 * 
		 * In a former version the following format was used:
		 * @code
		 * [17 random numbers] ['-'] [1 number in ascending order].
		 * nnnnnnnnnnnnnnnnn-m
		 * @endcode
		 *
		 * This is to generate a series of continuous transaction IDs
		 * and after reshuffling to start a new series.
		 */
		class trx
		{
		public:
			trx();
			trx(trx const&);

			/**
			 *	generate new transaction id (reshuffling)
			 */
			void shuffle(std::size_t = 7);

			/**
			 * Increase internal transaction number. 
			 */
			trx& operator++();
			trx operator++(int);

			/**
			 *	@return value of last generated (shuffled) transaction id
			 *	as string.
			 */
			std::string operator*() const;


		private:
			/**
			 * Generate a random string
			 */
			std::random_device rng_;
			std::mt19937 gen_;

			/**
			 * The fixed part of a series
			 */
			std::string		value_;

			/**
			 * Ascending number
			 */
			std::uint16_t	num_;

		};

		/**
		 * Base class for all SML generators
		 */
		class generator
		{
		public:
			generator();

			/**
			 * @return a file id generated from current time stamp
			 */
			static std::string gen_file_id();

			/**
			 * Clear message buffer
			 */
			void reset();

			/**
			 * Produce complete SML message from message buffer
			 */
			cyng::buffer_t boxing() const;

		//protected:
			std::size_t append_msg(cyng::tuple_t&&);

		protected:
			/**
			 * buffer for current SML message
			 */
			std::vector<cyng::buffer_t>	msg_;
			std::uint8_t	group_no_;

		};

		/**
		 * Generate SML requests
		 */
		class req_generator : public generator
		{
		public:
			req_generator();

			std::size_t public_open(cyng::mac48 client_id
				, cyng::buffer_t const& server_id
				, std::string const& name
				, std::string const& pwd);

			std::size_t public_close();

			/**
			 * Generic set process parameter function
			 */
			template< typename T >
			std::size_t set_proc_parameter(cyng::buffer_t const& server_id
				, obis_path tree_path
				, std::string const& username
				, std::string const& password
				, T&& val)
			{
				++trx_;
				return append_msg(message(cyng::make_object(*trx_)
					, group_no_++	//	group
					, 0 //	abort code
					, BODY_SET_PROC_PARAMETER_REQUEST	//	0x600 (1536)

					//
					//	generate process parameter request
					//
					, set_proc_parameter_request(cyng::make_object(server_id)
						, username
						, password
						, tree_path
						, parameter_tree(tree_path.back(), make_value(val)))
					)
				);
			}


			/**
			 * Restart system - 81 81 C7 83 82 01
			 */
			std::size_t set_proc_parameter_restart(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password);

			/**
			 * IP-T Host - 81 49 0D 07 00 FF
			 */
			std::size_t set_proc_parameter_ipt_host(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password
				, std::uint8_t idx
				, std::string const& address);

			std::size_t set_proc_parameter_ipt_port_local(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password
				, std::uint8_t idx
				, std::uint16_t);

			std::size_t set_proc_parameter_ipt_port_remote(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password
				, std::uint8_t idx
				, std::uint16_t);

			std::size_t set_proc_parameter_ipt_user(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password
				, std::uint8_t idx
				, std::string const&);

			std::size_t set_proc_parameter_ipt_pwd(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password
				, std::uint8_t idx
				, std::string const&);

			std::size_t set_proc_parameter_wmbus_protocol(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password
				, std::uint8_t);


			/**
			 * Simple root query - BODY_GET_PROC_PARAMETER_REQUEST (0x500)
			 */
			std::size_t get_proc_parameter(cyng::buffer_t const& server_id
				, obis
				, std::string const& username
				, std::string const& password);

			/**
			 * List query - BODY_GET_LIST_REQUEST (0x700)
			 */
			std::size_t get_list(cyng::buffer_t const& client_id
				, cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password
				, obis);

			/**
			 * get last data record - 99 00 00 00 00 03 (CODE_LAST_DATA_RECORD)
			 *  SML_GetList_Req
			 */
			std::size_t get_list_last_data_record(cyng::buffer_t const& client_id
				, cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password);

		private:
			trx	trx_;
		};

		/**
		 * Generate SML responses
		 */
		class res_generator : public generator
		{
		public:
			res_generator();

			std::size_t public_open(cyng::object trx
				, cyng::object	//	client id
				, cyng::object	//	req file id
				, cyng::object	//	server id
			);

			std::size_t public_close(cyng::object trx);

			/**
			 * Generate an empty response with the specified tree path
			 */
			std::size_t empty(cyng::object trx
				, cyng::object server_id
				, obis);

			/**
			 * OBIS_CLASS_OP_LOG_STATUS_WORD
			 */
			std::size_t get_proc_parameter_status_word(cyng::object trx
				, cyng::object server_id
				, std::uint32_t);

			/**
			 * OBIS_CODE_ROOT_DEVICE_IDENT - 81 81 C7 82 01 FF
			 */
			std::size_t get_proc_parameter_device_id(cyng::object trx
				, cyng::object server_id
				, std::string const& manufacturer
				, cyng::buffer_t const& server_id2
				, std::string const& model_code
				, std::uint32_t serial);

			/**
			 * OBIS_CODE_ROOT_MEMORY_USAGE - 00 80 80 00 10 FF
			 */
			std::size_t get_proc_mem_usage(cyng::object trx
				, cyng::object server_id
				, std::uint8_t
				, std::uint8_t);

			std::size_t get_proc_device_time(cyng::object trx
				, cyng::object server_id
				, std::chrono::system_clock::time_point
				, std::int32_t
				, bool);

			std::size_t get_proc_active_devices(cyng::object trx
				, cyng::object server_id
				, cyng::store::table const*);

			std::size_t get_proc_visible_devices(cyng::object trx
				, cyng::object server_id
				, cyng::store::table const*);

			/**
			 * 81 81 C7 86 00 FF
			 * Deliver an overview of the system configuration and state.
			 */
			std::size_t get_proc_sensor_property(cyng::object trx
				, cyng::object server_id
				, const cyng::table::record&);

			std::size_t get_proc_push_ops(cyng::object trx
				, cyng::buffer_t server_id
				, const cyng::store::table*);

			std::size_t get_proc_ipt_params(cyng::object trx
				, cyng::object server_id
				, node::ipt::redundancy const& cfg);

			std::size_t get_proc_0080800000FF(cyng::object trx
				, cyng::object server_id
				, std::uint32_t);

			std::size_t get_proc_990000000004(cyng::object trx
				, cyng::object server_id
				, std::string const&);

			std::size_t get_proc_actuators(cyng::object trx
				, cyng::object server_id);

			/**
			 * IF 1107 configuration
			 */
			std::size_t get_proc_1107_if(cyng::object trx
				, cyng::object server_id
				, cyng::store::table const*
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object
				, cyng::object);

			/**
			 * get current IP-T status - 81 49 0D 06 00 FF (OBIS_CODE_ROOT_IPT_STATE)
			 */
			std::size_t get_proc_parameter_ipt_state(cyng::object trx
				, cyng::object server_id
				, boost::asio::ip::tcp::endpoint remote_ep
				, boost::asio::ip::tcp::endpoint local_ep);

			/**
			 * Operation log data
			 */
			std::size_t get_profile_op_log(cyng::object trx
				, cyng::object client_id
				, std::chrono::system_clock::time_point act_time
				, std::uint32_t reg_period
				, std::chrono::system_clock::time_point val_time
				, std::uint64_t status
				, std::uint32_t evt
				, obis peer_address
				, std::chrono::system_clock::time_point tp
				, cyng::buffer_t const& server_id
				, std::string const& target
				, std::uint8_t push_nr);

			std::size_t get_profile_op_logs(cyng::object trx
				, cyng::object client_id
				, std::chrono::system_clock::time_point start_time
				, std::chrono::system_clock::time_point end_time
				, cyng::store::table const*);

			std::size_t get_proc_w_mbus_status(cyng::object trx
				, cyng::object client_id
				, std::string const&	// manufacturer of w-mbus adapter
				, cyng::buffer_t const&	//	adapter id (EN 13757-3/4)
				, std::string const&	//	firmware version of adapter
				, std::string const&);	//	hardware version of adapter);

			/**
			 * @param protocol node::mbus::radio_protocol
			 * @param s_mode Zeitdauer, die fortlaufend im SMode empfangen wird (der Parameter 
			 *	wird nur ben�tigt, falls vorstehend die Variante 2 ==  S/T - Automatik gew�hlt ist).
			 * @param t_mode Zeitdauer, die fortlaufend im TMode empfangen wird (der Parameter 
			 *	wird nur ben�tigt, falls vorstehend die Variante 2 ==  S/T - Automatik gew�hlt ist).
			 * @param reboot Periode, anzugeben in Sekunden, nach deren Ablauf das W-MBUS-Modem 
			 *	im MUC-C neu initialisiert werden soll. Bei 0 ist der automatische Reboot inaktiv. 
			 * @param power transmision power
			 * @param timeout Maximales Inter Message Timeout in Sekunden 
			 */
			std::size_t get_proc_w_mbus_if(cyng::object trx
				, cyng::object client_id
				, cyng::object protocol	// radio protocol
				, cyng::object s_mode	// duration in seconds
				, cyng::object t_mode	// duration in seconds
				, cyng::object reboot	//	duration in seconds
				, cyng::object power	//	transmision power (transmission_power)
				, cyng::object	//	installation mode
			);

			/**
			 * generate act_sensor_time and act_gateway_time with make_timestamp()
			 * or make_sec_index() from value.hpp
			 */
			std::size_t get_list(cyng::object trx
				, cyng::buffer_t const& client_id
				, cyng::buffer_t const& server_id
				, obis list_name
				, cyng::tuple_t act_sensor_time
				, cyng::tuple_t act_gateway_time
				, cyng::tuple_t val_list);

			/**
			 * produce an attention message
			 */
			std::size_t attention_msg(cyng::object trx
				, cyng::buffer_t const& server_id
				, cyng::buffer_t const& attention_nr
				, std::string attention_msg
				, cyng::tuple_t attention_details);

			/**
			 *	81 81 C7 86 20 FF - table "data.collector"
			 */
			std::size_t get_proc_data_collector(cyng::object trx
				, cyng::object server_id
				, cyng::store::table const* tbl_dc
				, cyng::store::table const* tbl_ro);
		};

	}
}
#endif
